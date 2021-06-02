/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_timer.h"
#include "app_wifi.h"
#include "rtsp_session.h"
#include "media_mjpeg.h"
#include "media_g711a.h"
#include "../images/frames.h"

char *wave_get(void);
uint32_t wave_get_size(void);
uint32_t wave_get_framerate(void);
uint32_t wave_get_bits(void);
uint32_t wave_get_ch(void);

static void streamImage(media_stream_t *mjpeg_stream)
{
    static uint32_t index = 0;
    static int64_t last_frame = 0;
    if (esp_timer_get_time() - last_frame > 80000) {
        printf("frame\n");
        uint8_t *p = g_frames[index][0];
        uint32_t len = g_frames[index][1] - g_frames[index][0];
        mjpeg_stream->handle_frame(mjpeg_stream, p, len);
        index++;
        if (index >= 10) {
            index = 0;
        }

        last_frame = esp_timer_get_time();
    }
}
#include "g711.h"
static void streamaudio(media_stream_t *pcma_stream)
{
    static uint8_t buffer[8192];
    static uint8_t *p=NULL;
    uint8_t *end = (uint8_t *)wave_get() + wave_get_size();
    static int64_t last_frame = 0;
    int64_t interval = (esp_timer_get_time() - last_frame) / 1000;
    if (0 == last_frame) {
        last_frame = esp_timer_get_time();
        p = (uint8_t *)wave_get();
        return;
    }

    if (interval > 100) {
        
        uint32_t len = interval * 8; //8byte per ms
        len = len * 2 > (end - p) ? (end - p) / 2 : len;
        uint16_t *pcm = (uint16_t *)p;
        for (size_t i = 0; i < len; i++) {
            buffer[i] = ALaw_Encode(pcm[i]);
        }
        printf("audio %d, %p\n", len, p);

        pcma_stream->handle_frame(pcma_stream, buffer, len);
        p += len*2;
        if (p >= end) {
            p = (uint8_t *)wave_get();
        }

        last_frame = esp_timer_get_time();
    }
}

static void rtsp_video()
{
    printf("running RTSP server\n");

    rtsp_session_t *rtsp = rtsp_session_create("mjpeg/1", 554);
    media_stream_t *mjpeg = media_stream_mjpeg_create();
    media_stream_t *pcma = media_stream_g711a_create();
    rtsp_session_add_media_stream(rtsp, mjpeg);
    rtsp_session_add_media_stream(rtsp, pcma);

    while (true) {

        rtsp_session_accept(rtsp);

        while (1) {
            int ret = rtsp_handle_requests(rtsp, 1);
            if (-3 == ret) {
                break;
            }

            if (rtsp->state & 0x02) {
                streamImage(mjpeg);
                streamaudio(pcma);
            }
        }
        rtsp_session_terminate(rtsp);

        // rtsp_session_delete(rtsp);
    }
}


void app_main(void)
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU cores, WiFi%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Free heap: %d\n", esp_get_free_heap_size());

    app_wifi_main();
    rtsp_video();
}
