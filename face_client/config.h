#ifndef CONFIG_H
#define CONFIG_H


// [WINDOW]
#define DEFAULT_WINDOW_TITLE		"人脸识别考勤系统"

// [CAPTURE]
#define DEFAULT_CAPTURE_DEV 		"/dev/video0"
#define DEFAULT_CAPTURE_WIDTH		640
#define DEFAULT_CAPTURE_HEIGH		480
#define FRAME_BUF_SIZE		(DEFAULT_CAPTURE_WIDTH *DEFAULT_CAPTURE_HEIGH *3)
#define IMAGE_BASE64_SIZE   ((FRAME_BUF_SIZE/3+1)*4)

// [SERVER]
#define DEFAULT_SERVER_IP           "127.0.0.1"  //  服务器IP
#define DEFAULT_SERVER_PORT         9100     // 服务器端口


#endif // CONFIG_H
