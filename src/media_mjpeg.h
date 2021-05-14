
#pragma once

#include "media.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct
{
    // little-endian
    uint32_t off : 24;  /* fragment byte offset */
    uint32_t tspec : 8; /* type-specific field */

    uint32_t height : 8; /* frame height in 8 pixel blocks */
    uint32_t width : 8;  /* frame width in 8 pixel blocks */
    uint32_t q : 8;      /* quantization factor (or table id) */
    uint32_t type : 8;   /* id of jpeg decoder params */
} jpeghdr_t;

typedef struct
{
    // little-endian
    uint32_t count : 14;
    uint32_t l : 1;
    uint32_t f : 1;
    uint32_t dri : 16;
} jpeghdr_rst_t;

typedef struct
{
    // little-endian
    uint32_t length : 16;
    uint32_t precision : 8;
    uint32_t mbz : 8;
} jpeghdr_qtable_t;

int media_mjpeg_create_session(const char *stream);
int media_mjpeg_delete_session(void);

int media_mjpeg_send_frame(const uint8_t *jpeg_data, uint32_t jpegLen, uint16_t w, uint16_t h);

#ifdef __cplusplus
}
#endif
