
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <rte_ring.h>
#include <rte_cycles.h>
#include <rte_mempool.h>
#include <rte_spinlock.h>
#include <rte_malloc.h>
#include <rte_jhash.h>


#include "pattern_tbl.h"
#include "utils.h"
#include "com.h"
#include "init.h"
#include "dpi.h"
#include "ftp.h"
#include "acsmx2.h"
#include "debug.h"
#include "rules.h"
#include "dpi_dynamic.h"
#include "dpi_login_analysis.h"
#include "shared_user_tbl.h"
#include "stat.h"

#define PATTERN_HASH_SIZE    (1 << 10)
#define PATTERN_MAX_LEN      (PATTERN_HASH_SIZE * 4)
#define PATTERN_TIMEOUT      (120)

//#define CLEAN_STAT
 
//ACSM_STRUCT2 * pv._acsm[ACSM_MAX_NUM];
static uint32_t g_hash_initval;


static inline int update_proto_mark(struct ssn_skb_values *ssv,unsigned int newmark,int flag)
{
//    if(NULL == ssv)
//        return;

//    if(NULL == ssv->ssn)
//        return;
    
    if(newmark == ssv->ssn->proto_mark)
    {
        return 1;
    }
    ssv->ssn->proto_mark = newmark;
    ssv->ssn->proto_mark_state = 1;
    //ssv->ssn->need_update_link = 1;
    //ssv->ssn->identifies_proto = 1;

#if 0
    if(sft_dpi_print_flag == 1)
    {
        if(flag == 0)
        {
            int big_type,mid_type,sub_type;
            big_type = (ssv->ssn->proto_mark & BIG_TYPE_MASK) >> 28;
            mid_type = (ssv->ssn->proto_mark & MID_TYPE_MASK) >> 24;
            sub_type = (ssv->ssn->proto_mark & SUB_TYPE_MASK) >> 16;
            D("l4proto:%d,%pI4:%d-->%pI4:%d,proto_mark:%u[0x%02x],[%s],state_cnt:%d.\n",
                    ssv->l4proto,&ssv->sip,ntohs(ssv->sport),
                    &ssv->dip,ntohs(ssv->dport),ssv->ssn->proto_mark,ssv->ssn->proto_mark,
                    DPI_Statistics[big_type][mid_type][sub_type].name,
                    ssv->ssn->state_cnt);
        }
    }
#endif
    return 0;
}



struct pattern_cache_table
{
	struct h_table h_table;
	struct rte_mempool *mem_cache;
} pattern_cache_tbl;


static uint32_t pattern_cache_hash_fn(void *key)
{
	struct pattern_key *k = (struct pattern_key *)key;                                                              
    return rte_jhash(k, sizeof(*k), g_hash_initval);
}
static int pattern_cache_comp_key_fn(struct h_scalar *he, void *key)
{
	pattern_t *pattern = container_of(he, pattern_t, h_scalar);
	//struct pattern_key *k = (struct pattern_key *)key;
    
    return memcmp((char *)&pattern->pattern_key ,(char *)key, sizeof(struct pattern_key));
}

static void pattern_cache_release_fn(struct h_scalar *he)
{
	pattern_t *pattern = container_of(he, pattern_t, h_scalar);
    rte_mempool_sp_put(pattern_cache_tbl.mem_cache, pattern);
}

static char *pattern_cache_build_line_fn(struct h_scalar *he)
{
	pattern_t *pattern = container_of(he, pattern_t, h_scalar);
	char *line, *dp;
	
	if((line = (char *)malloc(200)) == NULL)
		return NULL;
    char *s;	
    snprintf(line, 150, "%s--%s\n", pattern->pattern_name, pattern->pattern_key.pattern);
	return line;
}


static int __load_pattern(struct pattern_cache_table *pt)
{
//    FILE *file = fopen();
    return 0;
}

/*******************************************************************/
static inline int load_pattern(struct pattern_cache_table *pt)
{
    __load_pattern(pt);
    return 0;
}

/*******************************************************************/
static int pattern_operate_cmd(struct h_table *ht, const char *cmd)
{
    struct pattern_cache_table *pt = container_of(ht, struct pattern_cache_table, h_table);
    size_t len = strlen(cmd);
    if(cmd == NULL || len == 0)
        return -EINVAL;

    if(strncmp(cmd, "clear", 5) == 0)
    {
        D("clear app rules\n");
        h_scalar_table_clear(ht);
        return 0;
    }

    if(strncmp(cmd, "load", 5) == 0) {
        load_pattern(pt);
    }
    return 0;
}

