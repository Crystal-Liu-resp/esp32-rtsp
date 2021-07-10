
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "rtp.h"
#include "rtcp-internal.h"
#include "rtcp-header.h"
#include "rtp-member.h"
#include "rtp-member-list.h"

static const char *TAG = "RTP";

enum { 
	RTP_SENDER		= 1,	/// send RTP packet
	RTP_RECEIVER	= 2,	/// receive RTP packet
};

#define RTP_CHECK(a, str, ret_val)                       \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

// RFC3550 6.2 RTCP Transmission Interval (p21)
// It is recommended that the fraction of the session bandwidth added for RTCP be fixed at 5%.
// It is also recommended that 1/4 of the RTCP bandwidth be dedicated to participants that are sending data
#define RTCP_BANDWIDTH_FRACTION			0.05
#define RTCP_SENDER_BANDWIDTH_FRACTION	0.25

#define RTCP_REPORT_INTERVAL			5000 /* milliseconds RFC3550 p25 */
#define RTCP_REPORT_INTERVAL_MIN		2500 /* milliseconds RFC3550 p25 */

#define RTP_PAYLOAD_MAX_SIZE			(10 * 1024 * 1024)

rtp_session_t *rtp_session_create(rtp_session_info_t *session_info, uint32_t ssrc, uint32_t timestamp, int frequence, int bandwidth, int sender)
{
    rtp_session_t *session = (rtp_session_t *)calloc(1, sizeof(rtp_session_t));
    RTP_CHECK(NULL != session, "memory for RTP session is not enough", NULL);

    ESP_LOGI(TAG, "Creating RTP session");
    ESP_LOGI(TAG, "transport_mode=%d", session_info->transport_mode);
    ESP_LOGI(TAG, "socket_tcp=%d", session_info->socket_tcp);
    ESP_LOGI(TAG, "rtp_port=%d", session_info->rtp_port);

    session->session_info = *session_info;

    session->self = rtp_member_create(ssrc);
	session->members = rtp_member_list_create();
	session->senders = rtp_member_list_create();
	if(!session->self || !session->members || !session->senders)
	{
		rtp_session_delete(session);
		return NULL;
	}

    session->self->rtp_clock = rtpclock();
    session->self->rtp_timestamp = timestamp;
    rtp_member_list_add(session->members, session->self);


    // init RTSP Session transport type (UDP or TCP) and ports for UDP transport
    if (RTP_OVER_UDP == session->session_info.transport_mode) {
        rtp_InitUdpTransport(session);
    }

//	session->cbparam = param;
	session->rtcp_bw = (int)(bandwidth * RTCP_BANDWIDTH_FRACTION);
	session->avg_rtcp_size = 0;
	session->frequence = frequence;
	session->role = sender ? RTP_SENDER : RTP_RECEIVER;
	session->init = 1;

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
    if (NULL == session) {
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, "Pointer of rtp session is invalid");
        return;
    }

    if(session->members)
		rtp_member_list_destroy(session->members);
	if(session->senders)
		rtp_member_list_destroy(session->senders);
	if(session->self)
		rtp_member_release(session->self);

    rtp_ReleaseUdpTransport(session);
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

//在具体的数据类型文件中将其打包成packet
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

   // rtphdr=packet->rtp;
    mem_swap32_copy(udp_buf, (uint8_t *)rtphdr, RTP_HEADER_SIZE);//udp_buf指向rtp头开始的数据包
    uint32_t RtpPacketSize = packet->size + RTP_HEADER_SIZE;//数据大小+RTP_HEADER大小

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

