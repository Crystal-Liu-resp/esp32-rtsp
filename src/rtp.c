
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "rtp.h"
#include <pthread.h>

static const char *TAG = "RTP";

#define RTP_CHECK(a, str, ret_val)                       \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

static int rtp_InitUdpTransport(rtp_session_t *session);
static void rtp_ReleaseUdpTransport(rtp_session_t *session);
static void *rtcp_receive_task(void *args);

void mem_swap32(uint8_t *in, uint32_t length)
{
    if (length % 4) {
        ESP_LOGE(TAG, "length incorrect");
        return;
    }
    uint8_t m, n;
    for (size_t i = 0; i < length; i += 4) {
        m = in[i];
        n = in[i + 1];
        in[i] = in[i + 3];
        in[i + 1] = in[i + 2];
        in[i + 2] = n;
        in[i + 3] = m;
    }
}

uint8_t *mem_swap32_copy(uint8_t *out, const uint8_t *in, uint32_t length)
{
    if (length % 4) {
        ESP_LOGE("glue-esp32", "length incorrect");
        return out;
    }
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
    for (size_t i = 0; i < length; i += 4) {
        out[i] = in[i + 3];
        out[i + 1] = in[i + 2];
        out[i + 2] = in[i + 1];
        out[i + 3] = in[i];
    }
#else
    memcpy(out, in, length);
#endif
    return out + length;
}

rtp_session_t *rtp_session_create(rtp_session_info_t *session_info)
{
    rtp_session_t *session = (rtp_session_t *)calloc(1, sizeof(rtp_session_t));
    RTP_CHECK(NULL != session, "memory for RTP session is not enough", NULL);

    ESP_LOGI(TAG, "Creating RTP session");
    ESP_LOGI(TAG, "transport_mode=%d", session_info->transport_mode);
    ESP_LOGI(TAG, "socket_tcp=%d", session_info->socket_tcp);
    ESP_LOGI(TAG, "rtp_port=%d", session_info->rtp_port);

    session->session_info = *session_info;

    // init RTSP Session transport type (UDP or TCP) and ports for UDP transport
    if (RTP_OVER_UDP == session->session_info.transport_mode) {
        rtp_InitUdpTransport(session);
    }

    session->rtphdr.version = RTP_VERSION;
    session->rtphdr.p = 0;
    session->rtphdr.x = 0;
    session->rtphdr.cc = 0;
    session->rtphdr.m = 0;
    session->rtphdr.pt = 0;
    session->rtphdr.seq = 0;
    session->rtphdr.ts = 0;
    session->rtphdr.ssrc = GET_RANDOM();
    ESP_LOGI(TAG, "rtp sscr = %X", session->rtphdr.ssrc);

    pthread_t new_thread = (pthread_t)NULL;
    int res = pthread_create(&new_thread, NULL, rtcp_receive_task, (void *) session);

    return session;
}

void rtp_session_delete(rtp_session_t *session)
{
    if (NULL == session) {
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, "Pointer of rtp session is invalid");
        return;
    }

    rtp_ReleaseUdpTransport(session);
    free(session);
}

uint64_t rtp_time_now_us(void)
{
// #if _POSIX_TIMERS
#if 0
    struct timespec ts_now;
    clock_gettime(CLOCK_BOOTTIME, &ts_now);
    return ((uint64_t)ts_now.tv_sec * 1000000L) + ((uint64_t)ts_now.tv_nsec / 1000);
#else
    struct timeval ts_now;
    gettimeofday(&ts_now, NULL);
    return ((uint64_t)ts_now.tv_sec * 1000000L) + ((uint64_t)ts_now.tv_usec);
#endif
}

uint16_t rtp_GetRtpServerPort(rtp_session_t *session)
{
    return session->RtpServerPort;
}

uint16_t rtp_GetRtcpServerPort(rtp_session_t *session)
{
    return session->RtcpServerPort;
}

void rtp_set_rtp_port(rtp_session_t *session, uint16_t port)
{
    session->session_info.rtp_port = port;
}

static int rtp_InitUdpTransport(rtp_session_t *session)
{
    uint16_t P = 0;
#define UDP_PORT_MIN 6970
#define UDP_PORT_MAM 7000

    for (P = UDP_PORT_MIN; P < UDP_PORT_MAM; P += 2) {
        session->RtpSocket = udpsocketcreate(P);
        if (session->RtpSocket) {
            // Rtp socket was bound successfully. Lets try to bind the consecutive Rtsp socket
            session->RtcpSocket = udpsocketcreate(P + 1);
            if (session->RtcpSocket) {
                // allocate port pairs for RTP/RTCP ports in UDP transport mode
                session->RtpServerPort = P;
                session->RtcpServerPort = P + 1;
                break;
            } else {
                udpsocketclose(session->RtpSocket);
            }
        }
    }
    if (P >= UDP_PORT_MAM) {
        ESP_LOGE(TAG, "Can't create udp socket for RTP and RTCP");
        return -1;
    }
    return 0;
}

