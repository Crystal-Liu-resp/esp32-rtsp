
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


static const char *RTSP_VERSION = "RTSP/1.0";
static const char *USER_AGENT = "ESP32 (IDF:v4.4-dev-1594-g1d7068e4be-dirty)";

typedef struct {
    uint32_t code;
    const char *describe;
} status_code_t;

/**
 * https://www.rfc-editor.org/rfc/rfc2326.html
 */
static const status_code_t g_status_code[] = {
    {100, "100 Continue"},
    {200, "200 OK"},
    {201, "201 Created"},
    {250, "250 Low on Storage Space"},
    {300, "300 Multiple Choices"},
    {301, "301 Moved Permanently"},
    {302, "302 Moved Temporarily"},
    {303, "303 See Other"},
    {304, "304 Not Modified"},
    {305, "305 Use Proxy"},
    {400, "400 Bad Request"},
    {401, "401 Unauthorized"},
    {402, "402 Payment Required"},
    {403, "403 Forbidden"},
    {404, "404 Not Found"},
    {405, "405 Method Not Allowed"},
    {406, "406 Not Acceptable"},
    {407, "407 Proxy Authentication Required"},
    {408, "408 Request Time-out"},
    {410, "410 Gone"},
    {411, "411 Length Required"},
    {412, "412 Precondition Failed"},
    {413, "413 Request Entity Too Large"},
    {414, "414 Request-URI Too Large"},
    {415, "415 Unsupported Media Type"},
    {451, "451 Parameter Not Understood"},
    {452, "452 Conference Not Found"},
    {453, "453 Not Enough Bandwidth"},
    {454, "454 Session Not Found"},
    {455, "455 Method Not Valid in This State"},
    {456, "456 Header Field Not Valid for Resource"},
    {457, "457 Invalid Range"},
    {458, "458 Parameter Is Read-Only"},
    {459, "459 Aggregate operation not allowed"},
    {460, "460 Only aggregate operation allowed"},
    {461, "461 Unsupported transport"},
    {462, "462 Destination unreachable"},
    {500, "500 Internal Server Error"},
    {501, "501 Not Implemented"},
    {502, "502 Bad Gateway"},
    {503, "503 Service Unavailable"},
    {504, "504 Gateway Time-out"},
    {505, "505 RTSP Version not supported"},
    {551, "551 Option not supported"},
};

static const char *rtsp_get_status(uint32_t code)
{
    uint16_t num = sizeof(g_status_code) / sizeof(status_code_t);
    for (size_t i = 0; i < num; i++) {
        if (code == g_status_code[i].code) {
            return g_status_code[i].describe;
        }
    }
    return "500 Internal Server Error";
}

/**
 Define rtsp method
 method            direction        object     requirement
 DESCRIBE          C->S             P,S        recommended
 ANNOUNCE          C->S, S->C       P,S        optional
 GET_PARAMETER     C->S, S->C       P,S        optional
 OPTIONS           C->S, S->C       P,S        required
                                             (S->C: optional)
 PAUSE             C->S             P,S        recommended
 PLAY              C->S             P,S        required
 RECORD            C->S             P,S        optional
 REDIRECT          S->C             P,S        optional
 SETUP             C->S             S          required
 SET_PARAMETER     C->S, S->C       P,S        optional
 TEARDOWN          C->S             P,S        required
 */
static const char *METHOD_DESCRIBE      = "DESCRIBE";
static const char *METHOD_ANNOUNCE      = "ANNOUNCE";
static const char *METHOD_OPTIONS       = "OPTIONS";
static const char *METHOD_PAUSE         = "PAUSE";
static const char *METHOD_PLAY          = "PLAY";
static const char *METHOD_SETUP         = "SETUP";
static const char *METHOD_RECORD         = "RECORD";
static const char *METHOD_TEARDOWN      = "TEARDOWN";
static const char *METHOD_GET_PARAMETER = "GET_PARAMETER";
static const char *METHOD_SET_PARAMETER = "SET_PARAMETER";


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

