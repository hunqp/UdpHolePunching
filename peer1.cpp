#include "main.h"
#include "json.hpp"
#include "STUNExternalIP.h"

#define configPEER_NAME          (char *)"HaNoiMachine"
#define configPEER_PORT          18687
#define configSUBSCRIBE_TOPIC    (char *)"HN_SUBSCRIPTION"
#define configPUBLISH_TOPIC      (char *)"HCM_SUBSCRIPTION"

static bool bLoop = true;

static PeerCandidate ourPeer;
static PeerCandidate anotherPeer;

const struct STUNServer ourSTUN = { 
    configSTUNSERVER, 
    configSTUNPORT
};

static void sigProc(int sig) {
    bLoop = false;
    LOGP("[CAUGHT] Signal %d\n", sig);
    exit(EXIT_SUCCESS);
}

static void onSignalingMessage(struct mosquitto *mosq, void *arg, const struct mosquitto_message *message) {
    nlohmann::json JSON = nlohmann::json::parse((char *)message->payload);

    try {
        int port = JSON["Port"].get<int>();
        int state = JSON["State"].get<int>();
        std::string name = JSON["Name"].get<std::string>();
        std::string PubIP = JSON["PubIP"].get<std::string>();
        std::string prvIp = JSON["PriIP"].get<std::string>();

        if (anotherPeer.state == STATE_WAITING) {
            strcpy(anotherPeer.name, name.c_str());
            strcpy(anotherPeer.PubIP, PubIP.c_str());
            strcpy(anotherPeer.PriIP, prvIp.c_str());
            anotherPeer.port = port;
            anotherPeer.state = STATE_GATHERING;

            LOGP("[SIGNALING] Peer connection %s [%s:%d]\n", anotherPeer.name, anotherPeer.PubIP, anotherPeer.port);
        }
    }
    catch(const std::exception& e) {
        LOGE("%s\n", e.what());
    }
}

