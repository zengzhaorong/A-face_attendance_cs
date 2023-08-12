#ifndef LIB_PROTO_H
#define LIB_PROTO_H


#define PROTO_PACK_MAX_LEN		(1 *1024 *1024)
#define PROTO_PACK_MIN_LEN		(12)


int proto_makeup_packet(unsigned char cmd, unsigned char *data, int len, unsigned char *outbuf, int size, int *outlen);

/* 
 * raw_data: must be "struct ringbuffer" type
 */
int proto_detect_pack(void *raw_data, unsigned char *proto_data, int size, int *proto_len);

int proto_analy_packet(unsigned char *pack, int packLen, unsigned char *cmd, int *len, unsigned char **data);

int proto_lib_init(void);

#endif