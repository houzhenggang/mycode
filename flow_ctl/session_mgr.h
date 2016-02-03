#ifndef __SESSION_MGR_H___
#define __SESSION_MGR_H___

#include <rte_timer.h>
#include <rte_spinlock.h>
#include <rte_atomic.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#include "list.h"
#include "h_cache.h"
#include <pcap.h>

#define MAX_SSN_STATE_NUM 64
#define MRULES_MAX 8

#define HASH_IPFRAG_SIZE 1024
#define FRAG_GC_INTERVAL 5

//#define SESSION_CACHE_HASH
typedef struct PCAP_DUMP_NODE {
	pcap_dumper_t *dumper;
       //pcap_t *pcap; //no need to store since use pcap_open_dead
       uint64_t dump_bytes;
}pcap_dump_node_t;

typedef struct DUMP_PCAPS {
       pcap_dump_node_t pcap_dump_node[255];
}dump_pcaps_t;

struct mesg_st {
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
} __attribute__((packed));  /* GCC style */


struct l2ct_private_struct
{
	uint64_t up_num;
	uint64_t up_bytes;
	uint64_t down_num;
	uint64_t down_bytes;
    uint64_t begin_timestamp;    //begin_timestamp
    uint64_t curren_timestamp;
    uint32_t unknow_udp_reidentify_num;
    uint32_t unknow_udp_app_mutual_num;
    uint32_t unknow_udp_video_download;
    uint32_t unknow_tcp_reidentify_num;
    uint32_t unknow_tcp_app_mutual_num;
    uint32_t unknow_tcp_video_download;
};

struct l2ct_var_dpi
{
    uint8_t ac_state_tbl;
    uint8_t is_dynamic;


    uint64_t last_up_seq_next;
    uint64_t last_down_seq_next;
    uint8_t tcp_ofo_size;
    struct hlist_head tcp_ofo_head;
};

struct key_5tuple {
	uint32_t ip_dst;
	uint32_t ip_src;
	uint16_t port_dst;
	uint16_t port_src;
	uint8_t  proto;
    
    //
    uint8_t is_syn;
};

struct hash_entry
{
    uint8_t ext_size;
    uint8_t used_size;
    struct sft_fdb_entry *ext;
};

#if 0
struct sft_fdb_entry
{
    time_t timestamp;
    uint32_t ip_dst;
    uint32_t ip_src;
    uint16_t port_dst;
    uint16_t port_src;
    uint8_t  proto;

    uint32_t pack_num;
    uint32_t pack_bytes;
    uint8_t lcore_id;
    uint8_t portid;
};
#else
#define L2CT_VAR_MAX 5    
struct sft_fdb_entry 
{
#ifdef SESSION_CACHE_HASH
    struct h_scalar h_scalar;
#endif

    uint8_t                magic;
    
    struct key_5tuple   sess_key;
    uint8_t             lcore_id;
    
    int                    recvdevn;     //where the skb recv dev sequence(ifindex)
    uint64_t           timestamp;    //timestamp
    unsigned short        vid ; //skb->vid
    uint32_t        proto_mark;//conntrack already mark protocl
    uint32_t        init_proto_mark;
    uint32_t        prev_proto_mark;
    uint16_t            flag0:1,//1:drop
                        flag1:1,//1:recv fin or rst
                        flag2:1,//1:white flow
                        up_unmatch:1,
                        down_unmatch:1,
                        slave:1,
                        keepalive:2,
                        proto_mark_state:1,
                        is_http:1,
                        log_flag:1;
                        //new_link:1,
                        //unknow_link:1, //use for statictics
                        //need_update_link:1,
                        //identifies_proto:1;
    //void *                vars[L2CT_VAR_MAX];//mutile usage,0:l2ct,1:dpi...
    
    int match_rule;
    
    struct l2ct_private_struct  vars_l2ct;
    struct l2ct_var_dpi         vars_dpi;
    void            *pattern_ptr;  
    //struct rte_timer   timeout; 
    struct sft_flow_ip_entry *flow_entry;
    uint64_t flag_timestamp;//use for rule update rate
    
    int pipe_idx_up;
    int pipe_idx_down;
    
