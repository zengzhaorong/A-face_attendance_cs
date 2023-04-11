#ifndef SOCKET_CLIENT_H
#define SOCKET_CLIENT_H

#include <pthread.h>
#include <netinet/in.h>
#include "ringbuffer.h"


#define PROTO_HEAD_OFFSET		0
#define PROTO_VERIFY_OFFSET		(PROTO_HEAD_OFFSET +1)
#define PROTO_SEQ_OFFSET		(PROTO_VERIFY_OFFSET +4)
#define PROTO_CMD_OFFSET		(PROTO_SEQ_OFFSET +1)
#define PROTO_LEN_OFFSET		(PROTO_CMD_OFFSET +1)
#define PROTO_DATA_OFFSET		(PROTO_LEN_OFFSET +4)

#define PROTO_PACK_MAX_LEN		(1 *1024 *1024)
#define PROTO_PACK_MIN_LEN		(PROTO_DATA_OFFSET +1)

#define PROTO_HEAD		0xFF
#define PROTO_TAIL		0xFE
#define PROTO_VERIFY	"ABCD"

#define CLIENT_SENDBUF_SIZE     PROTO_PACK_MAX_LEN


typedef enum
{
    TCP_STATE_DISCONNECT,
    TCP_STATE_CONNECTED,
    TCP_STATE_LOGIN_OK,
}tcp_state_e;

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
    tcp_state_e tcp_state;
    struct sockaddr_in 	svr_addr;		// server ip addr
    pthread_mutex_t	send_mutex;
    struct ringbuffer recv_ringbuf;			// socket receive data ring buffer
    proto_detect_info_t detect_info;
    unsigned char tmp_buf[PROTO_PACK_MAX_LEN];
    unsigned char proto_buf[PROTO_PACK_MAX_LEN];		// protocol packet data buffer
    int proto_len;
    unsigned char ack_buf[PROTO_PACK_MAX_LEN];
} client_info_t;


int proto_0x21_sendframe(client_info_t *client, unsigned char format, void *frame, int len);

int start_socket_client_task(char *svr_ip);

#endif // SOCKET_CLIENT_H
