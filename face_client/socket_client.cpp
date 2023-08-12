#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "socket_client.h"
#include "lib_proto.h"
#include "capture.h"
#include "public.h"
#include "ringbuffer.h"
#include "config.h"

char *g_server_ip = NULL;
client_info_t g_client_info;
static unsigned char tmp_databuf[PROTO_PACK_MAX_LEN];
static unsigned char tmp_protobuf[PROTO_PACK_MAX_LEN];
extern int g_send_video_flag;
extern work_state_t g_work_state;

int client_init(client_info_t *client, char *srv_ip, int srv_port)
{
    int flags = 0;
    int ret;

    memset(client, 0, sizeof(client_info_t));

    client->tcp_state = TCP_STATE_DISCONNECT;

	// 创建一个socket套接字,TCP类型
    client->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(client->fd < 0)
    {
        return -1;
    }

    pthread_mutex_init(&client->send_mutex, NULL);

	// 设置参数属性
    flags = fcntl(client->fd, F_GETFL, 0);
    fcntl(client->fd, F_SETFL, flags | O_NONBLOCK);

    client->svr_addr.sin_family = AF_INET;
    inet_pton(AF_INET, srv_ip, &client->svr_addr.sin_addr);
    client->svr_addr.sin_port = htons(srv_port);

    ret = ringbuf_init(&client->recv_ringbuf, CLIENT_SENDBUF_SIZE);
    if(ret != 0)
    {
        return -2;
    }

    return 0;
}

int client_send_data(client_info_t *client, unsigned char *data, int len)
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
		// 发送数据
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

int client_recv_data(client_info_t *client)
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
int proto_0x03_heartbeat(client_info_t *client)
{
    uint8_t proto_buf[128];
    int pack_len = 0;
    uint32_t time_now;
    int data_len = 0;

    time_now = (uint32_t)time(NULL);
    data_len += 4;

    proto_makeup_packet(0x03, (uint8_t *)&time_now, data_len, proto_buf, sizeof (proto_buf), &pack_len);

    client_send_data(client, proto_buf, pack_len);

    return 0;
}

int client_0x03_heartbeat(client_info_t *client, uint8_t *data, int len, uint8_t *ack_data, int size, int *ack_len)
{
    uint32_t time;
    int offset = 0;
    int ret;

    UNUSED_4(client,len,ack_data,size);

    memcpy(&ret, data +offset, 4);
    offset += 4;

    /* bejing time */
    memcpy(&time, data +offset, 4);
    offset += 4;

    printf("%s: ret %d, time: %d\n", __FUNCTION__, ret, time);

    if(ack_len != NULL)
        *ack_len = 0;

    return 0;
}

int client_0x10_getframe(client_info_t *client, uint8_t *data, int len, uint8_t *ack_data, int size, int *ack_len)
{
    int frame_len = 0;
    int offset = 0;
    int ret = 0;

    UNUSED_3(client,data,len);

    /* request part */
    // NULL

    /* ack part */
    /* return value */
    memcpy(ack_data +offset, &ret, 4);
    offset += 4;

    /* type */
    ack_data[offset] = 0;
    offset += 1;

    /* frame data */
    ret = capture_get_newframe(ack_data +offset +4, size-offset, &frame_len);
    if(ret == -1)
        return -1;

    /* frame len */
    memcpy(ack_data +offset, &frame_len, 4);
    offset += 4;

    offset += frame_len;

    if(ack_len != NULL)
        *ack_len = offset;

    return 0;
}

int client_0x11_facedetect(client_info_t *client, uint8_t *data, int len, uint8_t *ack_data, int size, int *ack_len)
{
    rect_location_t rect;
    uint8_t count;
    int offset = 0;

    UNUSED_5(client,len,ack_data,size,ack_len);

    count = data[0];
    offset += 1;

    for(int i=0; i<count; i++)
    {
        rect.x = *((int*)(data+offset));
        offset += 4;
        rect.y = *((int*)(data+offset));
        offset += 4;
        rect.w = *((int*)(data+offset));
        offset += 4;
        rect.h = *((int*)(data+offset));
        offset += 4;
    }

    face_set_face_info(&rect, 1, NULL);

    return 0;
}

int client_0x12_facerecogn(client_info_t *client, uint8_t *data, int len, uint8_t *ack_data, int size, int *ack_len)
{
    int face_id = 0;
    uint8_t confidence = 0;
    char face_name[32] = {0};
    int status;
    int offset = 0;

    UNUSED_5(client,len,ack_data,size,ack_len);

    /* face id */
    memcpy(&face_id, data +offset, 4);
    offset += 4;

    /* confidence */
    confidence = data[offset];
    offset += 1;

    /* face name */
    memcpy(face_name, data +offset, 32);
    offset += 32;

    /* status */
    memcpy(&status, data +offset, 4);
    offset += 4;

    printf("[recogn]: ****** id: %d, name: %s, confid: %d \n", face_id, face_name, confidence);

    rect_location_t locat_array[MAX_FACE_NUM];
    recogn_info_t recogn_info;
    int face_num = 0;

    face_get_face_info(locat_array, MAX_FACE_NUM, &face_num, &recogn_info);

    recogn_info.user_id = face_id;
    recogn_info.score = confidence;
    strncpy(recogn_info.user_name, face_name, 32);
    printf("face recogn: face %d, score %d\n", face_id, confidence);
    face_set_face_info(locat_array, face_num, &recogn_info);

    g_work_state = WORK_STA_RECOGN_OK;

    return 0;
}

