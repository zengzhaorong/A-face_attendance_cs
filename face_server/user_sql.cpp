#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include "user_sql.h"
#include <pthread.h>
#include "public.h"
#include "config.h"

using namespace cv;
using namespace std;

static QSqlQuery* sqlquery;
static pthread_mutex_t	sql_mutex;
extern work_state_t g_work_state;

// remove dir not empty
int remove_dir(const char *dir)
{
    char cur_dir[] = ".";
    char up_dir[] = "..";
    char dir_name[128];
    DIR *dirp;
    struct dirent *dp;
    struct stat dir_stat;

    // dir not exist
    if ( 0 != access(dir, F_OK) ) {
        return 0;
    }

    if ( 0 > stat(dir, &dir_stat) ) {
        perror("get directory stat error");
        return -1;
    }

    if ( S_ISREG(dir_stat.st_mode) ) {  // file
        remove(dir);
    } else if ( S_ISDIR(dir_stat.st_mode) ) {   // dir
        dirp = opendir(dir);
        while ( (dp=readdir(dirp)) != NULL ) {
            //  . & ..
            if ( (0 == strcmp(cur_dir, dp->d_name)) || (0 == strcmp(up_dir, dp->d_name)) ) {
                continue;
            }

            memset(dir_name, 0, sizeof(dir_name));
            strcat(dir_name, dir);
            strcat(dir_name, "/");
            strcat(dir_name, dp->d_name);
            //sprintf(dir_name, "%s/%s", dir, dp->d_name);
            remove_dir(dir_name);   // recursive call
        }
        closedir(dirp);

        rmdir(dir);     // delete empty dir
    } else {
        perror("unknow file type!");
    }

    return 0;
}

/* get user face images and label(face id)*/
int user_get_faceimg_label(vector<Mat>& images, vector<int>& labels)
{
    user_info_t user;
    string img_path;
    int total, cursor = 0;
    int i, j, ret;

    total = db_user_get_total();
    for(i=0; i<total +1; i++)
    {
        ret = db_user_traverse(&cursor, &user);
        if(ret != 0)
            break;

        for(j=1; j<FACE_IMAGE_MAX_NUM; j++)
        {
            stringstream sstream;
            sstream << user.facepath << "/" << j << ".png";
            sstream >> img_path;
            /* check file if exist */
            if(access(img_path.c_str(), R_OK) == -1)
            {
                printf("%s: Warning: file[%s] not exist.\n", __FUNCTION__, img_path.c_str());
                continue;
            }

            images.push_back(imread(img_path, 0));
            labels.push_back(user.id);
        }
    }

    return 0;
}

int db_user_add(user_info_t *user)
{
    QString sql_cmd;

    pthread_mutex_lock(&sql_mutex);

    sql_cmd = QString("insert into %1(%2,%3,%4) values(%5,'%6','%7');").arg(TABLE_USER_INFO).arg(DB_COL_ID).arg(DB_COL_NAME).arg(DB_COL_FACEPATH).arg(user->id).arg(user->name).arg(user->facepath);
    qDebug() << sql_cmd;
    if(sqlquery->exec(sql_cmd))
        printf("%s: ok\n", __FUNCTION__);
    else
    {
        printf("%s: failed!\n", __FUNCTION__);
        pthread_mutex_unlock(&sql_mutex);
        return -1;
    }

    pthread_mutex_unlock(&sql_mutex);

    return 0;
}

int db_user_del(int id)
{
    QString sql_cmd;

    pthread_mutex_lock(&sql_mutex);

    sql_cmd = QString("delete from %1 where %2=%3;").arg(TABLE_USER_INFO).arg(DB_COL_ID).arg(id);
    qDebug() << sql_cmd;
    if(sqlquery->exec(sql_cmd))
        printf("%s: ok\n", __FUNCTION__);
    else
        printf("%s: failed!\n", __FUNCTION__);

    pthread_mutex_unlock(&sql_mutex);

    return 0;
}

int db_user_read(int id, user_info_t *user)
{
    QString sql_cmd;
    int ret = -1;

    pthread_mutex_lock(&sql_mutex);

    sql_cmd = QString("select * from %1 where %2=%3;").arg(TABLE_USER_INFO).arg(DB_COL_ID).arg(id);
    qDebug() << sql_cmd;
    if(sqlquery->exec(sql_cmd))
        printf("%s: ok\n", __FUNCTION__);
    else
    {
        printf("%s: failed!\n", __FUNCTION__);
        pthread_mutex_unlock(&sql_mutex);
        return -1;
    }

    bool bret = sqlquery->next();
    if(bret)
    {
        user->id = sqlquery->value(0).toInt();
        strcpy(user->name, (char *)sqlquery->value(1).toString().toLocal8Bit().data());
        strcpy(user->facepath, (char *)sqlquery->value(2).toString().toLocal8Bit().data());
        ret = 0;
    }

    pthread_mutex_unlock(&sql_mutex);

    return ret;
}

