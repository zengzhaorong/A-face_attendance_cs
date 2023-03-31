#include "stdio.h"
#include "string.h"
#include "public.h"



rect_location_t new_face_rect[MAX_FACE_NUM];
int g_face_num = 0;
recogn_info_t new_face_recogn;

int face_set_face_info(rect_location_t *locat_array, int face_num, recogn_info_t *recogn_info)
{
    int i;

    for(i=0; i<face_num; i++)
    {
        new_face_rect[i] = locat_array[i];
    }
    g_face_num = face_num;

    if(recogn_info)
    {
        memcpy(&new_face_recogn, recogn_info, sizeof(recogn_info_t));
    }

    return 0;
}

/* >0 人脸数量，<=0 失败，无人脸 */
int face_get_face_info(rect_location_t *locat_array, int array_size, int *face_num, recogn_info_t *recogn_info)
{
    if(array_size>0 && face_num)
    {
        if(array_size <= g_face_num)
            *face_num = array_size;
        else
            *face_num = g_face_num;

        for(int i=0; i<*face_num; i++)
        {
            locat_array[i] = new_face_rect[i];
        }
    }

    if(recogn_info)
    {
        memcpy(recogn_info, &new_face_recogn, sizeof(recogn_info_t));
    }

    return 0;
}

/*  将jpge/mjpge格式转换为QImage */
QImage jpeg_to_QImage(unsigned char *data, int len)
{
    QImage qtImage;

    if(data==NULL || len<=0)
        return qtImage;

    qtImage.loadFromData(data, len);
    if(qtImage.isNull())
    {
        printf("ERROR: %s: QImage is null !\n", __FUNCTION__);
    }

    return qtImage;
}

