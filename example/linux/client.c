

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include "rtsp_client.h"
#include "media_mjpeg.h"
#include "media_g711a.h"
#include "media_l16.h"
#include "g711.h"
// #include "media/video/frames.h"

static const char *TAG = "main";

char *wave_get(void);
uint32_t wave_get_size(void);
uint32_t wave_get_framerate(void);
uint32_t wave_get_bits(void);
uint32_t wave_get_ch(void);


static void read_img(const char *file, uint8_t *data, uint32_t *size)
{
    *size=0;
    int fd = open(file, O_RDONLY);

    if (fd == -1) {
        printf("[%s]error is %s\n", file, strerror(errno));
        return;
    }

    while (1) {
        ssize_t r = read(fd, data, 8192);
        if (r > 0) {
            *size+=r;
        }else{
            break;
        }
    }
    close(fd);
}

static void streamImage(media_stream_t *mjpeg_stream)
{
    static uint8_t img_buf[1024*300];
    static uint32_t index = 1;
    static int64_t last_frame = 0;
    int64_t interval = (rtp_time_now_us() - last_frame) / 1000;
    if (interval > 40) {
        printf("frame fps=%f\n", 1000.0f / (float)interval);
        uint8_t *p = img_buf;
        uint32_t len=0;
        char name[64]={0};
        sprintf(name, "../../simple/media/video/frames/hd_%03d.jpg", index);
        read_img(name, p, &len);
        printf("len=%d\n", len);
        mjpeg_stream->handle_frame(mjpeg_stream, p, len);
        index++;
        if (index >= 500) {
            index = 0;
        }

        last_frame = rtp_time_now_us();
    }
}

static uint8_t *audio_p;
static uint8_t *audio_end;
static int64_t audio_last_frame = 0;

static void streamaudio(media_stream_t *audio_stream)
{
    static uint8_t buffer[8192];
    int64_t interval = (rtp_time_now_us() - audio_last_frame) / 1000;
    if (audio_last_frame == 0) {
        audio_last_frame = rtp_time_now_us();
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
            audio_stream->handle_frame(audio_stream, buffer, len / 2);
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
        printf("audio fps=%f\n", 1000.0f / (float)interval);

        audio_last_frame = rtp_time_now_us();
    }
}

int main(void)
{
    printf("Hello world!\n");
    rtsp_client_t *rtsp = rtsp_client_create("rtsp://192.168.50.83:554/linux_rtsp");
    media_stream_t *mjpeg = media_stream_mjpeg_create();
    media_stream_t *pcma = media_stream_g711a_create(16000);
    media_stream_t *l16 = media_stream_l16_create(16000);
    rtsp_client_add_media_stream(rtsp, mjpeg);
    rtsp_client_add_media_stream(rtsp, pcma);

    int ret = rtsp_client_push_media(rtsp, RTP_OVER_TCP);
    if (0 != ret) {
        ESP_LOGE(TAG, "push error");
        return 1;
    }

    while (1) {

        // rtsp_session_accept(rtsp);
        audio_p = (uint8_t *)wave_get();
        audio_end = (uint8_t *)wave_get() + wave_get_size();
        audio_last_frame = 0;

        while (1) {
            // int ret = rtsp_handle_requests(rtsp, 1);
            // if (-3 == ret) {
            //     break;
            // }

            if (rtsp->state & 0x02) {
                streamImage(mjpeg);
                streamaudio(pcma);
            }
        }
        // rtsp_session_terminate(rtsp);

        // rtsp_session_delete(rtsp);
    }
}

