/*
 * yunsdr_api.h
 *
 *  Created on: 2016Äê5ÔÂ9ÈÕ
 *      Author: Eric
 */
#ifndef __YUNSDR_API_H__
#define __YUNSDR_API_H__

#include <stdint.h>

#if defined(__WINDOWS_) || defined(_WIN32)
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yunsdr_meta YUNSDR_META;

typedef struct yunsdr_transport YUNSDR_TRANSPORT;

typedef struct yunsdr_device_descriptor YUNSDR_DESCRIPTOR;

#if defined(__GNUC__)
typedef struct gps_info {
	uint8_t		seconds;
	uint8_t		minutes;
	uint8_t		hours;
	uint8_t		mday;
	uint8_t		month;
	uint16_t	year;
	float    temperature;
	double   latitude;
	double   longitude;
	double   altitude;
	uint8_t  alarm;
}__attribute__ ((packed))GPS;
#elif defined(_MSC_VER)
#pragma pack(1)
typedef struct gps_info {
	uint8_t		seconds;
	uint8_t		minutes;
	uint8_t		hours;
	uint8_t		mday;
	uint8_t		month;
	uint16_t	year;
	float    temperature;
	double   latitude;
	double   longitude;
	double   altitude;
	uint8_t  alarm;
}GPS;
#pragma pack()
#endif

typedef enum {
	START_TX_NORMAL  = 0x1,
	START_TX_LOOP    = 0x2,
	START_TX_BURST   = 0x4,
	START_RX_NORMAL  = 0x8,
	START_RX_BURST   = 0x10,
	START_RX_BULK    = 0x20,
    STOP_TX_LOOP     = 0xFF,
	STOP_RX_NORMAL     = 0xFD,
}RF_MODE;

typedef enum {
	TX1_CHANNEL    = 0x1,
	TX2_CHANNEL    = 0x2,
	TX_DUALCHANNEL = 0x3,
	RX1_CHANNEL    = 0x21,
	RX2_CHANNEL    = 0x22,
	RX_DUALCHANNEL = 0x23,
}RF_CHANNEL;
typedef enum rf_gain_ctrl_mode {
	RF_GAIN_MGC,
	RF_GAIN_FASTATTACK_AGC,
	RF_GAIN_SLOWATTACK_AGC,
}RF_GAIN_CTRL_MODE;

typedef enum pps_enable {
	PPS_INTERNAL_EN,
	PPS_EXTERNAL_EN,
	PPS_ALL_DISABLE,
}PPS_MODE;

typedef enum ref_select {
	INTERNAL_REFERENCE,
	EXTERNAL_REFERENCE,
}REF_SELECT;
typedef enum vco_cal_select {
	ADF4001,
	AUXDAC1,
}VCO_CAL_SELECT;
typedef enum fdd_tdd_sel {
	TDD,
	FDD,
}FDD_TDD_SEL;
typedef enum trx_switch {
	RX,
	TX,
}TRX_SWITCH;

DLLEXPORT uint64_t yunsdr_ticksToTimeNs(const uint64_t ticks, const double rate);

DLLEXPORT uint64_t yunsdr_timeNsToTicks(const uint64_t timeNs, const double rate);

DLLEXPORT YUNSDR_DESCRIPTOR *yunsdr_open_device(const char *ip);

DLLEXPORT int32_t yunsdr_close_device(YUNSDR_DESCRIPTOR *yunsdr);

/* Get current receive RF gain for the selected channel. */
DLLEXPORT int32_t yunsdr_get_rx_rf_gain (YUNSDR_DESCRIPTOR *yunsdr, RF_CHANNEL ch, int32_t *gain_db);

/* Get the RX RF bandwidth. */
DLLEXPORT int32_t yunsdr_get_rx_rf_bandwidth (YUNSDR_DESCRIPTOR *yunsdr, uint32_t *bandwidth_hz);

/* Get current RX sampling frequency. */
DLLEXPORT int32_t yunsdr_get_rx_sampling_freq (YUNSDR_DESCRIPTOR *yunsdr, uint32_t *sampling_freq_hz);

