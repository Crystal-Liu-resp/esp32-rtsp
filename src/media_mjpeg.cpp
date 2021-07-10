
#include <stdio.h>
#include <string.h>

#include "media_stream.h"
#include "media_mjpeg.h"
#include "rtp.h"

static const char *TAG = "rtp_mjpeg";

#define RTP_CHECK(a, str, ret_val)                       \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

#define MAX_JPEG_PACKET_SIZE (MAX_RTP_PAYLOAD_SIZE - RTP_HEADER_SIZE - RTP_TCP_HEAD_SIZE)


static void media_stream_mjpeg_get_description(media_stream_t *stream, char *buf, uint32_t buf_len, uint16_t port)
{
    snprintf(buf, buf_len, "m=video %hu RTP/AVP %d", port, RTP_PT_JPEG);
}

static void media_stream_mjpeg_get_attribute(media_stream_t *stream, char *buf, uint32_t buf_len)
{
    snprintf(buf, buf_len, "a=rtpmap:%d JPEG/90000", RTP_PT_JPEG);
}

static const uint8_t *findJPEGheader(const uint8_t *start, const uint8_t *end, uint16_t marker)
{
    const uint8_t *p = start;

    while (p < end) {
        uint16_t data = *(uint16_t *)p;
        if (data == marker) {
            return p;
        } else {
            p++;
            // // not the section we were looking for, skip the entire section
            // switch (data) {
            // case 0xd8: { // start of image
            //     break; // no data to skip
            // }
            // case 0xe0: // app0
            // case 0xdb: // dqt
            // case 0xc4: // dht
            // case 0xc0: // sof0
            // case 0xda: { // sos
            //     // standard format section with 2 bytes for len.  skip that many bytes
            //     uint32_t len = p[0] * 256 + p[1];
            //     //printf("skipping section 0x%x, %d bytes\n", typecode, len);
            //     p += len;
            //     break;
            // }
            // default:
            //     break;
            // }
        }
    }

    return NULL;
}

static bool decodeJPEGfile(const uint8_t **start, uint32_t *len, const uint8_t **qtable0, const uint8_t **qtable1, uint16_t *width, uint16_t *height)
{
    /**
     * JPEG format Reference:
     * https://en.wikipedia.org/wiki/JPEG_File_Interchange_Format
     * http://lad.dsc.ufcg.edu.br/multimidia/jpegmarker.pdf
     */
    #define SOI  0xD8FF
    #define EOI  0xD9FF
    #define S0F0 0xC0FF
    #define S0F2 0xC2FF
    #define DHT  0xC4FF
    #define DQT  0xDBFF
    #define DRI  0xDDFF
    const uint8_t *end = *start + *len;

    // Look for quant tables if they are present
    const uint8_t *p = findJPEGheader(*start, end, DQT);
    if (p) {
        *qtable0 = p + 5; // 3 bytes of header skipped
        uint32_t length = p[2] * 256 + p[3];
        p += length;
        p = findJPEGheader(p, end, DQT);
        if (p) {
            *qtable1 = p + 5;
            uint32_t length = p[2] * 256 + p[3];
            p += length;
        } else {
            printf("can't read qtable1\n");
            return false;
        }
    } else {
        printf("can't read qtable0\n");
        return false;
    }

    p = findJPEGheader(p, end, S0F0);
    if (p) {
        *height = (p[5] << 8) | (p[6]);
        *width  = (p[7] << 8) | (p[8]);
    } else {
        printf("can't read iamge size\n");
        return false;
    }

    return true;
}

