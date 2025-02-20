#include "app.h"
#include "json.hpp"
#include "STUNExternalIP.h"

#define configPEER_NAME          (char *)"HoChiMinhMachine"
#define configPEER_PORT          18686
#define configSUBSCRIBE_TOPIC    (char *)"HCM_SUBSCRIPTION"
#define configPUBLISH_TOPIC      (char *)"HN_SUBSCRIPTION"

static bool bLoop = true;

static PeerCandidate ourPeer;
static PeerCandidate anotherPeer;

static void sigProc(int sig) {
    bLoop = false;
    printf("[CAUGHT] Signal %d\n", sig);
    exit(EXIT_SUCCESS);
}

static void onSignalingMessage(struct mosquitto *mosq, void *arg, const struct mosquitto_message *message) {
    nlohmann::json JSON = nlohmann::json::parse((char *)message->payload);

    try {
        int port = JSON["Port"].get<int>();
        int state = JSON["State"].get<int>();
        std::string name = JSON["Name"].get<std::string>();
        std::string pubIp = JSON["Public"].get<std::string>();
        std::string prvIp = JSON["Private"].get<std::string>();

        if (anotherPeer.state == STATE_WAITING) {
            strcpy(anotherPeer.name, name.c_str());
            strcpy(anotherPeer.pubIp, pubIp.c_str());
            strcpy(anotherPeer.priIp, prvIp.c_str());
            anotherPeer.port = port;
            anotherPeer.state = STATE_GATHERING;

            printf("[CREATE] Peer connection %s [%s:%d]\n", anotherPeer.name, anotherPeer.pubIp, anotherPeer.port);
        }
    }
    catch(const std::exception& e) {
        printf("%s\n", e.what());
    }
    
}

static void * alwaysPublishMessage(void *arg) {
    const nlohmann::json JSON = {
        {"Port"     , ourPeer.port                  },
        {"Name"     , std::string(ourPeer.name)     },
        {"Public"   , std::string(ourPeer.pubIp)    },
        {"Private"  , std::string(ourPeer.priIp)    },
        {"State"    , ourPeer.state                 }
    };
    struct mosquitto *mosq = (struct mosquitto *)arg;
    
    while (bLoop) {
        mosquitto_publish(mosq, NULL, configPUBLISH_TOPIC, JSON.dump().length(), JSON.dump().c_str(), 0, 0);
        sleep(1);
    }

    pthread_detach(pthread_self());
    return NULL;
}

static int getPrivateIPAddress(char buffer[]) {
    struct ifaddrs *interfaceaddr, *ifaddress;
    char ip[INET_ADDRSTRLEN];

    if (getifaddrs(&interfaceaddr) == -1) {
        return -1;
    }

    for (ifaddress = interfaceaddr; ifaddress != NULL; ifaddress = ifaddress->ifa_next) {
        if (ifaddress->ifa_addr && ifaddress->ifa_addr->sa_family == AF_INET) { // Only IPv4
            struct sockaddr_in *addr = (struct sockaddr_in *)ifaddress->ifa_addr;
            inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);

            if (strcmp(ip, "127.0.0.1") != 0) {
                strcpy(buffer, ip);
            }
        }
    }
    freeifaddrs(interfaceaddr);
    return 0;
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

    /* -------------------------// CONNECT TO SIGNALING SERVER \\------------------------- */
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
    

    /* -------------------------// CREATE UDP SOCKET \\------------------------- */
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

    const struct STUNServer STUN = { 
        configSTUNSERVER, 
        configSTUNPORT
    };
    
    int ret = getPublicIPAddress(STUN, ourPeer.pubIp);
    if (ret != 0) {
        printf("Can't get IP Public address from %s:%d\n", configSTUNSERVER, configSTUNPORT);
        exit(EXIT_FAILURE);
    }
    getPrivateIPAddress(ourPeer.priIp);

    printf("---- PEER ENTRY ----\n");
    printf("NAME       : %s\n", ourPeer.name);
    printf("PORT       : %d\n", ourPeer.port);
    printf("PUBLIC IP  : %s\n", ourPeer.pubIp);
    printf("PRIVATE IP : %s\n", ourPeer.priIp);
    printf("--------------------\n\n");

    char buffer[512];
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
            static int counter = 0;
            
            memset(buffer, 0, sizeof(buffer));
            anotherPeerAddress.sin_family = AF_INET;
            anotherPeerAddress.sin_port = htons(anotherPeer.port);
            printf("\e[3m [STATE_GATHERING] CONNECTING %s [%s:%d] \e[0m\n", anotherPeer.name, anotherPeer.pubIp, anotherPeer.port);
            
            /* -- Both Peers is behind different NAT -- */
            if (strcmp(ourPeer.pubIp, anotherPeer.pubIp) != 0) {
                anotherPeerAddress.sin_addr.s_addr = inet_addr(anotherPeer.pubIp);
                printf("Both %s [%s:%d] and %s [%s:%d] also behind different NAT\n", ourPeer.name, ourPeer.pubIp, ourPeer.port, anotherPeer.name, anotherPeer.pubIp, anotherPeer.port);
            }
            /* -- Both Peers is behind the same NAT -- */
            else {
                /* -- Both Peers is running on the same machine -- */
                if (strcmp(ourPeer.priIp, anotherPeer.priIp) == 0) {
                    printf("Both %s [%s:%d] and %s [%s:%d] is running on the same machine with the same NAT\n", ourPeer.name, ourPeer.pubIp, ourPeer.port, anotherPeer.name, anotherPeer.pubIp, anotherPeer.port);
                    anotherPeerAddress.sin_addr.s_addr = inet_addr("127.0.0.1"); /* LOOPBACK address */
                }
                /* -- Both Peers is running on different machine -- */
                else {
                    printf("Both %s [%s:%d] and %s [%s:%d] is running on different machine with the same NAT\n", ourPeer.name, ourPeer.pubIp, ourPeer.port, anotherPeer.name, anotherPeer.pubIp, anotherPeer.port);
                    anotherPeerAddress.sin_addr.s_addr = inet_addr(anotherPeer.priIp);
                }
            }

            sendto(fd, configPUNCHING_CMD, strlen(configPUNCHING_CMD), 0, (struct sockaddr *)&anotherPeerAddress, anotherPeerAddressLen);

            int ret = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&anotherPeerAddress, &anotherPeerAddressLen);
            if (ret > 0) {
                if (strcmp(buffer, configPUNCHING_CMD) == 0) {
                    anotherPeer.state = STATE_CONNECTED;
                }
            }
            else (++counter);

            printf("\e[3m [STATE_GATHERING] UDP HOLE PUNCHING %s [%d] \e[0m\n", (anotherPeer.state == STATE_CONNECTED) ? "COMPLETED" : "FAILURE", counter);
        }
        break;

        case STATE_CONNECTED: {
            memset(buffer, 0, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "[CONNECTED] Message from %s [%s:%d]", ourPeer.name, ourPeer.pubIp, ourPeer.port);
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
