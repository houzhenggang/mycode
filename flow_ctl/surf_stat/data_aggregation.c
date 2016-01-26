/*
 * data_aggregation
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
#include <dirent.h>
#include <sys/stat.h>
 
#include "data_aggregation.h" 
#include "hash/h_cache.h" 
#include "hash/jhash.h" 
#include "common.h"
	
static uint32_t g_hash_initval;


static uint32_t  data_aggregation_cache_hash_fn(void *key)
{
	struct  data_aggregation_cache_key *k = (struct  data_aggregation_cache_key *)key;                                                              
	
	return jhash_2words(k->appid_ip_key, k->hash_class, g_hash_initval);	
}

static int  data_aggregation_cache_comp_key_fn(struct h_cache *he, void *key)
{
	struct data_aggregation_cache *dc = container_of(he, struct data_aggregation_cache, h_cache);
	struct data_aggregation_cache_key *k = (struct data_aggregation_cache_key *)key;
	
	return (dc->flck.appid_ip_key ^k->appid_ip_key || dc->flck.hash_class ^k->hash_class);	
}

static void data_aggregation_cache_release_fn(struct h_cache *he)
{
	struct data_aggregation_cache *dc = container_of(he, struct data_aggregation_cache, h_cache);

	free(dc);
}

static char *data_aggregation_cache_build_line_fn(struct h_cache *he)
{
    return NULL;
}

void data_aggregation_cache_clear(void)
{
	h_table_clear(&data_aggregation_cache_tbl.h_table);
}

static struct h_operations data_aggregation_cache_ops =
{
	.hash        = data_aggregation_cache_hash_fn,
	.comp_key    = data_aggregation_cache_comp_key_fn,
	.release     = data_aggregation_cache_release_fn,
	.build_line  = data_aggregation_cache_build_line_fn,
};


static struct h_cache *data_aggregation_cache_create_fn(struct h_table *ht, void *key)
{
	struct data_aggregation_cache_key *k = (struct data_aggregation_cache_key *)key;
	struct data_aggregation_cache *dc = (struct data_aggregation_cache *)malloc(sizeof(struct data_aggregation_cache));
       if(NULL == dc)
       {
		return NULL;
       }

	dc->flck = *k;

	return &dc->h_cache;
}

//struct fixed_length_cache *fixed_length_cache_try_get(const __u16 port, unsigned int proto_mark)
struct data_aggregation_cache *data_aggregation_cache_try_get(struct data_aggregation_cache_key *k)
{
	struct h_cache *he;
	struct data_aggregation_cache *dc;

	if((h_cache_try_get(&data_aggregation_cache_tbl.h_table, k, &he,data_aggregation_cache_create_fn, NULL)) == -1)
		return NULL;

	dc = container_of(he, struct data_aggregation_cache, h_cache);

	return dc;
}

static void __data_aggregation_cache_lookup_action(struct h_cache *he, void *key, void *result)
{
	struct data_aggregation_cache *dc = container_of(he, struct data_aggregation_cache, h_cache);

	//he->timeo = rte_rdtsc();

	*( struct data_aggregation_cache_value *) result = dc->flcv;
}

int data_aggregation_lookup_behavior(struct data_aggregation_cache_key *k, struct data_aggregation_cache_value *v)
{
	/* Loopup precise table. */
	if(h_cache_try_lookup(&data_aggregation_cache_tbl.h_table, k, v, __data_aggregation_cache_lookup_action) == 0)
		return 0;
	
	return -1;
}


int data_aggregation_cache_init(void)
{
	int retv = 0;

	srand(time(NULL));
	g_hash_initval = random();
	marbit_send_log(INFO,"---------------------data_aggregation_cache_init------------------------------\n");	

	if(h_table_create(&data_aggregation_cache_tbl.h_table, 
                                    NULL, 
                                    DATA_AGGREGATION_HASH_SIZE, 
                                    DATA_AGGREGATION_MAX_LEN, 
                                    DATA_AGGREGATION_TIMEOUT, 
                                    &data_aggregation_cache_ops) != 0)
	{
		retv = -1;
	}

	return retv;
}

void data_aggregation_cache_exit(void)
{
	h_table_release(&data_aggregation_cache_tbl.h_table);
}
void destroy_aggregation_info()
{
    if(-1 == h_table_release(&data_aggregation_cache_tbl.h_table))
    {
        marbit_send_log(ERROR, "failed to release hash table!\n");
    }
      
    return;
}




