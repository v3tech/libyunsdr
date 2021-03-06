/*
 * yunsdr_api.c
 *
 *  Created on: 2018/5/20
 *      Author: Eric
 */

#include <math.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#if !defined(__WINDOWS_) && !defined(_WIN32)
#include <unistd.h>
#else
#include <windows.h>
#endif
#include <immintrin.h>
#include "transport.h"
#include "yunsdr_api_ss.h"

#define MAX_TX_BULK_SIZE (8192*4)

#define MAX_TX_BUFFSIZE (32*1024*1024)
#define MAX_RX_BUFFSIZE (32*1024*1024)

#define MIN(x, y)  ((x < y)?x:y)


uint64_t yunsdr_ticksToTimeNs(const uint64_t ticks, const double rate)
{
    const long long ratell = (long long)(rate);
    const long long full = (long long)(ticks / ratell);
    const long long err = ticks - (full*ratell);
    const double part = full * (rate - ratell);
    const double frac = ((err - part) * 1000000000) / rate;
    return (full * 1000000000) + llround(frac);
}
uint64_t yunsdr_timeNsToTicks(const uint64_t timeNs, const double rate)
{
    const long long ratell = (long long)(rate);
    const long long full = (long long)(timeNs / 1000000000);
    const long long err = timeNs - (full * 1000000000);
    const double part = full * (rate - ratell);
    const double frac = part + ((err*rate) / 1000000000);
    return (full*ratell) + llround(frac);
}


YUNSDR_DESCRIPTOR *yunsdr_open_device(const char *url)
{
    int ret;
    YUNSDR_DESCRIPTOR *yunsdr;
    yunsdr = (YUNSDR_DESCRIPTOR *)calloc(1, sizeof(YUNSDR_DESCRIPTOR));
    if (!yunsdr) {
        //return (YUNSDR_DESCRIPTOR *)(-ENOMEM);
        return NULL;
    }

    yunsdr->trans = (YUNSDR_TRANSPORT *)calloc(1, sizeof(YUNSDR_TRANSPORT));
    if (!yunsdr->trans) {
        free(yunsdr);
        return NULL;
    }
    /* Create debug console window */
#if defined(__WINDOWS_) || defined(_WIN32)
#define _MYDEBUG
#ifdef  _MYDEBUG
    if (AllocConsole()) {
        FILE* f;
        freopen_s(&f, "CONOUT$", "wt", stdout);
        freopen("CONIN$", "r+t", stdin);
    }
#endif
#endif
    char string[20];
    strcpy(string, url);

    char *prefix = strtok(string, ":");
    if (!strcmp(prefix, "pcie")) {
        static int devid = 0;
        printf("Using pcie interface.\n");
        yunsdr->trans->type = INTERFACE_PCIE;
        devid = atoi(strtok(NULL, ":"));
        yunsdr->trans->app_opaque = &devid;
    }
    else if (!strcmp(prefix, "sfp")) {
        printf("Using sfp interface.\n");
        yunsdr->trans->type = INTERFACE_SFP;
        yunsdr->trans->app_opaque = strtok(NULL, ":");
    }
    else if (!strcmp(prefix, "gigabit")) {
        printf("Using gigabit interface.\n");
        yunsdr->trans->type = INTERFACE_GIGABIT;
        yunsdr->trans->app_opaque = strtok(NULL, ":");
    }
    else {
        printf("Not support interface.\n");
        free(yunsdr->trans);
        free(yunsdr);
        return NULL;
    }

    ret = __init_transport(yunsdr->trans);
    if (ret < 0) {
        printf("Can not init interface.\n");
        free(yunsdr->trans);
        free(yunsdr);
        return NULL;
    }

    return yunsdr;

}

int32_t yunsdr_close_device(YUNSDR_DESCRIPTOR *yunsdr)
{
    __deinit_transport(yunsdr->trans);
    free(yunsdr->trans);
    free(yunsdr);

    return 0;
}

/* Get current TX LO frequency. */
int32_t yunsdr_get_tx_lo_freq(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint64_t *lo_freq_hz)
{
    return yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 2, lo_freq_hz, sizeof(uint64_t), 0);
}

/* Get current TX sampling frequency. */
int32_t yunsdr_get_tx_sampling_freq(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t *sampling_freq_hz)
{
    return yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 3, sampling_freq_hz, sizeof(uint32_t), 0);
}

/* Get the TX RF bandwidth. */
int32_t yunsdr_get_tx_rf_bandwidth(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t *bandwidth_hz)
{
    return yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 4, bandwidth_hz, sizeof(uint32_t), 0);
}

