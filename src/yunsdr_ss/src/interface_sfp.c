#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#if !defined(__WINDOWS_) && !defined(_WIN32)
#include <unistd.h>
#include <sys/sysinfo.h>
#endif
#include <fcntl.h>
#include <string.h>

#include "interface_sfp.h"
#include "transport.h"
#include "debug.h"


static int local_port[MAX_RF_STREAM] = { LOCAL_STREAM1_PORT, LOCAL_STREAM2_PORT };
static int remote_port[MAX_RF_STREAM] = { REMOTE_STREAM1_PORT, REMOTE_STREAM2_PORT };

static int init_udp_socket(SOCKADDR_IN *remote_addr, const char *ip, int local_port, int remote_port)
{
    SOCKET sockfd;
    SOCKADDR_IN local_addr;
#if defined(__WINDOWS_) || defined(_WIN32)
    int err;
    WSADATA wsa;
    WORD wVersionRequested;
    WSADATA wsaData;

    WSAStartup(MAKEWORD(2, 2), &wsa);
    wVersionRequested = MAKEWORD(2, 2);

    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        debug(DEBUG_ERR, "WSAStartup failed with error: %d\n", err);
        return -1;
    }

    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        debug(DEBUG_ERR, "Could not find a usable version of Winsock.dll\n");
        WSACleanup();
        return -1;
    }
    else
        ;//printf("The Winsock 2.2 dll was found okay\n");
#endif

    memset(remote_addr, 0, sizeof(SOCKADDR_IN));
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) <= 0) {
        printf("Create cmd socket fail!\n");
        return -1;
    }

#if defined(__WINDOWS_) || defined(_WIN32)
    remote_addr->sin_addr.S_un.S_addr = inet_addr(ip);
    remote_addr->sin_family = AF_INET;
    remote_addr->sin_port = htons(remote_port);

    //local_addr.sin_addr.S_un.S_addr = inet_addr(ip);
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(local_port);
    //uint32_t mode = 1;  //non-blocking mode is enabled.
    //ioctlsocket(sockfd, FIONBIO, (u_long *)&mode);
#else
    remote_addr->sin_family = AF_INET;
    remote_addr->sin_addr.s_addr = inet_addr(ip);
    remote_addr->sin_port = htons(remote_port);

    local_addr.sin_family = AF_INET;
    //local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_addr.s_addr = inet_addr("169.254.45.118");
    local_addr.sin_port = htons(local_port);

    //fcntl(sockfd, F_SETFL, O_NONBLOCK);
#endif

#if 0
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(int)) < 0) {
        perror("setsockopt");
        return errno;
    }
    if (bind(sockfd, (struct sockaddr *)&(local_addr), sizeof(local_addr)) < 0) {
        perror("bind");
        return -1;	
    }
#endif

    return sockfd;
}

static inline int8_t wait_for_recv_ready(SOCKET sock_fd, double timeout)
{
    struct timeval tv;
    struct timeval *ptv = NULL;

    if (timeout > 0.0) {
        //setup timeval for timeout
        //If the tv_usec > 1 second on some platforms, select will
        //error EINVAL: An invalid timeout interval was specified.
        tv.tv_sec = (int)timeout;
        tv.tv_usec = (int)(timeout * 1000000) % 1000000;
        ptv = &tv;
    }
    //setup rset for timeout
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(sock_fd, &rset);

    //http://www.gnu.org/s/hello/manual/libc/Interrupted-Primitives.html
    //This macro is provided with gcc to properly deal with EINTR.
    //If not provided, define an empty macro, assume that is OK
#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(x) (x)
#endif

    //call select with timeout on receive socket
#if defined(__WINDOWS_) || defined(_WIN32)
    return TEMP_FAILURE_RETRY(select(0, &rset, NULL, NULL, ptv)) > 0;
#else
    return TEMP_FAILURE_RETRY(select(sock_fd + 1, &rset, NULL, NULL, ptv)) > 0;
#endif
}


