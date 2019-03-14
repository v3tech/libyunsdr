#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__WINDOWS_) || defined(_WIN32)
#include "wingetopt.h"
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <math.h>
#include <time.h>
#include <yunsdr_api_ss.h>

#if defined(__WINDOWS_) || defined(_WIN32)
#define M_PI 3.141592654L
#endif
#define AM 8192

typedef struct iq
{
    short i;
    short q;
}iq_t;

uint32_t nof_frames = 20; 

int time_adv_samples = 0; 
float tone_offset_hz = 1e6;
float rf_rx_gain = 40, rf_tx_gain = 40, rf_freq = 2.4e9, rf_sampling = 11.52e6; 
uint8_t rf_rx_ch = RX1_CHANNEL, rf_tx_ch = TX1_CHANNEL;
char *rf_args="pcie:0";
char *output_filename = NULL;
char *input_filename = NULL;

void usage(char *prog) {
    printf("Usage: %s -o [rx_signal_file]\n", prog);
    printf("\t-a RF args [Default %s]\n", rf_args);
    printf("\t-f RF TX/RX frequency [Default %.2f MHz]\n", rf_freq/1e6);
    printf("\t-g RF RX gain [Default %.1f dB]\n", rf_rx_gain);
    printf("\t-G RF TX gain [Default %.1f dB]\n", rf_tx_gain);
    printf("\t-s RF TX/RX Sampling frequency [Default %.2f MHz]\n", rf_sampling/1e6);
    printf("\t-c RF RX Channel [Default Channel%u]\n", rf_rx_ch);
    printf("\t-C RF TX Channel [Default Channel%u]\n", rf_tx_ch);
    printf("\t-t Single tone offset (Hz) [Default %f]\n", tone_offset_hz);
    printf("\t-T Time advance samples [Default %d]\n", time_adv_samples);    
    printf("\t-i File name to read signal from [Default single tone]\n");
}

void parse_args(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "i:o:a:f:g:G:t:T:c:C:s:h")) != -1) {
        switch (opt) {
        case 'a':
            rf_args = optarg;
            break;
        case 'o':
            output_filename = optarg;
            break;
        case 'i':
            input_filename = optarg;
            break;
        case 't':
            tone_offset_hz = atof(optarg);
            break;
        case 'T':
            time_adv_samples = atoi(optarg);
            break;
        case 'f':
            rf_freq = atof(optarg);
            break;
        case 'g':
            rf_rx_gain = atof(optarg);
            break;
        case 'G':
            rf_tx_gain = atof(optarg);
            break;
        case 'c':
            rf_rx_ch = atoi(optarg);
            break;
        case 'C':
            rf_tx_ch = atoi(optarg);
            break;
        case 's':
            rf_sampling = atof(optarg);
            break;
        case 'h':
        default:
            usage(argv[0]);
            exit(-1);
        }
    }
    if (!output_filename) {
        usage(argv[0]);
        exit(-1);
    }
    if (time_adv_samples < 0) {
        printf("Time advance must be positive\n");
        usage(argv[0]);
        exit(-1);
    }
}
void save_file(char *filename, const void *buffer, const uint32_t len) {
    FILE *f;
    f = fopen(filename, "w");
    if (f) {
        fwrite(buffer, len, 1, f);
        fclose(f);
    } else {
        perror("fopen");
    }
}

void load_file(char *filename, void *buffer, const uint32_t len) {
    FILE *f;
    f = fopen(filename, "r");
    if (f) {
        fread(buffer, len, 1, f);
        fclose(f);
    } else {
        perror("fopen");
    }
}
void cossingen(short *buf, int count, double tone_freq, double sampling_freq)
{
    int i;
    double value = (2 * M_PI * tone_freq / sampling_freq);
    short *ptr = buf;

    for (i = 0; i < count; i++) {
        *ptr = (short)AM * sin(value * i);
        ptr++;
        *ptr = (short)AM * cos(value * i);
        ptr++;
    }
}

