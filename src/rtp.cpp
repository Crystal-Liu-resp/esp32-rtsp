
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "rtp.h"


static const char *TAG = "RTP";

static rtp_session_info_t g_rtp_session;
static rtp_hdr_t g_rtphdr;
static int g_RtpServerPort = 0;
static int g_RtcpServerPort = 0;
static int g_RtpSocket = NULLSOCKET;
static int g_RtcpSocket = NULLSOCKET;
static uint16_t g_sequence_number = 0;

int rtp_init(rtp_session_info_t *rtp_session)
{
    ESP_LOGI(TAG, "Creating RTP session");

    ESP_LOGI(TAG, "transport_mode=%d", rtp_session->transport_mode);
    ESP_LOGI(TAG, "socket_tcp=%d", rtp_session->socket_tcp);
    ESP_LOGI(TAG, "rtp_port=%d", rtp_session->rtp_port);

    g_rtp_session =  *rtp_session;

    g_rtphdr.version = RTP_VERSION;
    g_rtphdr.p = 0;
    g_rtphdr.x = 0;
    g_rtphdr.cc = 0;
    g_rtphdr.m = 0;
    g_rtphdr.pt = 0;
    g_rtphdr.seq = 0;
    g_rtphdr.ts = 0;
    g_rtphdr.ssrc = GET_RANDOM();
    return 0;
}

uint16_t rtp_GetRtpServerPort()
{
    return g_RtpServerPort;
};

uint16_t rtp_GetRtcpServerPort()
{
    return g_RtcpServerPort;
};

bool rtp_InitUdpTransport(void)
{
    for (uint16_t P = 6970; P < 0xFFFE; P += 2) {
        g_RtpSocket = udpsocketcreate(P);
        if (g_RtpSocket) {
            // Rtp socket was bound successfully. Lets try to bind the consecutive Rtsp socket
            g_RtcpSocket = udpsocketcreate(P + 1);
            if (g_RtcpSocket) {
                g_RtpServerPort = P;
                g_RtcpServerPort = P + 1;
                break;
            } else {
                udpsocketclose(g_RtpSocket);
                udpsocketclose(g_RtcpSocket);
            }
        }
    }

    return true;
}

void rtp_ReleaseUdpTransport(void)
{
    g_RtpServerPort = 0;
    g_RtcpServerPort = 0;
    udpsocketclose(g_RtpSocket);
    udpsocketclose(g_RtcpSocket);

    g_RtpSocket = NULLSOCKET;
    g_RtcpSocket = NULLSOCKET;
}

int rtp_send_packet(rtp_packet_t *packet)
{
    int ret = -1;
    uint8_t *RtpBuf = packet->data; // Note: we assume single threaded, this large buf we keep off of the tiny stack
    uint8_t *udp_buf = RtpBuf + RTP_TCP_HEAD_SIZE;

    // Initialize RTP header
    rtp_hdr_t *rtphdr = &g_rtphdr;
    rtphdr->m = packet->is_last; // RTP marker bit must be set on last fragment
    rtphdr->pt = packet->type;
    rtphdr->seq = g_sequence_number;
    rtphdr->ts = packet->timestamp;
    mem_swap32_copy(udp_buf, (uint8_t *)rtphdr, RTP_HEADER_SIZE);
    uint32_t RtpPacketSize = packet->size + RTP_HEADER_SIZE;

    // Send RTP packet
    if (RTP_OVER_UDP == g_rtp_session.transport_mode) {
        IPADDRESS otherip;
        IPPORT otherport;
        socketpeeraddr(g_rtp_session.socket_tcp, &otherip, &otherport);
        udpsocketsend(g_RtpSocket, udp_buf, RtpPacketSize, otherip, g_rtp_session.rtp_port);
    } else if (RTP_OVER_TCP == g_rtp_session.transport_mode) {
        RtpBuf[0] = '$'; // magic number
        RtpBuf[1] = 0;   // number of multiplexed subchannel on RTPS connection - here the RTP channel
        RtpBuf[2] = (RtpPacketSize & 0x0000FF00) >> 8;
        RtpBuf[3] = (RtpPacketSize & 0x000000FF);
        // RTP over RTSP - we send the buffer + 4 byte additional header
        socketsend(g_rtp_session.socket_tcp, RtpBuf, RtpPacketSize + RTP_TCP_HEAD_SIZE);
    } else {
        ESP_LOGW(TAG, "Unsupport multicast");
    }
    g_sequence_number++;

    return ret;
}
