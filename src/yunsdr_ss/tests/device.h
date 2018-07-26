#ifndef __YUNSDR_DEVICE_H__
#define __YUNSDR_DEVICE_H__

#include <stdint.h>
#include "yunsdr_api_ss.h"

#define CENTER_FREQUENCY 2800000000ULL
#define SAMPLEING_FREQ   40000000UL
#define BANDWIDTH_FREQ   40000000UL
#define RX_GAIN			 10
#define TX_ATTEN		 10000
#define AUXDAC1_MV       1350



typedef enum yunsdr_rf_ops{
	SET_TX_LO_FREQ,
	SET_TX_SAMPLING_FREQ,
	SET_TX_RF_BANDWIDTH,
	SET_TX1_ATTENUATION,
	SET_TX2_ATTENUATION,
	SET_RX_LO_FREQ,
	SET_RX_SAMPLING_FREQ,
	SET_RX_RF_BANDWIDTH,
	SET_RX1_GAIN_CONTROL_MODE,
	SET_RX2_GAIN_CONTROL_MODE,
	SET_RX1_RF_GAIN,
	SET_RX2_RF_GAIN,

	SET_REF_CLOCK,
	SET_VCO_SELECT,
	SET_TRX_SELECT,
	SET_DUPLEX_SELECT,
	SET_ADF4001,
    SET_AUXDAC1,
}yunsdr_rf_ops;


extern int yunsdr_config_handler(YUNSDR_DESCRIPTOR *yunsdr,
		yunsdr_rf_ops ops, uint64_t param);

extern int yunsdr_rf_tx_init(YUNSDR_DESCRIPTOR *yunsdr);

extern int yunsdr_rf_rx_init(YUNSDR_DESCRIPTOR *yunsdr);

extern int yunsdr_rf_other_init(YUNSDR_DESCRIPTOR *yunsdr);

#endif /* __YUNSDR_DEVICE_H__ */
