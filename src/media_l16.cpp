
#include <stdio.h>
#include <string.h>
#include "media_stream.h"
#include "g711.h"
#include "media_l16.h"

static const char *TAG = "rtp_l16";

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
void media_stream_l16_get_description(media_stream_t *stream, char *buf, uint32_t buf_len, uint16_t port)
{
    snprintf(buf, buf_len, "m=audio %hu RTP/AVP %d", port, RTP_PT_L16_CH1);
}

void media_stream_l16_get_attribute(media_stream_t *stream, char *buf, uint32_t buf_len)
{
    snprintf(buf, buf_len, 
    "a=rtpmap:%d L16/%hu/1\r\n"
    "a=framerate:100", // There should be no "\r\n" in the end
    RTP_PT_L16_CH1, stream->sample_rate);
}

int media_stream_l16_send_frame(media_stream_t *stream, const uint8_t *data, uint32_t len)
{

#define MAX_L16_PACKET_SIZE (MAX_RTP_PAYLOAD_SIZE - RTP_HEADER_SIZE - RTP_TCP_HEAD_SIZE)

    rtp_packet_t rtp_packet;
    rtp_packet.is_last = 0;
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

        uint32_t fragmentLen = MAX_L16_PACKET_SIZE - (p_buf - pcma_buf);
        if (fragmentLen >= data_bytes_left) {
            fragmentLen = data_bytes_left;
            rtp_packet.is_last = 0; // RTP marker bit must be set on last fragment
        }

        memcpy(p_buf, data + offset, fragmentLen);
        p_buf += fragmentLen;
        offset += fragmentLen;
        data_bytes_left -= fragmentLen;

        rtp_packet.size = p_buf - pcma_buf;
        rtp_packet.timestamp = stream->Timestamp;
        rtp_packet.type = RTP_PT_L16_CH1;
        rtp_send_packet(stream->rtp_session, &rtp_packet);

        // Increment ONLY after a full frame
        stream->Timestamp += (stream->clock_rate * deltams / 1000);
    }
    return true;
}

static void media_stream_l16_delete(media_stream_t *stream)
{
    if (NULL != stream->rtp_buffer) {
        free(stream->rtp_buffer);
    }
    free(stream);
}

media_stream_t *media_stream_l16_create(uint16_t sample_rate)
{
    media_stream_t *stream = (media_stream_t *)calloc(1, sizeof(media_stream_t));
    RTP_CHECK(NULL != stream, "memory for g711a stream is not enough", NULL);

    stream->rtp_buffer = (uint8_t *)malloc(MAX_RTP_PAYLOAD_SIZE);
    if (NULL == stream->rtp_buffer) {
        free(stream);
        ESP_LOGE(TAG, "memory for media mjpeg buffer is insufficient");
        return NULL;
    }
    stream->type = MEDIA_STREAM_L16; //TODO:
    stream->sample_rate = sample_rate;
    stream->clock_rate = sample_rate;
    stream->delete_media = media_stream_l16_delete;
    stream->get_attribute = media_stream_l16_get_attribute;
    stream->get_description = media_stream_l16_get_description;
    stream->handle_frame = media_stream_l16_send_frame;
    return stream;
}


