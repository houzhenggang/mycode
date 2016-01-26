/*
 * operate the data_aggregation hash table
 *
 * Author: junwei.dong
 */
#ifndef __DATA_AGGREGATION_TBL_H
#define __DATA_AGGREGATION_TBL_H

#include <linux/types.h>

#include "hash/h_cache.h"
#include "common.h"
    
#define DATA_AGGREGATION_HASH_SIZE    (1 << 16)
#define DATA_AGGREGATION_MAX_LEN      (DATA_AGGREGATION_HASH_SIZE * 4)
#define DATA_AGGREGATION_TIMEOUT      (0)

struct  data_aggregation_cache_table
{
	struct h_table h_table;
	//struct rte_mempool *mem_cache;
} data_aggregation_cache_tbl;

struct data_aggregation_cache_key
{
    uint32_t appid_ip_key;
    uint16_t hash_class;
};

struct data_aggregation_cache_value
{
    sorted_node_t *sorted_node; 

    merge_sorted_node_t *merge_sorted_node; 
};



struct data_aggregation_cache
{
	struct h_cache h_cache;
	
	/* Last check time (time stamp) */
	//struct timespec last_check;
	
	struct data_aggregation_cache_key flck;
	struct data_aggregation_cache_value flcv;

       //baseapp_sorted_node_t *sorted_node; 
};

struct data_aggregation_cache *data_aggregation_cache_try_get(struct data_aggregation_cache_key *k);
int data_aggregation_lookup_behavior(struct data_aggregation_cache_key *k, struct data_aggregation_cache_value *v);
int data_aggregation_cache_init(void);
void data_aggregation_cache_exit(void);
void data_aggregation_cache_clear(void);
void destroy_aggregation_info();


void process_data_aggregation_list(struct msg_st *mesg,
                                                            data_aggregation_class data_agg_class,
                                                            sorted_list_condition sorted_list_index,
                                                            data_sort_condition data_sort_condition);

void do_data_aggregation_from_db(const char* dbname);

#endif       
