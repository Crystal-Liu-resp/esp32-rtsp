#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include "platglue.h"
#include "rtsp_server.h"
#include "media_mjpeg.h"
#include "media_g711a.h"
#include "media_l16.h"
#include "g711.h"

static const char *TAG = "linux";

char *wave_get(void);
uint32_t wave_get_size(void);
uint32_t wave_get_framerate(void);
uint32_t wave_get_bits(void);
uint32_t wave_get_ch(void);

static uint32_t pic_num = 0;
static uint8_t *audio_p;
static uint8_t *audio_end;
static int64_t audio_last_frame = 0;

static void read_img(const char *file, uint8_t *data, uint32_t *size)
{
    *size = 0;
    int fd = open(file, O_RDONLY);

    if (fd == -1) {
        printf("[%s]error is %s\n", file, strerror(errno));
        return;
    }

    while (1) {
        ssize_t r = read(fd, data, 8192);
        if (r > 0) {
            *size += r;
        } else {
            break;
        }
    }
    close(fd);
}

static int get_folder_recording_cnt(char *root, uint32_t *cnt)
{
    DIR *dir;
    struct dirent *ptr;
    uint32_t total = 0;
    char path[128];

    dir = opendir(root); /* 打开bai目录*/
    if (NULL == dir) {
        printf("fail to open dir\n");
    }
    errno = 0;
    while (NULL != (ptr = readdir(dir))) {
        //顺序读取每一个目录项；
        //跳过“duzhi..”和“.”两个目录
        if (0 == strcmp(ptr->d_name, ".") || 0 == strcmp(ptr->d_name, "..") ) {
            continue;
        }

        if (ptr->d_type == DT_DIR) {
            sprintf(path, "%s%s/", root, ptr->d_name);

        }
        if (ptr->d_type == DT_REG) {
            total++;
        }
    }
    if (0 != errno) {
        printf("fail to read dir\n"); //失败则输出提示信息
    }
    closedir(dir);

    *cnt = total;
    printf("total file num=%d\n", total);
    return 0;
}

static void streamImage(media_stream_t *mjpeg_stream)
{
    static uint8_t img_buf[1024 * 300];
    static uint32_t index = 1;
    static int64_t last_frame = 0;
    int64_t interval = (rtp_time_now_us() - last_frame) / 1000;
    {
        printf("frame fps=%f\n", 1000.0f / (float)interval);
        uint8_t *p = img_buf;
        uint32_t len = 0;
        char name[64] = {0};
        sprintf(name, "../../simple/media/video/frames/hd_%03d.jpg", index);
        read_img(name, p, &len);
        mjpeg_stream->handle_frame(mjpeg_stream, p, len);
        index++;
        if (index > pic_num) {
            index = 1;
            ESP_LOGW(TAG, "video over");
            pthread_exit(NULL);
        }

        last_frame = rtp_time_now_us();
    }
}

static void streamaudio(media_stream_t *audio_stream)
{
    static uint8_t buffer[8192];
    int64_t interval = (rtp_time_now_us() - audio_last_frame) / 1000;
    if (audio_last_frame == 0) {
        audio_last_frame = rtp_time_now_us();
        audio_p = (uint8_t *)wave_get();
        return;
    }
    {
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
                ESP_LOGW(TAG, "audeo over");
                pthread_exit(NULL);
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

static void get_local_ip(char *ipaddr)
{
    int sock_get_ip;
    struct   sockaddr_in *sin;
    struct   ifreq ifr_ip;
    if ((sock_get_ip = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        printf("socket create failse...GetLocalIp!\n");
        return;
    }

    memset(&ifr_ip, 0, sizeof(ifr_ip));
    strncpy(ifr_ip.ifr_name, "eth0", sizeof(ifr_ip.ifr_name) - 1);

    if ( ioctl( sock_get_ip, SIOCGIFADDR, &ifr_ip) < 0 ) {
        memset(&ifr_ip, 0, sizeof(ifr_ip));
        strncpy(ifr_ip.ifr_name, "wlan0", sizeof(ifr_ip.ifr_name) - 1);
        if ( ioctl( sock_get_ip, SIOCGIFADDR, &ifr_ip) < 0 ) {
            printf("socket ioctl failse\n");
            close( sock_get_ip );
            return;
        }
    }
    sin = (struct sockaddr_in *)&ifr_ip.ifr_addr;
    strcpy(ipaddr, inet_ntoa(sin->sin_addr));
    close( sock_get_ip );
}

media_stream_t *mjpeg;
media_stream_t *pcma;
media_stream_t *l16;


static void *send_audio(void *args)
{
    rtsp_session_t *session = (rtsp_session_t *)args;
    while (1) {
        if (session->state & 0x02) {
            streamaudio(l16);
            usleep(20000);
        }
    }
}

static void *send_video(void *args)
{
    rtsp_session_t *session = (rtsp_session_t *)args;
    while (1) {
        if (session->state & 0x02) {
            streamImage(mjpeg);
            usleep(40000);
        }
    }
}

int main(void)
{
    get_folder_recording_cnt("../../simple/media/video/frames", &pic_num);
    char ip_str[64] = {0};
    get_local_ip(ip_str);
    ESP_LOGI(TAG, "Creating RTSP session [rtsp://%s:%hu/%s]", ip_str, 8554, "mjpeg/1");

    rtsp_session_t *rtsp = rtsp_session_create("mjpeg/1", 8554);
    mjpeg = media_stream_mjpeg_create();
    pcma = media_stream_g711a_create(16000);
    l16 = media_stream_l16_create(16000);
    rtsp_session_add_media_stream(rtsp, mjpeg);
    rtsp_session_add_media_stream(rtsp, l16);

    while (1) {

        rtsp_session_accept(rtsp);
        audio_p = (uint8_t *)wave_get();
        audio_end = (uint8_t *)wave_get() + wave_get_size();
        audio_last_frame = 0;
        pthread_t new_thread = (pthread_t)NULL;
        pthread_create(&new_thread, NULL, send_audio, (void *) rtsp);
        pthread_t _thread = (pthread_t)NULL;
        pthread_create(&_thread, NULL, send_video, (void *) rtsp);

        while (1) {
            int ret = rtsp_handle_requests(rtsp, 1);
            if (-3 == ret) {
                ESP_LOGW(TAG, "break");
                break;
            }

            // if (rtsp->state & 0x02) {
            //     streamImage(mjpeg);
            //     streamaudio(pcma);
            // }
        }
        rtsp_session_terminate(rtsp);

        // rtsp_session_delete(rtsp);
    }
    return 0;
}

