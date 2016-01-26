#ifndef _TCP_PACK_H_
#define _TCP_PACK_H_

#include "session_mgr.h"
#include "com.h"

void tcp_ofo_mem_init(void);
int tcp_ofo_retrans_handle(struct ipv4_hdr *iph, struct sft_fdb_entry * ssn, struct rte_mbuf  * skb);
void tcp_ofo_list_packet_free(struct l2ct_var_dpi *var);
void check_tcp_ofo(struct ssn_skb_values  *ssv);

#endif

