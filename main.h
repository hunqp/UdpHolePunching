#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <bits/stdc++.h>

#include <mosquitto.h>
#include <mosquittopp.h>

#define configSIGNALINGSERVER       (char*)"broker.emqx.io"
#define configSIGNALINGPORT         1883

#define configSTUNSERVER            (char*)"stun.l.google.com"
#define configSTUNPORT              19302

#define configSIGNALING_TOPIC_REQ   "SIGNALING_REQ"
#define configSIGNALING_TOPIC_RES   "SIGNALING_RES"

typedef struct {
    int port;
    int state;
    std::string uuid;
    std::string PriIP;
    std::string PubIP;
} PeerCandidate;

#define LOGP(fmt, ...)  printf(fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...)  printf("\x1B[34m""\e[3m" fmt "\x1B[0m", ##__VA_ARGS__)
#define LOGE(fmt, ...)  printf("\x1B[31m" fmt "\x1B[0m", ##__VA_ARGS__)

extern const struct STUNServer ourSTUN;


#endif /* MAIN_H */