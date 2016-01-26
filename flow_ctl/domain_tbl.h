#ifndef __DOMAIN_TBL_H
#define __DOMAIN_TBL_H

#include <stdint.h>

#include "h_cache.h"
    


#define DOMAIN_HASH_SIZE    (1 << 11)
#define DOMAIN_MAX_LEN      (DOMAIN_HASH_SIZE * 4)
#define DOMAIN_TIMEOUT      (120)
    
#define DOMAIN_RULE_RECHECK_TIMEOUT  10

struct domain_cache
{
    struct  h_scalar  h_scalar;
//    struct timespec last_check;
    unsigned int proto_mark;
    uint8_t lcore_id;
    char   url[150];
    rte_spinlock_t lock;
};
struct domain_cache *domain_cache_try_get(const char *url, unsigned int proto_mark);
unsigned int domain_lookup_behavior(const char *hostname);
int domain_cache_init(void);
void domain_cache_exit(void);
#endif                                                                                                                         