extern int32_t g_data_agg_class_condition_appid;
extern uint32_t g_aggregation_line;
extern merge_sorted_node_t* merglist;
void process_data_aggregation_list(struct msg_st *mesg,
                                                             data_aggregation_class data_agg_class,
                                                              sorted_list_condition sorted_list_index,
                                                              data_sort_condition data_sort_condition)
{

    if(g_data_agg_class_condition_appid != -1)
    {
        if(g_data_agg_class_condition_appid != mesg->proto_mark)
        {
            return;
        }
    }
    struct list_head *head = &sorted_list_arry[sorted_list_index].list;

    //flck
    struct data_aggregation_cache_key flck;
    bzero(&flck, sizeof(struct data_aggregation_cache_key));
    
    if(DATA_AGG_CLASS_APP == data_agg_class)
    {
        flck.appid_ip_key = mesg->proto_mark;
    }
    else if(DATA_AGG_CLASS_IP_INNER == data_agg_class)
    {
        flck.appid_ip_key = mesg->ip_src;
    }
    else if(DATA_AGG_CLASS_IP_OUTER == data_agg_class)
    {
        flck.appid_ip_key = mesg->ip_dst;
    }
    else
    {
        marbit_send_log(ERROR,"data_agg_class is not valid!\n");
        return;
    }
    flck.hash_class = data_agg_class;

    //flcv
    struct data_aggregation_cache_value flcv;
    bzero(&flcv, sizeof(struct data_aggregation_cache_value));


    //try to find in hash table, if exist, then update
    if (0 == data_aggregation_lookup_behavior(&flck, &flcv) )
    {    
#if 0
        /*  for quick sort   */
        //udpate sorted_list
        if(DATA_AGG_CLASS_APP == data_agg_class)
        {
            flcv.sorted_node->data.con_nums ++;
            flcv.sorted_node->data.up_bps += (mesg->up_bytes * 8)/(mesg->duration_time);
            flcv.sorted_node->data.down_bps += (mesg->down_bytes * 8)/(mesg->duration_time);
            flcv.sorted_node->data.total_bits += (mesg->up_bytes * 8 + mesg->down_bytes * 8);
         }
         else if(DATA_AGG_CLASS_IP_INNER == data_agg_class ||DATA_AGG_CLASS_IP_OUTER == data_agg_class)
         {
            flcv.sorted_node->data.ip_con_nums ++;
            flcv.sorted_node->data.ip_up_size += mesg->up_bytes * 8;
            flcv.sorted_node->data.ip_down_size += mesg->down_bytes * 8;
            flcv.sorted_node->data.ip_total_size += (mesg->up_bytes * 8 + mesg->down_bytes * 8);
         }
#endif
        //udpate sorted_list
        if(DATA_AGG_CLASS_APP == data_agg_class)
        {
            flcv.merge_sorted_node->data.con_nums ++;
            flcv.merge_sorted_node->data.up_bps += (mesg->up_bytes * 8)/(mesg->duration_time);
            flcv.merge_sorted_node->data.down_bps += (mesg->down_bytes * 8)/(mesg->duration_time);
            flcv.merge_sorted_node->data.total_bits += (mesg->up_bytes * 8 + mesg->down_bytes * 8);
         }
         else if(DATA_AGG_CLASS_IP_INNER == data_agg_class ||DATA_AGG_CLASS_IP_OUTER == data_agg_class)
         {
            flcv.merge_sorted_node->data.ip_con_nums ++;
            flcv.merge_sorted_node->data.ip_up_size += (mesg->up_bytes * 8)/(mesg->duration_time);
            flcv.merge_sorted_node->data.ip_down_size += (mesg->down_bytes * 8)/(mesg->duration_time);
            flcv.merge_sorted_node->data.ip_total_size += (mesg->up_bytes * 8 + mesg->down_bytes * 8);
         }
    }
    
    //if not exist in hash table, then create entry in hash table
    else
    {
        struct data_aggregation_cache *flc = NULL;
        flc = data_aggregation_cache_try_get(&flck);
        if(NULL == flc)
        {     
            return;
        }

#if 0
        /*  for quick sort   */
        
        sorted_node_t *tmp = NULL;
        tmp = (sorted_node_t *)malloc(sizeof(sorted_node_t));
        if(NULL == tmp)
        {
            return;
        }
        
        bzero(tmp, sizeof(sorted_node_t));
        if(DATA_AGG_CLASS_APP == data_agg_class)
        {
            tmp->data.appid = mesg->proto_mark;
            tmp->data.con_nums = 1; 
            tmp->data.up_bps = (mesg->up_bytes * 8)/(mesg->duration_time);
            tmp->data.down_bps = (mesg->down_bytes * 8)/(mesg->duration_time);
            tmp->data.total_bits = (mesg->up_bytes * 8 + mesg->down_bytes * 8);
        }
        else if(DATA_AGG_CLASS_IP_INNER == data_agg_class ||DATA_AGG_CLASS_IP_OUTER == data_agg_class)
        {
            if(DATA_AGG_CLASS_IP_INNER == data_agg_class)
            {
                tmp->data.ip = mesg->ip_src;
            }
            else //if(DATA_AGG_CLASS_IP_OUTER == data_agg_class)
            {
                tmp->data.ip = mesg->ip_dst;
            }
            
            tmp->data.ip_con_nums = 1; 
            tmp->data.ip_up_size = mesg->up_bytes * 8;
            tmp->data.ip_down_size = mesg->down_bytes * 8;
            tmp->data.ip_total_size = mesg->up_bytes * 8 + mesg->down_bytes * 8;
        }

        flc->flcv.sorted_node = tmp;

        list_add(&(tmp->list), head);
#endif

        merge_sorted_node_t *tmp1 = NULL;
        tmp1 = (merge_sorted_node_t *)malloc(sizeof(merge_sorted_node_t));
        if(NULL == tmp1)
        {
            return;
        }
        
        bzero(tmp1, sizeof(merge_sorted_node_t));
        if(DATA_AGG_CLASS_APP == data_agg_class)
        {
            tmp1->data.appid = mesg->proto_mark;
            tmp1->data.con_nums = 1; 
            tmp1->data.up_bps = (mesg->up_bytes * 8)/(mesg->duration_time);
            tmp1->data.down_bps = (mesg->down_bytes * 8)/(mesg->duration_time);
            tmp1->data.total_bits = (mesg->up_bytes * 8 + mesg->down_bytes * 8);
        }
        else if(DATA_AGG_CLASS_IP_INNER == data_agg_class ||DATA_AGG_CLASS_IP_OUTER == data_agg_class)
        {
            if(DATA_AGG_CLASS_IP_INNER == data_agg_class)
            {
                tmp1->data.ip = mesg->ip_src;
            }
            else //if(DATA_AGG_CLASS_IP_OUTER == data_agg_class)
            {
                tmp1->data.ip = mesg->ip_dst;
            }
            
            tmp1->data.ip_con_nums = 1; 
            tmp1->data.ip_up_size = (mesg->up_bytes * 8)/(mesg->duration_time);
            tmp1->data.ip_down_size = (mesg->down_bytes * 8)/(mesg->duration_time);
            tmp1->data.ip_total_size = mesg->up_bytes * 8 + mesg->down_bytes * 8;
        }

        flc->flcv.merge_sorted_node = tmp1;

        if(NULL != merglist)
        {
            tmp1->next = merglist->next;
            merglist->next = tmp1;
        }
        else
        {
            merglist = tmp1;
            merglist->next = NULL;
        }

        g_aggregation_line++;
    }
    
    return;
}


void do_data_aggregation_from_db(const char* dbname)
{
    DIR              *pDir ;  
    struct dirent    *ent  ;  
    char              filename[MAX_FILE_PATH_LEN];  
    
    
    pDir=opendir(dbname); 
    if(!pDir)
    {
        marbit_send_log(ERROR,"config dir is not exist, dir:%s\n", dbname);
        return;
    }
    memset(filename, 0, sizeof(filename)); 
    
    while((ent=readdir(pDir))!=NULL)      
    {  
        memset(filename, 0, sizeof(filename)); 
        
        if(ent->d_type & DT_DIR)  
        {  
            //if(strcmp(ent->d_name,".")==0 || strcmp(ent->d_name,"..")==0)  
               // continue;  
            continue;
        }  
        else
        {
            sprintf(filename,"%s/%s",dbname,ent->d_name);  

            //do data aggregation
            do_data_aggregation_from_file(filename);
        }
    }  
    
    closedir(pDir);

    return;
}

