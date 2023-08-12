#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <semaphore.h>
#include "socket_server.h"
#include "lib_proto.h"
#include "capture.h"
#include "config.h"


server_info_t g_server_info;
extern sem_t g_getframe_sem;

int server_init(server_info_t *server, int port)
{
    int ret;

    memset(server, 0, sizeof(server_info_t));

    server->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server->fd < 0)
    {
        return -1;
    }

    server->svr_addr.sin_family = AF_INET;
    server->svr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server->svr_addr.sin_port = htons(port);

    ret = bind(server->fd, (struct sockaddr *)&server->svr_addr, sizeof(struct sockaddr_in));
    if(ret != 0)
    {
        return -2;
    }

	// 服务器监听客户端的连接
    ret = listen(server->fd, 10);
    if(ret != 0)
    {
        return -3;
    }

    return 0;
}

int server_send_data(client_info_t *client, uint8_t *data, int len)
{
    int total = 0;
    int ret;

    if(client->fd <= 0)
    {
        printf("%s error: client not connect!\n", __FUNCTION__);
        return -1;
    }

    // lock
    pthread_mutex_lock(&client->send_mutex);
    do{
        ret = send(client->fd, data +total, len -total, 0);
        if(ret < 0)
        {
            usleep(1000);
            continue;
        }
        total += ret;
    }while(total < len);
    // unlock
    pthread_mutex_unlock(&client->send_mutex);

    return total;
}

int server_recv_data(client_info_t *client)
{
    uint8_t *tmpBuf = client->tmp_buf;
    int len, space;
    int ret = 0;

    space = ringbuf_space(&client->recv_ringbuf);

    memset(tmpBuf, 0, PROTO_PACK_MAX_LEN);
    len = recv(client->fd, tmpBuf, PROTO_PACK_MAX_LEN>space ? space:PROTO_PACK_MAX_LEN, 0);
    if(len > 0)
    {
        ret = ringbuf_write(&client->recv_ringbuf, tmpBuf, len);
    }

    return ret;
}

int server_0x03_heartbeat(client_info_t *client, uint8_t *data, int len, uint8_t *ack_data, int size, int *ack_len)
{
    uint32_t tmpTime;
    int tmplen = 0;
    int ret;

    UNUSED_3(client,len,size);

    /* request part */
    memcpy(&tmpTime, data, 4);
    //printf("%s: time: %ld\n", __FUNCTION__, tmpTime);

    /* ack part */
    ret = 0;
    memcpy(ack_data +tmplen, &ret, 4);
    tmplen += 4;

    tmpTime = (uint32_t)time(NULL);
    memcpy(ack_data +tmplen, &tmpTime, 4);
    tmplen += 4;

    *ack_len = tmplen;

    return 0;
}

int proto_0x10_getframe(void)
{
    uint8_t proto_buf[128];
    int pack_len = 0;

    proto_makeup_packet(0x10, NULL, 0, proto_buf, sizeof(proto_buf), &pack_len);

    return server_send_data(&g_server_info.client, proto_buf, pack_len);
}

int server_0x10_recvframe(client_info_t *client, uint8_t *data, int len, uint8_t *ack_data, int size, int *ack_len)
{
    uint8_t type = 0;
    int frame_len = 0;
    uint8_t *frame = NULL;
    int offset = 0;
    int value = -1;
    int ret;

    UNUSED_5(client,len,ack_data,size,ack_len);

    memcpy(&ret, data+offset, 4);
    offset += 4;

    type = data[offset];
    offset += 1;
    (void)type;

    memcpy(&frame_len, data+offset, 4);
    offset += 4;

    frame = data + offset;
    offset += frame_len;

    //printf("*** recv one frame data: type: %d, data_len: %d\n", type, frame_len);

    /* put frame to detect */
    v4l2cap_update_newframe(frame, frame_len);

    ret = sem_getvalue(&g_getframe_sem, &value);
    if(value == 0)
    {
        sem_post(&g_getframe_sem);
    }

    return 0;
}

int proto_0x11_facedetect(uint8_t count, rect_location_t *face_locat)
{
    uint8_t data_buf[128];
    uint8_t proto_buf[128];
    int pack_len = 0;
    int offset = 0;
    int i;

    /* count */
    data_buf[0] = count;
    offset += 1;

    for(i=0; i<count; i++)
    {
        memcpy(data_buf+offset, &face_locat->x, 4);
        offset += 4;
        memcpy(data_buf+offset, &face_locat->y, 4);
        offset += 4;
        memcpy(data_buf+offset, &face_locat->w, 4);
        offset += 4;
        memcpy(data_buf+offset, &face_locat->h, 4);
        offset += 4;
    }

    proto_makeup_packet(0x11, data_buf, offset, proto_buf, sizeof(proto_buf), &pack_len);

    return server_send_data(&g_server_info.client, proto_buf, pack_len);
}

