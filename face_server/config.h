#ifndef CONFIG_H
#define CONFIG_H


// [WINDOW]
#define DEFAULT_WINDOW_TITLE		"人脸识别门禁系统"

// [CAPTURE]
#define DEFAULT_CAPTURE_DEV 		"/dev/video2"
#define DEFAULT_CAPTURE_WIDTH		640
#define DEFAULT_CAPTURE_HEIGH		480
#define FRAME_BUF_SIZE		(DEFAULT_CAPTURE_WIDTH *DEFAULT_CAPTURE_HEIGH *3)
#define IMAGE_BASE64_SIZE   ((FRAME_BUF_SIZE/3+1)*4)

// [SERVER]
#define DEFAULT_SERVER_PORT         9100     // 服务器端口

// [FACE]
#define DEFAULT_FACE_RECOGN_THRES_00		125.0	// confidence = 0%
#define DEFAULT_FACE_RECOGN_THRES_80		80.0	// confidence = 80%
#define DEFAULT_FACE_RECOGN_THRES_100		50.0	// confidence = 100%
#define DEFAULT_RECOGN_INTERVAL		        (3*1000)	// delay time after rocognize success
#define FACE_RECOGN_THRES 		DEFAULT_FACE_RECOGN_THRES_80	// face recognize threshold
#define FACE_IMAGE_MAX_NUM          10

#endif // CONFIG_H
