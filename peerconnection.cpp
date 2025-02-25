#include "peerconnection.h"

PeerConnection::PeerConnection() {
    createSocket();
    fState = INITITAL;
}

PeerConnection::~PeerConnection() {
    close(fFd);
}

GATHERING_STATE PeerConnection::gatheringState() {
    return fState;
}

void PeerConnection::selectedCaindidatePair(Candidate * remoteCandiate) {
    addrRemote.sin_family = AF_INET;
    addrRemote.sin_port = htons(remoteCandiate->fPort);

    /**
     * Both Peers are behind different NAT, so we need to make the router 
     * mapped local port into public port
     */
    if (fCandidate.fPublicIP != remoteCandiate->fPublicIP) {
        addrRemote.sin_addr.s_addr = inet_addr(remoteCandiate->fPublicIP.c_str());
        printf("Both Peers are behind different NAT\r\n");
    }
    /* -- Both Peers are behind the same NAT -- */
    else {
        /* -- Both Peers are running on the same machine -- */
        if (fCandidate.fPrivateIP == remoteCandiate->fPrivateIP) {
            addrRemote.sin_addr.s_addr = inet_addr("127.0.0.1");
            printf("Both Peers are behind the same NAT and the same machine\r\n");
        }
        /* -- Both Peers are running on different machine -- */
        else {
            addrRemote.sin_addr.s_addr = inet_addr(remoteCandiate->fPrivateIP.c_str());
            printf("Both Peers are behind the same NAT and different machine\r\n");
        }
    }
    fState = INPROGRESS;

    pthread_t createId;
    pthread_create(&createId, NULL, PeerConnection::iteratePunchingHole, this);
}

Candidate PeerConnection::gatherLocalCandidates() {
    char address[32];

    /* -- STUN Protocol to get IP Public -- */
    memset(address, 0, sizeof(address));
    getPublicIPAddress(ourSTUN, address);
    fCandidate.fPublicIP = std::string(address);

    /* -- Get IP Local -- */
    memset(address, 0, sizeof(address));
    GetPrivateIPAddress(address);
    fCandidate.fPrivateIP = std::string(address);

    fCandidate.fPort = ntohs(addr.sin_port);

    return fCandidate;
}

int PeerConnection::sendTo(uint8_t *data, uint32_t dataLen) {
    socklen_t addrRemoteLen = sizeof(addrRemote);
    return sendto(fFd, data, dataLen, 0, (struct sockaddr *)&addrRemote, addrRemoteLen);
}

int PeerConnection::readFrom(uint8_t *data, uint32_t dataLen) {
    socklen_t addrRemoteLen = sizeof(addrRemote);
    return recvfrom(fFd, data, dataLen, 0, (struct sockaddr *)&addrRemote, &addrRemoteLen);
}

int PeerConnection::createSocket() {
    fFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fFd < 0) {
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0; /* Let OS (Operating system) assign a port */

    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    setsockopt(fFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (bind(fFd, (const struct sockaddr *)&addr, (socklen_t)sizeof(addr))) {
        return -1;
    }
    socklen_t addrLen = sizeof(addr);
    getsockname(fFd, (struct sockaddr*)&addr, &addrLen);

    return 0;
}

void * PeerConnection::iteratePunchingHole(void *arg) {
    char chars[32];
    PeerConnection *pc = (PeerConnection *)arg;
    socklen_t addrRemoteLen = sizeof(addrRemote);
    const char *PUNCHING_COMMAND = "UDP_HOLE_PUNCHING";

    while (pc->gatheringState() != COMPLTED) {
        pc->sendTo((uint8_t*)PUNCHING_COMMAND, strlen(PUNCHING_COMMAND));

        memset(chars, 0, sizeof(chars));
        if (pc->readFrom((uint8_t*)chars, sizeof(chars)) > 0) {
            if (strcmp(chars, PUNCHING_COMMAND) == 0) {
                pc->fState = COMPLTED;
                break;
            }
        }
    }

    pthread_detach(pthread_self());
    return NULL;
}