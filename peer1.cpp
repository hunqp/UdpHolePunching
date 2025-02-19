#include "app.h"
#include "json.hpp"
#include "STUNExternalIP.h"

#define configPEER_NAME          (char *)"HaNoi"
#define configPEER_PORT          18632
#define configSUBSCRIBE_TOPIC    (char *)"CREATE_PEER_CONNECTION_HN"
#define configPUBLISH_TOPIC      (char *)"CREATE_PEER_CONNECTION_HCM"

static bool bLoop = true;

static PeerConnection ourPeer;
static PeerConnection anotherPeer;

static void sigProc(int sig) {
    bLoop = false;
    printf("[CAUGHT] Signal %d\n", sig);
    exit(EXIT_SUCCESS);
}

static void onSignalingMessage(struct mosquitto *mosq, void *arg, const struct mosquitto_message *message) {
    nlohmann::json JSON = nlohmann::json::parse((char *)message->payload);

    try {
        int port = JSON["port"].get<int>();
        int state = JSON["state"].get<int>();
        std::string ip = JSON["ip"].get<std::string>();
        std::string name = JSON["name"].get<std::string>();

        if (anotherPeer.state == STATE_WAITING) {
            strcpy(anotherPeer.name, name.c_str());
            strcpy(anotherPeer.publicIP, ip.c_str());
            anotherPeer.port = port;
            anotherPeer.state = STATE_GATHERING;

            printf("[CREATE] Peer connection %s [%s:%d]\n", anotherPeer.name, anotherPeer.publicIP, anotherPeer.port);
        }
    }
    catch(const std::exception& e) {
        printf("%s\n", e.what());
    }
    
}

static void * alwaysPublishMessage(void *arg) {
    const nlohmann::json JSON = {
        {"port" , ourPeer.port                  },
        {"name" , std::string(ourPeer.name)     },
        {"ip"   , std::string(ourPeer.publicIP) },
        {"state", ourPeer.state                 }
    };
    struct mosquitto *mosq = (struct mosquitto *)arg;
    
    while (bLoop) {
        mosquitto_publish(mosq, NULL, configPUBLISH_TOPIC, JSON.dump().length(), JSON.dump().c_str(), 0, 0);
        sleep(1);
    }

    pthread_detach(pthread_self());
    return NULL;
}

int main() {
    signal(SIGINT, 	sigProc);
	signal(SIGQUIT, sigProc);

    memset(&ourPeer, 0, sizeof(ourPeer));
    memset(&anotherPeer, 0, sizeof(anotherPeer));
    ourPeer.port = configPEER_PORT;
    ourPeer.state = STATE_WAITING;
    strncpy(ourPeer.name, configPEER_NAME, sizeof(ourPeer.name));

    anotherPeer.state = STATE_WAITING;

    ////////////////////////////// CONNECT TO SIGNALING SERVER //////////////////////////////
    struct mosquitto *mosq = NULL;

    mosq = mosquitto_new(NULL, true, NULL);
    if (!mosq) {
        exit(EXIT_FAILURE);
    }
    mosquitto_message_callback_set(mosq, onSignalingMessage);
    int rc = mosquitto_connect(mosq, configSIGNALINGSERVER, configSIGNALINGPORT, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        printf("Can't connect to %s:%d\n", configSIGNALINGSERVER, configSIGNALINGPORT);
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        exit(EXIT_FAILURE);
    }
    mosquitto_subscribe(mosq, NULL, configSUBSCRIBE_TOPIC, 0);
    mosquitto_loop_start(mosq);
    printf("Connected to broker at %s:%d\n", configSIGNALINGSERVER, configSIGNALINGPORT);
    
    ////////////////////////////// CREATE UDP SOCKET //////////////////////////////
    int fd = -1;
    socklen_t len;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        printf("Can't create UDP socket\n");
        exit(EXIT_FAILURE);
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(configPEER_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    struct timeval timeoutIn5Secs;
    timeoutIn5Secs.tv_sec = 5;
    timeoutIn5Secs.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeoutIn5Secs, sizeof(timeoutIn5Secs));

    if (bind(fd, (const struct sockaddr *)&addr, (socklen_t)sizeof(addr))) {
        printf("Can't bind UDP socket on port %d\n", ourPeer.port);
        exit(EXIT_FAILURE);
    }

    const struct STUNServer STUN= { 
        configSTUNSERVER, 
        configSTUNPORT
    };
    
    int ret = getPublicIPAddress(STUN, ourPeer.publicIP);
    if (ret != 0) {
        printf("Can't get IP Public address from %s:%d\n", configSTUNSERVER, configSTUNPORT);
        exit(EXIT_FAILURE);
    }

    printf("-- PEER ENTRY --\n");
    printf("Name             : %s\n", ourPeer.name);
    printf("Port             : %d\n", ourPeer.port);
    printf("Public IP address: %s\n", ourPeer.publicIP);

    struct sockaddr_in anotherPeerAddress;
    socklen_t anotherPeerAddressLen = sizeof(anotherPeerAddress);
    memset(&anotherPeerAddress, 0, sizeof(anotherPeerAddress));

    pthread_t createId;
    pthread_create(&createId, NULL, alwaysPublishMessage, mosq);

    while (bLoop) {
        switch (anotherPeer.state) {
        case STATE_WAITING: {
            printf("[STATE_WAITING] Wait for another peer\n");
        }
        break;
    
        case STATE_GATHERING: {
            char buffer[512];
            static int counter = 0;

            printf("[STATE_GATHERING] CONNECTING %s [%s:%d]\n", anotherPeer.name, anotherPeer.publicIP, anotherPeer.port);

            memset(buffer, 0, sizeof(buffer));
            anotherPeerAddress.sin_family = AF_INET;
            anotherPeerAddress.sin_addr.s_addr = inet_addr(anotherPeer.publicIP);
            anotherPeerAddress.sin_port = htons(anotherPeer.port);
            sendto(fd, configCOMMON_CMD, strlen(configCOMMON_CMD), 0, (struct sockaddr *)&anotherPeerAddress, anotherPeerAddressLen);

            int ret = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&anotherPeerAddress, &anotherPeerAddressLen);
            if (ret > 0) {
                if (strcmp(buffer, configCOMMON_CMD) == 0) {
                    anotherPeer.state = STATE_CONNECTED;
                }
            }
            else (++counter);

            printf("[STATE_GATHERING] UDP HOLE PUNCHING %s [%d]\n", (anotherPeer.state = STATE_CONNECTED) ? "COMPLETED" : "FAILURE", counter);
        }
        break;

        case STATE_CONNECTED: {
            char buffer[512];
            memset(buffer, 0, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "Message from %s [%s:%d]\n", ourPeer.name, ourPeer.publicIP, ourPeer.port);
            sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *)&anotherPeerAddress, anotherPeerAddressLen);

            memset(buffer, 0, sizeof(buffer));
            int ret = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&anotherPeerAddress, &anotherPeerAddressLen);
            if (ret > 0) {
                printf("%s\n", buffer);
            }
            else perror("recvfrom()");
        }   
        break;
        
        default:
        break;
        }

        sleep(1);
    }

    mosquitto_loop_stop(mosq, false);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    close(fd);

    return 0;
}