void pattern_clear()
{

    D("clear app rules\n");
    h_scalar_table_clear(&pattern_cache_tbl.h_table);
}

static struct h_scalar_operations pattern_cache_ops =
{
	.hash        = pattern_cache_hash_fn,
	.comp_key    = pattern_cache_comp_key_fn,
	.release     = pattern_cache_release_fn,
	.build_line  = pattern_cache_build_line_fn,
    .operate_cmd = pattern_operate_cmd,

};


static struct h_scalar *pattern_cache_create_fn(struct h_table *ht, void *key)
{
	struct pattern_key *k = (struct pattern_key *)key;
	pattern_t *pattern;
	
    if(rte_mempool_sc_get(pattern_cache_tbl.mem_cache, (void **)&pattern) <0)
		return NULL;
    pattern->pattern_key = *k;
//    pattern->id ++;
	return &pattern->h_scalar;
}

pattern_t *pattern_cache_try_get(struct pattern_key *key, int *ret_status)
{
	struct h_scalar *he;
	pattern_t *pattern;
    
    *ret_status = h_scalar_try_get(&pattern_cache_tbl.h_table, key, &he, pattern_cache_create_fn, NULL, 1, 0);
	return ( pattern_t *)container_of(he, pattern_t, h_scalar);
}

static void __pattern_cache_lookup_action(struct h_scalar *he, void *key, void *result)
{
    pattern_t *pattern = container_of(he, pattern_t, h_scalar);
    *(uint64_t *)result =  pattern;
}



pattern_t *pattern_lookup_by_key(const struct pattern_key *key)
{
    pattern_t *pattern;
    if(h_scalar_try_lookup(&pattern_cache_tbl.h_table, (void *)key, &pattern, __pattern_cache_lookup_action) == 0)
        return pattern;
    return NULL;
}

void *__look_pattern_node(struct h_scalar *he, void *priv)
{
    char *name = (char *)priv;
    pattern_t *node = container_of(he, pattern_t, h_scalar);

    size_t cmp_len = (strlen(node->place_name) > strlen(name) ? strlen(node->place_name): strlen(name));

    if (!strncmp_nocase(node->place_name, name, cmp_len))
        return (void *)node;
    return NULL;
//    rcu_read_unlock();
}


/* not found return -1, else return the place id */
int get_place_id_by_name(const char * name)
{
    pattern_t *node;
    if ((node =(pattern_t *) h_scalar_lookup_by_user(&pattern_cache_tbl.h_table, (void *)name, __look_pattern_node)))
        return node->pattern_key.place_token_id;
    return -1;
}

pattern_t *get_pattern_by_name(const char * name)
{
    return h_scalar_lookup_by_user(&pattern_cache_tbl.h_table, (void *)name, __look_pattern_node);
}

