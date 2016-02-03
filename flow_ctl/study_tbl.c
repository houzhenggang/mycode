
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


#include "study_tbl.h"
#include "utils.h"
#include "debug.h"
#include "com.h"
#include "stat.h"

static uint32_t g_hash_initval;
#define STUDY_RELOAD_TEST	
struct study_cache_table
{
	struct h_table h_table;
	struct rte_mempool *mem_cache;
} study_cache_tbl;
struct h_bucket study_cache_base[STUDY_HASH_SIZE];

struct study_cache_key
{
    __u32 ip;
    __u16 port;
    unsigned int proto_mark;
};

static __u32 study_cache_hash_fn(void *key)
{
	struct study_cache_key *k = (struct study_cache_key *)key;                                                              
    
    return rte_jhash_2words(k->ip, k->port, g_hash_initval);	
}

static int study_cache_comp_key_fn(struct h_scalar *he, void *key)
{
	struct study_cache *sc = container_of(he, struct study_cache, h_scalar);
	struct study_cache_key *k = (struct study_cache_key *)key;

    return (sc->ip ^ k->ip || sc->port ^ k->port);	
}

static void study_cache_release_fn(struct h_scalar *he)
{
	struct study_cache *sc = container_of(he, struct study_cache, h_scalar);
    //LOG("free study cache[%p]\n", sc);
//    printf("[%p]end time:%u\n",sc, time(NULL));
    skb_stat->study_hash_count--;
    rte_mempool_mp_put(study_cache_tbl.mem_cache, sc);
}

static char *study_cache_build_line_fn(struct h_scalar *he)
{
	struct study_cache *sc = container_of(he, struct study_cache, h_scalar);
	char *line, *dp;
	
	if((line = (char *)malloc(200)) == NULL)
		return NULL;
    char *s;	
    snprintf(line, 150, "study:%s:%u prot:%u\n", ipv4_nltos(sc->ip, s), ntohs(sc->port), sc->proto_mark);
	return line;
}

static struct h_scalar_operations study_cache_ops =
{
	.hash        = study_cache_hash_fn,
	.comp_key    = study_cache_comp_key_fn,
	.release     = study_cache_release_fn,
	.build_line  = study_cache_build_line_fn,
};


static struct h_scalar *study_cache_create_fn(struct h_table *ht, void *key)
{
	struct study_cache_key *k = (struct study_cache_key *)key;
	struct study_cache *sc;

    uint32_t lcore_id = rte_lcore_id();	
    if(rte_mempool_mc_get(study_cache_tbl.mem_cache, (void **)&sc) <0)
		return NULL;
    sc->ip = k->ip;
    sc->port = k->port;
    sc->proto_mark = k->proto_mark;
    sc->lcore_id = lcore_id;
    skb_stat->study_hash_count ++;
//    printf("[%p]start time:%u\n",sc, time(NULL));
//	rte_spinlock_init(&sc->lock);
	return &sc->h_scalar;
}

//struct study_cache *study_cache_try_get(const __u32 ip, const __u16 port, unsigned int proto_mark)
int study_cache_try_get(const __u32 ip, const __u16 port, unsigned int proto_mark, uint8_t is_static, uint64_t timeo)
{
	struct h_scalar *he;
	struct study_cache *sc;
	struct study_cache_key k;
    k.ip = ip;
    k.port = port;
    k.proto_mark = proto_mark;
    return h_scalar_try_get(&study_cache_tbl.h_table, &k, &he, study_cache_create_fn, NULL, is_static, timeo);
#if 0
	if((he = h_scalar_try_get(&study_cache_tbl.h_table, &k, study_cache_create_fn, NULL)) == NULL)
		return NULL;
	
	sc = container_of(he, struct study_cache, h_scalar);

	return sc;
#endif
}

static void __study_cache_lookup_action(struct h_scalar *he, void *key, void *result)
{
    struct study_cache *sc = container_of(he, struct study_cache, h_scalar);
    *(unsigned int *) result = sc->proto_mark;
}
unsigned int study_lookup_behavior(const __u32 ip, const __u16 port)
{
    unsigned int result;
    struct study_cache_key k;

    /* Loopup precise table. */
    k.ip = ip;
    k.port = port;

    if(h_scalar_try_lookup(&study_cache_tbl.h_table, (void *)&k, &result, __study_cache_lookup_action) == 0)
        return result;
    return 0;
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

void study_cache_clear()
{

    D("study_cache_clear\n");
    h_scalar_table_clear(&study_cache_tbl.h_table);
}

void study_cache_static_clear()
{

    D("study_cache_static_clear\n");
    h_scalar_table_static_clear(&study_cache_tbl.h_table);
}

int study_cache_init(void)
{
	int ret, retv = -1;
	
    srand(time(NULL));
    g_hash_initval = random();
    printf("---------------------study_cache_init------------------------------\n");	

    uint32_t  nb_lcores = rte_lcore_count();
    int i;
    char buf[80];
//    for (i = 0; i < nb_lcores; i++) 
    {

//        bzero(buf, 80);
//        sprintf(buf, "study_mem_cache_%d", i);

        /* create a mempool (with cache) */
        if ((study_cache_tbl.mem_cache = rte_mempool_create("study_mem_cache", STUDY_MAX_LEN,
                        sizeof(struct study_cache),
                        32, 0,
                        my_mp_init, NULL,
                        my_obj_init, NULL,
                        SOCKET_ID_ANY,  0/*MEMPOOL_F_NO_CACHE_ALIGN | MEMPOOL_F_SP_PUT | MEMPOOL_F_SC_GET*/)) == NULL)
        {
            retv = -ENOMEM;
            goto err_kmem_create;
        }
    }
    if(h_scalar_table_create(&study_cache_tbl.h_table, NULL, STUDY_HASH_SIZE, STUDY_MAX_LEN, STUDY_TIMEOUT, &study_cache_ops) != 0){
		retv = ret;
		goto err_study_create;
    }
	
    skb_stat->study_hash_count = 0;
	return 0;

	
err_study_create:
//	kmem_cache_destroy(study_cache_tbl.mem_cache);
err_kmem_create:
    E("study_mem_cache fail\n");
//	remove_proc_entry("study", proc_dynamic);
	return retv;
}

void study_cache_exit(void)
{
	
	h_scalar_table_release(&study_cache_tbl.h_table);
	//kmem_cache_destroy(study_cache_tbl.mem_cache);
}
