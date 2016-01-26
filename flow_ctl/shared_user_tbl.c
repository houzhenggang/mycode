/*
 * Identify the shared user
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
#include "shared_user_tbl.h"
#include "h_cache.h"
#include "utils.h"
#include "session_mgr.h"
	
extern struct state_array *DPI_State_TBL;
static uint32_t g_hash_initval;

struct  shared_user_cache_table
{
	struct h_table h_table;
	struct rte_mempool *mem_cache;
} shared_user_cache_tbl;

static uint32_t  shared_user_cache_hash_fn(void *key)
{
	struct  shared_user_cache_key *k = (struct  shared_user_cache_key *)key;                                                              
	
	return rte_jhash_2words(k->shared_flag, k->shared_key, g_hash_initval);	
}

static int  shared_user_cache_comp_key_fn(struct h_scalar *he, void *key)
{
	struct shared_user_cache *dc = container_of(he, struct shared_user_cache, h_scalar);
	struct shared_user_cache_key *k = (struct shared_user_cache_key *)key;
	
	return (	dc->flck.shared_flag^k->shared_flag || dc->flck.shared_key^k->shared_key);
}

static void shared_user_cache_release_fn(struct h_scalar *he)
{
    struct shared_user_cache *dc = container_of(he, struct shared_user_cache, h_scalar);

    rte_mempool_mp_put(shared_user_cache_tbl.mem_cache, dc);
}

static char *shared_user_cache_build_line_fn(struct h_scalar *he)
{
	struct shared_user_cache *dc = container_of(he, struct shared_user_cache, h_scalar);
	char *line;

	if((line = (char *)rte_malloc(NULL, 200, 0)) == NULL)
		return NULL;
    
	char *s;	
       if(dc->flck.shared_flag == SHARED_IP)
       {
	    snprintf(line, 150, "shared_user: ip[%s],shared_num[%u]\n", 
					ipv4_nltos(dc->flck.shared_key, s),  dc->flcv.shared_value);
       }
       else if(dc->flck.shared_flag == APP_QQ)
       {
            snprintf(line, 150, "shared_user: qq[%u], ip[%s]\n", 
					dc->flck.shared_key,  ipv4_nltos(dc->flcv.shared_value, s) );
       }
       
	return line;
}

void shared_user_cache_clear(void)
{
	D("clear shared_user cache table\n");
	h_scalar_table_clear(&shared_user_cache_tbl.h_table);
}

static struct h_scalar_operations shared_user_cache_ops =
{
	.hash        = shared_user_cache_hash_fn,
	.comp_key    = shared_user_cache_comp_key_fn,
	.release     = shared_user_cache_release_fn,
	.build_line  = shared_user_cache_build_line_fn,
};


static struct h_scalar *shared_user_cache_create_fn(struct h_table *ht, void *key)
{
    struct shared_user_cache_key *k = (struct shared_user_cache_key *)key;
    struct shared_user_cache *dc;

    if(rte_mempool_mc_get(shared_user_cache_tbl.mem_cache, (void **)&dc) <0)
        return NULL;

    dc->flck = *k;
    dc->flcv.shared_value = 0;

    return &dc->h_scalar;
}

struct shared_user_cache *shared_user_cache_try_get(struct shared_user_cache_key *k)
{
    struct h_scalar *he;
    struct shared_user_cache *dc;

    if((h_scalar_try_get(&shared_user_cache_tbl.h_table, k, &he,shared_user_cache_create_fn, NULL, 0, 0)) == -1)
        return NULL;

    dc = container_of(he, struct shared_user_cache, h_scalar);

    return dc;
}

int shared_user_try_free_key(struct shared_user_cache_key *k)
{
    return h_scalar_try_free_key(&shared_user_cache_tbl.h_table, k);
}

static void __shared_user_cache_lookup_action(struct h_scalar *he, void *key, void *result)
{
	struct shared_user_cache *dc = container_of(he, struct shared_user_cache, h_scalar);

	he->timeo = rte_rdtsc();

	*( struct shared_user_cache_value *) result = dc->flcv;
}

int shared_user_lookup_behavior(struct shared_user_cache_key *k, struct shared_user_cache_value *v)
{
	/* Loopup precise table. */
	if(h_scalar_try_lookup(&shared_user_cache_tbl.h_table, k, v, __shared_user_cache_lookup_action) == 0)
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

