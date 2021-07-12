

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
#include "media_l16.h"
#include "g711.h"
#include "frames.h"

char *wave_get(void);
uint32_t wave_get_size(void);
uint32_t wave_get_framerate(void);
uint32_t wave_get_bits(void);
uint32_t wave_get_ch(void);

static void streamImage(media_stream_t *mjpeg_stream)
{
    static uint32_t index = 0;
    static int64_t last_frame = 0;
    int64_t interval = (esp_timer_get_time() - last_frame) / 1000;
    if (interval > 40) {
        printf("frame fps=%f\n", 1000.0f/(float)interval);
        uint8_t *p = g_frames[index][0];
        uint32_t len = g_frames[index][1] - g_frames[index][0];
        mjpeg_stream->handle_frame(mjpeg_stream, p, len);
        index++;
        if (index >= sizeof(g_frames)/8) {
            index = 0;
        }

        last_frame = esp_timer_get_time();
    }
}

static uint8_t *audio_p;
static uint8_t *audio_end;
static int64_t audio_last_frame = 0;

static void streamaudio(media_stream_t *audio_stream)
{
    static uint8_t buffer[8192];
    int64_t interval = (esp_timer_get_time() - audio_last_frame) / 1000;
    if (audio_last_frame == 0) {
        audio_last_frame = esp_timer_get_time();
        audio_p = (uint8_t *)wave_get();
        return;
    }
    if (interval > 100) {
        uint32_t len = 0;
        if (MEDIA_STREAM_PCMA == audio_stream->type) {
            len = interval * 32;
            if (len > sizeof(buffer)) {
                len = sizeof(buffer);
            }
            int16_t *pcm = (int16_t *)audio_p;
            if (audio_p + len >= audio_end) {
                len = audio_end - audio_p;
                audio_p = (uint8_t *)wave_get();
            }
            for (size_t i = 0; i < len; i++) {
                buffer[i] = linear2alaw(pcm[i]);
            }
            audio_stream->handle_frame(audio_stream, buffer, len/2);
            audio_p += len;
        } else  if (MEDIA_STREAM_L16 == audio_stream->type) {
            len = interval * 32;
            if (len > sizeof(buffer)) {
                len = sizeof(buffer);
            }
            int16_t *pcm = (int16_t *)audio_p;
            if (audio_p + len >= audio_end) {
                len = audio_end - audio_p;
                audio_p = (uint8_t *)wave_get();
            }
            for (size_t i = 0; i < len / 2; i++) {
                buffer[i * 2] = pcm[i] >> 8;
                buffer[i * 2 + 1] = pcm[i] & 0xff;
            }
            audio_stream->handle_frame(audio_stream, buffer, len);
            audio_p += len;
        }
        printf("audio fps=%f\n", 1000.0f/(float)interval);

        audio_last_frame = esp_timer_get_time();
    }
}

static void rtsp_video()
{
    rtsp_session_t *rtsp = rtsp_session_create("mjpeg/1", 8554);
    media_stream_t *mjpeg = media_stream_mjpeg_create();//JPEG编码视频
    media_stream_t *pcma = media_stream_g711a_create(16000);//G711a编码音频
    media_stream_t *l16 = media_stream_l16_create(16000);//l16编码音频
    rtsp_session_add_media_stream(rtsp, mjpeg);//添加视频
    rtsp_session_add_media_stream(rtsp, pcma);//添加音频

    while (true) {

        rtsp_session_accept(rtsp);
        audio_p = (uint8_t *)wave_get();
        audio_end = (uint8_t *)wave_get() + wave_get_size();
        audio_last_frame = 0;

        while (1) {
            int ret = rtsp_handle_requests(rtsp, 1);//处理RTSP请求操作，包括options，describe，setup，play
            if (-3 == ret) {//客户关闭套接字，退出
                break;
            }

            if (rtsp->state & 0x02) { //play
                streamImage(mjpeg);
                streamaudio(pcma);
            }
        }
        rtsp_session_terminate(rtsp);//关闭RTSP通信

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
