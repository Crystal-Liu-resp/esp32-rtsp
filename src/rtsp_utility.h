
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// supported command types
typedef enum {
    RTSP_OPTIONS,
    RTSP_DESCRIBE,
    RTSP_SETUP,
    RTSP_PLAY,
    RTSP_RECORD,
    RTSP_PAUSE,
    RTSP_ANNOUNCE,
    RTSP_TEARDOWN,
    RTSP_GET_PARAMETER,
    RTSP_SET_PARAMETER,
    RTSP_UNKNOWN
} rtsp_method_t;

typedef enum  {
    PARSE_STATE_REQUESTLINE,
    PARSE_STATE_HEADERSLINE,
    PARSE_STATE_GOTALL,
} parse_state_t;

typedef struct {
    const rtsp_method_t method;
    const char *str;
} rtsp_methods_t;

extern const rtsp_methods_t rtsp_methods[RTSP_UNKNOWN];

const char *rtsp_get_version(void);
const char *rtsp_get_user_agent(void);
const char *rtsp_get_status_from_code(uint32_t code);
char *find_first_crlf(const char *str);

#ifdef __cplusplus
}
#endif
