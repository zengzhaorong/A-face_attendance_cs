#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "capture.h"
#include "public.h"
#include "config.h"

static MainWindow *mainwindow;
extern work_state_t g_work_state;


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    /* set window title - 设置窗口标题 */
    setWindowTitle(DEFAULT_WINDOW_TITLE);

    ui->videoTxtLab->setStyleSheet("color:red");
    ui->videoTxtLab->setHidden(true);

    /* 加载并显示背景图 */
    QImage image;
    image.load(BACKGROUND_IMAGE);
    ui->videoLab->setPixmap(QPixmap::fromImage(image));


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
    static int face_rect_delay = FACE_DETECT_DELAY_TIME;
    static int old_frame_index = 0;
    static int disconn_flag = 0;
    static unsigned int counter = 0;
    static int face_num = 0;
    QImage videoQImage;
    QString videoStr;
    int frame_len = 0;
    int ret;

    display_timer->stop();

    // 获取最新一帧图像
    ret = capture_get_newframe(videobuf, FRAME_BUF_SIZE, &frame_len);
    if(ret > 0 && ret != old_frame_index)
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

    switch (g_work_state)
    {
        case WORK_STA_DISCONNECT:
            if(disconn_flag == 0)
            {
                ui->videoTxtLab->setText(STR_NOT_CONN_SERVER);
                ui->videoTxtLab->setHidden(false);
                disconn_flag = 1;
            }

            break;

        case WORK_STA_NORMAL:
            if(disconn_flag)
            {
                ui->videoTxtLab->setHidden(true);
                disconn_flag = 0;
            }
            break;

        case WORK_STA_RECOGN_OK:
            if(counter == 0)
            {
                recogn_info_t recogn_info;
                face_get_face_info(NULL, 0, NULL, &recogn_info);
                ui->videoTxtLab->setHidden(false);
                videoStr = QString("%1: %2 - %3%").arg(STR_FACE_RECOGN_OK).arg(recogn_info.user_name).arg(recogn_info.score);
                ui->videoTxtLab->setText(videoStr);
            }
            counter ++;
            if(counter > FACE_RECOGN_DELAY_TIME)
            {
                g_work_state = WORK_STA_NORMAL;
                ui->videoTxtLab->setHidden(true);
                counter = 0;
            }
            break;

        default:
            break;
    }

    display_timer->start(TIMER_DISPLAY_INTERV_MS);

}


/* main window initial - 主界面初始化 */
int mainwindow_init(void)
{
    mainwindow = new MainWindow;

    mainwindow->show();

    return 0;
}

