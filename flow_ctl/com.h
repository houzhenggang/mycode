#ifndef __COM_H
#define __COM_H
#include <stdint.h>
#include "list.h"
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:        the pointer to the member.
 * @type:       the type of the container struct this is embedded in.
 * @member:     the name of the member within the struct.
 *
 */

#define true  1
#define false 0

#define TOKEN_MAX 64

//typedef int bool;
enum PROTO_TYPE
{
    DPI_PROTO_TCP = 0,
    DPI_PROTO_UDP,
    DPI_PROTO_HTTP,
    DPI_PROTO_ICMP,
    DPI_PROTO_TYPE_MAX = 8,
};

typedef enum http_token_flag
{
    HTTP_TOKEN_GET = 0,
    HTTP_TOKEN_HEAD,
    HTTP_TOKEN_POST,
    HTTP_TOKEN_LOCATION,
    HTTP_TOKEN_OPTIONS,
    HTTP_TOCKE_INVALID = 255
}http_token_flag_t;

struct ssn_skb_values
{
    struct sft_fdb_entry * ssn;
    struct rte_mbuf * skb;
    char * payload;
    unsigned int sip;
    unsigned int dip;
#if 0
    uint32_t    server_ip;
    uint16_t    server_port;
    uint32_t    client_ip;
    uint16_t    client_port;
#endif
    int l4proto;
    http_token_flag_t http_token_method;
    unsigned short http_token_host_offset;
    //unsigned short place_token_list[TOKEN_MAX];
    //unsigned short place_token_list_cnt;//from 0
    unsigned short payload_len;
    unsigned short sport;
    unsigned short dport;

    unsigned int seq_num;
    unsigned int ack_num;

    //  uint8_t isinner;
    //  uint8_t is_http;

    uint8_t  isinner:1,
        //is_http:1,
        is_syn:1,
        is_syn_ack:1,
        is_fin:1,
        is_rst:1,
        haved_dynamic:1;
};

#define RELOAD_TEST

#ifdef RELOAD_TEST
#define RCVPORT 9876
#define HTTP_TOKEN_GET_ID 0
#define HTTP_TOKEN_HEAD_ID 1
#define HTTP_TOKEN_POST_ID 2
#define HTTP_TOKEN_HOST_ID 3
#define HTTP_TOKEN_LOCATION_ID 4
#define HTTP_TOKEN_OPTIONS_ID 5

typedef enum KEY_ACSM_E
{
    REQ_TCP = 0,
    REQ_UDP,
    REQ_HTTP,
    RES_TCP,
    RES_UDP,
    RES_HTTP,
    REQ_HTTP_URL,
    ACSM_MAX_NUM,
}KEY_ACSM_T;

#define    SHAPER_QLEN                256                /* one pipe for on ip Maximum queued frames */
#define    SHAPER_PIPE_QLEN         75000           /* DataPipe Maximum queued frames */
#define KSFT_FIP_HASHSIZE 1024*100
#define FLOW_RULE_NUM 65536
#define DATAPIPE_NUM 256
#define IPGROUP_NUM 256

struct msg_st {
      uint16_t reload_type;

} __attribute__((packed));  /* GCC style */

#endif
#define STD_BUF  1024
#endif