/*******************************************************************/
void __build_pattern_acsm(struct h_scalar *he)
{
    ACSM_STRUCT2 * acsm = NULL;
    pattern_t *pattern = container_of(he, pattern_t, h_scalar);
    if (pattern) {

        if (pattern->id < pv.place_cnt)
        {//GET,HEAD,POST,HOST
        
        if (pattern->id < FIRSTN_TOKEN)
                acsmAddPattern2(pv._acsm[REQ_HTTP_URL], pattern->pattern_key.pattern, pattern->pattern_key.pattern_len, pattern->pattern_key.nocase, 0, 0, pattern, pattern->id);
            acsmAddPattern2(pv._acsm[REQ_HTTP], pattern->pattern_key.pattern, pattern->pattern_key.pattern_len, pattern->pattern_key.nocase, 0, 0, pattern, pattern->id);
            acsmAddPattern2(pv._acsm[RES_HTTP], pattern->pattern_key.pattern, pattern->pattern_key.pattern_len, pattern->pattern_key.nocase, 0, 0, pattern, pattern->id);
        } else {
            if (unlikely(pattern->pattern_key.repeated > 0)) {
                return;
            }   
            if (pattern->pattern_key.pattern_len == 0) {
                return;
            }
            /* HTTP */
                if(pattern->pattern_key.proto_type == DPI_PROTO_HTTP)
                {
                    if(pattern->pattern_key.pkt_dir == 1) //req
                    {
                        acsm = pv._acsm[REQ_HTTP];
                    }
                    else
                    {
                        acsm = pv._acsm[RES_HTTP];
                    }
                }
                else if(pattern->pattern_key.proto_type == DPI_PROTO_TCP)
                {
                    if(pattern->pattern_key.pkt_dir == 1) //req
                    {
                        acsm = pv._acsm[REQ_TCP];
                    }
                    else
                    {
                        acsm = pv._acsm[RES_TCP];
                    }
                }
                else if(pattern->pattern_key.proto_type == DPI_PROTO_UDP)
                {
                    if(pattern->pattern_key.pkt_dir == 1) //req
                    {
                            acsm = pv._acsm[REQ_UDP];
                    }
                    else
                    {
                            acsm = pv._acsm[RES_UDP];
                    }
                }
                else
                {
                    return;
                }

            acsmAddPattern2(acsm, pattern->pattern_key.pattern, pattern->pattern_key.pattern_len, pattern->pattern_key.nocase, 0, 0, pattern, pattern->id);

        }
    }
}
int build_pattern_acsm()
{
    


    h_scalar_table_iterate_safe(&pattern_cache_tbl.h_table,  __build_pattern_acsm);

	if(acsmCompile2(pv._acsm[REQ_HTTP]) == -1)
	{
		E(",Patterns compile err pv._acsm[REQ_HTTP].\n");
		return -1;
	}
	else
	{
		E(",Patterns compile pv._acsm[REQ_HTTP] OK!\n");
	}
	if(acsmCompile2(pv._acsm[RES_HTTP]) == -1)
	{
		E(",Patterns compile err pv._acsm[RES_HTTP].\n");
		return -1;
	}
	else
	{
		E(",Patterns compile pv._acsm[RES_HTTP] OK!\n");
	}

	if(acsmCompile2(pv._acsm[REQ_TCP]) == -1)
	{
		E(",Patterns compile err pv._acsm[REQ_TCP]\n");
		return -1;
	}
	else
	{
		E(",Patterns compile pv._acsm[REQ_TCP] OK!\n");
	}
	if(acsmCompile2(pv._acsm[RES_TCP]) == -1)
	{
		E("Patterns compile err pv._acsm[RES_TCP]\n");
		return -1;
	}
	else
	{
		E(",Patterns compile pv._acsm[RES_TCP] OK!\n");
	}

	if(acsmCompile2(pv._acsm[REQ_UDP]) == -1)
	{
		E("Patterns compile err pv._acsm[REQ_UDP]\n");
		return -1;
	}
	else
	{
		E(",Patterns compile pv._acsm[REQ_UDP] OK!\n",__FUNCTION__,__LINE__);
	}
	if(acsmCompile2(pv._acsm[RES_UDP]) == -1)
	{
		E("Patterns compile err pv._acsm[RES_UDP]\n");
		return -1;
	}
	else
	{
		//Print_DFA(pv._acsm[RES_UDP]);
		E("Patterns compile pv._acsm[RES_UDP] OK!\n");
		//Write_DFA(acsm, "acsmx2-snort.dfa");
		//acsmPrintInfo2(pv._acsm[RES_UDP]);
	}

	if(acsmCompile2(pv._acsm[REQ_HTTP_URL]) == -1)
	{
		E("Patterns compile err pv._acsm[REQ_HTTP_URL]\n");
		return -1;
	}
	else
	{
		E(",Patterns compile pv._acsm[REQ_HTTP_URL] OK!\n");
	}
	pv.acsm_enable = 1;


}
static void
my_obj_init(struct rte_mempool *mp, __attribute__((unused)) void *arg,
        void *obj, unsigned i)
{
    uint32_t *objnum = obj;
    memset(obj, 0, mp->elt_size);
    *objnum = i;
}
static void my_mp_init(struct rte_mempool * mp, __attribute__((unused)) void * arg)
{
    printf("mempool name is %s\n", mp->name);
    /* nothing to be implemented here*/
    return ;
}