int shared_user_cache_init(void)
{
	int retv = -1;

	srand(time(NULL));
	g_hash_initval = random();
	printf("---------------------fixed_length_cache_init------------------------------\n");	

	/* create a mempool (with cache) */
	if ((shared_user_cache_tbl.mem_cache = rte_mempool_create("shared_user_mem_cache", SHARED_USER_MAX_LEN,
		sizeof(struct shared_user_cache),
		32, 0,
		my_mp_init, NULL,
		my_obj_init, NULL,
		SOCKET_ID_ANY, 0 /*MEMPOOL_F_NO_CACHE_ALIGN | MEMPOOL_F_SP_PUT | MEMPOOL_F_SC_GET*/)) == NULL)
	{
		retv = -ENOMEM;
		goto err_kmem_create;
	}
	
	if(h_scalar_table_create(&shared_user_cache_tbl.h_table, NULL, SHARED_USER_HASH_SIZE, SHARED_USER_MAX_LEN, SHARED_USER_TIMEOUT, &shared_user_cache_ops) != 0)
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

void shared_user_cache_exit(void)
{
	h_scalar_table_release(&shared_user_cache_tbl.h_table);
}

void increase_statistics_qq(struct ssn_skb_values * ssv)                                                               
{                                                                                                                                                                                                              
    uint32_t qq = 0;                                                                                          
    char qq_hex[5] = { 0 };                                                                                   
    unsigned int *qq_num;          
    
    struct shared_user_cache_key flck;
    struct shared_user_cache *flc = NULL;
    bzero(&flck, sizeof(struct shared_user_cache_key));
    
    //strncpy(qq_hex, ssv->payload + 7, 4);   
    memcpy(qq_hex, ssv->payload + 7, 4); 
    qq_num = (unsigned int *)qq_hex;                                                                       
                              
    qq = ntohl(*qq_num);       
    printf("1, ssv->ssn->qq = %d\n", ssv->ssn->qq);
    if(qq > 0 && 0 == ssv->ssn->qq)                                                                                             
    {                                                                                                     
        ssv->ssn->qq = qq;

        //insert qq 
        flck.shared_flag = APP_QQ;
        flck.shared_key = ssv->ssn->qq;
        flc = shared_user_cache_try_get(&flck);
        if(NULL == flc)
        {
            return;
        }
        if(0 == flc->flcv.shared_value)
        {
            flc->flcv.shared_value = ssv->ssn->sess_key.ip_src;
        }
        else if(flc->flcv.shared_value == ssv->ssn->sess_key.ip_src)
        {
            return;
        }
        else if(flc->flcv.shared_value != ssv->ssn->sess_key.ip_src)
        {
            flck.shared_flag = SHARED_IP;
            flck.shared_key = flc->flcv.shared_value;
            flc = shared_user_cache_try_get(&flck);
            if(flc)
            {
                flc->flcv.shared_value--;
            }
        }
        
        //insert shared num
        flck.shared_flag = SHARED_IP;
        flck.shared_key = ssv->ssn->sess_key.ip_src;
        flc = shared_user_cache_try_get(&flck);
        if(flc)
        {
            flc->flcv.shared_value++;
#if 1
            FILE *fp = NULL;                                                                                            
            fp = fopen("ip_qq.log", "a+");                                                                              
            if (fp != (FILE *) NULL)                                                                                    
            {                                                                                                           
                //statistics_qq(ssv);                                                                             
                if(flc)                                                                            
                {                                                                                               
                    fprintf(fp, "increase_statistics_qq [%u.%u.%u.%u] ->qq[%u]====shared_num[%u]\n", 
                        IPQUADS(ssv->ssn->sess_key.ip_src), 
                        ssv->ssn->qq,
                        flc->flcv.shared_value);      
                }                                                                                                                                                                                         
                fclose(fp);                                                                                             
            }
#endif
	}
    }                                                                                                     
                                                                              
    return;                                                                                                   
}     

void decrease_statistics_qq(struct sft_fdb_entry * ssn)                                                               
{                                                                                                                                                                                                                  
    struct shared_user_cache_key flck;
    struct shared_user_cache_value flcv;
    bzero(&flck, sizeof(struct shared_user_cache_key));
    bzero(&flcv, sizeof(struct shared_user_cache_value));
    struct shared_user_cache *flc = NULL;

    if(ssn->qq <= 0)                                                                                             
    {
        return;
    }

    //delete qq entry
    flck.shared_flag = APP_QQ;
    flck.shared_key = ssn->qq;
    if (0 != shared_user_lookup_behavior(&flck, &flcv) )
    {
#if 1
    FILE *fp = NULL;                                                                                            
    fp = fopen("ip_qq.log", "a+");                                                                              
    if (fp != (FILE *) NULL)                                                                                    
    {                                                                                                                                                                                                                                                                          
        fprintf(fp, "decrease_statistics_qq not find qq[%u]\n",  ssn->qq);                                                                                                                                                                                             
        fclose(fp);                                                                                             
    }
#endif
        return;
    }
    
    if(0 == shared_user_try_free_key(&flck))
    {
#if 0
    FILE *fp = NULL;                                                                                            
    fp = fopen("ip_qq.log", "a+");                                                                              
    if (fp != (FILE *) NULL)                                                                                    
    {                                                                                                                                                                                                                                                                          
        fprintf(fp, "decrease_statistics_qq delete qq[%u], ip[%u.%u.%u.%u] \n",  ssn->qq, IPQUADS(ssn->sess_key.ip_src));                                                                                                                                                                                             
        fclose(fp);                                                                                             
    }
#endif
    }

    //decrease shared user num
    bzero(&flck, sizeof(struct shared_user_cache_key));
    bzero(&flcv, sizeof(struct shared_user_cache_value));
    
    flck.shared_flag = SHARED_IP;
    flck.shared_key= ssn->sess_key.ip_src; 
    if (0 != shared_user_lookup_behavior(&flck, &flcv) )
    {
#if 1
    FILE *fp = NULL;                                                                                            
    fp = fopen("ip_qq.log", "a+");                                                                              
    if (fp != (FILE *) NULL)                                                                                    
    {                                                                                                                                                                                                                                                                          
        fprintf(fp, "decrease_statistics_qq not find ip[%u.%u.%u.%u]\n", IPQUADS(ssn->sess_key.ip_src));                                                                                                                                                                                             
        fclose(fp);                                                                                             
    }
#endif

        return;
    }
    
    flc = shared_user_cache_try_get(&flck);
    if(NULL == flc)
    {
#if 1
    FILE *fp = NULL;                                                                                            
    fp = fopen("ip_qq.log", "a+");                                                                              
    if (fp != (FILE *) NULL)                                                                                    
    {                                                                                                                                                                                                                                                                          
        fprintf(fp, "decrease_statistics_qq not get ip[%u.%u.%u.%u]\n", IPQUADS(ssn->sess_key.ip_src));                                                                                                                                                                                             
        fclose(fp);                                                                                             
    }
#endif

        return;
    }

    if(flc->flcv.shared_value > 0)
    {
        flc->flcv.shared_value--;
#if 1
        FILE *fp = NULL;                                                                                            
        fp = fopen("ip_qq.log", "a+");                                                                              
        if (fp != (FILE *) NULL)                                                                                    
        {                                                                                                                                                                                                                                                                          
            fprintf(fp, "decrease_statistics_qq successful ip[%u.%u.%u.%u], flc->flcv.shared_num[%u]\n", 
                    IPQUADS(ssn->sess_key.ip_src), 
                    flc->flcv.shared_value);                                                                                                                                                                                             
            fclose(fp);                                                                                             
        }
#endif
    }
    

    if(0 == flc->flcv.shared_value)
    {
        if(0 == shared_user_try_free_key(&flck))
        {
#if 1
            FILE *fp = NULL;                                                                                            
            fp = fopen("ip_qq.log", "a+");                                                                              
            if (fp != (FILE *) NULL)                                                                                    
            {                                                                                                                                                                                                                                                                          
                fprintf(fp, "decrease_statistics_qq delete ip[%u.%u.%u.%u] ,flc->flcv.shared_num[%u]\n", 
                                IPQUADS(ssn->sess_key.ip_src), flc->flcv.shared_value);                                                                                                                                                                                             
                fclose(fp);                                                                                             
            }
#endif
        }
    }
                                                        
    return;                                                                                                   
}