int db_user_get_total(void)
{
    QString sql_cmd;
    int total = 0;

    pthread_mutex_lock(&sql_mutex);

    sql_cmd = QString("select count(*) from %1").arg(TABLE_USER_INFO);
    qDebug() << sql_cmd;
    if(sqlquery->exec(sql_cmd) == false)
    {
        printf("%s: failed!\n", __FUNCTION__);
        return 0;
    }

    sqlquery->next();
    total = sqlquery->value(0).toInt();

    pthread_mutex_unlock(&sql_mutex);

    return total;
}

int db_user_traverse(int *cursor, user_info_t *user_info)
{
    QString sql_cmd;

    pthread_mutex_lock(&sql_mutex);

    if(*cursor == 0)
    {
        sql_cmd = QString("select * from %1").arg(TABLE_USER_INFO);
        qDebug() << sql_cmd;
        if(sqlquery->exec(sql_cmd) == false)
        {
            printf("%s: failed!\n", __FUNCTION__);
            pthread_mutex_unlock(&sql_mutex);
            return -1;
        }
        (*cursor) ++;
    }

    if(sqlquery->next())
    {
        memset(user_info, 0, sizeof(user_info_t));
        user_info->id = sqlquery->value(0).toInt();
        strcpy(user_info->name, sqlquery->value(1).toString().toLocal8Bit().data());
        strcpy(user_info->facepath, sqlquery->value(2).toString().toLocal8Bit().data());
    }
    else
    {
        pthread_mutex_unlock(&sql_mutex);
        return -1;
    }

    pthread_mutex_unlock(&sql_mutex);

    return 0;
}

/* ------------------------------ attendance table ------------------------------ */

int db_attend_update(attend_info_t *attend_info)
{
    QString sql_cmd;
    char atdin_time[32] = "NULL";
    char atdout_time[32] = "NULL";

    if(strlen(attend_info->in_time) > 0)
    {
        sprintf(atdin_time, "'%s'", attend_info->in_time);
    }
    if(strlen(attend_info->out_time) > 0)
    {
        sprintf(atdout_time, "'%s'", attend_info->out_time);
    }

    pthread_mutex_lock(&sql_mutex);

    sql_cmd = QString("update %1 set %2=%3,%4=%5,%6='%7' where %8=%9;").arg(TABLE_ATTENDANCE).arg(DB_COL_TIME_IN).arg(atdin_time).arg(DB_COL_TIME_OUT).arg(atdout_time).arg(DB_COL_STATE).arg(attend_info->state).arg(DB_COL_ID).arg(attend_info->user_id);
    qDebug() << sql_cmd;
    if(sqlquery->exec(sql_cmd))
        printf("%s: ok\n", __FUNCTION__);
    else
    {
        printf("%s: failed!\n", __FUNCTION__);
        qDebug() << sqlquery->lastError().text();
        pthread_mutex_unlock(&sql_mutex);
        return -1;
    }

    pthread_mutex_unlock(&sql_mutex);

    return 0;
}

int db_attend_write(attend_info_t *attend_info)
{
    attend_info_t attend_r;
    QString sql_cmd;
    char atdin_time[32] = "NULL";
    char atdout_time[32] = "NULL";

    printf("write attend user id: %d\n", attend_info->user_id);

    if(db_attend_read(attend_info->user_id, &attend_r) == 0)
    {
        db_attend_update(attend_info);
        return 0;
    }

    if(strlen(attend_info->in_time) > 0)
    {
        sprintf(atdin_time, "'%s'", attend_info->in_time);
    }
    if(strlen(attend_info->out_time) > 0)
    {
        sprintf(atdout_time, "'%s'", attend_info->out_time);
    }

    pthread_mutex_lock(&sql_mutex);

    sql_cmd = QString("insert into %1(%2,%3,%4,%5,%6) values(%7,'%8',%9,%10,'%11');").arg(TABLE_ATTENDANCE).arg(DB_COL_ID).arg(DB_COL_NAME).arg(DB_COL_TIME_IN).arg(DB_COL_TIME_OUT).arg(DB_COL_STATE).arg(attend_info->user_id).arg(attend_info->user_name).arg(atdin_time).arg(atdout_time).arg(attend_info->state);
    qDebug() << sql_cmd;
    if(sqlquery->exec(sql_cmd))
        printf("%s: ok\n", __FUNCTION__);
    else
    {
        printf("%s: failed!\n", __FUNCTION__);
        qDebug() << sqlquery->lastError().text();
        pthread_mutex_unlock(&sql_mutex);
        return -1;
    }

    pthread_mutex_unlock(&sql_mutex);

    return 0;
}

