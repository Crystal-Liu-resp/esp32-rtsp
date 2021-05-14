
#include <stdio.h>
#include <time.h>
#include "esp_log.h"
#include "rtsp_session.h"

static const char *TAG = "rtsp";

CRtspSession::CRtspSession(WiFiClient &aClient) :
    m_Client(aClient)
{
    printf("Creating RTSP session\n");

    m_RtspClient = m_Client;
    m_RtspSessionID  = GET_RANDOM() >> 16;       // create a session ID
    m_StreamID       = -1;
    m_ClientRTPPort  =  0;
    m_ClientRTCPPort =  0;
    m_Transport_mode   =  RTP_OVER_UDP;
    m_streaming = false;
    m_stopped = false;

};

CRtspSession::~CRtspSession()
{
    printf("Deleting RTSP session\n");
    rtp_ReleaseUdpTransport();
    closesocket(m_RtspClient);
};

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

bool CRtspSession::ParseRequestLine(const char *message)
{
    char method[32] = {0};
    char url[128] = {0};
    char version[32] = {0};

    if (sscanf(message, "%s %s %s", method, url, version) != 3) {
        return true;
    }

    if (strstr(method, "OPTIONS")) {
        m_RtspCmdType = RTSP_OPTIONS;
    } else if (strstr(method, "DESCRIBE")) {
        m_RtspCmdType = RTSP_DESCRIBE;
    } else if (strstr(method, "SETUP")) {
        m_RtspCmdType = RTSP_SETUP;
    } else if (strstr(method, "PLAY")) {
        m_RtspCmdType = RTSP_PLAY;
    } else if (strstr(method, "TEARDOWN")) {
        m_RtspCmdType = RTSP_TEARDOWN;
    } else if (strstr(method, "GET_PARAMETER")) {
        m_RtspCmdType = RTSP_GET_PARAMETER;
    } else {
        m_RtspCmdType = RTSP_UNKNOWN;
        return true;
    }

    printf("RTSP received %s\n", method);

    if (strncmp(url, "rtsp://", 7) != 0) {
        return true;
    }

    // parse url
    uint16_t port = 0;
    char suffix[64] = {0};

    if (sscanf(url + 7, "%[^:]:%hu/%s", m_ip, &port, suffix) == 3) {

    } else if (sscanf(url + 7, "%[^/]/%s", m_ip, suffix) == 2) {
        port = 554;
    } else {
        return true;
    }

    sprintf(m_HostPort, "%s:%d", m_ip, port);
    sprintf(m_url, "%s", url);

    ESP_LOGD(TAG, "HostPort:%s", m_HostPort);
    ESP_LOGD(TAG, "url:%s", m_url);

    return 0;
};

bool CRtspSession::ParseHeadersLine(const char *message)
{
    ESP_LOGW(TAG, "<%s>", message);
    char *TmpPtr = NULL;
    TmpPtr = strstr(message, "CSeq: ");
    if (TmpPtr) {
        m_CSeq  = atoi(TmpPtr + 6);
        return 0;
    }

    if (m_RtspCmdType == RTSP_DESCRIBE || m_RtspCmdType == RTSP_SETUP || m_RtspCmdType == RTSP_PLAY) {
        // ParseAuthorization(message);
    }

    if (m_RtspCmdType == RTSP_OPTIONS) {
        state_ = ParseState_GotAll;
        return 0;
    }

    if (m_RtspCmdType == RTSP_DESCRIBE) {
        state_ = ParseState_GotAll;
        return 0;
    }

    if (m_RtspCmdType == RTSP_SETUP) {
        TmpPtr = strstr(message, "Transport");
        if (TmpPtr) { // parse transport header
            TmpPtr = strstr(TmpPtr, "RTP/AVP/TCP");
            if (TmpPtr) {
                m_Transport_mode = RTP_OVER_TCP;
            } else {
                m_Transport_mode = RTP_OVER_UDP;
            }

            TmpPtr = strstr(message, "multicast");
            if (TmpPtr) {
                m_Transport_mode = RTP_OVER_MULTICAST;
                ESP_LOGD(TAG, "multicast");
            } else {
                ESP_LOGD(TAG, "unicast");
            }

            char *ClientPortPtr = NULL;
            if (RTP_OVER_UDP == m_Transport_mode) {
                ClientPortPtr = strstr(message, "client_port=");
            } else if (RTP_OVER_MULTICAST == m_Transport_mode) {
                ClientPortPtr = strstr(message, "port=");
            }
            if (ClientPortPtr) {
                ClientPortPtr += (RTP_OVER_UDP == m_Transport_mode) ? 12 : 5;
                char cp[16] = {0};
                char *p = strchr(ClientPortPtr, '-');
                if (p) {
                    strncpy(cp, ClientPortPtr, p - ClientPortPtr);
                    m_ClientRTPPort  = atoi(cp);
                    m_ClientRTCPPort = m_ClientRTPPort + 1;
                    ESP_LOGI(TAG, "rtsp client port %d-%d", m_ClientRTPPort, m_ClientRTCPPort);
                } else {
                    return 1;
                }
            }

            if (RTP_OVER_TCP == m_Transport_mode) {
                TmpPtr = strstr(message, "interleaved=");
                if (TmpPtr) {
                    int RTP_channel, RTCP_channel;
                    if (sscanf(TmpPtr += 12, "%d-%d", &RTP_channel, &RTCP_channel) == 2) {
                        ESP_LOGI(TAG, "RTP channel=%d, RTCP channel=%d", RTP_channel, RTCP_channel);
                    }                
                }
            }

            state_ = ParseState_GotAll;
        }
        return 0;
    }

    if (m_RtspCmdType == RTSP_PLAY) {
        state_ = ParseState_GotAll;
        return 0;
    }

    if (m_RtspCmdType == RTSP_TEARDOWN) {
        state_ = ParseState_GotAll;
        return 0;
    }

    if (m_RtspCmdType == RTSP_GET_PARAMETER) {
        state_ = ParseState_GotAll;
        return 0;
    }

    return 1;
};

