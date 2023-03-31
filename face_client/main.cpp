#include <QApplication>
#include <unistd.h>
#include "capture.h"
#include "mainwindow.h"
#include "socket_client.h"
#include "public.h"

work_state_t g_work_state;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 设置stdout缓冲大小为0，使得printf马上输出
    setbuf(stdout, NULL);

    g_work_state = WORK_STA_NORMAL;

    // 初始化显示界面
    mainwindow_init();

    newframe_mem_init();

    sleep(1);	// only to show background image
    start_capture_task();

    start_socket_client_task();

    return a.exec();
}