/* The line that parses the resulting request information (line 1)*/
static int ParseRequestLine(rtsp_session_t *session, const char *message)
{
    char method[32] = {0};
    char *url = session->url;
    char version[32] = {0};

    /*
    * The number of arguments is not 3.
    * method url vesion\r\n
    * 
    * 
    OPTIONS rtsp://192.168.31.115:8554/live RTSP/1.0\r\n
    CSeq: 2\r\n
    \r\n

    DESCRIBE rtsp://192.168.31.115:8554/live RTSP/1.0\r\n
    CSeq: 3\r\n
    Accept: application/sdp\r\n
    \r\n

    SETUP rtsp://192.168.31.115:8554/live/track0 RTSP/1.0\r\n
    CSeq: 4\r\n
    Transport: RTP/AVP;unicast;client_port=54492-54493\r\n
    \r\n

    PLAY rtsp://192.168.31.115:8554/live RTSP/1.0\r\n
    CSeq: 5\r\n
    Session: 66334873\r\n
    Range: npt=0.000-\r\n
    \r\n

    TEARDOWN rtsp://192.168.31.115:8554/live RTSP/1.0\r\n
    CSeq: 6\r\n
    Session: 66334873\r\n
    \r\n
    */
    if (sscanf(message, "%s %s %s", method, url, version) != 3) {  
        return true;
    }


    char *url_end = &url[strlen(url) - 1];
    if (*url_end == '/') {
        *url_end = '\0'; // The character '/' at the end of url may cause some trouble in later processing, remove it.
    }

    // Get rtsp method
    if (strstr(method, METHOD_OPTIONS)) {
        session->method = RTSP_OPTIONS;
    } else if (strstr(method, METHOD_DESCRIBE)) {
        session->method = RTSP_DESCRIBE;
    } else if (strstr(method, METHOD_SETUP)) {
        session->method = RTSP_SETUP;
    } else if (strstr(method, METHOD_PLAY)) {
        session->method = RTSP_PLAY;
    } else if (strstr(method, METHOD_TEARDOWN)) {
        session->method = RTSP_TEARDOWN;
    } else if (strstr(method, METHOD_PAUSE)) {
        session->method = RTSP_PAUSE;
    } else if (strstr(method, METHOD_ANNOUNCE)) {
        session->method = RTSP_ANNOUNCE;
    } else if (strstr(method, METHOD_GET_PARAMETER)) {
        session->method = RTSP_GET_PARAMETER;
    } else if (strstr(method, METHOD_SET_PARAMETER)) {
        session->method = RTSP_SET_PARAMETER;
    } else {
        session->method = RTSP_UNKNOWN;
        return 1;
    }

    //url
    if (strncmp(url, "rtsp://", 7) != 0) {//7 characters more than the previous one
        return 1;
    }

    // parse url；  url + 7：192.168.31.115:8554/live
    // [^:]is replaced by url_ip，hu is replaced by url_port
    // [^:]represents that once read the ‘：‘，it ends reading
    if (sscanf(url + 7, "%[^:]:%hu/%s", session->url_ip, &session->url_port, session->url_suffix) == 3) {

    } else if (sscanf(url + 7, "%[^/]/%s", session->url_ip, session->url_suffix) == 2) {
        session->url_port = 554; // set to default port 554
    } else {
        return 1;
    }

    ESP_LOGD(TAG, "url:%s", session->url);
    ESP_LOGD(TAG, "url_suffix:%s", session->url_suffix);
    return 0;
}