int main(int argc, char **argv) {
    parse_args(argc, argv);

    uint32_t flen = rf_sampling / 1000;

    iq_t *rx_buffer = malloc(sizeof(iq_t)*flen*nof_frames);
    if (!rx_buffer) {
        perror("malloc");
        exit(-1);
    }

    iq_t *tx_buffer = malloc(sizeof(iq_t)*(flen+time_adv_samples));
    if (!tx_buffer) {
        perror("malloc");
        exit(-1);
    }
    memset(tx_buffer, 0, sizeof(iq_t)*(flen+time_adv_samples));

    iq_t *zeros = calloc(sizeof(iq_t), flen);
    if (!zeros) {
        perror("calloc");
        exit(-1);
    }

    //float time_adv_sec = (float) time_adv_samples/rf_sampling;
    float time_adv_sec = yunsdr_ticksToTimeNs(time_adv_samples, rf_sampling) / 1e9;

    // Send through RF 
    YUNSDR_DESCRIPTOR *yunsdr;
    printf("Opening RF device...\n");
    yunsdr = yunsdr_open_device(rf_args);
    if (yunsdr <= 0) {
        fprintf(stderr, "Error opening rf\n");
        exit(-1);
    }

    yunsdr_set_ref_clock (yunsdr, 0, INTERNAL_REFERENCE);
    yunsdr_set_pps_select (yunsdr, 0, PPS_INTERNAL_EN);
    yunsdr_set_vco_select (yunsdr, 0, AUXDAC1);
    yunsdr_set_auxdac1 (yunsdr, 0, 0);
    yunsdr_set_trxsw_fpga_enable(yunsdr, 0, 0);
    yunsdr_set_rx_ant_enable (yunsdr, 0, 1);
    yunsdr_set_duplex_select (yunsdr, 0, FDD);
    yunsdr_set_hwbuf_depth(yunsdr, 0, 327680);

    uint8_t tx_chipid = 0, rx_chipid = 0;
    if(rf_tx_ch == TX3_CHANNEL || rf_tx_ch == TX4_CHANNEL)
        tx_chipid = 1;
    if(rf_rx_ch == RX3_CHANNEL || rf_rx_ch == RX4_CHANNEL)
        rx_chipid = 1;

    yunsdr_set_tx_lo_freq (yunsdr, tx_chipid, rf_freq);
    yunsdr_set_tx_sampling_freq (yunsdr, tx_chipid, rf_sampling);
    yunsdr_set_tx_rf_bandwidth (yunsdr, tx_chipid, rf_sampling);

    (rf_tx_ch == TX1_CHANNEL || rf_tx_ch == TX3_CHANNEL)?
        yunsdr_set_tx1_attenuation (yunsdr, tx_chipid, (90 - rf_tx_gain) * 1000):
        yunsdr_set_tx2_attenuation (yunsdr, tx_chipid, (90 - rf_tx_gain) * 1000);

    yunsdr_set_rx_lo_freq (yunsdr, rx_chipid, rf_freq);
    yunsdr_set_rx_sampling_freq (yunsdr, rx_chipid, rf_sampling);
    yunsdr_set_rx_rf_bandwidth (yunsdr, rx_chipid, rf_sampling);

    (rf_rx_ch == RX1_CHANNEL || rf_rx_ch == RX3_CHANNEL)?
        yunsdr_set_rx1_gain_control_mode (yunsdr, rx_chipid, RF_GAIN_MGC):
        yunsdr_set_rx2_gain_control_mode (yunsdr, rx_chipid, RF_GAIN_MGC);

    (rf_rx_ch == RX1_CHANNEL || rf_rx_ch == RX3_CHANNEL)?
        yunsdr_set_rx1_rf_gain (yunsdr, rx_chipid, rf_rx_gain):
        yunsdr_set_rx2_rf_gain (yunsdr, rx_chipid, rf_rx_gain);

    printf("Subframe len:   %d samples\n", flen);
    printf("Time advance:   %f us\n",time_adv_sec*1e6);
    printf("Set TX/RX rate: %.2f MHz\n", (float) rf_sampling / 1000000);
    printf("Set RX gain:    %.1f dB\n", rf_rx_gain);
    printf("Set TX gain:    %.1f dB\n", rf_tx_gain);
    printf("Set TX/RX freq: %.2f MHz\n",rf_freq / 1000000);

    if (input_filename) {
        load_file(input_filename, &tx_buffer[time_adv_samples], flen*sizeof(iq_t));
    } else {
        cossingen((short *)&tx_buffer[time_adv_samples], flen-time_adv_samples, tone_offset_hz, rf_sampling);
        save_file("rf_txrx_tone", tx_buffer, flen*sizeof(iq_t));
    }

    uint64_t tstamp; 

    yunsdr_enable_timestamp (yunsdr, 0, 1);
#if defined(__WINDOWS_) || defined(_WIN32)
    Sleep(2000);
#else
    sleep(5);
#endif
    uint32_t nframe=0;

    while(nframe<nof_frames) {
        printf("Rx subframe %d\n", nframe);
        tstamp = 0;
        yunsdr_read_samples(yunsdr, &rx_buffer[flen*nframe], flen, rf_rx_ch, &tstamp);
        nframe++;
        if (nframe==9) {
            tstamp = tstamp + yunsdr_timeNsToTicks((4e-3-time_adv_sec)*1e9, rf_sampling);
            yunsdr_write_samples(yunsdr, tx_buffer, flen+time_adv_samples, rf_tx_ch, tstamp);
            printf("Transmitting Signal\n");        
        }
    }
    printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    uint32_t count = 0;
    yunsdr_get_channel_event(yunsdr, RX_CHANNEL_TIMEOUT, rf_rx_ch, &count);
    printf("RX%d Channel timeout: %u\n", rf_rx_ch, count);
    yunsdr_get_channel_event(yunsdr, TX_CHANNEL_TIMEOUT, rf_tx_ch, &count);
    printf("TX%d Channel timeout: %u\n", rf_tx_ch, count);
    yunsdr_get_channel_event(yunsdr, RX_CHANNEL_OVERFLOW, rf_rx_ch, &count);
    printf("RX%d Channel overflow: %u\n", rf_rx_ch, count);
    yunsdr_get_channel_event(yunsdr, TX_CHANNEL_UNDERFLOW, rf_tx_ch, &count);
    printf("TX%d Channel underflow: %u\n", rf_tx_ch, count);
    yunsdr_get_channel_event(yunsdr, RX_CHANNEL_COUNT, rf_rx_ch, &count);
    printf("RX%d Channel count: %u\n", rf_rx_ch, count);
    yunsdr_get_channel_event(yunsdr, TX_CHANNEL_COUNT, rf_tx_ch, &count);
    printf("TX%d Channel count: %u\n", rf_tx_ch, count);
    printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");

    yunsdr_close_device(yunsdr);

    save_file(output_filename, &rx_buffer[12*flen], flen*sizeof(iq_t));

    free(tx_buffer);
    free(rx_buffer);

    printf("Done\n");
    exit(0);
}
