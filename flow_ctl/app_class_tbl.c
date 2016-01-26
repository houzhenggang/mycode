
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


#include "app_class_tbl.h"
#include "utils.h"
#include "sft_statictics.h"
#include "com.h"
#include "debug.h"

#define APP_CLASS_HASH_SIZE    (1 << 10)
#define APP_CLASS_MAX_LEN      (APP_CLASS_HASH_SIZE * 4)
 
static uint32_t g_hash_initval;
struct app_class_cache_table
{
	struct h_table h_table;
	struct rte_mempool *mem_cache;
} app_class_cache_tbl;


static uint32_t app_class_cache_hash_fn(void *key)
{
	struct app_class_key *k = (struct app_class_key *)key;                                                              
    return rte_jhash_1word(k->type_id, g_hash_initval);
}

static int app_class_cache_comp_key_fn(struct h_scalar *he, void *key)
{
	app_class_t *ac = container_of(he, app_class_t, h_scalar);
	struct app_class_key *k = (struct app_class_key *)key;
    return (ac->type_id ^ k->type_id);	
}

static void app_class_cache_release_fn(struct h_scalar *he)
{
	app_class_t *ac = container_of(he, app_class_t, h_scalar);
    rte_mempool_sp_put(app_class_cache_tbl.mem_cache, ac);
}

static char *app_class_cache_build_line_fn(struct h_scalar *he)
{
	app_class_t *ac = container_of(he, app_class_t, h_scalar);
	char *line, *dp;
	
	if((line = (char *)malloc(200)) == NULL)
		return NULL;
    char *s;	
    snprintf(line, 150, "app_class[%s] type id[%u]\n", ac->type_name, ac->type_id);
	return line;
}

static struct h_scalar_operations app_class_cache_ops =
{
	.hash        = app_class_cache_hash_fn,
	.comp_key    = app_class_cache_comp_key_fn,
	.release     = app_class_cache_release_fn,
	.build_line  = app_class_cache_build_line_fn,

};


static struct h_scalar *app_class_cache_create_fn(struct h_table *ht, void *key)
{
	struct app_class_key *k = (struct app_class_key *)key;
	app_class_t *ac;
	
    if(rte_mempool_sc_get(app_class_cache_tbl.mem_cache, (void **)&ac) <0)
		return NULL;
    ac->type_id = k->type_id;
	return &ac->h_scalar;
}

app_class_t *app_class_cache_try_get(struct app_class_key *key, int *ret_status)
{
	struct h_scalar *he;
	app_class_t *ac;
    
    *ret_status = h_scalar_try_get(&app_class_cache_tbl.h_table, key, &he, app_class_cache_create_fn, NULL, 0, 0);
	return ( app_class_t *)container_of(he, app_class_t, h_scalar);
}

static void __app_class_cache_lookup_action(struct h_scalar *he, void *key, void *result)
{
    app_class_t *ac = container_of(he, app_class_t, h_scalar);
    *(uint64_t *)result =  ac;
}



app_class_t *app_class_lookup_by_key(const struct app_class_key *key)
{
    app_class_t *ac;
    if(h_scalar_try_lookup(&app_class_cache_tbl.h_table, (void *)key, &ac, __app_class_cache_lookup_action) == 0)
        return ac;
    return NULL;
}

void *__look_app_class_node(struct h_scalar *he, void *priv)
{
    char *name = (char *)priv;
    app_class_t *node = container_of(he, app_class_t, h_scalar);

    size_t cmp_len = (strlen(node->type_name) > strlen(name) ? strlen(node->type_name): strlen(name));

    if (!strncmp_nocase(node->type_name, name, cmp_len))
        return (void *)node;
    return NULL;
//    rcu_read_unlock();
}


/* not found return -1, else return the place id */
int get_app_class_id_by_name(const char * name)
{
    app_class_t *node;
    if ((node =(app_class_t *) h_scalar_lookup_by_user(&app_class_cache_tbl.h_table, (void *)name, __look_app_class_node)))
        return node->type_id;
    return APP_CLASS_ID_NOT_DEFINED;
}

app_class_t *get_app_class_by_name(const char * name)
{
    return h_scalar_lookup_by_user(&app_class_cache_tbl.h_table, (void *)name, __look_app_class_node);
}

void app_class_clear()
{
    h_scalar_table_clear(&app_class_cache_tbl.h_table);
}
static void __init_app_stat(struct h_scalar *he)
{
    app_class_t *app_class = container_of(he, app_class_t, h_scalar);
    if (app_class) {
        if(app_class->type_name[0])
        {
            uint32_t big_type = (app_class->type_id & BIG_TYPE_MASK) >> 28;
            uint32_t mid_type = (app_class->type_id & MID_TYPE_MASK) >> 24;
            uint32_t sub_type = (app_class->type_id & SUB_TYPE_MASK) >> 16;
            strncpy(DPI_Statistics[big_type][mid_type][sub_type].name, app_class->type_name,strlen(app_class->type_name));
        }
    }

}
int init_app_stat()
{

    h_scalar_table_iterate_safe(&app_class_cache_tbl.h_table,  __init_app_stat);
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

int app_class_cache_init(void)
{
	int ret, retv = -1;
	
    srand(time(NULL));
    g_hash_initval = random();

    /* create a mempool (with cache) */
    if ((app_class_cache_tbl.mem_cache = rte_mempool_create("app_class_mem_cache", APP_CLASS_MAX_LEN,
                sizeof(app_class_t),
                32, 0,
                my_mp_init, NULL,
                my_obj_init, NULL,
                SOCKET_ID_ANY,  MEMPOOL_F_NO_CACHE_ALIGN | MEMPOOL_F_SP_PUT | MEMPOOL_F_SC_GET)) == NULL)
    	{
		retv = -ENOMEM;
		goto err_kmem_create;
	}

    if(h_scalar_table_create(&app_class_cache_tbl.h_table, NULL, APP_CLASS_HASH_SIZE, APP_CLASS_MAX_LEN, 0, &app_class_cache_ops) != 0){
		retv = ret;
		goto err_app_class_create;
    }
	
	return 0;

	
err_app_class_create:
//	kmem_cache_destroy(app_class_cache_tbl.mem_cache);
err_kmem_create:
//	remove_proc_entry("pattern", proc_dynamic);
	return retv;
}

void app_class_cache_exit(void)
{
	
	h_scalar_table_release(&app_class_cache_tbl.h_table);
	//kmem_cache_destroy(app_class_cache_tbl.mem_cache);
}
