
#pragma once

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
    IPPORT server_rtp_port;                                  // client port for UDP based RTP transport
    IPPORT server_rtcp_port;                                 // client port for UDP based RTCP transport
    IPPORT client_rtp_port;
    IPPORT client_rtcp_port;
    transport_mode_t transport_mode;
    uint16_t rtp_channel;                             // only used for rtp over tcp
    uint16_t rtcp_channel;                            // only used for rtp over tcp

    uint8_t RecvBuf[RTSP_BUFFER_SIZE];
    rtsp_method_t method;                             // method of the current request
    uint32_t response_code;
    uint32_t server_method_mask;
    uint32_t CSeq;                                    // RTSP command sequence number
    char url[RTSP_PARAM_STRING_MAX];                // stream url
    uint16_t url_port;                                // port in url
    char url_ip[20];
    parse_state_t parse_state;
    uint8_t state;
    TaskHandle_t task_hdl;
} rtsp_client_t;



rtsp_client_t *rtsp_client_create(const char *url);
int rtsp_client_delete(rtsp_client_t *session);
int rtsp_client_add_media_stream(rtsp_client_t *session, media_stream_t *media);
int rtsp_client_push_media(rtsp_client_t *session, transport_mode_t transport_mode);

#ifdef __cplusplus
}
#endif