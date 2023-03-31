#ifndef OPENCV_FACE_H
#define OPENCV_FACE_H

#include "opencv2/core.hpp"
#include "opencv2/objdetect.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/face.hpp"
#include <semaphore.h>
#include "public.h"

using namespace std;
using namespace cv;
using namespace cv::face;


#define FRONTAL_FACE_HAAR_XML "../face_server/haarcascade_frontalface_alt.xml"
#define MAX_FACE_NUM    10
#define MIN_FACE_PIXEL  100     // 最小人脸像素，小于过滤丢掉


typedef struct {
    int face_num;
    rect_location_t locat[MAX_FACE_NUM];
}face_detect_info_t;

typedef struct {
    int user_id;
    int score;
}face_recogn_info_t;

class opencv_face_detect
{
public:
    opencv_face_detect(void);
    int face_detect_init(void);
    int face_detect(Mat& img, CascadeClassifier& cascade, double scale, bool tryflip, Mat& gray_face, vector<Rect> &faces);

public:
    unsigned char *frame_buf;
    int frame_size;
    CascadeClassifier face_cascade;
};

class opencv_face_recogn
{
private:

public:
    opencv_face_recogn(void);
    int face_recogn_init(void);
    int face_recogn(Mat &face_mat, int *face_id, uint8_t *confid);

public:
    Ptr<LBPHFaceRecognizer> mod_LBPH;
    Mat face_mat;
    sem_t recogn_sem;
    int recogn_valid;	// 0-valid 1-invalid
};

int face_set_face_info(rect_location_t *locat_array, int face_num, face_recogn_info_t *recogn_info);
int face_get_face_info(rect_location_t *locat_array, int array_size, int *face_num, face_recogn_info_t *recogn_info);

int start_face_process_task(void);

#endif // OPENCV_FACE_H
