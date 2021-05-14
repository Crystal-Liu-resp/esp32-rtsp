
#include "esp_timer.h"
#include "rtsp_session.h"
#include "media_mjpeg.h"
#include "../images/frames.h"

static void streamImage(void)
{
    static uint32_t index=0;
    static int64_t last_frame = 0;
    if (esp_timer_get_time() - last_frame > 40000) {
        printf("frame\n");
        uint8_t *p = g_frames[index][0];
        uint32_t len = g_frames[index][1]-g_frames[index][0];
        media_mjpeg_send_frame(p, len, 0, 0);
        index++;
        if (index>=10) {
            index=0;
        }

        last_frame = esp_timer_get_time();
    }
}

extern "C" void rtsp_video()
{
    SOCKET MasterSocket;                                      // our masterSocket(socket that listens for RTSP client connections)
    SOCKET ClientSocket;                                      // RTSP socket to handle an client
    sockaddr_in ServerAddr;                                   // server address parameters
    sockaddr_in ClientAddr;                                   // address parameters of a new RTSP client
    socklen_t ClientAddrLen = sizeof(ClientAddr);

    printf("running RTSP server\n");

    ServerAddr.sin_family      = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port        = htons(8554);                 // listen on RTSP port 8554
    MasterSocket               = socket(AF_INET, SOCK_STREAM, 0);

    int enable = 1;
    if (setsockopt(MasterSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        printf("setsockopt(SO_REUSEADDR) failed");
        return ;
    }

    // bind our master socket to the RTSP port and listen for a client connection
    if (bind(MasterSocket, (sockaddr *)&ServerAddr, sizeof(ServerAddr)) != 0) {
        printf("error can't bind port errno=%d\n", errno);

        return ;
    }
    if (listen(MasterSocket, 5) != 0) {
        return ;
    }

    while (true) {
        // loop forever to accept client connections
        ClientSocket = accept(MasterSocket, (struct sockaddr *)&ClientAddr, &ClientAddrLen);
        CRtspSession *rtsp = new CRtspSession(ClientSocket);
        printf("Client connected. Client address: %s\r\n", inet_ntoa(ClientAddr.sin_addr));
        media_mjpeg_create_session("");

        while (!rtsp->m_stopped) {
            rtsp->handleRequests(1);

            if (rtsp->m_streaming) {
                streamImage();
            }
        }
        delete rtsp;
        media_mjpeg_delete_session();

        //break;
    }

    closesocket(MasterSocket);
}