int proto_0x12_facerecogn(int face_id, uint8_t confidence, char *face_name)
{
    uint8_t data_buf[128];
    uint8_t proto_buf[128];
    int pack_len = 0;
    int offset = 0;

    /* face id */
    memcpy(data_buf +offset, &face_id, 4);
    offset += 4;

    /* confidence */
    data_buf[offset] = confidence;
    offset += 1;

    /* face name */
    memcpy(data_buf +offset, face_name, 32);
    offset += 32;

    proto_makeup_packet(0x12, data_buf, offset, proto_buf, sizeof(proto_buf), &pack_len);

    return server_send_data(&g_server_info.client, proto_buf, pack_len);
}

int proto_0x20_switchcamera(unsigned char onoff)
{
    uint8_t proto_buf[128];
    int pack_len = 0;

    proto_makeup_packet(0x20, &onoff, 1, proto_buf, sizeof(proto_buf), &pack_len);

    return server_send_data(&g_server_info.client, proto_buf, pack_len);
}

int server_proto_handle(client_info_t *client, unsigned char *pack, unsigned int pack_len)
{
    uint8_t *ack_buf = client->ack_buf;
    uint8_t *tmpBuf = client->tmp_buf;
    uint8_t cmd = 0;
    int data_len = 0;
    uint8_t *data = 0;
    int ack_len = 0;
    int tmpLen = 0;
    int ret;

    ret = proto_analy_packet(pack, pack_len, &cmd, &data_len, &data);
    if(ret != 0)
        return -1;

    //printf("%s: recv cmd: 0x%02x, seq: %d, pack_len: %d, data_len: %d\n", __FUNCTION__, cmd, seq, pack_len, data_len);

    switch(cmd)
    {
        case 0x01:
            break;

        case 0x03:
            ret = server_0x03_heartbeat(client, data, data_len, ack_buf, PROTO_PACK_MAX_LEN, &ack_len);
            break;

        case 0x10:
            ret = server_0x10_recvframe(client, data, data_len, ack_buf, PROTO_PACK_MAX_LEN, &ack_len);
            break;

    }

    /* send ack data */
    if(ret==0 && ack_len>0)
    {
        proto_makeup_packet(cmd, ack_buf, ack_len, tmpBuf, PROTO_PACK_MAX_LEN, &tmpLen);
        server_send_data(client, tmpBuf, tmpLen);
    }

    return 0;
}

void *server_task_thread(void *arg)
{
    client_info_t *client = (client_info_t *)arg;
    int recv_ret;
    int det_ret;
    int flags;
    int ret;

    pthread_mutex_init(&client->send_mutex, NULL);

    flags = fcntl(client->fd, F_GETFL, 0);
    fcntl(client->fd, F_SETFL, flags | O_NONBLOCK);

    ret = ringbuf_init(&client->recv_ringbuf, SVR_RECVBUF_SIZE);
    if(ret != 0)
        return NULL;

    while(1)
    {
        recv_ret = server_recv_data(client);
        det_ret = proto_detect_pack(&client->recv_ringbuf, client->proto_buf, sizeof(client->proto_buf), &client->proto_len);
        if(det_ret == 0)
        {
            server_proto_handle(client, client->proto_buf, client->proto_len);
        }

        if(recv_ret<=0 && det_ret!=0)
        {
            usleep(30*1000);
        }
    }

    return NULL;
}

void *server_listen_thread(void *arg)
{
    server_info_t *server = &g_server_info;
    struct sockaddr_in client_addr;
    pthread_t tid;
    int len;
    int ret;

    UNUSED_1(arg);

    ret = server_init(server, DEFAULT_SERVER_PORT);
    if(ret != 0)
    {
        printf("%s server init failed! ret: %d\n", __FUNCTION__, ret);
        return NULL;
    }

    while(1)
    {
        memset(&client_addr, 0, sizeof(struct sockaddr_in));

		// 接受客户端发起的连接
        server->client.fd = accept(server->fd, (struct sockaddr *)&server->client.sockaddr, (socklen_t *)&len);
        if(server->client.fd < 0)
        {
            continue;
        }
        printf("********** accept client tcp connect ok **********\n");

        ret = pthread_create(&tid, NULL, server_task_thread, &server->client);
        if(ret != 0)
        {
            printf("create server thread failed!\n");
        }
    }

    return NULL;
}

int start_socket_server_task(void)
{
    pthread_t tid;
    int ret;

    ret = pthread_create(&tid, NULL, server_listen_thread, NULL);
    if(ret != 0)
    {
        return -1;
    }

    return 0;
}

