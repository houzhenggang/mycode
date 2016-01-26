#ifndef _TC_FRAME_FWD_H_
#define _TC_FRAME_FWD_H_

#include "tc_private.h"

int sft_queue_xmit_hook(struct rte_mbuf *skb);

void sft_tc_send_oneip_handler(struct rte_timer *tim, void *data);
void sft_tc_send_pipe_handler(struct rte_timer *tim, void *data);

void sft_queue_xmit(struct rte_mbuf* m);

#endif