/* Get current transmit attenuation for the selected channel. */
int32_t yunsdr_get_tx1_attenuation(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t *attenuation_mdb)
{

    return yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 5, attenuation_mdb, sizeof(uint32_t), 0);
}

/* Get current transmit attenuation for the selected channel. */
int32_t yunsdr_get_tx2_attenuation(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t *attenuation_mdb)
{

    return yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 6, attenuation_mdb, sizeof(uint32_t), 0);
}

/* Get current RX LO frequency. */
int32_t yunsdr_get_rx_lo_freq(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint64_t *lo_freq_hz)
{

    return yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 8, lo_freq_hz, sizeof(uint64_t), 0);
}

/* Get the RX RF bandwidth. */
int32_t yunsdr_get_rx_rf_bandwidth(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t *bandwidth_hz)
{

    return yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 9, bandwidth_hz, sizeof(uint32_t), 0);
}

/* Get the gain control mode for the selected channel. */
int32_t yunsdr_get_rx1_gain_control_mode(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        RF_GAIN_CTRL_MODE *gc_mode)
{

    return yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 10, gc_mode, sizeof(uint8_t), 0);
}

/* Get the gain control mode for the selected channel. */
int32_t yunsdr_get_rx2_gain_control_mode(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        RF_GAIN_CTRL_MODE *gc_mode)
{
    return yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 11, gc_mode, sizeof(uint8_t), 0);
}

/* Get current receive RF gain for the selected channel. */
int32_t yunsdr_get_rx1_rf_gain(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        int32_t *gain_db)
{

    return yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 12, gain_db, sizeof(uint32_t), 0);
}

/* Get current receive RF gain for the selected channel. */
int32_t yunsdr_get_rx2_rf_gain(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        int32_t *gain_db)
{
    return  yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 13, gain_db, sizeof(uint32_t), 0);
}


/* Set the RX LO frequency. */
int32_t yunsdr_set_rx_lo_freq(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint64_t lo_freq_hz)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 8, &lo_freq_hz, sizeof(uint64_t));
}

/* Set the RX RF bandwidth. */
int32_t yunsdr_set_rx_rf_bandwidth(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t  bandwidth_hz)
{

    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 9, &bandwidth_hz, sizeof(uint32_t));
}

/* Set the RX sampling frequency. */
int32_t yunsdr_set_rx_sampling_freq(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t sampling_freq_hz)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 3, &sampling_freq_hz, sizeof(uint32_t));
}

/* Set the gain control mode for the selected channel. */
int32_t yunsdr_set_rx1_gain_control_mode(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        RF_GAIN_CTRL_MODE gc_mode)
{

    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 10, &gc_mode, sizeof(uint8_t));
}

int32_t yunsdr_set_rx2_gain_control_mode(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        RF_GAIN_CTRL_MODE gc_mode)
{

    return  yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 11, &gc_mode, sizeof(uint8_t));
}
/* Set the receive RF gain for the selected channel. */
int32_t yunsdr_set_rx1_rf_gain(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        int32_t gain_db)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 12, &gain_db, sizeof(uint32_t));
}

/* Set the receive RF gain for the selected channel. */
int32_t yunsdr_set_rx2_rf_gain(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        int32_t gain_db)
{
    return  yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 13, &gain_db, sizeof(uint32_t));
}

int32_t yunsdr_set_rx_fir_en_dis(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t enable)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 14, &enable, sizeof(uint32_t));
}

/* Set the TX LO frequency. */
int32_t yunsdr_set_tx_lo_freq(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint64_t lo_freq_hz)
{

    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 2, &lo_freq_hz, sizeof(uint64_t));
}

/* Set the TX RF bandwidth. */
int32_t yunsdr_set_tx_rf_bandwidth(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t  bandwidth_hz)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 4, &bandwidth_hz, sizeof(uint32_t));
}

/* Set the TX sampling frequency. */
int32_t yunsdr_set_tx_sampling_freq(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t sampling_freq_hz)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 3, &sampling_freq_hz, sizeof(uint32_t));
}

/* Set the transmit attenuation for the selected channel. */
int32_t yunsdr_set_tx1_attenuation(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t attenuation_mdb)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 5, &attenuation_mdb, sizeof(uint32_t));
}

/* Set the transmit attenuation for the selected channel. */
int32_t yunsdr_set_tx2_attenuation(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t attenuation_mdb)
{
    return  yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 6, &attenuation_mdb, sizeof(uint32_t));
}

int32_t yunsdr_set_tx_fir_en_dis(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t status)
{
    return  yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 7, &status, sizeof(uint8_t));
}

