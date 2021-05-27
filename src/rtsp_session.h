#pragma once

#include "media_stream.h"


#ifdef __cplusplus
extern "C" {
#endif

#define RTSP_BUFFER_SIZE       4096    // for incoming requests, and outgoing responses
#define RTSP_PARAM_STRING_MAX  128
#define MAX_HOSTNAME_LEN       128


// supported command types
typedef enum {
    RTSP_OPTIONS,
    RTSP_DESCRIBE,
    RTSP_SETUP,
    RTSP_PLAY,
    RTSP_TEARDOWN,
    RTSP_GET_PARAMETER,
    RTSP_UNKNOWN
} RTSP_CMD_TYPES;

typedef enum  {
    ParseState_RequestLine,
    ParseState_HeadersLine,
    ParseState_GotAll,
} RtspRequestParseState;


typedef struct {
    media_stream_t *media_stream[2];
    uint8_t media_stream_num;

    rtp_session_t *rtp_session[2];
    uint8_t rtp_session_num;

    int m_RtspSessionID;
    SOCKET MasterSocket;                                      // our masterSocket(socket that listens for RTSP client connections)
    SOCKET m_RtspClient;                                      // RTSP socket of that session
    int m_StreamID;                                           // number of simulated stream of that session
    IPPORT m_ClientRTPPort;                                  // client port for UDP based RTP transport
    IPPORT m_ClientRTCPPort;                                 // client port for UDP based RTCP transport
    transport_mode_t transport_mode;

    uint8_t RecvBuf[RTSP_BUFFER_SIZE];
    RTSP_CMD_TYPES m_RtspCmdType;                             // command type (if any) of the current request
    char m_url[RTSP_PARAM_STRING_MAX];                        // stream url
    uint32_t m_CSeq;                                          // RTSP command sequence number
    char resource_url[RTSP_PARAM_STRING_MAX];
    char m_HostPort[MAX_HOSTNAME_LEN];                        // host:port part of the URL
    char m_ip[20];
    RtspRequestParseState state_;

    uint8_t state;
} rtsp_session_t;


rtsp_session_t *rtsp_session_create(const char *url, uint16_t port);

int rtsp_session_delete(rtsp_session_t *session);

int rtsp_session_accept(rtsp_session_t *session);

int rtsp_session_terminate(rtsp_session_t *session);

int rtsp_session_add_media_stream(rtsp_session_t *session);

int rtsp_handle_requests(rtsp_session_t *session, uint32_t readTimeoutMs);

#ifdef __cplusplus
}
#endif