int pattern_cache_init(void)
{
	int ret, retv = -1;
	
    srand(time(NULL));
    g_hash_initval = random();
    printf("---------------------pattern_cache_init------------------------------\n");	

    /* create a mempool (with cache) */
    if ((pattern_cache_tbl.mem_cache = rte_mempool_create("pattern_mem_cache", PATTERN_MAX_LEN,
                sizeof(pattern_t),
                32, 0,
                my_mp_init, NULL,
                my_obj_init, NULL,
                SOCKET_ID_ANY,  MEMPOOL_F_NO_CACHE_ALIGN | MEMPOOL_F_SP_PUT | MEMPOOL_F_SC_GET)) == NULL)
    	{
		retv = -ENOMEM;
		goto err_kmem_create;
	}

    if(h_scalar_table_create(&pattern_cache_tbl.h_table, NULL, PATTERN_HASH_SIZE, PATTERN_MAX_LEN, 0, &pattern_cache_ops) != 0){
		retv = ret;
		goto err_pattern_create;
    }
	
	return 0;

	
err_pattern_create:
//	kmem_cache_destroy(pattern_cache_tbl.mem_cache);
err_kmem_create:
//	remove_proc_entry("pattern", proc_dynamic);
	return retv;
}
int init_acsm()
{
    int i, k;
    struct list_head * pos, * n;
    pattern_t * p = NULL;
    place_t * q = NULL;
    ACSM_STRUCT2 * acsm = NULL;

//    max_memory = 0;
    /* create pattern acsm */
    for(i = 0; i < ACSM_MAX_NUM; i++)
    {
        pv._acsm[i] = acsmNew2();
        if(!pv._acsm[i])
        {
            E(",acsm no memory %d\n", i);
            return -1;
        }

        pv._acsm[i]->acsmFormat = ACF_FULL;
        pv._acsm[i]->acsmEnd = ACT_NOEND_MATCH;
    }
    build_pattern_acsm();
}
inline int check_pattern(pattern_t * pattern ,struct ssn_skb_values * ssv,int offset)
{
//    if((NULL == ssv) || (NULL == pattern))
//        return -1;
    
    if ((pattern->pattern_key.sport & ssv->sport) != pattern->pattern_key.sport || 
         (pattern->pattern_key.dport & ssv->dport) != pattern->pattern_key.dport)
        return -1;
#if 0
    if (pattern->pattern_key.sport) {
    printf("pattern->sport=%u,ssv->sport=%u\n",pattern->pattern_key.sport,ssv->sport);

    }

    if (pattern->pattern_key.dport) {
    printf("pattern->dport=%u,ssv->dport=%u\n",pattern->pattern_key.dport,ssv->dport);

    }
#endif

    if(pattern->pattern_key.offset)
    {
        if(pattern->pattern_key.offset > 0)
        {
            if(offset != pattern->pattern_key.offset + ssv->ssn->offset)
            {
                //D("%s(%d),text offset:%d, pattern_offset:%d.\n",__FUNCTION__,__LINE__,offset,pattern->pattern_key.offset);
                //                D("%s(%d),text offset:%d, pattern:%s.\n",__FUNCTION__,__LINE__,offset,pattern->pattern_key.pattern);
                return -1;
            }
            //D("%s(%d),text offset:%d, pattern_offset:%d.\n",__FUNCTION__,__LINE__,offset,pattern->pattern_key.offset);
        }
        else
        {
            if(offset != ssv->payload_len + pattern->pattern_key.offset + ssv->ssn->offset)
            {
                return -1;
            }
        }
    }


    if(pattern->pattern_key.depth > 0)
    {
        if(offset + pattern->pattern_key.pattern_len > pattern->pattern_key.depth + ssv->ssn->offset)
            return -1;
    }

#if 0
    if (pattern->pattern_key.pkt_len_min &&(pattern->pattern_key.pkt_len_max == pattern->pattern_key.pkt_len_min)) {
        return (ssv->payload_len ^ pattern->pattern_key.pkt_len_min);
    }
#endif
    if ((pattern->pattern_key.pkt_len_max && ssv->payload_len > pattern->pattern_key.pkt_len_max) || 
        (pattern->pattern_key.pkt_len_min && ssv->payload_len < pattern->pattern_key.pkt_len_min)) {
        return -1;
    }
    
    return 0;
}

