#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <riffa.h>

#include "interface_pcie.h"
#include "transport.h"
#include "debug.h"
#include "spinlock.h"

int32_t pcie_cmd_send(YUNSDR_TRANSPORT *trans, uint8_t rf_id, uint8_t cmd_id, void *buf, uint32_t len)
{
	int ret = 0;
	PCIE_HANDLE *handle = (PCIE_HANDLE *)trans->opaque;
	YUNSDR_CMD pcie_cmd;
	uint64_t parameter;
	switch (len)
	{
	case 1:
		parameter = *(uint8_t *)buf;
		break;
	case 2:
		parameter = *(uint16_t *)buf;
		break;
	case 4:
		parameter = *(uint32_t *)buf;
		break;
	case 8:
		parameter = *(uint64_t *)buf;
		break;
	default:
		return -EINVAL;
	}

	pcie_cmd.head = 0xdeadbeef;
	pcie_cmd.reserve = 0;
	pcie_cmd.rf_id = rf_id;
	pcie_cmd.w_or_r = 1;
	pcie_cmd.cmd_id = cmd_id;
	pcie_cmd.cmd_l = (uint32_t)parameter;
	pcie_cmd.cmd_h = parameter >> 32;

    lock();
	ret = fpga_send(handle->fpga, 0, &pcie_cmd, sizeof(YUNSDR_CMD) / 4, 0, 1, 25000);
	if (ret < 0) {
		printf("%s failed\n", __func__);
		return ret;
	}
	memset(&pcie_cmd, 0, sizeof(YUNSDR_CMD));
	ret = fpga_recv(handle->fpga, 0, &pcie_cmd, sizeof(YUNSDR_CMD) / 4, 25000);
	if (ret < 0) {
		printf("%s failed\n", __func__);
		return ret;
	}
    unlock();
	return 0;
}