int db_attend_read(int id, attend_info_t *attend_info)
{
    QString sql_cmd;
    int ret = -1;

    pthread_mutex_lock(&sql_mutex);

    sql_cmd = QString("select * from %1 where %2=%3;").arg(TABLE_ATTENDANCE).arg(DB_COL_ID).arg(id);
    qDebug() << sql_cmd;
    if(sqlquery->exec(sql_cmd))
        printf("%s: ok\n", __FUNCTION__);
    else
    {
        printf("%s: failed!\n", __FUNCTION__);
        pthread_mutex_unlock(&sql_mutex);
        return -1;
    }

    bool bret = sqlquery->next();
    if(bret)
    {
        attend_info->user_id = sqlquery->value(0).toInt();
        strcpy(attend_info->user_name, (char *)sqlquery->value(1).toString().toLocal8Bit().data());
        strcpy(attend_info->in_time, (char *)sqlquery->value(2).toString().toLocal8Bit().data());
        strcpy(attend_info->out_time, (char *)sqlquery->value(3).toString().toLocal8Bit().data());
        strcpy(attend_info->state, (char *)sqlquery->value(4).toString().toLocal8Bit().data());
        ret = 0;
    }

    pthread_mutex_unlock(&sql_mutex);

    return ret;
}

int db_attend_clear_data(void)
{
    QString sql_cmd;

    pthread_mutex_lock(&sql_mutex);

    sql_cmd = QString("delete from %1").arg(TABLE_ATTENDANCE);
    qDebug() << sql_cmd;
    if(sqlquery->exec(sql_cmd) == false)
    {
        printf("%s: failed!\n", __FUNCTION__);
        pthread_mutex_unlock(&sql_mutex);
        return 0;
    }

    pthread_mutex_unlock(&sql_mutex);
}

int sql_create_user_tbl(void)
{
    QString sql_cmd;

    pthread_mutex_lock(&sql_mutex);

    sql_cmd = QString("create table if not exists %1(%2 int primary key not null,%3 char(32), %4 text);").arg(TABLE_USER_INFO).arg(DB_COL_ID).arg(DB_COL_NAME).arg(DB_COL_FACEPATH);
    qDebug() << sql_cmd;
    if(sqlquery->exec(sql_cmd))
        printf("%s: ok!\n", __FUNCTION__);
    else
    {
        printf("%s: failed!\n", __FUNCTION__);
        pthread_mutex_unlock(&sql_mutex);
        return -1;
    }

    pthread_mutex_unlock(&sql_mutex);

    return 0;
}

int sql_create_attend_tbl(void)
{
    QString sql_cmd;

    pthread_mutex_lock(&sql_mutex);

    sql_cmd = QString("create table if not exists %1(%2 int primary key not null,%3 char(32),%4 char(32),%5 char(32),%6 char(32));").arg(TABLE_ATTENDANCE).arg(DB_COL_ID).arg(DB_COL_NAME).arg(DB_COL_TIME_IN).arg(DB_COL_TIME_OUT).arg(DB_COL_STATE);
    qDebug() << sql_cmd;
    if(sqlquery->exec(sql_cmd))
        cout << "create attend table success." ;
    else
    {
        printf("%s: failed!\n", __FUNCTION__);
        pthread_mutex_unlock(&sql_mutex);
        return -1;
    }

    pthread_mutex_unlock(&sql_mutex);

    return 0;
}

int user_sql_init(QSqlDatabase &sql)
{
    sql = QSqlDatabase::addDatabase("QSQLITE");
    sql.setDatabaseName(USER_DB_NAME);
    if(!sql.open())
    {
        cout << "fail to open sqlite!" << endl;
    }

    sqlquery = new QSqlQuery(sql);

    sql_create_user_tbl();

    sql_create_attend_tbl();

    pthread_mutex_init(&sql_mutex, NULL);

    qDebug() << "sql db init ok." ;

    return 0;
}
