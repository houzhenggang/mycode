
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <rte_cycles.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_spinlock.h>
#include <rte_malloc.h>
#include <rte_jhash.h>

#include <urcu.h>
#include <urcu/rculist.h>


#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>

#include "ftp.h"
#include "utils.h"
#include "init.h"
#include "debug.h"
#include "session_mgr.h"
#include "h_cache.h"

static uint32_t g_hash_initval;
#define FTP_RELOAD_TEST	
struct ftp_cache_table
{
	struct h_table h_table;
	struct rte_mempool *mem_cache[32];
} ftp_cache_tbl;

struct ftp_cache
{
    struct h_scalar h_scalar;
    /* Last check time (time stamp) */
//    struct timespec last_check;
    unsigned int proto_mark;
    __u32       sip;
    __u32       dip;
    __u16       dport;
    uint8_t lcore_id;
    rte_spinlock_t lock;
};

struct ftp_cache_key
{
    __u32 sip;
    __u32 dip;
    __u16 dport;
    unsigned int proto_mark;
};

static __u32 ftp_cache_hash_fn(void *key)
{
	struct ftp_cache_key *k = (struct ftp_cache_key *)key;                                                              
    
    return rte_jhash_3words(k->sip, k->dip, k->dport, g_hash_initval);	
}

static int ftp_cache_comp_key_fn(struct h_scalar *he, void *key)
{
	struct ftp_cache *fc = container_of(he, struct ftp_cache, h_scalar);
	struct ftp_cache_key *k = (struct ftp_cache_key *)key;

    return (fc->sip ^ k->sip || fc->dip ^ k->dip || fc->dport ^ k->dport);	
}

static void ftp_cache_release_fn(struct h_scalar *he)
{
	struct ftp_cache *fc = container_of(he, struct ftp_cache, h_scalar);
    rte_mempool_mp_put(ftp_cache_tbl.mem_cache[fc->lcore_id], fc);
}

static char *ftp_cache_build_line_fn(struct h_scalar *he)
{
	struct ftp_cache *fc = container_of(he, struct ftp_cache, h_scalar);
	char *line, *dp;
	
	if((line = (char *)malloc(200)) == NULL)
		return NULL;
    char *s;	
    snprintf(line, 150, "study:%s:%u prot:%u\n", ipv4_nltos(fc->dip, s), ntohs(fc->dport), fc->proto_mark);
	return line;
}

static struct h_scalar_operations ftp_cache_ops =
{
	.hash        = ftp_cache_hash_fn,
	.comp_key    = ftp_cache_comp_key_fn,
	.release     = ftp_cache_release_fn,
	.build_line  = ftp_cache_build_line_fn,
};


static struct h_scalar *ftp_cache_create_fn(struct h_table *ht, void *key)
{
	struct ftp_cache_key *k = (struct ftp_cache_key *)key;
	struct ftp_cache *fc;
	
    uint32_t lcore_id = rte_lcore_id();	
    if(rte_mempool_mc_get(ftp_cache_tbl.mem_cache[lcore_id], (void **)&fc) <0)
		return NULL;
    fc->sip = k->sip;
    fc->dip = k->dip;
    fc->dport = k->dport;
    fc->proto_mark = k->proto_mark;
    fc->lcore_id = lcore_id;
	rte_spinlock_init(&fc->lock);
	return &fc->h_scalar;
}

//struct ftp_cache *ftp_cache_try_get(const __u32 ip, const __u16 port, unsigned int proto_mark)
int ftp_cache_try_get(const __u32 sip, const __u32 dip, const __u16 dport, unsigned int proto_mark)
{
	struct h_scalar *he;
	struct ftp_cache *fc;
	struct ftp_cache_key k;

    k.sip = sip;
    k.dip = dip;
    k.dport = dport;
    k.proto_mark = proto_mark;
    return h_scalar_try_get(&ftp_cache_tbl.h_table, &k, &he, ftp_cache_create_fn, NULL, 0, 0);
#if 0
	if((he = h_scalar_try_get(&ftp_cache_tbl.h_table, &k, ftp_cache_create_fn, NULL)) == NULL)
		return NULL;
	
	fc = container_of(he, struct ftp_cache, h_scalar);

	return fc;
#endif
}

