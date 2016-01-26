/*
 * common
 *
 * Author: junwei.dong
 */
#ifndef __COMMON_H
#define __COMMON_H

#include <stdio.h>
#include <stdlib.h>  
#include <string.h>  
#include <errno.h> 

#include <sys/types.h>  
#include <sys/ipc.h>
#include  <sys/shm.h>
#include <sys/socket.h>  
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <netinet/in.h>  
#include <arpa/inet.h>  
#include <linux/types.h>

#include <ifaddrs.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "hash/h_cache.h"
#include "log.h"

//#define IPPROTO_TCP 6
//#define IPPROTO_UDP 17
#define IPPROTO_TCP_NAME "tcp"
#define IPPROTO_UDP_NAME "udp"

//shm 
#define SHM_SIZE    4096
#define SHM_MODE    (SHM_R | SHM_W | IPC_CREAT) /* user read/write */
#define STAT_SHM_KEY 0x99

log_debug_t logdebug;

#define MAX_FILE_PATH_LEN   4096
#define DATA_STREAM_FILE_PATH                        "/dev/shm/data_stream"
#define CONFIG_FILE_NAME                       "/opt/utc/conf/marbt.conf"

#define DATA_STREAM_FILE_NAME            "/dev/shm/data_stream_list.conf"
#define DATA_BASEAPP_FILE_NAME           "/dev/shm/data_baseapp_list.conf"
#define DATA_BASEIP_FILE_NAME              "/dev/shm/data_baseip_list.conf"

#define DATA_BASEAPP_FILE_NAME_RESULT     "/dev/shm/data_baseapp_list_result.conf"
#define DATA_BASEIP_FILE_NAME_RESULT        "/dev/shm/data_baseip_list_result.conf"
#define DATA_BASELINK_FILE_NAME_RESULT    "/dev/shm/data_baselink_list_result.conf"


#define G_DISPLAY_LINE_DEFAULT          (0) 
#define G_FLUSH_INTERVAL_DEFAULT     (300)  
#define  G_DATA_AGG_APPID_DEFAULT   (-1) 
#define G_BASELINK_IP_DEFAULT            (0)  

#define LINK_CON_INFO_SIZE_MAX 255

#define IPQUADS(addr) \
        ((unsigned char *)&addr)[0], \
        ((unsigned char *)&addr)[1], \
        ((unsigned char *)&addr)[2], \
        ((unsigned char *)&addr)[3]
#define IPQUAD_FMT "%u.%u.%u.%u"

typedef enum
{
    DATA_SORT_CONDITION_NOT_USED = 0,
    //baseapp
    DATA_SORT_CONDITION_CON_NUM,
    DATA_SORT_CONDITION_UP_BPS,
    DATA_SORT_CONDITION_DOWN_BPS,
    DATA_SORT_CONDITION_TOTAL_BITS,
    //baseip
    DATA_SORT_CONDITION_IP_CON_NUM,
    DATA_SORT_CONDITION_IP_UP_SIZE,
    DATA_SORT_CONDITION_IP_DOWN_SIZE,
    DATA_SORT_CONDITION_IP_TOTAL_SIZE,
    //baselink
    //DATA_SORT_CONDITION_DOWN_SIZE
} data_sort_condition;

typedef enum
{
    DATA_AGG_CLASS_NOT_USED = 0,
    DATA_AGG_CLASS_APP,
    DATA_AGG_CLASS_IP_INNER,
    DATA_AGG_CLASS_IP_OUTER,
    DATA_AGG_CLASS_LINK_IP_INNER,
    DATA_AGG_CLASS_LINK_IP_OUTER,
    DATA_AGG_CLASS_LINK_IP_ALL,
    DATA_AGG_CLASS_MAX
} data_aggregation_class;

data_sort_condition  g_data_sort_condition;
data_aggregation_class g_data_agg_class;

// __attribute__((packed))
//#pragma pack(push) 
//#pragma pack(1)
//#pragma pack(pop)
//message queue
#define KEYPATH "/etc/hosts"
#define KEYPROJ 'k'

//message
struct msg_st {
    long mtype;
    uint32_t ip_src;
    uint16_t port_src;
    uint32_t ip_dst;
    uint16_t port_dst;
    uint8_t  proto;
    uint32_t proto_mark;
    uint64_t up_bytes;
    uint64_t down_bytes;
    uint64_t duration_time;
    uint8_t finish_flag;
}__attribute__((packed)) ;

//shm
typedef struct {
uint64_t diff_tsc[8];
    uint64_t arg_diff_tsc[8];
    uint64_t skb_num[8];
    uint64_t max_arg_diff_tsc[8];
    uint64_t total_tsc[8];
    uint64_t total_num[8];
    uint32_t hash_level_water_mark;
    uint32_t exceed_hash_level_count;
    uint32_t max_hash_level;
    uint64_t fail_skb;
    uint64_t exceed_high_water_mark_count;
    uint64_t nums_sft_fip_entry;
    uint64_t rte_timer_tsc_per_sec[8];
    uint64_t send_pkt_tsc_per_sec[8];
    uint8_t web_mesg_flag;
    uint64_t cache_pkg_num;
    uint64_t failed_recv_pkg[8];
    int web_msgid;
}skbstat_t;

typedef struct AGGREGATION_NODE
{
    //baseapp
    uint32_t appid;
    uint64_t con_nums; 
    uint64_t up_bps; //bits
    uint64_t down_bps; //bits
    uint64_t total_bits; //bits

    //baseip
    uint32_t ip;
    uint64_t ip_con_nums;  
    uint64_t ip_up_size; //bits
    uint64_t ip_down_size; //bits
    uint64_t ip_total_size; //bits

    //baselink
    uint32_t link_app;
    uint64_t link_proto; 
    unsigned char link_con_info[LINK_CON_INFO_SIZE_MAX]; 
    uint64_t link_duration_time; //seconds
    uint64_t link_up_size; //bits
    uint64_t link_down_size; //bits
}aggregation_node_t;

//stream list
typedef struct STREAM_NODE
{
    struct msg_st data; 
    struct list_head list;;
}stream_node_t;
stream_node_t  stream_list_head;


typedef enum
{
    SORTED_LIST_NOT_USED = 0,
    SORTED_BASE_AGG = 1,//just do data aggregation
    SORTED_BASE_IP,
    SORTED_BASE_LINK,
    SORTED_BASE_APP,
    SORTED_LIST_MAX
} sorted_list_condition;

//sort_list
typedef struct SORTED_NODE
{
    aggregation_node_t data;
    //struct data_aggregation_cache *hash_node; 
    struct list_head list;
}sorted_node_t;

//way1: use more list for insert sort
sorted_node_t sorted_list_arry[SORTED_LIST_MAX];
sorted_list_condition g_sorted_list_index;

//way2: use two list for quick sort
sorted_node_t appid_sorted_list_head;
//sorted_node_t ip_sorted_list_head;

typedef struct MERGE_SORTED_NODE  
{  
    aggregation_node_t data;  
    struct MERGE_SORTED_NODE   *next;  
    struct MERGE_SORTED_NODE   *prev;  
}merge_sorted_node_t;

void _strim(char *str);
int strcmp_nocase(char * src, char * mark);
char **mSplit(char *, char *, int, int *, char);
void mSplitFree(char ***toks, int numtoks);

#endif   
