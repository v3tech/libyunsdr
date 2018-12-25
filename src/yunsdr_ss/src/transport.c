#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "transport.h"
#ifdef ENABLE_PCIE
#include "interface_pcie.h"
#endif
#ifdef ENABLE_SFP
#include "interface_sfp.h"
#endif

int32_t init_transport(YUNSDR_TRANSPORT *trans)
{
    int ret;
    
    ret = posix_memalign((void **)&trans->rx_meta, 16, sizeof(int16_t) * MAX_BUFFSIZE + sizeof(YUNSDR_META));
    if(ret) {
        printf("Failed to alloc memory\n");
        return -1;
    }
    ret = posix_memalign((void **)&trans->tx_meta, 16, sizeof(int16_t) * MAX_BUFFSIZE + sizeof(YUNSDR_META));
    if(ret) {
        printf("Failed to alloc memory\n");
        return -1;
    }
    ret = 0;
#if 0
    trans->rx_meta = (YUNSDR_META *)calloc(1, MAX_BUFFSIZE + sizeof(YUNSDR_META));
    if (!trans->rx_meta) {
        ret = -1;
    }
    else {
        ret = 0;
    }

    trans->tx_meta = (YUNSDR_META *)calloc(1, MAX_BUFFSIZE + sizeof(YUNSDR_META));
    if (!trans->tx_meta) {
        ret = -1;
    }
    else {
        ret = 0;
    }
#endif
    switch (trans->type) {
    case INTERFACE_PCIE:
#ifdef ENABLE_PCIE
        if (init_interface_pcie(trans) < 0)
            return -1;
#else
        printf("INTERFACE_PCIE not support yet\n");
        return -1;
#endif
        break;
    case INTERFACE_SFP:
#ifdef ENABLE_SFP
        if (init_interface_sfp(trans) < 0)
            return -1;
#else
        printf("INTERFACE_SFP not support yet\n");
        return -1;
#endif
        break;
    case INTERFACE_GIGABIT:
        printf("INTERFACE_GIGABIT not support yet\n");
        return -1;
        break;
    default:
        break;

    }

    return ret;
}

int32_t deinit_transport(YUNSDR_TRANSPORT *trans)
{
    free(trans->rx_meta);
    free(trans->tx_meta);

    switch (trans->type) {
    case INTERFACE_PCIE:
#ifdef ENABLE_PCIE
        deinit_interface_pcie(trans);
#endif
        break;
    case INTERFACE_SFP:
#ifdef ENABLE_SFP
        deinit_interface_sfp(trans);
#endif
        break;
    case INTERFACE_GIGABIT:
        printf("INTERFACE_GIGABIT not support yet\n");
        break;
    default:
        break;

    }

    return 0;
}
