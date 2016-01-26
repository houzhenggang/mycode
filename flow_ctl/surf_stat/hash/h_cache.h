#ifndef __H_TABLE_H
#define __H_TABLE_H
#include "list.h"
struct h_cache
{
    struct list_head  list;
    struct h_bucket  *bucket;
    struct h_table   *table;
};

struct h_bucket
{
    struct list_head chain;
    uint32_t hash_level;
};

struct h_table
{
    int table_type;
    struct h_operations *ops;
    struct h_bucket *base;
    int      static_base;
    size_t    size;
    size_t    max_len;
    size_t    len;
    long      timeo;
    unsigned int tid;
};
/* Hash table operation collections. */
struct h_operations
{
    uint32_t   (*hash)(void *key);
    int   (*comp_key)(struct h_cache *he, void *key);
    void  (*release)(struct h_cache *he);
    char *(*build_line)(struct h_cache *he);
    int   (*operate_cmd)(struct h_table *ht, const char *cmd);
};

int h_table_iterate_safe(struct h_table *ht, void (*operate)(struct h_cache *),
        int max_duration, int min_per_sec );

int h_table_clear(struct h_table *ht);

int h_cache_try_get(struct h_table *ht, void *key, struct h_cache **ret_he,
        struct h_cache *(*create)(struct h_table *, void *),
        void (*modify)(struct h_cache *, void *) );
void h_entry_put(struct h_cache *he);
void h_entry_put_free(struct h_cache *he);



int h_table_create(struct h_table *ht, 
        struct h_bucket *base, size_t size, size_t max_len, long timeo,
        struct h_operations *ops);
int h_table_release(struct h_table *ht);

int h_cache_try_lookup(struct h_table *ht, void *key, void *result,
		void (*action)(struct h_cache *, void *, void *) );
#define true  1
#define false 0

#endif /* __H_TABLE_H */
