#pragma once

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <unistd.h>

#include <stdlib.h>
#include <errno.h>

typedef int WiFiClient;
typedef int SOCKET;
typedef int UDPSOCKET;
typedef uint32_t IPADDRESS; // On linux use uint32_t in network byte order (per getpeername)
typedef uint16_t IPPORT; // on linux use network byte order

#define NULLSOCKET 0

// inline void closesocket(SOCKET s) {
//     printf("closing TCP socket\n");

//     close(s);
// }

#define GET_RANDOM() (esp_random())

inline void socketpeeraddr(SOCKET s, IPADDRESS *addr, IPPORT *port) {

    struct sockaddr_in r;
    socklen_t len = sizeof(r);
    if(getpeername(s,(struct sockaddr*)&r,&len) < 0) {
        printf("getpeername failed\n");
        *addr = 0;
        *port = 0;
    }
    else {
        //htons

        *port  = r.sin_port;
        *addr = r.sin_addr.s_addr;
    }
}

inline void udpsocketclose(UDPSOCKET s) {
    printf("closing UDP socket\n");
    close(s);
}


inline UDPSOCKET udpsocketcreate(unsigned short portNum)
{
    struct sockaddr_in addr;
    int sockfd;
    // int opt = 1;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        ESP_LOGW("glue esp32", "%s error, error code: %d, reason: %s", "udp server create", sockfd, strerror(sockfd));
        return ESP_FAIL;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(portNum);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGW("glue esp32", "%s error, error code: %d, reason: %s", "udp server bind", sockfd, strerror(sockfd));
        return ESP_FAIL;
    }

    return sockfd;
}

// TCP sending
inline ssize_t socketsend(SOCKET sockfd, const void *buf, size_t len)
{
    // printf("TCP send\n");
    return send(sockfd, buf, len, 0);
}

inline ssize_t udpsocketsend(UDPSOCKET sockfd, const void *buf, size_t len,
                             IPADDRESS destaddr, IPPORT destport)
{
    struct sockaddr_in addr;

    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = destaddr;
    addr.sin_port = htons(destport);

    return sendto(sockfd, buf, len, 0, (sockaddr *) &addr, sizeof(addr));
}

/**
   Read from a socket with a timeout.

   Return 0=socket was closed by client, -1=timeout, >0 number of bytes read
 */
inline int socketread(SOCKET sock, char *buf, size_t buflen, int timeoutmsec)
{
    // Use a timeout on our socket read to instead serve frames
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeoutmsec * 1000; // send a new frame ever
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    int res = recv(sock,buf,buflen,0);
    if(res > 0) {
        return res;
    }
    else if(res == 0) {
        return 0; // client dropped connection
    }
    else {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return -1;
        else
            return 0; // unknown error, just claim client dropped it
    };
}


inline static uint8_t *mem_swap32_copy(uint8_t *out, const uint8_t *in, uint32_t length)
{
    if (length % 4) {
        ESP_LOGE("glue-esp32", "length incorrect");
        return out;
    }
    for (size_t i = 0; i < length; i += 4) {
        out[i] = in[i + 3];
        out[i + 1] = in[i + 2];
        out[i + 2] = in[i + 1];
        out[i + 3] = in[i];
    }
    return out + length;
}

inline static void mem_swap32(uint8_t *in, uint32_t length)
{
    if (length % 4) {
        ESP_LOGE("glue-esp32", "length incorrect");
        return;
    }
    uint8_t m, n;
    for (size_t i = 0; i < length; i += 4) {
        m = in[i];
        n = in[i + 1];
        in[i] = in[i + 3];
        in[i + 1] = in[i + 2];
        in[i + 2] = n;
        in[i + 3] = m;
    }
}