static void * alwaysPublishMessage(void *arg) {
    const nlohmann::json JSON = {
        {"Port"     , ourPeer.port                  },
        {"Name"     , std::string(ourPeer.name)     },
        {"PubIP"    , std::string(ourPeer.PubIP)    },
        {"PriIP"    , std::string(ourPeer.PriIP)    },
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

    /* ===========================// PREPARE PEER CANDIDATE \\=========================== */
    memset(&ourPeer, 0, sizeof(ourPeer));
    memset(&anotherPeer, 0, sizeof(anotherPeer));
    ourPeer.port = configPEER_PORT;
    ourPeer.state = STATE_WAITING;
    strncpy(ourPeer.name, configPEER_NAME, sizeof(ourPeer.name));

    /**
     * NOTE: This is an the most important stage for UDP Hole Punching between 
     * two peer are located behind the different NAT.
     * Because STUN Protocol will give you a pair IP_PUBLIC and PORT_PUBLIC that 
     * your peer asks to STUN SERVER and those pair IP_PUBLIC and PORT_PUBLIC
     * will be keep in a limit time depends router configuration.
     */
    if (getPublicIPAddress(ourSTUN, ourPeer.PubIP) != 0) {
        LOGE("Can't get IP Public address from %s:%d\n", configSTUNSERVER, configSTUNPORT);
        exit(EXIT_FAILURE);
    }
    getPrivateIPAddress(ourPeer.PriIP);

    LOGP("---- PEER CANDIDATE ----\n");
    LOGP("NAME       : %s\n", ourPeer.name);
    LOGP("LOCAL PORT : %d\n", ourPeer.port);
    LOGP("PUBLIC IP  : %s\n", ourPeer.PubIP);
    LOGP("PRIVATE IP : %s\n", ourPeer.PriIP);
    LOGP("------------------------\n\n");

    /* ===========================// CREATE UDP SOCKET LOCAL \\=========================== */
    int fd = -1;
    socklen_t len;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        LOGE("Can't create UDP socket\n");
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
        LOGE("Can't bind UDP socket on port %d\n", ourPeer.port);
        exit(EXIT_FAILURE);
    }

    char buffer[512];
    struct sockaddr_in anotherPeerAddress;
    socklen_t anotherPeerAddressLen = sizeof(anotherPeerAddress);
    memset(&anotherPeerAddress, 0, sizeof(anotherPeerAddress));

    /* ===========================// CONNECT TO SIGNALING SERVER \\=========================== */
    struct mosquitto *mosq = NULL;

    mosq = mosquitto_new(NULL, true, NULL);
    if (!mosq) {
        exit(EXIT_FAILURE);
    }
    mosquitto_message_callback_set(mosq, onSignalingMessage);
    int rc = mosquitto_connect(mosq, configSIGNALINGSERVER, configSIGNALINGPORT, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOGP("Can't connect to %s:%d\n", configSIGNALINGSERVER, configSIGNALINGPORT);
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        exit(EXIT_FAILURE);
    }
    mosquitto_subscribe(mosq, NULL, configSUBSCRIBE_TOPIC, 0);
    mosquitto_loop_start(mosq);
    LOGP("[CONNECTED] Broker at %s:%d\n", configSIGNALINGSERVER, configSIGNALINGPORT);

    /* SIGNALIG PROCESS: Create a thread to always publish your peer candidate to another */
    pthread_t createId;
    pthread_create(&createId, NULL, alwaysPublishMessage, mosq);

    while (bLoop) {
        switch (anotherPeer.state) {
        case STATE_WAITING: {
            LOGD("[STATE_WAITING] Waiting for another peer\n");
        }
        break;
    
        case STATE_GATHERING: {
            static int counter = 0;
            
            memset(buffer, 0, sizeof(buffer));
            anotherPeerAddress.sin_family = AF_INET;
            anotherPeerAddress.sin_port = htons(anotherPeer.port);
            LOGP("[STATE_GATHERING] CONNECTING %s [%s:%d]\n", anotherPeer.name, anotherPeer.PubIP, anotherPeer.port);
            
            /**
             * Both Peers are behind different NAT, so we need to make the router mapped local port into public port
             */
            if (strcmp(ourPeer.PubIP, anotherPeer.PubIP) != 0) {
                anotherPeerAddress.sin_addr.s_addr = inet_addr(anotherPeer.PubIP);
                LOGP("Both %s [%s:%d] and %s [%s:%d] also behind different NAT\n", ourPeer.name, ourPeer.PubIP, ourPeer.port, anotherPeer.name, anotherPeer.PubIP, anotherPeer.port);
            }
            /* -- Both Peers are behind the same NAT -- */
            else {
                /* -- Both Peers are running on the same machine -- */
                if (strcmp(ourPeer.PriIP, anotherPeer.PriIP) == 0) {
                    LOGP("Both %s [%s:%d] and %s [%s:%d] are running on the same machine with the same NAT\n", ourPeer.name, ourPeer.PubIP, ourPeer.port, anotherPeer.name, anotherPeer.PubIP, anotherPeer.port);
                    anotherPeerAddress.sin_addr.s_addr = inet_addr("127.0.0.1"); /* Loopback address */
                }
                /* -- Both Peers are running on different machine -- */
                else {
                    LOGP("Both %s [%s:%d] and %s [%s:%d] are running on different machine with the same NAT\n", ourPeer.name, ourPeer.PubIP, ourPeer.port, anotherPeer.name, anotherPeer.PubIP, anotherPeer.port);
                    anotherPeerAddress.sin_addr.s_addr = inet_addr(anotherPeer.PriIP);
                }
            }

            /* Send message for puching between two peers */
            sendto(fd, configPUNCHING_CMD, strlen(configPUNCHING_CMD), 0, (struct sockaddr *)&anotherPeerAddress, anotherPeerAddressLen);

            int ret = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&anotherPeerAddress, &anotherPeerAddressLen);
            if (ret > 0) {
                if (strcmp(buffer, configPUNCHING_CMD) == 0) {
                    anotherPeer.state = STATE_CONNECTED;
                }
            }
            else (++counter);

            LOGD("[STATE_GATHERING] UDP HOLE PUNCHING %s [AFTER TRY %d]\n", (anotherPeer.state == STATE_CONNECTED) ? "COMPLETED" : "FAILURE", counter);
        }
        break;

        case STATE_CONNECTED: {
            memset(buffer, 0, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "\x1B[32m [PONG] Message from %s [%s:%d] \x1B[0m", ourPeer.name, ourPeer.PubIP, ourPeer.port);
            sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *)&anotherPeerAddress, anotherPeerAddressLen);

            memset(buffer, 0, sizeof(buffer));
            int ret = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&anotherPeerAddress, &anotherPeerAddressLen);
            if (ret > 0) {
                LOGP("%s\n", buffer);
            }
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
