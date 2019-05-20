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

int32_t __init_transport(YUNSDR_TRANSPORT *trans)
{
    int ret;

    for(int i = 0; i < MAX_RF_STREAM; i++) {
#if defined(__WINDOWS_) || defined(_WIN32)
        trans->rx_meta[i] = (YUNSDR_META *)_aligned_malloc(MAX_BUFFSIZE + sizeof(YUNSDR_META), 16);
        if (!trans->rx_meta[i]) {
            ret = -1;
        }
        else {
            ret = 0;
        }

        trans->tx_meta[i] = (YUNSDR_META *)_aligned_malloc(MAX_BUFFSIZE + sizeof(YUNSDR_META), 16);
        if (!trans->tx_meta[i]) {
            ret = -1;
        }
        else {
            ret = 0;
        }
    }
#else
        ret = posix_memalign((void **)&trans->rx_meta[i], 16, sizeof(int16_t) * MAX_BUFFSIZE + sizeof(YUNSDR_META));
        if(ret) {
            printf("Failed to alloc memory\n");
            return -1;
        }
        ret = posix_memalign((void **)&trans->tx_meta[i], 16, sizeof(int16_t) * MAX_BUFFSIZE + sizeof(YUNSDR_META));
        if(ret) {
            printf("Failed to alloc memory\n");
            return -1;
        }
    }
    ret = 0;
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

int32_t __deinit_transport(YUNSDR_TRANSPORT *trans)
{
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

    for(int i = 0; i < MAX_RF_STREAM; i++) {
#if defined(__WINDOWS_) || defined(_WIN32)
        _aligned_free(trans->rx_meta[i]);
        _aligned_free(trans->tx_meta[i]);
#else
        free(trans->rx_meta[i]);
        free(trans->tx_meta[i]);
#endif
    }

    return 0;
}