/*study bt peers list from http/udp bt tracker server */
static inline void study_bt_peers(void * data, pattern_t * pattern)
{
    struct ssn_skb_values * ssv = (struct ssn_skb_values *)data;
    if((NULL == ssv) || (NULL == pattern))   
        return;        
    if((0 == ssv->payload_len) || (NULL == ssv->payload))
        return;
    
    int bt_httptracker_flag = strcmp_nocase(pattern->pattern_name, "BT_httptracker_peers");
    int bt_udptracker_flag = strcmp_nocase(pattern->pattern_name, "BT_udptracker_peers");
    //if not bt tracker way, just return 
    if((0 != bt_httptracker_flag) && (0 != bt_udptracker_flag))
        return;

    int i =0;
    unsigned char *bt_payload_peers_list = NULL;
    int peers_list_length = 0;
    int peers_addr_num = 0;
    int peers_addr_statis = 0;

    uint16_t peer_port = 0;
    uint32_t peer_ip = 0;

    //if bt tracker way get peers list, and then insert into study hash table
    if(0 == bt_httptracker_flag)
    {
        unsigned char *bt_payload = ssv->payload;
        unsigned char *bt_payload_peers = strstr(bt_payload, "peers");
        if(NULL == bt_payload_peers)
            return;

        bt_payload_peers_list = strchr(bt_payload_peers, ':');
        if(NULL == bt_payload_peers_list || (bt_payload_peers_list - bt_payload >= ssv->payload_len))
            return;

        bt_payload_peers_list++;

        peers_list_length = ssv->payload_len - (bt_payload_peers_list - bt_payload + 1);
    }
    else if (0 == bt_udptracker_flag)
    {		
        if(ssv->payload_len <= 20)
            return;
        
        bt_payload_peers_list = ssv->payload + 20;
        if(NULL == bt_payload_peers_list)
            return;
        
        peers_list_length = ssv->payload_len - 20;
    }
    
    if((peers_list_length <= 0) || (peers_list_length >= ssv->payload_len))
            return;

#if 0
    struct in_addr addr_src, addr_dst;
    memset(&addr_src,0,sizeof(addr_src));
    memset(&addr_dst,0,sizeof (addr_dst));
    memcpy(&addr_src, &ssv->ssn->sess_key.ip_src, 4);
    memcpy(&addr_dst, &ssv->ssn->sess_key.ip_dst, 4);
    printf("[%s][%d]---->", inet_ntoa(addr_src), ntohs(ssv->ssn->sess_key.port_src));
    printf("[%s][%d]\n",  inet_ntoa(addr_dst), ntohs(ssv->ssn->sess_key.port_dst));

    printf("print app[%d] peers list:\n",ssv->ssn->proto_mark);
#endif
    peers_addr_num = peers_list_length / 6; //ip + port = 6 bytes
    peers_addr_statis = 0;
    for(i =0; (i < peers_list_length) && (peers_addr_statis < peers_addr_num); i += 6)
    {
        peers_addr_statis++;
        peer_port = ((bt_payload_peers_list[i+5] << 8) | bt_payload_peers_list[i+4]);
        if(0 == peer_port)
        {
            continue;
        }

        peer_ip = ((bt_payload_peers_list[i+3] << 24) | (bt_payload_peers_list[i+2] << 16) | (bt_payload_peers_list[i+1] << 8) | bt_payload_peers_list[i]);

#if 0		
        printf("[ %d ] %x %x %x %x %x %x ==>", peers_addr_statis,
        bt_payload_peers_list[i], bt_payload_peers_list[i+1], bt_payload_peers_list[i+2],
        bt_payload_peers_list[i+3], bt_payload_peers_list[i+4], bt_payload_peers_list[i+5]);

        struct in_addr peer_ip_addr;
        memset(&peer_ip_addr,0,sizeof(peer_ip_addr));
        memcpy(&peer_ip_addr, &peer_ip, 4);
        printf("[%s : %d]\n", inet_ntoa(peer_ip_addr), ntohs(peer_port));
#endif
        study_cache_try_get(peer_ip, peer_port, ssv->ssn->proto_mark, 0); 
    }

    return;
}

