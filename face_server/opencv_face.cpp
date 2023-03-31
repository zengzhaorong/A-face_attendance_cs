#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <opencv2/opencv.hpp>
#include "opencv_face.h"
#include "capture.h"
#include "user_sql.h"
#include "socket_server.h"
#include "attendance.h"
#include <QTime>
#include "public.h"
#include "config.h"

class opencv_face_detect face_detect_unit;
class opencv_face_recogn face_recogn_unit;

static face_detect_info_t new_detect_info;
static face_recogn_info_t new_recogn_info;
int face_train_flag = 0;
sem_t g_getframe_sem;

extern work_state_t g_work_state;
extern user_info_t g_adding_user;
extern unsigned char g_adding_index;


int face_set_face_info(rect_location_t *locat_array, int face_num, face_recogn_info_t *recogn_info)
{
    face_detect_info_t *detect = &new_detect_info;
    int i;

    for(i=0; i<face_num; i++)
    {
        detect->locat[i] = locat_array[i];
    }
    detect->face_num = face_num;

    if(recogn_info)
    {
        memcpy(&new_recogn_info, recogn_info, sizeof(face_recogn_info_t));
    }

    return 0;
}

/* >0 人脸数量，<=0 失败，无人脸 */
int face_get_face_info(rect_location_t *locat_array, int array_size, int *face_num, face_recogn_info_t *recogn_info)
{
    if(array_size>0 && face_num)
    {
        if(array_size <= new_detect_info.face_num)
            *face_num = array_size;
        else
            *face_num = new_detect_info.face_num;

        for(int i=0; i<*face_num; i++)
        {
            locat_array[i] = new_detect_info.locat[i];
        }
    }

    if(recogn_info)
    {
        memcpy(recogn_info, &new_recogn_info, sizeof(face_recogn_info_t));
    }

    return 0;
}

int face_resize_save(Mat& faceImg, char *path, int index)
{
    string file_path;
    Mat faceSave;
    int ret;

    if(faceImg.empty())
        return -1;

    if(faceImg.cols < 100)
    {
        printf("face image is too small, skip!\n");
        return -1;
    }

    resize(faceImg, faceSave, Size(92, 112));
    file_path = format("%s/%d.png", path, index);
    ret = imwrite(file_path, faceSave);
    if(ret == false)
        return -1;

    printf("[Add user]***save face: %s\n", file_path.c_str());

    return 0;
}


opencv_face_detect::opencv_face_detect(void)
{
    printf("%s: enter ++\n", __FUNCTION__);
}

int opencv_face_detect::face_detect_init(void)
{
    bool bret;
    int ret;

    this->frame_size = FRAME_BUF_SIZE;
    this->frame_buf = (unsigned char *)malloc(this->frame_size);
    if(this->frame_buf == NULL)
        return -1;

    bret = this->face_cascade.load(FRONTAL_FACE_HAAR_XML);
    if(bret == false)
    {
        printf("%s: load [%s] failed !\n", __FUNCTION__, FRONTAL_FACE_HAAR_XML);
        return -1;
    }

    return 0;
}