/* Parsing the resulting header line (line 2) */
static int ParseHeadersLine(rtsp_session_t *session, const char *message)
{ 
    /*
    CSeq: x\r\n
    */
    ESP_LOGD(TAG, "<%s>", message);
    char *TmpPtr = NULL;
    TmpPtr = (char *)strstr(message, "CSeq: ");//Find the location in message where "CSeq: "first appeared (substring)
    if (TmpPtr) {
        session->CSeq  = atoi(TmpPtr + 6);//Converts the string pointed to by STR to an integer of type int
        return 0;
    }

    //session->method determined when parsing the first line of request information
    if (session->method == RTSP_DESCRIBE || session->method == RTSP_SETUP || session->method == RTSP_PLAY) {
        // ParseAuthorization(message);
    }

    /*
    OPTIONS rtsp://192.168.31.115:8554/live RTSP/1.0\r\n
    CSeq: 2\r\n
    \r\n
    */
    if (session->method == RTSP_OPTIONS) {
        session->parse_state = PARSE_STATE_GOTALL;
        return 0;
    }

    /*
    DESCRIBE rtsp://192.168.31.115:8554/live RTSP/1.0\r\n
    CSeq: 3\r\n
    Accept: application/sdp\r\n
    \r\n
    */
    if (session->method == RTSP_DESCRIBE) {
        session->parse_state = PARSE_STATE_GOTALL;
        return 0;
    }

    /*
    SETUP rtsp://192.168.31.115:8554/live/track0 RTSP/1.0\r\n
    CSeq: 4\r\n
    Transport: RTP/AVP;unicast;client_port=54492-54493\r\n
    \r\n
    */

    if (session->method == RTSP_SETUP) {
        TmpPtr = (char *)strstr(message, "Transport");
        if (TmpPtr) { // parse transport header
            TmpPtr = (char *)strstr(TmpPtr, "RTP/AVP/TCP");
            if (TmpPtr) {  //TCP
                session->transport_mode = RTP_OVER_TCP;
            } else {       //UDP
                session->transport_mode = RTP_OVER_UDP;
            }

            TmpPtr = (char *)strstr(message, "multicast");
            if (TmpPtr) {       //multicast
                session->transport_mode = RTP_OVER_MULTICAST;
                ESP_LOGD(TAG, "multicast");
            } else {            //unicast
                ESP_LOGD(TAG, "unicast");
            }

            char *ClientPortPtr = NULL;
            if (RTP_OVER_UDP == session->transport_mode) {
                //client_port=54492-54493\r\n
                ClientPortPtr = (char *)strstr(message, "client_port=");
            } else if (RTP_OVER_MULTICAST == session->transport_mode) {
                //multicast
                ClientPortPtr = (char *)strstr(message, "port=");
            }
            if (ClientPortPtr) {
                //UDP ports are 12 characters long and multicast is 5 characters long
                ClientPortPtr += (RTP_OVER_UDP == session->transport_mode) ? 12 : 5;
                char cp[16] = {0};
                /*
                 *Searches the string pointed to by the parameter STR 
                 *for the first occurrence of the character c (an unsigned character).
                 */
                char *p = strchr(ClientPortPtr, '-');//54492-54493->-54493
                if (p) { //UDP
                    strncpy(cp, ClientPortPtr, p - ClientPortPtr);//copy the first port number
                    session->m_ClientRTPPort  = atoi(cp); //Convert to an integer, which is the RTP port number
                    session->m_ClientRTCPPort = session->m_ClientRTPPort + 1;//RTCP port=RTP+1
                    ESP_LOGI(TAG, "rtsp client port %d-%d", session->m_ClientRTPPort, session->m_ClientRTCPPort);
                } else {
                    return 1;
                }
            }

            //Transport:RTP/AVP/TCP;unicast;interleaved=0-1
            if (RTP_OVER_TCP == session->transport_mode) {
                TmpPtr = (char *)strstr(message, "interleaved=");//point to 'i'
                if (TmpPtr) {
                    if (sscanf(TmpPtr += 12, "%hu-%hu", &session->rtp_channel, &session->rtcp_channel) == 2) {
                        ESP_LOGI(TAG, "RTP channel=%d, RTCP channel=%d", session->rtp_channel, session->rtcp_channel);
                    }
                }
            }

            session->parse_state = PARSE_STATE_GOTALL;
        }
        return 0;
    }

    if (session->method == RTSP_PLAY) {
        session->parse_state = PARSE_STATE_GOTALL;
        return 0;
    }

    if (session->method == RTSP_TEARDOWN) {
        session->parse_state = PARSE_STATE_GOTALL;
        return 0;
    }

    if (session->method == RTSP_GET_PARAMETER) {
        session->parse_state = PARSE_STATE_GOTALL;
        return 0;
    }

    return 1;
}

