#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include "device.h"
#include "yunsdr_api_ss.h"

#define ROUND 1
#define YUNSDR_ID 0
#define NSAMPLES_PER_PACKET (23040)

#define __RX_ENABLE__
//#define __TX_ENABLE__

pthread_t tx_thread;
pthread_t rx_thread;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static uint64_t timestamp;

void *thread_tx(void *arg);
void *thread_rx(void *arg);
void cossingen(short *buf, int nsamples, uint8_t ch);

void handler(int sigid)
{
#ifdef __TX_ENABLE__
    pthread_cancel(tx_thread);
#endif
#ifdef __RX_ENABLE__
    pthread_cancel(rx_thread);
#endif

}

int main(int argc, char *argv[])
{
    int32_t ret, i;

    YUNSDR_DESCRIPTOR *yunsdr;
    printf("Opening YunSDR %u ...\t", YUNSDR_ID);
    //yunsdr = yunsdr_open_device("pcie:0");
	yunsdr = yunsdr_open_device("sfp:169.254.45.119");
    if(yunsdr > 0) {
        yunsdr_rf_other_init(yunsdr);
        yunsdr_rf_tx_init(yunsdr);
        yunsdr_rf_rx_init(yunsdr);
        printf("Open success!\n");
    } else {
        yunsdr = NULL;
        printf("Open failed!\n");
        return -1;
    }
    uint32_t version, model;
    yunsdr_get_firmware_version(yunsdr, &version);
    yunsdr_get_model_version(yunsdr, &model);
    printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    printf("firmware version: %#x\n", version&0xffff);
    printf("software version: %#x\n", version>>16);
    printf("product version: Y%u\n", model&0xffff);
    printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
    yunsdr_enable_timestamp (yunsdr, 0, 0);

    sleep(1);

    yunsdr_enable_timestamp (yunsdr, 0, 1);
#ifdef __TX_ENABLE__
    ret = pthread_create(&tx_thread, NULL, &thread_tx, yunsdr);
    if (ret)
    {
        printf("ERROR, return code from pthread_create() is %d\n", ret);
        return -1;
    }
#endif
#ifdef __RX_ENABLE__
    ret = pthread_create(&rx_thread, NULL, &thread_rx, yunsdr);
    if (ret)
    {
        printf("ERROR, return code from pthread_create() is %d\n", ret);
        return -1;
    }
#endif
    signal(SIGINT, handler);
#ifdef __TX_ENABLE__
    pthread_join(tx_thread, NULL);
#endif
#ifdef __RX_ENABLE__
    pthread_join(rx_thread, NULL);
#endif
    yunsdr_close_device(yunsdr);

    return 0;
}

void *thread_tx(void *arg)
{
    YUNSDR_DESCRIPTOR *yunsdr = (YUNSDR_DESCRIPTOR *)arg;
    FILE *fp;
    uint32_t nread, nwrite, block_size;
    uint8_t *buf;
    uint64_t tx_timestamp = 0;
    uint32_t count;

    block_size = NSAMPLES_PER_PACKET * 4;

    buf = (uint8_t *)malloc(block_size * 2);
    if(buf == NULL) {
        fprintf(stderr, "Cannot alloc %d Bytes memory for yunsdr[%d] failed\n",
                block_size, yunsdr->id);
        return (void *)(-1);
    }
    cossingen((short *)buf, NSAMPLES_PER_PACKET, 1);

    yunsdr_read_timestamp (yunsdr, 0, &timestamp);
    count = 0;
    printf("block_size: %d Bytes\n", block_size);
    struct timeval tpstart,tpend; 
    float timeuse; 

    while(1) {
        if(count == 0)
            gettimeofday(&tpstart,NULL); 
        //pthread_mutex_lock(&lock);
        tx_timestamp +=  (NSAMPLES_PER_PACKET + yunsdr_timeNsToTicks(1*1e6, SAMPLEING_FREQ));
        //pthread_mutex_unlock(&lock);
        //printf("tx timestamp:%lld, samples:%d\n", tx_timestamp, block_size/4);;
        pthread_testcancel();
		nwrite = yunsdr_write_samples(yunsdr, buf, NSAMPLES_PER_PACKET, 1, 0);// tx_timestamp);
        if(nwrite < 0){
            printf("Send data error, will be closed ...\n");
            break;
        }
		uint64_t usec = yunsdr_ticksToTimeNs(NSAMPLES_PER_PACKET, SAMPLEING_FREQ) / 1e4;
		usleep(usec);
        count++;
        if(count == 1024){
            gettimeofday(&tpend,NULL); 
            timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+ 
                tpend.tv_usec-tpstart.tv_usec; 
            timeuse/=1000000; 
            printf("TX RealTime Rate:%.2f MB/s\n",nwrite/1024.0/timeuse);
            count = 0;
        }
    }

    free(buf);

    return NULL;
}

void *thread_rx(void *arg)
{
    YUNSDR_DESCRIPTOR *yunsdr = (YUNSDR_DESCRIPTOR *)arg;
    FILE *fp;
    uint32_t nread, nwrite;
    uint32_t count;
    uint32_t block_size;
    uint8_t *buf;
    uint64_t rx_timestamp;

    fp = fopen("data_rx.bin", "wb+");
    if(fp == NULL) {
        fprintf(stderr, "Open data.bin for yunsdr[%d] failed\n", yunsdr->id);
        return (void *)(-1);
    }

    block_size = NSAMPLES_PER_PACKET * 4;

    buf = (uint8_t *)malloc(block_size*2);
    if(buf == NULL) {
        fprintf(stderr, "Cannot alloc %d Bytes memory for yunsdr[%d] failed\n",
                block_size, yunsdr->id);
        return (void *)(-1);
    }

    //yunsdr_read_timestamp (yunsdr, 0, &timestamp);
    
    count = 0;

    do {
        rx_timestamp = 0;
        pthread_testcancel();
        nread = yunsdr_read_samples(yunsdr, buf, NSAMPLES_PER_PACKET, RX1_CHANNEL, &rx_timestamp);
        if(nread < 0){
            printf("Recv data error, will be closed ...\n");
            break;
        }

        //pthread_mutex_lock(&lock);
        timestamp = rx_timestamp;
        //pthread_mutex_unlock(&lock);
        printf("rx_timestamp:%#lx, samples:%d\n", timestamp, nread);
#if 0
        nwrite = fwrite(buf, 1, nread, fp);
        if(nwrite < 0) {
            perror("fread");
            continue;
        }
#endif
        count++;
    } while(count < (ROUND)*1024);

    uint32_t value = 0;
    //yunsdr_get_status (yunsdr, 32, 1, &value);
    //printf("overflow:%d\n", value);
    printf("FILE_SIZE:%d bytes.\n", block_size*1024);
    fclose(fp);
    free(buf);

    return NULL;
}
