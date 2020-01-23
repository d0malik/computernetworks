#ifndef _NETWORKHELPERS
#define _NETWORKHELPERS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <netdb.h>

void lookup_host (const char *host, char *serv_IP);

#endif