int32_t yunsdr_get_rfchip_reg(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id, uint32_t reg,
        uint32_t *value)
{
    int ret;

    ret = yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 1, &reg, sizeof(uint32_t), 1);
    if(ret)
        *value = 0xFFFFFFFF;
    else
        *value = reg;

    return ret;
}

int32_t yunsdr_set_rfchip_reg(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id, uint32_t reg,
        uint32_t value)
{
    uint64_t parameter = (uint64_t)value<<32 | reg;
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 1, &parameter, sizeof(uint64_t));
}

int32_t yunsdr_set_tx_lo_int_ext(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t enable)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 15, &enable, sizeof(uint8_t));
}

int32_t yunsdr_set_rx_lo_int_ext(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t enable)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 16, &enable, sizeof(uint8_t));
}

int32_t yunsdr_set_ext_lo_freq(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint64_t lo_freq_hz)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 17, &lo_freq_hz, sizeof(uint64_t));
}

int32_t yunsdr_do_mcs(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t enable)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 18, &enable, sizeof(uint8_t));
}

int32_t yunsdr_set_rx_ant_enable(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t enable)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 19, &enable, sizeof(uint8_t));
}

int32_t yunsdr_set_ref_clock(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        REF_SELECT source)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 20, &source, sizeof(uint8_t));
}

int32_t yunsdr_set_vco_select(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        VCO_CAL_SELECT vco)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 21, &vco, sizeof(uint8_t));
}

int32_t yunsdr_tx_cyclic_enable(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t enable)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 24, &enable, sizeof(uint8_t));
}

int32_t yunsdr_set_auxdac1(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t vol_mV)
{

    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 25, &vol_mV, sizeof(uint32_t));
}

int32_t yunsdr_set_duplex_select(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        DUPLEX_SELECT duplex)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 27, &duplex, sizeof(uint8_t));
}

int32_t yunsdr_set_trxsw_fpga_enable(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t enable)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 64, &enable, sizeof(uint8_t));
}

int32_t yunsdr_set_rxchannel_coef(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id, RF_RX_CHANNEL channel, int16_t coef1, int16_t coef2)
{
    uint32_t param = (uint32_t)coef2 << 16 | (uint16_t)coef1;

    switch (channel) {
    case RX2_CHANNEL:
        if(yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 35, &param, sizeof(uint32_t)) < 0)
            return -1;
        break;
    case RX3_CHANNEL:
        if(yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 36, &param, sizeof(uint32_t)) < 0)
            return -1;
        break;
    case RX4_CHANNEL:
        if(yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 37, &param, sizeof(uint32_t)) < 0)
            return -1;
        break;
    case RX1_CHANNEL:
    default:
        break;
    }

    return 0;
}

int32_t yunsdr_set_txchannel_coef(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id, RF_TX_CHANNEL channel, int16_t coef1, int16_t coef2)
{
    uint32_t param = (uint32_t)coef2 << 16 | (uint16_t)coef1;

    switch (channel) {
    case TX2_CHANNEL:
        if(yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 39, &param, sizeof(uint32_t)) < 0)
            return -1;
        break;
    case TX3_CHANNEL:
        if(yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 40, &param, sizeof(uint32_t)) < 0)
            return -1;
        break;
    case TX4_CHANNEL:
        if(yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 41, &param, sizeof(uint32_t)) < 0)
            return -1;
        break;
    case TX1_CHANNEL:
    default:
        break;
    }

    return 0;
}

int32_t yunsdr_enable_rxchannel_corr(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id, RF_RX_CHANNEL channel, uint8_t enable)
{
    uint32_t status;

    if(yunsdr->trans->cmd_send_then_recv(yunsdr->trans, 0, 38, &status, sizeof(uint32_t), 0) < 0)
        return -1;
    switch (channel) {
    case RX2_CHANNEL:
    case RX3_CHANNEL:
    case RX4_CHANNEL:
        if(enable)
            status |= (1 << (channel - 2));
        else
            status &= (~(1 << (channel - 2)));
        break;
    case RX1_CHANNEL:
    default:
        break;
    }

    if(yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 38, &status, sizeof(uint32_t)) < 0)
        return -1;

    return 0;
}

int32_t yunsdr_enable_txchannel_corr(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id, RF_TX_CHANNEL channel, uint8_t enable)
{
    uint32_t status;

    if(yunsdr->trans->cmd_send_then_recv(yunsdr->trans, 0, 38, &status, sizeof(uint32_t), 0) < 0)
        return -1;
    switch (channel) {
    case TX2_CHANNEL:
    case TX3_CHANNEL:
    case TX4_CHANNEL:
        if(enable)
            status |= (1 << (channel + 1));
        else
            status &= (~(1 << (channel + 1)));
        break;
    case TX1_CHANNEL:
    default:
        break;
    }

    if(yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 38, &status, sizeof(uint32_t)) < 0)
        return -1;

    return 0;
}



