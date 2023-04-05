#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "opencv_face.h"
#include "capture.h"
#include "user_sql.h"
#include "socket_server.h"
#include "attendance.h"
#include "public.h"
#include "config.h"

static MainWindow *mainwindow;
user_info_t g_adding_user = {0};
unsigned char g_adding_index = 0;
int video_pause_flag = 0;
extern unsigned int g_atdin_time;
extern unsigned int g_atdout_time;

extern work_state_t g_work_state;
extern int face_train_flag;


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    int ret;

    ui->setupUi(this);

    /* set window title - 设置窗口标题 */
    setWindowTitle(DEFAULT_WINDOW_TITLE);

    ui->videoTxtLab->setHidden(true);
    ui->attendListBtn->setCheckable(true);
    ui->switchCamBtn->setCheckable(true);
    video_display_flag = 0;

    QDate date = QDate::currentDate();
    ui->attendInTime->setDate(date);
    ui->attendOutTime->setDate(date);

    /* 加载并显示背景图 */
    backgroundImg.load(BACKGROUND_IMAGE);
    ui->videoLab->setPixmap(QPixmap::fromImage(backgroundImg));

    user_sql_init(sql_db);

    user_info_t user_info;
    int total, cursor = 0;
    total = db_user_get_total();
    for(int i=0; i<total; i++)
    {
        QString item_str;
        char id_str[16] = {0};
        ret = db_user_traverse(&cursor, &user_info);
        if(ret != 0)
            break;

        sprintf(id_str, "%d", user_info.id);
        item_str = id_str + QString(", ") + user_info.name;
        ui->userListBox->addItem(item_str);
        qDebug() << i << ": "<< item_str;
    }

    init_table_view();
    ui->tableView->setHidden(true);
    ui->tableView->setModel(sqlmodel);

    display_timer = new QTimer(this);
    connect(display_timer, SIGNAL(timeout()), this, SLOT(window_display()));
    display_timer->start(TIMER_DISPLAY_INTERV_MS);

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::window_display(void)
{
    static rect_location_t location[MAX_FACE_NUM];
    static face_recogn_info_t recogn_info;
    static int face_rect_delay = FACE_DETECT_DELAY_TIME;
    static unsigned int counter = 0;
    static int old_frame_index = 0;
    static int face_num = 0;
    QImage videoQImage;
    QString videoStr;
    int frame_len = 0;
    int ret;

    display_timer->stop();

    if(video_display_flag && !video_pause_flag)
    {
        // 获取最新一帧图像
        ret = capture_get_newframe(videobuf, FRAME_BUF_SIZE, &frame_len);
        if(ret >0 && ret != old_frame_index)
        {
            old_frame_index = ret;
            //qDebug() << "frame index " << ret;
            videoQImage = jpeg_to_QImage(videobuf, frame_len);

            face_get_face_info(location, MAX_FACE_NUM, &ret, NULL);
            if(ret > 0)
            {
                face_num = ret;
                face_rect_delay = 0;
                face_set_face_info(NULL, 0, NULL);
            }

            if(face_rect_delay < FACE_DETECT_DELAY_TIME)
            {
                for(int i=0; i<face_num; i++)
                {
                    // 在图像中，框出人脸
                    QPainter painter(&videoQImage);
                    painter.setPen(QPen(Qt::green, 3, Qt::SolidLine, Qt::RoundCap));
                    painter.drawRect(location[0].x, location[0].y, location[0].w, location[0].h);
                }
                face_rect_delay ++;
                //printf("face[%d], %d\n", face_num, face_rect_delay);
            }

            // 显示一帧图像
            ui->videoLab->setPixmap(QPixmap::fromImage(videoQImage));
            ui->videoLab->show();
        }
    }

    switch(g_work_state)
    {
        case WORK_STA_NORMAL:
            counter = 0;
            break;

        case WORK_STA_ADD_USER:
            videoStr = QString("%1: %2/%3").arg(STR_ADDING_FACE).arg(g_adding_index).arg(FACE_IMAGE_MAX_NUM);
            ui->videoTxtLab->setText(videoStr);
            ui->videoTxtLab->setHidden(false);
            counter = 0;
            break;

        case WORK_STA_ADD_USER_OK:
            ui->videoTxtLab->setHidden(false);
            ui->videoTxtLab->setText(STR_ADD_FACE_OK);
            counter ++;
            if(counter > 200)
            {
                g_work_state = WORK_STA_NORMAL;
                capture_task_stop();
                video_display_flag = 0;
                ui->videoTxtLab->setHidden(true);
                ui->videoLab->setPixmap(QPixmap::fromImage(backgroundImg));
            }
            break;

        case WORK_STA_RECOGN_OK:
            if(counter == 0)
            {
                user_info_t user;
                face_get_face_info(NULL, 0, NULL, &recogn_info);
                ret = db_user_read(recogn_info.user_id, &user);
                if(ret == 0)
                {
                    videoStr = QString("%1: %2 - %3%").arg(STR_FACE_RECOGN_OK).arg(user.name).arg(recogn_info.score);
                    ui->videoTxtLab->setText(videoStr);
                    ui->videoTxtLab->setHidden(false);
                }
            }
            else if(counter > FACE_RECOGN_DELAY_TIME)
            {
                ui->videoTxtLab->setHidden(true);
                g_work_state = WORK_STA_NORMAL;
            }
            counter ++;

            break;
    }

    if(video_pause_flag)
        ui->videoTxtLab->setHidden(true);

    display_timer->start(TIMER_DISPLAY_INTERV_MS);

}