//rtcp packet 有很多种类型
int rtcp_send_packet(rtp_session_t *session, rtcp_packet_t *packet)
{
    int ret = -1;
    uint32_t RTCP_SIZE;
    uint8_t *udp_buf;

    // Initialize RTP header
    rtcp_hdr_t *rtcphdr = &session->rtcphdr;    

    rtcphdr->count = packet->count;

    rtcphdr->pt = packet->type;
    rtcphdr->length = packet->size;

    switch(rtcphdr.pt)
	{
	case RTCP_SR:
		RTCP_SIZE=rtcp_sr_pack(session, udp_buf, packet->size);
		break;

	case RTCP_RR:
		RTCP_SIZE=rtcp_rr_pack(session, udp_buf, packet->size);
		break;

	case RTCP_SDES:
        RTCP_SIZE=rtcp_sdes_pack(session, udp_buf, packet->size);
		break;

	case RTCP_BYE:
        RTCP_SIZE=rtcp_bye_pack(session, udp_buf, packet->size);
		break;

	case RTCP_APP:
        RTCP_SIZE=rtcp_app_pack(session, udp_buf, packet->size);
		break;

	default:
		assert(0);
	}

    // Send RTCP packet
    if (RTP_OVER_UDP == session->session_info.transport_mode) {
        IPADDRESS otherip;
        IPPORT otherport;
        socketpeeraddr(session->session_info.socket_tcp, &otherip, &otherport);
        udpsocketsend(session->RtcpSocket, udp_buf, RTCP_SIZE, otherip, session->session_info.rtcp_port);
    }
    session->sn++;

    return ret;
}

int rtp_onreceived(void* rtp, const void* data, int bytes)
{
	struct rtp_context *session = (struct rtp_context *)rtp;
	return rtcp_input_rtp(session, data, bytes);
}

int rtp_onreceived_rtcp(void* rtp, const void* rtcp, int bytes)
{
	struct rtp_context *session = (struct rtp_context *)rtp;
	return rtcp_input_rtcp(session, rtcp, bytes);
}

int rtp_rtcp_report(void* rtp, void* data, int bytes)
{
	int n;
	struct rtp_context *session = (struct rtp_context *)rtp;

#pragma message("update we_sent flag")
	// don't send packet in 2T
	//session->role = RTP_RECEIVER

	if(RTP_SENDER == session->role)
	{
		// send RTP in 2T
		n = rtcp_sr_pack(session, (uint8_t*)data, bytes);
	}
	else
	{
		assert(RTP_RECEIVER == session->role);
		n = rtcp_rr_pack(session, (uint8_t*)data, bytes);
	}

	// compound RTCP Packet
	if(n < bytes)
	{
		n += rtcp_sdes_pack(session, (uint8_t*)data+n, bytes-n);
	}

	session->init = 0;
	return n;
}

int rtp_rtcp_bye(void* rtp, void* data, int bytes)
{
	struct rtp_context *session = (struct rtp_context *)rtp;
	return rtcp_bye_pack(session, (uint8_t*)data, bytes);
}

int rtp_rtcp_interval(void* rtp)
{
	double interval;
	struct rtp_context *session = (struct rtp_context *)rtp;
	interval = rtcp_interval(rtp_member_list_count(session->members),
		rtp_member_list_count(session->senders) + ((RTP_SENDER==session->role) ? 1 : 0),
		session->rtcp_bw, 
		(session->self->rtp_clock + 2*RTCP_REPORT_INTERVAL*1000 > rtpclock()) ? 1 : 0,
		session->avg_rtcp_size,
		session->init);

	return (int)(interval * 1000);
}

const char* rtp_get_cname(void* rtp, uint32_t ssrc)
{
	rtp_member *member;
	struct rtp_context *session = (struct rtp_context *)rtp;
	member = rtp_member_list_find(session->members, ssrc);
	return member ? (char*)member->sdes[RTCP_SDES_CNAME].data : NULL;
}

const char* rtp_get_name(void* rtp, uint32_t ssrc)
{
	rtp_member *member;
	struct rtp_context *session = (struct rtp_context *)rtp;
	member = rtp_member_list_find(session->members, ssrc);
	return member ? (char*)member->sdes[RTCP_SDES_NAME].data : NULL;
}