/* Get current RX LO frequency. */
DLLEXPORT int32_t yunsdr_get_rx_lo_freq (YUNSDR_DESCRIPTOR *yunsdr, uint64_t *lo_freq_hz);

/* Get the gain control mode for the selected channel. */
DLLEXPORT int32_t yunsdr_get_rx_gain_control_mode (YUNSDR_DESCRIPTOR *yunsdr, RF_CHANNEL ch, uint8_t *gc_mode);

/* Get current transmit attenuation for the selected channel. */
DLLEXPORT int32_t yunsdr_get_tx_attenuation (YUNSDR_DESCRIPTOR *yunsdr, RF_CHANNEL ch, uint32_t *attenuation_mdb);

/* Get the TX RF bandwidth. */
DLLEXPORT int32_t yunsdr_get_tx_rf_bandwidth (YUNSDR_DESCRIPTOR *yunsdr, uint32_t *bandwidth_hz);

/* Get current TX sampling frequency. */
DLLEXPORT int32_t yunsdr_get_tx_sampling_freq (YUNSDR_DESCRIPTOR *yunsdr, uint32_t *sampling_freq_hz);

/* Get current TX LO frequency. */
DLLEXPORT int32_t yunsdr_get_tx_lo_freq (YUNSDR_DESCRIPTOR *yunsdr, uint64_t *lo_freq_hz);

/* Gets the RX FIR state. */
DLLEXPORT int32_t yunsdr_get_rx_fir_en_dis(YUNSDR_DESCRIPTOR *yunsdr, uint8_t *en_dis);

/* Gets the TX FIR state. */
DLLEXPORT int32_t yunsdr_get_tx_fir_en_dis(YUNSDR_DESCRIPTOR *yunsdr, uint8_t *en_dis);

/* Sets the RX FIR state. */
DLLEXPORT int32_t yunsdr_set_rx_fir_en_dis(YUNSDR_DESCRIPTOR *yunsdr, uint8_t en_dis);

/* Sets the TX FIR state. */
DLLEXPORT int32_t yunsdr_set_tx_fir_en_dis(YUNSDR_DESCRIPTOR *yunsdr, uint8_t en_dis);

/* Load TX/RX FIR configure data from file. */
DLLEXPORT int32_t yunsdr_set_trx_fir_config(YUNSDR_DESCRIPTOR *yunsdr, char *fir_config);

/* Sets the TRX FIR state. */
DLLEXPORT int32_t yunsdr_set_trx_fir_en_dis(YUNSDR_DESCRIPTOR *yunsdr, uint8_t en_dis);

DLLEXPORT int32_t yunsdr_get_version(YUNSDR_DESCRIPTOR *yunsdr, uint64_t *version);

/* Dump current value of yunsdr's register. */
DLLEXPORT int32_t yunsdr_dump_register (YUNSDR_DESCRIPTOR *yunsdr,
		uint16_t regid, uint32_t *reg_val);

/* Set the receive RF gain for the selected channel. */
DLLEXPORT int32_t yunsdr_set_rx_rf_gain (YUNSDR_DESCRIPTOR *yunsdr,
		RF_CHANNEL ch, int32_t gain_db);

/* Set the RX RF bandwidth. */
DLLEXPORT int32_t yunsdr_set_rx_rf_bandwidth (YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t bandwidth_hz);

/* Set the RX sampling frequency. */
DLLEXPORT int32_t yunsdr_set_rx_sampling_freq (YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t sampling_freq_hz);

/* Set the RX LO frequency. */
DLLEXPORT int32_t yunsdr_set_rx_lo_freq (YUNSDR_DESCRIPTOR *yunsdr,
		uint64_t lo_freq_hz);

/* Set the gain control mode for the selected channel. */
DLLEXPORT int32_t yunsdr_set_rx_gain_control_mode (YUNSDR_DESCRIPTOR *yunsdr,
		RF_CHANNEL ch, RF_GAIN_CTRL_MODE gc_mode);

/* Set the transmit attenuation for the selected channel. */
DLLEXPORT int32_t yunsdr_set_tx_attenuation (YUNSDR_DESCRIPTOR *yunsdr,
		RF_CHANNEL ch, uint32_t attenuation_mdb);

