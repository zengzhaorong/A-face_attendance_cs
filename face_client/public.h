#ifndef PUBLIC_H
#define PUBLIC_H

#include <QImage>

#define UNUSED_1(a)         {(void)a;}
#define UNUSED_2(a,b)       {(void)a;(void)b;}
#define UNUSED_3(a,b,c)     {(void)a;(void)b;(void)c;}
#define UNUSED_4(a,b,c,d)   {(void)a;(void)b;(void)c;(void)d;}
#define UNUSED_5(a,b,c,d,e) {(void)a;(void)b;(void)c;(void)d;(void)e;}

#define MAX_FACE_NUM    10

typedef enum{
    WORK_STA_DISCONNECT,
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

typedef struct {
    int user_id;
    char user_name[32];
    int score;
}recogn_info_t;


int face_set_face_info(rect_location_t *locat_array, int face_num, recogn_info_t *recogn_info);
int face_get_face_info(rect_location_t *locat_array, int array_size, int *face_num, recogn_info_t *recogn_info);

QImage jpeg_to_QImage(unsigned char *data, int len);

#endif // PUBLIC_H
