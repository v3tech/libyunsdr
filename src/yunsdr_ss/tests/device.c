#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "device.h"
#include "yunsdr_api_ss.h"

int yunsdr_config_handler(YUNSDR_DESCRIPTOR *yunsdr,
		yunsdr_rf_ops ops, uint64_t param)
{
	switch (ops) {
	case SET_TX_LO_FREQ:
		yunsdr_set_tx_lo_freq (yunsdr, 0, param);
		break;
	case SET_TX_SAMPLING_FREQ:
		yunsdr_set_tx_sampling_freq (yunsdr, 0, param);
		break;
	case SET_TX_RF_BANDWIDTH:
		yunsdr_set_tx_rf_bandwidth (yunsdr, 0, param);
		break;
	case SET_TX1_ATTENUATION:
		yunsdr_set_tx1_attenuation (yunsdr, 0, param);
		break;
	case SET_TX2_ATTENUATION:
		yunsdr_set_tx2_attenuation (yunsdr, 0, param);
		break;
	case SET_RX_LO_FREQ:
		yunsdr_set_rx_lo_freq (yunsdr, 0, param);
		break;
	case SET_RX_SAMPLING_FREQ:
		yunsdr_set_rx_sampling_freq (yunsdr, 0, param);
		break;
	case SET_RX_RF_BANDWIDTH:
		yunsdr_set_rx_rf_bandwidth (yunsdr, 0, param);
		break;
	case SET_RX1_GAIN_CONTROL_MODE:
		yunsdr_set_rx1_gain_control_mode (yunsdr, 0, param);
		break;
	case SET_RX2_GAIN_CONTROL_MODE:
		yunsdr_set_rx2_gain_control_mode (yunsdr, 0, param);
		break;
	case SET_RX1_RF_GAIN:
		yunsdr_set_rx1_rf_gain (yunsdr, 0, param);
		break;
	case SET_RX2_RF_GAIN:
		yunsdr_set_rx2_rf_gain (yunsdr, 0, param);
		break;
	case SET_REF_CLOCK:
		yunsdr_set_ref_clock (yunsdr, 0, param);//EXTERNAL_REFERENCE
		break;
	case SET_VCO_SELECT:
		yunsdr_set_vco_select (yunsdr, 0, param);
		break;
	case SET_TRX_SELECT:
		yunsdr_set_rx_ant_enable (yunsdr, 0, param);
		break;
	case SET_DUPLEX_SELECT:
		yunsdr_set_duplex_select (yunsdr, 0, param);
		break;
	case SET_ADF4001:
#if 0
		yunsdr_set_adf4001 (yunsdr, 0, param);
#endif
    case SET_AUXDAC1:
		yunsdr_set_auxdac1 (yunsdr, 0, param);
		break;
	default:
		break;
	}

	return 0;
}

int yunsdr_rf_tx_init(YUNSDR_DESCRIPTOR *yunsdr)
{
	int64_t val;


	yunsdr_config_handler(yunsdr, SET_TX_LO_FREQ, CENTER_FREQUENCY);

	yunsdr_config_handler(yunsdr, SET_TX_SAMPLING_FREQ, SAMPLEING_FREQ);

	yunsdr_config_handler(yunsdr, SET_TX_RF_BANDWIDTH, BANDWIDTH_FREQ);

	yunsdr_config_handler(yunsdr, SET_TX1_ATTENUATION, TX_ATTEN);

	yunsdr_config_handler(yunsdr, SET_TX2_ATTENUATION, TX_ATTEN);

	return 0;
}

int yunsdr_rf_rx_init(YUNSDR_DESCRIPTOR *yunsdr)
{
	int64_t val;

	yunsdr_config_handler(yunsdr, SET_RX_LO_FREQ, CENTER_FREQUENCY);

	yunsdr_config_handler(yunsdr, SET_RX_SAMPLING_FREQ, SAMPLEING_FREQ);

	yunsdr_config_handler(yunsdr, SET_RX_RF_BANDWIDTH, BANDWIDTH_FREQ);

	yunsdr_config_handler(yunsdr, SET_RX1_GAIN_CONTROL_MODE, RF_GAIN_MGC);

	yunsdr_config_handler(yunsdr, SET_RX2_GAIN_CONTROL_MODE, RF_GAIN_MGC);

	yunsdr_config_handler(yunsdr, SET_RX1_RF_GAIN, RX_GAIN);

	yunsdr_config_handler(yunsdr, SET_RX2_RF_GAIN, RX_GAIN);

	return 0;
}
int yunsdr_rf_other_init(YUNSDR_DESCRIPTOR *yunsdr)
{
	int64_t val;

	yunsdr_config_handler(yunsdr, SET_REF_CLOCK, INTERNAL_REFERENCE);

	yunsdr_config_handler(yunsdr, SET_VCO_SELECT, AUXDAC1);
    
    yunsdr_config_handler(yunsdr, SET_AUXDAC1, AUXDAC1_MV);
    
	yunsdr_config_handler(yunsdr, SET_TRX_SELECT, TX);

	yunsdr_config_handler(yunsdr, SET_DUPLEX_SELECT, FDD);
#if 0
	int16_t rcount = 10;
	int16_t ncount = 26;

	yunsdr_config_handler(yunsdr, SET_ADF4001, ncount<<16|rcount);

	yunsdr_set_tone_bist (yunsdr, 1);
#endif
	return 0;
}


