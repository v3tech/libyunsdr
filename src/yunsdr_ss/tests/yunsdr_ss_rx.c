
#include <stdio.h>
#include <stdbool.h>
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

#define NBYTE_OF_METAHEADER 16
#define MAX_RX_WORKER  4
#define TEST_NSAMPLES  (8188L)
#define TEST_CYCLE     10000UL

static uint32_t auxdac1 = 1200; 
static uint32_t flen = (TEST_NSAMPLES*4);
static float rf_rx_gain = 40, rf_freq = 2.4e9, rf_sampling = 30.72e6; 
static uint8_t rf_rx_ch = RX1_CHANNEL;
static uint8_t rx_ch_mask = 1, num_of_rfport = 0;
static char *rf_args="pcie:0";
static uint32_t max_rx_cycle = TEST_CYCLE;
static bool file_save = false;
static FILE *f[MAX_RX_WORKER];

static YUNSDR_DESCRIPTOR *yunsdr;

typedef struct {
    uint8_t channel;
    uint32_t nsamples;
    short *buffer;
    uint8_t buf_index;
    uint64_t rx_timestamp;
}RX_WORKER;

#if defined(__WINDOWS_) || defined(_WIN32)
DWORD WINAPI rf_rx_worker(LPVOID arg);
#else
void *rf_rx_worker(void *arg);
#endif
void usage(char *prog)
{
    printf("Usage: %s -o [rx_signal_file]\n", prog);
    printf("\t-a RF Args [Default %s]\n", rf_args);
    printf("\t-f RF RX Frequency [Default %.2f MHz]\n", rf_freq/1e6);
    printf("\t-G RF RX Gain [Default %.1f dB]\n", rf_rx_gain);
    printf("\t-s RF RX Sampling frequency [Default %.2f MHz]\n", rf_sampling/1e6);
    printf("\t-C RF RX Channel[1,2,3,4] [Default Channel%u]\n", rf_rx_ch);
    printf("\t-T Test Cycle [Default %ld]\n", TEST_CYCLE);
    printf("\t-N Test Number of tone samples [Default %lu]\n", TEST_NSAMPLES);
    printf("\t-F Enable file save function [Default false]\n");
    printf("\t-d RF auxdac1 value [Default %u mV]\n", auxdac1);
}

