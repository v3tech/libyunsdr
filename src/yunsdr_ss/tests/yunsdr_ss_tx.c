
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#if defined(__WINDOWS_) || defined(_WIN32)
#include "wingetopt.h"
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif
#include <math.h>
#include <time.h>
#include <yunsdr_api_ss.h>

#if defined(__WINDOWS_) || defined(_WIN32)
#define M_PI 3.141592654L
#endif
#define AM 8192
#define NBYTE_OF_METAHEADER 16
#define MAX_TX_WORKER  4
#define TEST_CYCLE     100000L
#define TEST_NSAMPLES  (10*1024L)

static uint32_t auxdac1 = 1200; 
static uint32_t flen = 1*1024*1024;
static float tone_offset_hz = 1e6;
static float rf_tx_gain = 40, rf_freq = 2.4e9, rf_sampling = 30.72e6; 
static uint8_t rf_tx_ch = TX1_CHANNEL;
static uint8_t tx_ch_mask = 1, num_of_rfport = 0;
static char *rf_args="pcie:0";
static char *input_filename = NULL;
static uint32_t max_tx_cycle = TEST_CYCLE;
static uint32_t default_tone_nsamples = TEST_NSAMPLES;

static YUNSDR_DESCRIPTOR *yunsdr;

typedef struct {
    uint8_t channel;
    uint32_t nsamples;
    short *buffer;
    uint8_t buf_index;
    uint64_t tx_timestamp;
}TX_WORKER;

#if defined(__WINDOWS_) || defined(_WIN32)
DWORD WINAPI rf_tx_worker(LPVOID arg);
#else
void *rf_tx_worker(void *arg);
#endif

void usage(char *prog)
{
    printf("Usage: %s -o [rx_signal_file]\n", prog);
    printf("\t-a RF Args [Default %s]\n", rf_args);
    printf("\t-f RF TX Frequency [Default %.2f MHz]\n", rf_freq/1e6);
    printf("\t-G RF TX Gain [Default %.1f dB]\n", rf_tx_gain);
    printf("\t-s RF TX Sampling frequency [Default %.2f MHz]\n", rf_sampling/1e6);
    printf("\t-C RF TX Channel[1,2,3,4] [Default Channel%u]\n", rf_tx_ch);
    printf("\t-t Single tone offset (Hz) [Default %f]\n", tone_offset_hz);
	printf("\t-T Test Cycle [Default %lu]\n", TEST_CYCLE);
	printf("\t-N Test Number of tone samples [Default %lu]\n", TEST_NSAMPLES);
    printf("\t-i File name to read signal from [Default single tone]\n");
    printf("\t-d RF auxdac1 value [Default %u mV]\n", auxdac1);
}

void parse_args(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "i:a:f:G:t:C:s:T:N:d:h")) != -1) {
        switch (opt) {
        case 'a':
            rf_args = optarg;
            break;
        case 'i':
            input_filename = optarg;
            break;
        case 't':
            tone_offset_hz = atof(optarg);
            break;
        case 'f':
            rf_freq = atof(optarg);
            break;
        case 'G':
            rf_tx_gain = atof(optarg);
            break;
		case 'T':
			max_tx_cycle = atof(optarg);
            break;
		case 'N':
			default_tone_nsamples = atof(optarg);
            break;
        case 'C':
            {
                char *ch;
                char *ptr = optarg;
                while ((ch = strtok(ptr, ",")) != NULL) {
                    ptr = NULL;
                    uint8_t index = atoi(ch);
                    if(index > 0 && index < 5) {
                        tx_ch_mask = tx_ch_mask | (1 << (index - 1));
                        num_of_rfport++;
                    }
                }
            }

            break;
        case 's':
            rf_sampling = atof(optarg);
            break;
        case 'd':
            auxdac1 = atoi(optarg);
            break;
        case 'h':
        default:
            usage(argv[0]);
            exit(-1);
        }
    }
   
    if(!num_of_rfport)
        num_of_rfport = 1;
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

