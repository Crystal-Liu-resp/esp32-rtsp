
#include <stdio.h>
#include <time.h>
#include "esp_log.h"
#include "rtsp_session.h"
#include "media_mjpeg.h"


static const char *TAG = "rtsp";

#define RTSP_SESSION_CHECK(a, str, ret_val)                       \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }


static char *FindFirstCrlf(const char *str)
{
    while (*str) {
        if (str[0] == '\r' && str[1] == '\n') {
            return (char *)str;
        }
        str++;
    }
    return NULL;
}

static bool ParseRequestLine(rtsp_session_t *session, const char *message)
{
    char method[32] = {0};
    char url[128] = {0};
    char version[32] = {0};

    if (sscanf(message, "%s %s %s", method, url, version) != 3) {
        return true;
    }
    char *url_end = &url[strlen(url) - 1];
    if (*url_end == '/') {
        *url_end = '\0';
    }

    if (strstr(method, "OPTIONS")) {
        session->m_RtspCmdType = RTSP_OPTIONS;
    } else if (strstr(method, "DESCRIBE")) {
        session->m_RtspCmdType = RTSP_DESCRIBE;
    } else if (strstr(method, "SETUP")) {
        session->m_RtspCmdType = RTSP_SETUP;
    } else if (strstr(method, "PLAY")) {
        session->m_RtspCmdType = RTSP_PLAY;
    } else if (strstr(method, "TEARDOWN")) {
        session->m_RtspCmdType = RTSP_TEARDOWN;
    } else if (strstr(method, "GET_PARAMETER")) {
        session->m_RtspCmdType = RTSP_GET_PARAMETER;
    } else {
        session->m_RtspCmdType = RTSP_UNKNOWN;
        return true;
    }

    if (strncmp(url, "rtsp://", 7) != 0) {
        return true;
    }

    // parse url
    if (sscanf(url + 7, "%[^:]:%hu/%s", session->m_ip, &session->port, session->url_suffix) == 3) {

    } else if (sscanf(url + 7, "%[^/]/%s", session->m_ip, session->url_suffix) == 2) {
        session->port = 554;
    } else {
        return true;
    }
    strcpy(session->m_url, url);
    ESP_LOGD(TAG, "url:%s", session->m_url);
    ESP_LOGD(TAG, "url_suffix:%s", session->url_suffix);
    return 0;
}

static bool ParseHeadersLine(rtsp_session_t *session, const char *message)
{
    ESP_LOGD(TAG, "<%s>", message);
    char *TmpPtr = NULL;
    TmpPtr = (char *)strstr(message, "CSeq: ");
    if (TmpPtr) {
        session->m_CSeq  = atoi(TmpPtr + 6);
        return 0;
    }

    if (session->m_RtspCmdType == RTSP_DESCRIBE || session->m_RtspCmdType == RTSP_SETUP || session->m_RtspCmdType == RTSP_PLAY) {
        // ParseAuthorization(message);
    }

    if (session->m_RtspCmdType == RTSP_OPTIONS) {
        session->state_ = ParseState_GotAll;
        return 0;
    }

    if (session->m_RtspCmdType == RTSP_DESCRIBE) {
        session->state_ = ParseState_GotAll;
        return 0;
    }

    if (session->m_RtspCmdType == RTSP_SETUP) {
        TmpPtr = (char *)strstr(message, "Transport");
        if (TmpPtr) { // parse transport header
            TmpPtr = (char *)strstr(TmpPtr, "RTP/AVP/TCP");
            if (TmpPtr) {
                session->transport_mode = RTP_OVER_TCP;
            } else {
                session->transport_mode = RTP_OVER_UDP;
            }

            TmpPtr = (char *)strstr(message, "multicast");
            if (TmpPtr) {
                session->transport_mode = RTP_OVER_MULTICAST;
                ESP_LOGD(TAG, "multicast");
            } else {
                ESP_LOGD(TAG, "unicast");
            }

            char *ClientPortPtr = NULL;
            if (RTP_OVER_UDP == session->transport_mode) {
                ClientPortPtr = (char *)strstr(message, "client_port=");
            } else if (RTP_OVER_MULTICAST == session->transport_mode) {
                ClientPortPtr = (char *)strstr(message, "port=");
            }
            if (ClientPortPtr) {
                ClientPortPtr += (RTP_OVER_UDP == session->transport_mode) ? 12 : 5;
                char cp[16] = {0};
                char *p = strchr(ClientPortPtr, '-');
                if (p) {
                    strncpy(cp, ClientPortPtr, p - ClientPortPtr);
                    session->m_ClientRTPPort  = atoi(cp);
                    session->m_ClientRTCPPort = session->m_ClientRTPPort + 1;
                    ESP_LOGI(TAG, "rtsp client port %d-%d", session->m_ClientRTPPort, session->m_ClientRTCPPort);
                } else {
                    return 1;
                }
            }

            if (RTP_OVER_TCP == session->transport_mode) {
                TmpPtr = (char *)strstr(message, "interleaved=");
                if (TmpPtr) {
                    int RTP_channel, RTCP_channel;
                    if (sscanf(TmpPtr += 12, "%d-%d", &RTP_channel, &RTCP_channel) == 2) {
                        ESP_LOGI(TAG, "RTP channel=%d, RTCP channel=%d", RTP_channel, RTCP_channel);
                    }
                }
            }

            session->state_ = ParseState_GotAll;
        }
        return 0;
    }

    if (session->m_RtspCmdType == RTSP_PLAY) {
        session->state_ = ParseState_GotAll;
        return 0;
    }

    if (session->m_RtspCmdType == RTSP_TEARDOWN) {
        session->state_ = ParseState_GotAll;
        return 0;
    }

    if (session->m_RtspCmdType == RTSP_GET_PARAMETER) {
        session->state_ = ParseState_GotAll;
        return 0;
    }

    return 1;
}