int32_t yunsdr_set_hwbuf_depth(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id, uint32_t depth)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 70, &depth, sizeof(uint32_t));
}

int32_t yunsdr_get_hwbuf_depth(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id, uint32_t *depth)
{
    return yunsdr->trans->cmd_send_then_recv(yunsdr->trans, 0, 70, depth, sizeof(uint32_t), 0);
}

int32_t yunsdr_get_firmware_version(YUNSDR_DESCRIPTOR *yunsdr, uint32_t *version)
{
    uint32_t fpga_ver, arm_ver;

    if(yunsdr->trans->cmd_send_then_recv(yunsdr->trans, 0, 100, &fpga_ver, sizeof(uint32_t), 0) < 0)
        return -1;
    if(yunsdr->trans->cmd_send_then_recv(yunsdr->trans, 0, 101, &arm_ver, sizeof(uint32_t), 0) < 0)
        return -1;
    *version = (fpga_ver&0xffff) | (arm_ver << 16);

    return 0;
}
int32_t yunsdr_get_model_version(YUNSDR_DESCRIPTOR *yunsdr, uint32_t *version)
{
    return yunsdr->trans->cmd_send_then_recv(yunsdr->trans, 0, 102, version, sizeof(uint32_t), 0);
}

int32_t yunsdr_get_channel_event(YUNSDR_DESCRIPTOR *yunsdr, CHANNEL_EVENT event, uint8_t channel, uint32_t *count)
{
    return yunsdr->trans->cmd_send_then_recv(yunsdr->trans, channel, event, count, sizeof(uint32_t), 0);
}
/* Get current RX sampling frequency. */

int32_t yunsdr_enable_timestamp(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t enable)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 23, &enable, sizeof(uint64_t));
}

int32_t yunsdr_read_timestamp(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint64_t *timestamp)
{
    return  yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 28, timestamp, sizeof(uint64_t), 0);
}

DLLEXPORT int32_t yunsdr_set_pps_select (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id, PPSModeEnum pps)
{
    return yunsdr->trans->cmd_send(yunsdr->trans, rf_id, 22, &pps, sizeof(uint32_t));
}

int32_t yunsdr_get_utc(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id, struct tm *ltime)
{
    int32_t ret;
    uint64_t val;

    ret = yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 42, &val, sizeof(uint64_t), 0);
    if(ret < 0)
        return ret;

    ltime->tm_hour = (val >> 16 & 0xff) + 8;
    ltime->tm_min = val >> 8 & 0xff;
    ltime->tm_sec = val & 0xff;
    ltime->tm_mday = val >> 24 & 0xff;

    ltime->tm_mon = (val >> 32) & 0xff;
    ltime->tm_year = __be16_to_cpu((val >> 40) & 0xffff);

    return 0;
}

int32_t yunsdr_get_xyz(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id, struct xyz_t *xyz)
{
    int32_t ret;
    uint64_t x, y, z;

    ret = yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 43, &x, sizeof(uint64_t), 0);
    if(ret < 0)
        return ret;
    ret = yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 44, &y, sizeof(uint64_t), 0);
    if(ret < 0)
        return ret;
    ret = yunsdr->trans->cmd_send_then_recv(yunsdr->trans, rf_id, 45, &z, sizeof(uint64_t), 0);
    if(ret < 0)
        return ret;

    xyz->latitude = __be64_to_cpu(x) * 180/3.1415926;
    xyz->longitude = __be64_to_cpu(y) * 180/3.1415926;
    xyz->altitude = __be64_to_cpu(z);

    return 0;
}

int32_t yunsdr_read_samples(YUNSDR_DESCRIPTOR *yunsdr,
        void *buffer, uint32_t count, RF_RX_CHANNEL channel, uint64_t *timestamp)
{
    return  yunsdr->trans->stream_recv(yunsdr->trans, buffer, count, channel, timestamp);
}

int32_t yunsdr_read_samples_zerocopy(YUNSDR_DESCRIPTOR *yunsdr,
        void *buffer, uint32_t count, RF_RX_CHANNEL channel, uint64_t *timestamp)
{
    return  yunsdr->trans->stream_recv2(yunsdr->trans, buffer, count, channel, timestamp);
}

