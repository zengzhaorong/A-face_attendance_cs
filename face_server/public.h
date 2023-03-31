#ifndef PUBLIC_H
#define PUBLIC_H

#include <QImage>
#include <opencv2/imgproc/types_c.h>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>

using namespace std;
using namespace cv;

typedef enum{
    WORK_STA_NORMAL,
    WORK_STA_ADD_USER,
    WORK_STA_ADD_USER_OK,
    WORK_STA_RECOGN_OK,
}work_state_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
}rect_location_t;

QImage jpeg_to_QImage(unsigned char *data, int len);
cv::Mat QImage_to_cvMat(QImage qimage);
QImage cvMat_to_QImage(const cv::Mat& mat);


#endif // PUBLIC_H