int media_stream_mjpeg_send_frame(media_stream_t *stream, const uint8_t *jpeg_data, uint32_t jpegLen)
{
    uint16_t w = 0;
    uint16_t h = 0;
    uint32_t curMsec = (uint32_t)(esp_timer_get_time() / 1000);
    if (stream->prevMsec == 0) { // first frame init our timestamp
        stream->prevMsec = curMsec;
    }
    // compute deltat (being careful to handle clock rollover with a little lie)
    uint32_t deltams = (curMsec >= stream->prevMsec) ? curMsec - stream->prevMsec : 100;
    stream->prevMsec = curMsec;

    rtp_packet_t rtp_packet;
    rtp_packet.is_last = 0;
    rtp_packet.data = stream->rtp_buffer;

    // locate quant tables if possible
    const uint8_t *qtable0 = NULL, *qtable1 = NULL;
    if (!decodeJPEGfile(&jpeg_data, &jpegLen, &qtable0, &qtable1, &w, &h)) {
        ESP_LOGW(TAG, "can't decode jpeg data");
        // return -1;
    }

    uint8_t q = (qtable0 && qtable1) ? 128 : 0x5e;
    uint8_t *mjpeg_buf = rtp_packet.data + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE;

    /**
     * Prepare the 8 byte payload JPEG header. Reference https://tools.ietf.org/html/rfc2435
     */
    jpeghdr_t jpghdr;
    jpghdr.tspec = 0; // type specific
    jpghdr.off = 0;
    jpghdr.type = 0; // (fixme might be wrong for camera data)
    jpghdr.q = q;
    jpghdr.width = w / 8;
    jpghdr.height = h / 8;

    int jpeg_bytes_left = jpegLen;
    while (jpeg_bytes_left != 0) {
        uint8_t *p_buf = mjpeg_buf;
        p_buf = mem_swap32_copy(p_buf, (uint8_t *)&jpghdr, sizeof(jpeghdr_t));

        // if (dri != 0) {
        //  jpeghdr_rst_t rsthdr;
        //         memcpy(ptr, &rsthdr, sizeof(rsthdr));
        //         ptr += sizeof(rsthdr);
        // }

        if (q >= 128 && jpghdr.off == 0) {
            // we need a quant header - but only in first packet of the frame
            int numQantBytes = 64; // Two 64 byte tables
            jpeghdr_qtable_t qtblhdr;
            qtblhdr.mbz = 0;
            qtblhdr.precision = 0; // 8 bit precision
            qtblhdr.length = 2 * numQantBytes;
            p_buf = mem_swap32_copy(p_buf, (uint8_t *)&qtblhdr, sizeof(jpeghdr_qtable_t));
            memcpy(p_buf, qtable0, numQantBytes);
            p_buf += numQantBytes;
            memcpy(p_buf, qtable1, numQantBytes);
            p_buf += numQantBytes;
        }

        uint32_t fragmentLen = MAX_JPEG_PACKET_SIZE - (p_buf - mjpeg_buf);
        if (fragmentLen >= jpeg_bytes_left) {
            fragmentLen = jpeg_bytes_left;
            rtp_packet.is_last = 1; // RTP marker bit must be set on last fragment
        }

        memcpy(p_buf, jpeg_data + jpghdr.off, fragmentLen);
        p_buf += fragmentLen;
        jpghdr.off += fragmentLen;
        jpeg_bytes_left -= fragmentLen;

        rtp_packet.size = p_buf - mjpeg_buf;
        rtp_packet.timestamp = stream->Timestamp;
        rtp_packet.type = RTP_PT_JPEG;
        rtp_send_packet(stream->rtp_session, &rtp_packet);
    }
    // Increment ONLY after a full frame
    stream->Timestamp += (stream->clock_rate * deltams / 1000);
    return 0;
}

static void media_stream_mjpeg_delete(media_stream_t *stream)
{
    if (NULL != stream->rtp_buffer) {
        free(stream->rtp_buffer);
    }
    free(stream);
}

media_stream_t* media_stream_mjpeg_create(void)
{
    media_stream_t *stream = (media_stream_t*)calloc(1, sizeof(media_stream_t));
    RTP_CHECK(NULL != stream, "memory for mjpeg stream is not enough", NULL);

    stream->rtp_buffer = (uint8_t *)malloc(MAX_RTP_PAYLOAD_SIZE);
    if (NULL == stream->rtp_buffer) {
        free(stream);
        ESP_LOGE(TAG, "memory for media mjpeg buffer is insufficient");
        return NULL;
    }
    stream->type = MEDIA_STREAM_MJPEG;
    stream->clock_rate = 90000;
    stream->delete_media = media_stream_mjpeg_delete;
    stream->get_attribute = media_stream_mjpeg_get_attribute;
    stream->get_description = media_stream_mjpeg_get_description;
    stream->handle_frame = media_stream_mjpeg_send_frame;
    return stream;
}

