#include <QApplication>
#include <unistd.h>
#include "capture.h"
#include "mainwindow.h"
#include "socket_client.h"
#include "public.h"

work_state_t g_work_state;

int main(int argc, char *argv[])
{
    char *videodev = NULL;
    char *svr_ip = NULL;

    QApplication a(argc, argv);

    if(argc > 1)
    {
        videodev = argv[1];
        printf("[input] video: %s\n", videodev);
    }
    if(argc > 2)
    {
        svr_ip = argv[2];
        printf("[input] server ip: %s\n", svr_ip);
    }

    // 设置stdout缓冲大小为0，使得printf马上输出
    setbuf(stdout, NULL);

    g_work_state = WORK_STA_DISCONNECT;

    // 初始化显示界面
    mainwindow_init();

    newframe_mem_init();

    sleep(1);	// only to show background image
    start_capture_task(videodev);

    start_socket_client_task(svr_ip);

    return a.exec();
}