static void __ftp_cache_lookup_action(struct h_scalar *he, void *key, void *result)
{
    struct ftp_cache *fc = container_of(he, struct ftp_cache, h_scalar);

    *(unsigned int *) result = fc->proto_mark;
}
unsigned int ftp_lookup_behavior(const __u32 sip, const __u32 dip, const __u16 dport)
{
    unsigned int result;
    struct ftp_cache_key k;

    /* Loopup precise table. */
    k.sip = sip;
    k.dip = dip;
    k.dport = dport;

    if(h_scalar_try_lookup(&ftp_cache_tbl.h_table, (void *)&k, &result, __ftp_cache_lookup_action) == 0)
        return result;
    return 0;
}
//int ftp_num = 0;
void detect_ftp_proto(struct ssn_skb_values *ssv)
{
//    if(NULL == ssv)
//        return;

//    if(NULL == ssv->ssn)
//        return;
    
    // ftp_num++;
    //  printf("in detect_ftp_proto [%d] ssv->payload_len = %d\n", ftp_num, ssv->payload_len);
    uint32_t buf[6];
    uint16_t dport;
    uint32_t dip;
    unsigned char *cmdptr;
    

    if (ssv->ssn->proto_mark != pv.ftp_proto_appid)
        return;

    if (ssv->isinner) //request
    {
        ssv->ssn->keepalive = 1;
        if(ssv->payload_len > 29 || ssv->payload_len < 21)
        {
            return;
        }

        //Active Mode 
        if(ssv->payload[0] == 'P' && ssv->payload[1] == 'O' && ssv->payload[2] == 'R' && ssv->payload[3] == 'T')
        {
            cmdptr = ssv->payload + 4;
            if((*cmdptr) == ' ')
            {
                sscanf(cmdptr, " %u,%u,%u,%u,%u,%u",buf, buf + 1, buf + 2, buf + 3, buf + 4, buf + 5);

                buf[0] &= 0xff;
                buf[1] &= 0xff;
                buf[2] &= 0xff;
                buf[3] &= 0xff;
                buf[4] &= 0xff;
                buf[5] &= 0xff;
                dip = ((buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0]);
                dport = ((buf[5] << 8) | buf[4]);

                // D("ftp %u.%u.%u.%u->%u.%u.%u.%u:%u.\n", IPQUADS(ssv->dip), IPQUADS(dip), ntohs(dport));
                ftp_cache_try_get(ssv->dip, dip, dport, ssv->ssn->proto_mark);
            }
        }
    } 
    else //response
    {
        ssv->ssn->keepalive = 1;
        if(ssv->payload_len > 54 || ssv->payload_len < 42)
        {
            return;
        }

        //Passive Mode
        if(ssv->payload[0] == '2' && ssv->payload[1] == '2')
        {
            if(ssv->payload[2] == '7')
            {
                cmdptr = ssv->payload + 17;
                cmdptr = strchr(cmdptr , '(');
                if(cmdptr)
                {
                    sscanf(cmdptr, "(%u,%u,%u,%u,%u,%u)",buf, buf + 1, buf + 2, buf + 3, buf + 4, buf + 5);

                    buf[0] &= 0xff;
                    buf[1] &= 0xff;
                    buf[2] &= 0xff;
                    buf[3] &= 0xff;
                    buf[4] &= 0xff;
                    buf[5] &= 0xff;
                    dip = ((buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0]);
                    dport = ((buf[5] << 8) | buf[4]);

                    ftp_cache_try_get(ssv->dip, dip, dport, ssv->ssn->proto_mark);
                    //D("ftp Passive %u.%u.%u.%u->%u.%u.%u.%u:%u.\n", IPQUADS(ssv->dip), IPQUADS(dip), ntohs(dport));
                }
             } 
            //Extended Passive Mode
            else if (ssv->payload[2] == '9') 
            {//扩展被动模式
                cmdptr = ssv->payload + 26;
                cmdptr = strchr(cmdptr, '(');
                if(cmdptr)
                {
                    sscanf(cmdptr, "(|||%u|)",&dport);
                    ftp_cache_try_get(ssv->dip, ssv->sip, htons(dport), ssv->ssn->proto_mark);
                    // D("ftp Extended Passive %u.%u.%u.%u->%u.%u.%u.%u:%u.\n", IPQUADS(ssv->dip), IPQUADS(ssv->dip),dport);
                }
            }	
        }    
    }   
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

int ftp_cache_init(void)
{
	int ret, retv = -1;
	
    srand(time(NULL));
    g_hash_initval = random();
    printf("---------------------ftp_cache_init------------------------------\n");	

    uint32_t  nb_lcores = rte_lcore_count();
    int i;
    char buf[80];

    for (i = 0; i < nb_lcores; i++) {

        bzero(buf, 80);
        sprintf(buf, "ftp_mem_cache_%d", i);


        /* create a mempool (with cache) */
        if ((ftp_cache_tbl.mem_cache[i] = rte_mempool_create(buf, FTP_MAX_LEN,
                        sizeof(struct ftp_cache),
                        32, 0,
                        my_mp_init, NULL,
                        my_obj_init, NULL,
                        SOCKET_ID_ANY, 0 /*MEMPOOL_F_NO_CACHE_ALIGN | MEMPOOL_F_SP_PUT | MEMPOOL_F_SC_GET*/)) == NULL)
        {
            retv = -ENOMEM;
            goto err_kmem_create;
        }
    }
    if(h_scalar_table_create(&ftp_cache_tbl.h_table, NULL, FTP_HASH_SIZE, FTP_MAX_LEN, 120, &ftp_cache_ops) != 0){
		retv = -ENOMEM;
		goto err_ftp_create;
    }
	
	return 0;

	
err_ftp_create:
//	kmem_cache_destroy(ftp_cache_tbl.mem_cache);
err_kmem_create:
    E("ftp_create fail\n");
//	remove_proc_entry("study", proc_dynamic);
	return retv;
}

void ftp_cache_exit(void)
{
	
	h_scalar_table_release(&ftp_cache_tbl.h_table);
}
