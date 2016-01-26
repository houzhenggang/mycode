#ifndef _STAT_H
#define _STAT_H
#define STAT_SHM_KEY 0x99
#include <rte_timer.h>

typedef struct {
    uint64_t diff_tsc[8];
    uint64_t arg_diff_tsc[8];
    uint64_t skb_num[8];
    uint64_t max_arg_diff_tsc[8];
    uint64_t total_tsc[8];
    uint64_t total_num[8];
    uint32_t hash_level_water_mark;
    uint32_t exceed_hash_level_count;
    uint32_t max_hash_level;
    uint64_t fail_skb;
    uint64_t exceed_high_water_mark_count;
    uint64_t nums_sft_fip_entry;
    uint64_t rte_timer_tsc_per_sec[8];
    uint64_t send_pkt_tsc_per_sec[8];
    uint8_t web_mesg_flag;
    uint64_t cache_pkg_num;
    uint64_t failed_recv_pkg[8];
    int web_msgid;
}skbstat_t;
int show_stat_init();
extern skbstat_t *skb_stat;

extern uint64_t send_pkt_tsc[8];
extern uint64_t rte_timer_tsc[8];
void diff_timer_func(struct rte_timer *tim, void *data);
#endif
