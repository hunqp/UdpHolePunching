// Server side implementation of UDP client-server model
#include <stdio.h>
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
#include <json-c/json.h>
#include "common.h"

#define PORT 20315
#define MAXLINE 1024

#include <pthread.h>

static int run = -1;
static int quitto_publishent_mid = -1;

int sent_mid = -1;

static int clientenable = 0;
static char clientip[30];
static int clientport = -1;
struct mosquitto *mosq = NULL;
char jsoncmd[MAXLINE];

int on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg)
{
    printf("on_message %s\n", (const char *)msg->payload);
    char *s = msg->payload;
    enum json_tokener_error error;
    json_object *obj = json_tokener_parse_verbose(s, &error);

    if (error != json_tokener_success)
    {
        printf("Parse error. \n");
        return;
    }

    json_object *value;
    bool isFound = json_object_object_get_ex(obj, "ip", &value);
    if (isFound)
    {
        printf("ip: %s \n", json_object_get_string(value));
        strcpy(clientip, json_object_get_string(value));
    }

    json_object *valueport;
    isFound = json_object_object_get_ex(obj, "port", &valueport);
    if (isFound)
    {
        printf("port: %d \n", json_object_get_int(valueport));
        clientport = json_object_get_int(valueport);
        clientenable = 1;
    }

    run = 0;
    return 0;
}

mqttloop()
{
    int rc;
    printf("run mqttloop\n");
    while (true)
    {
        rc = mosquitto_loop(mosq, -1, 1);
        if (rc)
        {
            // mosquitto_publish(mosq, &sent_mid, SERVERTOPIC , strlen(jsoncmd), jsoncmd, 0, 0 );
            sleep(1);
        }
    }
}

struct STUNServer
{
    char *address;

    unsigned short port;
};

// RFC 5389 Section 6 STUN Message Structure
struct STUNMessageHeader
{
    // Message Type (Binding Request / Response)
    unsigned short type;

    // Payload length of this message
    unsigned short length;

    // Magic Cookie
    unsigned int cookie;

    // Unique Transaction ID
    unsigned int identifier[3];
};

#define XOR_MAPPED_ADDRESS_TYPE 0x0020
#define STUN_MAPPED_ADDRESS_TYPE 0x0001

// RFC 5389 Section 15 STUN Attributes
struct STUNAttributeHeader
{
    // Attibute Type
    unsigned short type;

    // Payload length of this attribute
    unsigned short length;
};

#define IPv4_ADDRESS_FAMILY 0x01;
#define IPv6_ADDRESS_FAMILY 0x02;

//  RFC 5389 Section 15.1 MAPPED-ADDRESS
struct STUNMappedIPv4Address
{
    unsigned char reserved;

    unsigned char family;

    unsigned short port;

    unsigned int address;
};

// RFC 5389 Section 15.2 XOR-MAPPED-ADDRESS
struct STUNXORMappedIPv4Address
{
    unsigned char reserved;

    unsigned char family;

    unsigned short port;

    unsigned int address;
};

struct STUNServer servers[] = {
    {STUNSERVER, STUNPORT},
    {"stun.l.google.com", 19302},
    {"stun.l.google.com", 19305},
    {"stun1.l.google.com", 19302},
    {"stun1.l.google.com", 19305},
    {"stun2.l.google.com", 19302},
    {"stun2.l.google.com", 19305},
    {"stun3.l.google.com", 19302},
    {"stun3.l.google.com", 19305},
    {"stun4.l.google.com", 19302},
    {"stun4.l.google.com", 19305},
};