int opencv_face_detect::face_detect(Mat& img, CascadeClassifier& cascade, double scale, bool tryflip, Mat& gray_face, vector<Rect> &faces)
{
    double t = 0;
    vector<Rect> faces2;
    Mat gray, smallImg;

    cvtColor( img, gray, COLOR_BGR2GRAY );
    double fx = 1 / scale;
    resize( gray, smallImg, Size(), fx, fx, CV_INTER_LINEAR );
    equalizeHist( smallImg, smallImg );

    t = (double)getTickCount();
    cascade.detectMultiScale( smallImg, faces,
        1.1, 2, 0
        //|CASCADE_FIND_BIGGEST_OBJECT
        //|CASCADE_DO_ROUGH_SEARCH
        |CASCADE_SCALE_IMAGE,
        Size(30, 30) );

    if(faces.size() <= 0)
        return -1;

    if( tryflip )
    {
        flip(smallImg, smallImg, 1);
        cascade.detectMultiScale( smallImg, faces2,
                                 1.1, 2, 0
                                 //|CASCADE_FIND_BIGGEST_OBJECT
                                 //|CASCADE_DO_ROUGH_SEARCH
                                 |CASCADE_SCALE_IMAGE,
                                 Size(30, 30) );
        for( vector<Rect>::const_iterator r = faces2.begin(); r != faces2.end(); ++r )
        {
            faces.push_back(Rect(smallImg.cols - r->x - r->width, r->y, r->width, r->height));
        }
    }
    t = (double)getTickCount() - t;
    //printf( "detection time = %g ms\n", t*1000/getTickFrequency());

    /* restore face size */
    for(uint32_t i=0; i<faces.size(); i++)
    {
        faces[i].x *= scale;
        faces[i].y *= scale;
        faces[i].width *= scale;
        faces[i].height *= scale;
    }

    gray_face = gray(faces[0]);

    return faces.size();
}

int face_database_train(void)
{
    vector<Mat> images;
    vector<int> labels;
    int ret;

    ret = user_get_faceimg_label(images, labels);
    if(ret<0 || images.size() <= 0)
    {
        printf("%s: user_get_faceimg_label no images.\n", __FUNCTION__);
        goto ERR_TRAIN;
    }

    face_recogn_unit.mod_LBPH->train(images, labels);

    face_recogn_unit.mod_LBPH->setThreshold(FACE_RECOGN_THRES);

    printf("face_database_retrain successfully.\n");

    return 0;

ERR_TRAIN:
    face_recogn_unit.recogn_valid = 0;

    return -1;
}

opencv_face_recogn::opencv_face_recogn(void)
{
    printf("%s: enter ++\n", __FUNCTION__);
}

int opencv_face_recogn::face_recogn_init(void)
{
    string fdb_csv;
    vector<Mat> images;
    vector<int> labels;
    int ret;

    ret = sem_init(&this->recogn_sem, 0, 0);
    if(ret != 0)
    {
        return -1;
    }

#if defined(OPENCV_VER_3_X_X)
    this->mod_LBPH = createLBPHFaceRecognizer();
#else
    this->mod_LBPH = LBPHFaceRecognizer::create();
#endif
    recogn_valid = 0;

    ret = face_database_train();
    if(ret < 0)
    {
        printf("%s: Face database maybe is 0.\n", __FUNCTION__);
        return -1;
    }
    printf("%s: Face model train success.\n", __FUNCTION__);

    /* wether save model file */
    //this->mod_LBPH->write("MyFaceLBPHModel.xml");
    //this->mod_LBPH->read("MyFaceLBPHModel.xml");

    this->mod_LBPH->setThreshold(FACE_RECOGN_THRES);

    recogn_valid = 1;

    return 0;
}

int opencv_face_recogn::face_recogn(Mat &face_mat, int *face_id, uint8_t *confid)
{
    Mat recogn_mat;
    int predict;
    double confidence = 0.0;
    double recoThres00, recoThres80, recoThres100;

    if(face_mat.empty())
    {
        printf("ERROR: face mat is empty!\n");
        return -1;
    }

    resize(face_mat, recogn_mat, Size(92, 112));
    if(recogn_mat.empty())
        printf("recogn_mat is empty!!!\n");

    face_recogn_unit.mod_LBPH->predict(recogn_mat , predict, confidence);

    if(predict < 0)
        return -1;

    *face_id = predict;

    recoThres00 = DEFAULT_FACE_RECOGN_THRES_00;
    recoThres80 = DEFAULT_FACE_RECOGN_THRES_80;
    recoThres100 = DEFAULT_FACE_RECOGN_THRES_100;

    /* calculate confidence */
    if(confidence < recoThres100)
    {
        *confid = 100;
    }
    else if(confidence < recoThres80)
    {
        *confid = 80 +(recoThres80 -confidence)*20/(recoThres80 -recoThres100);
    }
    else if(confidence < recoThres00)
    {
        *confid = (recoThres00 -confidence)*80 /(recoThres00 -recoThres80);
    }
    else
    {
        *confid = 0;
    }

    printf("[recogn]*** predict: %d, confidence: %f = %d%%\n", predict, confidence, *confid);

    return 0;
}


