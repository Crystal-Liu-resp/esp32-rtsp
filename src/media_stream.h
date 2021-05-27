

#ifndef _MEDIA_STREAM_H_
#define _MEDIA_STREAM_H_

#include "rtp.h"
#include "media.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct media_stream_t{
    uint8_t *rtp_buffer;
    uint32_t prevMsec;
    uint32_t Timestamp;
    uint32_t clock_rate;
    rtp_session_t *rtp_session;
    void (*delete_media)(struct media_stream_t *stream);
    uint32_t (*get_samplerate)(void);
    uint32_t (*get_channels)(void);
    void (*get_description)(char *buf, uint32_t buf_len, uint16_t port);
    void (*get_attribute)(char *buf, uint32_t buf_len);
    int (*handle_frame)(struct media_stream_t *stream, const uint8_t *data, uint32_t len);
    uint32_t (*get_timestamp)();
} media_stream_t;


#ifdef __cplusplus
}
#endif

#endif
