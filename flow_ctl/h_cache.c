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
#include "hash/h_cache.h"
#include "log.h"

int h_table_create(struct h_table *ht,
        struct h_bucket *base, size_t size, size_t max_len, long timeo,
        struct h_operations *ops)

{

    struct h_bucket *b;
    size_t __size;
    int i;

    memset(ht, 0x0, sizeof(ht[0]));

    for(__size = size - 1, i = 0; __size; __size >>= 1, i++);
    __size = (1UL << i);
    if(size != __size)
    {
        marbit_send_log(INFO,"Warning: size '%lu' is accepted as '%lu'.\n", 
                                            (unsigned long)size, (unsigned long)__size);
        size = __size;
    }

    if(base)
    {
        ht->base = base;
        ht->static_base = true;
    }
    else
    {
        if((ht->base = (struct h_bucket *) malloc(sizeof(struct h_bucket) * size)) == NULL)
            return -1;
        ht->static_base = false;
    }

    ht->size  = size;
    ht->max_len   = max_len;
    ht->timeo = timeo;
    ht->len   = 0;


    {
        ht->ops   = ops;
        for(i = 0; i < size; i++)
        {
            b = &ht->base[i];
            INIT_LIST_HEAD(&b->chain);
        }



        /* Setup idle queue timeout checking timer. */
        if(ht->timeo > 0)
        {
            static int ht_index = 0;
            int n;

            n = ht_index++;

        }

        //return -1;
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
		list_for_each_entry_safe(he, he_next, &b->chain, list)
		{
			if(operate)
				operate(he);
			count++;
		}
	}/* for(i = 0; i < ht->size; i++) */
	
	return count;
}
int h_cache_try_get(struct h_table *ht, void *key, struct h_cache **ret_he,
        struct h_cache *(*create)(struct h_table *, void *),
        void (*modify)(struct h_cache *, void *) )
{
    struct h_bucket *b = &ht->base[ht->ops->hash(key) & (ht->size - 1)];
    struct h_cache *he;
    list_for_each_entry(he, &b->chain, list)
    {
        if(ht->ops->comp_key(he, key) == 0)
        {
            /* Invoke the call back to do modifications. */
            if(modify)
                modify(he, key);
            *ret_he = he;
            return 1;
        }
    }

    /* Not found, try to create a new entry. */
    if(create == NULL)
    {
        return -1;
    }
#if 0
    /* If the table is full, try to recycle idle entries. */
    if(h_table_len(ht) >= ht->max_len)
    {
            printf( "-- ht->len: %d, ht->max_len: %d\n", (int)ht->len, (int)ht->max_len);
        return -1;
    }
#endif
    if((he = create(ht, key)) == NULL)
    {
        return -1;
    }


    /* Initialize the base class. */
    he->bucket = b;
    he->table = ht;

//    init_list_entry(&he->list);
	list_add(&he->list, &b->chain);
    //list_add_head(&he->list, &b->chain);

    ++ht->len;
    
    *ret_he = he;

    
    return 0;
}

int h_table_clear(struct h_table *ht)
{

	int i, count = 0;
	struct h_bucket *b;
	struct h_cache *he, *he_next;
	
	for(i = 0; i < ht->size; i++)
	{
		b = &ht->base[i];
		list_for_each_entry_safe(he, he_next, &b->chain, list)
		{
                    list_del(&he->list);
                    ht->ops->release(he);
                    --ht->len;
                    count++;
		}
	}/* for(i = 0; i < ht->size; i++) */
	
    return count;
}

int h_table_release(struct h_table *ht)
{
    size_t ht_len = ht->len;
    if(ht->base)
    {
        if(h_table_clear(ht) != ht_len)
            return -1;
        if(!ht->static_base)
            free(ht->base);
        ht->base = NULL;
    }
    ht->size = 0;

    return 0;
}

int h_cache_try_lookup(struct h_table *ht, void *key, void *result,
		void (*action)(struct h_cache *, void *, void *) )
{
	struct h_bucket *b = &ht->base[ht->ops->hash(key) & (ht->size - 1)];
	struct h_cache *he;
	
	list_for_each_entry(he, &b->chain, list)
	{
		if(ht->ops->comp_key(he, key) == 0)
		{
			/* Invoke the callback to do modifications or return values. */
			if(action) {
				action(he, key, result);
           		}
			return 0;
		}
	}
	
	/* Not found, return error code. */
	return -1;
}