static int ParseRtspRequest(rtsp_session_t *session, const char *aRequest, uint32_t aRequestSize)
{
    printf("[%s]\n", aRequest);
    if (aRequestSize < 5) {
        printf("[%x, %x, %x, %x, %x]\n", aRequest[0], aRequest[1], aRequest[2], aRequest[3], aRequest[4]);
    }

    if (aRequest[0] == '$') {
        session->method = RTSP_UNKNOWN;
        return 0;
    }

    session->method = RTSP_UNKNOWN;
    session->CSeq = 0;
    memset(session->url, 0x00, RTSP_PARAM_STRING_MAX);
    session->parse_state = PARSE_STATE_REQUESTLINE;

    int ret = 0;
    char *string = (char *)aRequest;
    char const *end = string + aRequestSize;
    while (string < end) {
        switch (session->parse_state) {
        case PARSE_STATE_REQUESTLINE: {//Parse the request line (the first line) and return an int value
            char *firstCrlf = FindFirstCrlf((const char *)string);//Find the first enter/new line character
            if (firstCrlf != nullptr) {
                firstCrlf[0] = '\0';//null
                ret = ParseRequestLine(session, string);
                string = firstCrlf + 2;
            }

            if (0 == ret) {
                session->parse_state = PARSE_STATE_HEADERSLINE;
            } else {        //Returns 1 if it does not conform to the standard format
                string = (char *)end;
                ret = 1;
            }
        } break;

        case PARSE_STATE_HEADERSLINE: {//Parses the resulting request line (second line) and returns the int value
            char *firstCrlf = FindFirstCrlf((const char *)string);//find the first enter/ new line character
            if (firstCrlf != nullptr) {
                firstCrlf[0] = '\0';
                ret = ParseHeadersLine(session, string);
                string = firstCrlf + 2;
            } else {
                string = (char *)end;
                ret = 1;
            }
        } break;

        case PARSE_STATE_GOTALL: {
            string = (char *)end;
            ret = 0;
        } break;

        default:
            ret = 1;
            break;
        }
    }

    return ret;
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
    if (0 != strcmp(session->url_suffix, session->resource_url)) {
        ESP_LOGE(TAG, "[%s] Stream Not Found", session->url);
        // Stream not available
        int len = snprintf(Response, *length,
                           "%s %s\r\n"
                           "CSeq: %u\r\n"
                           "%s\r\n",
                           RTSP_VERSION,
                           rtsp_get_status(404),
                           session->CSeq,
                           DateHeader(time_str, sizeof(time_str)));
        if (len > 0) {
            *length = len;
        }
        return;
    }

    int len = snprintf(Response, *length,
                       "%s %s\r\n"
                       "CSeq: %u\r\n"
                       "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n\r\n",
                       RTSP_VERSION,
                       rtsp_get_status(200),
                       session->CSeq);
    if (len > 0) {
        *length = len;
    }
}

/*
Media level description
m=video 0 RTP/AVP 96\r\n
a=rtpmap:96 H264/90000\r\n
a=framerate:25\r\n
a=control:track0\r\n
 
m=video 0 RTP/AVP 96\r\n
format is m =< media type > < port number > < transport protocol > < media format >
media type：video
port number：0，The port number is told during the SETUP process, so it is not needed here
transfer protocol：RTP/AVP，represents RTP OVER UDP，if RTP/AVP/TCP，represents RTP OVER TCP
media format：represents payload type，
a=rtpmap:96 H264/90000
format: a=rtpmap:<media format><Coding format>/<clock frequency>
a=framerate:25
a=control:track0,Represents the number of the video stream in the session
*/
static void GetSdpMessage(rtsp_session_t *session, char *buf, uint32_t buf_len, const char *session_name)
{
/*
session level description
v=0\r\n
o=- 91565340853 1 IN IP4 192.168.31.115\r\n
t=0 0\r\n
a=contol:*\r\n

v=0,represents the version of sdp
o=- 91565340853 1 IN IP4 192.168.31.115
format: o=<username> <session id> <session version> <network type> <address type> <address>
username：-
session id：91565340853，Represents the live session in the rtsp://192.168.31.115:8554/live request.
version type：1
network type：IN，(internet)
address type：IP4，(ipv4),
address：192.168.31.115，(the address of the server)
*/
    snprintf(buf, buf_len,
             "v=0\r\n"
             "o=- 9%u 1 IN IP4 %s\r\n" //o=<username>(-) <session id> <version> <network type> <address type> <address>
             "t=0 0\r\n"
             "a=control:*\r\n",
             GET_RANDOM(), session->url_ip);

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
            it->media_stream->get_description(it->media_stream, str_buf, sizeof(str_buf), 0);
            snprintf(buf + strlen(buf), buf_len - strlen(buf),
                     "%s\r\n", str_buf);

            snprintf(buf + strlen(buf), buf_len - strlen(buf),
                     "c=IN IP4 %s/255\r\n", "0.0.0.0"/*multicast_ip_.c_str()*/);
        } else {
            it->media_stream->get_description(it->media_stream, str_buf, sizeof(str_buf), 0);
            snprintf(buf + strlen(buf), buf_len - strlen(buf),
                     "%s\r\n", str_buf);
        }

        it->media_stream->get_attribute(it->media_stream, str_buf, sizeof(str_buf));
        snprintf(buf + strlen(buf), buf_len - strlen(buf),
                 "%s\r\n", str_buf);
        snprintf(buf + strlen(buf), buf_len - strlen(buf),
                 "a=control:trackID=%d\r\n", it->trackid);
    }
}

