
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

static media_stream_t* media_stream_g711a_create(void)
{
    media_stream_t *stream = (media_stream_t*)calloc(1, sizeof(media_stream_t));
    RTP_CHECK(NULL != stream, "memory for g711a stream is not enough", NULL);

    stream->rtp_buffer = (uint8_t *)malloc(MAX_RTP_PAYLOAD_SIZE);
    if (NULL == stream->rtp_buffer) {
        free(stream);
        ESP_LOGE(TAG, "memory for media mjpeg buffer is insufficient");
        return NULL;
    }
    stream->clock_rate = 8000;
    return stream;
}

static void media_stream_g711a_delete(media_stream_t *stream)
{
    if (NULL != stream->rtp_buffer) {
        free(stream->rtp_buffer);
    }
    free(stream);
}

/**
 * https://datatracker.ietf.org/doc/html/rfc2327
 * 
 */
void media_stream_g711a_get_description(char *buf, uint32_t buf_len, uint16_t port)
{
    snprintf(buf, buf_len, "m=audio %hu RTP/AVP %d", port, RTP_PT_PCMA);
}

void media_stream_g711a_get_attribute(char *buf, uint32_t buf_len)
{
    snprintf(buf, buf_len, "a=rtpmap:%d PCMA/8000/1", RTP_PT_PCMA);
}


int media_stream_g711a_send_frame(media_stream_t *stream, const uint8_t *data, uint32_t len)
{
    if (len > MAX_RTP_PAYLOAD_SIZE) {
        return 1;
    }

    rtp_packet_t rtp_packet;
    rtp_packet.is_last = 0;
    rtp_packet.data = stream->rtp_buffer;

    uint32_t curMsec = (uint32_t)(esp_timer_get_time() / 1000);
    if (stream->prevMsec == 0) { // first frame init our timestamp
        stream->prevMsec = curMsec;
    }
    // compute deltat (being careful to handle clock rollover with a little lie)
    uint32_t deltams = (curMsec >= stream->prevMsec) ? curMsec - stream->prevMsec : 100;
    stream->prevMsec = curMsec;

    // ALaw_Encode();

    // rtp_packet.size = p_buf - mjpeg_buf;
    rtp_packet.timestamp = stream->Timestamp;
    rtp_packet.type = RTP_PT_JPEG;
    rtp_send_packet(stream->rtp_session, &rtp_packet);
    
    // Increment ONLY after a full frame
    stream->Timestamp += (stream->clock_rate * deltams / 1000);

    return true;
}


