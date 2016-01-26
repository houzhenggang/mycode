
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


#include "session_tbl.h"
#include "utils.h"
#include "com.h"

static uint32_t g_hash_initval;
#define SESSION_RELOAD_TEST	
struct session_cache_table
{
	struct h_table h_table;
	struct rte_mempool *mem_cache[32];
} session_cache_tbl;
struct h_bucket session_cache_base[SESSION_HASH_SIZE];

struct session_cache_key
{
    uint32_t    sip;
    uint16_t    sport;
    uint32_t    dip;
    uint16_t    dport;
    uint16_t    protocol; 
};


static __u32 session_cache_hash_fn(void *key)
{
	struct session_cache_key *k = (struct session_cache_key *)key;                                                              
   
    uint32_t a = k->sip ^ k->dip;
    uint32_t b = (k->sip + k->dip) & 0xFFFFFFFF;
    uint32_t c = ((k->sport + k->dport)& 0xFFFF) | (k->protocol << 16) | (a << 24);
    return rte_jhash_3words(a, b, c, g_hash_initval) & (g_hash_initval - 1); 
}

static int session_cache_comp_key_fn(struct h_scalar *he, void *key)
{
	struct session_cache *sc = container_of(he, struct session_cache, h_scalar);
	struct session_cache_key *k = (struct session_cache_key *)key;

    return (sc->sip ^ k->sip || sc->dip ^ k->dip|| sc->sport ^ k->sport ||sc->dport ^ k->dport|| sc->protocol ^ k->protocol);	
}

static void session_cache_release_fn(struct h_scalar *he)
{
	struct session_cache *sc = container_of(he, struct session_cache, h_scalar);
//    printf("free session:%u.%u.%u.%u:%u->%u.%u.%u.%u:%u, proto %u\n", IPQUADS(sc->sip), ntohs(sc->sport), IPQUADS(sc->dip), ntohs(sc->dport), sc->protocol);
    rte_mempool_mp_put(session_cache_tbl.mem_cache[sc->lcore_id], sc);
}


static char *session_cache_build_line_fn(struct h_scalar *he)
{
	struct session_cache *sc = container_of(he, struct session_cache, h_scalar);
	char *line, *dp;
	
	if((line = (char *)malloc(200)) == NULL)
		return NULL;
    char *s;	
    snprintf(line, 150, "session:%u.%u.%u.%u:%u->%u.%u.%u.%u:%u, proto %u\n", IPQUADS(sc->sip), ntohs(sc->sport), IPQUADS(sc->dip), ntohs(sc->dport), sc->protocol);
	return line;
}

static struct h_scalar_operations session_cache_ops =
{
	.hash        = session_cache_hash_fn,
	.comp_key    = session_cache_comp_key_fn,
	.release     = session_cache_release_fn,
	.build_line  = session_cache_build_line_fn,
};

static struct h_scalar *session_cache_create_fn(struct h_table *ht, void *key)
{
	struct session_cache_key *k = (struct session_cache_key *)key;
	struct session_cache *sc;
    uint32_t lcore_id = rte_lcore_id();	

    if(rte_mempool_mc_get(session_cache_tbl.mem_cache[lcore_id], (void **)&sc) <0)
		return NULL;
    sc->sip = k->sip;
    sc->sport = k->sport;
    sc->dip = k->dip;
    sc->dport = k->dport;
    sc->protocol = k->protocol;
    sc->lcore_id = lcore_id;
	rte_spinlock_init(&sc->lock);
	return &sc->h_scalar;
}
static void session_cache_modify_fn(struct h_scalar *he, void *key)
{
    struct session_cache *sc = container_of(he, struct session_cache, h_scalar);
    sc->pack_num ++;
}

//struct session_cache *session_cache_try_get(const __u32 ip, const __u16 port, unsigned int proto_mark)
struct session_cache *session_cache_try_get(const __u32 sip, const __u16 sport, const __u32 dip, const __u16 dport, uint16_t protocol, int *retv)
{
	struct h_scalar *he;
	struct session_cache *sc;
	struct session_cache_key k;

    k.sip = sip;
    k.sport = sport;
    k.dip = dip;
    k.dport = dport;
    k.protocol = protocol;
    *retv = h_scalar_try_get(&session_cache_tbl.h_table, &k, &he, session_cache_create_fn, session_cache_modify_fn, 0, 0);

    if (*retv == -1)
            return NULL;
	    return (struct session_cache *)container_of(he, struct session_cache, h_scalar);
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

int session_cache_init(void)
{
	int ret, retv = -1;
	
    srand(time(NULL));
    g_hash_initval = random();
    printf("---------------------session_cache_init------------------------------\n");	
    uint32_t  nb_lcores = rte_lcore_count();
    int i;
    char buf[80];
    for (i = 0; i < nb_lcores; i++) {

        bzero(buf, 80);
        sprintf(buf, "session_mem_cache_%d", i);

        /* create a mempool (with cache) */
        if ((session_cache_tbl.mem_cache[i] = rte_mempool_create(buf, SESSION_MAX_LEN,
                        sizeof(struct session_cache),
                        32, 0,
                        my_mp_init, NULL,
                        my_obj_init, NULL,
                        SOCKET_ID_ANY, 0/* MEMPOOL_F_NO_CACHE_ALIGN | MEMPOOL_F_SP_PUT | MEMPOOL_F_SC_GET*/)) == NULL)
        {
            retv = -ENOMEM;
            goto err_kmem_create;
        }
    }
    if(h_scalar_table_create(&session_cache_tbl.h_table, session_cache_base, SESSION_HASH_SIZE, SESSION_MAX_LEN, 3, &session_cache_ops) != 0){
		retv = ret;
		goto err_session_create;
    }
	
	return 0;

	
err_session_create:
//	kmem_cache_destroy(session_cache_tbl.mem_cache);
err_kmem_create:
//	remove_proc_entry("session", proc_dynamic);
	return retv;
}

void session_cache_exit(void)
{
	
	//h_table_release(&session_cache_tbl.h_table);
	//kmem_cache_destroy(session_cache_tbl.mem_cache);
}


