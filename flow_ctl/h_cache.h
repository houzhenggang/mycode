#ifndef __H_TABLE_H
#define __H_TABLE_H
#include <urcu.h>
#include <urcu/rculist.h>
#include <asm-generic/errno-base.h>


#include <rte_spinlock.h>
#include <rte_rwlock.h>
#include <rte_timer.h>
#include "list.h"
#include "timer.h"

/* Scalar hash entry (mostly used for rule tables). */
struct h_scalar
{
	struct cds_list_head  list;
    struct rcu_head rcu;
    //struct rte_timer timer;
    struct timer_list timer;
	struct h_bucket  *bucket;
	struct h_table   *table;
    uint64_t timeo;
    uint64_t timestamp;
    uint8_t is_static:1,
            deleted:1;
};
struct h_cache
{
	struct cds_list_head  list;
    struct rte_timer timer;
    struct h_bucket  *bucket;
    struct h_table   *table;
    uint32_t refs;
    uint64_t timeo;
    unsigned int tid;
};

struct h_bucket
{
	struct cds_list_head chain;
    rte_atomic16_t bucket_level;
    rte_spinlock_t lock;
};

enum __h_table_type
{
	H_TABLE_TYPE_CACHE = 0,
	H_TABLE_TYPE_SCALAR,
};

struct h_table
{
    int table_type;
    union
    {
        struct h_operations *ops;
        struct h_scalar_operations *s_ops;
    };
    struct h_bucket *base;
    int      static_base;
    size_t    size;
    size_t    max_len;
    size_t    len;
    uint64_t      timeo;
    rte_rwlock_t rwlock;
    uint8_t tid;
};

/* Scalar hash table operation collections. */
struct h_scalar_operations
{
	uint32_t   (*hash)(void *key);
	int   (*comp_key)(struct h_scalar *he, void *key);
	void  (*release)(struct h_scalar *he);
	char *(*build_line)(struct h_scalar *he);
	int   (*operate_cmd)(struct h_table *ht, const char *cmd);
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

/* Exported operations. */

static inline int h_table_len(struct h_table *ht)
{
	return ht->len;
}

int h_table_iterate_safe(struct h_table *ht, void (*operate)(struct h_cache *),
        int max_duration, int min_per_sec );

int h_table_clear(struct h_table *ht);

int __h_cache_try_get(struct h_table *ht, void *key, struct h_cache **ret_he,
        struct h_cache *(*create)(struct h_table *, void *),
        void (*modify)(struct h_cache *, void *) );

static inline int h_entry_try_get(struct h_table *ht, void *key, struct h_cache **ret_he,
        struct h_cache *(*create)(struct h_table *, void *),
        void (*modify)(struct h_cache *, void *) )
{
    return __h_cache_try_get(ht, key, ret_he, create, modify);
}

void h_entry_put(struct h_cache *he);
void h_entry_put_free(struct h_cache *he);



int __h_table_create(struct h_table *ht, enum __h_table_type table_type,
        struct h_bucket *base, size_t size, size_t max_len, uint64_t timeo,
        struct h_operations *ops);
static inline int h_table_create(struct h_table *ht,
        struct h_bucket *base, size_t size, size_t max_len, uint64_t timeo,
        struct h_operations *ops)
{
    return __h_table_create(ht, H_TABLE_TYPE_CACHE, base, size, max_len, timeo,
        ops);
}

/* The following methods are usally for scalar and non-timer uses. */

int h_scalar_try_get(struct h_table *ht, void *key, struct h_scalar **ret_he,
		struct h_scalar *(*create)(struct h_table *, void *),
		void (*modify)(struct h_scalar *, void *), uint8_t is_static, uint64_t timeo);

int h_scalar_try_lookup(struct h_table *ht, void *key, void *result,
		void (*action)(struct h_scalar *, void *, void *) );

void *h_scalar_lookup_by_user(struct h_table *ht, void *priv, void *(*operate)(struct h_scalar *, void *));
int h_scalar_try_free_key(struct h_table *ht, void *key);

int h_scalar_table_iterate_safe(struct h_table *ht,  void (*operate)(struct h_scalar *));

void h_scalar_table_clear(struct h_table *ht);
void h_scalar_table_static_clear(struct h_table *ht);

void h_scalar_table_release(struct h_table *ht);

static inline int h_scalar_table_create(struct h_table *ht, struct h_bucket *base,
		size_t size, size_t max_len, uint64_t timeo, struct h_scalar_operations *s_ops)
{
	return __h_table_create(ht, H_TABLE_TYPE_SCALAR, base, size, max_len, timeo,
		//(struct h_scalar_operations *)s_ops);
		(struct h_operations *)s_ops);
}

void queue_timer_func(struct rte_timer *tim, void *data);
#endif /* __H_TABLE_H */
