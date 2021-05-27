#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef int SOCKET;
typedef int UDPSOCKET;
typedef uint32_t IPADDRESS; // On linux use uint32_t in network byte order (per getpeername)
typedef uint16_t IPPORT; // on linux use network byte order

#define NULLSOCKET 0

void closesocket(SOCKET s);

#define GET_RANDOM() rand()

void socketpeeraddr(SOCKET s, IPADDRESS *addr, IPPORT *port);

void udpsocketclose(UDPSOCKET s);

UDPSOCKET udpsocketcreate(unsigned short portNum);

// TCP sending
ssize_t socketsend(SOCKET sockfd, const void *buf, size_t len);
ssize_t udpsocketsend(UDPSOCKET sockfd, const void *buf, size_t len,
                             IPADDRESS destaddr, uint16_t destport);
/**
   Read from a socket with a timeout.

   Return 0=socket was closed by client, -1=timeout, >0 number of bytes read
 */
int socketread(SOCKET sock, char *buf, size_t buflen, int timeoutmsec);

#ifdef __cplusplus
}
#endif
