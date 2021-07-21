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



typedef int32_t esp_err_t;

/* Definitions for error constants. */
#define ESP_OK          0       /*!< esp_err_t value indicating success (no error) */
#define ESP_FAIL        -1      /*!< Generic esp_err_t code indicating failure */

#define ESP_ERR_NO_MEM              0x101   /*!< Out of memory */
#define ESP_ERR_INVALID_ARG         0x102   /*!< Invalid argument */
#define ESP_ERR_INVALID_STATE       0x103   /*!< Invalid state */
#define ESP_ERR_INVALID_SIZE        0x104   /*!< Invalid size */
#define ESP_ERR_NOT_FOUND           0x105   /*!< Requested resource not found */
#define ESP_ERR_NOT_SUPPORTED       0x106   /*!< Operation or feature not supported */
#define ESP_ERR_TIMEOUT             0x107   /*!< Operation timed out */
#define ESP_ERR_INVALID_RESPONSE    0x108   /*!< Received response was invalid */

#define CONFIG_LOG_COLORS 1
#if CONFIG_LOG_COLORS
#define LOG_COLOR_BLACK   "30"
#define LOG_COLOR_RED     "31"
#define LOG_COLOR_GREEN   "32"
#define LOG_COLOR_BROWN   "33"
#define LOG_COLOR_BLUE    "34"
#define LOG_COLOR_PURPLE  "35"
#define LOG_COLOR_CYAN    "36"
#define LOG_COLOR(COLOR)  "\033[0;" COLOR "m"
#define LOG_BOLD(COLOR)   "\033[1;" COLOR "m"
#define LOG_RESET_COLOR   "\033[0m"
#define LOG_COLOR_E       LOG_COLOR(LOG_COLOR_RED)
#define LOG_COLOR_W       LOG_COLOR(LOG_COLOR_BROWN)
#define LOG_COLOR_I       LOG_COLOR(LOG_COLOR_GREEN)
#define LOG_COLOR_D
#define LOG_COLOR_V
#else //CONFIG_LOG_COLORS
#define LOG_COLOR_E
#define LOG_COLOR_W
#define LOG_COLOR_I
#define LOG_COLOR_D
#define LOG_COLOR_V
#define LOG_RESET_COLOR
#endif //CONFIG_LOG_COLORS

#define LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " (%d) %s: " format LOG_RESET_COLOR "\n"

/// macro to output logs in startup code, before heap allocator and syscalls have been initialized. log at ``ESP_LOG_ERROR`` level. @see ``printf``,``ESP_LOGE``
#define ESP_EARLY_LOGE( tag, format, ... ) ESP_LOG_EARLY_IMPL(tag, format,    E, ##__VA_ARGS__)
/// macro to output logs in startup code at ``ESP_LOG_WARN`` level.  @see ``ESP_EARLY_LOGE``,``ESP_LOGE``, ``printf``
#define ESP_EARLY_LOGW( tag, format, ... ) ESP_LOG_EARLY_IMPL(tag, format,     W, ##__VA_ARGS__)
/// macro to output logs in startup code at ``ESP_LOG_INFO`` level.  @see ``ESP_EARLY_LOGE``,``ESP_LOGE``, ``printf``
#define ESP_EARLY_LOGI( tag, format, ... ) ESP_LOG_EARLY_IMPL(tag, format,     I, ##__VA_ARGS__)
/// macro to output logs in startup code at ``ESP_LOG_DEBUG`` level.  @see ``ESP_EARLY_LOGE``,``ESP_LOGE``, ``printf``
#define ESP_EARLY_LOGD( tag, format, ... ) ESP_LOG_EARLY_IMPL(tag, format,    D, ##__VA_ARGS__)
/// macro to output logs in startup code at ``ESP_LOG_VERBOSE`` level.  @see ``ESP_EARLY_LOGE``,``ESP_LOGE``, ``printf``
#define ESP_EARLY_LOGV( tag, format, ... ) ESP_LOG_EARLY_IMPL(tag, format,  V, ##__VA_ARGS__)


#define ESP_LOG_EARLY_IMPL(tag, format, log_tag_letter, ...) do {                         \
        if (1) {                                                          \
            printf(LOG_FORMAT(log_tag_letter, format), 1u, tag, ##__VA_ARGS__); \
        }} while(0)

#define ESP_LOGE( tag, format, ... )  ESP_EARLY_LOGE(tag, format, ##__VA_ARGS__)
/// macro to output logs at ``ESP_LOG_WARN`` level.  @see ``ESP_LOGE``
#define ESP_LOGW( tag, format, ... )  ESP_EARLY_LOGW(tag, format, ##__VA_ARGS__)
/// macro to output logs at ``ESP_LOG_INFO`` level.  @see ``ESP_LOGE``
#define ESP_LOGI( tag, format, ... )  ESP_EARLY_LOGI(tag, format, ##__VA_ARGS__)
/// macro to output logs at ``ESP_LOG_DEBUG`` level.  @see ``ESP_LOGE``
#define ESP_LOGD( tag, format, ... )  ESP_EARLY_LOGD(tag, format, ##__VA_ARGS__)
/// macro to output logs at ``ESP_LOG_VERBOSE`` level.  @see ``ESP_LOGE``
#define ESP_LOGV( tag, format, ... )  ESP_EARLY_LOGV(tag, format, ##__VA_ARGS__)


#define portTICK_PERIOD_MS 1
#define vTaskDelay(t) udelay(1000*(t))



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

uint8_t *mem_swap32_copy(uint8_t *out, const uint8_t *in, uint32_t length);

#ifdef __cplusplus
}
#endif
