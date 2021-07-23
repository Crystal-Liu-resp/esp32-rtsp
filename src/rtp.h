
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

#define  RTP_SEQ_MOD  (1<<16)
#define  RTP_MAX_SDES  255 /* maximum text length for SDES */

typedef enum {
    RTCP_SR   =  200,
    RTCP_RR   =  201,
    RTCP_SDES =  202,
    RTCP_BYE  =  203,
    RTCP_APP  =  204,
} rtcp_type_t;

typedef enum {
    RTCP_SDES_END   = 0,
    RTCP_SDES_CNAME = 1,
    RTCP_SDES_NAME  = 2,
    RTCP_SDES_EMAIL = 3,
    RTCP_SDES_PHONE = 4,
    RTCP_SDES_LOC   = 5,
    RTCP_SDES_TOOL  = 6,
    RTCP_SDES_NOTE  = 7,
    RTCP_SDES_PRIV  = 8,
} rtcp_sdes_type_t;


#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
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
    uint32_t length: 16;    /* pkt len in words, w/o this word */
    uint32_t pt: 8;      /* RTCP packet type */
    uint32_t count: 5;   /* varies by packet type */
    uint32_t p: 1;       /* padding flag */
    uint32_t version: 2; /* protocol version */
} rtcp_common_t;

/*
* Reception report block */
typedef struct {
    uint32_t ssrc;             /* data source being reported */
    int32_t lost: 24;          /* cumul. no. pkts lost (signed!) */
    uint32_t fraction: 8;      /* fraction lost since last SR/RR */
    uint32_t last_seq;         /* extended last seq. no. received */
    uint32_t jitter;           /* interarrival jitter */
    uint32_t lsr;              /* last SR packet from this source */
    uint32_t dlsr;             /* delay since last SR packet */
} rtcp_rr_t;

#else
typedef struct {
    // big-endian
    uint32_t version: 2; /* protocol version */
    uint32_t p: 1;       /* padding flag */
    uint32_t x: 1;       /* header extension flag */
    uint32_t cc: 4;      /* CSRC count */
    uint32_t m: 1;       /* marker bit */
    uint32_t pt: 7;      /* payload type */
    uint32_t seq: 16;    /* sequence number */

    uint32_t ts;      /* timestamp */
    uint32_t ssrc;    /* synchronization source */
} rtp_hdr_t;

typedef struct {
    uint32_t version: 2; /* protocol version */
    uint32_t p: 1;       /* padding flag */
    uint32_t count: 5;   /* varies by packet type */
    uint32_t pt: 8;      /* RTCP packet type */
    uint32_t length: 16;    /* pkt len in words, w/o this word */
} rtcp_common_t;

/*
* Reception report block */
typedef struct {
    uint32_t ssrc;             /* data source being reported */
    unsigned int fraction: 8;  /* fraction lost since last SR/RR */
    int lost: 24;              /* cumul. no. pkts lost (signed!) */
    uint32_t last_seq;         /* extended last seq. no. received */
    uint32_t jitter;           /* interarrival jitter */
    uint32_t lsr;              /* last SR packet from this source */
    uint32_t dlsr;             /* delay since last SR packet */
} rtcp_rr_t;

/*
*  SDES  item */
typedef struct {
    uint8_t type;    /* type of item (rtcp_sdes_type_t) */
    uint8_t length; /* length of item (in octets) */
    char data[1];   /* text, not null-terminated */
} rtcp_sdes_item_t;

#endif

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
    uint8_t recv_buf[256];
    rtp_hdr_t rtphdr;
    int RtpServerPort;
    int RtcpServerPort;
    int RtpSocket;
    int RtcpSocket;
    uint16_t sn;
} rtp_session_t;


#define RTP_HEADER_SIZE        12  // size of the RTP header
#define MAX_RTP_PAYLOAD_SIZE   1420 //1460  1500-20-12-8
#define RTP_VERSION            2
#define RTP_TCP_HEAD_SIZE      4

void mem_swap32(uint8_t *in, uint32_t length);
uint8_t *mem_swap32_copy(uint8_t *out, const uint8_t *in, uint32_t length);

rtp_session_t *rtp_session_create(rtp_session_info_t *session_info);

void rtp_session_delete(rtp_session_t *session);
uint64_t rtp_time_now_us(void);
uint16_t rtp_GetRtpServerPort(rtp_session_t *session);

uint16_t rtp_GetRtcpServerPort(rtp_session_t *session);

void rtp_set_rtp_port(rtp_session_t *session, uint16_t port);

int rtp_send_packet(rtp_session_t *session, rtp_packet_t *packet);

#ifdef __cplusplus
}
#endif
