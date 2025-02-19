#include "app.h"
#include "json.hpp"
#include "STUNExternalIP.h"

#define configPEER_NAME          (char *)"Peer0"
#define configPEER_PORT          50001
#define configSUBSCRIBE_TOPIC    (char *)"CreatePeerConnection0"
#define configPUBLISH_TOPIC      (char *)"CreatePeerConnection1"

static bool bLoop = true;

static PeerConnection ourPeer;
static PeerConnection anotherPeer;

static void sigProc(int sig) {
    bLoop = false;
    printf("Caught signal %d\n", sig);
}

static void onSignalingMessage(struct mosquitto *mosq, void *arg, const struct mosquitto_message *message) {
    printf("Received message: %s\n", (char *)message->payload);

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
        }
    }
    catch(const std::exception& e) {
        printf("%s\n", e.what());
    }
    
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
    if (fd) {
        printf("Can't create UDP socket\n");
        exit(EXIT_FAILURE);
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(configPEER_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (const struct sockaddr *)&addr, (socklen_t)sizeof(addr))) {
        printf("Can't bind UDP socket on port %d\n", configPEER_PORT);
        exit(EXIT_FAILURE);
    }

    const struct STUNServer STUN= { 
        configSTUNSERVER, 
        configSTUNPORT
    };
    
    int ret = getPublicIPAddress(STUN, ourPeer.publicIP, configPEER_PORT);
    if (ret == 0) {
        printf("Can't get IP Public address from %s:%d\n", configSTUNSERVER, configSTUNPORT);
        exit(EXIT_FAILURE);
    }

    printf("-- PEER CONNECTION --\n");
    printf("Name             : %s\n", ourPeer.name);
    printf("Port             : %d\n", ourPeer.port);
    printf("Public IP address: %s\n", ourPeer.publicIP);

    struct sockaddr_in anotherPeerAddress;
    socklen_t anotherPeerAddressLen = sizeof(anotherPeerAddress);
    memset(&anotherPeerAddress, 0, sizeof(anotherPeerAddress));

    while (bLoop) {
        switch (anotherPeer.state) {
        case STATE_WAITING: {
            const nlohmann::json JSON = {
                {"port" , ourPeer.port                  },
                {"name" , std::string(ourPeer.name)     },
                {"ip"   , std::string(ourPeer.publicIP) },
                {"state", ourPeer.state                 }
            };
            mosquitto_publish(mosq, NULL, configPUBLISH_TOPIC, JSON.dump().length(), JSON.dump().c_str(), 0, 0);
            sleep(1);
        }
        break;
    
        case STATE_GATHERING: {
            char buffer[512];
            const char *CMD = (const char *)"UDP_HOLE_PUNCHING";

            memset(buffer, 0, sizeof(buffer));
            anotherPeerAddress.sin_family = AF_INET;
            anotherPeerAddress.sin_addr.s_addr = inet_addr(anotherPeer.publicIP);
            anotherPeerAddress.sin_port = htons(anotherPeer.port);
            sendto(fd, CMD, strlen(CMD), 0, (struct sockaddr *)&anotherPeerAddress, anotherPeerAddressLen);
            int ret = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&anotherPeerAddress, &anotherPeerAddressLen);
            if (ret > 0) {
                if (strcmp(buffer, CMD) == 0) {
                    anotherPeer.state = STATE_CONNECTED;
                    printf("UDP Hole Punching is completed\r\n");
                }
            }
            else {
                printf("UDP Hole Punching is trying ...\r\n");
                sleep(1);
            }

        }
        break;

        case STATE_CONNECTED: {
            // char buffer[512];
            // memset(buffer, 0, sizeof(buffer));
            // recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&anotherPeerAddress, &anotherPeerAddressLen);
            // printf("Message from %s (%s:%d): %s\n", anotherPeer.name, anotherPeer.publicIP, anotherPeer.port, buffer);

            printf("STATE_CONNECTED\r\n");
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
