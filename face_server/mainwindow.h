#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QPainter>
#include <QDateTime>
#include <QTimer>
#include <QDebug>
#include "config.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#define STR_ADDING_FACE      "正在录入人脸"
#define STR_ADD_FACE_OK      "录入人脸成功"
#define STR_ADD_FACE_FAILED  "录入人脸失败"
#define STR_FACE_RECOGN_OK   "人脸识别成功"

#define BACKGROUND_IMAGE    "../face_server/background.jpg"

#define TIMER_DISPLAY_INTERV_MS			10
#define FACE_DETECT_DELAY_TIME          10
#define FACE_RECOGN_DELAY_TIME          200

#define STR_ID           "编号"
#define STR_USER_ID      "用户ID"
#define STR_TIME         "时间"
#define STR_NAME         "名字"
#define STR_ATDIN_TIME   "签到时间"
#define STR_ATDOUT_TIME  "签退时间"
#define STR_ATTEND_STATE "考勤状态"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    QSqlDatabase sql_db;
    QSqlTableModel *sqlmodel;

private slots:
    void window_display(void);

    void on_addUserBtn_clicked();

    void on_delUserBtn_clicked();

    void on_attendListBtn_clicked();

    void on_switchCamBtn_clicked();

    void on_setAtdTimeBtn_clicked();

    void on_clearAtdTblBtn_clicked();

private:
    Ui::MainWindow *ui;

    QTimer  *display_timer;				// 用于刷新显示
    unsigned char videobuf[FRAME_BUF_SIZE];      // 用于缓存视频图像
    QImage			backgroundImg;		// background image
    int     video_display_flag;              // 显示视频

    int init_table_view(void);
};

int mainwindow_init(void);

#endif // MAINWINDOW_H
