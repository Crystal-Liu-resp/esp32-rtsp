
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "rtp.h"

static const char *TAG = "RTP";

#define RTP_CHECK(a, str, ret_val)                       \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }


rtp_session_t *rtp_session_create(rtp_session_info_t *rtp_session)
{
    rtp_session_t *session = (rtp_session_t *)calloc(1, sizeof(rtp_session_t));
    RTP_CHECK(NULL != session, "memory for RTP session is not enough", NULL);

    ESP_LOGI(TAG, "Creating RTP session");
    ESP_LOGI(TAG, "transport_mode=%d", rtp_session->transport_mode);
    ESP_LOGI(TAG, "socket_tcp=%d", rtp_session->socket_tcp);
    ESP_LOGI(TAG, "rtp_port=%d", rtp_session->rtp_port);

    session->rtp_session =  *rtp_session;

    session->rtphdr.version = RTP_VERSION;
    session->rtphdr.p = 0;
    session->rtphdr.x = 0;
    session->rtphdr.cc = 0;
    session->rtphdr.m = 0;
    session->rtphdr.pt = 0;
    session->rtphdr.seq = 0;
    session->rtphdr.ts = 0;
    session->rtphdr.ssrc = GET_RANDOM();
    return session;
}

void rtp_session_delete(rtp_session_t *session)
{
    free(session);
}

uint16_t rtp_GetRtpServerPort(rtp_session_t *session)
{
    return session->RtpServerPort;
}

uint16_t rtp_GetRtcpServerPort(rtp_session_t *session)
{
    return session->RtcpServerPort;
}

bool rtp_InitUdpTransport(rtp_session_t *session)
{
    for (uint16_t P = 6970; P < 0xFFFE; P += 2) {
        session->RtpSocket = udpsocketcreate(P);
        if (session->RtpSocket) {
            // Rtp socket was bound successfully. Lets try to bind the consecutive Rtsp socket
            session->RtcpSocket = udpsocketcreate(P + 1);
            if (session->RtcpSocket) {
                session->RtpServerPort = P;
                session->RtcpServerPort = P + 1;
                break;
            } else {
                udpsocketclose(session->RtpSocket);
                udpsocketclose(session->RtcpSocket);
            }
        }
    }

    return true;
}

void rtp_ReleaseUdpTransport(rtp_session_t *session)
{
    session->RtpServerPort = 0;
    session->RtcpServerPort = 0;
    if (session->RtpSocket) {
        udpsocketclose(session->RtpSocket);
        udpsocketclose(session->RtcpSocket);
    }

    session->RtpSocket = NULLSOCKET;
    session->RtcpSocket = NULLSOCKET;
}

int rtp_send_packet(rtp_session_t *session, rtp_packet_t *packet)
{
    int ret = -1;
    uint8_t *RtpBuf = packet->data; // Note: we assume single threaded, this large buf we keep off of the tiny stack
    uint8_t *udp_buf = RtpBuf + RTP_TCP_HEAD_SIZE;

    // Initialize RTP header
    rtp_hdr_t *rtphdr = &session->rtphdr;
    rtphdr->m = packet->is_last; // RTP marker bit must be set on last fragment
    rtphdr->pt = packet->type;
    rtphdr->seq = session->sn;
    rtphdr->ts = packet->timestamp;
    mem_swap32_copy(udp_buf, (uint8_t *)rtphdr, RTP_HEADER_SIZE);
    uint32_t RtpPacketSize = packet->size + RTP_HEADER_SIZE;

    // Send RTP packet
    if (RTP_OVER_UDP == session->rtp_session.transport_mode) {
        IPADDRESS otherip;
        IPPORT otherport;
        socketpeeraddr(session->rtp_session.socket_tcp, &otherip, &otherport);
        udpsocketsend(session->RtpSocket, udp_buf, RtpPacketSize, otherip, session->rtp_session.rtp_port);
    } else if (RTP_OVER_TCP == session->rtp_session.transport_mode) {
        RtpBuf[0] = '$'; // magic number
        RtpBuf[1] = 0;   // number of multiplexed subchannel on RTPS connection - here the RTP channel
        RtpBuf[2] = (RtpPacketSize & 0x0000FF00) >> 8;
        RtpBuf[3] = (RtpPacketSize & 0x000000FF);
        // RTP over RTSP - we send the buffer + 4 byte additional header
        socketsend(session->rtp_session.socket_tcp, RtpBuf, RtpPacketSize + RTP_TCP_HEAD_SIZE);
    } else {
#define MULTICAST_IP        "239.255.255.11"
        // #define MULTICAST_PORT      9832
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        inet_aton(MULTICAST_IP, &addr.sin_addr);
        IPADDRESS otherip = addr.sin_addr.s_addr;
        // IPPORT otherport;
        // socketpeeraddr(session->rtp_session.socket_tcp, &otherip, &otherport);
        udpsocketsend(session->RtpSocket, udp_buf, RtpPacketSize, otherip, session->rtp_session.rtp_port);
    }
    session->sn++;

    return ret;
}