int32_t sfp_cmd_send(YUNSDR_TRANSPORT *trans, uint8_t rf_id, uint8_t cmd_id, void *buf, uint32_t len)
{
    int ret = 0;
    SFP_HANDLE *handle = (SFP_HANDLE *)trans->opaque;
    YUNSDR_CMD sfp_cmd;
#if defined(__GNUC__)
    socklen_t addr_len = sizeof(struct sockaddr_in);
#else
    int addr_len = sizeof(struct sockaddr_in);
#endif
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

    sfp_cmd.head = 0xdeadbeef;
    sfp_cmd.reserve = 0;
    sfp_cmd.rf_id = rf_id;
    sfp_cmd.w_or_r = 1;
    sfp_cmd.cmd_id = cmd_id;
    sfp_cmd.cmd_l = (uint32_t)parameter;
    sfp_cmd.cmd_h = parameter >> 32;

    ret = sendto(handle->cmd_sock, (char *)&sfp_cmd, sizeof(YUNSDR_CMD), 0, 
            (struct sockaddr *)&handle->cmd_addr, sizeof(handle->cmd_addr));
    if (ret < 0) {
        printf("%s failed, cmd id:%d\n", __func__, cmd_id);
        return ret;
    }
    if (wait_for_recv_ready(handle->cmd_sock, 3)) {
        ret = recvfrom(handle->cmd_sock, (char *)&sfp_cmd, sizeof(YUNSDR_CMD), 0,
                (struct sockaddr *)&handle->cmd_addr, &addr_len);
        if (ret < 0) {
            return -1;
        }
    } else {
        debug(DEBUG_ERR, "Recv cmd timeout! cmd id:%u\n", cmd_id);
        return -ETIMEDOUT;
    }
    return 0;
}

