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


#include "debug.h"
#include "dpi_dynamic.h"
#include "domain_tbl.h"
#include "dynamic_tbl.h"
#include "h_cache.h"
#include "utils.h"
#include "stat.h"

#define DYNAMIC_RELOAD_TEST	


static uint32_t g_hash_initval;
struct dynamic_cache_table
{
	struct h_table h_table;
	struct rte_mempool *mem_cache[32];
//    unsigned long expire;
} dynamic_cache_tbl;
struct h_bucket dynamic_cache_base[DYNAMIC_HASH_SIZE];

struct dynamic_cache_key
{
    uint32_t ip;
    uint16_t port;
    unsigned int proto_mark;
};

static uint32_t dynamic_cache_hash_fn(void *key)
{
	struct dynamic_cache_key *k = (struct dynamic_cache_key *)key;                                                              
    
    return rte_jhash_2words(k->ip, k->port, g_hash_initval);	
}

static int dynamic_cache_comp_key_fn(struct h_scalar *he, void *key)
{
	struct dynamic_cache *dc = container_of(he, struct dynamic_cache, h_scalar);
	struct dynamic_cache_key *k = (struct dynamic_cache_key *)key;

    return (dc->ip ^ k->ip || dc->port ^ k->port);	
}

static void dynamic_cache_release_fn(struct h_scalar *he)
{
	struct dynamic_cache *dc = container_of(he, struct dynamic_cache, h_scalar);

    skb_stat->dynamic_hash_count -- ;
    rte_mempool_mp_put(dynamic_cache_tbl.mem_cache[dc->lcore_id], dc);
}

static char *dynamic_cache_build_line_fn(struct h_scalar *he)
{
	struct dynamic_cache *dc = container_of(he, struct dynamic_cache, h_scalar);
	char *line, *dp;
	
	if((line = (char *)rte_malloc(NULL, 200, 0)) == NULL)
		return NULL;
    char *s;	
    snprintf(line, 150, "dynamic:%s:%u prot:%u\n", ipv4_nltos(dc->ip, s), ntohs(dc->port), dc->proto_mark);
	return line;
}

static struct h_scalar_operations dynamic_cache_ops =
{
	.hash        = dynamic_cache_hash_fn,
	.comp_key    = dynamic_cache_comp_key_fn,
	.release     = dynamic_cache_release_fn,
	.build_line  = dynamic_cache_build_line_fn,
};


static struct h_scalar *dynamic_cache_create_fn(struct h_table *ht, void *key)
{
	struct dynamic_cache_key *k = (struct dynamic_cache_key *)key;
	struct dynamic_cache *dc;
	
    uint32_t lcore_id = rte_lcore_id();	
    if(rte_mempool_mc_get(dynamic_cache_tbl.mem_cache[lcore_id], (void **)&dc) <0)
		return NULL;
    dc->ip = k->ip;
    dc->port = k->port;
    dc->proto_mark = k->proto_mark;
    dc->lcore_id = lcore_id;
//	rte_spinlock_init(&dc->lock);
    skb_stat->dynamic_hash_count ++ ;
	return &dc->h_scalar;
}

struct dynamic_cache *dynamic_cache_try_get(const __uint32_t ip, const __u16 port, unsigned int proto_mark, uint8_t is_static)
{
	struct h_scalar *he;
	struct dynamic_cache *dc;
	struct dynamic_cache_key k;

    k.ip = ip;
    k.port = port;
    k.proto_mark = proto_mark;

	if((h_scalar_try_get(&dynamic_cache_tbl.h_table, &k, &he,dynamic_cache_create_fn, NULL, is_static, 0)) == -1)
		return NULL;
	
	dc = container_of(he, struct dynamic_cache, h_scalar);

	return dc;
}

static void __dynamic_cache_lookup_action(struct h_scalar *he, void *key, void *result)
{
    struct dynamic_cache *dc = container_of(he, struct dynamic_cache, h_scalar);

    *(unsigned int *) result = dc->proto_mark;
}
unsigned int dynamic_lookup_behavior(const __uint32_t ip, const __u16 port)
{
    unsigned int result;
    struct dynamic_cache_key k;

    /* Loopup precise table. */
    k.ip = ip;
    k.port = port;

    if(h_scalar_try_lookup(&dynamic_cache_tbl.h_table, &k, &result, __dynamic_cache_lookup_action) == 0)
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

void dynamic_cache_clear()
{

    D("dynamic_cache_clear\n");
    h_scalar_table_clear(&dynamic_cache_tbl.h_table);
}

void dynamic_cache_static_clear()
{

    D("dynamic_cache_static_clear\n");
    h_scalar_table_static_clear(&dynamic_cache_tbl.h_table);
}

int dynamic_cache_init(void)
{
	int ret, retv = -1;
	
    srand(time(NULL));
    g_hash_initval = random();
    printf("---------------------study_cache_init------------------------------\n");	

    uint32_t  nb_lcores = rte_lcore_count();
    int i;
    char buf[80];

    for (i = 0; i < nb_lcores; i++) {

        bzero(buf, 80);
        sprintf(buf, "dynamic_mem_cache_%d", i);


        /* create a mempool (with cache) */
        if ((dynamic_cache_tbl.mem_cache[i] = rte_mempool_create(buf, DYNAMIC_MAX_LEN,
                        sizeof(struct dynamic_cache),
                        32, 0,
                        my_mp_init, NULL,
                        my_obj_init, NULL,
                        SOCKET_ID_ANY, 0 /*MEMPOOL_F_NO_CACHE_ALIGN | MEMPOOL_F_SP_PUT | MEMPOOL_F_SC_GET*/)) == NULL)
        {
            retv = -ENOMEM;
            goto err_kmem_create;
        }
    }
    if(h_scalar_table_create(&dynamic_cache_tbl.h_table, NULL, DYNAMIC_HASH_SIZE, DYNAMIC_MAX_LEN, 1200, &dynamic_cache_ops) != 0){
		retv = ret;
		goto err_dynamic_create;
    }
    skb_stat->dynamic_hash_count = 0;	
	return 0;

	
err_dynamic_create:
err_kmem_create:
    E("dynamic_create fail\n");
//	remove_proc_entry("dynamic", proc_dynamic);
	return retv;
}

void dynamic_cache_exit(void)
{
	h_scalar_table_release(&dynamic_cache_tbl.h_table);
}