/*
RTSP/1.0 200 OK\r\n
CSeq: 3\r\n
Content-length: 146\r\n
Content-type: application/sdp\r\n
\r\n

v=0\r\n
o=- 91565340853 1 in IP4 192.168.31.115\r\n
t=0 0\r\n
a=contol:*\r\n
m=video 0 RTP/AVP 96\r\n
a=rtpmap:96 H264/90000\r\n
a=framerate:25\r\n
a=control:track0\r\n
*/
static void Handle_RtspDESCRIBE(rtsp_session_t *session, char *Response, uint32_t *length)
{
    char time_str[64];
    char SDPBuf[256];
    GetSdpMessage(session, SDPBuf, sizeof(SDPBuf), NULL);
    int len = snprintf(Response, *length,
                       "%s %s\r\n"
                       "CSeq: %u\r\n"
                       "%s\r\n"
                       "Content-Base: %s/\r\n"
                       "Content-Type: application/sdp\r\n"
                       "Content-Length: %d\r\n\r\n"
                       "%s",
                       RTSP_VERSION,
                       rtsp_get_status(200),
                       session->CSeq,
                       DateHeader(time_str, sizeof(time_str)),
                       session->url,
                       (int) strlen(SDPBuf),
                       SDPBuf);

    if (len > 0) {
        *length = len;
    }
}

/*
RTSP/1.0 200 OK\r\n
CSeq: 4\r\n
Transport: RTP/AVP;unicast;client_port=54492-54493;server_port=56400-56401\r\n
Session: 66334873\r\n
\r\n
*/
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
        .socket_tcp = session->client_socket,
        .rtp_port = session->m_ClientRTPPort,
        .rtsp_channel = session->rtp_channel,
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
        snprintf(Transport, sizeof(Transport), "RTP/AVP/TCP;unicast;interleaved=%i-%i", session->rtp_channel, session->rtcp_channel);
    } else {
        snprintf(Transport, sizeof(Transport),
                 "RTP/AVP;unicast;client_port=%i-%i;server_port=%i-%i",
                 session->m_ClientRTPPort,
                 session->m_ClientRTCPPort,
                 rtp_GetRtpServerPort(it->media_stream->rtp_session),
                 rtp_GetRtcpServerPort(it->media_stream->rtp_session));
    }
    int len = snprintf(Response, *length,
                       "%s %s\r\n"
                       "CSeq: %u\r\n"
                       "%s\r\n"
                       "Transport: %s\r\n"
                       "Session: %s\r\n\r\n",
                       RTSP_VERSION,
                       rtsp_get_status(200),
                       session->CSeq,
                       DateHeader(time_str, sizeof(time_str)),
                       Transport,
                       session->session_id);

    if (len > 0) {
        *length = len;
    }
}

/*
RTSP/1.0 200 OK\r\n
CSeq: 5\r\n
Range: npt=0.000-\r\n
Session: 66334873; timeout=60\r\n
\r\n
*/
static void Handle_RtspPLAY(rtsp_session_t *session, char *Response, uint32_t *length)
{
    char time_str[64];
    int len = snprintf(Response, *length,
                       "%s %s\r\n"
                       "CSeq: %u\r\n"
                       "%s\r\n"
                       "Range: npt=0.000-\r\n"
                       "Session: %s\r\n"
                       "\r\n",
                       RTSP_VERSION,
                       rtsp_get_status(200),
                       session->CSeq,
                       DateHeader(time_str, sizeof(time_str)),
                       session->session_id);
    if (len > 0) {
        *length = len;
    }
}


static int get_optionReq(rtsp_session_t *session, char *buf, int buf_size)
{
    memset((void *)buf, 0, buf_size);
    int ret = snprintf(buf, buf_size,
                       "%s %s %s\r\n"
                       "CSeq: %u\r\n"
                       "User-Agent: %s\r\n"
                       "\r\n",
                       METHOD_OPTIONS, session->url, RTSP_VERSION,
                       session->CSeq + 1,
                       USER_AGENT);

    session->method = RTSP_OPTIONS;
    return ret;
}

