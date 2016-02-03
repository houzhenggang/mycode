#ifndef __STUDY_TBL_H
#define __STUDY_TBL_H

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
    


#define STUDY_HASH_SIZE    (1 << 18)
#define STUDY_MAX_LEN      (STUDY_HASH_SIZE * 8)
#define STUDY_TIMEOUT      (1200)
#define STUDY_OUTPUT_INTERVAL    (300)
    
#define STUDY_RULE_RECHECK_TIMEOUT  10

struct study_cache
{
    struct h_scalar h_scalar;
    /* Last check time (time stamp) */
//    struct timespec last_check;
    unsigned int proto_mark;
    __u32       ip;
    __u16       port;
    uint8_t     lcore_id;
    rte_spinlock_t lock;
};
//struct study_cache *study_cache_try_get(const __u32 ip, const __u16 port, unsigned int proto_mark);
int study_cache_try_get(const __u32 ip, const __u16 port, unsigned int proto_mark, uint8_t is_static, uint64_t timeo);
unsigned int study_lookup_behavior(const __u32 ip, __u16 port);
void study_cache_clear(void);
void study_cache_static_clear(void);
int study_cache_init(void);
void study_cache_exit(void);
#endif                                                                                                                         