int32_t yunsdr_read_samples_multiport(YUNSDR_DESCRIPTOR *yunsdr,
        void **buffer, uint32_t count, uint8_t channel_mask, uint64_t *timestamp)
{
    return  yunsdr->trans->stream_recv3(yunsdr->trans, buffer, count, channel_mask, timestamp);
}

int32_t yunsdr_write_samples(YUNSDR_DESCRIPTOR *yunsdr,
        void *buffer, uint32_t count, RF_TX_CHANNEL channel, uint64_t timestamp)
{
    return  yunsdr->trans->stream_send(yunsdr->trans, buffer, count, channel, timestamp);
}

int32_t yunsdr_write_samples2(YUNSDR_DESCRIPTOR *yunsdr,
        void *buffer, uint32_t count, RF_TX_CHANNEL channel, uint64_t timestamp, uint32_t flags)
{
    return  yunsdr->trans->stream_send2(yunsdr->trans, buffer, count, channel, timestamp, flags);
}

int32_t yunsdr_write_samples_multiport(YUNSDR_DESCRIPTOR *yunsdr,
        const void **buffer, uint32_t count, uint8_t channel_mask, uint64_t timestamp, uint32_t flags)
{
    return  yunsdr->trans->stream_send3(yunsdr->trans, buffer, count, channel_mask, timestamp, flags);
}

int32_t yunsdr_write_samples_zerocopy(YUNSDR_DESCRIPTOR *yunsdr,
        void *buffer, uint32_t count, RF_TX_CHANNEL channel, uint64_t timestamp)
{
    return  yunsdr->trans->stream_send4(yunsdr->trans, buffer, count, channel, timestamp);
}

#ifdef LV_HAVE_SSE
/* Note: src and dst must be 16 byte aligned */
void float_to_int16(int16_t *dst, const float *src, int n, float mult)
{
#ifdef __GNUC__
    const __m128 *p;
    __m128i *q, a0, a1;
    __m128 mult1;

    mult1 = _mm_set1_ps(mult);
    p = (const void *)src;
    q = (void *)dst;

    while (n >= 16) {
        a0 = _mm_cvtps_epi32(p[0] * mult1);
        a1 = _mm_cvtps_epi32(p[1] * mult1);
        q[0] = _mm_packs_epi32(a0, a1);
        a0 = _mm_cvtps_epi32(p[2] * mult1);
        a1 = _mm_cvtps_epi32(p[3] * mult1);
        q[1] = _mm_packs_epi32(a0, a1);
        p += 4;
        q += 2;
        n -= 16;
    }
    if (n >= 8) {
        a0 = _mm_cvtps_epi32(p[0] * mult1);
        a1 = _mm_cvtps_epi32(p[1] * mult1);
        q[0] = _mm_packs_epi32(a0, a1);
        p += 2;
        q += 1;
        n -= 8;
    }
    if (n != 0) {
        /* remaining samples (n <= 7) */
        do {
            a0 = _mm_cvtps_epi32(_mm_load_ss((float *)p) * mult);
            *(int16_t *)q = _mm_cvtsi128_si32(_mm_packs_epi32(a0, a0));
            p = (__m128 *)((float *)p + 1);
            q = (__m128i *)((int16_t *)q + 1);
            n--;
        } while (n != 0);
    }
#endif
}
/* Note: src and dst must be 16 byte aligned */
void int16_to_float(float *dst, const int16_t *src, int len, float mult)
{
#ifdef __GNUC__
    __m128i a0, a1, a, b, sign;
    __m128 mult1;

    mult1 = _mm_set1_ps(mult);
    while (len >= 8) {
        a = *(__m128i *)&src[0];
        a0 = _mm_cvtepi16_epi32(a);
        b = _mm_srli_si128(a, 8);
        a1 = _mm_cvtepi16_epi32(b);
        *(__m128 *)&dst[0] = _mm_cvtepi32_ps(a0) * mult1;
        *(__m128 *)&dst[4] = _mm_cvtepi32_ps(a1) * mult1;
        dst += 8;
        src += 8;
        len -= 8;
    }
    /* remaining data */
    while (len != 0) {
        _mm_store_ss(&dst[0], _mm_cvtsi32_ss(_mm_setzero_ps(), src[0]) * mult1);
        dst++;
        src++;
        len--;
    }
#endif
}
#else
void float_to_int16(int16_t *dst, const float *src, int n, float mult)
{
#if defined(__WINDOWS_) || defined(_WIN32)
#else
#warning "float_to_int16 is not copmiled!"
#endif
}
void int16_to_float(float *dst, const int16_t *src, int len, float mult)
{
#if defined(__WINDOWS_) || defined(_WIN32)
#else
#warning "float_to_int16 is not copmiled!"
#endif
}
#endif
