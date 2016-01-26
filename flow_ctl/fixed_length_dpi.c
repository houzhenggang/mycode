/*
 * Identify the fixed length packet
 *
 * Author: junwei.dong
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <rte_cycles.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_spinlock.h>
#include <rte_malloc.h>
#include <rte_jhash.h>

#include "debug.h"
#include "fixed_length_dpi.h"
#include "domain_tbl.h"

#include "h_cache.h"
#include "utils.h"
#include "session_mgr.h"
#include "rules.h"
	
extern struct state_array *DPI_State_TBL;
static uint32_t g_hash_initval;

struct  fixed_length_cache_table
{
	struct h_table h_table;
	struct rte_mempool *mem_cache;
} fixed_length_cache_tbl;

static inline void update_proto_mark(struct ssn_skb_values *ssv,unsigned int newmark,int flag)
{
    if(NULL == ssv)
        return;

    if(NULL == ssv->ssn)
        return;
    
    if(newmark == ssv->ssn->proto_mark)
    {
        return;
    }

    ssv->ssn->proto_mark = newmark;
    ssv->ssn->proto_mark_state = 1;
    //ssv->ssn->need_update_link = 1;
    //ssv->ssn->identifies_proto = 1;
    return;
}

static uint32_t  fixed_length_cache_hash_fn(void *key)
{
	struct  fixed_length_cache_key *k = (struct  fixed_length_cache_key *)key;                                                              

	//build unite_key to hash
	uint32_t unite_key = (uint32_t)(k->proto_type<<21) + 
						(uint32_t)(k->pkt_dir << 20) + 
						(uint32_t)(k->pkt_num<<16) + 
						(uint32_t)(k->payload_len);
	
	return rte_jhash_2words(unite_key, k->port, g_hash_initval);	
}

static int  fixed_length_cache_comp_key_fn(struct h_scalar *he, void *key)
{
	struct fixed_length_cache *dc = container_of(he, struct fixed_length_cache, h_scalar);
	struct fixed_length_cache_key *k = (struct fixed_length_cache_key *)key;

//	D("key: proto_type[%d], pkt_dir[%d], pkt_num[%d], payload_len[%d], port[%d]\n",
//		k->proto_type, k->pkt_dir, k->pkt_num, k->payload_len, k->port);
	
	return (	dc->flck.proto_type ^k->proto_type ||
			dc->flck.pkt_dir ^ k->pkt_dir ||
			dc->flck.pkt_num ^ k->pkt_num ||
			dc->flck.payload_len ^ k->payload_len ||
			dc->flck.port ^ k->port);
}

static void fixed_length_cache_release_fn(struct h_scalar *he)
{
	struct fixed_length_cache *dc = container_of(he, struct fixed_length_cache, h_scalar);

	rte_mempool_mp_put(fixed_length_cache_tbl.mem_cache, dc);
}

static char *fixed_length_cache_build_line_fn(struct h_scalar *he)
{
	struct fixed_length_cache *dc = container_of(he, struct fixed_length_cache, h_scalar);
	char *line;

	if((line = (char *)rte_malloc(NULL, 200, 0)) == NULL)
		return NULL;
	
	snprintf(line, 150, "fixed_length: proto_type[%d], pkt_dir[%d], pkt_num[%d], payload_len[%d], port:[%d]\n", 
					dc->flck.proto_type, dc->flck.pkt_dir, dc->flck.pkt_num, 
					dc->flck.payload_len, ntohs(dc->flck.port));
	return line;
}

void fixed_length_cache_clear(void)
{
	D("clear fixed length cache table\n");
	h_scalar_table_clear(&fixed_length_cache_tbl.h_table);
}

static struct h_scalar_operations fixed_length_cache_ops =
{
	.hash        = fixed_length_cache_hash_fn,
	.comp_key    = fixed_length_cache_comp_key_fn,
	.release     = fixed_length_cache_release_fn,
	.build_line  = fixed_length_cache_build_line_fn,
};


static struct h_scalar *fixed_length_cache_create_fn(struct h_table *ht, void *key)
{
	struct fixed_length_cache_key *k = (struct fixed_length_cache_key *)key;
	struct fixed_length_cache *dc;

	if(rte_mempool_mc_get(fixed_length_cache_tbl.mem_cache, (void **)&dc) <0)
		return NULL;

	dc->flck = *k;

	return &dc->h_scalar;
}

//struct fixed_length_cache *fixed_length_cache_try_get(const __u16 port, unsigned int proto_mark)
struct fixed_length_cache *fixed_length_cache_try_get(struct fixed_length_cache_key *k)
{
	struct h_scalar *he;
	struct fixed_length_cache *dc;

	if((h_scalar_try_get(&fixed_length_cache_tbl.h_table, k, &he,fixed_length_cache_create_fn, NULL, 1, 0)) == -1)
		return NULL;

	dc = container_of(he, struct fixed_length_cache, h_scalar);

	return dc;
}

static void __fixed_length_cache_lookup_action(struct h_scalar *he, void *key, void *result)
{
	struct fixed_length_cache *dc = container_of(he, struct fixed_length_cache, h_scalar);

	*( struct fixed_length_cache_value *) result = dc->flcv;
}

int fixed_length_lookup_behavior(struct fixed_length_cache_key *k, struct fixed_length_cache_value *v)
{
	/* Loopup precise table. */
	if(h_scalar_try_lookup(&fixed_length_cache_tbl.h_table, k, v, __fixed_length_cache_lookup_action) == 0)
		return 0;
	
	return -1;
}

