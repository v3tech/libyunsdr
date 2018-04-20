/*
 * yunsdr_api.c
 *
 *  Created on: 2015年9月9日
 *      Author: Eric
 */

#include <time.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#if defined(__WINDOWS_) || defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#include <WS2tcpip.h>
#if(_WIN32_WINNT < 0x0502)
#define MSG_WAITALL  0
#endif
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define SOCKET int
#define SOCKADDR_IN struct sockaddr_in
#define INVALID_SOCKET (SOCKET)(~0)
#endif

#include "yunsdr_api.h"

enum TRANS_ID{
	TRANSPORT_NONE,
	TRANSPORT_CMD,
	TRANSPORT_TX,
	TRANSPORT_RX,
};
enum DEV_STATUS {
	DEV_DISCONNECTED = 0x0,
	DEV_CONNECTED = 0x1,
	DEV_ENABLE_TX = 0x2,
	DEV_ENABLE_RX = 0x4
};

typedef struct yunsdr_meta {
	uint32_t head;
	uint32_t nsamples;
	uint32_t timestamp_l;
	uint32_t timestamp_h;
	uint32_t payload[0];
}YUNSDR_META;

typedef struct yunsdr_transport {
	SOCKET tx_sock;
	SOCKET rx_sock;
	SOCKET cmd_sock;
	SOCKADDR_IN cmd_addr;
}YUNSDR_TRANSPORT;

typedef struct yunsdr_device_descriptor {
	uint32_t id;
	int32_t mark;
	enum DEV_STATUS status;
	char ipaddr[20];
	YUNSDR_TRANSPORT *transport;
	YUNSDR_META *tx_meta;
	YUNSDR_META *rx_meta;
}YUNSDR_DESCRIPTOR;


#define MAX_TX_BUFFSIZE (32*1024*1024)
#define MAX_RX_BUFFSIZE (32*1024*1024)
#define MIN(x, y)  ((x < y)?x:y)

#ifndef ETIMEDOUT
#define ETIMEDOUT 110
#endif

#define IDENTIFIER_CODE 0xEB93

#define YUNSDR_HAVE_DATA_LINK
#define YUNSDR_HAVE_TIMESTAMP

static int __yunsdr_init_transport(YUNSDR_DESCRIPTOR *yunsdr, enum TRANS_ID port);
static void __yunsdr_deinit_transport(YUNSDR_DESCRIPTOR *yunsdr, enum TRANS_ID port);

uint64_t yunsdr_ticksToTimeNs(const uint64_t ticks, const double rate)
{
	const long long ratell = (long long)(rate);
	const long long full = (long long)(ticks/ratell);
	const long long err = ticks - (full*ratell);
	const double part = full*(rate - ratell);
	const double frac = ((err - part)*1000000000)/rate;
	return (full*1000000000) + llround(frac);
}

uint64_t yunsdr_timeNsToTicks(const uint64_t timeNs, const double rate)
{
	const long long ratell = (long long)(rate);
	const long long full = (long long)(timeNs/1000000000);
	const long long err = timeNs - (full*1000000000);
	const double part = full*(rate - ratell);
	const double frac = part + ((err*rate)/1000000000);
	return (full*ratell) + llround(frac);
}

static int8_t wait_for_recv_ready(SOCKET sock_fd, double timeout)
{
	struct timeval tv;
	struct timeval *ptv = NULL;

	if(timeout > 0.0) {
		//setup timeval for timeout
		//If the tv_usec > 1 second on some platforms, select will
		//error EINVAL: An invalid timeout interval was specified.
		tv.tv_sec = (int)timeout;
		tv.tv_usec = (int)(timeout*1000000)%1000000;
		ptv = &tv;
	}
	//setup rset for timeout
	fd_set rset;
	FD_ZERO(&rset);
	FD_SET(sock_fd, &rset);

	//http://www.gnu.org/s/hello/manual/libc/Interrupted-Primitives.html
	//This macro is provided with gcc to properly deal with EINTR.
	//If not provided, define an empty macro, assume that is OK
#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(x) (x)
#endif

	//call select with timeout on receive socket
	return TEMP_FAILURE_RETRY(select(sock_fd+1, &rset, NULL, NULL, ptv)) > 0;
}

static int32_t __trans_cmd_send(YUNSDR_TRANSPORT *transport, void *buf, uint32_t len)
{
	int ret;

	assert(buf);
	ret = sendto(transport->cmd_sock, buf, len, 0,
			(struct sockaddr *)&transport->cmd_addr, sizeof(transport->cmd_addr));
	if(ret < 0) {
		debug(DEBUG_ERR, "Send cmd error\n");
		return ret;
	}

	return 0;
}