int client_0x20_switchcamera(client_info_t *client, uint8_t *data, int len, uint8_t *ack_data, int size, int *ack_len)
{
    unsigned char onoff = 0;

    UNUSED_5(client,len,ack_data,size,ack_len);

    onoff = data[0];
    printf("%s: onoff=%d\n", __FUNCTION__, onoff);

    g_send_video_flag = onoff;

    return 0;
}

int proto_0x21_sendframe(client_info_t *client, unsigned char format, void *frame, int len)
{
    int offset = 0;
    int pack_len = 0;

    /* format */
    tmp_databuf[offset] = format;
    offset += 1;

    /* data len */
    memcpy(tmp_databuf +offset, &len, 4);
    offset += 4;

    /* frame data */
    memcpy(tmp_databuf +offset, frame, len);
    offset += len;

    proto_makeup_packet(0x21, tmp_databuf, offset, tmp_protobuf, sizeof (tmp_protobuf), &pack_len);

    client_send_data(client, tmp_protobuf, pack_len);

    return 0;
}

int client_proto_handle(client_info_t *client, unsigned char *pack, unsigned int pack_len)
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
            ret = client_0x03_heartbeat(client, data, data_len, ack_buf, PROTO_PACK_MAX_LEN, &ack_len);
            break;

        case 0x10:
            ret = client_0x10_getframe(client, data, data_len, ack_buf, PROTO_PACK_MAX_LEN, &ack_len);
            break;

        case 0x11:
            ret = client_0x11_facedetect(client, data, data_len, ack_buf, PROTO_PACK_MAX_LEN, &ack_len);
            break;

        case 0x12:
            ret = client_0x12_facerecogn(client, data, data_len, ack_buf, PROTO_PACK_MAX_LEN, &ack_len);
            break;

        case 0x20:
            ret = client_0x20_switchcamera(client, data, data_len, ack_buf, PROTO_PACK_MAX_LEN, &ack_len);
            break;

    }

    /* send ack data */
    if(ret==0 && ack_len>0)
    {
        proto_makeup_packet(cmd, ack_buf, ack_len, tmpBuf, PROTO_PACK_MAX_LEN, &tmpLen);
        client_send_data(client, tmpBuf, tmpLen);
    }

    return 0;
}

void *client_task_thread(void *arg)
{
    client_info_t *client = &g_client_info;
    time_t heartbeat_time = 0;
    time_t tmp_time;
    int ret;

    UNUSED_1(arg);

	// 初始化客户端，创建socket，设置相关属性
    ret = client_init(client, g_server_ip, DEFAULT_SERVER_PORT);
    if(ret != 0)
    {
        printf("%s client init failed!\n", __FUNCTION__);
        return NULL;
    }

    while(1)
    {
        switch (client->tcp_state)
        {
            case TCP_STATE_DISCONNECT:
				// 连接服务器
                ret = connect(client->fd, (struct sockaddr *)&client->svr_addr, sizeof(struct sockaddr_in));
                if(ret == 0)
                {
                    client->tcp_state = TCP_STATE_CONNECTED;
                    g_work_state = WORK_STA_NORMAL;
                    printf("********** tcp connect server ok **********\n");
                }
                break;

            case TCP_STATE_CONNECTED:
                // 保持心跳
                tmp_time = time(NULL);
                if(abs(tmp_time - heartbeat_time) >= 6)
                {
                    proto_0x03_heartbeat(client);
                    heartbeat_time = tmp_time;
                }
                break;

            case TCP_STATE_LOGIN_OK:    // not use
                break;
        }

        if(client->tcp_state == TCP_STATE_CONNECTED)
        {
            int recv_ret;
            int det_ret;

            recv_ret = client_recv_data(client);
            det_ret = proto_detect_pack(&client->recv_ringbuf, client->proto_buf, sizeof(client->proto_buf), &client->proto_len);
            if(det_ret == 0)
            {
                client_proto_handle(client, client->proto_buf, client->proto_len);
            }

            if(recv_ret<=0 && det_ret!=0)
            {
                usleep(30*1000);
            }
        }
        else
        {
            usleep(100*1000);
        }
    }

    return NULL;
}

int start_socket_client_task(char *svr_ip)
{
    pthread_t tid;
    int ret;

    if(svr_ip)
        g_server_ip = svr_ip;
    else
        g_server_ip = (char *)DEFAULT_SERVER_IP;

    ret = pthread_create(&tid, NULL, client_task_thread, NULL);
    if(ret != 0)
    {
        return -1;
    }

    return 0;
}

