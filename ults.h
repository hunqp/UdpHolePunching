#ifndef ULTS_H
#define ULTS_H

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "STUNExternalIP.h"

extern int GetPrivateIPAddress(char buffer[]);

#endif /* ULTS_H */