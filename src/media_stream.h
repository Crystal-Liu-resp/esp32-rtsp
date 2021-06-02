

#ifndef _MEDIA_STREAM_H_
#define _MEDIA_STREAM_H_

#include "rtp.h"
#include "media.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MEDIA_STREAM_MJPEG,
    MEDIA_STREAM_PCMA,
    MEDIA_STREAM_PCMU,
}media_stream_type_t;

typedef struct media_stream_t{
    media_stream_type_t type;
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

media_stream_t* media_stream_create();

#ifdef __cplusplus
}
#endif

#endif
