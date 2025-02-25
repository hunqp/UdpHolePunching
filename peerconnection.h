#ifndef PEER_CONNECTION_H
#define PEER_CONNECTION_H

#include "main.h"
#include "ults.h"

typedef enum {
    INITITAL,
    INPROGRESS,
    COMPLTED,
} GATHERING_STATE;

typedef struct {
    int fPort;
    std::string fPublicIP;
    std::string fPrivateIP;
} Candidate;

class PeerConnection {
public:
    PeerConnection();
    ~PeerConnection();

    GATHERING_STATE gatheringState();
    Candidate gatherLocalCandidates();
    void selectedCaindidatePair(Candidate * remoteCandiate);

    int sendTo(uint8_t *data, uint32_t dataLen);
    int readFrom(uint8_t *data, uint32_t dataLen);

private:
    int createSocket();
    static void * iteratePunchingHole(void *);

private:
    int fFd = -1;
    socklen_t fLen;
    struct sockaddr_in addr;
    struct sockaddr_in addrRemote;

    Candidate fCandidate;
    GATHERING_STATE fState;
};

#endif /* PEER_CONNECTION_H */