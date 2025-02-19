#ifndef _APP_H_
#define _APP_H_

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <netdb.h>

#include <mosquitto.h>
#include <mosquittopp.h>

#define configSIGNALINGSERVER   (char*)"broker.emqx.io"
#define configSIGNALINGPORT     1883

#define configSTUNSERVER        (char*)"stun.l.google.com"
#define configSTUNPORT          19302

enum {
    STATE_WAITING,
    STATE_GATHERING,
    STATE_CONNECTED,
};

typedef struct {
    int port;
    int state;
    char publicIP[16];
    char name[32];
} PeerConnection;

#endif /* _APP_H_ */