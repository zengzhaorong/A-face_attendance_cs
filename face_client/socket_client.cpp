#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "socket_client.h"
#include "capture.h"
#include "public.h"
#include "ringbuffer.h"
#include "config.h"

char *g_server_ip = NULL;
client_info_t g_client_info = {0};
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

    client->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(client->fd < 0)
    {
        return -1;
    }

    pthread_mutex_init(&client->send_mutex, NULL);

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
    static unsigned int seq_num = 0;
    int total = 0;
    int ret;

    if(client->fd <= 0)
    {
        printf("%s error: client not connect!\n", __FUNCTION__);
        return -1;
    }

    /* add sequence */
    data[PROTO_SEQ_OFFSET] = seq_num++;

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

int proto_makeup_packet(uint8_t seq, uint8_t cmd, int len, uint8_t *data, uint8_t *outbuf, int size, int *outlen)
{
    uint8_t *packBuf = outbuf;
    int packLen = 0;

    if((len!=0 && data==NULL) || outbuf==NULL || outlen==NULL)
        return -1;

    /* if outbuf is not enough */
    if(PROTO_DATA_OFFSET +len +1 > size)
    {
        printf("ERROR: %s: outbuf size [%d:%d] is not enough !!!\n", __FUNCTION__, size, PROTO_DATA_OFFSET +len +1);
        return -2;
    }

    packBuf[PROTO_HEAD_OFFSET] = PROTO_HEAD;
    packLen += 1;

    memcpy(packBuf +PROTO_VERIFY_OFFSET, PROTO_VERIFY, 4);
    packLen += 4;

    packBuf[PROTO_SEQ_OFFSET] = seq;
    packLen += 1;

    packBuf[PROTO_CMD_OFFSET] = cmd;
    packLen += 1;

    memcpy(packBuf +PROTO_LEN_OFFSET, &len, 4);
    packLen += 4;

    memcpy(packBuf +PROTO_DATA_OFFSET, data, len);
    packLen += len;

    packBuf[PROTO_DATA_OFFSET +len] = PROTO_TAIL;
    packLen += 1;

    *outlen = packLen;

    return 0;
}

int proto_0x03_heartbeat(client_info_t *client)
{
    uint8_t proto_buf[128];
    int pack_len = 0;
    uint32_t time_now;
    int data_len = 0;

    time_now = (uint32_t)time(NULL);
    data_len += 4;

    proto_makeup_packet(0, 0x03, data_len, (uint8_t *)&time_now, proto_buf, sizeof (proto_buf), &pack_len);

    client_send_data(client, proto_buf, pack_len);

    return 0;
}

int client_0x03_heartbeat(client_info_t *client, uint8_t *data, int len, uint8_t *ack_data, int size, int *ack_len)
{
    uint32_t time;
    int offset = 0;
    int ret;

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

    proto_makeup_packet(0, 0x21, offset, tmp_databuf, tmp_protobuf, sizeof (tmp_protobuf), &pack_len);

    client_send_data(client, tmp_protobuf, pack_len);

    return 0;
}


