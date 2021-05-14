#pragma once

#include "platglue.h"
#include "rtp.h"

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

#define RTSP_BUFFER_SIZE       4096    // for incoming requests, and outgoing responses
#define RTSP_PARAM_STRING_MAX  128
#define MAX_HOSTNAME_LEN       128

class CRtspSession {
public:
    CRtspSession(WiFiClient &aRtspClient);
    ~CRtspSession();

    int GetStreamID();
    bool handleRequests(uint32_t readTimeoutMs);

    bool m_streaming;
    bool m_stopped;

private:
    RTSP_CMD_TYPES Handle_RtspRequest(char const *aRequest, unsigned aRequestSize);
    bool ParseRtspRequest(const char *aRequest, uint32_t aRequestSize);
    bool ParseRequestLine(const char *message);
    bool ParseHeadersLine(const char *message);
    char const *DateHeader(char *buf, uint32_t length);

    // RTSP request command handlers
    void Handle_RtspOPTION(char *Response, uint32_t *length);
    void Handle_RtspDESCRIBE(char *Response, uint32_t *length);
    void Handle_RtspSETUP(char *Response, uint32_t *length);
    void Handle_RtspPLAY(char *Response, uint32_t *length);

    // global session state parameters
    int m_RtspSessionID;
    WiFiClient m_Client;
    SOCKET m_RtspClient;                                      // RTSP socket of that session
    int m_StreamID;                                           // number of simulated stream of that session
    IPPORT m_ClientRTPPort;                                  // client port for UDP based RTP transport
    IPPORT m_ClientRTCPPort;                                 // client port for UDP based RTCP transport
    transport_mode_t m_Transport_mode;

    // parameters of the last received RTSP request
    char RecvBuf[RTSP_BUFFER_SIZE];
    RTSP_CMD_TYPES m_RtspCmdType;                             // command type (if any) of the current request
    char m_url[RTSP_PARAM_STRING_MAX];                        // stream url
    uint32_t m_CSeq;                                          // RTSP command sequence number
    char m_HostPort[MAX_HOSTNAME_LEN];                        // host:port part of the URL
    char m_ip[20];
    RtspRequestParseState state_;
};
