
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "rtp.h"
#include "media_mjpeg.h"

static const char *TAG = "rtsp_server";

typedef struct {

}rtsp_server_t;

rtsp_server_t* rtsp_server_create()
{
    return NULL;
}

int rtsp_server_add_session(const char* session_name)
{
    return 0;
}

