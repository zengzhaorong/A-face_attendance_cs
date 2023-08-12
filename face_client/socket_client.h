#ifndef SOCKET_CLIENT_H
#define SOCKET_CLIENT_H

#include <pthread.h>
#include <netinet/in.h>
#include "ringbuffer.h"
#include "lib_proto.h"


#define CLIENT_SENDBUF_SIZE     PROTO_PACK_MAX_LEN


typedef enum
{
    TCP_STATE_DISCONNECT,
    TCP_STATE_CONNECTED,
    TCP_STATE_LOGIN_OK,
}tcp_state_e;


typedef struct {
    int fd;
    tcp_state_e tcp_state;
    struct sockaddr_in 	svr_addr;		// server ip addr
    pthread_mutex_t	send_mutex;
    struct ringbuffer recv_ringbuf;			// socket receive data ring buffer
    unsigned char tmp_buf[PROTO_PACK_MAX_LEN];
    unsigned char proto_buf[PROTO_PACK_MAX_LEN];		// protocol packet data buffer
    int proto_len;
    unsigned char ack_buf[PROTO_PACK_MAX_LEN];
} client_info_t;


int proto_0x21_sendframe(client_info_t *client, unsigned char format, void *frame, int len);

int start_socket_client_task(char *svr_ip);

#endif // SOCKET_CLIENT_H