/* get frame from detect buffer */
int face_get_frame_detect(unsigned char *buf, int size)
{
    struct timespec ts;
    int len = 0;
    int ret;

    if(g_work_state != WORK_STA_ADD_USER)
    {
        proto_0x10_getframe();

        if(clock_gettime(CLOCK_REALTIME, &ts) == -1)
        {
            printf("clock_gettime failed!\n");
            return -1;
        }

        int value = -1;
        sem_getvalue(&g_getframe_sem, &value);
        if(value > 0)
        {
            sem_wait(&g_getframe_sem);
            printf("sem wait, value: %d\n", value);
        }
        ts.tv_sec += 3;
        ret = sem_timedwait(&g_getframe_sem, &ts);
        //ret = sem_trywait(&g_getframe_sem);
        if(ret != 0)
        {
            return -1;
        }

    }

    capture_get_newframe(buf, size, &len);

    return len;
}

/* put frame to recognize buffer */
int face_put_frame_recogn(Mat& face_mat)
{
    int value = 0;

    if(face_mat.empty())
        return -1;

    face_recogn_unit.face_mat = face_mat;

    sem_getvalue(&face_recogn_unit.recogn_sem, &value);

    if(value <= 0)
        sem_post(&face_recogn_unit.recogn_sem);

    return 0;
}

/* get frame from recognize buffer */
int face_get_frame_recogn(Mat& face_mat)
{
    struct timespec ts;
    int ret;

    if(clock_gettime(CLOCK_REALTIME, &ts) == -1)
    {
        return -1;
    }

    ts.tv_sec += 3;
    ret = sem_timedwait(&face_recogn_unit.recogn_sem, &ts);
    //ret = sem_trywait(&face_detect_unit.detect_sem);
    if(ret != 0)
        return -1;

    face_mat = face_recogn_unit.face_mat;

    return 0;
}