static void rtp_ReleaseUdpTransport(rtp_session_t *session)
{
    session->RtpServerPort = 0;
    session->RtcpServerPort = 0;
    if (RTP_OVER_UDP == session->session_info.transport_mode) {
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
    if (RTP_OVER_UDP == session->session_info.transport_mode) {
        IPADDRESS otherip;
        IPPORT otherport;
        socketpeeraddr(session->session_info.socket_tcp, &otherip, &otherport);
        udpsocketsend(session->RtpSocket, udp_buf, RtpPacketSize, otherip, session->session_info.rtp_port);
    } else if (RTP_OVER_TCP == session->session_info.transport_mode) {
        RtpBuf[0] = '$'; // magic number
        RtpBuf[1] = session->session_info.rtsp_channel;   // number of multiplexed subchannel on RTPS connection - here the RTP channel
        RtpBuf[2] = (RtpPacketSize & 0x0000FF00) >> 8;
        RtpBuf[3] = (RtpPacketSize & 0x000000FF);
        // RTP over RTSP - we send the buffer + 4 byte additional header
        socketsend(session->session_info.socket_tcp, RtpBuf, RtpPacketSize + RTP_TCP_HEAD_SIZE);
    } else {
#define MULTICAST_IP        "239.255.255.11"
        // #define MULTICAST_PORT      9832
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        inet_aton(MULTICAST_IP, &addr.sin_addr);
        IPADDRESS otherip = addr.sin_addr.s_addr;
        // IPPORT otherport;
        // socketpeeraddr(session->session_info.socket_tcp, &otherip, &otherport);
        udpsocketsend(session->RtpSocket, udp_buf, RtpPacketSize, otherip, session->session_info.rtp_port);
    }
    session->sn++;

    return ret;
}

void rtcp_receive_parse(const uint8_t *buffer, uint32_t len)
{
    printf("RTCP Received %d bytes [", len);
    for (size_t i = 0; i < len; i++) {
        printf("%x, ", buffer[i]);
    }
    printf("]\n");

    uint32_t deal_len = 0;
    while (deal_len < len) {
        rtcp_common_t *rtsp_com = (rtcp_common_t *)buffer;
        printf("pt=%d, len=%d\n", rtsp_com->pt, (rtsp_com->length + 1) * 4);

        if (RTCP_RR == rtsp_com->pt) {
            printf("sender ssrc=%X\n", *(uint32_t *)(buffer + 4));
            rtcp_rr_t *rr = (rtcp_rr_t *)(buffer + 8);
            printf("ssrc=%X\n", rr->ssrc);
            printf("lost=%d\n", rr->lost);
            printf("fraction=%d\n", rr->fraction);
            printf("last_seq=%d\n", rr->last_seq);
            printf("jitter=%d\n", rr->jitter);
            printf("lsr=%d\n", rr->lsr);
            printf("dlsr=%d\n", rr->dlsr);
        } else if (RTCP_SR == rtsp_com->pt) {

        } else if (RTCP_SDES == rtsp_com->pt) {

        } else if (RTCP_BYE == rtsp_com->pt) {

        } else if (RTCP_APP == rtsp_com->pt) {

        } else {
            ESP_LOGE(TAG, "Unknow RTCP packet");
            return;
        }
        deal_len += (rtsp_com->length + 1) * 4;
        buffer += deal_len;
    }
    if (deal_len != len) {
        ESP_LOGE(TAG, "rtcp length incorrect");
    }
}

static void *rtcp_receive_task(void *args)
{
    ESP_LOGI(TAG, "rtcp_receive_task");
    rtp_session_t *session = (rtp_session_t *)args;

    while (1) {
        if (RTP_OVER_UDP == session->session_info.transport_mode) {
            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(session->RtcpSocket, session->recv_buf, sizeof(session->recv_buf) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                pthread_exit(NULL);
            } else {
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
                mem_swap32(session->recv_buf, len);
#endif
                rtcp_receive_parse(session->recv_buf, len);

                // int err = sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                // if (err < 0) {
                //     ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                //     break;
                // }
            }
        } else if (RTP_OVER_TCP == session->session_info.transport_mode) {
            ESP_LOGW(TAG, "rtsp over tcp not need this task");
            pthread_exit(NULL);
        }
    }
}