    uint8_t up_mr_num;
    uint8_t down_mr_num;
    unsigned short up_mr_idx[MRULES_MAX];
    unsigned short down_mr_idx[MRULES_MAX];

//    void * log;
    uint16_t dpi_state_id_list[MAX_SSN_STATE_NUM];
    unsigned short place_token_list_cnt;//from 0//zebra
    //unsigned short place_token_list[TOKEN_MAX];//zebra
    unsigned short place_token_list[2];//zebra
    uint16_t offset;
    //rte_spinlock_t state_lock;  
    int state_cnt;
    uint64_t    link_timestamp;    //timestamp
    uint32_t    pkt_cnt;
    uint32_t    max_pkt_cnt;
    unsigned short ftp_id;
    char *ssn_ptr1;
    char *ssn_ptr2;

    pcap_dump_node_t *pcap_dump_node;  
    uint32_t qq;
    uint64_t logout_up_num;
};

#endif

typedef struct frag_info_s
{
    struct list_head	list;
    unsigned short      frag_off;
    unsigned int        frag_payload_len;
    struct rte_mbuf 	*skb;
}frag_info_t;

typedef struct frag_list_head_s
{
    struct list_head frag_list;
}frag_list_head_t;

typedef struct ip_frag_s
{
    struct hlist_node	hlist;
    unsigned int 	saddr;
    unsigned int 	daddr;
    unsigned short 	id;
    unsigned short	frag_off_next;
    unsigned char 	proto;
    
    union header{
    	struct tcp_hdr 	_tcph;
        struct udp_hdr	_udph;
        unsigned char	buf[32];
    } hdr;
    
    struct rte_timer	timeout;
    //rte_spinlock_t		lock;
    unsigned int	queue_size;
    frag_list_head_t 	frag_queue;
    
    struct sess_hash    *hash;
    uint64_t    timestamp;
}ip_frag_t;

struct ip_frag_hash_node
{
	struct hlist_head hhead;
	//rte_spinlock_t lock;  
};

struct ip_frag_globle
{
    struct ip_frag_hash_node ksft_hash_ipfrag[HASH_IPFRAG_SIZE];
    unsigned int sft_hash_rnd;
};

struct sess_hash
{
    uint64_t max_hash_size;
    uint64_t max_entry_size;
    uint64_t curr_entry_count;
    uint32_t sess_hash_rnd;
    
#ifdef SESSION_CACHE_HASH
    struct h_table h_table;
	struct rte_mempool *mem_cache;
#else
    struct hash_entry *hash_buf;
#endif

    struct ip_frag_globle globle;
};

void session_load_conf(void);

int session_mgr_init(void);
int session_fwd_handle(struct rte_mbuf *m, unsigned portid);
void session_mgr_uninit(void);

void init_ip_frag(struct ip_frag_globle *globle);
int ip_frag_handle(struct rte_mbuf *skb, void *hdr, struct sess_hash *hash);

#ifdef SESSION_CACHE_HASH
struct sess_hash * hash_init_with_cache(void);
void hash_uninit_with_cache(struct sess_hash *hash);
struct sft_fdb_entry *session_hash_cache_try_get(struct sess_hash *hash, struct key_5tuple *key);
int session_hash_lookup_behavior(struct sess_hash *hash, struct key_5tuple *key);
#else
typedef int (*ipaddr_hash)(uint32_t sip, uint32_t dip,
                           uint16_t source, uint16_t dest,
                           uint8_t proto, uint64_t max_hash_size, uint32_t hash_rnd);
struct sess_hash * hash_init_with_vect(void);
void hash_uninit_with_vect(struct sess_hash *hash);
struct sft_fdb_entry * hash_find_and_new(struct sess_hash *hash, struct key_5tuple *key, ipaddr_hash hash_5tuple);
struct sft_fdb_entry * hash_find(struct sess_hash *hash, struct key_5tuple *key, ipaddr_hash hash_5tuple);
int ipaddr_hash_func(uint32_t sip, uint32_t dip,
                     uint16_t source, uint16_t dest,
                     uint8_t protocol, uint64_t hash_size, uint32_t hash_rnd);
#endif
int send_session_to_remote(uint8_t core_id, 
    int (*process)(uint8_t, void *));

int update_session_to_shm(uint8_t core_id);
#endif

