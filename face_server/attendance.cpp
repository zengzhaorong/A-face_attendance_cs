#include <stdio.h>
#include <string.h>
#include <QDateTime>
#include "user_sql.h"
#include "attendance.h"


unsigned int g_atdin_time = 0;
unsigned int g_atdout_time = 0;

attend_state_e attend_set_user(int id, char *name, unsigned int time)
{
    attend_state_e state = ATTEND_STA_NONE;
    QDateTime attendTime = QDateTime::fromTime_t(time);
    attend_info_t attend_info;
    unsigned int mid_time;
    int ret;

    mid_time = (g_atdin_time + g_atdout_time)/2;

    memset(&attend_info, 0, sizeof (attend_info_t));
    ret = db_attend_read(id, &attend_info);
    if(ret != 0)
    {
        printf("user[%d] attend info not exist.\n", id);
        attend_info.user_id = id;
        strncpy(attend_info.user_name, name, USER_NAME_LEN);
    }

    printf("user id: %d\n", attend_info.user_id);
    printf("%d : %d = %d, %d\n", g_atdin_time, g_atdout_time, mid_time, time);

    /* attend in: use the first time */
    if(time <= mid_time)
    {
        if(time <= g_atdin_time)
        {
            state = ATTEND_STA_NO_OUT;
            strcpy(attend_info.state, TEXT_ATTEND_NO_OUT);
            printf("user [id=%d]: attend in ok.\n", id);
        }
        else
        {
            state = ATTEND_STA_IN_LATE;
            strcpy(attend_info.state, TEXT_ATTEND_IN_LATE);
            printf("user [id=%d]: attend in late.\n", id);
        }
        if(strlen(attend_info.in_time) <= 0)
        {
            strcpy(attend_info.in_time, attendTime.toString("hh:mm:ss").toLocal8Bit().data());
            db_attend_write(&attend_info);
        }
    }
    /* attend out: use the last time */
    else
    {
        if(time < g_atdout_time)
        {
            state = ATTEND_STA_OUT_EARLY;
            memset(attend_info.state, 0, sizeof(attend_info.state));
            if(strlen(attend_info.in_time) <= 0)
            {
                sprintf(attend_info.state, "%s-%s", TEXT_ATTEND_NO_IN, TEXT_ATTEND_OUT_EARLY);
            }
            else
            {
                strcpy(attend_info.state, TEXT_ATTEND_OUT_EARLY);
            }
        }
        else
        {
            state = ATTEND_STA_OUT_OK;
            if(strlen(attend_info.in_time) <= 0)
            {
                strcpy(attend_info.state, TEXT_ATTEND_NO_IN);
            }
            else
            {
                if(!strcmp(attend_info.state, TEXT_ATTEND_IN_LATE))
                {
                    //strcpy(attend_info.state, TEXT_ATTEND_IN_LATE);
                }
                else
                {
                    strcpy(attend_info.state, TEXT_ATTEND_OK);
                }
            }
        }
        printf("user [id=%d]: attend out %s.\n", id, attend_info.state);

        /* add attend times */
        if(strlen(attend_info.out_time) <= 0)
        {
        }

        strcpy(attend_info.out_time, attendTime.toString("hh:mm:ss").toLocal8Bit().data());
        db_attend_write(&attend_info);
    }

    printf("%s: ok\n", __FUNCTION__);
    return state;
}

