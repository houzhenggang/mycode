#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_timer.h>
#include <rte_random.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_spinlock.h>
#include <rte_rwlock.h>
#include "h_cache.h"
#include "init.h"
#include "debug.h"


//#include "list.h"
#include "stat.h"
#include "com.h"

static inline size_t __h_table_len_inc(struct h_table *ht)
{
    size_t len;
    len = ++ht->len;
    return len;
}

static inline size_t h_table_len_inc(struct h_table *ht)
{
    size_t len;

    rte_rwlock_write_lock(&ht->rwlock);
    len = __h_table_len_inc(ht);
    rte_rwlock_write_unlock(&ht->rwlock);
    return len;
}
static inline size_t __h_table_len_dec(struct h_table *ht)
{
    size_t len;
    len = --ht->len;
    return len;
}


static inline size_t h_table_len_dec(struct h_table *ht)
{
    size_t len;

    rte_rwlock_write_lock(&ht->rwlock);
    len = __h_table_len_dec(ht);
    rte_rwlock_write_unlock(&ht->rwlock);

    return len;
}


static void h_scalar_free_rcu(struct rcu_head *rcu)
{
    struct h_scalar *he = container_of(rcu, struct h_scalar, rcu);
    struct h_table *ht = he->table;

    skb_stat->max_hash_level++;
    ht->s_ops->release(he);
    h_table_len_dec(ht);
}


void scalar_timer_func(unsigned long data)
{
	uint64_t hz = rte_get_timer_hz();
	unsigned lcore_id = rte_lcore_id();
	uint64_t cur_time = rte_get_timer_cycles();
	struct h_scalar *he = (struct h_scalar *) data;
	struct h_bucket *b = he->bucket;
        rte_spinlock_lock(&b->lock);
        cds_list_del_rcu(&he->list);
        rte_spinlock_unlock(&b->lock);
        call_rcu(&he->rcu, h_scalar_free_rcu);
}


int h_scalar_try_get(struct h_table *ht, void *key, struct h_scalar **ret_he,
		struct h_scalar *(*create)(struct h_table *, void *),
		void (*modify)(struct h_scalar *, void *) , uint8_t is_static, uint64_t timeo)

{
	struct h_bucket *b = &ht->base[ht->s_ops->hash(key) & (ht->size - 1)];
	struct h_scalar *he = NULL;
//	struct h_scalar *p = NULL;

	rcu_read_lock();
//    cds_list_for_each_entry_safe(he, p, &b->chain, list) {
    cds_list_for_each_entry_rcu(he, &b->chain, list) {
        if(ht->s_ops->comp_key(he, key) == 0)
        {
            /* Invoke the callback to do modifications. */
            if(modify)
                modify(he, key);
            he->timestamp = rte_rdtsc();
            *ret_he = he;
            rcu_read_unlock();
            return 1;
        }
    }
    rcu_read_unlock();
	/* Not found, try to create a new entry. */
	if(create == NULL)
	{
        *ret_he = NULL;
	//	rte_spinlock_unlock(&b->lock);
		return -1;
	}
	if(unlikely((he = create(ht, key)) == NULL))
	{
        *ret_he = NULL;
		//rte_spinlock_unlock(&b->lock);
		return -1;
	}
    *ret_he = he;
	/* Intialize the base class. */
	he->bucket = b;
    he->deleted = 0;
	he->table = ht;
    he->is_static = is_static;
    he->timestamp = rte_rdtsc();
    if (is_static == 0) {
        he->timeo = (timeo > 0 ? timeo : he->table->timeo);
    } else {
        he->timeo = 0;
    }
//    rte_atomic16_init(&he->ref);
//    CDS_INIT_LIST_HEAD(&he->list);
//	rcu_read_lock();
	rte_spinlock_lock(&b->lock);
	cds_list_add_rcu(&he->list, &b->chain);
	rte_spinlock_unlock(&b->lock);
	//spin_unlock(&test_lock);
    if (he->timeo > 0) {
        init_timer(&he->timer);
        setup_timer(&he->timer, scalar_timer_func, (unsigned long)he);
        mod_timer(&he->timer, jiffies + he->timeo * HZ_1S);
    }
    h_table_len_inc(ht);
	return 0;
}



int h_scalar_try_lookup(struct h_table *ht, void *key, void *result,
		void (*action)(struct h_scalar *, void *, void *) )
{
	struct h_bucket *b = &ht->base[ht->s_ops->hash(key) & (ht->size - 1)];
	struct h_scalar *he, *p;
	rcu_read_lock();
    //cds_list_for_each_entry_safe(he, p, &b->chain, list) {
    cds_list_for_each_entry_rcu(he, &b->chain, list) {
		if(ht->s_ops->comp_key(he, key) == 0)
		{
			/* Invoke the callback to do modifications or return values. */
			if(action) {
				action(he, key, result);
           	}
            he->timestamp = rte_rdtsc();
			rcu_read_unlock();
			return 0;
		} 
	}
	rcu_read_unlock();
	
	/* Not found, return error code. */
	return -1;
}

