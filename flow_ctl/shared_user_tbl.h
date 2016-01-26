/*
 * Identify the shared user
 *
 * Author: junwei.dong
 */
#ifndef __SHARED_USER_TBL_H
#define __SHARED_USER_TBL_H

#include <linux/types.h>

#include "h_cache.h"
#include "com.h"
    
#define SHARED_USER_HASH_SIZE    (1 << 10)
#define SHARED_USER_MAX_LEN      (SHARED_USER_HASH_SIZE * 4)
#define SHARED_USER_TIMEOUT      (120)

#define SHARED_USER_MAX_PKT_NUM  (MAX_PKT_NUM + 5)

enum SHARED_FLAG
{
    SHARED_IP,
    APP_QQ,
    APP_MAX
};

struct shared_user_cache_key
{
    uint8_t  shared_flag;
    uint32_t shared_key;
};

struct shared_user_cache_value
{
    uint32_t shared_value;
};

struct shared_user_cache
{
	struct h_scalar h_scalar;
	
	/* Last check time (time stamp) */
	struct timespec last_check;
	
	struct shared_user_cache_key flck;
	struct shared_user_cache_value flcv;
};

struct shared_user_cache *shared_user_cache_try_get(struct shared_user_cache_key *k);
int shared_user_lookup_behavior(struct shared_user_cache_key *k, struct shared_user_cache_value *v);
int shared_user_try_free_key(struct shared_user_cache_key *k);
int shared_user_cache_init(void);
void shared_user_cache_exit(void);
void shared_user_cache_clear(void);

void increase_statistics_qq(struct ssn_skb_values * ssv);
void decrease_statistics_qq(struct sft_fdb_entry * ssn) ;
#endif       