static const char *pn[] = {
"get_serverlist", "dotachuanqi_get_backdoor" ,"dotachuanqi_host_dota", "dotachuanqi_body_ip"

};
static inline int deal_search_key(void * _pattern, int offset, void * data, void * arg)
{
    int i;
    int cnt;
    uint16_t next_state_id;
    uint16_t *id_list = NULL;
    pattern_t * pattern = (pattern_t *)_pattern;
    struct ssn_skb_values * ssv = (struct ssn_skb_values *)arg;

    if((NULL == ssv) || (NULL == pattern))
        return -1;

    if(pattern->id < TOKEN_MAX)
    {//place pattern

        if(ssv->ssn->place_token_list_cnt >= (TOKEN_MAX-1))
        {
            return -1;
        }
#if 0
        ssv->ssn->place_token_list[ssv->ssn->place_token_list_cnt] = pattern->id;
        ssv->ssn->place_token_list_cnt++;
#else
        ssv->ssn->place_token_list[0] = pattern->id;
//        ssv->ssn->place_token_list_cnt++;
#endif

        if (pattern->id == 3)
        ssv->ssn->offset = offset + strlen(pattern->pattern_key.pattern);
//        printf("offset=%d[%s]pattern->id=%d\n",ssv->ssn->offset,pattern->pattern_key.pattern, pattern->id); 
        switch(pattern->id)
        {
            case HTTP_TOKEN_GET_ID:
                ssv->http_token_method = HTTP_TOKEN_GET;
                break;
            case HTTP_TOKEN_HEAD_ID:
                ssv->http_token_method = HTTP_TOKEN_HEAD;
                break;
            case HTTP_TOKEN_POST_ID:
                ssv->http_token_method = HTTP_TOKEN_POST;
                break;
            case HTTP_TOKEN_LOCATION_ID:
                //ssv->http_token_host_offset = offset;
                ssv->http_token_method = HTTP_TOKEN_LOCATION;
                break;

            case HTTP_TOKEN_OPTIONS_ID:
                ssv->http_token_method = HTTP_TOKEN_OPTIONS;
                break;
            case HTTP_TOKEN_HOST_ID:
                ssv->http_token_host_offset = offset;
                break;

            default:
                break;
        }
        return 0;
    }
    else
    {//other pattern
        if(pattern->pattern_key.place_token_id != -1)
        {//with place token(HTTP proto)
#if 0 //zebra
            if(ssv->ssn->place_token_list_cnt == 0)
            {
                return -1;
            }
            if(ssv->ssn->place_token_list[ssv->ssn->place_token_list_cnt-1] == pattern->pattern_key.place_token_id)
            {
#if 0
                printf("%s(%d),0x%p,%pI4:%d-->%pI4:%d,%d,[%d],[%d]\n",\
                __FUNCTION__,__LINE__, ssv->ssn,&ssv->sip,ntohs(ssv->sport),&ssv->dip,ntohs(ssv->dport),\
                ssv->place_token_list_cnt,ssv->place_token_list[ssv->place_token_list_cnt-1],pattern->pattern_key.place_token_id);
#endif
            }
            else
            {
                return -1;
            }
#else
            if(ssv->ssn->place_token_list[0] ^ pattern->pattern_key.place_token_id)
                return -1;
#endif

        }
        else
        {//without place token
        }
    }
    if(check_pattern(pattern,ssv,offset))
    {
        return -1;
    }//pattern_t

 #if 0
   if (strncmp(pattern->pattern_name,"baidutieba_post_post",strlen("baidutieba_post_post")) == 0) {
        *test ++;
        printf("baidutieba_post_post\n");
    }
    if (strncmp(pattern->pattern_name,"baidutieba_host_tieba",strlen("baidutieba_host_tieba")) == 0) {
        *test ++;
        printf("baidutieba_host_tieba\n");
    }

    if (strncmp(pattern->pattern_name,"PC-post-baidutieba_Cookie_uid",strlen("PC-post-baidutieba_Cookie_uid")) == 0) {
        *test ++;
        printf("PC-post-baidutieba_Cookie_uid\n");
    }
    if (*test == 3) {
        int aaab=0;
        printf("aaabbccc\n");
    }
    if (strncmp(pattern->pattern_name,"IOS-post-sinaweibo_uid",strlen("IOS-post-sinaweibo_uid")) == 0) {
        printf("pattern:IOS-post-sinaweibo_uid\n");
    }
   if (strncmp(pattern->pattern_name,"qq_msg_start_udp_req",strlen("qq_msg_start_udp_req")) == 0) {
        printf("qq_msg_start_udp_req\n");
    }

    if (strncmp(pattern->pattern_name,"qq_request_key",strlen("qq_request_key")) == 0) {
        printf("qq_request_key\n");
    }

    if (strncmp(pattern->pattern_name,"qq_msg_end_udp_req",strlen("qq_msg_end_udp_req")) == 0) {
        printf("qq_msg_end_udp_req\n");
    }
    if (strncmp(pattern->pattern_name,"qq_msg_start_udp_res",strlen("qq_msg_start_udp_res")) == 0) {
        printf("qq_msg_start_udp_res\n");
    }
#endif
    cnt = ssv->ssn->state_cnt;
    id_list = ssv->ssn->dpi_state_id_list;
#ifdef CLEAN_STAT
    int keep_cnt = cnt;
    int keep_cnt_flag = 0;
    int indentify = 0;
#endif
    for(i = 0; i < ssv->ssn->state_cnt; i++)//±éÀúid_list
    {
        next_state_id = DPI_State_TBL[id_list[i]].pattern[pattern->id].nextstate_id;

        if(0 == next_state_id)
        {
            continue;
        }
        
        if(DPI_State_Flag_TBL[next_state_id] & 0x1)//final state
        {
            if (pattern->pattern_key.tlv_len == 0) {
#ifdef CLEAN_STAT
                keep_cnt_flag = update_proto_mark(ssv,DPI_State_TBL[next_state_id].type_id,1);
#else
                update_proto_mark(ssv,DPI_State_TBL[next_state_id].type_id,1);
#endif
                if (pv.urlog && pattern->pattern_key.dynamic_type & 0x80) {
                    login_parse(ssv, pattern, offset, 1);
#ifdef CLEAN_STAT
                    if (keep_cnt_flag == 1) 
                        keep_cnt_flag = DPI_State_TBL[next_state_id].type_id;
#endif
                } else if (pattern->pattern_key.dynamic_type && (0 == ssv->haved_dynamic)) {
                    if (pattern->pattern_key.mult_iplist)
                    {
                        ssv->ssn->pattern_ptr = (void *)pattern;
                    }
                    ssv->haved_dynamic = 1;
                    dynamic_parse(ssv, pattern, offset);
                }
#if 0
                if (ssv->ssn->ssn_ptr1) {
                    rte_free(ssv->ssn->ssn_ptr1);
                    ssv->ssn->ssn->ptr1 = NULL;
                }
                if (ssv->ssn->ssn_ptr2) {
                    rte_free(ssv->ssn->ssn_ptr2);
                    ssv->ssn->ssn->ptr2 = NULL;
                }
#endif
            } else {
                uint16_t tlv16_packet_len;
                uint32_t tlv32_packet_len;
                if (pattern->pattern_key.tlv_len == 2) {
                    if (pattern->pattern_key.tlv_type == 1) {
                        tlv16_packet_len = *(uint16_t *)(ssv->payload + pattern->pattern_key.tlv_start) + pattern->pattern_key.tlv_offset;
                    } else {
                        tlv16_packet_len = htons(*(uint16_t *)(ssv->payload + pattern->pattern_key.tlv_start)) + pattern->pattern_key.tlv_offset;
                    }
                    //printf("tlv16_packet_len=%u \n",tlv16_packet_len);
                    if (tlv16_packet_len == ssv->payload_len ) {
                        update_proto_mark(ssv,DPI_State_TBL[next_state_id].type_id,1);
                    }

                } else if (pattern->pattern_key.tlv_len == 4) {
                    if (pattern->pattern_key.tlv_type == 1) {
                        tlv32_packet_len = *(uint32_t *)(ssv->payload + pattern->pattern_key.tlv_start) + pattern->pattern_key.tlv_offset;
                    } else {
                        tlv32_packet_len = htonl(*(uint32_t *)(ssv->payload + pattern->pattern_key.tlv_start)) + pattern->pattern_key.tlv_offset;
                    }
                    //printf("tlv32_packet_len=%u\n",tlv32_packet_len);
                    if (tlv32_packet_len == ssv->payload_len ) {
                        update_proto_mark(ssv,DPI_State_TBL[next_state_id].type_id,1);
                    }

                }

            }
#ifdef CLEAN_STAT
            if ((DPI_State_TBL[next_state_id].type_id &0x3 == 0x3) && keep_cnt_flag >100) {
                indentify = 1;
            }
#endif

        } else if ( pv.urlog && pattern->pattern_key.dynamic_type & 0x80){
                login_parse(ssv, pattern, offset, 0);
        }
        id_list[cnt] = next_state_id;
        cnt++;
        if(cnt >= MAX_SSN_STATE_NUM)
        {
            skb_stat->exceed_ac_state[rte_lcore_id()] ++;            
            //            printf("%s(%d),error,id_list cnt:%d,maxlimit:%d pattern[%s].\n",__FUNCTION__,__LINE__,cnt,MAX_SSN_STATE_NUM, pattern->pattern_name);
            return -1;
        }
    }//for loop
#ifdef CLEAN_STAT
    if (indentify&&keep_cnt_flag>100) {
        id_list[keep_cnt] = 0;
        //bzero(id_list, sizeof(id_list[0]) * cnt);
        ssv->ssn->state_cnt = keep_cnt;
    } else {
        ssv->ssn->state_cnt = cnt;
    }
#else
        ssv->ssn->state_cnt = cnt;
#endif
    if((0==strcmp_nocase(pattern->pattern_name, "BT_httptracker_peers")) ||
        (0 == strcmp_nocase(pattern->pattern_name, "BT_udptracker_peers")))
    {
        study_bt_peers(ssv, pattern);
    }

#if 0
    if(pv.qq_login_appid == ssv->ssn->proto_mark)                                                                   
    {                                                                                                               
        //if(0==strcmp_nocase(pattern->pattern_name, "qq_request_key") && (81 == rte_pktmbuf_pkt_len(ssv->skb)))    
       increase_statistics_qq(ssv); 
#if 0
        FILE *fp = NULL;                                                                                            
        fp = fopen("ip_qq.log", "a+");                                                                              
        if (fp != (FILE *) NULL)                                                                                    
        {                                                                                                           
            struct sft_fdb_entry * ssn;                                                                             
            ssn = (struct sft_fdb_entry *)ssv->ssn;                                                                 
            if(ssn != NULL)                                                                                         
            {                                                                                                       
                if(81 == rte_pktmbuf_pkt_len(ssv->skb))                                                             
                {                                                                                                   
                    //statistics_qq(ssv);                                                                             
                    if(ssv->ssn->qq > 0)                                                                            
                    {                                                                                               
                        fprintf(fp, "[%u.%u.%u.%u] ->%u\n", IPQUADS(ssv->ssn->sess_key.ip_src), ssv->ssn->qq);      
                    }                                                                                               
                }                                                                                                   
                else                                                                                                
                {                                                                                                   
                    fprintf(fp, "pattern = %s\n", pattern->pattern_name);                                           
                }                                                                                                   
            }                                                                                                       
            fclose(fp);                                                                                             
        }
 #endif
    }                                                                                                               
#endif

#if 0
    ssv->ssn->pattern_ptr = (void *)pattern;
    if (pattern->pattern_key.dynamic_type && ssv->ssn->pattern_id == pattern->id) {
    dynamic_parse(ssv, pattern, offset);
    }
#endif
    return 0;
}
static int test_flag = 0;
void dpi_acsm_search(struct ssn_skb_values * ssv)
{
    if(NULL == ssv)
        return;
    
    ACSM_STRUCT2 * acsm = NULL;
    void *acsm_p = NULL;
#if 0
    if(!acsm_enable) //现在不可能有这种情况
    {
        return;
    }
#endif

    if(ssv->l4proto == IPPROTO_TCP)
    {
        if(ssv->isinner)//uplink
        {
            if(ssv->ssn->is_http)
            {


                acsm = pv._acsm[REQ_HTTP];

            }
            else
            {
                acsm = pv._acsm[REQ_TCP];
            }
        }
        else
        {//downlink
            if(ssv->ssn->is_http)
            {
                acsm = pv._acsm[RES_HTTP];

            }
            else
            {
                acsm = pv._acsm[RES_TCP];
            }
        }
    }
    else if(ssv->l4proto == IPPROTO_UDP)
    {
        if(ssv->isinner)//uplink
        {
            acsm = pv._acsm[REQ_UDP];
        }
        else//downlink
        {
            acsm =  pv._acsm[RES_UDP] ;
        }
    }
    if(acsm == NULL)
    {

	    rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "%s(%d), udp,acsm is NULL\n", __FUNCTION__, __LINE__);
        return;
    }
