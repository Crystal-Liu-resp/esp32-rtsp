
#pragma once

#include "platglue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RTP_OVER_UDP,
    RTP_OVER_TCP,
    RTP_OVER_MULTICAST,
} transport_mode_t;

typedef struct {
    // little-endian
    uint32_t seq : 16;    /* sequence number */
    uint32_t pt : 7;      /* payload type */
    uint32_t m : 1;       /* marker bit */
    uint32_t cc : 4;      /* CSRC count */
    uint32_t x : 1;       /* header extension flag */
    uint32_t p : 1;       /* padding flag */
    uint32_t version : 2; /* protocol version */

    uint32_t ts;      /* timestamp */
    uint32_t ssrc;    /* synchronization source */
} rtp_hdr_t;

typedef struct {
    uint8_t *data;
    uint32_t size;
    uint32_t timestamp;
    uint8_t  type;
    uint8_t is_last;
} rtp_packet_t;

typedef struct {
    transport_mode_t transport_mode;
    SOCKET socket_tcp;// tcp
    uint16_t rtp_port;// udp
    uint16_t rtsp_channel; //channel for rtsp over tcp

} rtp_session_info_t;

typedef struct {
    rtp_session_info_t session_info;
    rtp_hdr_t rtphdr;
    int RtpServerPort;
    int RtcpServerPort;
    int RtpSocket;
    int RtcpSocket;
    uint16_t sn;
}rtp_session_t;


#define RTP_HEADER_SIZE        12  // size of the RTP header
#define MAX_RTP_PAYLOAD_SIZE   1420 //1460  1500-20-12-8
#define RTP_VERSION            2
#define RTP_TCP_HEAD_SIZE      4


rtp_session_t* rtp_session_create(rtp_session_info_t *session_info);

void rtp_session_delete(rtp_session_t *session);

uint16_t rtp_GetRtpServerPort(rtp_session_t *session);

uint16_t rtp_GetRtcpServerPort(rtp_session_t *session);

void rtp_set_rtp_port(rtp_session_t *session, uint16_t port);

int rtp_send_packet(rtp_session_t *session, rtp_packet_t *packet);

#ifdef __cplusplus
}
#endif
