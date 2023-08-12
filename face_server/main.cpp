#include <QApplication>
#include <unistd.h>
#include "mainwindow.h"
#include "opencv_face.h"
#include "socket_server.h"
#include "lib_proto.h"
#include "capture.h"
#include "public.h"

work_state_t g_work_state;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 设置stdout缓冲大小为0，使得printf马上输出
    setbuf(stdout, NULL);

    g_work_state = WORK_STA_NORMAL;

    newframe_mem_init();

    // 初始化显示界面
    mainwindow_init();

    proto_lib_init();

    sleep(1);	// only to show background image
    start_socket_server_task();

    start_face_process_task();

    return a.exec();
}