int h_scalar_try_free_key(struct h_table *ht, void *key)
{
	struct h_bucket *b = &ht->base[ht->s_ops->hash(key) & (ht->size - 1)];
	struct h_scalar *he;
	
	rte_spinlock_lock(&b->lock);
	cds_list_for_each_entry(he, &b->chain, list)
	{
		if(ht->s_ops->comp_key(he, key) == 0)
		{
			cds_list_del_rcu(&he->list);
			rte_spinlock_unlock(&b->lock);
			call_rcu(&he->rcu, h_scalar_free_rcu);
			return 0;
		}
	}
	rte_spinlock_unlock(&b->lock);
	
	return -1;
}

int h_scalar_table_iterate_safe(struct h_table *ht, void (*operate)(struct h_scalar *))
{
	int i, count = 0;
	struct h_bucket *b;
	struct h_scalar *he, *he_next;
	
	for(i = 0; i < ht->size; i++)
	{
		b = &ht->base[i];
		rte_spinlock_lock(&b->lock);
		cds_list_for_each_entry_safe(he, he_next, &b->chain, list)
		{
			if(operate)
				operate(he);
			count++;
		}
		rte_spinlock_unlock(&b->lock);
	}/* for(i = 0; i < ht->size; i++) */
	
	return count;
}
void *h_scalar_lookup_by_user(struct h_table *ht, void *priv, void *(*operate)(struct h_scalar *, void *))
{
    int i;
    void *p;
	struct h_bucket *b;
	struct h_scalar *he, *he_next;
	
    if (unlikely(!operate))
        return NULL;
	for(i = 0; i < ht->size; i++)
	{
		b = &ht->base[i];
		rte_spinlock_lock(&b->lock);
		cds_list_for_each_entry_safe(he, he_next, &b->chain, list)
		{
				if ((p = operate(he, priv))) {
		            rte_spinlock_unlock(&b->lock);
                    return p;
                }
		}
		rte_spinlock_unlock(&b->lock);
	}/* for(i = 0; i < ht->size; i++) */
	
	return NULL;
}



void h_scalar_table_clear(struct h_table *ht)
{
	int i, count = 0;
	struct h_bucket *b;
	struct h_scalar *he, *he_next;
	for(i = 0; i < ht->size; i++)
	{
		b = &ht->base[i];
		rte_spinlock_lock(&b->lock);
		cds_list_for_each_entry_safe(he, he_next, &b->chain, list)
		{
			cds_list_del_rcu(&he->list);
			synchronize_rcu();
			call_rcu(&he->rcu, h_scalar_free_rcu);
			count++;
		}
		rte_spinlock_unlock(&b->lock);
	}/* for(i = 0; i < ht->size; i++) */

}

void h_scalar_table_static_clear(struct h_table *ht)
{
	int i, count = 0;
	struct h_bucket *b;
	struct h_scalar *he, *he_next;
	for(i = 0; i < ht->size; i++)
	{
		b = &ht->base[i];
		rte_spinlock_lock(&b->lock);
		cds_list_for_each_entry_safe(he, he_next, &b->chain, list)
		{
			if(0 == he->is_static)
			{
				continue;
			}
			
			cds_list_del_rcu(&he->list);
			synchronize_rcu();
			call_rcu(&he->rcu, h_scalar_free_rcu);
			count++;
		}
		rte_spinlock_unlock(&b->lock);
	}/* for(i = 0; i < ht->size; i++) */

}

void h_scalar_table_release(struct h_table *ht)
{

	if(ht->base)
	{
		int i, count = 0;
		struct h_bucket *b;
		struct h_scalar *he, *he_next;
		
		for(i = 0; i < ht->size; i++)
		{
			b = &ht->base[i];
			rte_spinlock_lock(&b->lock);
			cds_list_for_each_entry_safe(he, he_next, &b->chain, list)
			{
				cds_list_del_rcu(&he->list);
                synchronize_rcu();
				ht->s_ops->release(he);
				h_table_len_dec(ht);
				count++;
			}
			rte_spinlock_unlock(&b->lock);
		}/* for(i = 0; i < ht->size; i++) */
		
		if(!ht->static_base)
			rte_free(ht->base);
		ht->base = NULL;
	}
	ht->size = 0;
}



/*
 * h_table_create: Initialize a new hash table.
 * Parameters: 
 *  @ht: `h_table` structure, already allocated or static,
 *  @base: optional argument, used for large sized hash table,
 *  @size: hash size,
 *  @max_len: maximum entries,
 *  @timeo: idle timeout secs.,
 *  @ops: operation collections for hash table,
 *  @proc_name: `proc_fs` interface filename,
 *  @proc_parent: `proc_fs` parent directory.
 * return value:
*   0 for success, <0 for error codes, use `errno` standards.
 */
int __h_table_create(struct h_table *ht, enum __h_table_type table_type,
        struct h_bucket *base, size_t size, size_t max_len, uint64_t timeo,
        struct h_operations *ops)
{

    struct h_bucket *b;
    size_t __size;
    uint32_t i;
    memset(ht, 0x0, sizeof(*ht));

