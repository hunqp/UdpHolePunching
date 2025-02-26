#ifndef _APP_H_
#define _APP_H_

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <mosquitto.h>
#include <mosquittopp.h>

#define configSIGNALINGSERVER   (char*)"broker.emqx.io"
#define configSIGNALINGPORT     1883

#define configSTUNSERVER        (char*)"stun.l.google.com"
#define configSTUNPORT          19302

#define configPUNCHING_CMD      (char*)"UDP_HOLE_PUNCHING"

enum {
    STATE_WAITING,
    STATE_GATHERING,
    STATE_CONNECTED,
};

typedef struct {
    int port;
    int state;
    char PriIP[16];
    char PubIP[16];
    char name[32];
} PeerCandidate;

#define LOGP(fmt, ...)  printf(fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...)  printf("\x1B[34m""\e[3m" fmt "\x1B[0m", ##__VA_ARGS__)
#define LOGE(fmt, ...)  printf("\x1B[31m" fmt "\x1B[0m", ##__VA_ARGS__)

#endif /* _APP_H_ */