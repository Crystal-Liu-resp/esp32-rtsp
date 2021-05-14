
#ifndef _RTSP_MEDIA_H_
#define _RTSP_MEDIA_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Defined in RFC3551
 * https://datatracker.ietf.org/doc/html/rfc3551#page-33
 *
 */
typedef enum {
    RTP_PT_PCMU = 0,
    RTP_PT_PCMA = 8,
    RTP_PT_JPEG = 26,
    RTP_PT_H264 = 96,
    RTP_PT_AAC  = 37,
    RTP_PT_H265 = 265,
} MediaType_t;

typedef enum {
    VIDEO_FRAME_I = 0x01,
    VIDEO_FRAME_P = 0x02,
    VIDEO_FRAME_B = 0x03,
    AUDIO_FRAME   = 0x11,
} FrameType;

#ifdef __cplusplus
}
#endif

#endif
