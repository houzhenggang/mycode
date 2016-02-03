#ifndef __DNS_STUDY__TBL_H
#define __DNS_STUDY__TBL_H

#include <linux/types.h>
#include <urcu.h>
#include <urcu/rculist.h>
#include <asm-generic/errno-base.h>


#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include "h_cache.h"
    


#define DNS_STUDY_HASH_SIZE    (1 << 10)
#define DNS_STUDY_MAX_LEN      (DNS_STUDY_HASH_SIZE * 4)
#define DNS_STUDY_TIMEOUT      (1200)
#define DNS_STUDY_OUTPUT_INTERVAL    (300)
    
#define DNS_STUDY_RULE_RECHECK_TIMEOUT  10

struct dns_study_cache
{
    struct h_scalar h_scalar;
    /* Last check time (time stamp) */
//    struct timespec last_check;
    unsigned int proto_mark;
    __u32       ip;
    __u16       port;
    uint8_t lcore_id;
//    rte_spinlock_t lock;
};
//struct dns_study_cache *dns_study_cache_try_get(const __u32 ip, const __u16 port, unsigned int proto_mark);
int dns_study_cache_try_get(const __u32 ip, const __u16 port, unsigned int proto_mark);
unsigned int dns_study_lookup_behavior(const __u32 ip, __u16 port);
int dns_study_cache_init(void);
void dns_study_cache_exit(void);
#endif                                                                                                                         