// Driver code
int main()
{
    int sockfd;
    int rc;
    char recvbuffer[MAXLINE];
    char cmd[MAXLINE];
    memset(&recvbuffer, 0, MAXLINE);
    char *hello = "Hello from client";
    struct sockaddr_in servaddr, cliaddr, cliaddrtmp;

    mosq = mosquitto_new("stunclient", true, NULL);

    mosquitto_lib_init();

    pthread_t ploop;

    rc = mosquitto_connect(mosq, BROKERSERVER, BROKERPORT, 300); // 5s timeout

    pthread_create(&ploop, NULL, mqttloop, NULL);

    mosquitto_subscribe(mosq, NULL, SERVERTOPIC, 0);

    mosquitto_message_callback_set(mosq, on_message);

    printf("wait 1s, then start request\n");

    if (rc)
    {
        printf("mosquitto_connect Error: %s\n", mosquitto_strerror(rc));
        return -1;
    }

    sleep(1);

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Ficlientserver information
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&servaddr,
             sizeof(servaddr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Remote Address
    struct addrinfo *results = NULL;
    struct addrinfo *hints = malloc(sizeof(struct addrinfo));
    bzero(hints, sizeof(struct addrinfo));

    hints->ai_family = AF_INET;
    hints->ai_socktype = SOCK_STREAM;
    struct STUNServer server = servers[0];
    if (getaddrinfo(server.address, NULL, hints, &results) != 0)
    {
        perror("getaddrinfo get failed");
        free(hints);
        close(sockfd);
        return -2;
    }

    struct in_addr stunaddr;
    if (results != NULL)
    {
        stunaddr = ((struct sockaddr_in *)results->ai_addr)->sin_addr;
    }
    else
    {
        perror("getaddrinfo result is null");
        free(hints);
        freeaddrinfo(results);
        close(sockfd);
        return -2;
    }

    // Create the remote address
    struct sockaddr_in *remoteAddress = malloc(sizeof(struct sockaddr_in));

    bzero(remoteAddress, sizeof(struct sockaddr_in));

    remoteAddress->sin_family = AF_INET;

    remoteAddress->sin_addr = stunaddr;

    remoteAddress->sin_port = htons(server.port);

    // Construct a STUN request
    struct STUNMessageHeader *request = malloc(sizeof(struct STUNMessageHeader));

    request->type = htons(0x0001);

    request->length = htons(0x0000);

    request->cookie = htonl(0x2112A442);

#if 0
    request->identifier[0] = 560020616;
    request->identifier[1] = 560020617;
    request->identifier[2] = 560020618;
#else
    for (int index = 0; index < 3; index++)
    {
        srand((unsigned int)time(0));

        request->identifier[index] = rand();
        printf("index = %d %u\n", index, request->identifier);
    }
#endif

    if (sendto(sockfd, request, sizeof(struct STUNMessageHeader), 0, (struct sockaddr *)remoteAddress, sizeof(struct sockaddr_in)) == -1)
    {
        printf("request STUNMessageHeader error\n");
        free(hints);
        freeaddrinfo(results);
        free(remoteAddress);
        free(request);
        close(sockfd);
        return -3;
    }

    // Set the timeout
    struct timeval tv = {5, 0};

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));

    // Read the response
    char *buffer = malloc(sizeof(char) * 512);

    bzero(buffer, 512);

    long length = read(sockfd, buffer, 512);

    if (length < 0)
    {

        free(hints);

        freeaddrinfo(results);

        free(remoteAddress);

        free(request);

        free(buffer);

        close(sockfd);

        return -4;
    }

    char *pointer = buffer;

    struct STUNMessageHeader *response = (struct STUNMessageHeader *)buffer;

    if (response->type == htons(0x0101))
    {
        // Check the identifer
        for (int index = 0; index < 3; index++)
        {
            if (request->identifier[index] != response->identifier[index])
            {
                return -1;
            }
        }

        pointer += sizeof(struct STUNMessageHeader);

        while (pointer < buffer + length)
        {
            struct STUNAttributeHeader *header = (struct STUNAttributeHeader *)pointer;

            if (header->type == htons(XOR_MAPPED_ADDRESS_TYPE))
            {
                printf("XOR_MAPPED_ADDRESS_TYPE\n");
                pointer += sizeof(struct STUNAttributeHeader);

                struct STUNXORMappedIPv4Address *xorAddress = (struct STUNXORMappedIPv4Address *)pointer;

                unsigned int numAddress = htonl(xorAddress->address) ^ 0x2112A442;
                unsigned int xnumPort = xorAddress->port;
                unsigned int numPort = ((xnumPort >> 8) & 0xff) | ((xnumPort << 8) & 0xff00);
                numPort = numPort ^ 0x2112;

                // Parse the IP address
                printf("xor %d.%d.%d.%d : %d \n",
                       (numAddress >> 24) & 0xFF,
                       (numAddress >> 16) & 0xFF,
                       (numAddress >> 8) & 0xFF,
                       numAddress & 0xFF, numPort);
                snprintf(cmd, sizeof(cmd), "./udpclient  %d.%d.%d.%d %d 2> /tmp/udpclient.log &", (numAddress >> 24) & 0xFF,
                         (numAddress >> 16) & 0xFF,
                         (numAddress >> 8) & 0xFF,
                         numAddress & 0xFF, numPort);

                snprintf(jsoncmd, sizeof(cmd), "{ \"ip\":\"%d.%d.%d.%d\",\"port\":%d } \n", (numAddress >> 24) & 0xFF,
                         (numAddress >> 16) & 0xFF,
                         (numAddress >> 8) & 0xFF,
                         numAddress & 0xFF, numPort);

                printf(jsoncmd);
                mosquitto_publish(mosq, &sent_mid, CLIENTTOPIC, strlen(jsoncmd), jsoncmd, 0, 0);
                break;
            }

            pointer += (sizeof(struct STUNAttributeHeader) + ntohs(header->length));
        }
    }

    while (1)
    {
        if (clientenable)
        {
            printf("get peer %s %d\n", clientip, clientport);
            break;
        }
        mosquitto_publish(mosq, &sent_mid, CLIENTTOPIC, strlen(jsoncmd), jsoncmd, 0, 0);
        printf("wait peer...\n");
        sleep(2);
    }

    printf("[CLIENT-SIDE] We catched the peer %s %d\n", clientip, clientport);

    cliaddr.sin_family = AF_INET;
    cliaddr.sin_addr.s_addr = inet_addr(clientip);
    cliaddr.sin_port = htons(clientport);

    while (1)
    {
        int len, n;

        len = sizeof(cliaddr); // len is value/resuslt

        sleep(2);
        sendto(sockfd, (const char *)hello, strlen(hello),
               MSG_CONFIRM, (const struct sockaddr *)&cliaddr,
               len);
        //       printf("Hello message sent.\n");
        memset(&recvbuffer, 0, MAXLINE);

        n = recvfrom(sockfd, (char *)recvbuffer, MAXLINE,
                     MSG_WAITALL, (struct sockaddr *)&cliaddr,
                     &len);
        if (n < 1)
        {
            perror("recvfrom");
            mosquitto_publish(mosq, &sent_mid, CLIENTTOPIC, strlen(jsoncmd), jsoncmd, 0, 0);
        }
        buffer[n] = '\0';
        printf("[%d] Server : %s\n", n, recvbuffer);
    }

    free(hints);

    freeaddrinfo(results);

    free(remoteAddress);

    free(request);

    free(buffer);

    close(sockfd);

    return 0;
}


Hi Eason Qiu, because I don't known exactly which value for I-Frame is the best, I have the 
refer from the document AnyCloud Platform SDK User Development Manual_V1.3.4 in section 7.1.1 (page 27), Anyka has recommended:
 - Setting 2MP:
 

can you give me the document about 