int32_t pcie_cmd_send_then_recv(YUNSDR_TRANSPORT *trans, uint8_t rf_id, uint8_t cmd_id, void *buf, uint32_t len, uint8_t with_param)
{
	int ret = 0;
	PCIE_HANDLE *handle = (PCIE_HANDLE *)trans->opaque;
	YUNSDR_CMD pcie_cmd;
    uint64_t parameter;
	switch (len)
	{
	case 1:
		parameter = *(uint8_t *)buf;
		break;
	case 2:
		parameter = *(uint16_t *)buf;
		break;
	case 4:
		parameter = *(uint32_t *)buf;
		break;
	case 8:
		parameter = *(uint64_t *)buf;
		break;
	default:
		return -EINVAL;
	}

	pcie_cmd.head = 0xdeadbeef;
	pcie_cmd.reserve = 0;
	pcie_cmd.rf_id = rf_id;
	pcie_cmd.w_or_r = 0;
	pcie_cmd.cmd_id = cmd_id;
    if(with_param) {
	    pcie_cmd.cmd_l = (uint32_t)parameter;
	    pcie_cmd.cmd_h = parameter >> 32;
    } else {
    	pcie_cmd.cmd_l = 0;
	    pcie_cmd.cmd_h = 0;
    }

    lock();
	ret = fpga_send(handle->fpga, 0, &pcie_cmd, sizeof(YUNSDR_CMD) / 4, 0, 1, 25000);
	if (ret < 0) {
		printf("%s failed\n", __func__);
		return ret;
	}

	memset(&pcie_cmd, 0, sizeof(YUNSDR_CMD));
	ret = fpga_recv(handle->fpga, 0, &pcie_cmd, sizeof(YUNSDR_CMD) / 4, 25000);
	if (ret < 0) {
		printf("%s failed\n", __func__);
		return ret;
	}
    unlock();

	switch (len)
	{
	case 1:
		*(uint8_t *)buf = (uint8_t )pcie_cmd.cmd_l;
		break;
	case 2:
		*(uint16_t *)buf = (uint16_t)pcie_cmd.cmd_l;
		break;
	case 4:
		*(uint32_t *)buf = pcie_cmd.cmd_l;
		break;
	case 8:
		*(uint64_t *)buf = pcie_cmd.cmd_l | ((uint64_t)pcie_cmd.cmd_h << 32);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int32_t pcie_stream_recv(YUNSDR_TRANSPORT *trans, void *buf, uint32_t count, uint8_t channel, uint64_t *timestamp)
{
	int ret;
	PCIE_HANDLE *handle = (PCIE_HANDLE *)trans->opaque;
	YUNSDR_META *rx_meta = trans->rx_meta;
	YUNSDR_READ_REQ pcie_cmd;

	pcie_cmd.head = 0xcafefee0 | channel;
	pcie_cmd.rxlength = count + sizeof(YUNSDR_READ_REQ) / 4;
	pcie_cmd.rxtime_l = (uint32_t)*timestamp;
	pcie_cmd.rxtime_h = *timestamp >> 32;

    lock();
	ret = fpga_send(handle->fpga, 0, &pcie_cmd, sizeof(YUNSDR_READ_REQ) / 4, 0, 1, 25000);
	if (ret < 0) {
		printf("%s failed\n", __func__);
		return ret;
	}

	ret = fpga_recv(handle->fpga, channel, rx_meta, pcie_cmd.rxlength, 25000);
	if (ret < 0) {
		printf("%s failed\n", __func__);
		ret = -EIO;
		return ret;
	}
    unlock();

	*timestamp = ((uint64_t)rx_meta->timestamp_h) << 32 | rx_meta->timestamp_l;
	memcpy(buf, (unsigned char *)rx_meta->payload, count * 4);

	return count;
}

int32_t pcie_stream_recv2(YUNSDR_TRANSPORT *trans, void *buf, uint32_t count, uint8_t channel, uint64_t *timestamp)
{
	int ret;
	PCIE_HANDLE *handle = (PCIE_HANDLE *)trans->opaque;
	YUNSDR_READ_REQ pcie_cmd;

	pcie_cmd.head = 0xcafefee0 | channel;
	pcie_cmd.rxlength = count + sizeof(YUNSDR_READ_REQ) / 4;
	pcie_cmd.rxtime_l = (uint32_t)*timestamp;
	pcie_cmd.rxtime_h = *timestamp >> 32;

    lock();
	ret = fpga_send(handle->fpga, 0, &pcie_cmd, sizeof(YUNSDR_READ_REQ) / 4, 0, 1, 25000);
	if (ret < 0) {
		printf("%s failed\n", __func__);
		return ret;
	}

	ret = fpga_recv(handle->fpga, channel, buf, pcie_cmd.rxlength, 25000);
	if (ret < 0) {
		printf("%s failed\n", __func__);
		ret = -EIO;
		return ret;
	}
    unlock();

	YUNSDR_META *rx_meta = (YUNSDR_META *)buf;
	*timestamp = ((uint64_t)rx_meta->timestamp_h) << 32 | rx_meta->timestamp_l;

	return count;
}

int32_t pcie_stream_send(YUNSDR_TRANSPORT *trans, void *buf, uint32_t count, uint8_t channel, uint64_t timestamp)
{
	int ret;
	char *samples;
	PCIE_HANDLE *handle = (PCIE_HANDLE *)trans->opaque;
	YUNSDR_META *tx_meta = trans->tx_meta;
	
	tx_meta->timestamp_l = (uint32_t)timestamp;
	tx_meta->timestamp_h = (uint32_t)(timestamp >> 32);
	tx_meta->head = 0xdeadbeef;
	tx_meta->nsamples = count;

	memcpy(tx_meta->payload, buf, count * 4);
	samples = (void *)tx_meta;

	int32_t remain = 0;
	int32_t nbytes = 0;
	int32_t sum = count * 4 + sizeof(YUNSDR_META);
	do {
		nbytes = MIN(MAX_TX_BULK_SIZE, sum);
		remain = sum - nbytes;
		ret = fpga_send(handle->fpga, channel, samples, nbytes / 4, 0, 1, 25000);
		if (ret < 0) {
			printf("%s failed\n", __func__);
			ret = -EIO;
			return ret;
		}
		samples += nbytes;
		sum -= nbytes;
	} while (remain > 0);

	return count;
}

int32_t pcie_stream_send2(YUNSDR_TRANSPORT *trans, void *buf, uint32_t count, uint8_t channel, uint64_t timestamp, uint32_t flags)
{
	int ret;
	char *samples;
	PCIE_HANDLE *handle = (PCIE_HANDLE *)trans->opaque;
	YUNSDR_META *tx_meta = trans->tx_meta;
	
	tx_meta->timestamp_l = (uint32_t)timestamp;
	tx_meta->timestamp_h = (uint32_t)(timestamp >> 32);
    if(flags)
        tx_meta->head        = 0xdeadbeee;
    else
        tx_meta->head        = 0xdeadbeef;
	tx_meta->nsamples = count;

	memcpy(tx_meta->payload, buf, count * 4);
	samples = (void *)tx_meta;

	int32_t remain = 0;
	int32_t nbytes = 0;
	int32_t sum = count * 4 + sizeof(YUNSDR_META);
	do {
		nbytes = MIN(MAX_TX_BULK_SIZE, sum);
		remain = sum - nbytes;
		ret = fpga_send(handle->fpga, channel, samples, nbytes / 4, 0, 1, 25000);
		if (ret < 0) {
			printf("%s failed\n", __func__);
			ret = -EIO;
			return ret;
		}
		samples += nbytes;
		sum -= nbytes;
	} while (remain > 0);

	return count;
}

int32_t init_interface_pcie(YUNSDR_TRANSPORT *trans)
{
	PCIE_HANDLE *handle;

	handle = (PCIE_HANDLE *)malloc(sizeof(PCIE_HANDLE));
	memset(handle, 0, sizeof(*handle));
	handle->id = *((int *)trans->app_opaque);

	fpga_info_list info;
	if (fpga_list(&info) != 0) {
		printf("Error populating fpga_info_list\n");
		//return -1;
	}
	else {
		printf("Number of devices: %d\n", info.num_fpgas);
		for (int i = 0; i < info.num_fpgas; i++) {
			printf("%d: id:%d\n", i, info.id[i]);
			printf("%d: num_chnls:%d\n", i, info.num_chnls[i]);
			printf("%d: name:%s\n", i, info.name[i]);
			printf("%d: vendor id:%04X\n", i, info.vendor_id[i]);
			printf("%d: device id:%04X\n", i, info.device_id[i]);
		}

	}
	handle->fpga = fpga_open(handle->id);

	if (handle->fpga == NULL) {
		printf("Could not open FPGA %d\n", handle->id);
		return -ENODEV;
	}

	// Reset
	fpga_reset(handle->fpga);

	trans->opaque = handle;
	trans->cmd_send = pcie_cmd_send;
	trans->cmd_send_then_recv = pcie_cmd_send_then_recv;
	trans->stream_recv = pcie_stream_recv;
	trans->stream_recv2 = pcie_stream_recv2;
	trans->stream_send = pcie_stream_send;
	trans->stream_send2 = pcie_stream_send2;

    spinlock_init();

	return 0;
}

int32_t deinit_interface_pcie(YUNSDR_TRANSPORT *trans)
{
	PCIE_HANDLE *handle = (PCIE_HANDLE *)trans->opaque;
    spinlock_deinit();
	if (handle->fpga != NULL) {
		fpga_close(handle->fpga);
		return 0;
	}

	return -1;
}