static int32_t __trans_cmd_send_then_recv(YUNSDR_TRANSPORT *transport, void *buf, uint32_t len)
{
	int ret;
#if defined(__GNUC__)
	socklen_t addr_len = sizeof(struct sockaddr_in);
#else
	int addr_len = sizeof(struct sockaddr_in);
#endif

	ret = sendto(transport->cmd_sock, buf, len, 0,
			(struct sockaddr *)&transport->cmd_addr, sizeof(transport->cmd_addr));
	if(ret < 0){
		//debug(DEBUG_ERR, "Send cmd error\n");
		return ret;
	}

	if(wait_for_recv_ready(transport->cmd_sock, 3)) {
		ret = recvfrom(transport->cmd_sock, buf, len, 0,
				(struct sockaddr *)&transport->cmd_addr, &addr_len);
		if(ret < 0) {
			//debug(DEBUG_ERR, "Recv cmd error!\n");
			return ret;
		}
	} else {
		//debug(DEBUG_ERR, "Recv cmd timeout!\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int32_t __trans_cmd_recv(YUNSDR_TRANSPORT *transport, void *buf, uint32_t len)
{
	int ret;
#if defined(__GNUC__)
	socklen_t addr_len = sizeof(struct sockaddr_in);
#else
	int addr_len = sizeof(struct sockaddr_in);
#endif

	if(wait_for_recv_ready(transport->cmd_sock, 3)) {
		ret = recvfrom(transport->cmd_sock, buf, len, 0,
				(struct sockaddr *)&transport->cmd_addr, &addr_len);
		if(ret < 0) {
			//debug(DEBUG_ERR, "Recv cmd error!\n");
			return ret;
		}
	} else {
		//debug(DEBUG_ERR, "Recv cmd timeout!\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int32_t __yunsdr_init_meta(YUNSDR_META **meta)
{
	int32_t ret;

	*meta = (YUNSDR_META *)calloc(1, MAX_TX_BUFFSIZE + sizeof(YUNSDR_META));
	if (!*meta) {
		ret = -1;
	} else {
		ret = 0;
	}

	return ret;
}

static void __yunsdr_deinit_meta(YUNSDR_META *meta)
{
	free(meta);
}

YUNSDR_DESCRIPTOR *yunsdr_open_device(const char *ip)
{
	YUNSDR_DESCRIPTOR *yunsdr;
	yunsdr = (YUNSDR_DESCRIPTOR *)calloc(1, sizeof(
			YUNSDR_DESCRIPTOR));
	if (!yunsdr) {
		//return (YUNSDR_DESCRIPTOR *)(-ENOMEM);
		return NULL;
	}

	memcpy(yunsdr->ipaddr, ip, strlen(ip));

	if(__yunsdr_init_transport(yunsdr, TRANSPORT_CMD) < 0)
		goto err_init_transport;

	return yunsdr;

	err_init_transport:
	free(yunsdr);
	//return (YUNSDR_DESCRIPTOR *)(-EINVAL);
	return NULL;
}

int32_t yunsdr_close_device(YUNSDR_DESCRIPTOR *yunsdr)
{
	int ret;

	ret = 0;

#ifdef YUNSDR_HAVE_DATA_LINK
	yunsdr_disable_rx(yunsdr);
	yunsdr_disable_tx(yunsdr);
	if(yunsdr->tx_meta)
		__yunsdr_deinit_meta(yunsdr->tx_meta);
	if(yunsdr->rx_meta)
		__yunsdr_deinit_meta(yunsdr->rx_meta);
#endif
	if(yunsdr->transport)
		__yunsdr_deinit_transport(yunsdr, TRANSPORT_CMD);

	free(yunsdr);

	return ret;
}

/* Set the receive RF gain for the selected channel. */
int32_t yunsdr_set_rx_rf_gain (YUNSDR_DESCRIPTOR *yunsdr,
		RF_CHANNEL ch, int32_t gain_db)
{
	uint32_t cmd[2];

	if(ch == RX1_CHANNEL) {
		cmd[0] = 0xf0220000|(25<<8);
		cmd[1] = gain_db;
	} else {
		cmd[0] = 0xf0220000|(27<<8);
		cmd[1] = gain_db;
	}

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

/* Set the RX RF bandwidth. */
int32_t yunsdr_set_rx_rf_bandwidth (YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t bandwidth_hz)
{
	uint32_t cmd[2];

	cmd[0] = 0xf0220000|(19<<8);
	cmd[1] = bandwidth_hz;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

/* Set the RX sampling frequency. */
int32_t yunsdr_set_rx_sampling_freq (YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t sampling_freq_hz)
{
	uint32_t cmd[2];
	cmd[0] = 0xf0220000|(17<<8);
	cmd[1] = sampling_freq_hz;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

/* Set the RX LO frequency. */
int32_t yunsdr_set_rx_lo_freq (YUNSDR_DESCRIPTOR *yunsdr,
		uint64_t lo_freq_hz)
{
	uint32_t cmd[2];

	if (lo_freq_hz > UINT32_MAX)
		cmd[0] = 0xf0220000 | (15 << 8) | 1;
	else
		cmd[0] = 0xf0220000 | (15 << 8);

	cmd[1] = (uint32_t)lo_freq_hz;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

/* Set the gain control mode for the selected channel. */
int32_t yunsdr_set_rx_gain_control_mode (YUNSDR_DESCRIPTOR *yunsdr,
		RF_CHANNEL ch, RF_GAIN_CTRL_MODE gc_mode)
{
	uint32_t cmd[2];

	if(ch == RX1_CHANNEL) {
		cmd[0] = 0xf0220000|(21<<8);
		cmd[1] = gc_mode;
	} else {
		cmd[0] = 0xf0220000|(23<<8);
		cmd[1] = gc_mode;
	}

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

/* Set the transmit attenuation for the selected channel. */
int32_t yunsdr_set_tx_attenuation (YUNSDR_DESCRIPTOR *yunsdr,
		RF_CHANNEL ch, uint32_t attenuation_mdb)
{
	uint32_t cmd[2];

	if(ch == TX1_CHANNEL) {
		cmd[0] = 0xf0220000|(9<<8);
		cmd[1] = attenuation_mdb;
	} else {
		cmd[0] = 0xf0220000|(11<<8);
		cmd[1] = attenuation_mdb;
	}

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

/* Set the TX RF bandwidth. */
int32_t yunsdr_set_tx_rf_bandwidth (YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t  bandwidth_hz)
{
	uint32_t cmd[2];

	cmd[0] = 0xf0220000|(7<<8);
	cmd[1] = bandwidth_hz;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}


/* Set the TX sampling frequency. */
int32_t yunsdr_set_tx_sampling_freq (YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t sampling_freq_hz)
{
	uint32_t cmd[2];

	cmd[0] = 0xf0220000|(5<<8);
	cmd[1] = sampling_freq_hz;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

/* Set the TX LO frequency. */
int32_t yunsdr_set_tx_lo_freq (YUNSDR_DESCRIPTOR *yunsdr,
		uint64_t lo_freq_hz)
{
	uint32_t cmd[2];

	if(lo_freq_hz > UINT32_MAX) {
		cmd[0] = 0xf0220000|(3<<8)|1;
	} else {
		cmd[0] = 0xf0220000|(3<<8);
	}

	cmd[1] = (uint32_t)lo_freq_hz;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

/* Get current receive RF gain for the selected channel. */
int32_t yunsdr_get_rx_rf_gain (YUNSDR_DESCRIPTOR *yunsdr, RF_CHANNEL ch, int32_t *gain_db)
{
	int ret;
	uint32_t cmd[2];

	if(ch == RX1_CHANNEL) {
		cmd[0] = 0xf0220000|(24<<8);
		cmd[1] = 0;
	} else {
		cmd[0] = 0xf0220000|(26<<8);
		cmd[1] = 0;
	}

	ret = __trans_cmd_send_then_recv(yunsdr->transport, (void *)cmd, sizeof(cmd));
	if(ret < 0)
		*gain_db = 0;
	else
		*gain_db = cmd[1];

	return ret;
}

/* Get the RX RF bandwidth. */
int32_t yunsdr_get_rx_rf_bandwidth (YUNSDR_DESCRIPTOR *yunsdr, uint32_t *bandwidth_hz)
{
	int ret;
	uint32_t cmd[2];

	cmd[0] = 0xf0220000|(18<<8);
	cmd[1] = 0;

	ret = __trans_cmd_send_then_recv(yunsdr->transport, (void *)cmd, sizeof(cmd));
	if(ret < 0)
		*bandwidth_hz = 0;
	else
		*bandwidth_hz = cmd[1];

	return ret;
}

/* Get current RX sampling frequency. */
int32_t yunsdr_get_rx_sampling_freq (YUNSDR_DESCRIPTOR *yunsdr, uint32_t *sampling_freq_hz)
{
	int ret;
	uint32_t cmd[2];

	cmd[0] = 0xf0220000|(16<<8);
	cmd[1] = 0;

	ret = __trans_cmd_send_then_recv(yunsdr->transport, (void *)cmd, sizeof(cmd));
	if(ret < 0)
		*sampling_freq_hz = 0;
	else
		*sampling_freq_hz = cmd[1];

	return ret;
}

/* Get current RX LO frequency. */
int32_t yunsdr_get_rx_lo_freq (YUNSDR_DESCRIPTOR *yunsdr, uint64_t *lo_freq_hz)
{
	int ret;
	uint32_t cmd[2];

	cmd[0] = 0xf0220000|(14<<8);
	cmd[1] = 0;

	ret = __trans_cmd_send_then_recv(yunsdr->transport, (void *)cmd, sizeof(cmd));
	if(ret < 0)
		*lo_freq_hz = 0;
	else
		*lo_freq_hz = cmd[1];

	return ret;
}

/* Get the gain control mode for the selected channel. */
int32_t yunsdr_get_rx_gain_control_mode (YUNSDR_DESCRIPTOR *yunsdr, RF_CHANNEL ch, uint8_t *gc_mode)
{
	int ret;
	uint32_t cmd[2];

	if(ch == RX1_CHANNEL) {
		cmd[0] = 0xf0220000|(20<<8);
		cmd[1] = 0;
	} else {
		cmd[0] = 0xf0220000|(22<<8);
		cmd[1] = 0;
	}

	ret = __trans_cmd_send_then_recv(yunsdr->transport, (void *)cmd, sizeof(cmd));
	if(ret < 0)
		*gc_mode = 0;
	else
		*gc_mode = cmd[1];

	return ret;
}

/* Get current transmit attenuation for the selected channel. */
int32_t yunsdr_get_tx_attenuation (YUNSDR_DESCRIPTOR *yunsdr, RF_CHANNEL ch, uint32_t *attenuation_mdb)
{
	int ret;
	uint32_t cmd[2];

	if(ch == TX1_CHANNEL) {
		cmd[0] = 0xf0220000|(8<<8);
		cmd[1] = 0;
	} else {
		cmd[0] = 0xf0220000|(10<<8);
		cmd[1] = 0;
	}

	ret = __trans_cmd_send_then_recv(yunsdr->transport, (void *)cmd, sizeof(cmd));
	if(ret < 0)
		*attenuation_mdb = 0;
	else
		*attenuation_mdb = cmd[1];

	return ret;
}

/* Get the TX RF bandwidth. */
int32_t yunsdr_get_tx_rf_bandwidth (YUNSDR_DESCRIPTOR *yunsdr, uint32_t *bandwidth_hz)
{
	int ret;
	uint32_t cmd[2];

	cmd[0] = 0xf0220000|(6<<8);
	cmd[1] = 0;

	ret = __trans_cmd_send_then_recv(yunsdr->transport, (void *)cmd, sizeof(cmd));
	if(ret < 0)
		*bandwidth_hz = 0;
	else
		*bandwidth_hz = cmd[1];

	return ret;
}

/* Get current TX sampling frequency. */
int32_t yunsdr_get_tx_sampling_freq (YUNSDR_DESCRIPTOR *yunsdr, uint32_t *sampling_freq_hz)
{
	int ret;
	uint32_t cmd[2];

	cmd[0] = 0xf0220000|(4<<8);
	cmd[1] = 0;

	ret = __trans_cmd_send_then_recv(yunsdr->transport, (void *)cmd, sizeof(cmd));
	if(ret < 0)
		*sampling_freq_hz = 0;
	else
		*sampling_freq_hz = cmd[1];

	return ret;
}

/* Get current TX LO frequency. */
int32_t yunsdr_get_tx_lo_freq (YUNSDR_DESCRIPTOR *yunsdr, uint64_t *lo_freq_hz)
{
	int ret;
	uint32_t cmd[2];

	cmd[0] = 0xf0220000|(2<<8);
	cmd[1] = 0;

	ret = __trans_cmd_send_then_recv(yunsdr->transport, (void *)cmd, sizeof(cmd));
	if(ret < 0)
		*lo_freq_hz = 0;
	else
		*lo_freq_hz = ((uint64_t)cmd[0]&0x1)<<32|cmd[1];

	return ret;
}

int32_t yunsdr_dump_register (YUNSDR_DESCRIPTOR *yunsdr,
		uint16_t regid, uint32_t *reg_val)
{
	int ret;
	uint32_t cmd[2];

	cmd[0] = 0xf0250000;
	cmd[1] = regid;

	ret = __trans_cmd_send_then_recv(yunsdr->transport, (void *)cmd, sizeof(cmd));
	if(ret < 0)
		*reg_val = 0xFFFFFFFF;
	else
		*reg_val = cmd[1];

	return ret;
}

int32_t yunsdr_set_trx_channel (YUNSDR_DESCRIPTOR *yunsdr, uint8_t ch)
{
	uint32_t cmd[2];

	switch (ch) {
	case TX1_CHANNEL:
		cmd[0] = 0xf0200000;
		cmd[1] = 1;
		break;
	case TX2_CHANNEL:
		cmd[0] = 0xf0200000;
		cmd[1] = 2;
		break;
	case RX1_CHANNEL:
		cmd[0] = 0xf0210000;
		cmd[1] = 1;
		break;
	case RX2_CHANNEL:
		cmd[0] = 0xf0210000;
		cmd[1] = 2;
		break;
	case TX_DUALCHANNEL:
		cmd[0] = 0xf0200000;
		cmd[1] = 3;
		break;
	case RX_DUALCHANNEL:
		cmd[0] = 0xf0210000;
		cmd[1] = 3;
		break;
	default:
		break;
	}

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

int32_t yunsdr_calibrate_qec (YUNSDR_DESCRIPTOR *yunsdr, uint8_t val)
{
	uint32_t cmd[2];

	cmd[0] = 0xf0190000;
	cmd[1] = val;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

int32_t yunsdr_set_tone_bist (YUNSDR_DESCRIPTOR *yunsdr, uint8_t enable)
{
	uint32_t cmd[2];

	cmd[0] = 0xf0240000;
	cmd[1] = enable;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

int32_t yunsdr_set_ref_clock (YUNSDR_DESCRIPTOR *yunsdr,
		REF_SELECT select)
{
	uint32_t cmd[2];

	cmd[0] = 0xf0220000|(40<<8);
	cmd[1] = select;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

int32_t yunsdr_set_vco_select (YUNSDR_DESCRIPTOR *yunsdr,
		VCO_CAL_SELECT select)
{
	uint32_t cmd[2];

	cmd[0] = 0xf0220000|(41<<8);
	cmd[1] = select;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

int32_t yunsdr_set_trx_select (YUNSDR_DESCRIPTOR *yunsdr,
		TRX_SWITCH select)
{
	uint32_t cmd[2];

	cmd[0] = 0xf0220000|(43<<8);
	cmd[1] = select;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

int32_t yunsdr_set_duplex_select (YUNSDR_DESCRIPTOR *yunsdr,
		FDD_TDD_SEL select)
{
	uint32_t cmd[2];

	cmd[0] = 0xf0220000|(42<<8);
	cmd[1] = select;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

int32_t yunsdr_set_auxdac1 (YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t val)
{
	uint32_t cmd[2];

	cmd[0] = 0xf0220000|(44<<8);
	cmd[1] = val;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

int32_t yunsdr_set_adf4001 (YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t val)
{
	uint32_t cmd[2];

	cmd[0] = 0xf0220000|(45<<8);
	cmd[1] = val;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

int yunsdr_get_disk_status(YUNSDR_DESCRIPTOR *yunsdr, uint8_t *percent)
{
	int ret;
	uint32_t cmd[2];

	cmd[0] = 0xF0A30000;
	cmd[1] = 0;

	ret = __trans_cmd_send_then_recv(yunsdr->transport, (void *)cmd, sizeof(cmd));
	if(ret < 0)
		*percent = 0xFF;
	else
		*percent = cmd[1]&0xFF;

	return ret;
}

int yunsdr_remote_reboot(YUNSDR_DESCRIPTOR *yunsdr)
{
	uint32_t cmd[2];

	cmd[0] = 0xF0AA0000;
	cmd[1] = 0;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}
int yunsdr_say_hello(YUNSDR_DESCRIPTOR *yunsdr)
{
	int ret;
	uint8_t i;
	uint32_t cmd[2];

	cmd[0] = 0xF0BB0000;
	cmd[1] = 0;

	i = 5;
	printf("\n");
	while(i) {
		ret = __trans_cmd_send_then_recv(yunsdr->transport, (void *)cmd, sizeof(cmd));
		if(ret < 0) {
			if(ret == -ETIMEDOUT) {
				i--;
				printf(". ");
				continue;
			} else {
				printf("Send cmd error!\n");
				return ret;
			}
		} else {
			if(cmd[1] == 0x55AAAA55) {
				return 0;
			} else {
				printf("\n");
				return -1;
			}
		}
	}

	return -1;
}

#ifdef YUNSDR_HAVE_TIMESTAMP
int32_t yunsdr_enable_timestamp (YUNSDR_DESCRIPTOR *yunsdr, PPS_MODE pps)
{
	uint32_t cmd[2];

	cmd[0] = 0xf0230000;
	switch (pps) {
	case PPS_INTERNAL_EN:
		cmd[1] = 3;
		break;
	case PPS_EXTERNAL_EN:
		cmd[1] = 4;
		break;
	case PPS_ALL_DISABLE:
		cmd[1] = 1;
		break;
	}

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
}

int32_t yunsdr_disable_timestamp (YUNSDR_DESCRIPTOR *yunsdr)
{
	uint32_t cmd[2];

	cmd[0] = 0xf0230000;
	cmd[1] = 2;

	return __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));

}
int32_t yunsdr_read_timestamp (YUNSDR_DESCRIPTOR *yunsdr,
		uint64_t *timestamp)
{
	int ret;
	uint32_t cmd[2];

	cmd[0] = 0xf0230000;
	cmd[1] = 5;

	ret = __trans_cmd_send_then_recv(yunsdr->transport, (void *)cmd, sizeof(cmd));
	if(ret < 0)
		*timestamp = 0;
	else
		*timestamp = (uint64_t)cmd[1]<<32|cmd[0];

	return ret;
}

int32_t yunsdr_read_gps (YUNSDR_DESCRIPTOR *yunsdr,
		GPS *pgps)
{
	int ret;
	uint32_t cmd[2];

	assert(pgps);

	cmd[0] = 0xf0240000;
	cmd[1] = 5;
	ret = __trans_cmd_send(yunsdr->transport, (void *)cmd, sizeof(cmd));
	if(ret < 0) {
		debug(DEBUG_ERR, "Send cmd error!\n");
		return ret;
	}

	ret = __trans_cmd_recv(yunsdr->transport, (void *)pgps, sizeof(*pgps));
	if(ret < 0) {
		debug(DEBUG_ERR, "Recv cmd error!\n");
		return ret;
	} else {
		pgps->hours += 8;
		pgps->latitude = pgps->latitude*180/3.1415926;
		pgps->longitude = pgps->longitude*180/3.1415926;
	}

	return ret;
}

int32_t yunsdr_enable_tx(YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t nsamples,
		RF_MODE mode)
{
	int ret;
	YUNSDR_META tx_meta;
	if(!yunsdr->transport)
		return -1;
	if(!(yunsdr->status&DEV_ENABLE_TX)) {
		if(__yunsdr_init_transport(yunsdr, TRANSPORT_TX) < 0)
			return -1;
		if(!yunsdr->tx_meta) {
			if(__yunsdr_init_meta(&yunsdr->tx_meta) < 0) {
				__yunsdr_deinit_transport(yunsdr, TRANSPORT_TX);
				return -1;
			}
		}
	}

	yunsdr->tx_meta->head = (mode << 24);

	if(mode == STOP_TX_LOOP) {
		tx_meta.head = IDENTIFIER_CODE | (STOP_TX_LOOP << 24);
		tx_meta.nsamples = 0;

		ret = send(yunsdr->transport->tx_sock, (void *)&tx_meta, sizeof(YUNSDR_META), 0);
		if(ret < 0) {
			printf("[%s]send error!\n", yunsdr->ipaddr);
			return ret;
		}
	}

	return 0;
}

int32_t yunsdr_disable_tx(YUNSDR_DESCRIPTOR *yunsdr)
{
	if(!yunsdr->transport)
		return -1;
	if(yunsdr->status&DEV_ENABLE_TX) {
		__yunsdr_deinit_transport(yunsdr, TRANSPORT_TX);
		if(yunsdr->tx_meta)
			__yunsdr_deinit_transport(yunsdr, TRANSPORT_TX);
	}

	return 0;
}

static uint32_t nbyte_per_frame = 0;
int32_t yunsdr_enable_rx(YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t nsamples,
		RF_CHANNEL rf_port,
		RF_MODE mode,
		uint64_t timestamp)
{
	int ret;
	//YUNSDR_META rx_meta;
	if(!yunsdr->transport)
		return -1;
	if(!(yunsdr->status&DEV_ENABLE_RX)) {
		if(__yunsdr_init_transport(yunsdr, TRANSPORT_RX) < 0)
			return -1;
		if(!yunsdr->rx_meta) {
			if(__yunsdr_init_meta(&yunsdr->rx_meta) < 0) {
				__yunsdr_deinit_transport(yunsdr, TRANSPORT_RX);
				return -1;
			}
		}
	}

	rf_port &= 0x3;
	if(nsamples*(rf_port == 3?8:4) > MAX_TX_BUFFSIZE)
		return -1;
	if(timestamp > 0 && mode == START_RX_BURST)
		yunsdr->rx_meta->head = (rf_port<<16) | (1<<19) | IDENTIFIER_CODE | (START_RX_BURST << 24);
	else
		yunsdr->rx_meta->head = (rf_port<<16) | IDENTIFIER_CODE | (mode << 24);

	yunsdr->rx_meta->nsamples = nsamples;
	yunsdr->rx_meta->timestamp_l = (uint32_t)timestamp;
	yunsdr->rx_meta->timestamp_h = timestamp >> 32;
	nbyte_per_frame = yunsdr->rx_meta->nsamples *
			((yunsdr->rx_meta->head>>16&0x3) == 3?8:4) + sizeof(YUNSDR_META);
	ret = send(yunsdr->transport->rx_sock, (void *)yunsdr->rx_meta, sizeof(YUNSDR_META), 0);
	if(ret < 0) {
		printf("[%s]send error!\n", yunsdr->ipaddr);
		return ret;
	}

	return 0;
}

int32_t yunsdr_disable_rx(YUNSDR_DESCRIPTOR *yunsdr)
{
	if(!yunsdr->transport)
		return -1;
	if(yunsdr->status&DEV_ENABLE_RX) {
		__yunsdr_deinit_transport(yunsdr, TRANSPORT_RX);
		if(yunsdr->rx_meta)
			__yunsdr_deinit_transport(yunsdr, TRANSPORT_RX);
	}

	return 0;
}
#endif

#if (defined YUNSDR_HAVE_DATA_LINK) && (defined YUNSDR_HAVE_TIMESTAMP)
int32_t yunsdr_read_samples(YUNSDR_DESCRIPTOR *yunsdr,
		void **buffer,
		uint32_t nbyte,
		uint64_t *timestamp,
		double timeout)
{
	int ret;

	uint32_t nrecv = 0;
	uint32_t count = 0;

	if(wait_for_recv_ready(yunsdr->transport->rx_sock, timeout)) {
		do {
			retry:		ret = recv(yunsdr->transport->rx_sock, (char *)yunsdr->rx_meta+count, nbyte_per_frame-count, MSG_WAITALL);
			if(ret < 0) {
				if(errno == EINTR)
					goto retry;
#if defined(__WINDOWS_) || defined(_WIN32)
				debug(DEBUG_ERR, "recv data error %d!\n", GetLastError());
#else
				debug(DEBUG_ERR, "recv data error %s!\n", strerror(errno));
#endif
				break;
			}
			count += ret;
		} while(count != nbyte_per_frame);
		nrecv = yunsdr->rx_meta->nsamples * ((yunsdr->rx_meta->head>>16&0x3) == 3?8:4);
		*timestamp = (uint64_t)yunsdr->rx_meta->timestamp_h<<32|yunsdr->rx_meta->timestamp_l;
		memcpy((char *)buffer[0], yunsdr->rx_meta->payload, count - sizeof(YUNSDR_META));
	} else {
		count = -1;
	}
	return count - sizeof(YUNSDR_META);
}

int32_t yunsdr_write_samples(YUNSDR_DESCRIPTOR *yunsdr,
		void **buffer,
		uint32_t nbyte,
		RF_CHANNEL  rf_port,
		uint64_t timestamp)
{
	int ret;
	uint8_t mode;
	uint8_t timestamp_en;
	uint32_t ncopy;

	ncopy = MIN(nbyte + sizeof(YUNSDR_META), MAX_TX_BUFFSIZE);
	ncopy -= sizeof(YUNSDR_META);
	mode = yunsdr->tx_meta->head>>24;

	if(timestamp > 0 && mode != START_TX_LOOP)
		timestamp_en = 1;
	else
		timestamp_en = 0;

	yunsdr->tx_meta->timestamp_l = (uint32_t)timestamp;
	yunsdr->tx_meta->timestamp_h = (uint32_t)(timestamp>>32);

	if(*buffer != NULL && ncopy > 0)
		memcpy(yunsdr->tx_meta->payload, buffer[0], ncopy);

	rf_port &= 0x3;
	yunsdr->tx_meta->head = (yunsdr->tx_meta->head&0xFF000000) | (rf_port<<16) |
			(timestamp_en<<19) | IDENTIFIER_CODE;
	yunsdr->tx_meta->nsamples = ncopy/(rf_port == 3?8:4);

	ret = send(yunsdr->transport->tx_sock, (void *)(yunsdr->tx_meta),
			sizeof(YUNSDR_META) + ncopy, 0);
	if(ret < 0) {
		printf("[%s]send error!\n", yunsdr->ipaddr);
		return -1;
	}

	return ncopy;
}
#endif

#ifdef YUNSDR_HAVE_DATA_LINK
static SOCKET yunsdr_init_data_socket(const char *ip, int port)
{
	int err;
#if defined(__WINDOWS_) || defined(_WIN32)
	WSADATA wsa;
	WORD wVersionRequested;
	WSADATA wsaData;

	WSAStartup(MAKEWORD(2,2), &wsa);
	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		debug(DEBUG_ERR,"WSAStartup failed with error: %d\n", err);
		return -1;
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		debug(DEBUG_ERR,"Could not find a usable version of Winsock.dll\n");
		WSACleanup();
		return -1;
	}
	else
		;//debug(DEBUG_INFO,"The Winsock 2.2 dll load success\n");
#endif
#if defined(__WINDOWS_) || defined(_WIN32)
	SOCKET sockfd;
	SOCKADDR_IN addr;
#else
	struct sockaddr_in addr;
	int sockfd;
#endif
	memset(&addr, 0, sizeof(addr));

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) <= 0) {
		debug(DEBUG_ERR,"Create data socket failed!\n");
		return -1;
	}
#if defined(__WINDOWS_) || defined(_WIN32)
	addr.sin_addr.S_un.S_addr = inet_addr(ip);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
#else
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(port);
#endif
	int buflen = 16*1024*1024;
	err = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (const char *)&buflen, sizeof(int));
	if(err < 0)
		debug(DEBUG_ERR,"setsockopt SO_SNDBUF failed, %s!\n", strerror(errno));

	err = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char *)&buflen, sizeof(int));
	if(err < 0)
		debug(DEBUG_ERR,"setsockopt SO_RCVBUF failed, %s!\n", strerror(errno));

	err = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
	if(err < 0) {
		debug(DEBUG_ERR,"Connect yunsdr failed!\n");
		return -1;
	}

	return sockfd;
}
#endif
static int yunsdr_init_cmd_socket(YUNSDR_TRANSPORT *transport,
		const char *ip, int port)
{
#if defined(__WINDOWS_) || defined(_WIN32)
	int err;
	WSADATA wsa;
	WORD wVersionRequested;
	WSADATA wsaData;

	WSAStartup(MAKEWORD(2,2), &wsa);
	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		debug(DEBUG_ERR, "WSAStartup failed with error: %d\n", err);
		return -1;
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		debug(DEBUG_ERR, "Could not find a usable version of Winsock.dll\n");
		WSACleanup();
		return -1;
	}
	else
		;//printf("The Winsock 2.2 dll was found okay\n");
#endif

	memset(&transport->cmd_addr, 0, sizeof(transport->cmd_addr));
	if((transport->cmd_sock = socket(AF_INET, SOCK_DGRAM, 0)) <= 0) {
		debug(DEBUG_ERR, "Create cmd socket fail!\n");
		return -1;
	}
#if defined(__WINDOWS_) || defined(_WIN32)
	transport->cmd_addr.sin_addr.S_un.S_addr = inet_addr(ip);
	transport->cmd_addr.sin_family = AF_INET;
	transport->cmd_addr.sin_port = htons(port);
	uint32_t mode = 1;  //non-blocking mode is enabled.
	ioctlsocket(transport->cmd_sock, FIONBIO, (u_long *)&mode); //设置为非阻塞模式
#else
	transport->cmd_addr.sin_family = AF_INET;
	transport->cmd_addr.sin_addr.s_addr = inet_addr(ip);
	transport->cmd_addr.sin_port = htons(port);
	fcntl(transport->cmd_sock, F_SETFL, O_NONBLOCK);
#endif

	return 0;
}

static int __yunsdr_init_transport(YUNSDR_DESCRIPTOR *yunsdr, enum TRANS_ID port)
{
	int ret;

	if(!yunsdr->transport) {
		yunsdr->transport = (YUNSDR_TRANSPORT *)calloc(1, sizeof(YUNSDR_TRANSPORT));
		if (!yunsdr->transport) {
			return -1;
		}
	}

	switch (port) {
	case TRANSPORT_CMD:
		ret = yunsdr_init_cmd_socket(yunsdr->transport, yunsdr->ipaddr, 5006);
		if(ret < 0) {
			free(yunsdr->transport);
			return -1;
		}
		ret = yunsdr_say_hello(yunsdr);
		if( ret < 0) {
			__yunsdr_deinit_transport(yunsdr, TRANSPORT_CMD);
			return -1;
		}
		yunsdr->status = DEV_CONNECTED;
		break;
	case TRANSPORT_TX:
		yunsdr->transport->tx_sock = yunsdr_init_data_socket(yunsdr->ipaddr, 5005);
		if(yunsdr->transport->tx_sock < 0)
			return -1;
		yunsdr->status |= DEV_ENABLE_TX;
		break;
	case TRANSPORT_RX:
		yunsdr->transport->rx_sock = yunsdr_init_data_socket(yunsdr->ipaddr, 5004);
		if(yunsdr->transport->rx_sock < 0)
			return -1;
		yunsdr->status |= DEV_ENABLE_RX;
		break;
	default:
		break;
	}

	return 0;
}

static void __yunsdr_deinit_transport(YUNSDR_DESCRIPTOR *yunsdr, enum TRANS_ID port)
{
	switch (port) {
	case TRANSPORT_CMD:
#if defined(__WINDOWS_) || defined(_WIN32)
		closesocket(yunsdr->transport->cmd_sock);
#else
		close(yunsdr->transport->cmd_sock);
#endif
		yunsdr->status &= ~DEV_CONNECTED;
		break;
	case TRANSPORT_TX:
#if defined(__WINDOWS_) || defined(_WIN32)
#ifdef YUNSDR_HAVE_DATA_LINK
		closesocket(yunsdr->transport->tx_sock);
#endif
#else
#ifdef YUNSDR_HAVE_DATA_LINK
		close(yunsdr->transport->tx_sock);
#endif
#endif
		yunsdr->status &= ~DEV_ENABLE_TX;
		break;
	case TRANSPORT_RX:
#if defined(__WINDOWS_) || defined(_WIN32)
#ifdef YUNSDR_HAVE_DATA_LINK
		closesocket(yunsdr->transport->rx_sock);
#endif
#else
#ifdef YUNSDR_HAVE_DATA_LINK
		close(yunsdr->transport->rx_sock);
#endif
#endif
		yunsdr->status &= ~DEV_ENABLE_RX;
		break;
	default:
		break;
	}

	if(!yunsdr->status) {
		free(yunsdr->transport);
#if defined(__WINDOWS_) || defined(_WIN32)
		WSACleanup();	//clean up Ws2_32.dll
#endif
	}
	return;
}

#ifdef __DEBUG__
void debug(int level, const char *fmt, ...)
{
	if(level <= DEBUG_OUTPUT_LEVEL) {
		va_list ap;
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}
}
#else
void debug(int level, const char *fmt, ...)
{
}
#endif
