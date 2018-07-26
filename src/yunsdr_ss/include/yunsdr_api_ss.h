/*
 * yunsdr_api.h
 *
 *  Created on: 2016/5/9
 *      Author: Eric
 */
#ifndef __YUNSDR_API_H__
#define __YUNSDR_API_H__
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <riffa.h>

#if defined(__WINDOWS_) || defined(_WIN32)
#define DLLEXPORT __declspec(dllexport)
#define YUNSDRCALL __cdecl
#else
#define DLLEXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yunsdr_transport YUNSDR_TRANSPORT;

typedef struct yunsdr_device_descriptor {
    int32_t id;
    int32_t status;
	YUNSDR_TRANSPORT *trans;
}YUNSDR_DESCRIPTOR;

typedef enum {
    TX1_CHANNEL = 1,
    TX2_CHANNEL = 2,
    TX3_CHANNEL = 3,
    TX4_CHANNEL = 4,
}RF_TX_CHANNEL;

typedef enum {
    RX1_CHANNEL = 1,
    RX2_CHANNEL = 2,
    RX3_CHANNEL = 3,
    RX4_CHANNEL = 4,
}RF_RX_CHANNEL;

typedef enum rf_gain_ctrl_mode {
    RF_GAIN_MGC,
    RF_GAIN_FASTATTACK_AGC,
    RF_GAIN_SLOWATTACK_AGC,
}RF_GAIN_CTRL_MODE;

typedef enum ref_select {
    INTERNAL_REFERENCE = 0,
    EXTERNAL_REFERENCE,
}REF_SELECT;

typedef enum vco_cal_select {
    ADF4001 = 0,
    AUXDAC1,
}VCO_CAL_SELECT;

typedef enum duplex_select {
    TDD = 0,
    FDD,
}DUPLEX_SELECT;

typedef enum trx_switch {
    RX = 0,
    TX,
}TRX_SWITCH;

/***************************************************************************/
DLLEXPORT uint64_t yunsdr_ticksToTimeNs(const uint64_t ticks, const double rate);
DLLEXPORT uint64_t yunsdr_timeNsToTicks(const uint64_t timeNs, const double rate);

/***************************************************************************/
DLLEXPORT YUNSDR_DESCRIPTOR *yunsdr_open_device(char *url);
DLLEXPORT int32_t yunsdr_close_device(YUNSDR_DESCRIPTOR *yunsdr);


/***************************************************************************/
/* Get current TX LO frequency. */
DLLEXPORT int32_t yunsdr_get_tx_lo_freq (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint64_t *lo_freq_hz);
/* Get current TX sampling frequency. */
DLLEXPORT int32_t yunsdr_get_tx_sampling_freq (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t *sampling_freq_hz);
/* Get the TX RF bandwidth. */
DLLEXPORT int32_t yunsdr_get_tx_rf_bandwidth (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t *bandwidth_hz);
/* Get current transmit attenuation for the selected channel. */
DLLEXPORT int32_t yunsdr_get_tx1_attenuation (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t *attenuation_mdb);        
/* Get current transmit attenuation for the selected channel. */
DLLEXPORT int32_t yunsdr_get_tx2_attenuation (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t *attenuation_mdb);        
        
/***************************************************************************/        
/* Get current RX LO frequency. */
DLLEXPORT int32_t yunsdr_get_rx_lo_freq (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint64_t *lo_freq_hz);
/* Get the RX RF bandwidth. */
DLLEXPORT int32_t yunsdr_get_rx_rf_bandwidth (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t *bandwidth_hz);
/* Get the gain control mode for the selected channel. */
DLLEXPORT int32_t yunsdr_get_rx1_gain_control_mode (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        RF_GAIN_CTRL_MODE *gc_mode);
/* Get the gain control mode for the selected channel. */
DLLEXPORT int32_t yunsdr_get_rx2_gain_control_mode (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        RF_GAIN_CTRL_MODE *gc_mode);
/* Get current receive RF gain for the selected channel. */
DLLEXPORT int32_t yunsdr_get_rx1_rf_gain (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        int32_t *gain_db);
/* Get current receive RF gain for the selected channel. */
DLLEXPORT int32_t yunsdr_get_rx2_rf_gain (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        int32_t *gain_db);

/***************************************************************************/        
/* Set the RX LO frequency. */
DLLEXPORT int32_t yunsdr_set_rx_lo_freq (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint64_t lo_freq_hz);        
/* Set the RX RF bandwidth. */
DLLEXPORT int32_t yunsdr_set_rx_rf_bandwidth (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t  bandwidth_hz);       
/* Set the RX sampling frequency. */
DLLEXPORT int32_t yunsdr_set_rx_sampling_freq (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t sampling_freq_hz);
/* Set the gain control mode for the selected channel. */
DLLEXPORT int32_t yunsdr_set_rx1_gain_control_mode (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        RF_GAIN_CTRL_MODE gc_mode);
DLLEXPORT int32_t yunsdr_set_rx2_gain_control_mode (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        RF_GAIN_CTRL_MODE gc_mode);        
/* Set the receive RF gain for the selected channel. */
DLLEXPORT int32_t yunsdr_set_rx1_rf_gain (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        int32_t gain_db);
/* Set the receive RF gain for the selected channel. */
DLLEXPORT int32_t yunsdr_set_rx2_rf_gain (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        int32_t gain_db);
        
