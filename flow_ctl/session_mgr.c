#include <arpa/inet.h>

#include <rte_ether.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_jhash.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_ethdev.h>
#include <rte_ip_frag.h>
#include <rte_cycles.h>

#include "utils.h"
#include "session_mgr.h"
#include "session_config.h"
#include "global.h"
#include "tcp_pack.h"
#include "stat.h"
#include "init.h"

#define RTE_LOGTYPE_L2CT RTE_LOGTYPE_USER1

#define SES_EXPIRED_TIME 60

#define STEP_EXT_SIZE 5
#define MAX_EXT_SIZE  15

#define SUCCESS 0
#define FAILED -1

//#define DEBUG_YHL
 
#ifdef DEBUG_YHL
#define SESS_DEBUG_LOG(x,args...) \
    do{                         \
        printf(x, ##args);      \
    } while(0)
#else
#define SESS_DEBUG_LOG(...)
#endif

#define RTE_NIPQUAD(addr) \
    ((unsigned char *)&addr)[0], \
    ((unsigned char *)&addr)[1], \
    ((unsigned char *)&addr)[2], \
    ((unsigned char *)&addr)[3]

static uint64_t error_log = 0;
static uint64_t timestamp_log = 0;

static uint64_t HZ = 0;
extern uint64_t pcap_dump_file_size;
extern dump_pcaps_t dump_pcaps;
static void sess_init(struct sft_fdb_entry *fdb, struct key_5tuple *key)
{
    if(!fdb || !key)
    {
        return;
    }

    fdb->timestamp = rte_rdtsc();
    fdb->link_timestamp = fdb->timestamp;
    fdb->sess_key.ip_src = key->ip_src;
    fdb->sess_key.ip_dst = key->ip_dst;
    fdb->sess_key.port_src = key->port_src;
    fdb->sess_key.port_dst = key->port_dst;
    fdb->sess_key.proto = key->proto;
    fdb->prev_proto_mark = pv.original;
    if (key->is_syn) {
       fdb->proto_mark = pv.syn_ack;
       fdb->init_proto_mark = pv.syn_ack;
    } else {
        fdb->proto_mark = 0;
        fdb->init_proto_mark = 0;
    }

//    fdb->identifies_proto = 0;
//    fdb->new_link = 1;
//    rte_atomic64_inc(&pv.all_link);
    fdb->lcore_id = rte_lcore_id();
    if (fdb->ssn_ptr1) {
        rte_free(fdb->ssn_ptr1);
        fdb->ssn_ptr1 = NULL;
    }
     if (fdb->ssn_ptr2) {
        rte_free(fdb->ssn_ptr2);
        fdb->ssn_ptr2 = NULL;
    } 

    memset(fdb->place_token_list,-1,sizeof(fdb->place_token_list));
    fdb->magic = 'X';
    //fdb->recvdevn = ifindex;
    //fdb->vid = skb->vid;
    fdb->state_cnt = 1;//for dpi
    fdb->pkt_cnt = 0;
    //rte_spinlock_init(&fdb->state_lock);
    //fdb->vars[0] = (void *)rte_zmalloc(NULL, sizeof(struct l2ct_private_struct), 0);
    
    //memset(&fdb->vars_dpi, 0, sizeof(struct l2ct_var_dpi));
     
    struct l2ct_private_struct *p = &fdb->vars_l2ct;
    //p->up_num = 0;
    //p->up_bytes = 0;
    //p->down_num = 0;
    //p->down_bytes = 0;
    p->begin_timestamp = time(NULL); //record the begin timestamp

    fdb->flag_timestamp = rte_rdtsc();
    //printf("ses_init:%p, time:%u\n", &fdb->vars_dpi, time(NULL));
    fdb->pcap_dump_node = NULL;
    unsigned lcore_id = rte_lcore_id();
    if(dump_pcaps.pcap_dump_node[lcore_id].dumper)
    {
        if(dump_pcaps.pcap_dump_node[lcore_id].dump_bytes < pcap_dump_file_size)
        {
            fdb->pcap_dump_node = &dump_pcaps.pcap_dump_node[lcore_id];
        }
    }
    
    fdb->qq = 0;
    fdb->logout_up_num = 0;
        
    SESS_DEBUG_LOG("%s(%d),fdb[%p],mark[%d],%u.%u.%u.%u:%d-->%u.%u.%u.%u:%d,recv_ifindex:%d,vid is %d\n",\
                __FUNCTION__,__LINE__,fdb,fdb->proto_mark,RTE_NIPQUAD(fdb->sess_key.ip_src),ntohs(fdb->sess_key.port_src),
                RTE_NIPQUAD(fdb->sess_key.ip_dst),ntohs(fdb->sess_key.port_dst),\
                fdb->recvdevn,fdb->vid);

}

#ifdef SESSION_CACHE_HASH

static uint32_t g_hash_size = 0;
static uint32_t g_hash_rnd = 0;

static uint32_t  session_hash_cache_hash_fn(void *key)
{
	struct key_5tuple *k = (struct key_5tuple *)key;                                                              

    uint32_t a = k->ip_src ^ k->ip_dst;
    uint32_t b = (k->ip_src + k->ip_dst) & 0xFFFFFFFF;
    uint32_t c = ((k->port_src + k->port_dst)& 0xFFFF) | (k->proto << 16) | (a << 24);
    return rte_jhash_3words(a, b, c, g_hash_rnd) & (g_hash_size - 1);
}

static int  session_hash_cache_comp_key_fn(struct h_scalar *he, void *key)
{
	struct sft_fdb_entry *dc = container_of(he, struct sft_fdb_entry, h_scalar);
	struct key_5tuple *k = (struct key_5tuple *)key;

    if(dc->sess_key.proto == k->proto && 
        ((dc->sess_key.ip_src == k->ip_src && dc->sess_key.ip_dst == k->ip_dst && 
          dc->sess_key.port_src == k->port_src && dc->sess_key.port_dst == k->port_dst)||
         (dc->sess_key.ip_src == k->ip_dst && dc->sess_key.ip_dst == k->ip_src &&
          dc->sess_key.port_src == k->port_dst && dc->sess_key.port_dst == k->port_src)))
    {
        SESS_DEBUG_LOG("SUCESS:%u.%u.%u.%u:%u-->%u.%u.%u.%u:%u|%u  VS  %u.%u.%u.%u:%u-->%u.%u.%u.%u:%u|%u\n", 
					RTE_NIPQUAD(dc->sess_key.ip_src), ntohs(dc->sess_key.port_src), RTE_NIPQUAD(dc->sess_key.ip_dst), ntohs(dc->sess_key.port_dst), dc->sess_key.proto, 
                    RTE_NIPQUAD(k->ip_src), ntohs(k->port_src), RTE_NIPQUAD(k->ip_dst), ntohs(k->port_dst), k->proto
                    );
        return 0;
    }
    
    SESS_DEBUG_LOG("FAILED:%u.%u.%u.%u:%u-->%u.%u.%u.%u:%u|%u  VS  %u.%u.%u.%u:%u-->%u.%u.%u.%u:%u|%u\n", 
					RTE_NIPQUAD(dc->sess_key.ip_src), ntohs(dc->sess_key.port_src), RTE_NIPQUAD(dc->sess_key.ip_dst), ntohs(dc->sess_key.port_dst), dc->sess_key.proto, 
                    RTE_NIPQUAD(k->ip_src), ntohs(k->port_src), RTE_NIPQUAD(k->ip_dst), ntohs(k->port_dst), k->proto
                    );
    return 1;
}

static void session_hash_cache_release_fn(struct h_scalar *he)
{
	struct sft_fdb_entry *dc = container_of(he, struct sft_fdb_entry, h_scalar);

    struct sess_hash *hash = get_session_hash_by_lcore(dc->lcore_id);
    hash->curr_entry_count--;
    SESS_DEBUG_LOG("%s(%d),fdb[%p],%u.%u.%u.%u:%d-->%u.%u.%u.%u:%d,lcore_id:%d\n",\
                __FUNCTION__,__LINE__,dc,RTE_NIPQUAD(dc->sess_key.ip_src),ntohs(dc->sess_key.port_src),
                RTE_NIPQUAD(dc->sess_key.ip_dst),ntohs(dc->sess_key.port_dst), dc->lcore_id);
    
    tcp_ofo_list_packet_free(&dc->vars_dpi);
    
	rte_mempool_mp_put(hash->mem_cache, dc);
}

static char *session_hash_cache_build_line_fn(struct h_scalar *he)
{
	struct sft_fdb_entry *dc = container_of(he, struct sft_fdb_entry, h_scalar);
	char *line;

	if((line = (char *)rte_malloc(NULL, 200, 0)) == NULL)
		return NULL;
	
	snprintf(line, 150, "sess:%u.%u.%u.%u:%u--%u.%u.%u.%u:%u|%u\n", 
					RTE_NIPQUAD(dc->sess_key.ip_src), dc->sess_key.port_src, RTE_NIPQUAD(dc->sess_key.ip_dst), dc->sess_key.port_dst, dc->sess_key.proto);
	return line;
}

static struct h_scalar *session_hash_cache_create_fn(struct h_table *ht, void *key, uint8_t *is_syn)
{
	struct key_5tuple *k = (struct key_5tuple *)key;
	struct sft_fdb_entry *dc;

    if(k->proto == IPPROTO_TCP && !k->is_syn)
    {
        return NULL;
    }
    struct sess_hash *hash = get_session_hash_by_lcore(rte_lcore_id());
    
	if(rte_mempool_mc_get(hash->mem_cache, (void **)&dc) <0)
		return NULL;

    hash->curr_entry_count++;
	sess_init(dc, k);

	return &dc->h_scalar;
}

struct sft_fdb_entry *session_hash_cache_try_get(struct sess_hash *hash, struct key_5tuple *k)
{
	struct h_scalar *he;
	struct sft_fdb_entry *dc;

	if((h_scalar_try_get(&hash->h_table, k, &he,session_hash_cache_create_fn, NULL, 0, 0)) == -1)
		return NULL;

	dc = container_of(he, struct sft_fdb_entry, h_scalar);

	return dc;
}

static struct h_scalar_operations session_hash_cache_ops =
{
	.hash        = session_hash_cache_hash_fn,
	.comp_key    = session_hash_cache_comp_key_fn,
	.release     = session_hash_cache_release_fn,
	.build_line  = session_hash_cache_build_line_fn,
};

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

struct sess_hash * hash_init_with_cache(void)
{
    uint64_t i = 0;
    uint64_t hash_size = get_hash_size() / rte_lcore_count();

    hash_size = roundup_pow_of_two(hash_size);    //Best hash sizes are of power of two
    
    if(!g_hash_size)
        g_hash_size = hash_size;
    
    RTE_LOG(INFO, L2CT, "core:%d, hash size:%lu\n", rte_lcore_id(), hash_size);
    
    struct sess_hash * hash = (struct sess_hash *)rte_zmalloc(NULL, sizeof(struct sess_hash), 0);
    if(!hash)
    {
        RTE_LOG(INFO, L2CT, "%s: malloc hash error\n", __FUNCTION__);
        return NULL;
    }

    char buf[128];
    sprintf(buf, "session_hash_mem_cache-%d", rte_lcore_id());
	/* create a mempool (with cache) */
	if ((hash->mem_cache = rte_mempool_create(buf, hash_size * 4,
		sizeof(struct sft_fdb_entry),
		64, 0,
		my_mp_init, NULL,
		my_obj_init, NULL,
		SOCKET_ID_ANY,  0)) == NULL)
	{
		rte_free(hash);
        RTE_LOG(ERR, L2CT, "%s: malloc hash buf error\n", __FUNCTION__);
        return NULL;
	}
	
	if(h_scalar_table_create(&hash->h_table, NULL, hash_size, hash_size * 4, SES_EXPIRED_TIME * HZ, &session_hash_cache_ops) != 0)
	{
		rte_free(hash);
        RTE_LOG(ERR, L2CT, "%s: malloc hash buf error\n", __FUNCTION__);
        return NULL;
	}

    srand(time(NULL));
    hash->sess_hash_rnd = random();
    hash->max_hash_size = hash_size;
    hash->max_entry_size = hash_size * 4;
    hash->curr_entry_count = 0;
    
    if(!g_hash_rnd)
        g_hash_rnd = hash->sess_hash_rnd;
    
    
    SESS_DEBUG_LOG("%s, hash init finish, hash size:%lu, sizeof(entry):%lu, entry size:%lu, used entry:%lu, lcore:%d\n", __FUNCTION__, 
                hash->max_hash_size, sizeof(struct sft_fdb_entry),
                hash->max_entry_size, hash->curr_entry_count, rte_lcore_id());

    return hash;
}

void hash_uninit_with_cache(struct sess_hash *hash)
{
    uint64_t i = 0;
    struct hash_entry *p = NULL;

    if(!hash)
    {
        return;
    }

    h_scalar_table_release(&hash->h_table);

    rte_free(hash);
}

#else

struct sess_hash * hash_init_with_vect(void)
{
    uint64_t i = 0;
    uint64_t hash_size = get_hash_size() / rte_lcore_count();

    hash_size = roundup_pow_of_two(hash_size);    //Best hash sizes are of power of two
    
    RTE_LOG(INFO, L2CT, "core:%d, hash size:%lu\n", rte_lcore_id(), hash_size);
    
    struct sess_hash * hash = (struct sess_hash *)rte_zmalloc(NULL, sizeof(struct sess_hash), 0);
    if(!hash)
    {
        RTE_LOG(INFO, L2CT, "%s: malloc hash error\n", __FUNCTION__);
        return NULL;
    }
    struct hash_entry * hash_buf = (struct hash_entry *)rte_zmalloc(NULL, sizeof(struct hash_entry )* hash_size, 0);
    if(!hash_buf)
    {
        rte_free(hash);
        RTE_LOG(ERR, L2CT, "%s: malloc hash buf error\n", __FUNCTION__);
        return NULL;
    }

    srand(time(NULL));
    hash->sess_hash_rnd = random();
    hash->max_hash_size = hash_size;
    hash->curr_entry_count = 0;
    hash->hash_buf = hash_buf;
    
    for(; i < hash->max_hash_size; i++)
    {
        hash_buf = &hash->hash_buf[i];
        hash_buf->ext = (struct sft_fdb_entry *)rte_zmalloc(NULL, sizeof(struct sft_fdb_entry) * STEP_EXT_SIZE, 0);
        if(hash_buf->ext)
        {
            hash_buf->ext_size = STEP_EXT_SIZE;
            hash->max_entry_size += STEP_EXT_SIZE;
        }
    }
    
    SESS_DEBUG_LOG("%s, hash init finish, hash size:%lu, sizeof(entry):%lu, entry size:%lu, used entry:%lu, lcore:%d\n", __FUNCTION__, 
                hash->max_hash_size, sizeof(struct sft_fdb_entry),
                hash->max_entry_size, hash->curr_entry_count, rte_lcore_id());

    return hash;
}

void hash_uninit_with_vect(struct sess_hash *hash)
{
    uint64_t i = 0;
    struct hash_entry *p = NULL;

    if(!hash)
    {
        return;
    }

    for(; i < hash->max_hash_size; i++)
    {
        p = &hash->hash_buf[i];
        rte_free(p->ext);
    }

    rte_free(hash->hash_buf);
    rte_free(hash);
}

inline int ipaddr_hash_func(uint32_t sip, uint32_t dip, 
                              uint16_t source, uint16_t dest,
                              uint8_t protocol, uint64_t hash_size, uint32_t hash_rnd)
{
    uint32_t a = sip ^ dip;
    uint32_t b = (sip + dip) & 0xFFFFFFFF;
    uint32_t c = ((source + dest)& 0xFFFF) | (protocol << 16) | (a << 24);
    return rte_jhash_3words(a, b, c, hash_rnd) & (hash_size - 1);
}

static void sess_zero(struct sft_fdb_entry *ses)
{
    if(!ses)
    {
        return;
    }

    //printf("sess_zero:%p, ofo:%d, size:%d, time:%u\n", &ses->vars_dpi, ses->vars_dpi.tcp_ofo_idx, ses->vars_dpi.tcp_ofo_size, time(NULL));
    tcp_ofo_list_packet_free(&ses->vars_dpi);
    
    memset(ses, 0, sizeof(struct sft_fdb_entry));
}

static inline int free_entry(struct sess_hash *hash, struct sft_fdb_entry *ses)
{
    uint64_t curr_time = 0;

    if(!ses->timestamp)
    {
        return 0;
    }

    curr_time = rte_rdtsc();
    if(curr_time - ses->timestamp > SES_EXPIRED_TIME * HZ) 
    {
        //ses->timestamp = 0;
        sess_zero(ses);
        hash->curr_entry_count--;
        return 1;
    }

    return 0;
}
static inline int match_entry(struct sft_fdb_entry *ses, struct key_5tuple *key)
{   
#if 1
    SESS_DEBUG_LOG("match_entry:[%u.%u.%u.%u:%u--%u.%u.%u.%u:%u|%u]vs[%u.%u.%u.%u:%u--%u.%u.%u.%u:%u|%u].\n", 
                    RTE_NIPQUAD(ses->sess_key.ip_src), ntohs(ses->sess_key.port_src), RTE_NIPQUAD(ses->sess_key.ip_dst), ntohs(ses->sess_key.port_dst), ses->sess_key.proto,
                    RTE_NIPQUAD(key->ip_src), ntohs(key->port_src), RTE_NIPQUAD(key->ip_dst), ntohs(key->port_dst), key->proto);
#endif

    if(ses->sess_key.proto ^ key->proto || 
        ((ses->sess_key.ip_src ^ key->ip_src || ses->sess_key.ip_dst ^ key->ip_dst || 
          ses->sess_key.port_src ^ key->port_src || ses->sess_key.port_dst ^ key->port_dst) &&
         (ses->sess_key.ip_src ^ key->ip_dst || ses->sess_key.ip_dst ^ key->ip_src ||
          ses->sess_key.port_src ^ key->port_dst || ses->sess_key.port_dst ^ key->port_src)))
    {
        return 0;
    }

    return 1;
}

static void check_session_entry(struct hash_entry *p)
{
    int i = 0, j = 0;
    struct sft_fdb_entry *ses;
    for (i = 0; i < p->ext_size; i++) {
        ses = &p->ext[i];
        if(ses->timestamp)
            j++;
    }
    
    if(j != p->used_size)
        assert(0);
}
static int is_used_entry(struct sess_hash *hash, struct sft_fdb_entry *ses, struct hash_entry *p)
{
    uint64_t curr;
    
    if(!ses->timestamp)
    {
        return 0;
    }

    curr = rte_rdtsc();
    if(curr - ses->timestamp > SES_EXPIRED_TIME * HZ)
    {
        //ses->timestamp = 0;
        sess_zero(ses);
        hash->curr_entry_count--;
        p->used_size--;
        return 0;
    }

    return 1;
}

struct sft_fdb_entry * hash_find(struct sess_hash *hash, struct key_5tuple *key, ipaddr_hash hash_5tuple)
{
    int hash_val = 0;
    struct hash_entry *p = NULL;
    int i = 0;

    if(!hash || !key || !hash_5tuple)
    {
        return NULL;
    }

    if(!hash->curr_entry_count)
    {
        return NULL;
    }

    hash_val = hash_5tuple(key->ip_src, key->ip_dst, key->port_src, key->port_dst, 
                           key->proto, hash->max_hash_size, hash->sess_hash_rnd);
    p = &hash->hash_buf[hash_val];
    
    for(; i < p->ext_size; i ++)
    {
        if(!is_used_entry(hash, &p->ext[i], p))
        {
            continue;
        }
        
        if(match_entry(&p->ext[i], key))
        {
            return &p->ext[i];
        }
    }

    return NULL;
}

static void sync_session_data(struct sft_fdb_entry *src, struct sft_fdb_entry *dst, uint32_t len)
{
    int i = 0;
    for(; i < len; i++)
    {
        memcpy(&dst[i], &src[i], sizeof(struct sft_fdb_entry));
        
        hlist_move_list(&src[i].vars_dpi.tcp_ofo_head, &dst[i].vars_dpi.tcp_ofo_head);
    }
}

struct sft_fdb_entry * hash_find_and_new(struct sess_hash *hash, struct key_5tuple *key, ipaddr_hash hash_5tuple)
{
    int hash_val = 0;
    struct hash_entry *p = NULL;
    struct sft_fdb_entry *free_p = NULL;
    int hash_index = -1, ext_index = -1;
    int i = 0;

    if(!hash || !key || !hash_5tuple)
    {
        RTE_LOG(ERR, L2CT, "%s: para invalid, hash=%p, key=%p, hash_5tuple=%p\n", __FUNCTION__, hash, key, hash_5tuple);
        return NULL;
    }
 
    hash_val = hash_5tuple(key->ip_src, key->ip_dst, key->port_src, key->port_dst, 
                           key->proto, hash->max_hash_size, hash->sess_hash_rnd);
    p = &hash->hash_buf[hash_val];

    hash_index = hash_val;

    int j = 0;
    for(; i < p->ext_size && j < p->used_size; i ++)
    {
        if(!is_used_entry(hash, &p->ext[i], p))
        {
            if(!free_p)
            {
                free_p = &p->ext[i];
            }
            continue;
        }
        
        if(match_entry(&p->ext[i], key))
        {
            return &p->ext[i];
        }
        
        j++;
    }

    if(key->proto == IPPROTO_TCP && !key->is_syn)
    {
        return NULL;
    }
    
    if(!free_p && i < p->ext_size)
    {
        free_p = &p->ext[i];
    }

    if(free_p)
    {
        sess_init(free_p, key);
        hash->curr_entry_count++;
        p->used_size++;
        
        return free_p;
    }

    if(!p->ext_size)
    {
        p->ext = (struct sft_fdb_entry *)rte_zmalloc(NULL, sizeof(struct sft_fdb_entry) * STEP_EXT_SIZE, 0);
        if(!p->ext)
        {
            RTE_LOG(ERR, L2CT, "%s: malloc sess entry error\n", __FUNCTION__);
            return NULL;
        }

        p->ext_size = STEP_EXT_SIZE;
        hash->max_entry_size += STEP_EXT_SIZE;
        sess_init(&p->ext[0], key);
        hash->curr_entry_count++;
        p->used_size++;

        SESS_DEBUG_LOG("hash index %d,hash ext malloc, size: %d\n", hash_val, p->ext_size);
        SESS_DEBUG_LOG("hash create entry: index hash:%d, ext:%d.\n", hash_index, 0);
        return &p->ext[0];
    }
    else
    {
        if(p->ext_size < MAX_EXT_SIZE)
        {
            //struct sft_fdb_entry * tmp = p->ext;
            //p->ext = (struct sft_fdb_entry *)rte_realloc(p->ext, (p->ext_size + STEP_EXT_SIZE) * sizeof(struct sft_fdb_entry), 0);
            struct sft_fdb_entry * tmp = (struct sft_fdb_entry *)rte_zmalloc(NULL, 
                                               sizeof(struct sft_fdb_entry) * (p->ext_size + STEP_EXT_SIZE), 0);
            if(!tmp)
            {
                RTE_LOG(ERR, L2CT, "%s: realloc sess entry error, hash size:%lu, entry size:%lu, current entry:%lu, lcore:%d\n", __FUNCTION__,
                            hash->max_hash_size, hash->max_entry_size, hash->curr_entry_count, rte_lcore_id());
                return NULL;
            }
            //memcpy(tmp, p->ext, p->ext_size * sizeof(struct sft_fdb_entry));
            sync_session_data(p->ext, tmp, p->ext_size);
            rte_free(p->ext);
            p->ext = tmp;
            
            //memset(&p->ext[p->ext_size], 0, STEP_EXT_SIZE * sizeof(struct sft_fdb_entry));
            p->ext_size += STEP_EXT_SIZE;
            hash->max_entry_size += STEP_EXT_SIZE;
            sess_init(&p->ext[p->ext_size - STEP_EXT_SIZE], key);
            hash->curr_entry_count++;
            p->used_size++;

            SESS_DEBUG_LOG("hash index:%d, hash ext realloc, size: %d\n", hash_val, p->ext_size);
            SESS_DEBUG_LOG("hash create entry: index hash:%d, ext:%d, size off:%lu, step off:%lu.\n", 
                            hash_index, p->ext_size - STEP_EXT_SIZE, p->ext_size * sizeof(struct sft_fdb_entry),
                            STEP_EXT_SIZE * sizeof(struct sft_fdb_entry));
            return &p->ext[p->ext_size - STEP_EXT_SIZE];
        } 
        else
        {
            error_log++;
            if(rte_rdtsc() - timestamp_log > HZ)
            {
                RTE_LOG(ERR, L2CT, "hash index:%d,hash ext size is too long[%d] , used entry:%lu, max entry:%lu, droped session:%lu.\n", hash_val, p->ext_size, hash->curr_entry_count, hash->max_entry_size, error_log);
                timestamp_log = rte_rdtsc();
            }
            
        }
    }

    return NULL;
}

#endif

int session_mgr_init(void)
{
    struct sess_hash *hash = NULL;
    
    HZ = rte_get_timer_hz();

#ifdef SESSION_CACHE_HASH
    hash = hash_init_with_cache();
#else
    hash = hash_init_with_vect();
#endif
    if(hash == NULL)
    {
        RTE_LOG(ERR, L2CT, "%s: session hash init error\n", __FUNCTION__);
        return -1;
    }
    
    if(sess_hash_init_by_lcore(rte_lcore_id(), hash) < 0)
    {
        RTE_LOG(ERR, L2CT, "%s: session hash init by lcore error\n", __FUNCTION__);
        return -1;
    }
 
//#define TC_NIC_CONF_PATH "/root/angus/dpdk/arbiter_dpdk/dpdk-2.0.0/examples/arbiter/build/conf/tc_nic.conf" 
    const char *path = "/opt/utc/conf/";
    if(get_conf_path())
    {
        path = get_conf_path();
    }
    
    init_ip_frag(&hash->globle);
    return 0;
}

void session_load_conf(void)
{
    const char *path = "/opt/utc/conf/";
    if(get_conf_path())
    {
        path = get_conf_path();
    }
    
    sess_config_init(path);
    tcp_ofo_mem_init();
}

//1:yes
static inline int is_inner_dev_by_port(int port)
{
    return isinner_by_portid(port);
}

static void update_l2ct_pkt_num(struct rte_mbuf* mbuf)
{
    struct l2ct_private_struct *p;
    struct sft_fdb_entry *fdb;
    int ethhdr_len;
    int len;
    if(mbuf == NULL || mbuf->l2ct == NULL)
        return;
    fdb = mbuf->l2ct;
    p = &fdb->vars_l2ct;
    
    ethhdr_len = sizeof(struct ether_hdr);
    len = (int)rte_pktmbuf_data_len(mbuf);
    if(is_inner_dev_by_port(mbuf->in_port))
    {//uplink
        p->up_bytes += len; 
        p->up_num++;
    }
    else
    {//downlink
        p->down_bytes += len; 
        p->down_num++;
    }
    p->curren_timestamp =time(NULL);
}

/*
 *1:go orig path
 *0:I consume this skb
 *-1: error
 * */
static int session_handle(struct sess_hash * hash, struct rte_mbuf* m)
{
    struct ether_addr *dest = NULL;
    struct ether_hdr *eth_hdr = NULL;
    struct ipv4_hdr *ipv4_hdr;
    struct key_5tuple key;
    struct tcp_hdr *tcp;
    int is_syn = 1;

    struct sft_fdb_entry *p = NULL;

    if(!hash || !m)
    {
        RTE_LOG(ERR, L2CT, "%s: para invalid, hash=%p, m=%p\n", __FUNCTION__, hash, m);
        return 1;
    }
    
    eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

    uint16_t ether_type = eth_hdr->ether_type;
    size_t vlan_offset = get_vlan_offset(eth_hdr, &ether_type);


//    uint16_t l3_len = (ip_hdr->version_ihl & 0x0f) * 4; 

    dest = &eth_hdr->d_addr;
    
    if(is_broadcast_ether_addr(dest))
        return 1;
    if(is_multicast_ether_addr(dest))
        return 1;
    if(ether_type != rte_cpu_to_be_16(ETHER_TYPE_IPv4))
        return 1;
    
    ipv4_hdr = (struct ipv4_hdr *)((char *)(eth_hdr + 1) + vlan_offset);
    if(!ipv4_hdr) 
        return 1;
    
    if(ipv4_is_multicast(ipv4_hdr->dst_addr) || 
        ipv4_is_lbcast(ipv4_hdr->dst_addr) ||
        ipv4_is_local_multicast(ipv4_hdr->dst_addr) || 
        ipv4_is_zeronet(ipv4_hdr->dst_addr))
        return 1;
      
    if(ipv4_hdr->src_addr == get_local_addr() || ipv4_hdr->dst_addr == get_local_addr())
    {//send to localhost or localhost send
        set_bit(1, &m->vars); 
        return -1;
    }

    key.ip_dst = ipv4_hdr->dst_addr;
    key.ip_src = ipv4_hdr->src_addr;
    key.proto = ipv4_hdr->next_proto_id;

    switch (ipv4_hdr->next_proto_id) 
    {
        case IPPROTO_TCP:
            break;

        case IPPROTO_UDP:
            break;

        default:
            return 1;
    }
    
    union header{
        struct tcp_hdr   _tcph;
        struct udp_hdr   _udph;
        unsigned char   buf[32];
    }hdr;
        
    int ip_frag_status = 0;
    if(rte_ipv4_frag_pkt_is_fragmented(ipv4_hdr))
    {
        ip_frag_status = ip_frag_handle(m, hdr.buf, hash);
        if(ip_frag_status == -1)
        {
            return -1;
        } 
        else if(ip_frag_status == 0)
        {
            return 0;
        }

        tcp = &hdr._tcph;
    }
    else
    {
        tcp = (struct tcp_hdr *)((unsigned char *)ipv4_hdr + (ipv4_hdr->version_ihl & 0x0f) * 4);
    }
    
    key.port_dst = tcp->dst_port;
    key.port_src = tcp->src_port;
    if(key.port_dst == 0 || key.port_src == 0)
    {
        return -1;
    }
    
    key.is_syn = 0;
    if(ipv4_hdr->next_proto_id == IPPROTO_TCP)
    {
        is_syn = (tcp->tcp_flags & TCP_SYN_FLAG) && !(tcp->tcp_flags & TCP_ACK_FLAG);
        key.is_syn = is_syn;
    }

#ifdef SESSION_CACHE_HASH    
    p = session_hash_cache_try_get(hash, &key);
#else
    p = hash_find_and_new(hash, &key, ipaddr_hash_func);
#endif    
    if(!p)
    {
        SESS_DEBUG_LOG("%s: session entry find failed, hash=%p, %u.%u.%u.%u:%u--%u.%u.%u.%u:%u[%u]\n", 
                __FUNCTION__, hash, RTE_NIPQUAD(key.ip_src), ntohs(key.port_src), 
                RTE_NIPQUAD(key.ip_dst), ntohs(key.port_dst), key.proto);
        return -1;
    }
 
    p->timestamp = rte_rdtsc();
    m->l2ct = p;
    update_l2ct_pkt_num(m);
    //m->sft_timestamp = p->timestamp;
    
    if(ipv4_hdr->next_proto_id == IPPROTO_TCP)
    {
        tcp_ofo_retrans_handle(ipv4_hdr, p, m);
    }

    SESS_DEBUG_LOG("%s: session = hash:%p, %u.%u.%u.%u:%u--%u.%u.%u.%u:%u[%u], port:%d, lcore:%d, pkgs:[%u-%u], bytes:[%u-%u]\n", 
                    __FUNCTION__, p, RTE_NIPQUAD(key.ip_src), ntohs(key.port_src), RTE_NIPQUAD(key.ip_dst), ntohs(key.port_dst), 
                    key.proto, m->in_port, rte_lcore_id(), p->vars_l2ct.up_num, p->vars_l2ct.down_num, 
                    p->vars_l2ct.up_bytes, p->vars_l2ct.down_bytes);
    
    return 1;
}
int update_session_to_shm(uint8_t core_id)
{
    struct sess_hash *hash = get_session_hash_by_lcore(core_id);
    struct hash_entry *p = NULL;
    struct sft_fdb_entry *fdb_entry;
    char session_path[64];
    uint64_t duration_time;
    if(!hash)
    {
        return 1;
    }
    FILE *fp;
    int j, i;
    static uint8_t web_flag;
    static  int hash_indix[16] = {0};
    int step = 0;

   
    if (hash_indix[core_id] == 0) {
        session_path[0] = '\0';
        web_flag = 0;
        sprintf(session_path, "/dev/shm/data_stream/session_%u",core_id);
        fp = fopen(session_path,"w+");
        if (!fp)
            return -1;

    }

    for (; hash_indix[core_id] < hash->max_hash_size && skb_stat->web_mesg_flag; hash_indix[core_id] ++) {
        p = &hash->hash_buf[hash_indix[core_id]];

        for (j = 0, i = 0; j < p->used_size && i < p->ext_size; i++) {
            fdb_entry = &p->ext[j];
            uint64_t curr = rte_rdtsc();
#if 1            
            if ( fdb_entry->timestamp && curr - fdb_entry->timestamp < SES_EXPIRED_TIME * HZ ) 
#else
            if (is_used_entry(hash, fdb_entry, p))     
#endif    
            {
                if (fdb_entry->vars_l2ct.curren_timestamp > fdb_entry->vars_l2ct.begin_timestamp) {
                    duration_time = (fdb_entry->vars_l2ct.curren_timestamp - fdb_entry->vars_l2ct.begin_timestamp);
                }else {
                    duration_time = 1;
                }

                   fprintf(fp, "%-15u,%-15u,%-15u,%-15u,%-15u,%-15u,%-15u,%-15u,%-15u\n",
                            fdb_entry->sess_key.ip_src, fdb_entry->sess_key.port_src,fdb_entry->sess_key.ip_dst,
                            fdb_entry->sess_key.port_dst, fdb_entry->sess_key.proto, fdb_entry->proto_mark & 0xffff0000,
                            fdb_entry->vars_l2ct.up_bytes,fdb_entry->vars_l2ct.down_bytes,duration_time
                            );
                            
                j++;

            }

        }

      //  if (++step >= hash->max_hash_size/8 && hash_indix[core_id] < hash->max_hash_size)
      //      return 0;
    }

    //hash_indix = 0;
        hash_indix[core_id] = 0;
        web_flag |= (1 << core_id);
        
        if (web_flag ^ pv.need_web_flag == 0) {
            skb_stat->web_mesg_flag = 0;
        }
        fclose(fp); 
        return 1;
}

int send_session_to_remote(uint8_t core_id, 
    int (*process)(uint8_t core_id, void *data))
{
    struct sess_hash *hash = get_session_hash_by_lcore(core_id);
    struct hash_entry *p = NULL;
    struct sft_fdb_entry *fdb_entry;

    if(!hash)
    {
        return 1;
    }
  
    int j;
    static  int hash_indix[16] = {0};
    int step = 0;
   // int count = 0;
    struct mesg_st msg;

    for (; hash_indix[core_id] < hash->max_hash_size && skb_stat->web_mesg_flag; hash_indix[core_id] ++) {
        p = &hash->hash_buf[hash_indix[core_id]];
        for (j = 0; j < p->used_size; j++) {
            fdb_entry = &p->ext[j];
            if (fdb_entry->timestamp && fdb_entry->proto_mark) {
                msg.ip_src = fdb_entry->sess_key.ip_src;
                msg.port_src = fdb_entry->sess_key.port_src;
                msg.ip_dst = fdb_entry->sess_key.ip_dst;
                msg.port_dst = fdb_entry->sess_key.port_dst;
                msg.proto = fdb_entry->sess_key.proto;
                msg.proto_mark = fdb_entry->proto_mark & 0xffff0000;
                msg.up_bytes = fdb_entry->vars_l2ct.up_bytes;
                msg.down_bytes = fdb_entry->vars_l2ct.down_bytes;
                if (fdb_entry->vars_l2ct.curren_timestamp > fdb_entry->vars_l2ct.begin_timestamp) {
                    msg.duration_time = (fdb_entry->vars_l2ct.curren_timestamp - fdb_entry->vars_l2ct.begin_timestamp);
                }else {
                    msg.duration_time = 1;
                }
                process(core_id, (void *)&msg);
            }
        }

        if (++step >= hash->max_hash_size/4 && hash_indix[core_id] < hash->max_hash_size)
            return 0;
    }

    //hash_indix = 0;
    if ((core_id + 1)>= rte_lcore_count() || skb_stat->web_mesg_flag == 0) {
        hash_indix[core_id] = 0;
        skb_stat->web_mesg_flag = 0; 
        return 1;

    } else {
        skb_stat->web_mesg_flag = core_id + 2;
        hash_indix[core_id] = 0;
    }
    return 0;
}


/*
 *1:go orig path
 *0:I consume this skb
 * */
int session_fwd_handle(struct rte_mbuf* mbuf, unsigned portid)
{
    uint8_t out = (uint8_t)-1;
    struct sess_hash * hash = NULL;

    if(mbuf)
    {
        //init packet self vars
        mbuf->mr_tab_idx = 0;
        mbuf->l2ct = NULL;
        //mbuf->sft_timestamp = 0;
        mbuf->in_port = portid;
    }

    hash = get_session_hash_by_lcore(rte_lcore_id());
    
    if(!hash)
    {
        return 1;
    }
    
    if(!test_flags_l2ct_enable())
    {
        return 1;
    }
    
    set_bit(0, &mbuf->vars); //mark skb from bridge
    
    int ret = session_handle(hash, mbuf);
    if(ret < 0)
    {
        goto direct_go_L2;
    }
    else if(ret == 0)
    {
        return 0;
    }
    
    return 1;
    
direct_go_L2:
    out = get_dport_by_sport(mbuf->in_port);
    if(out == (uint8_t)-1)
    {
        rte_pktmbuf_free(mbuf);
    }
    else
    {
        send_single_packet(mbuf, out);
    } 

    return 0;
}

void session_mgr_uninit(void)
{
    struct sess_hash *hash = get_session_hash_by_lcore(rte_lcore_id());
    if(!hash)
    {
        return;
    }

#ifdef SESSION_CACHE_HASH
    hash_uninit_with_cache(hash);
#else
    hash_uninit_with_vect(hash);
#endif

    sess_hash_uninit_by_lcore();
}
