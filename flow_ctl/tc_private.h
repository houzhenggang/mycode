#ifndef _TC_PRIVATE_H_
#define _TC_PRIVATE_H_

#include <rte_timer.h>
#include <rte_spinlock.h>
#include <rte_log.h>

#include <urcu.h>
#include <urcu/rcuhlist.h>
#include <asm-generic/errno-base.h>

#include "list.h"
 #include "sft_queue.h"
#include "utils.h"

#define RTE_LOGTYPE_ARBITER RTE_LOGTYPE_USER1

#define TIMER_RESOLUTION_CYCLES 200000ULL /* around 0.1ms at 2 Ghz */
#define TIMER_RESOLUTION_MS     2000000ULL /* 1ms */

//#define DEBUG_YHL

#define RTE_ERR(x,args...) \
    do{                         \
        RTE_LOG(ERR, ARBITER, x, ##args);      \
    } while(0)
        
#ifdef DEBUG_YHL
#define RTE_DBG(x,args...) \
    do{                         \
        RTE_LOG(DEBUG, ARBITER, x, ##args);      \
    } while(0)
#else
#define RTE_DBG(...)
#endif
        
#define RTE_INF(x,args...) \
    do{                         \
        RTE_LOG(INFO, ARBITER, x, ##args);      \
    } while(0)

#define TRUE    1
#define FALSE   0

#define SEND_INTERVAL            5 //datapipe ticks
#define SEND_INTERVAL_ONEIP        50 //per ip ticks

#define TOKEN_FLOW_HASH_ITEM KSFT_FIP_HASHSIZE / sizeof(struct ksft_hash_node)

#define CMD_SET_RULE        "set_rule"
#define CMD_SET_DATAPIPE    "set_datapipe"
#define CMD_SET_IPGROUP     "set_ipgroup"
#define CMD_DISPATCHER      "dispatcher"
#define CMD_DEFAULT_RULE    "default_rule"

#define MAX_FILE_PATH_LEN   4096
#define MAX_FILE_NAME_LEN   64

#define MAX_DISRULE_NUM     16
#define MAX_RULE_PROTO_NUM  32

//4 4 8 16 bit
#define TOPCLASS_MASK 0xF0000000
#define MIDCLASS_MASK 0x0F000000
#define SUBCLASS_MASK 0x00FF0000

enum{
    UN_TIMER,
    UN_ENTRY,
    UN_CACHE,
};

enum{//ifgoon
    RULE_STOP,
    RULE_CONTINUE,
};

enum{//action type
    BLOCK ,
    ACCEPT,
    DATAPIPE,
    ONE_IP_LIMIT,
};

enum{
    IPT_ANY_IP = 1,
    IPT_ONE_IP,
    IPT_NETWORK_MASK_IP,
    IPT_START_END_IP,
    IPT_GROUP_IP,
};

enum{
    PT_ANY_PORT = 1,
    PT_ONE_PORT,
    PT_START_END_PORT,
};

enum{
    ANY_DIR = 1,
    DOWN_DIR,
    UP_DIR,
};

enum{
    PROTO_ANY = 1,
    PROTO_TCP,
    PROTO_UDP,
};

struct sft_flow_rule
{
    int index;//1-65535
    int prev_index;
    int next_index;
    unsigned int        dir:2,
                        l4proto:2,
                        //next_do:2, //accept or block
                        ifgoon:1, //stop or continue
                        action_type:2, //accept or block or one_ip limit or datapipe
                        inner_ip_type:3,
                        outer_ip_type:3,
                        inner_port_type:2,
                        outer_port_type:2,
                        enable:1,//enable or disable 
                        if_oneip_limit:1;//1:yes,0:no
    union{
        uint32_t inner_ip;//only one ip,network order
        struct{
            uint8_t inner_network[4];
            uint8_t inner_mask[4];
        };
        struct{
            uint32_t inner_ip_start;//host order
            uint32_t inner_ip_end;//host order
        };
    };
    union{
        uint16_t inner_port;//only one port network order
        struct{
            uint16_t inner_port_start;//host order
            uint16_t inner_port_end;//host order
        };
    };

    union{
        uint32_t outer_ip;//only one ip,network order
        struct{
            uint8_t outer_network[4];
            uint8_t outer_mask[4];
        };
        struct{
            uint32_t outer_ip_start;//host order
            uint32_t outer_ip_end;//host order
        };
    };
    union{
        uint16_t outer_port;//only one port ,network order
        struct{
            uint16_t outer_port_start;//host order
            uint16_t outer_port_end;//host order
        };
    };

    unsigned int app_proto[MAX_RULE_PROTO_NUM];
    uint8_t app_proto_num;
    
    int ip_rate;
    int datapipe_index;
    struct rte_timer   timeout; 
    //struct rcu_head rcu;
    unsigned short ipg_inner_idx;
    unsigned short ipg_outer_idx;
    
    struct rule_statics_t *flow_statics;
};

struct sft_flow_ip_entry
{    
    struct cds_hlist_node   hlist;
    struct rcu_head     rcu;
    struct sft_queue *pktQ;

    struct token_struct token_entry;

    int ip_rate;
    uint32_t inner_ip;
    uint32_t l4proto;
    unsigned int app_proto;
    int dir;
    uint8_t delete:1;
    int index;

//    struct rte_timer   timeout_token; 
//    u64 time_mark;
    struct rte_timer   timeout_send; 

    unsigned long timestamp;
    struct rte_timer   timeout_kill; 
    
    struct tc_private_t *priv;
};

struct sft_flow_datapipe_entry
{
    struct sft_queue *pktQ;

    int dir;
    int index;

    struct token_struct token_entry;

    int rate;
    int used;

//    struct rte_timer   timeout_token; 
//    u64 time_mark;
    struct rte_timer   timeout_send; 
    
    struct tc_private_t *priv;
};

struct sft_ipgroup_entry
{
    struct hlist_node   hlist;
    //struct rcu_head     rcu;
    int ip_type;
    union{
        uint32_t ip;//only one ip,network order
        struct{
            uint8_t network[4];
            uint8_t mask[4];
        };
        struct{
            uint32_t ip_start;//host order
            uint32_t ip_end;//host order
        };
    };
};

struct sft_ipgroup_list
{
    unsigned short enable;
    unsigned short index;
    struct hlist_head hhead;

    //rte_spinlock_t lock; 
};

struct ksft_hash_node
{
    struct cds_hlist_head hhead;
    rte_spinlock_t lock;  
};

struct dispat_rule
{
    int id;
    char stat[16];
    char period_type[16];
    int per_mon;
    int start;
    int end;
    
    char stime[16];
    char etime[16];
    
    char rule_set_name[MAX_FILE_NAME_LEN];
};

struct dispatch_conf
{
    char curr_rule_name[MAX_FILE_NAME_LEN];
    char default_rule_name[MAX_FILE_NAME_LEN];
    time_t last_modify_time;
    time_t last_dispatch_time;
    time_t last_default_time;
    
    int ruleset_count;
    struct dispat_rule dis_ruleset[MAX_DISRULE_NUM];
    
    struct rte_timer timeout;
};

struct token_hash_node
{
    struct hlist_head hhead;
    rte_spinlock_t lock;  
};

struct free_node
{
    struct hlist_node hlist;
    int idx;
};

struct tc_private_t 
{
    char rule_conf_path[MAX_FILE_PATH_LEN];
    char ipgroup_conf_path[MAX_FILE_PATH_LEN];
    char datapipe_conf_path[MAX_FILE_PATH_LEN];
    char dispatch_conf_path[MAX_FILE_PATH_LEN];
    char default_rule_conf_path[MAX_FILE_PATH_LEN];
    
    struct dispatch_conf disp_conf;
    
    struct rte_timer reload_tim;
    
    //struct sft_skb_queue Queue[MAX_CPU_NUM];
    
    struct sft_flow_datapipe_entry ksft_datapipe[DATAPIPE_NUM];
    struct sft_ipgroup_list sft_ipgroup_list[IPGROUP_NUM];
 
    int sft_flow_rule_start;
    int sft_flow_rule_end;
    struct sft_flow_rule **rule_active;
    struct sft_flow_rule **rule_backup;
    struct sft_flow_rule *sft_flow_rule1[FLOW_RULE_NUM];
    struct sft_flow_rule *sft_flow_rule2[FLOW_RULE_NUM];
    unsigned long rule_update_timestamp;
};

inline uint32_t dton(uint32_t mask);
inline int parse_string_to_ip_address(uint8_t *ip, char *buffer);
inline void pkg_queue_purge(struct sft_queue *);
inline void flow_ip_delete(struct tc_private_t *priv, struct sft_flow_ip_entry *fdb);
inline void token_flow_ip_free(struct sft_flow_ip_entry *token, int hash_idx);

int tc_flow_ip_init(void);

#endif