bool CRtspSession::ParseRtspRequest(const char *aRequest, uint32_t aRequestSize)
{
    printf("[%s]\n", aRequest);
    if (aRequestSize < 5) {
        printf("[%x, %x, %x, %x, %x]\n", aRequest[0], aRequest[1], aRequest[2], aRequest[3], aRequest[4]);
    }

    if (aRequest[0] == '$') {
        m_RtspCmdType   = RTSP_UNKNOWN;
        return true;
    }

    m_RtspCmdType   = RTSP_UNKNOWN;
    m_CSeq = 0;
    memset(m_url, 0x00, sizeof(m_url));
    memset(m_HostPort, 0x00, sizeof(m_HostPort));
    state_ = ParseState_RequestLine;

    bool ret = 0;
    char *string = (char *)aRequest;
    char const *end = string + aRequestSize;
    while (string < end) {
        switch (state_) {
        case ParseState_RequestLine: {
            char *firstCrlf = FindFirstCrlf((const char *)string);
            if (firstCrlf != nullptr) {
                firstCrlf[0] = '\0';
                ret = ParseRequestLine(string);
                string = firstCrlf + 2;
            }

            if (0 == ret) {
                state_ = ParseState_HeadersLine;
            } else {
                ESP_LOGE(TAG, "rtsp request parse failed");
                string = (char *)end;
            }
        } break;

        case ParseState_HeadersLine: {
            char *firstCrlf = FindFirstCrlf((const char *)string);
            if (firstCrlf != nullptr) {
                firstCrlf[0] = '\0';
                ret = ParseHeadersLine(string);
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
};

RTSP_CMD_TYPES CRtspSession::Handle_RtspRequest(char const *aRequest, unsigned aRequestSize)
{
    if (ParseRtspRequest(aRequest, aRequestSize)) {
        char *response = (char *)aRequest;
        uint32_t length = RTSP_BUFFER_SIZE;
        switch (m_RtspCmdType) {
        case RTSP_OPTIONS:  {
            Handle_RtspOPTION(response, &length);
            break;
        };
        case RTSP_DESCRIBE: {
            Handle_RtspDESCRIBE(response, &length);
            break;
        };
        case RTSP_SETUP:    {
            Handle_RtspSETUP(response, &length);
            break;
        };
        case RTSP_PLAY:     {
            Handle_RtspPLAY(response, &length);
            break;
        };
        default: {};
        };
        socketsend(m_RtspClient, response, length);
    };
    return m_RtspCmdType;
};

void CRtspSession::Handle_RtspOPTION(char *Response, uint32_t *length)
{
    int len = snprintf(Response, *length,
                       "RTSP/1.0 200 OK\r\nCSeq: %u\r\n"
                       "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n\r\n", m_CSeq);
    if (len > 0) {
        *length = len;
    }
}

void CRtspSession::Handle_RtspDESCRIBE(char *Response, uint32_t *length)
{
    char time_str[64];
    m_StreamID = -1;        // invalid URL
    if (strstr(m_url, "mjpeg/1")) {
        m_StreamID = 0;
    } else if (strstr(m_url, "mjpeg/2")) {
        m_StreamID = 1;
    }
    if (m_StreamID == -1) {
        ESP_LOGI(TAG, "Stream Not Found");
        // Stream not available
        snprintf(Response, *length,
                 "RTSP/1.0 404 Stream Not Found\r\nCSeq: %u\r\n%s\r\n",
                 m_CSeq,
                 DateHeader(time_str, sizeof(time_str)));

        socketsend(m_RtspClient, Response, strlen(Response));
        return;
    };

    char SDPBuf[128];
    snprintf(SDPBuf, sizeof(SDPBuf),
             "v=0\r\n"
             "o=- %d 1 IN IP4 %s\r\n"
             "s=\r\n"
             "t=0 0\r\n"                          // start / stop - 0 -> unbounded and permanent session
             "m=video 0 RTP/AVP 26\r\n"           // currently we just handle UDP sessions
             "c=IN IP4 0.0.0.0\r\n",
             rand(),
             m_ip);
    int len = snprintf(Response, *length,
                       "RTSP/1.0 200 OK\r\nCSeq: %u\r\n"
                       "%s\r\n"
                       "Content-Base: %s/\r\n"
                       "Content-Type: application/sdp\r\n"
                       "Content-Length: %d\r\n\r\n"
                       "%s",
                       m_CSeq,
                       DateHeader(time_str, sizeof(time_str)),
                       m_url,
                       (int) strlen(SDPBuf),
                       SDPBuf);

    if (len > 0) {
        *length = len;
    }
}

void CRtspSession::Handle_RtspSETUP(char *Response, uint32_t *length)
{
    // init RTSP Session transport type (UDP or TCP) and ports for UDP transport
    if (!m_Transport_mode) {
        // allocate port pairs for RTP/RTCP ports in UDP transport mode
        rtp_InitUdpTransport();
    }

    rtp_session_info_t rtp_session = {
        .transport_mode = m_Transport_mode,
        .socket_tcp = m_RtspClient,
        .rtp_port = m_ClientRTPPort,
    };
    rtp_init(&rtp_session);

    char Transport[128];
    char time_str[64];
    if (m_Transport_mode) {
        snprintf(Transport, sizeof(Transport), "RTP/AVP/TCP;unicast;interleaved=0-1");
    } else {
        snprintf(Transport, sizeof(Transport),
                 "RTP/AVP;unicast;client_port=%i-%i;server_port=%i-%i",
                 m_ClientRTPPort,
                 m_ClientRTCPPort,
                 rtp_GetRtpServerPort(),
                 rtp_GetRtcpServerPort());
    }
    int len = snprintf(Response, *length,
                       "RTSP/1.0 200 OK\r\nCSeq: %u\r\n"
                       "%s\r\n"
                       "Transport: %s\r\n"
                       "Session: %i\r\n\r\n",
                       m_CSeq,
                       DateHeader(time_str, sizeof(time_str)),
                       Transport,
                       m_RtspSessionID);

    if (len > 0) {
        *length = len;
    }
}

void CRtspSession::Handle_RtspPLAY(char *Response, uint32_t *length)
{
    char time_str[64];
    int len = snprintf(Response, *length,
                       "RTSP/1.0 200 OK\r\nCSeq: %u\r\n"
                       "%s\r\n"
                       "Range: npt=0.000-\r\n"
                       "Session: %i\r\n"
                       "\r\n",
                       m_CSeq,
                       DateHeader(time_str, sizeof(time_str)),
                       m_RtspSessionID);
    if (len > 0) {
        *length = len;
    }
}

char const *CRtspSession::DateHeader(char *buf, uint32_t length)
{
    time_t tt = time(NULL);
    strftime(buf, length, "Date: %a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
    return buf;
}

int CRtspSession::GetStreamID()
{
    return m_StreamID;
};

/**
   Read from our socket, parsing commands as possible.
 */
bool CRtspSession::handleRequests(uint32_t readTimeoutMs)
{
    if (m_stopped) {
        return false;    // Already closed down
    }

    memset(RecvBuf, 0x00, RTSP_BUFFER_SIZE);
    int res = socketread(m_RtspClient, RecvBuf, RTSP_BUFFER_SIZE, readTimeoutMs);
    if (res > 0) {
        RTSP_CMD_TYPES C = Handle_RtspRequest(RecvBuf, res);
        if (C == RTSP_PLAY) {
            m_streaming = true;
        } else if (C == RTSP_TEARDOWN) {
            m_stopped = true;
            m_streaming = false;
        }

        return true;
    } else if (res == 0) {
        printf("client closed socket, exiting\n");
        m_stopped = true;
        return true;
    } else  {
        // Timeout on read

        return false;
    }
}
