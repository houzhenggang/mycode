#ifndef __SESSION_TBL_H
#define __SESSION_TBL_H

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
    


#define SESSION_HASH_SIZE    (1 << 9)
#define SESSION_MAX_LEN      (SESSION_HASH_SIZE * 4)
#define SESSION_TIMEOUT      (120)
#define SESSION_OUTPUT_INTERVAL    (300)
    
#define SESSION_RULE_RECHECK_TIMEOUT  10

struct session_cache
{
    struct h_scalar h_scalar;
    uint32_t    sip;
    uint16_t    sport;
    uint32_t    dip;
    uint16_t    dport;
    uint16_t    protocol; 
    //    time_t timestamp;
    uint32_t    pack_num;
    uint32_t    pack_bytes;
    uint8_t     lcore_id;
    uint8_t     portid;
    rte_spinlock_t lock;
};
//struct session_cache *session_cache_try_get(const __u32 ip, const __u16 port, unsigned int proto_mark);
struct session_cache * session_cache_try_get(const __u32 sip, const __u16 sport, const __u32 dip, const __u16 dport, uint16_t protocol, int *retv);
//unsigned int session_lookup_behavior(const __u32 sip, const __u16 sport, const __u32 dip, const __u16 dport, uint16_t protocol);
int session_cache_init(void);
void session_cache_exit(void);
#endif                                                                                                                         