void parse_args(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "i:a:f:G:t:C:s:T:N:d:hF")) != -1) {
        switch (opt) {
        case 'a':
            rf_args = optarg;
            break;
        case 'f':
            rf_freq = atof(optarg);
            break;
        case 'G':
            rf_rx_gain = atof(optarg);
            break;
        case 'T':
            max_rx_cycle = atof(optarg);
            break;
        case 'N':
            flen = atof(optarg) * 4;
            break;
        case 'C':
            {
                char *ch;
                char *ptr = optarg;
                while ((ch = strtok(ptr, ",")) != NULL) {
                    ptr = NULL;
                    uint8_t index = atoi(ch);
                    if(index > 0 && index < 5) {
                        rx_ch_mask = rx_ch_mask | (1 << (index - 1));
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
        case 'F':
            file_save = true;
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

/* Subtract timespec t2 from t1
 *
 * Both t1 and t2 must already be normalized
 * i.e. 0 <= nsec < 1000000000
 */
static int timespec_check(struct timespec *t)
{
    if ((t->tv_nsec < 0) || (t->tv_nsec >= 1000000000))
        return -1;
    return 0;

}

void timespec_sub(struct timespec *t1, struct timespec *t2)
{
    if (timespec_check(t1) < 0) {
        fprintf(stderr, "invalid time #1: %lld.%.9ld.\n",
                (long long)t1->tv_sec, t1->tv_nsec);
        return;
    }
    if (timespec_check(t2) < 0) {
        fprintf(stderr, "invalid time #2: %lld.%.9ld.\n",
                (long long)t2->tv_sec, t2->tv_nsec);
        return;
    }
    t1->tv_sec -= t2->tv_sec;
    t1->tv_nsec -= t2->tv_nsec;
    if (t1->tv_nsec >= 1000000000) {
        t1->tv_sec++;
        t1->tv_nsec -= 1000000000;
    } else if (t1->tv_nsec < 0) {
        t1->tv_sec--;
        t1->tv_nsec += 1000000000;
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

int main(int argc, char **argv)
{
    parse_args(argc, argv);

    printf("Opening RF device...\n");
    yunsdr = yunsdr_open_device(rf_args);
    if (yunsdr <= 0) {
        fprintf(stderr, "Error opening rf\n");
        exit(-1);
    }

    yunsdr_set_ref_clock (yunsdr, 0, INTERNAL_REFERENCE);
    yunsdr_set_pps_select (yunsdr, 0, PPS_INTERNAL_EN);
    yunsdr_set_vco_select (yunsdr, 0, AUXDAC1);
    yunsdr_set_auxdac1 (yunsdr, 0, auxdac1);
    yunsdr_set_rx_ant_enable (yunsdr, 0, 1);
    yunsdr_set_duplex_select (yunsdr, 0, FDD);
    yunsdr_set_trxsw_fpga_enable (yunsdr, 0, 0);
    yunsdr_set_hwbuf_depth(yunsdr, 0, 3276800);

    if(rx_ch_mask & 1 || rx_ch_mask >> 1 & 1) {
        yunsdr_set_rx_lo_freq (yunsdr, 0, rf_freq);
        yunsdr_set_rx_sampling_freq (yunsdr, 0, rf_sampling);
        yunsdr_set_rx_rf_bandwidth (yunsdr, 0, rf_sampling);
        if(rx_ch_mask & 1)
            yunsdr_set_rx1_rf_gain (yunsdr, 0, rf_rx_gain);
        if(rx_ch_mask >> 1 & 1)
            yunsdr_set_rx2_rf_gain (yunsdr, 0, rf_rx_gain);
    }

    if(rx_ch_mask >> 2 & 1 || rx_ch_mask >> 3 & 1) {
        yunsdr_set_rx_lo_freq (yunsdr, 1, rf_freq);
        yunsdr_set_rx_sampling_freq (yunsdr, 1, rf_sampling);
        yunsdr_set_rx_rf_bandwidth (yunsdr, 1, rf_sampling);
        if(rx_ch_mask >> 2 & 1)
            yunsdr_set_rx1_rf_gain (yunsdr, 1, rf_rx_gain);
        if(rx_ch_mask >> 3 & 1)
            yunsdr_set_rx2_rf_gain (yunsdr, 1, rf_rx_gain);
    }

    yunsdr_enable_timestamp (yunsdr, 0, 0);
    yunsdr_enable_timestamp (yunsdr, 0, 1);
#if defined(__WINDOWS_) || defined(_WIN32)
    Sleep(2000);
#else
    sleep(2);
#endif

    printf("Rx Signal frame len: %u samples\n", flen / 4);
    printf("Set Rx rate:   %.2f MHz\n", (float) rf_sampling / 1000000);
    printf("Set Rx gain:   %.1f dB\n", rf_rx_gain);
    printf("Set Rx freq:   %.2f MHz\n", rf_freq / 1000000);
    printf("Set AUXDAC1:   %u mV\n", auxdac1);


    short **rx_buffer_meta = malloc(sizeof(short *) * num_of_rfport);
    for(int i = 0; i < num_of_rfport; i++) {
        rx_buffer_meta[i] = malloc(flen + NBYTE_OF_METAHEADER);
        if (!rx_buffer_meta[i]) {
            perror("malloc");
            exit(-1);
        }
    }

    if(file_save) {
        char filename[MAX_RX_WORKER][64];
        for(int i = 0; i < num_of_rfport; i++) {
            sprintf(filename[i], "rx_iq_[port-%d].dat", i);

            f[i] = fopen(filename[i], "wb");
            if (f[i] == NULL) {
                printf("Cannot open file %s to save IQ data.\n", filename[i]);
                exit(-1);
            }
        }
    }

    printf("Recving Signal\n");        

    RX_WORKER worker[MAX_RX_WORKER];
#if defined(__WINDOWS_) || defined(_WIN32)
    DWORD tidRX[MAX_RX_WORKER];
    HANDLE worker_id[MAX_RX_WORKER];
#else
    pthread_t worker_id[MAX_RX_WORKER];
#endif

    uint8_t buf_index = 0;;

    for(int i = 0; i < MAX_RX_WORKER; i++) {
        if(rx_ch_mask >> i & 1) {
            worker[i].channel = i + 1;
            worker[i].nsamples = flen / 4;
            worker[i].buffer = rx_buffer_meta[buf_index];
            worker[i].buf_index = buf_index;
            worker[i].rx_timestamp = 0;
            buf_index++;

#if defined(__WINDOWS_) || defined(_WIN32)
            worker_id[i] = CreateThread(NULL, 0, rf_rx_worker, (LPVOID)&worker[i], 0, &tidRX[i]);
#else
            if(pthread_create(&worker_id[i], NULL, rf_rx_worker, &worker[i])) {
                printf("Cannot create worker thread!\n");
                exit(-1);
            }
#endif
        }
    }

    for(int i = 0; i < MAX_RX_WORKER; i++) {
        if(rx_ch_mask >> i & 1) {
#if defined(__WINDOWS_) || defined(_WIN32)
            WaitForSingleObject(worker_id[i], INFINITE);
            CloseHandle(worker_id[i]);
#else
            pthread_join(worker_id[i], NULL);
#endif
        }
    }

    if(file_save) {
        for(int i = 0; i < num_of_rfport; i++)
            fclose(f[i]);
    }

    for(int i = 0; i < num_of_rfport; i++) {
        if(rx_ch_mask >> i & 1) {
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

    for(int i = 0; i < num_of_rfport; i++)
        free(rx_buffer_meta[i]);

    free(rx_buffer_meta);

    printf("Done\n");
    exit(0);
}

#if defined(__WINDOWS_) || defined(_WIN32)
DWORD WINAPI rf_rx_worker(LPVOID arg)
#else
void *rf_rx_worker(void *arg)
#endif
{
    RX_WORKER *worker = (RX_WORKER *)arg;
    uint64_t timestamp = 0;
    int32_t i;
#if defined(__WINDOWS_) || defined(_WIN32)
    LARGE_INTEGER  large_interger;
    uint64_t c1, c2, total_tick = 0;
    QueryPerformanceFrequency(&large_interger);
    double cfreq = large_interger.QuadPart;;
#else
	struct timespec ts_start, ts_end;
#endif
	uint64_t total_time = 0;

    printf(">>>>>>>>>>>>>>>>>>>Test for RF%u start\n", worker->channel);
    for(i = 0; i < max_rx_cycle; i++) {
#if defined(__WINDOWS_) || defined(_WIN32)
        QueryPerformanceCounter(&large_interger);
        c1 = large_interger.QuadPart;
#else
		clock_gettime(CLOCK_MONOTONIC, &ts_start);
#endif
        if(i == 0) {
            timestamp = worker->rx_timestamp;
        } else
            timestamp = 0;
        if(yunsdr_read_samples_zerocopy(yunsdr, worker->buffer, worker->nsamples, worker->channel, &timestamp) < 0) {
            printf("[%d]tx_timestamp:%" PRId64 ", timeout\n", i, timestamp);
            break;
        }
        //printf("[%d]rx_timestamp:%" PRId64 "\n", i, timestamp);
        if(file_save)
            fwrite(worker->buffer, worker->nsamples * 4, 1, f[worker->channel - 1]);
#if defined(__WINDOWS_) || defined(_WIN32)
        QueryPerformanceCounter(&large_interger);
        c2 = large_interger.QuadPart;
        total_tick += (c2 - c1);
#else
		clock_gettime(CLOCK_MONOTONIC, &ts_end);
		/* subtract the start time from the end time */
		timespec_sub(&ts_end, &ts_start);
		total_time += ts_end.tv_nsec;
#endif
    }
#if defined(__WINDOWS_) || defined(_WIN32)
    total_time = total_tick / cfreq * 1e9;
#endif
	printf("** total time %" PRIu64 " nsec(%f mins) **\n", total_time, total_time/1e9/60);
    printf(">>>>>>>>>>>>>>>>>>>Test for RF%u end\n", worker->channel);

    return NULL;
}
