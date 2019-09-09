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

static char *rf_args = "sfp:169.254.45.119";

static YUNSDR_DESCRIPTOR *yunsdr;

void usage(char *prog)
{
    printf("Usage: %s -a \"sfp:169.254.45.119\"\n", prog);
    printf("\t-a RF Args [Default %s]\n", rf_args);
}

void parse_args(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "a:h")) != -1) {
        switch (opt) {
        case 'a':
            rf_args = optarg;
            break;
        case 'h':
        default:
            usage(argv[0]);
            exit(-1);
        }
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

    for(int i = 0; i < 15; i++) {
        struct tm ltime;
        struct xyz_t xyz;

        yunsdr_get_utc(yunsdr, 0, &ltime);
        yunsdr_get_xyz(yunsdr, 0, &xyz);

        printf("%d-%d-%d, %d-%d-%d\n", ltime.tm_year, ltime.tm_mon,
                ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
        printf("latitude:%.4f\t", xyz.latitude);
        printf("longitude:%.4f\t", xyz.longitude);
        printf("altitude:%.2f\n", xyz.altitude);
#if defined(__WINDOWS_) || defined(_WIN32)
    Sleep(1000);
#else
    sleep(1);
#endif
    }

    yunsdr_close_device(yunsdr);

    printf("Done\n");
    exit(0);
}
