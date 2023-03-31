#ifndef CAPTURE_H
#define CAPTURE_H

#include "config.h"
#include <linux/videodev2.h>


#define QUE_BUF_MAX_NUM		5


struct buffer_info
{
    unsigned char *addr;
    int len;
};


struct v4l2cap_info
{
    int run;
    int fd;
    struct v4l2_format format;
    struct buffer_info buffer[QUE_BUF_MAX_NUM];
};


int start_capture_task(void);
int capture_task_stop(void);

/* 返回值：-1 出错，0-图像没有更新还是上一帧，>0 图像的编号（递增） */
int capture_get_newframe(unsigned char *data, int size, int *len);

int v4l2cap_update_newframe(unsigned char *data, int len);

int v4l2cap_clear_newframe(void);

int newframe_mem_init(void);


#endif // CAPTURE_H
