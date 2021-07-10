
#include <stdio.h>
#include <string.h>
#include "media_stream.h"
#include "g711.h"
#include "media_g711a.h"

static const char *TAG = "rtp_g711a";

#define RTP_CHECK(a, str, ret_val)                       \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

/**
 * https://datatracker.ietf.org/doc/html/rfc2327
 *
 */
static void media_stream_g711a_get_description(media_stream_t *stream, char *buf, uint32_t buf_len, uint16_t port)
{
    snprintf(buf, buf_len, "m=audio %hu RTP/AVP %d", port, RTP_PT_PCMA);
}

static void media_stream_g711a_get_attribute(media_stream_t *stream, char *buf, uint32_t buf_len)
{
    snprintf(buf, buf_len,
             "a=rtpmap:%d PCMA/%hu/1\r\n"
             "a=framerate:10", // There should be no "\r\n" in the end
             RTP_PT_PCMA, stream->sample_rate);
}

int media_stream_g711a_SendRTCP(rtp_session_t *session , uint64_t *rtcp_clock, uint8_t *data, uint32_t len)
{
	// make sure have sent RTP packet
    struct timespec tp;
    clock_gettime(1, &tp);
    uint64_t clock=(uint64_t)tp.tv_sec * 1000 + tp.tv_nsec;
	int interval = rtp_rtcp_interval(session);
	if(0 == rtcp_clock || rtcp_clock + interval < clock)
	{
		char rtcp[1024] = {0};
		size_t n = rtp_rtcp_report(session, rtcp, sizeof(rtcp));

		// send RTCP packet
		rtcp_send_packet(&session, data, len);

		rtcp_clock = clock;
	}
	
	return 0;
}

static int media_stream_g711a_send_frame(media_stream_t *stream, const uint8_t *data, uint32_t len)
{

#define MAX_PCMA_PACKET_SIZE (MAX_RTP_PAYLOAD_SIZE - RTP_HEADER_SIZE - RTP_TCP_HEAD_SIZE)

    rtp_packet_t rtp_packet;
    rtp_packet.is_last = 0;
    uint64_t *rtcp_clock=0;
    rtp_packet.data = stream->rtp_buffer;
    uint8_t *pcma_buf = rtp_packet.data + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE;

    int data_bytes_left = len;
    uint32_t offset = 0;
    while (data_bytes_left != 0) {
        uint32_t curMsec = (uint32_t)(rtp_time_now_us() / 1000);
        if (stream->prevMsec == 0) { // first frame init our timestamp
            stream->prevMsec = curMsec;
        }
        // compute deltat (being careful to handle clock rollover with a little lie)
        uint32_t deltams = (curMsec >= stream->prevMsec) ? curMsec - stream->prevMsec : 100;
        stream->prevMsec = curMsec;

        uint8_t *p_buf = (uint8_t *)pcma_buf;

        uint32_t fragmentLen = MAX_PCMA_PACKET_SIZE - (p_buf - pcma_buf);
        if (fragmentLen >= data_bytes_left) {
            fragmentLen = data_bytes_left;
            rtp_packet.is_last = 1; // RTP marker bit must be set on last fragment
        }

        memcpy(p_buf, data + offset, fragmentLen);
        p_buf += fragmentLen;
        offset += fragmentLen;
        data_bytes_left -= fragmentLen;

        rtp_packet.size = p_buf - pcma_buf;
        rtp_packet.timestamp = stream->Timestamp;
        rtp_packet.type = RTP_PT_PCMA;
        rtp_send_packet(stream->rtp_session, &rtp_packet);
        /**
         * TODO:refactor RTCP sender!
         */
        media_stream_g711a_SendRTCP(stream->rtp_session ,rtcp_clock, stream->rtp_buffer, 1000);

        // Increment ONLY after a full frame
        stream->Timestamp += (stream->clock_rate * deltams / 1000);
    }
    return 1;
}

static void media_stream_g711a_delete(media_stream_t *stream)
{
    if (NULL != stream->rtp_buffer) {
        free(stream->rtp_buffer);
    }
    free(stream);
}

media_stream_t *media_stream_g711a_create(uint16_t sample_rate)
{
    media_stream_t *stream = (media_stream_t *)calloc(1, sizeof(media_stream_t));
    RTP_CHECK(NULL != stream, "memory for g711a stream is not enough", NULL);

    stream->rtp_buffer = (uint8_t *)malloc(MAX_RTP_PAYLOAD_SIZE);
    if (NULL == stream->rtp_buffer) {
        free(stream);
        ESP_LOGE(TAG, "memory for media mjpeg buffer is insufficient");
        return NULL;
    }
    stream->type = MEDIA_STREAM_PCMA;
    stream->clock_rate = 8000;
    stream->sample_rate = sample_rate;
    stream->delete_media = media_stream_g711a_delete;
    stream->get_attribute = media_stream_g711a_get_attribute;
    stream->get_description = media_stream_g711a_get_description;
    stream->handle_frame = media_stream_g711a_send_frame;
    return stream;
}


