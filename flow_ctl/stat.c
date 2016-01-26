#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <syslog.h>
#include <string.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>

#include "debug.h"
#include "stat.h"
#include <rte_lcore.h>
#include "tc_private.h"

#define SHM_SIZE    4096
#define SHM_MODE    (SHM_R | SHM_W | IPC_CREAT) /* user read/write */
skbstat_t *skb_stat;

uint64_t rte_timer_tsc[8] = {0};
uint64_t send_pkt_tsc[8] = {0};
extern rte_atomic64_t nums_sft_fip_entry;

void diff_timer_func(struct rte_timer *tim, void *data)
{
    uint64_t hz = rte_get_timer_hz();
    unsigned lcore_id = rte_lcore_id();
    uint64_t cur_time = rte_get_timer_cycles();
    if (skb_stat->skb_num[lcore_id] && skb_stat->diff_tsc[lcore_id]) {
        //skb_stat->arg_diff_tsc[lcore_id] = skb_stat->diff_tsc[lcore_id] / skb_stat->skb_num[lcore_id];
        skb_stat->total_tsc[lcore_id] = skb_stat->diff_tsc[lcore_id];
        skb_stat->total_num[lcore_id] = skb_stat->skb_num[lcore_id];
        skb_stat->diff_tsc[lcore_id] = 0;
        skb_stat->skb_num[lcore_id] = 0;
    }

    skb_stat->rte_timer_tsc_per_sec[lcore_id]=  rte_timer_tsc[lcore_id] / 5;
    skb_stat->send_pkt_tsc_per_sec[lcore_id]=  send_pkt_tsc[lcore_id] / 5;
    rte_timer_tsc[lcore_id] = 0;
    send_pkt_tsc[lcore_id] = 0;
    skb_stat->nums_sft_fip_entry = rte_atomic64_read(&nums_sft_fip_entry);
	skb_stat->cache_pkg_num = pkg_num_read();
	if(rte_eth_devices[lcore_id].data)
		skb_stat->failed_recv_pkg[lcore_id] = rte_eth_devices[lcore_id].data->rx_mbuf_alloc_failed;

    int ret = rte_timer_reset(tim, 5*hz, SINGLE, lcore_id,
            diff_timer_func, NULL);
}


int show_stat_init()
{
    int bufflen;
    int id;
    bufflen = sizeof(skbstat_t);

    id = shmget(STAT_SHM_KEY, SHM_SIZE, SHM_MODE | IPC_CREAT);
    if(id<0) {
        E("call shmget error int StatInit !\n");
        skb_stat = malloc(bufflen);
        if(skb_stat == NULL) {
            E("Call shmat error in StatInit!\n");
            exit(-1);
        }
        return 0;
    }

    skb_stat = (skbstat_t *)shmat(id, NULL, 0);

    memset(skb_stat, 0, sizeof(skbstat_t));

    if(skb_stat == NULL) {
        E("Call shmat error in StatInit!\n");
        exit(-1);
    }

    return 0;
}
