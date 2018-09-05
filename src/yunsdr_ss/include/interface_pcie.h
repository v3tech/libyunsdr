#pragma once
#ifndef __INTERFACE_PCIE__
#define __INTERFACE_PCIE__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <riffa.h>
#include "transport.h"

typedef struct interface_handle_pcie {
    fpga_t *fpga;
    int32_t id;
    int32_t num_of_channel;
}PCIE_HANDLE;


int32_t init_interface_pcie(YUNSDR_TRANSPORT *trans);
int32_t deinit_interface_pcie(YUNSDR_TRANSPORT *trans);

#endif // !__INTERFACE_PCIE__