static bool ParseRtspRequest(rtsp_session_t *session, const char *aRequest, uint32_t aRequestSize)
{
    printf("[%s]\n", aRequest);
    if (aRequestSize < 5) {
        printf("[%x, %x, %x, %x, %x]\n", aRequest[0], aRequest[1], aRequest[2], aRequest[3], aRequest[4]);
    }

    if (aRequest[0] == '$') {
        session->m_RtspCmdType   = RTSP_UNKNOWN;
        return true;
    }

    session->m_RtspCmdType   = RTSP_UNKNOWN;
    session->m_CSeq = 0;
    memset(session->m_url, 0x00, sizeof(session->m_url));
    session->state_ = ParseState_RequestLine;

    bool ret = 0;
    char *string = (char *)aRequest;
    char const *end = string + aRequestSize;
    while (string < end) {
        switch (session->state_) {
        case ParseState_RequestLine: {
            char *firstCrlf = FindFirstCrlf((const char *)string);
            if (firstCrlf != nullptr) {
                firstCrlf[0] = '\0';
                ret = ParseRequestLine(session, string);
                string = firstCrlf + 2;
            }

            if (0 == ret) {
                session->state_ = ParseState_HeadersLine;
            } else {
                ESP_LOGE(TAG, "rtsp request parse failed");
                string = (char *)end;
            }
        } break;

        case ParseState_HeadersLine: {
            char *firstCrlf = FindFirstCrlf((const char *)string);
            if (firstCrlf != nullptr) {
                firstCrlf[0] = '\0';
                ret = ParseHeadersLine(session, string);
                string = firstCrlf + 2;
            } else {
                string = (char *)end;
            }
        } break;

        case ParseState_GotAll: {
            string = (char *)end;
        } break;

        default:
            return true;
            break;
        }
    }

    return true;
}

static char const *DateHeader(char *buf, uint32_t length)
{
    time_t tt = time(NULL);
    strftime(buf, length, "Date: %a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
    return buf;
}

static void Handle_RtspOPTION(rtsp_session_t *session, char *Response, uint32_t *length)
{
    char time_str[64];
    session->m_StreamID = -1;        // invalid URL
    if (0 == strcmp(session->url_suffix, session->resource_url)) {
        session->m_StreamID = 0;
    }

    if (session->m_StreamID == -1) {
        ESP_LOGE(TAG, "[%s] Stream Not Found", session->m_url);
        // Stream not available
        int len = snprintf(Response, *length,
                           "RTSP/1.0 404 Not Found\r\nCSeq: %u\r\n%s\r\n",
                           session->m_CSeq,
                           DateHeader(time_str, sizeof(time_str)));
        if (len > 0) {
            *length = len;
        }
        return;
    }

    int len = snprintf(Response, *length,
                       "RTSP/1.0 200 OK\r\nCSeq: %u\r\n"
                       "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n\r\n", session->m_CSeq);
    if (len > 0) {
        *length = len;
    }
}

static void GetSdpMessage(rtsp_session_t *session, char *buf, uint32_t buf_len, const char *session_name)
{
    snprintf(buf, buf_len,
             "v=0\r\n"
             "o=- 9%u 1 IN IP4 %s\r\n" //o=<username> <session id> <version> <network type> <address type> <address>
             "t=0 0\r\n"
             "a=control:*\r\n",
             GET_RANDOM(), session->m_ip);

    if (session_name) {
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "s=%s\r\n", session_name);
    } else {
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "s=Unnamed\r\n");
    }

    if (RTP_OVER_MULTICAST == session->transport_mode) {
        snprintf(buf + strlen(buf), buf_len - strlen(buf),
                 "a=type:broadcast\r\n"
                 "a=rtcp-unicast: reflection\r\n");
    }

    char str_buf[128];
    media_streams_t *it;
    SLIST_FOREACH(it, &session->media_list, next) {
        if (RTP_OVER_MULTICAST == session->transport_mode) {
            it->media_stream->get_description(str_buf, sizeof(str_buf), 0);
            snprintf(buf + strlen(buf), buf_len - strlen(buf),
                     "%s\r\n", str_buf);

            snprintf(buf + strlen(buf), buf_len - strlen(buf),
                     "c=IN IP4 %s/255\r\n", "0.0.0.0"/*multicast_ip_.c_str()*/);
        } else {
            it->media_stream->get_description(str_buf, sizeof(str_buf), 0);
            snprintf(buf + strlen(buf), buf_len - strlen(buf),
                     "%s\r\n", str_buf);
        }

        it->media_stream->get_attribute(str_buf, sizeof(str_buf));
        snprintf(buf + strlen(buf), buf_len - strlen(buf),
                 "%s\r\n", str_buf);
        snprintf(buf + strlen(buf), buf_len - strlen(buf),
                 "a=control:trackID=%d\r\n", it->trackid);
    }
}

