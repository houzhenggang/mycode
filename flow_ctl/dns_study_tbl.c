
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
#include "dns_study_tbl.h"
#include "utils.h"
#include "stat.h"
#include "com.h"

static uint32_t g_hash_initval;
#define DNS_STUDY_RELOAD_TEST	
struct dns_study_cache_table
{
	struct h_table h_table;
	struct rte_mempool *mem_cache[32];
} dns_study_cache_tbl;

struct dns_study_cache_key
{
    __u32 ip;
    __u16 port;
    unsigned int proto_mark;
};

static __u32 dns_study_cache_hash_fn(void *key)
{
	struct dns_study_cache_key *k = (struct dns_study_cache_key *)key;                                                              
    
    return rte_jhash_2words(k->ip, k->port, g_hash_initval);	
}

static int dns_study_cache_comp_key_fn(struct h_scalar *he, void *key)
{
	struct dns_study_cache *sc = container_of(he, struct dns_study_cache, h_scalar);
	struct dns_study_cache_key *k = (struct dns_study_cache_key *)key;

    return (sc->ip ^ k->ip || sc->port ^ k->port);	
}

static void dns_study_cache_release_fn(struct h_scalar *he)
{
	struct dns_study_cache *sc = container_of(he, struct dns_study_cache, h_scalar);
    skb_stat->dns_hash_count--;
    rte_mempool_mp_put(dns_study_cache_tbl.mem_cache[sc->lcore_id], sc);
}

static char *dns_study_cache_build_line_fn(struct h_scalar *he)
{
	struct dns_study_cache *sc = container_of(he, struct dns_study_cache, h_scalar);
	char *line, *dp;
	
	if((line = (char *)malloc(200)) == NULL)
		return NULL;
    char *s;	
    snprintf(line, 150, "study:%s:%u prot:%u\n", ipv4_nltos(sc->ip, s), ntohs(sc->port), sc->proto_mark);
	return line;
}

static struct h_scalar_operations dns_study_cache_ops =
{
	.hash        = dns_study_cache_hash_fn,
	.comp_key    = dns_study_cache_comp_key_fn,
	.release     = dns_study_cache_release_fn,
	.build_line  = dns_study_cache_build_line_fn,
};


static struct h_scalar *dns_study_cache_create_fn(struct h_table *ht, void *key)
{
	struct dns_study_cache_key *k = (struct dns_study_cache_key *)key;
	struct dns_study_cache *sc;
	
    uint32_t lcore_id = rte_lcore_id();	
    if(rte_mempool_mc_get(dns_study_cache_tbl.mem_cache[lcore_id], (void **)&sc) <0)
		return NULL;
    sc->ip = k->ip;
    sc->port = k->port;
    sc->proto_mark = k->proto_mark;
    sc->lcore_id = lcore_id;
     
    skb_stat->dns_hash_count++;
//	rte_spinlock_init(&sc->lock);
	return &sc->h_scalar;
}

//struct dns_study_cache *dns_study_cache_try_get(const __u32 ip, const __u16 port, unsigned int proto_mark)
int dns_study_cache_try_get(const __u32 ip, const __u16 port, unsigned int proto_mark)
{
	struct h_scalar *he;
	struct dns_study_cache *sc;
	struct dns_study_cache_key k;

    k.ip = ip;
    k.port = port;
    k.proto_mark = proto_mark;
    return h_scalar_try_get(&dns_study_cache_tbl.h_table, &k, &he, dns_study_cache_create_fn, NULL, 0, 0);
#if 0
	if((he = h_scalar_try_get(&dns_study_cache_tbl.h_table, &k, dns_study_cache_create_fn, NULL)) == NULL)
		return NULL;
	
	sc = container_of(he, struct dns_study_cache, h_scalar);

	return sc;
#endif
}

static void __dns_study_cache_lookup_action(struct h_scalar *he, void *key, void *result)
{
    struct dns_study_cache *sc = container_of(he, struct dns_study_cache, h_scalar);

    *(unsigned int *) result = sc->proto_mark;
}
unsigned int dns_study_lookup_behavior(const __u32 ip, const __u16 port)
{
    unsigned int result;
    struct dns_study_cache_key k;

    /* Loopup precise table. */
    k.ip = ip;
    k.port = port;

    if(h_scalar_try_lookup(&dns_study_cache_tbl.h_table, (void *)&k, &result, __dns_study_cache_lookup_action) == 0)
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

int dns_study_cache_init(void)
{
	int ret, retv = -1;
	
    srand(time(NULL));
    g_hash_initval = random();
    printf("---------------------dns_study_cache_init------------------------------\n");	

    uint32_t  nb_lcores = rte_lcore_count();
    int i;
    char buf[80];

    for (i = 0; i < nb_lcores; i++) {

        bzero(buf, 80);
        sprintf(buf, "dns_study_mem_cache_%d", i);

        /* create a mempool (with cache) */
        if ((dns_study_cache_tbl.mem_cache[i] = rte_mempool_create(buf, DNS_STUDY_MAX_LEN,
                        sizeof(struct dns_study_cache),
                        32, 0,
                        my_mp_init, NULL,
                        my_obj_init, NULL,
                        SOCKET_ID_ANY,  0/*MEMPOOL_F_NO_CACHE_ALIGN | MEMPOOL_F_SP_PUT | MEMPOOL_F_SC_GET*/)) == NULL)
        {
            retv = -ENOMEM;
            goto err_kmem_create;
        }
    }
    if(h_scalar_table_create(&dns_study_cache_tbl.h_table, NULL, DNS_STUDY_HASH_SIZE, DNS_STUDY_MAX_LEN, DNS_STUDY_TIMEOUT, &dns_study_cache_ops) != 0){
		retv = ret;
		goto err_dns_study_create;
    }
	
    skb_stat->dns_hash_count = 0;
	return 0;

	
err_dns_study_create:
//	kmem_cache_destroy(dns_study_cache_tbl.mem_cache);
err_kmem_create:
    E("dns_study_create fail\n");
//	remove_proc_entry("study", proc_dynamic);
	return retv;
}

void dns_study_cache_exit(void)
{
	
	h_scalar_table_release(&dns_study_cache_tbl.h_table);
	//kmem_cache_destroy(dns_study_cache_tbl.mem_cache);
}