/* Set the TX RF bandwidth. */
DLLEXPORT int32_t yunsdr_set_tx_rf_bandwidth (YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t  bandwidth_hz);

/* Set the TX sampling frequency. */
DLLEXPORT int32_t yunsdr_set_tx_sampling_freq (YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t sampling_freq_hz);

/* Set the TX LO frequency. */
DLLEXPORT int32_t yunsdr_set_tx_lo_freq (YUNSDR_DESCRIPTOR *yunsdr,
		uint64_t lo_freq_hz);

DLLEXPORT int32_t yunsdr_set_ref_clock (YUNSDR_DESCRIPTOR *yunsdr,
		REF_SELECT select);

DLLEXPORT int32_t yunsdr_set_vco_select (YUNSDR_DESCRIPTOR *yunsdr,
		VCO_CAL_SELECT select);

DLLEXPORT int32_t yunsdr_set_trx_select (YUNSDR_DESCRIPTOR *yunsdr,
		TRX_SWITCH select);

DLLEXPORT int32_t yunsdr_set_duplex_select (YUNSDR_DESCRIPTOR *yunsdr,
		FDD_TDD_SEL select);

DLLEXPORT int32_t yunsdr_set_trx_channel (YUNSDR_DESCRIPTOR *yunsdr, uint8_t ch);

DLLEXPORT int32_t yunsdr_set_auxdac1 (YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t val);

DLLEXPORT int32_t yunsdr_set_adf4001 (YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t val);

DLLEXPORT int32_t yunsdr_enable_timestamp (YUNSDR_DESCRIPTOR *yunsdr, PPS_MODE pps);

DLLEXPORT int32_t yunsdr_disable_timestamp (YUNSDR_DESCRIPTOR *yunsdr);

DLLEXPORT int32_t yunsdr_read_timestamp(YUNSDR_DESCRIPTOR *yunsdr,
		uint64_t *timestamp);

DLLEXPORT int32_t yunsdr_read_gps (YUNSDR_DESCRIPTOR *yunsdr,
		GPS *pgps);

DLLEXPORT int32_t yunsdr_calibrate_qec (YUNSDR_DESCRIPTOR *yunsdr, uint8_t val);

DLLEXPORT int32_t yunsdr_set_tone_bist (YUNSDR_DESCRIPTOR *yunsdr, uint8_t enable);

DLLEXPORT int32_t yunsdr_enable_tx(YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t nsamples,
		RF_MODE mode);

DLLEXPORT int32_t yunsdr_enable_rx(YUNSDR_DESCRIPTOR *yunsdr,
		uint32_t nsamples,
		RF_CHANNEL rf_port,
		RF_MODE mode,
		uint64_t timestamp);

DLLEXPORT int32_t yunsdr_disable_tx(YUNSDR_DESCRIPTOR *yunsdr);

DLLEXPORT int32_t yunsdr_disable_rx(YUNSDR_DESCRIPTOR *yunsdr);

DLLEXPORT int32_t yunsdr_read_samples(YUNSDR_DESCRIPTOR *yunsdr,
		void **buffer,
		uint32_t nbyte,
		uint64_t *timestamp,
		double timeout);

DLLEXPORT int32_t yunsdr_write_samples(YUNSDR_DESCRIPTOR *yunsdr,
		void **buffer,
		uint32_t nbyte,
		RF_CHANNEL  rf_port,
		uint64_t timestamp);

DLLEXPORT int32_t yunsdr_read_samples2(YUNSDR_DESCRIPTOR *yunsdr,
		void *buffer,
		uint32_t nbyte,
		uint64_t *timestamp,
		double timeout);

DLLEXPORT int32_t yunsdr_write_samples2(YUNSDR_DESCRIPTOR *yunsdr,
		void *buffer,
		uint32_t nbyte,
		RF_CHANNEL  rf_port,
		uint64_t timestamp);

#define __DEBUG__
#ifdef __DEBUG__
#include <stdarg.h>

enum {
	DEBUG_ERR,
	DEBUG_WARN,
	DEBUG_INFO,
};

#define DEBUG_OUTPUT_LEVEL DEBUG_INFO
DLLEXPORT void debug(int level, const char *fmt, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif /*  __YUNSDR_API_H__ */