static int get_announceReq(rtsp_session_t *session, char *buf, int buf_size, const char *sdp)
{
    memset((void *)buf, 0, buf_size);
    char time_str[64];
    char SDPBuf[256];
    GetSdpMessage(session, SDPBuf, sizeof(SDPBuf), NULL);
    int ret = snprintf(buf, buf_size,
                       "%s %s %s\r\n"
                       "Content-Type: application/sdp\r\n"
                       "CSeq: %u\r\n"
                       "User-Agent: %s\r\n"
                       "Session: %s\r\n"
                       "Content-Length: %d\r\n"
                       "\r\n"
                       "%s",
                       METHOD_ANNOUNCE, session->url, RTSP_VERSION,
                       session->CSeq + 1,
                       USER_AGENT,
                       session->session_id,
                       (int)strlen(SDPBuf),
                       SDPBuf);

    session->method = RTSP_ANNOUNCE;
    return ret;
}

static int get_describeReq(rtsp_session_t *session, char *buf, int buf_size)
{
    memset((void *)buf, 0, buf_size);
    int ret = snprintf(buf, buf_size,
                       "%s %s %s\r\n"
                       "CSeq: %u\r\n"
                       "Accept: application/sdp\r\n"
                       "User-Agent: %s\r\n"
                       "\r\n",
                       METHOD_DESCRIBE, session->url, RTSP_VERSION,
                       session->CSeq + 1,
                       USER_AGENT);

    session->method = RTSP_DESCRIBE;
    return ret;
}

static int get_setupTcpReq(rtsp_session_t *session, char *buf, int buf_size, int trackId)
{
    int interleaved[2] = { 0, 1 };
    if (trackId == 1) {
        interleaved[0] = 2;
        interleaved[1] = 3;
    }

    memset((void *)buf, 0, buf_size);
    int ret = snprintf((char *)buf, buf_size,
                       "%s %s/track%d %s\r\n"
                       "Transport: RTP/AVP/TCP;unicast;mode=record;interleaved=%d-%d\r\n"
                       "CSeq: %u\r\n"
                       "User-Agent: %s\r\n"
                       "Session: %s\r\n"
                       "\r\n",
                       METHOD_SETUP, session->url, trackId, RTSP_VERSION,
                       interleaved[0], interleaved[1],
                       session->CSeq + 1,
                       USER_AGENT,
                       session->session_id);

    session->method = RTSP_SETUP;
    return ret;
}

static int get_recordReq(rtsp_session_t *session, const char *buf, int buf_size)
{
    memset((void *)buf, 0, buf_size);
    int ret = snprintf((char *)buf, buf_size,
                       "%s %s %s\r\n"
                       "Range: npt=0.000-\r\n"
                       "CSeq: %u\r\n"
                       "User-Agent: %s\r\n"
                       "Session: %s\r\n"
                       "\r\n",
                       METHOD_RECORD, session->url, RTSP_VERSION,
                       session->CSeq + 1,
                       USER_AGENT,
                       session->session_id);

    session->method = RTSP_RECORD;
    return ret;
}

rtsp_session_t *rtsp_session_create(const char *url, uint16_t port)
{
    rtsp_session_t *session = (rtsp_session_t *)calloc(1, sizeof(rtsp_session_t));
    RTSP_SESSION_CHECK(NULL != session, "memory for rtsp session is not enough", NULL);

    strcpy(session->resource_url, url);
    port = (0 == port) ? 554 : port;

    sockaddr_in ServerAddr;                                 // server address parameters
    ServerAddr.sin_family      = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port        = htons(port); // listen on RTSP port
    session->MasterSocket      = socket(AF_INET, SOCK_STREAM, 0);

    tcpip_adapter_ip_info_t if_ip_info;
    char ip_str[64] = {0};
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &if_ip_info);
    sprintf(ip_str, "rtsp://%d.%d.%d.%d", IP2STR(&if_ip_info.ip));
    ESP_LOGI(TAG, "Creating RTSP session [%s:%hu/%s]", ip_str, port, url);

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
    snprintf(session->session_id, sizeof(session->session_id), "%X",  GET_RANDOM()); // create a session ID
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
    session->client_socket = accept(session->MasterSocket, (struct sockaddr *)&ClientAddr, &ClientAddrLen);
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
    closesocket(session->client_socket);
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
    int res = socketread(session->client_socket, buffer, RTSP_BUFFER_SIZE, readTimeoutMs);
    if (res > 0) {
        if (0 == ParseRtspRequest(session, buffer, res)) {
            uint32_t length = RTSP_BUFFER_SIZE;
            switch (session->method) {
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
            socketsend(session->client_socket, buffer, length);
        } else {
            ESP_LOGE(TAG, "rtsp request parse failed");
        }
        if (session->method == RTSP_PLAY) {
            session->state |= 0x02;
        } else if (session->method == RTSP_TEARDOWN) {
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