static void
my_obj_init(struct rte_mempool *mp, __attribute__((unused)) void *arg,
        void *obj, unsigned i)
{
	uint32_t *objnum = obj;
	memset(obj, 0, mp->elt_size);
	*objnum = i;
}
static void my_mp_init(struct rte_mempool * mp, __attribute__((unused)) void * arg)
{
	printf("mempool name is %s\n", mp->name);
	/* nothing to be implemented here*/
	return ;
}

int fixed_length_cache_init(void)
{
	int retv = -1;

	srand(time(NULL));
	g_hash_initval = random();
	printf("---------------------fixed_length_cache_init------------------------------\n");	

	/* create a mempool (with cache) */
	if ((fixed_length_cache_tbl.mem_cache = rte_mempool_create("fixed_length_mem_cache", FIXED_LENGTH_MAX_LEN,
		sizeof(struct fixed_length_cache),
		32, 0,
		my_mp_init, NULL,
		my_obj_init, NULL,
		SOCKET_ID_ANY, 0 /*MEMPOOL_F_NO_CACHE_ALIGN | MEMPOOL_F_SP_PUT | MEMPOOL_F_SC_GET*/)) == NULL)
	{
		retv = -ENOMEM;
		goto err_kmem_create;
	}
	
	if(h_scalar_table_create(&fixed_length_cache_tbl.h_table, NULL, FIXED_LENGTH_HASH_SIZE, FIXED_LENGTH_MAX_LEN, FIXED_LENGTH_TIMEOUT, &fixed_length_cache_ops) != 0)
	{
		retv = -1;
		goto err_fixed_length_create;
	}

	return 0;

	err_fixed_length_create:
	err_kmem_create:
		E("fixed_length_create fail\n");

	return retv;
}

void fixed_length_cache_exit(void)
{
	h_scalar_table_release(&fixed_length_cache_tbl.h_table);
}

uint16_t get_pattern_id_from_fixed_length_hash_tbl(struct fixed_length_cache_key *flck, uint16_t dport, uint16_t sport)
{
	struct fixed_length_cache_value flcv;
	bzero(&flcv, sizeof(struct fixed_length_cache_value));
	
	//check dport
	flck->port = dport;
	if ( (!fixed_length_lookup_behavior(flck, &flcv)) )
	{
		//check pattern
		if (flcv.pattern)
		{
			if (dport == flcv.pattern->pattern_key.dport)
			{
				return flcv.pattern->id;
			}
		}
	}
	
	//check sport
	flck->port = sport;
	if ( (!fixed_length_lookup_behavior(flck, &flcv)) )
	{
		//check pattern
		if (flcv.pattern)
		{
			if (sport == flcv.pattern->pattern_key.sport)
			{
				return flcv.pattern->id;
			}
		}
	}
	
	//check any port
	flck->port = 0;
	if ( (!fixed_length_lookup_behavior(flck, &flcv)) )
	{
		//check pattern
		if (flcv.pattern)
		{
			return flcv.pattern->id;
		}
	}

	return 0;
}

void identify_fixed_length_packet(struct ssn_skb_values *ssv)
{
    if(NULL == ssv)
        return;

    if(NULL == ssv->ssn)
        return;
		
    if (ssv->ssn->pkt_cnt > FIXED_LENGTH_MAX_PKT_NUM)
	    return;

    int i = 0;
    int cnt = 0;
    int next_state_id = 0;
    unsigned short *id_list = NULL;
    struct fixed_length_cache_key flck;
    uint32_t pattern_id = 0;

       //build key
    bzero(&flck, sizeof(struct fixed_length_cache_key));
    switch(ssv->l4proto)
    {
    	case IPPROTO_TCP : 
    	{
    		flck.proto_type = DPI_PROTO_TCP;
    		break;
    	}
    	case IPPROTO_UDP : 
    	{
    		flck.proto_type = DPI_PROTO_UDP;
    		break;
    	}
    	default:
    	{
    		return;
    	}
    }
    flck.pkt_num = ssv->ssn->pkt_cnt;
    flck.payload_len = ssv->payload_len;
    if(ssv->isinner)//uplink-request-pkt_dir:1:req
    {
    	flck.pkt_dir = 1; 
    }
    else //downlink-response-pkt_dir:0:res
    {
    	flck.pkt_dir = 0; 
    }

    //search hash table, and get pattern id
    pattern_id = get_pattern_id_from_fixed_length_hash_tbl(&flck, ssv->dport, ssv->sport);
    if (!pattern_id)
    	return;

    //check state table
    cnt = ssv->ssn->state_cnt;
    id_list = ssv->ssn->dpi_state_id_list;
    for(i = 0; i < ssv->ssn->state_cnt && cnt < MAX_SSN_STATE_NUM; i++)//±éÀúid_list
    {
    	next_state_id = DPI_State_TBL[id_list[i]].pattern[pattern_id].nextstate_id;

    	if(0 == next_state_id)
    	{
    		continue;
    	}

    	id_list[cnt] = next_state_id;
    	cnt++;
    	
    	//  printk_G("%s(%d),i:%d,[%d],next_state_id:%d,%s,cnt:%d.\n",__FUNCTION__,__LINE__,i,id_list[i],next_state_id,pattern->pattern_name,cnt);
    	if(DPI_State_Flag_TBL[next_state_id] & 0x1)//final state
    	{
    		//update ssn's proto_mark 
    		//if(DPI_State_TBL[next_state_id].type_id != ssv->ssn->proto_mark)
    		//{
    			update_proto_mark(ssv,DPI_State_TBL[next_state_id].type_id,0);
    	//	}
    		break;
    	}	
    }//for loop
    ssv->ssn->state_cnt = cnt;

    return;
}