static void Handle_RtspDESCRIBE(rtsp_session_t *session, char *Response, uint32_t *length)
{
    char time_str[64];
    char SDPBuf[256];
    GetSdpMessage(session, SDPBuf, sizeof(SDPBuf), NULL);
    int len = snprintf(Response, *length,
                       "RTSP/1.0 200 OK\r\nCSeq: %u\r\n"
                       "%s\r\n"
                       "Content-Base: %s/\r\n"
                       "Content-Type: application/sdp\r\n"
                       "Content-Length: %d\r\n\r\n"
                       "%s",
                       session->m_CSeq,
                       DateHeader(time_str, sizeof(time_str)),
                       session->m_url,
                       (int) strlen(SDPBuf),
                       SDPBuf);

    if (len > 0) {
        *length = len;
    }
}

static void Handle_RtspSETUP(rtsp_session_t *session, char *Response, uint32_t *length)
{
    int32_t trackID = 0;
    char *p = strstr(session->url_suffix, "trackID=");
    if (p) {
        trackID = atoi(p + 8);
    } else {
        ESP_LOGE(TAG, "can't parse trackID");
    }

    ESP_LOGI(TAG, "trackID=%d", trackID);

    rtp_session_info_t session_info = {
        .transport_mode = session->transport_mode,
        .socket_tcp = session->m_RtspClient,
        .rtp_port = session->m_ClientRTPPort,
    };
    media_streams_t *it;
    SLIST_FOREACH(it, &session->media_list, next) {
        if (it->trackid == trackID) {
            it->media_stream->rtp_session = rtp_session_create(&session_info);
            break;
        }
    }

    char Transport[128];
    char time_str[64];
    if (RTP_OVER_TCP == session->transport_mode) {
        snprintf(Transport, sizeof(Transport), "RTP/AVP/TCP;unicast;interleaved=0-1");
    } else {
        snprintf(Transport, sizeof(Transport),
                 "RTP/AVP;unicast;client_port=%i-%i;server_port=%i-%i",
                 session->m_ClientRTPPort,
                 session->m_ClientRTCPPort,
                 rtp_GetRtpServerPort(it->media_stream->rtp_session),
                 rtp_GetRtcpServerPort(it->media_stream->rtp_session));
    }
    int len = snprintf(Response, *length,
                       "RTSP/1.0 200 OK\r\nCSeq: %u\r\n"
                       "%s\r\n"
                       "Transport: %s\r\n"
                       "Session: %i\r\n\r\n",
                       session->m_CSeq,
                       DateHeader(time_str, sizeof(time_str)),
                       Transport,
                       session->m_RtspSessionID);

    if (len > 0) {
        *length = len;
    }
}

static void Handle_RtspPLAY(rtsp_session_t *session, char *Response, uint32_t *length)
{
    char time_str[64];
    int len = snprintf(Response, *length,
                       "RTSP/1.0 200 OK\r\nCSeq: %u\r\n"
                       "%s\r\n"
                       "Range: npt=0.000-\r\n"
                       "Session: %i\r\n"
                       "\r\n",
                       session->m_CSeq,
                       DateHeader(time_str, sizeof(time_str)),
                       session->m_RtspSessionID);
    if (len > 0) {
        *length = len;
    }
}

