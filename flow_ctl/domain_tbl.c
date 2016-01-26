#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <rte_prefetch.h>
#include <rte_cycles.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_spinlock.h>
#include <rte_malloc.h>
#include <rte_jhash.h>

#include "debug.h"
#include "dpi_dynamic.h"
#include "domain_tbl.h"
#include "h_cache.h"

#define DOMAIN_RELOAD_TEST

static uint32_t g_hash_initval;	
struct domain_cache_table
{
	struct h_table h_table;
	struct rte_mempool *mem_cache[32];
} domain_cache_tbl;
struct h_bucket domain_cache_base[DOMAIN_HASH_SIZE];

struct domain_cache_key
{
    char     url[150];
    unsigned int proto_mark;

//	size_t     url_len; 
};

static uint32_t domain_cache_hash_fn(void *key)
{
	struct domain_cache_key *k = (struct domain_cache_key *)key;                                                              
    return rte_jhash(k->url, strlen(k->url), g_hash_initval);	
}

static int domain_cache_comp_key_fn(struct  h_scalar *he, void *key)
{
	struct domain_cache *dc = container_of(he, struct domain_cache,  h_scalar);
	struct domain_cache_key *k = (struct domain_cache_key *)key;
    printf("dc[%s]key[%s]\n", dc->url,k->url);
    return strcasecmp(dc->url, k->url);	
}

static void domain_cache_release_fn(struct  h_scalar *he)
{
	struct domain_cache *dc = container_of(he, struct domain_cache,  h_scalar);
    printf("free domain_cache_tbl.mem_cache\n");
    rte_mempool_sp_put(domain_cache_tbl.mem_cache[dc->lcore_id], dc);
}

static char *domain_cache_build_line_fn(struct  h_scalar *he)
{
	struct domain_cache *dc = container_of(he, struct domain_cache,  h_scalar);
	char s1[20], s2[20];
	char *line, *dp;
	
	if((line = (char *)rte_zmalloc(NULL, 200, 0)) == NULL)
		return NULL;
	
    snprintf(line, 150, "domain:%s\n", dc->url);
	return line;
}

static struct  h_scalar_operations domain_cache_ops =
{
	.hash        = domain_cache_hash_fn,
	.comp_key    = domain_cache_comp_key_fn,
	.release     = domain_cache_release_fn,
	.build_line  = domain_cache_build_line_fn,
};


static struct  h_scalar *domain_cache_create_fn(struct h_table *ht, void *key)
{
	struct domain_cache_key *k = (struct domain_cache_key *)key;
	struct domain_cache *dc;

   uint32_t lcore_id = rte_lcore_id();	
   if(rte_mempool_sc_get(domain_cache_tbl.mem_cache[lcore_id], (void **)&dc) <0)
        return NULL;	
	strncpy(dc->url, k->url, 150);
    dc->proto_mark = k->proto_mark;
    dc->lcore_id = lcore_id;
	rte_spinlock_init(&dc->lock);
	return &dc-> h_scalar;
}
struct domain_cache *domain_cache_try_get(const char *url, unsigned int proto_mark)
{
	struct  h_scalar *he;
	struct domain_cache *dc;
	struct domain_cache_key k;

    if (unlikely(strlen(url) > 150))
        return NULL;
    strncpy(k.url, url,  150);
    k.proto_mark = proto_mark;
 
    printf("111proto[%u]\n", proto_mark);
	if(unlikely(h_scalar_try_get(&domain_cache_tbl.h_table, &k, &he, domain_cache_create_fn, NULL, 0, 0) != 0))
		return NULL;
	dc = container_of(he, struct domain_cache,  h_scalar);

	return dc;
}

static void __domain_cache_lookup_action(struct  h_scalar *he, void *key, void *result)
{
    struct domain_cache *dc = container_of(he, struct domain_cache, h_scalar);
    *(unsigned int *) result = dc->proto_mark;
}
unsigned int domain_lookup_behavior(const char *hostname)
{
    unsigned int result;
    struct domain_cache_key k;

    /* Loopup precise table. */
    strncpy(k.url, hostname, 150);
    if(h_scalar_try_lookup(&domain_cache_tbl.h_table, &k, &result, __domain_cache_lookup_action) == 0)
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



int domain_cache_init(void)
{
	int ret, retv = -1;

    printf("domain_cache_init\n");	
    srand(time(NULL));
    g_hash_initval = random();
    printf("---------------------study_cache_init------------------------------\n");	



#if 0	
	if((proc_dynamic_domain = proc_mkdir("domain", proc_dynamic)) == NULL)
	{
		printk(KERN_ERR "handle_ip: creating proc_fs directory failed.\n");
		return -1;
	}
	
#endif	

    uint32_t  nb_lcores = rte_lcore_count();
    int i;
    char buf[80];

    for (i = 0; i < nb_lcores; i++) {

        bzero(buf, 80);
        sprintf(buf, "domain_mem_cache_%d", i);
        /* create a mempool (with cache) */
        if ((domain_cache_tbl.mem_cache[i] = rte_mempool_create(buf, DOMAIN_MAX_LEN,
                        sizeof(struct domain_cache),
                        32, 0,
                        my_mp_init, NULL,
                        my_obj_init, NULL,
                        SOCKET_ID_ANY,  MEMPOOL_F_NO_CACHE_ALIGN | MEMPOOL_F_SP_PUT | MEMPOOL_F_SC_GET)) == NULL)
        {
            retv = -ENOMEM;
            goto err_kmem_create;
        }
    }
    if(h_scalar_table_create(&domain_cache_tbl.h_table, NULL, DOMAIN_HASH_SIZE, DOMAIN_MAX_LEN, 1200, &domain_cache_ops) != 0){
		retv = ret;
		goto err_domain_create;
    }
	
	return 0;

	
err_domain_create:
err_kmem_create:
    E("domain_create fail\n");
	return retv;
}

void domain_cache_exit(void)
{
	
    synchronize_rcu();
#ifdef DOMAIN_RELOAD_TEST	
#endif
	h_scalar_table_release(&domain_cache_tbl.h_table);
	//kmem_cache_destroy(domain_cache_tbl.mem_cache);
//	remove_proc_entry("domain", proc_dynamic);
}