int proto_detect_pack(struct ringbuffer *ringbuf, proto_detect_info_t *detect, uint8_t *proto_data, int size, int *proto_len)
{
    unsigned char buf[256];
    int len;
    char veri_buf[] = PROTO_VERIFY;
    int tmp_protoLen;
    uint8_t byte;

    if(ringbuf==NULL || proto_data==NULL || proto_len==NULL || size<PROTO_PACK_MIN_LEN)
        return -1;

    tmp_protoLen = *proto_len;

    /* get and check protocol head */
    if(!detect->head)
    {
        while(ringbuf_datalen(ringbuf) > 0)
        {
            ringbuf_read(ringbuf, &byte, 1);
            if(byte == PROTO_HEAD)
            {
                proto_data[0] = byte;
                tmp_protoLen = 1;
                detect->head = 1;
                //printf("********* detect head\n");
                break;
            }
        }
    }

    /* get and check verify code */
    if(detect->head && !detect->verify)
    {
        while(ringbuf_datalen(ringbuf) > 0)
        {
            ringbuf_read(ringbuf, &byte, 1);
            if(byte == veri_buf[tmp_protoLen-1])
            {
                proto_data[tmp_protoLen] = byte;
                tmp_protoLen ++;
                if(tmp_protoLen == 1+strlen(PROTO_VERIFY))
                {
                    detect->verify = 1;
                    //printf("********* detect verify\n");
                    break;
                }
            }
            else
            {
                if(byte == PROTO_HEAD)
                {
                    proto_data[0] = byte;
                    tmp_protoLen = 1;
                    detect->head = 1;
                }
                else
                {
                    tmp_protoLen = 0;
                    detect->head = 0;
                }
            }
        }
    }

    /* get other protocol data */
    if(detect->head && detect->verify)
    {
        while(ringbuf_datalen(ringbuf) > 0)
        {
            if(tmp_protoLen < PROTO_DATA_OFFSET)	// read data_len
            {
                len = ringbuf_read(ringbuf, buf, sizeof(buf) < PROTO_DATA_OFFSET -tmp_protoLen ? \
                                                    sizeof(buf) : PROTO_DATA_OFFSET -tmp_protoLen);
                if(len > 0)
                {
                    memcpy(proto_data +tmp_protoLen, buf, len);
                    tmp_protoLen += len;
                }
                if(tmp_protoLen >= PROTO_DATA_OFFSET)
                {
                    memcpy(&len, proto_data +PROTO_LEN_OFFSET, 4);
                    detect->pack_len = PROTO_DATA_OFFSET +len +1;
                    if(detect->pack_len > size)
                    {
                        printf("ERROR: %s: pack len[%d] > buf size[%d]\n", __FUNCTION__, size, detect->pack_len);
                        memset(detect, 0, sizeof(proto_detect_info_t));
                    }
                }
            }
            else	// read data
            {
                len = ringbuf_read(ringbuf, buf, sizeof(buf) < detect->pack_len -tmp_protoLen ? \
                                                    sizeof(buf) : detect->pack_len -tmp_protoLen);
                if(len > 0)
                {
                    memcpy(proto_data +tmp_protoLen, buf, len);
                    tmp_protoLen += len;
                    if(tmp_protoLen == detect->pack_len)
                    {
                        if(proto_data[tmp_protoLen-1] != PROTO_TAIL)
                        {
                            printf("%s : packet data error, no detect tail!\n", __FUNCTION__);
                            memset(detect, 0, sizeof(proto_detect_info_t));
                            tmp_protoLen = 0;
                            break;
                        }
                        *proto_len = tmp_protoLen;
                        memset(detect, 0, sizeof(proto_detect_info_t));
                        //printf("%s : get complete protocol packet, len: %d\n", __FUNCTION__, *proto_len);
                        return 0;
                    }
                }
            }
        }
    }

    *proto_len = tmp_protoLen;

    return -1;
}

int proto_analy_packet(uint8_t *pack, int packLen, uint8_t *seq, uint8_t *cmd, int *len, uint8_t **data)
{

    if(pack==NULL || seq==NULL || cmd==NULL || len==NULL || data==NULL)
        return -1;

    if(packLen < PROTO_PACK_MIN_LEN)
        return -2;

    *seq = pack[PROTO_SEQ_OFFSET];

    *cmd = pack[PROTO_CMD_OFFSET];

    memcpy(len, pack +PROTO_LEN_OFFSET, 4);

    if(*len +PROTO_PACK_MIN_LEN != packLen)
    {
        return -1;
    }

    if(*len > 0)
        *data = pack + PROTO_DATA_OFFSET;

    return 0;
}

int client_proto_handle(client_info_t *client, unsigned char *pack, unsigned int pack_len)
{
    uint8_t *ack_buf = client->ack_buf;
    uint8_t *tmpBuf = client->tmp_buf;
    uint8_t seq = 0, cmd = 0;
    int data_len = 0;
    uint8_t *data = 0;
    int ack_len = 0;
    int tmpLen = 0;
    int ret;

    ret = proto_analy_packet(pack, pack_len, &seq, &cmd, &data_len, &data);
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
        proto_makeup_packet(seq, cmd, ack_len, ack_buf, tmpBuf, PROTO_PACK_MAX_LEN, &tmpLen);
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
            det_ret = proto_detect_pack(&client->recv_ringbuf, &client->detect_info, client->proto_buf, sizeof(client->proto_buf), &client->proto_len);
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
        g_server_ip = DEFAULT_SERVER_IP;

    ret = pthread_create(&tid, NULL, client_task_thread, NULL);
    if(ret != 0)
    {
        return -1;
    }

    return 0;
}

