#ifndef __INIT_H_
#define __INIT_H_

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <rte_atomic.h>
#include <rte_timer.h>
#include "acsmx2.h"
#include"list.h"
#include"kfifo.h"

enum file_type_item {
    APP_CLASS_TYPE = 0,
    PLACE_TYPE,
    PATTERN_TYPE,
    RULE_TYPE,
};


#define MAX_DPI_FILE_TYPE 4
struct conf_node {
    char *conf_name;
    struct list_head list;
};
struct conf_list {
    //char name[32];
    char *conf_type_name;
    struct list_head conf_head;
};
typedef struct _progvars
{
    char *key_lib_path;
    char *conf_path;
    char *sft_dpi_path;
    kfifo_t *hash_do_queue[32];
    //struct rte_mempool *hash_cache;
    struct rte_timer queue_timer[32];
    int daemon_flag;
    uint16_t place_cnt;
    uint32_t app_class_cnt;
    uint8_t  do_shutdown;
    struct conf_list dpi_conf[MAX_DPI_FILE_TYPE];
    char sft_tc_dev_name[32];
    ACSM_STRUCT2 * _acsm[ACSM_MAX_NUM];
    //struct dpi_statistics_struct (*DPI_Statistics)[16][256];
//    struct dip_conf dip_conf;    
    uint32_t http_proto_appid;
	uint32_t qq_login_appid;
    uint32_t weibo_appid;
    uint32_t tieba_appid;
	uint32_t ftp_proto_appid;
	uint32_t connectionless_tcp_appid; 
	uint32_t unknown_80_appid;
	uint32_t udp_application_mutual;
	uint32_t udp_video_download;
	uint32_t tcp_application_mutual;
	uint32_t tcp_video_download;
	uint32_t bt_download;
	uint32_t syn_ack;
    uint32_t original;
	uint8_t qqlog;
    uint8_t urlog;
    int syslog_sd;
    struct sockaddr_in syslog_peer_addr;
    int dpi_ctrl;
    int reload;
    uint8_t acsm_enable;
    uint8_t need_web_flag;
    char *peer_ip;
    uint16_t peer_port;
    uint64_t hz;
    rte_atomic64_t all_link;
    int msgid;
    uint8_t nb_lcores;
    int verbose;
}PV;
void *update_statictics(void *arg);

/*GLOBALS ************************************************************/
extern PV pv;                 /* program vars (command line args) */
void init_resouce_for_each_core(uint32_t lcore_id);
int parse_log_conf();
int init_sys();
int init_net_mode_conf();
int init_connect_mode_conf();
#endif
