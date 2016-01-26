#ifndef __IP_FRAG_H__
#define __IP_FRAG_H__

int decode_skb_ipfrag(struct rte_mbuf * skb, struct ssn_skb_values * ssv, struct sft_fdb_entry * ssn);

#endif