DLLEXPORT int32_t yunsdr_set_rx_fir_en_dis (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t enable);

/***************************************************************************/
/* Set the TX LO frequency. */
DLLEXPORT int32_t yunsdr_set_tx_lo_freq (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint64_t lo_freq_hz); 
/* Set the TX RF bandwidth. */
DLLEXPORT int32_t yunsdr_set_tx_rf_bandwidth (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t  bandwidth_hz);
/* Set the TX sampling frequency. */
DLLEXPORT int32_t yunsdr_set_tx_sampling_freq (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t sampling_freq_hz);
/* Set the transmit attenuation for the selected channel. */
DLLEXPORT int32_t yunsdr_set_tx1_attenuation (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t attenuation_mdb);
/* Set the transmit attenuation for the selected channel. */
DLLEXPORT int32_t yunsdr_set_tx2_attenuation (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t attenuation_mdb);
DLLEXPORT int32_t yunsdr_set_tx_fir_en_dis (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t status);

/***************************************************************************/
DLLEXPORT int32_t yunsdr_get_ad9361_reg (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id, uint32_t reg,
        uint32_t *value);
        
DLLEXPORT int32_t yunsdr_set_ad9361_reg(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id, uint32_t reg,
        uint32_t value);

DLLEXPORT int32_t yunsdr_get_status (YUNSDR_DESCRIPTOR *yunsdr, uint8_t status_id, uint8_t channel, 
        uint32_t *value);
        
/***************************************************************************/        
DLLEXPORT int32_t yunsdr_set_tx_lo_int_ext (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t enable);
DLLEXPORT int32_t yunsdr_set_rx_lo_int_ext (YUNSDR_DESCRIPTOR *yunsdr,uint8_t rf_id,
        uint8_t enable);
DLLEXPORT int32_t yunsdr_set_ext_lo_freq(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint64_t lo_freq_hz);
DLLEXPORT int32_t yunsdr_do_mcs(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t enable);
DLLEXPORT int32_t yunsdr_set_rx_ant_enable(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t enable);     
DLLEXPORT int32_t yunsdr_set_ref_clock(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        REF_SELECT source);
DLLEXPORT int32_t yunsdr_set_vco_select (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        VCO_CAL_SELECT vco);
DLLEXPORT int32_t yunsdr_set_auxdac1 (YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint32_t vol_mV);
DLLEXPORT int32_t yunsdr_set_duplex_select(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        DUPLEX_SELECT duplex);
DLLEXPORT int32_t yunsdr_tx_cyclic_enable(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t enable);
DLLEXPORT int32_t yunsdr_set_trxsw_fpga_enable(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t enable);
DLLEXPORT int32_t yunsdr_get_firmware_version(YUNSDR_DESCRIPTOR *yunsdr, uint32_t *version);
DLLEXPORT int32_t yunsdr_get_model_version(YUNSDR_DESCRIPTOR *yunsdr, uint32_t *version);

/***************************************************************************/
DLLEXPORT int32_t yunsdr_enable_timestamp(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id,
        uint8_t enbale);
DLLEXPORT int32_t yunsdr_read_timestamp(YUNSDR_DESCRIPTOR *yunsdr, uint8_t rf_id, uint64_t *timestamp);


/***************************************************************************/
DLLEXPORT int32_t yunsdr_read_samples(YUNSDR_DESCRIPTOR *yunsdr,
        void *buffer, uint32_t count, RF_RX_CHANNEL channel, uint64_t *timestamp);
        
DLLEXPORT int32_t yunsdr_write_samples(YUNSDR_DESCRIPTOR *yunsdr,
        void *buffer, uint32_t count, RF_TX_CHANNEL channel, uint64_t timestamp);

DLLEXPORT int32_t yunsdr_write_samples2(YUNSDR_DESCRIPTOR *yunsdr,
        void *buffer, uint32_t count, RF_TX_CHANNEL channel, uint64_t timestamp, uint32_t);
/***************************************************************************/
DLLEXPORT void float_to_int16(int16_t *dst, const float *src, int n, float mult);
DLLEXPORT void int16_to_float(float *dst, const int16_t *src, int len, float mult);

/***************************************************************************/


#ifdef __cplusplus
}
#endif

#endif /*  __YUNSDR_API_H__ */
