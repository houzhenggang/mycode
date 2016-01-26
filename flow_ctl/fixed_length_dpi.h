/*
 * Identify the fixed length packet
 *
 * Author: junwei.dong
 */
#ifndef __FIXED_LENGTH_DPI_TBL_H
#define __FIXED_LENGTH_DPI_TBL_H

#include <linux/types.h>

#include "pattern_tbl.h"
#include "h_cache.h"
#include "com.h"
    
#define FIXED_LENGTH_HASH_SIZE    (1 << 10)
#define FIXED_LENGTH_MAX_LEN      (FIXED_LENGTH_HASH_SIZE * 4)
#define FIXED_LENGTH_TIMEOUT      (0)

#define FIXED_LENGTH_MAX_PKT_NUM  (MAX_PKT_NUM + 5)

struct fixed_length_cache_key
{
	uint8_t proto_type:3,
		    pkt_dir:1,
		    pkt_num:4;
	uint16_t payload_len; 
	uint16_t port;
};

struct fixed_length_cache_value
{
	pattern_t * pattern;
};

struct fixed_length_cache
{
	struct h_scalar h_scalar;
	
	/* Last check time (time stamp) */
	struct timespec last_check;
	
	struct fixed_length_cache_key flck;
	struct fixed_length_cache_value flcv;
};

struct fixed_length_cache *fixed_length_cache_try_get(struct fixed_length_cache_key *k);
int fixed_length_lookup_behavior(struct fixed_length_cache_key *k, struct fixed_length_cache_value *v);
int fixed_length_cache_init(void);
void fixed_length_cache_exit(void);
void fixed_length_cache_clear(void);
uint16_t get_pattern_id_from_fixed_length_hash_tbl(struct fixed_length_cache_key *flck, uint16_t dport, uint16_t sport);
void identify_fixed_length_packet(struct ssn_skb_values *ssv);
#endif       
