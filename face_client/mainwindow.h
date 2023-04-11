#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPainter>
#include <QTimer>
#include <QDebug>
#include "config.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#define STR_NOT_CONN_SERVER  "未连接服务器"
#define STR_FACE_RECOGN_OK   "人脸识别成功"

#define BACKGROUND_IMAGE    "../face_client/background.jpg"

#define TIMER_DISPLAY_INTERV_MS			10
#define FACE_DETECT_DELAY_TIME          20
#define FACE_RECOGN_DELAY_TIME          200

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void window_display(void);

private:
    Ui::MainWindow *ui;
    QTimer  *display_timer;				// 用于刷新显示
    unsigned char videobuf[FRAME_BUF_SIZE];      // 用于缓存视频图像
};

int mainwindow_init(void);

#endif // MAINWINDOW_H