void load_file(char *filename, void *buffer, const uint32_t len)
{
    FILE *f;
    f = fopen(filename, "rb");
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

int main(int argc, char **argv)
{
    parse_args(argc, argv);

    // Send through RF 
    printf("Opening RF device...\n");
    yunsdr = yunsdr_open_device(rf_args);
    if (yunsdr <= 0) {
        fprintf(stderr, "Error opening rf\n");
        exit(-1);
    }
#if 1
    yunsdr_set_ref_clock (yunsdr, 0, INTERNAL_REFERENCE);
    yunsdr_set_pps_select (yunsdr, 0, PPS_INTERNAL_EN);
    yunsdr_set_vco_select (yunsdr, 0, AUXDAC1);
    yunsdr_set_auxdac1 (yunsdr, 0, auxdac1);
    yunsdr_set_rx_ant_enable (yunsdr, 0, 1);
    yunsdr_set_duplex_select (yunsdr, 0, FDD);
    yunsdr_set_trxsw_fpga_enable (yunsdr, 0, 0);
    yunsdr_set_hwbuf_depth(yunsdr, 0, 3276800);

    if(tx_ch_mask & 1 || tx_ch_mask >> 1 & 1) {
        yunsdr_set_tx_lo_freq (yunsdr, 0, rf_freq);
        yunsdr_set_tx_sampling_freq (yunsdr, 0, rf_sampling);
        yunsdr_set_tx_rf_bandwidth (yunsdr, 0, rf_sampling);
        if(tx_ch_mask & 1)
            yunsdr_set_tx1_attenuation (yunsdr, 0, (90 - rf_tx_gain) * 1000);
        if(tx_ch_mask >> 1 & 1)
            yunsdr_set_tx1_attenuation (yunsdr, 0, (90 - rf_tx_gain) * 1000);
    }
    if(tx_ch_mask >> 2 & 1 || tx_ch_mask >> 3 & 1) {
        yunsdr_set_tx_lo_freq (yunsdr, 1, rf_freq);
        yunsdr_set_tx_sampling_freq (yunsdr, 1, rf_sampling);
        yunsdr_set_tx_rf_bandwidth (yunsdr, 1, rf_sampling);
        if(tx_ch_mask >> 2 & 1)
            yunsdr_set_tx1_attenuation (yunsdr, 1, (90 - rf_tx_gain) * 1000);
        if(tx_ch_mask >> 3 & 1)
            yunsdr_set_tx1_attenuation (yunsdr, 1, (90 - rf_tx_gain) * 1000);
    }
#endif
    yunsdr_tx_cyclic_enable(yunsdr, 0, 0);
    yunsdr_enable_timestamp (yunsdr, 0, 0);
    yunsdr_enable_timestamp (yunsdr, 0, 1);
#if defined(__WINDOWS_) || defined(_WIN32)
    Sleep(2000);
#else
    sleep(2);
#endif

    short *data_buf = NULL;
    if (input_filename) {
        unsigned long filesize = -1;  
        FILE *fp;  
        fp = fopen(input_filename, "r");  
        if(fp == NULL) { 
            perror("fopen");
            exit(-1);
        }
        fseek(fp, 0L, SEEK_END);  
        filesize = ftell(fp);  
        fclose(fp);  
        data_buf = malloc(filesize);
        if (!data_buf) {
            perror("malloc");
            exit(-1);
        }
        memset(data_buf, 0, filesize);
        load_file(input_filename, data_buf, filesize);
        flen = filesize;
    } else {
        data_buf = malloc(default_tone_nsamples * 4);
        if (!data_buf) {
            perror("malloc");
            exit(-1);
        }
        cossingen((short *)data_buf, default_tone_nsamples, tone_offset_hz, rf_sampling);
        flen = default_tone_nsamples * 4;
    }

    printf("Tx Signal len: %u samples\n", flen / 4);
    printf("Set Tx rate:   %.2f MHz\n", (float) rf_sampling / 1000000);
    printf("Set Tx gain:   %.1f dB\n", rf_tx_gain);
    printf("Set Tx freq:   %.2f MHz\n", rf_freq / 1000000);
    printf("Set AUXDAC1:   %u mV\n", auxdac1);

    
    short **tx_buffer_meta = malloc(sizeof(short *) * MAX_TX_WORKER);
    for(int i = 0; i < MAX_TX_WORKER; i++) {
        tx_buffer_meta[i] = malloc(flen + NBYTE_OF_METAHEADER);
        if (!tx_buffer_meta[i]) {
            perror("malloc");
            exit(-1);
        }
        memcpy(tx_buffer_meta[i], data_buf, flen);
    }

    printf("Transmitting Signal\n");        
    uint64_t tx_timestamp = 0;
    if(yunsdr_read_timestamp (yunsdr, 0, &tx_timestamp) < 0)
        printf("Cannot read timestamp\n");
    printf("current_timestamp:%" PRId64 "\n", tx_timestamp);
    tx_timestamp = tx_timestamp + yunsdr_timeNsToTicks(2e9, rf_sampling);
    printf("tx_timestamp[+1s]:%" PRId64 "\n", tx_timestamp);

    TX_WORKER worker[MAX_TX_WORKER];
#if defined(__WINDOWS_) || defined(_WIN32)
    DWORD tidTX[MAX_TX_WORKER];
    HANDLE worker_id[MAX_TX_WORKER];
#else
    pthread_t worker_id[MAX_TX_WORKER];
#endif

    uint8_t buf_index = 0;;

    for(int i = 0; i < MAX_TX_WORKER; i++) {
        if(tx_ch_mask >> i & 1) {
            worker[i].channel = i + 1;
            worker[i].nsamples = flen / 4;
            worker[i].buffer = tx_buffer_meta[buf_index];
            worker[i].buf_index = buf_index;
            worker[i].tx_timestamp = tx_timestamp;
            buf_index++;

#if defined(__WINDOWS_) || defined(_WIN32)
            worker_id[i] = CreateThread(NULL, 0, rf_tx_worker, (LPVOID)&worker[i], 0, &tidTX[i]);
#else
            if(pthread_create(&worker_id[i], NULL, rf_tx_worker, &worker[i])) {
                printf("Cannot create worker thread!\n");
                exit(-1);
            }
#endif
        }
    }
    for(int i = 0; i < MAX_TX_WORKER; i++) {
        if(tx_ch_mask >> i & 1) {
#if defined(__WINDOWS_) || defined(_WIN32)
        WaitForSingleObject(worker_id[i], INFINITE);
        CloseHandle(worker_id[i]);
#else
        pthread_join(worker_id[i], NULL);
#endif
        }
    }

    for(int i = 0; i < MAX_TX_WORKER; i++) {
        if(tx_ch_mask >> i & 1) {
            printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
            uint32_t count = 0;
            yunsdr_get_channel_event(yunsdr, RX_CHANNEL_TIMEOUT, i + 1, &count);
            printf("RX%d Channel timeout: %u\n", i + 1, count);
            yunsdr_get_channel_event(yunsdr, TX_CHANNEL_TIMEOUT, i + 1, &count);
            printf("TX%d Channel timeout: %u\n", i + 1, count);
            yunsdr_get_channel_event(yunsdr, RX_CHANNEL_OVERFLOW, i + 1, &count);
            printf("RX%d Channel overflow: %u\n", i + 1, count);
            yunsdr_get_channel_event(yunsdr, TX_CHANNEL_UNDERFLOW, i + 1, &count);
            printf("TX%d Channel underflow: %u\n", i + 1, count);
            yunsdr_get_channel_event(yunsdr, RX_CHANNEL_COUNT, i + 1, &count);
            printf("RX%d Channel count: %u\n", i + 1, count);
            yunsdr_get_channel_event(yunsdr, TX_CHANNEL_COUNT, i + 1, &count);
            printf("TX%d Channel count: %u\n", i + 1, count);
            printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
        }
    }

    yunsdr_close_device(yunsdr);

    for(int i = 0; i < MAX_TX_WORKER; i++)
        free(tx_buffer_meta[i]);

    free(data_buf);
    free(tx_buffer_meta);

    printf("Done\n");
    exit(0);
}

#if defined(__WINDOWS_) || defined(_WIN32)
DWORD WINAPI rf_tx_worker(LPVOID arg)
#else
void *rf_tx_worker(void *arg)
#endif
{
    TX_WORKER *worker = (TX_WORKER *)arg;
    uint64_t timestamp = 0;
    int32_t i;

    printf(">>>>>>>>>>>>>>>>>>>Test for RF%u start\n", worker->channel);
    for(i = 0; i < max_tx_cycle; i++) {
        if(i == 0) {
            timestamp = worker->tx_timestamp;
        } else
            timestamp = 0;
        if(yunsdr_write_samples(yunsdr, worker->buffer, worker->nsamples, worker->channel, timestamp) < 0) {
            printf("[%d]tx_timestamp:%" PRId64 ", timeout\n", i, timestamp);
            break;
        }
        //usleep(1000);
        //printf("[%d]tx_timestamp:%" PRId64 "\n", i, timestamp);
    }
    printf(">>>>>>>>>>>>>>>>>>>Test for RF%u end\n", worker->channel);

    return NULL;
}