int32_t sfp_cmd_send_then_recv(YUNSDR_TRANSPORT *trans, uint8_t rf_id, uint8_t cmd_id, void *buf, uint32_t len, uint8_t with_param)
{
    int ret = 0;
    SFP_HANDLE *handle = (SFP_HANDLE *)trans->opaque;
    YUNSDR_CMD sfp_cmd;

#if defined(__GNUC__)
    socklen_t addr_len = sizeof(struct sockaddr_in);
#else
    int addr_len = sizeof(struct sockaddr_in);
#endif
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

    sfp_cmd.head = 0xdeadbeef;
    sfp_cmd.reserve = 0;
    sfp_cmd.rf_id = rf_id;
    sfp_cmd.w_or_r = 0;
    sfp_cmd.cmd_id = cmd_id;
    if(with_param) {
        sfp_cmd.cmd_l = (uint32_t)parameter;
        sfp_cmd.cmd_h = parameter >> 32;
    } else {
        sfp_cmd.cmd_l = 0;
        sfp_cmd.cmd_h = 0;
    }

    ret = sendto(handle->cmd_sock, (char *)&sfp_cmd, sizeof(YUNSDR_CMD), 0,
            (struct sockaddr *)&handle->cmd_addr, sizeof(handle->cmd_addr));
    if (ret < 0) {
        printf("%s failed, cmd id:%d\n", __func__, cmd_id);
        return -1;
    }

    memset(&sfp_cmd, 0, sizeof(YUNSDR_CMD));

    if (wait_for_recv_ready(handle->cmd_sock, 3)) {
        ret = recvfrom(handle->cmd_sock, (char *)&sfp_cmd, sizeof(YUNSDR_CMD), 0,
                (struct sockaddr *)&handle->cmd_addr, &addr_len);
        if (ret < 0) {
            return -1;
        }
    }
    else {
        //debug(DEBUG_ERR, "Recv cmd timeout!\n");
        return -ETIMEDOUT;
    }

    switch (len)
    {
    case 1:
        *(uint8_t *)buf = (uint8_t)sfp_cmd.cmd_l;
        break;
    case 2:
        *(uint16_t *)buf = (uint16_t)sfp_cmd.cmd_l;
        break;
    case 4:
        *(uint32_t *)buf = sfp_cmd.cmd_l;
        break;
    case 8:
        *(uint64_t *)buf = sfp_cmd.cmd_l | ((uint64_t)sfp_cmd.cmd_h << 32);
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

int32_t sfp_stream_recv(YUNSDR_TRANSPORT *trans, void *buf, uint32_t count, uint8_t channel, uint64_t *timestamp)
{
    int ret;
    SFP_HANDLE *handle = (SFP_HANDLE *)trans->opaque;
    YUNSDR_META *rx_meta = trans->rx_meta;
    YUNSDR_READ_REQ sfp_req;

    sfp_req.head = 0xcafefee0 | (1 << (channel - 1));
    sfp_req.rxlength = count + sizeof(YUNSDR_READ_REQ) / 4;
    sfp_req.rxtime_l = (uint32_t)*timestamp;
    sfp_req.rxtime_h = *timestamp >> 32;

    ret = sendto(handle->cmd_sock, (char *)&sfp_req, sizeof(YUNSDR_READ_REQ), 0,
            (struct sockaddr *)&handle->cmd_addr, sizeof(handle->cmd_addr));
    if (ret < 0) {
        printf("%s failed\n", __func__);
        return -1;
    }
#if defined(__GNUC__)
    socklen_t addr_len = sizeof(struct sockaddr_in);
#else
    int addr_len = sizeof(struct sockaddr_in);
#endif

    char *buf_ptr = (char *)rx_meta;
    uint32_t all_bytes = count * 4 + sizeof(YUNSDR_META);
    uint32_t remain = 0;
    uint32_t nrecv = 0;

    if (wait_for_recv_ready(handle->sockfd[channel - 1], 10)) {
        do {
            remain = all_bytes - nrecv;
retry:		ret = recvfrom(handle->sockfd[channel - 1], buf_ptr, remain, 0, 
                    (struct sockaddr *)&handle->stream_addr[channel - 1], &addr_len);
            if (ret < 0) {
                if (errno == EINTR)
                    goto retry;
#if defined(__WINDOWS_) || defined(_WIN32)
                printf("recv data error %d!\n", GetLastError());
#else
                printf("recv data error %s!\n", strerror(errno));
#endif
                return -ETIMEDOUT;
            }
            nrecv += ret;
            buf_ptr += ret;
        } while (all_bytes != nrecv);
        //nrecv = rx_meta->nsamples * ((rx_meta->head >> 16 & 0x3) == 3 ? 8 : 4);
        *timestamp = (uint64_t)rx_meta->timestamp_h << 32 | rx_meta->timestamp_l;
        memcpy((char *)buf, rx_meta->payload, all_bytes - sizeof(YUNSDR_META));
    }
    else {
        count = -1;
    }
    return count;

}

int32_t sfp_stream_recv2(YUNSDR_TRANSPORT *trans, void *buf, uint32_t count, uint8_t channel, uint64_t *timestamp)
{
    int ret;
    SFP_HANDLE *handle = (SFP_HANDLE *)trans->opaque;
    YUNSDR_READ_REQ sfp_req;

    sfp_req.head = 0xcafefee0 | (1 << (channel - 1));
    sfp_req.rxlength = count + sizeof(YUNSDR_READ_REQ) / 4;
    sfp_req.rxtime_l = (uint32_t)*timestamp;
    sfp_req.rxtime_h = *timestamp >> 32;

    ret = sendto(handle->cmd_sock, (char *)&sfp_req, sizeof(YUNSDR_READ_REQ), 0,
            (struct sockaddr *)&handle->cmd_addr, sizeof(handle->cmd_addr));
    if (ret < 0) {
        printf("%s failed\n", __func__);
        return -1;
    }
#if defined(__GNUC__)
    socklen_t addr_len = sizeof(struct sockaddr_in);
#else
    int addr_len = sizeof(struct sockaddr_in);
#endif

    char *buf_ptr = (char *)buf;
    uint32_t all_bytes = count * 4 + sizeof(YUNSDR_META);
    uint32_t remain = 0;
    uint32_t nrecv = 0;

    if (wait_for_recv_ready(handle->sockfd[channel - 1], 10)) {
        do {
            remain = all_bytes - nrecv;
retry:		ret = recvfrom(handle->sockfd[channel - 1], buf_ptr, remain, 0, 
                    (struct sockaddr *)&handle->stream_addr[channel - 1], &addr_len);
            if (ret < 0) {
                if (errno == EINTR)
                    goto retry;
#if defined(__WINDOWS_) || defined(_WIN32)
                printf("recv data error %d!\n", GetLastError());
#else
                printf("recv data error %s!\n", strerror(errno));
#endif
                return -ETIMEDOUT;
            }
            nrecv += ret;
            buf_ptr += ret;
        } while (all_bytes != nrecv);
        YUNSDR_META *rx_meta = (YUNSDR_META *)buf;
        *timestamp = (uint64_t)rx_meta->timestamp_h << 32 | rx_meta->timestamp_l;
    }
    else {
        count = -1;
    }
    return count;

}

int32_t sfp_stream_send(YUNSDR_TRANSPORT *trans, void *buf, uint32_t count, uint8_t channel, uint64_t timestamp)
{
    int ret;
    SFP_HANDLE *handle = (SFP_HANDLE *)trans->opaque;
    YUNSDR_META *tx_meta = trans->tx_meta;

    tx_meta->timestamp_l = (uint32_t)timestamp;
    tx_meta->timestamp_h = (uint32_t)(timestamp >> 32);
    tx_meta->head = 0xdeadbeef;
    tx_meta->nsamples = count;

    memcpy(tx_meta->payload, buf, count * 4);

    int32_t remain = 0;
    int32_t nbytes = 0;
    int32_t sum = count * 4 + sizeof(YUNSDR_META);
    char *samples = (char *)tx_meta;
    do {
        nbytes = MIN(MAX_TX_BULK_SIZE/4, sum);
        remain = sum - nbytes;
        ret = sendto(handle->sockfd[channel - 1], samples, nbytes, 0,
                (struct sockaddr *)&handle->stream_addr[channel - 1], sizeof(handle->stream_addr[channel - 1]));
        if (ret < 0) {
            printf("%s failed: %s\n", __func__, strerror(errno));
            ret = -EIO;
            return ret;
        }
        samples += nbytes;
        sum -= nbytes;
    } while (remain > 0);

    return count;
}

int32_t sfp_stream_send2(YUNSDR_TRANSPORT *trans, void *buf, uint32_t count, uint8_t channel, uint64_t timestamp, uint32_t flags)
{
    int ret;
    SFP_HANDLE *handle = (SFP_HANDLE *)trans->opaque;
    YUNSDR_META *tx_meta = trans->tx_meta;

    tx_meta->timestamp_l = (uint32_t)timestamp;
    tx_meta->timestamp_h = (uint32_t)(timestamp >> 32);
    if(flags)
        tx_meta->head        = 0xdeadbeee;
    else
        tx_meta->head        = 0xdeadbeef;
    tx_meta->nsamples = count;

    memcpy(tx_meta->payload, buf, count * 4);

    int32_t remain = 0;
    int32_t nbytes = 0;
    int32_t sum = count * 4 + sizeof(YUNSDR_META);
    char *samples = (char *)tx_meta;
    do {
        nbytes = MIN(MAX_TX_BULK_SIZE/4, sum);
        remain = sum - nbytes;
        ret = sendto(handle->sockfd[channel - 1], samples, nbytes, 0,
                (struct sockaddr *)&handle->stream_addr[channel - 1], sizeof(handle->stream_addr[channel - 1]));
        if (ret < 0) {
            printf("%s failed: %s\n", __func__, strerror(errno));
            ret = -EIO;
            return ret;
        }
        samples += nbytes;
        sum -= nbytes;
    } while (remain > 0);

    return count;
}


int32_t init_interface_sfp(YUNSDR_TRANSPORT *trans)
{
    SFP_HANDLE *handle;

    handle = (SFP_HANDLE *)malloc(sizeof(SFP_HANDLE));
    memset(handle, 0, sizeof(*handle));
    char *ip = (char *)trans->app_opaque;

    handle->cmd_sock = init_udp_socket(&handle->cmd_addr, ip, LOCAL_CMD_PORT, REMOTE_CMD_PORT);
    if (handle->cmd_sock < 0) {
        printf("Could not open sfp device.\n");
        return -ENODEV;
    }

    for (int i = 0; i < MAX_RF_STREAM; i++) {
        handle->sockfd[i] = init_udp_socket(&handle->stream_addr[i], ip, local_port[i], remote_port[i]);
        if (handle->sockfd[i] < 0) {
            printf("Could not open sfp device.\n");
            return -ENODEV;
        }
        printf("========>UDP data socket %d\n", handle->sockfd[i]);
#if defined(__WINDOWS_) || defined(_WIN32)
        uint32_t buflen;
        MEMORYSTATUSEX statusex;
        statusex.dwLength = sizeof(statusex);
        if (GlobalMemoryStatusEx(&statusex))
            //buflen = statusex.ullTotalPhys / 50;
            buflen = statusex.ullAvailPhys / 50;
        else
            buflen = 100*1024*1024;
        if (setsockopt(handle->sockfd[i], SOL_SOCKET, SO_RCVBUF, (char *)&buflen, sizeof(uint32_t)) < 0)
            perror("setsockopt:");
        else
            printf("========>UDP socket buffer size: %u Bytes\n", buflen);
#else
        struct sysinfo s_info;
        uint32_t buflen;
        if(sysinfo(&s_info) < 0)
            buflen = 100*1024*1024;
        else
            buflen = s_info.freeram / 50;
        if (setsockopt(handle->sockfd[i], SOL_SOCKET, SO_RCVBUF, &buflen, sizeof(int)) < 0)
            perror("setsockopt:");
        else
            printf("========>UDP socket buffer size: %u Bytes\n", buflen);
#endif

        int handshake[2] = {0x12345678, 0x87654321};
        int ret = sendto(handle->sockfd[i], handshake, sizeof(int) * 2, 0,
                (struct sockaddr *)&handle->stream_addr[i], sizeof(handle->stream_addr[i]));
        if (ret < 0) {
            printf("%s failed: %s\n", __func__, strerror(errno));
        }
    }

    trans->opaque = handle;
    trans->cmd_send = sfp_cmd_send;
    trans->cmd_send_then_recv = sfp_cmd_send_then_recv;
    trans->stream_recv = sfp_stream_recv;
    trans->stream_recv2 = sfp_stream_recv2;
    trans->stream_send = sfp_stream_send;
    trans->stream_send2 = sfp_stream_send2;

    return 0;
}

int32_t deinit_interface_sfp(YUNSDR_TRANSPORT *trans)
{
    SFP_HANDLE *handle = (SFP_HANDLE *)trans->opaque;

#if defined(__WINDOWS_) || defined(_WIN32)
    closesocket(handle->cmd_sock);
    for (int i = 0; i < MAX_RF_STREAM; i++)
        closesocket(handle->sockfd[i]);
#else
    close(handle->cmd_sock);
    for (int i = 0; i < MAX_RF_STREAM; i++)
        close(handle->sockfd[i]);
#endif

    return 0;
}
