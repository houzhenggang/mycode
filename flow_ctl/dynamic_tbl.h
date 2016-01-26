#ifndef __DYNAMIC_TBL_H
#define __DYNAMIC_TBL_H

#include <linux/types.h>

#include "pattern_tbl.h"
#include "h_cache.h"
    

#define DYNAMIC_HASH_SIZE    (1 << 12)
#define DYNAMIC_MAX_LEN      (DYNAMIC_HASH_SIZE * 4)
#define DYNAMIC_TIMEOUT      (120)
#define DYNAMIC_OUTPUT_INTERVAL    (300)
    
#define DYNAMIC_RULE_RECHECK_TIMEOUT  10

struct dynamic_cache
{
    struct h_scalar h_scalar;
    /* Last check time (time stamp) */
    struct timespec last_check;
    unsigned int proto_mark;
    __u32       ip;
    __u16       port;
    uint8_t lcore_id;
    rte_spinlock_t lock;
};
struct dynamic_cache *dynamic_cache_try_get(const __u32 ip, const __u16 port, unsigned int proto_mark, uint8_t is_static);
unsigned int dynamic_lookup_behavior(const __u32 ip, __u16 port);
void dynamic_cache_clear(void);
void dynamic_cache_static_clear(void);
int dynamic_cache_init(void);
void dynamic_cache_exit(void);
#endif                                                                                                                         