void *opencv_face_detect_thread(void *arg)
{
    class opencv_face_detect *detect_unit = &face_detect_unit;
    Mat detectMat, face_mat;
    QImage qImage;
    vector<Rect> faces;
    int frame_len = 0;
    int ret;

    printf("%s enter ++\n", __FUNCTION__);

    ret = detect_unit->face_detect_init();
    if(ret != 0)
    {
        printf("%s: face_detect_init failed !\n", __FUNCTION__);
        return NULL;
    }

    sem_init(&g_getframe_sem, 0, 0);

    while(1)
    {

        //获取最新一帧图像用于检测人脸
        frame_len = face_get_frame_detect(detect_unit->frame_buf, detect_unit->frame_size);
        if(frame_len <= 0)
        {
            usleep(10*1000);
            continue;
        }

        /* convert v4l2 data to qimage */
        qImage = jpeg_to_QImage(detect_unit->frame_buf, frame_len);
        if(qImage.isNull())
        {
            printf("ERROR: qImage is null !\n");
            continue;
        }

        /* convert qimage to cvMat */
        detectMat = QImage_to_cvMat(qImage).clone();
        if(detectMat.empty())
        {
            printf("ERROR: detectMat is empty\n");
            continue;
        }

        ret = detect_unit->face_detect(detectMat, face_detect_unit.face_cascade, 3, 0, face_mat, faces);
        if(ret > 0)
        {
            printf("face: x=%d, y=%d, w=%d, h=%d\n", faces[0].x, faces[0].y, faces[0].width, faces[0].height);
            if(faces[0].width < MIN_FACE_PIXEL || faces[0].height < MIN_FACE_PIXEL)
            {
                printf("face too small, ignore ...\n");
                continue;
            }
            rect_location_t locat;
            locat.x = faces[0].x;
            locat.y = faces[0].y;
            locat.w = faces[0].width;
            locat.h = faces[0].height;

            if(g_work_state == WORK_STA_NORMAL)
            {
                proto_0x11_facedetect(1, &locat);
                face_put_frame_recogn(face_mat);
            }
            else if(g_work_state == WORK_STA_ADD_USER)
            {
                face_set_face_info(&locat, 1, NULL);

                if(g_adding_index == 0)
                {
                    remove_dir(g_adding_user.facepath);
                    ret = mkdir(g_adding_user.facepath, 0777);
                    if(ret != 0)
                    {
                        printf("create dir failed!\n");
                        continue;
                    }
                }
                ret = face_resize_save(face_mat, g_adding_user.facepath, g_adding_index);
                if(ret != 0)
                {
                    printf("face resize save failed!\n");
                    continue;
                }

                g_adding_index ++;
                usleep(500 *1000);

                if(g_adding_index >= FACE_IMAGE_MAX_NUM)
                {
                    db_user_add(&g_adding_user);

                    face_train_flag = 1;

                    g_work_state = WORK_STA_ADD_USER_OK;

                    printf("--------add user success.\n");
                }
            }
            else
            {
            }

        }

        usleep(100*1000);
    }
}

void *opencv_face_recogn_thread(void *arg)
{
    class opencv_face_recogn *recogn_unit = &face_recogn_unit;
    Mat face_mat;
    int face_id;
    uint8_t confidence;
    int ret;

    printf("%s enter ++\n", __FUNCTION__);

    /* do not return if failed, because it will init again after add face. */
    recogn_unit->face_recogn_init();

    while(1)
    {
        if(face_train_flag)
        {
            recogn_unit->recogn_valid = 0;
            ret = face_database_train();
            if(ret == 0)
            {
                recogn_unit->recogn_valid = 1;
            }
            face_train_flag = 0;
        }

        if(!recogn_unit->recogn_valid || g_work_state!=WORK_STA_NORMAL)
        {
            usleep(100 *1000);
            continue;
        }

        /* get one face frame to recognize */
        ret = face_get_frame_recogn(face_mat);
        if(ret != 0)
        {
            continue;
        }

        ret = recogn_unit->face_recogn(face_mat, &face_id, &confidence);
        if(ret==0 && confidence>=80)
        {
            rect_location_t locat_array[MAX_FACE_NUM];
            face_recogn_info_t recogn_info;
            int face_num;
            face_get_face_info(locat_array, MAX_FACE_NUM, &face_num, &recogn_info);

            recogn_info.user_id = face_id;
            recogn_info.score = confidence;
            printf("face recogn: face %d, score %d\n", face_id, confidence);
            face_set_face_info(locat_array, face_num, &recogn_info);
            g_work_state = WORK_STA_RECOGN_OK;

            // 添加识别记录
            user_info_t user;
            attend_info_t attend_info;
            memset(&attend_info, 0, sizeof(attend_info_t));
            db_user_read(face_id, &user);
            QDateTime time = QDateTime::currentDateTime();
            attend_set_user(user.id, user.name, time.toTime_t());

            proto_0x12_facerecogn(face_id, confidence, user.name);
        }

        sleep(1);
    }
}

int start_face_process_task(void)
{
    pthread_t tid;
    int ret;

    ret = pthread_create(&tid, NULL, opencv_face_detect_thread, NULL);
    if(ret != 0)
    {
        return -1;
    }

    ret = pthread_create(&tid, NULL, opencv_face_recogn_thread, NULL);
    if(ret != 0)
    {
        return -1;
    }

    return 0;
}
