#ifndef ATTENDANCE_H
#define ATTENDANCE_H

#define TEXT_ATTEND_OK			"正常"
#define TEXT_ATTEND_NO_IN		"未签到"
#define TEXT_ATTEND_NO_OUT		"未签退"
#define TEXT_ATTEND_OUT_EARLY	"早退"
#define TEXT_ATTEND_IN_LATE		"迟到"
#define TEXT_ATTEND_NONE		"缺勤"

typedef enum {
    ATTEND_STA_OUT_OK,		// 正常
    ATTEND_STA_NO_IN,       // 未签到
    ATTEND_STA_NO_OUT,      // 未签退
    ATTEND_STA_OUT_EARLY,	// 早退
    ATTEND_STA_IN_LATE,		// 迟到
    ATTEND_STA_NONE,		// 缺勤
} attend_state_e;

attend_state_e attend_set_user(int id, char *name, unsigned int time);

#endif // ATTENDANCE_H
