#ifndef __RULES_H
#define __RULES_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "config.h"
#define NAME_LEN 32
#define APP_CLASS_TEXT_SIZE_MIN 10
#define SAMEPORT_IPNUM 16

enum RULE_TYPE {
    APP_CLASS_RULE = 0,
    PLACE_RULE,
    PATTERN_RULE,
    RULES_RULE,
    PROTO_NODEMGT_RULE,
    PROTO_USERADD_RULE,
    ALL_RULE_TYPE,
    INVALID_RULE_TYPE,
    MAX_RULE_TYPE 
}; 


struct dpi_user_proto
{
    char name[NAME_LEN];
    unsigned int type_id;
    unsigned short port;
};
struct dpi_user_proto_node
{
    char name[NAME_LEN];
    unsigned int type_id;
    unsigned int ip;//network order
};

struct pattern_list
{
    uint16_t nextstate_id;
};

struct state_array
{
    struct pattern_list pattern[DPI_PATTERN_MAX];
    uint32_t type_id;
    uint8_t enable;
};


extern struct dpi_user_proto user_tcp_proto[0XFFFF+1];
extern struct dpi_user_proto user_udp_proto[0XFFFF+1];
extern struct dpi_user_proto_node user_tcp_proto_node[0XFFFF+1][SAMEPORT_IPNUM];
extern struct dpi_user_proto_node user_udp_proto_node[0XFFFF+1][SAMEPORT_IPNUM];


extern unsigned int tcp_server_port[0XFFFF+1];
extern unsigned int tcp_client_port[0XFFFF+1];
extern unsigned int udp_server_port[0XFFFF+1];
extern unsigned int udp_client_port[0XFFFF+1];

extern struct state_array *DPI_State_TBL;
extern int *DPI_State_Flag_TBL;
extern uint32_t http_proto_appid;

extern int16_t place_cnt;
int parse_dpi_conf();
int rules_reload(int rules_type);
int start_reload_timer(); 
#endif