	rte_rwlock_init(&ht->rwlock);
    for(__size = size - 1, i = 0; __size; __size >>= 1, i++);
    __size = (1UL << i);
    if(size != __size)
    {
        printf("%s() Warning: size '%lu' is accepted as '%lu'.\n",
            __FUNCTION__, (unsigned long)size, (unsigned long)__size);
        size = __size;
    }

    if(base)
    {
        ht->base = base;
        ht->static_base = true;
    }
    else
    {
        if((ht->base = (struct h_bucket *) rte_zmalloc(NULL, sizeof(struct h_bucket) * size, 64)) == NULL)
            return -ENOMEM;
        ht->static_base = false;
    }

    ht->size  = size;
    ht->max_len   = max_len;
    ht->timeo = timeo;
    ht->len   = 0;


    switch(table_type)
    {
    case H_TABLE_TYPE_CACHE:
        ht->ops   = ops;
        for(i = 0; i < size; i++)
        {
            b = &ht->base[i];
            CDS_INIT_LIST_HEAD(&b->chain);
        }


        ht->table_type = table_type;
        /* Setup idle queue timeout checking timer. */
        if(ht->timeo > 0)
        {
            static int ht_index = 0;
            int n;
            n = ht_index++;

        }

        break;
    case H_TABLE_TYPE_SCALAR:

        ht->s_ops   = (struct h_scalar_operations *)ops;
        for(i = 0; i < size; i++)
        {
            b = &ht->base[i];
            CDS_INIT_LIST_HEAD(&b->chain);
            rte_spinlock_init(&b->lock);
        }

        ht->table_type = table_type;

        break;
    default:
        printf("%s() Erron:Invalid table type %d.\n",
            __FUNCTION__, table_type);
        return -EINVAL;
    }


	return 0;
}

int h_table_iterate_safe(struct h_table *ht, void (*operate)(struct h_cache *) ,int max_duration, int min_per_sec)
{
	int i, count = 0;
	struct h_bucket *b;
	struct h_cache *he, *he_next;
	
	for(i = 0; i < ht->size; i++)
	{
		b = &ht->base[i];
		cds_list_for_each_entry_safe(he, he_next, &b->chain, list)
		{
			if(operate)
				operate(he);
			count++;
		}
	}/* for(i = 0; i < ht->size; i++) */
	
	return count;
}

static void h_cache_timer_func(struct rte_timer *tim, void *data)
{
    struct h_table *ht;
	uint64_t hz = rte_get_timer_hz();
	unsigned lcore_id = rte_lcore_id();
	uint64_t cur_time = rte_get_timer_cycles();

	struct h_cache *he = (struct h_cache *) data;
	struct h_bucket *b = he->bucket;
	if ((rte_rdtsc() - he->timeo) / hz > he->table->timeo){
		rte_timer_stop(tim);
		cds_list_del(&he->list);
//        list_del(&he->list);
        ht = he->table;
        ht->ops->release(he);
        __h_table_len_dec(ht);
		memset(tim, 0xAA, sizeof(struct rte_timer));
	} else {

		int ret = rte_timer_reset(&he->timer, 5*hz, SINGLE, lcore_id,
				h_cache_timer_func, (void *)(uintptr_t)he);
	}
}
int __h_cache_try_get(struct h_table *ht, void *key, struct h_cache **ret_he,
        struct h_cache *(*create)(struct h_table *, void *),
        void (*modify)(struct h_cache *, void *) )
{
    struct h_bucket *b = &ht->base[ht->ops->hash(key) & (ht->size - 1)];
    struct h_cache *he;
    cds_list_for_each_entry(he, &b->chain, list)
    {
        if(ht->ops->comp_key(he, key) == 0)
        {
            /* Invoke the call back to do modifications. */
            if(modify)
                modify(he, key);
            he->timeo = rte_rdtsc();
            *ret_he = he;
            return 1;
        }
    }

    /* Not found, try to create a new entry. */
    if(create == NULL)
    {
        return -1;
    }

    /* If the table is full, try to recycle idle entries. */
    if(h_table_len(ht) >= ht->max_len)
    {
            printf( "-- ht->len: %d, ht->max_len: %d\n", (int)ht->len, (int)ht->max_len);
        return -1;
    }

    if((he = create(ht, key)) == NULL)
    {
        return -1;
    }


    /* Initialize the base class. */
    he->bucket = b;
    he->table = ht;


//    init_list_entry(&he->list);
	cds_list_add(&he->list, &b->chain);
    //list_add_head(&he->list, &b->chain);
    he->refs = 1;
    if (ht->timeo > 0) {
        uint64_t hz = rte_get_timer_hz();
        unsigned lcore_id = rte_lcore_id();
        rte_timer_init(&he->timer);
        he->timeo = rte_rdtsc();
        rte_timer_reset_sync(&he->timer, 5*hz, SINGLE, lcore_id,
                h_cache_timer_func, (void *)(uintptr_t)he);
    }

    *ret_he = he;
    __h_table_len_inc(ht);
    return 0;
}