int MainWindow::init_table_view(void)
{
    bool bret;

    sqlmodel = new QSqlTableModel(this, sql_db);
    sqlmodel->setTable(TABLE_ATTENDANCE);

    bret = sqlmodel->select();
    if(bret == false)
    {
        qDebug() << "sqlmodel select failed!";
        return -1;
    }

    sqlmodel->setHeaderData(sqlmodel->fieldIndex(DB_COL_ID), Qt::Horizontal, STR_ID);
    sqlmodel->setHeaderData(sqlmodel->fieldIndex(DB_COL_NAME), Qt::Horizontal, STR_NAME);
    sqlmodel->setHeaderData(sqlmodel->fieldIndex(DB_COL_TIME_IN), Qt::Horizontal, STR_ATDIN_TIME);
    sqlmodel->setHeaderData(sqlmodel->fieldIndex(DB_COL_TIME_OUT), Qt::Horizontal, STR_ATDOUT_TIME);
    sqlmodel->setHeaderData(sqlmodel->fieldIndex(DB_COL_STATE), Qt::Horizontal, STR_ATTEND_STATE);

    return 0;
}


void MainWindow::on_addUserBtn_clicked()
{
    user_info_t user;
    QString str_id;
    QString str_name;
    QString str_user;

    str_id = ui->userIdEdit->text();
    str_name = ui->userNameEdit->text();
    if(str_id.length()<=0 || str_name.length()<=0)
    {
        qDebug() << "user id or name is null!";
        return ;
    }

    // toLocal8Bit(): Unicode编码
    user.id = atoi(str_id.toLocal8Bit().data());
    if(user.id <= 0)
    {
        qDebug() << "user id is illegal!";
        return ;
    }

    strcpy(user.name, str_name.toLocal8Bit().data());
    sprintf(user.facepath, "%s/%d_%s", FACE_LIB_PATH, user.id, user.name);

    printf("add user: %s\n", user.facepath);

    memcpy(&g_adding_user, &user, sizeof(user_info_t));

    str_user = str_id + QString(", ") +str_name;
    qDebug() << "user: " << str_user;

    ui->userListBox->addItem(str_user);

    g_adding_index = 0;
    g_work_state = WORK_STA_ADD_USER;
    video_display_flag = 1;

    start_capture_task();
}

void MainWindow::on_delUserBtn_clicked()
{
    QString str_user;
    user_info_t user;
    int index;
    int id;

    str_user = ui->userListBox->currentText();
    if(str_user.length() <= 0)
    {
        qDebug() << "user is null!";
        return ;
    }

    id = atoi(str_user.toLocal8Bit().data());
    db_user_read(id, &user);
    db_user_del(id);
    remove_dir(user.facepath);
    qDebug() << "del car user: " << str_user << ", id: " << id;

    face_train_flag = 1;

    index = ui->userListBox->currentIndex();
    ui->userListBox->removeItem(index);

}

void MainWindow::on_attendListBtn_clicked()
{
    if(ui->attendListBtn->isChecked())
    {
        sqlmodel->submit();
        sqlmodel->select();
        ui->tableView->setHidden(false);
        video_pause_flag = 1;
    }
    else
    {
        ui->tableView->setHidden(true);
        video_pause_flag = 0;
    }
}

void MainWindow::on_switchCamBtn_clicked()
{
    if(ui->switchCamBtn->isChecked())
    {
        proto_0x20_switchcamera(1);
        ui->tableView->setHidden(true);
        ui->attendListBtn->setChecked(false);
        video_pause_flag = 0;
        video_display_flag = 1;
    }
    else
    {
        proto_0x20_switchcamera(0);
        video_display_flag = 0;
        ui->videoLab->setPixmap(QPixmap::fromImage(backgroundImg));
    }
}

void MainWindow::on_setAtdTimeBtn_clicked()
{
    QDateTime timeAtdIn = ui->attendInTime->dateTime();
    QDateTime timeAtdOut = ui->attendOutTime->dateTime();

    g_atdin_time = timeAtdIn.toTime_t();
    g_atdout_time = timeAtdOut.toTime_t();

    if(g_atdin_time >= g_atdout_time)
        qDebug() << "error: set time illegal!";

}

void MainWindow::on_clearAtdTblBtn_clicked()
{
    db_attend_clear_data();
    sqlmodel->submit();
    sqlmodel->select();
}

/* main window initial - 主界面初始化 */
int mainwindow_init(void)
{
    mainwindow = new MainWindow;

    mainwindow->show();

    return 0;
}
