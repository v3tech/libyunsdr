#pragma once
#ifndef __INTERFACE_SFP__
#define __INTERFACE_SFP__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#if defined(__WINDOWS_) || defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#include <WS2tcpip.h>
#if(_WIN32_WINNT < 0x0502)
#define MSG_WAITALL  0
#endif
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define SOCKET int
#define SOCKADDR_IN struct sockaddr_in
#define INVALID_SOCKET (SOCKET)(~0)
#endif
#include "transport.h"

#define LOCAL_CMD_PORT       5001
#define REMOTE_CMD_PORT      1024

#define LOCAL_STREAM1_PORT   5002
#define REMOTE_STREAM1_PORT  1025

#define LOCAL_STREAM2_PORT   5003
#define REMOTE_STREAM2_PORT  1026

#define LOCAL_STREAM3_PORT   5004
#define REMOTE_STREAM3_PORT  1027

#define LOCAL_STREAM4_PORT   5005
#define REMOTE_STREAM4_PORT  1028


typedef struct interface_handle_sfp {
    SOCKET sockfd[MAX_RF_STREAM];
    SOCKADDR_IN stream_addr[MAX_RF_STREAM];
    SOCKET cmd_sock;
    SOCKADDR_IN cmd_addr;
}SFP_HANDLE;


int32_t init_interface_sfp(YUNSDR_TRANSPORT *trans);
int32_t deinit_interface_sfp(YUNSDR_TRANSPORT *trans);

#endif
