#ifndef SOCKET_SERVER_H
#define SOCKET_SERVER_H

#include <pthread.h>
#include <netinet/in.h>
#include "ringbuffer.h"
#include "opencv_face.h"
#include "lib_proto.h"

#define SVR_RECVBUF_SIZE			(PROTO_PACK_MAX_LEN*6)
#define SVR_SENDBUF_SIZE			PROTO_PACK_MAX_LEN

typedef struct
{
    char head;
    char verify;
    char tail;
    int len;
    int pack_len;
}proto_detect_info_t;

typedef struct {
    int fd;
    pthread_mutex_t	send_mutex;
    struct sockaddr_in sockaddr;
    struct ringbuffer recv_ringbuf;			// socket receive data ring buffer
    unsigned char tmp_buf[PROTO_PACK_MAX_LEN];
    unsigned char proto_buf[PROTO_PACK_MAX_LEN];		// protocol packet data buffer
    int proto_len;
    unsigned char ack_buf[PROTO_PACK_MAX_LEN];
} client_info_t;

typedef struct {
    int fd;
    struct sockaddr_in 	svr_addr;		// server ip addr
    client_info_t client;
} server_info_t;


int proto_0x10_getframe(void);
int proto_0x11_facedetect(uint8_t count, rect_location_t *face_locat);
int proto_0x12_facerecogn(int face_id, uint8_t confidence, char *face_name);
int proto_0x20_switchcamera(unsigned char onoff);

int start_socket_server_task(void);


#endif // SOCKET_SERVER_H