#if 0
    if (substr_in_mainstr_nocase(ssv->payload, ssv->payload_len, "/tb/static-common/html/pass/", 0) > 0) {
        test_flag = 55;
        D("[%u.%u.%u.%u->%u.%u.%u.%u][%s]\n",IPQUADS(ssv->sip), IPQUADS(ssv->dip), ssv->payload);
    }
#endif
    acsmSearch2(acsm, ssv->payload, ssv->payload_len, deal_search_key, NULL, ssv);


    if ((0 == ssv->haved_dynamic) && ssv->ssn->pattern_ptr) 
    {
        pattern_t *pattern = (pattern_t *)ssv->ssn->pattern_ptr;
//	    if (pattern->pattern_key.dynamic_type && pattern->pattern_key.mult_iplist) 
        {
		    dynamic_parse(ssv, pattern, 0);
	  }

    }

    //acsmSearch2(acsm, ssv->payload, ssv->payload_len, deal_search_key, (void *)&test_flag, ssv);
}
int cleanup_acsm(void)
{
    int i;
    D("start,cpuid:,app_class_cnt:%d,\n", pv.app_class_cnt);
    D("Cleanup acsm start,acsm_enable:%d.\n", pv.acsm_enable);
    if(pv.acsm_enable)
    {
        pv.acsm_enable = 0;
        for(i = 0; i < ACSM_MAX_NUM && pv._acsm[i] != NULL; i++)
        {
            acsmFree2(pv._acsm[i]);
        }
    }

    D("Cleanup acsm.\n");
    //max_memory = 0;
    return 0;
}
void pattern_cache_exit(void)
{
	
	h_scalar_table_release(&pattern_cache_tbl.h_table);
	//kmem_cache_destroy(pattern_cache_tbl.mem_cache);
}
