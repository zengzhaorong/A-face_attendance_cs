#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <semaphore.h>
#include "socket_server.h"
#include "capture.h"
#include "config.h"


server_info_t g_server_info = {0};
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

int server_0x03_heartbeat(client_info_t *client, uint8_t *data, int len, uint8_t *ack_data, int size, int *ack_len)
{
    uint32_t tmpTime;
    int tmplen = 0;
    int ret;

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

    proto_makeup_packet(0, 0x10, 0, NULL, proto_buf, sizeof(proto_buf), &pack_len);

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
    int buf_size = 0;
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

    proto_makeup_packet(0, 0x11, offset, data_buf, proto_buf, sizeof(proto_buf), &pack_len);

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

    proto_makeup_packet(0, 0x12, offset, data_buf, proto_buf, sizeof(proto_buf), &pack_len);

    return server_send_data(&g_server_info.client, proto_buf, pack_len);
}

int proto_0x20_switchcamera(unsigned char onoff)
{
    uint8_t proto_buf[128];
    int pack_len = 0;

    proto_makeup_packet(0, 0x20, 1, &onoff, proto_buf, sizeof(proto_buf), &pack_len);

    return server_send_data(&g_server_info.client, proto_buf, pack_len);
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

int server_proto_handle(client_info_t *client, unsigned char *pack, unsigned int pack_len)
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
            ret = server_0x03_heartbeat(client, data, data_len, ack_buf, PROTO_PACK_MAX_LEN, &ack_len);
            break;

        case 0x10:
            ret = server_0x10_recvframe(client, data, data_len, ack_buf, PROTO_PACK_MAX_LEN, &ack_len);
            break;

    }

    /* send ack data */
    if(ret==0 && ack_len>0)
    {
        proto_makeup_packet(seq, cmd, ack_len, ack_buf, tmpBuf, PROTO_PACK_MAX_LEN, &tmpLen);
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
        det_ret = proto_detect_pack(&client->recv_ringbuf, &client->detect_info, client->proto_buf, sizeof(client->proto_buf), &client->proto_len);
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