rtsp_session_t *rtsp_session_create(const char *url, uint16_t port)
{
    rtsp_session_t *session = (rtsp_session_t *)calloc(1, sizeof(rtsp_session_t));
    RTSP_SESSION_CHECK(NULL != session, "memory for rtsp session is not enough", NULL);

    ESP_LOGI(TAG, "Creating RTSP session\n");
    strcpy(session->resource_url, url);

    sockaddr_in ServerAddr;                                 // server address parameters
    ServerAddr.sin_family      = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port        = htons((0 == port) ? 554 : port); // listen on RTSP port
    session->MasterSocket      = socket(AF_INET, SOCK_STREAM, 0);

    int enable = 1;
    if (setsockopt(session->MasterSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        ESP_LOGE(TAG, "setsockopt(SO_REUSEADDR) failed");
        free(session);
        return NULL;
    }

    // bind our master socket to the RTSP port and listen for a client connection
    if (bind(session->MasterSocket, (sockaddr *)&ServerAddr, sizeof(ServerAddr)) != 0) {
        ESP_LOGE(TAG, "error can't bind port errno=%d", errno);
        free(session);
        return NULL;
    }
    if (listen(session->MasterSocket, 5) != 0) {
        ESP_LOGE(TAG, "error can't listen socket errno=%d", errno);
        free(session);
        return NULL;
    }

    session->m_RtspSessionID  = GET_RANDOM() >> 16;       // create a session ID
    session->m_StreamID       = -1;
    session->m_ClientRTPPort  =  0;
    session->m_ClientRTCPPort =  0;
    session->transport_mode =  RTP_OVER_UDP;
    session->state = 0;
    SLIST_INIT(&session->media_list);
    return session;
}


int rtsp_session_delete(rtsp_session_t *session)
{
    media_streams_t *it;
    SLIST_FOREACH(it, &session->media_list, next) {
        it->media_stream->delete_media(it->media_stream);
        free(it);
    }

    closesocket(session->MasterSocket);
    free(session);
    return 0;
}

int rtsp_session_accept(rtsp_session_t *session)
{
    sockaddr_in ClientAddr;                                   // address parameters of a new RTSP client
    socklen_t ClientAddrLen = sizeof(ClientAddr);
    session->m_RtspClient = accept(session->MasterSocket, (struct sockaddr *)&ClientAddr, &ClientAddrLen);
    session->state |= 0x01;
    ESP_LOGI(TAG, "Client connected. Client address: %s", inet_ntoa(ClientAddr.sin_addr));
    return 0;
}

int rtsp_session_terminate(rtsp_session_t *session)
{
    ESP_LOGI(TAG, "closing RTSP session");
    media_streams_t *it;
    SLIST_FOREACH(it, &session->media_list, next) {
        rtp_session_delete(it->media_stream->rtp_session);
    }
    closesocket(session->m_RtspClient);
    return 0;
}

int rtsp_session_add_media_stream(rtsp_session_t *session, media_stream_t *media)
{
    media_streams_t *it = (media_streams_t *) calloc(1, sizeof(media_streams_t));
    RTSP_SESSION_CHECK(NULL != it, "memory for rtsp media is not enough", -1);
    it->media_stream = media;
    it->trackid = session->media_stream_num++;
    it->next.sle_next = NULL;
    SLIST_INSERT_HEAD(&session->media_list, it, next);
    return 0;
}

/**
   Read from our socket, parsing commands as possible.
 */
int rtsp_handle_requests(rtsp_session_t *session, uint32_t readTimeoutMs)
{
    if (!(session->state & 0x01)) {
        return -1;    // Already closed down
    }
    char *buffer = (char *)session->RecvBuf;
    memset(buffer, 0x00, RTSP_BUFFER_SIZE);
    int res = socketread(session->m_RtspClient, buffer, RTSP_BUFFER_SIZE, readTimeoutMs);
    if (res > 0) {
        if (ParseRtspRequest(session, buffer, res)) {
            uint32_t length = RTSP_BUFFER_SIZE;
            switch (session->m_RtspCmdType) {
            case RTSP_OPTIONS: Handle_RtspOPTION(session, buffer, &length);
                break;

            case RTSP_DESCRIBE: Handle_RtspDESCRIBE(session, buffer, &length);
                break;

            case RTSP_SETUP: Handle_RtspSETUP(session, buffer, &length);
                break;

            case RTSP_PLAY: Handle_RtspPLAY(session, buffer, &length);
                break;

            default: break;
            }
            socketsend(session->m_RtspClient, buffer, length);
        }
        if (session->m_RtspCmdType == RTSP_PLAY) {
            session->state |= 0x02;
        } else if (session->m_RtspCmdType == RTSP_TEARDOWN) {
            session->state &= ~(0x02);
        }
    } else if (res == 0) {
        ESP_LOGI(TAG, "client closed socket, exiting");
        session->state = 0;
        return -3;
    } else  {
        return -2;
    }
    return 0;
}
