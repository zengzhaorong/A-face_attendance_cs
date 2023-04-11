#ifndef USER_SQL_H
#define USER_SQL_H

#include <iostream>
#include <fstream>
#include <sstream>
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include <QSqlDatabase>

#define USER_NAME_LEN		32
#define DIR_PATH_LEN		64

#define USER_DB_NAME        "../face_server/user.db"

using namespace cv;
using namespace std;


#define TABLE_USER_INFO     "user_table"
#define TABLE_ATTENDANCE    "attend_table"

#define DB_COL_ID           "id"
#define DB_COL_NAME         "name"
#define DB_COL_TIME         "time"
#define DB_COL_FACEPATH		"facepath"
#define DB_COL_TIME_IN      "time_in"
#define DB_COL_TIME_OUT     "time_out"
#define DB_COL_STATE        "state"

#define FACE_LIB_PATH       "../face_server/faces"

typedef struct
{
    int id;
    char name[USER_NAME_LEN];
    char facepath[DIR_PATH_LEN];    // face path
}user_info_t;

typedef struct {
    int user_id;
    char user_name[USER_NAME_LEN];
    char in_time[32];
    char out_time[32];
    char state[32];
}attend_info_t;

int remove_dir(const char *dir);
int user_get_faceimg_label(vector<Mat>& images, vector<int>& labels);

int db_user_add(user_info_t *user);
int db_user_del(int id);
int db_user_read(int id, user_info_t *user);
int db_user_get_total(void);
int db_user_traverse(int *cursor, user_info_t *user_info);

int db_attend_write(attend_info_t *attend_info);
int db_attend_read(int id, attend_info_t *attend_info);
int db_attend_clear_data(void);

int sql_create_user_tbl(void);
int sql_create_attend_tbl(void);

int user_sql_init(QSqlDatabase &sql);

#endif // USER_SQL_H
