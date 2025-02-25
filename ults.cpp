#include "ults.h"


int GetPrivateIPAddress(char buffer[]) {
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
