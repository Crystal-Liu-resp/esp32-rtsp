#pragma once

#include <sys/queue.h>
#include "media_stream.h"
#include "rtsp_utility.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RTSP_BUFFER_SIZE       4096    // for incoming requests, and outgoing responses
#define RTSP_PARAM_STRING_MAX  128



typedef struct media_streams_t {
    media_stream_t *media_stream;
    uint32_t trackid;
    /* Next endpoint entry in the singly linked list */
    SLIST_ENTRY(media_streams_t) next;
} media_streams_t;

typedef struct {
    SLIST_HEAD(media_streams_list_t, media_streams_t) media_list;
    uint8_t media_stream_num;

    char session_id[32];
    SOCKET MasterSocket;                                      // our masterSocket(socket that listens for RTSP client connections)
    SOCKET client_socket;                                      // RTSP socket of that session
    IPPORT m_ClientRTPPort;                                  // client port for UDP based RTP transport
    IPPORT m_ClientRTCPPort;                                 // client port for UDP based RTCP transport
    transport_mode_t transport_mode;
    uint16_t rtp_channel;                             // only used for rtp over tcp
    uint16_t rtcp_channel;                            // only used for rtp over tcp

    uint8_t RecvBuf[RTSP_BUFFER_SIZE];
    rtsp_method_t method;                             // method of the current request
    uint32_t CSeq;                                    // RTSP command sequence number
    char url[RTSP_PARAM_STRING_MAX];                // stream url
    uint16_t url_port;                                // port in url
    char url_ip[20];
    char url_suffix[RTSP_PARAM_STRING_MAX];
    char resource_url[RTSP_PARAM_STRING_MAX];         // registered url
    parse_state_t parse_state;
    uint8_t state;
} rtsp_session_t;


rtsp_session_t *rtsp_session_create(const char *url, uint16_t port);

int rtsp_session_delete(rtsp_session_t *session);

int rtsp_session_accept(rtsp_session_t *session);

int rtsp_session_terminate(rtsp_session_t *session);

int rtsp_session_add_media_stream(rtsp_session_t *session, media_stream_t *media);

int rtsp_handle_requests(rtsp_session_t *session, uint32_t readTimeoutMs);

#ifdef __cplusplus
}
#endif
