#pragma once
#ifndef __TRANSPORT_H__
#define __TRANSPORT_H__

#include <stdint.h>

#define MAX_BUFFSIZE (32*1024*1024)
#define MAX_TX_BULK_SIZE (8192*4)

#define MIN(x, y)  ((x < y)?x:y)

typedef struct yunsdr_trans_cmd {
    uint32_t head;
    uint8_t  cmd_id;
    uint8_t  w_or_r;
    uint8_t  rf_id;
    uint8_t  reserve;
    uint32_t cmd_l;
    uint32_t cmd_h;
}YUNSDR_CMD;

typedef struct yunsdr_trans_read_req {
    uint32_t head;
    uint32_t rxlength;
    uint32_t rxtime_l;
    uint32_t rxtime_h;
}YUNSDR_READ_REQ;

typedef enum e_interface {
    INTERFACE_GIGABIT,
    INTERFACE_SFP,
    INTERFACE_PCIE
}INTERFACES;

typedef struct yunsdr_meta {
    uint32_t head;
    uint32_t nsamples;
    uint32_t timestamp_l;
    uint32_t timestamp_h;
    uint32_t payload[0];
}YUNSDR_META;

typedef struct yunsdr_transport YUNSDR_TRANSPORT;
struct yunsdr_transport {
    void *app_opaque;
    void *opaque;
    YUNSDR_META *tx_meta;
    YUNSDR_META *rx_meta;
    INTERFACES type;
    int32_t(*cmd_send)(YUNSDR_TRANSPORT *trans, uint8_t rf_id, uint8_t cmd_id, void *buf, uint32_t len);
    int32_t (*cmd_send_then_recv)(YUNSDR_TRANSPORT *trans, uint8_t rf_id, uint8_t cmd_id, void *buf, uint32_t len, uint8_t with_param);

    int32_t (*stream_recv)(YUNSDR_TRANSPORT *trans, void *buf, uint32_t count, uint8_t channel, uint64_t *timestamp);
    /* for zero copy, the data in buf contains a 16-byte header */
    int32_t (*stream_recv2)(YUNSDR_TRANSPORT *trans, void *buf, uint32_t count, uint8_t channel, uint64_t *timestamp);
    /* for mimo mode */
    int32_t (*stream_recv3)(YUNSDR_TRANSPORT *trans, void **buf, uint32_t count, uint8_t channel_mask, uint64_t *timestamp);
    int32_t (*stream_send)(YUNSDR_TRANSPORT *trans, void *buf, uint32_t count, uint8_t channel, uint64_t timestamp);
    int32_t (*stream_send2)(YUNSDR_TRANSPORT *trans, void *buf, uint32_t count, uint8_t channel, uint64_t timestamp, uint32_t flags);
    int32_t (*stream_send3)(YUNSDR_TRANSPORT *trans, const void **buf, uint32_t count, uint8_t channel_mask, uint64_t timestamp, uint32_t flags);
};

int32_t init_transport(YUNSDR_TRANSPORT *trans);
int32_t deinit_transport(YUNSDR_TRANSPORT *trans);

#endif
