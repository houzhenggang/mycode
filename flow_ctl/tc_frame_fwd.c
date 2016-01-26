#include <assert.h>

#include <rte_errno.h>
#include <rte_cycles.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_jhash.h>
#include <rte_malloc.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_ethdev.h>

#include "tc_frame_fwd.h"
#include "tc_config.h"
#include "session_mgr.h"
#include "utils.h"
#include "global.h"
#include "tc_queue.h"
#define HZ 1000

extern struct token_hash_node token_flow_ip_hash[TOKEN_FLOW_HASH_ITEM];
extern struct rte_mempool *datapipe_fifo_mem_cache;
extern struct rte_mempool *datapipe_data_mem_cache;
extern struct rte_mempool *fip_fifo_mem_cache;
extern struct rte_mempool *fip_data_mem_cache;
extern struct rte_mempool *flow_ip_mem_cache;

extern  rte_atomic64_t fip_num;
extern  rte_atomic64_t ftoken_num;

unsigned int KSFT_FLOW_HASH_ITEM;
unsigned int ksft_fip_hashsize;
struct ksft_hash_node *ksft_flow_ip_hash;
rte_atomic64_t nums_sft_fip_entry = RTE_ATOMIC64_INIT(0);

rte_atomic64_t free_pkg_num = RTE_ATOMIC64_INIT(0);

struct tc_private_t *priv = NULL;

void sft_queue_xmit(struct rte_mbuf* m)
{
    uint8_t out = get_dport_by_sport(m->in_port);
    if(out == (uint8_t)-1)
    {
        rte_pktmbuf_free(m);
        rte_atomic64_inc(&free_pkg_num);
    }
    else
    {
        send_single_packet(m, out);
    } 
}

static inline int is_inner_dev_by_port(int portid)
{
    //TO DO
    return isinner_by_portid(portid);
}

static inline int flow_ip_hash(unsigned int inner_ip,
        unsigned int l4proto,int dir,uint32_t app_proto,int index, uint32_t KSFT_FLOW_HASH_ITEM)
{
    return rte_jhash_3words(inner_ip,app_proto,dir|(l4proto<<8)|((index&0xFFFF)<<16), 0) & (KSFT_FLOW_HASH_ITEM - 1);
}

static inline struct sft_flow_ip_entry *flow_ip_find(struct cds_hlist_head *head,
        unsigned int inner_ip,
        unsigned int l4proto,int dir,uint32_t app_proto,int index)
{
    struct sft_flow_ip_entry *fdb;
    struct cds_hlist_node   *pos;
    cds_hlist_for_each_entry_rcu(fdb, pos, head, hlist){
        if(fdb->inner_ip == inner_ip && fdb->l4proto == l4proto && fdb->dir == dir && fdb->app_proto == app_proto && fdb->index == index) 
        {
            return fdb;
        }
    }
    return NULL;
}

static void sft_flow_ip_free(struct rte_timer *tim, void *data)
{
    struct sft_flow_ip_entry *fdb;
    struct ksft_hash_node * hash_node;
    struct cds_hlist_head *head;
    uint64_t cur_tsc = 0, diff_tsc = 0;
    uint64_t hz1min = rte_get_timer_hz() * 60;  //1min
    int hash_idx;
    
    if(data == 0)
        return;
    fdb = (struct sft_flow_ip_entry *)data;

    hash_idx = flow_ip_hash(fdb->inner_ip,fdb->l4proto,
                                                fdb->dir,fdb->app_proto,fdb->index, KSFT_FLOW_HASH_ITEM);
    rcu_read_lock();
    head = &ksft_flow_ip_hash[hash_idx].hhead;

    cur_tsc = rte_rdtsc() / TIMER_RESOLUTION_MS;
    diff_tsc = cur_tsc - fdb->timestamp;
    if(diff_tsc > 2 * 60 * 1000)
    {
        // free 2 min
        hash_node = container_of(head,struct ksft_hash_node,hhead);
        RTE_DBG("%s(%d),delete fdb inner ip:%pI4,l4proto:%d.\n",\
                __FUNCTION__,__LINE__,&fdb->inner_ip,fdb->l4proto);
        rte_spinlock_lock(&hash_node->lock);

        flow_ip_delete(fdb->priv, fdb);

        rte_spinlock_unlock(&hash_node->lock);
        rcu_read_unlock();
        return;
    }
    //mod_timer(&fdb->timeout_kill, jiffies + HZ_PER_MIN*3);
    
    rte_timer_reset(tim, 3 *  hz1min, 
                    SINGLE, rte_lcore_id(), sft_flow_ip_free, (void *)fdb);
    rcu_read_unlock();
    return;
}

static inline struct sft_flow_ip_entry *flow_ip_create(struct tc_private_t *priv, struct cds_hlist_head *head,
        unsigned int inner_ip,
        unsigned int l4proto,int ip_rate,int dir,uint32_t app_proto,int index)
{
    struct sft_flow_ip_entry *fdb;
    
    if(!priv || !flow_ip_mem_cache)
    {
        return NULL;
    }

    if(rte_atomic64_read(&nums_sft_fip_entry) >= (KSFT_FLOW_HASH_ITEM << 1))
    {
        RTE_DBG("%s: max fip entry limit %d!\n",__FUNCTION__, KSFT_FLOW_HASH_ITEM);
        return NULL;
    }

    //fdb = (struct sft_flow_ip_entry *)rte_zmalloc(NULL, sizeof(struct sft_flow_ip_entry), 0);
    if(rte_mempool_mc_get(flow_ip_mem_cache, (void **)&fdb) <0)
		return NULL;
    if(fdb)
    {
        fdb->pktQ = tc_queue_init();
        if(!fdb->pktQ)
        {
            //rte_free(fdb);
            rte_mempool_mp_put(flow_ip_mem_cache, fdb);
            RTE_ERR("%s: kfifo init failed\n", __FUNCTION__);
            return NULL;
        }
        
        struct ksft_hash_node * hash_node = container_of(head,struct ksft_hash_node,hhead);
        rte_atomic64_inc(&fip_num);
        rte_atomic64_inc(&nums_sft_fip_entry);
        fdb->index = index;
        fdb->inner_ip = inner_ip;
        fdb->l4proto = l4proto;
        fdb->ip_rate = ip_rate;
        fdb->dir = dir;
        fdb->app_proto = app_proto;
        fdb->priv = priv;
        //setup_timer(&fdb->timeout_send, sft_tc_send_oneip_handler,(unsigned long)fdb);
        //setup_timer(&fdb->timeout_kill, sft_flow_ip_free,(unsigned long)fdb);
        rte_timer_init(&fdb->timeout_send);
        rte_timer_init(&fdb->timeout_kill);
        
        //atomic_set(&fdb->token, 0);
        rte_atomic32_init(&fdb->token_entry.token);
        //rte_spinlock_init(&fdb->token_entry.lock);
        //fdb->token_entry.token = 0;

        rte_spinlock_lock(&hash_node->lock);
        cds_hlist_add_head_rcu(&fdb->hlist, head);
        rte_spinlock_unlock(&hash_node->lock);

//        start oneip timer
        //mod_timer(&fdb->timeout_send, jiffies + 1); 
        //mod_timer(&fdb->timeout_kill, jiffies + 3*HZ_PER_MIN); 
        
        uint64_t hz1ms = rte_get_timer_hz() / 1000; //1ms
        uint64_t hz1min = rte_get_timer_hz() * 60;  //1min
        rte_timer_reset(&fdb->timeout_send, hz1ms, 
                                    SINGLE, rte_lcore_id(), sft_tc_send_oneip_handler, (void *)fdb);
        //rte_timer_reset(&fdb->timeout_kill, 3 *  hz1min, 
        //                            SINGLE, rte_lcore_id(), sft_flow_ip_free, (void *)fdb);
        RTE_DBG("%s(%d),%pI4,l4proto:%d,ip_rate:%d,dir:%d.\n",__FUNCTION__,__LINE__,&fdb->inner_ip,fdb->l4proto,fdb->ip_rate,fdb->dir);
    }
    return fdb;
}

//1:consume skb
//0:I queue it
static inline int sft_pkt_enqueue_pipe(struct tc_private_t *priv, struct rte_mbuf *skb,int pipe_idx)
{
    struct sft_queue *Q;
    Q = priv->ksft_datapipe[pipe_idx].pktQ;
    if(Q && tc_enqueue(Q, (void *)&skb, sizeof(skb)) > 0)
    {
        //pkg_num_inc();
        return 0;
    }
    else
    {
        rte_pktmbuf_free(skb);
        rte_atomic64_inc(&free_pkg_num);
        RTE_DBG("%s(%d),queue full ,free skb.\n",__FUNCTION__,__LINE__);

        return    1;
    }

}

//1:consume skb
//0:I queue it
static inline int sft_pkt_enqueue(struct rte_mbuf *skb,struct sft_flow_ip_entry *fdb)
{
    struct sft_queue *Q;
    Q = fdb->pktQ;
    if(Q && tc_enqueue(Q, (void *)&skb, sizeof(skb)) > 0)
    {
        //pkg_num_inc();
        return 0;
    }
    else 
    {
        rte_pktmbuf_free(skb);
        rte_atomic64_inc(&free_pkg_num);
        RTE_DBG("%s(%d),queue full ,free skb.\n",__FUNCTION__,__LINE__);

        return    1;
    }
}

static inline int check_port_range(struct sft_flow_rule *rule,uint16_t port,int flag)//ip host order
{
	if(flag == 0)//inner
	{
		if((port >= rule->inner_port_start) && (port <= rule->inner_port_end))
			return 1;
	}
	else
	{
		if((port >= rule->outer_port_start) && (port <= rule->outer_port_end))
			return 1;
	}
	return 0;
}

static inline int check_ip_range_ipg(struct sft_ipgroup_entry *fdb,uint32_t ip)//ip host order
{
	if((ip >= fdb->ip_start) && (ip <= fdb->ip_end))
		return 1;
	return 0;
}

static inline int check_ip_mask_ipg(struct sft_ipgroup_entry *fdb,uint8_t *ip)
{
	if((((*(ip+0)) & fdb->mask[0]) == fdb->network[0]) && \
			(((*(ip+1)) & fdb->mask[1]) == fdb->network[1]) && \
			(((*(ip+2)) & fdb->mask[2]) == fdb->network[2]) && \
			(((*(ip+3)) & fdb->mask[3]) == fdb->network[3]) )
	{   
		return 1;
	}

	return 0;
}

static inline int check_ip_mask(struct sft_flow_rule *rule,uint8_t *ip,int flag)
{
	if(flag == 0)//inner
	{
		if((((*(ip+0)) & rule->inner_mask[0]) == rule->inner_network[0]) && \
				(((*(ip+1)) & rule->inner_mask[1]) == rule->inner_network[1]) && \
				(((*(ip+2)) & rule->inner_mask[2]) == rule->inner_network[2]) && \
				(((*(ip+3)) & rule->inner_mask[3]) == rule->inner_network[3]) )
		{   
			return 1;
		}
	}
	else
	{
		if((((*(ip+0)) & rule->outer_mask[0]) == rule->outer_network[0]) && \
				(((*(ip+1)) & rule->outer_mask[1]) == rule->outer_network[1]) && \
				(((*(ip+2)) & rule->outer_mask[2]) == rule->outer_network[2]) && \
				(((*(ip+3)) & rule->outer_mask[3]) == rule->outer_network[3]) )
		{   
			return 1;
		}
	}

	return 0;
}

static inline int check_ip_range(struct sft_flow_rule *rule,uint32_t ip,int flag)//ip host order
{
	if(flag == 0)//inner
	{
		if((ip >= rule->inner_ip_start) && (ip <= rule->inner_ip_end))
			return 1;
	}
	else
	{
		if((ip >= rule->outer_ip_start) && (ip <= rule->outer_ip_end))
			return 1;
	}
	return 0;

}

static inline int check_ipgroup(struct tc_private_t *priv, struct sft_flow_rule *rule,uint32_t ip,int flag)//network order
{
	struct sft_ipgroup_entry *fdb;
	struct hlist_head *head;
	unsigned short idx;
    
    if(!priv)
    {
        return 0;
    }

	if(flag == 0)
	{//inner
		idx = rule->ipg_inner_idx;
	}
	else
	{//outer
		idx = rule->ipg_outer_idx;
	}

	if(priv->sft_ipgroup_list[idx].enable == 0)
		return 0;

	head = &priv->sft_ipgroup_list[idx].hhead;
	if(head->first == NULL)
		return 0;

	hlist_for_each_entry(fdb, head, hlist){
		switch(fdb->ip_type)
		{
			case IPT_ONE_IP:
				if(fdb->ip == ip)
				{
					return 1;
				}
				break;
			case IPT_NETWORK_MASK_IP:
				if(check_ip_mask_ipg(fdb,(uint8_t *)&ip) == 1)
				{
					return 1;
				}
				break;
			case IPT_START_END_IP:
				if(check_ip_range_ipg(fdb,ntohl(ip)) == 1)
				{
					return 1;
				}
				break;
			default:
				break;
		}
	}
	return 0;
}

#if 0
static inline struct token_flow_ip *token_flow_ip_find_and_new(int hash_idx,
                                    unsigned int inner_ip,unsigned int l4proto,int dir,uint32_t app_proto,int index)
{
    struct token_flow_ip *fdb;
    if(!token_flow_mem_cache || (unsigned int)hash_idx >= TOKEN_FLOW_HASH_ITEM)
    {
        return NULL;
    }
    
    rte_spinlock_lock(&token_flow_ip_hash[hash_idx].lock);
    struct hlist_head *head = &token_flow_ip_hash[hash_idx].hhead;
    hlist_for_each_entry(fdb, head, hlist){
        if(fdb->inner_ip == inner_ip && fdb->l4proto == l4proto && fdb->dir == dir && fdb->app_proto == app_proto && fdb->index == index) 
        {
            rte_spinlock_unlock(&token_flow_ip_hash[hash_idx].lock);
            rte_atomic16_inc(&fdb->dataref);
            return fdb;
        }
    }
    rte_spinlock_unlock(&token_flow_ip_hash[hash_idx].lock);
    
    //fdb = (struct token_flow_ip *)rte_zmalloc(NULL, sizeof(struct token_flow_ip), 0);
    if(rte_mempool_sc_get(token_flow_mem_cache, (void **)&fdb) <0)
		return NULL;
    if(fdb)
    {
        fdb->inner_ip = inner_ip;
        fdb->l4proto = l4proto;
        fdb->dir = dir;
        fdb->app_proto = app_proto;
        fdb->index = index;
        rte_atomic16_init(&fdb->dataref);
        rte_atomic32_init(&fdb->token_entry.token);
        rte_atomic64_init(&fdb->token_entry.up_time);
        
        rte_spinlock_lock(&token_flow_ip_hash[hash_idx].lock);
        hlist_add_head(&fdb->hlist, head);
        rte_spinlock_unlock(&token_flow_ip_hash[hash_idx].lock);
        
        rte_atomic16_inc(&fdb->dataref);
        
        rte_atomic64_inc(&ftoken_num);
        return fdb;
    }
    
    return NULL;
}

inline void token_flow_ip_free(struct sft_flow_ip_entry *token, int hash_idx)
{
    struct token_flow_ip *fdb;
    struct hlist_head *head = &token_flow_ip_hash[hash_idx].hhead;
    if(!token)
        return;
    
    rte_spinlock_lock(&token_flow_ip_hash[hash_idx].lock);
    hlist_for_each_entry(fdb, head, hlist){
    if(fdb->inner_ip == token->inner_ip && fdb->l4proto == token->l4proto && fdb->dir == token->dir && 
        fdb->app_proto == token->app_proto && fdb->index == token->index) 
        {
            if(rte_atomic16_dec_and_test(&fdb->dataref))
            {
                hlist_del(&fdb->hlist);
                //rte_free(fdb);
                if(token_flow_mem_cache)
                {
                    rte_mempool_sp_put(token_flow_mem_cache, fdb);
                }
                rte_atomic64_dec(&ftoken_num);
            }
            rte_spinlock_unlock(&token_flow_ip_hash[hash_idx].lock);
            return;
        }
    }
    rte_spinlock_unlock(&token_flow_ip_hash[hash_idx].lock);
}
#endif

/*
 *1:go orig path
 *0:I consume this skb
 *maybe in process context,already local_bh_disable
 * */
static int do_send_work(struct tc_private_t *priv, struct rte_mbuf *skb)
{  
    int mr_num;
    unsigned short *rule_idx_tab;
    int i,idx;
    int skb_dir = 0;
    struct sft_fdb_entry *l2fdb = NULL;  
    struct sft_flow_rule *rule = NULL;
    struct cds_hlist_head *head = NULL; 
    uint32_t inner_ip=0,outer_ip=0;//network order
    struct ipv4_hdr *iph; 
    struct sft_flow_ip_entry *flow_ip_fdb; 

    if(!priv || !skb)
    {
        RTE_ERR("%s(%d), param is error, priv:%p, skb:%p\n", __FUNCTION__, __LINE__, priv, skb);
        return 1;
    }
    //sft_queue_xmit(skb);
    /* Handle IPv4 headers.*/
    struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(skb, struct ether_hdr *);
    uint16_t ether_type = eth_hdr->ether_type;
    size_t vlan_offset = get_vlan_offset(eth_hdr, &ether_type);
    iph = (struct ipv4_hdr *)((char *)(eth_hdr + 1) + vlan_offset);
    if(!iph) return 1;

    if(!is_inner_dev_by_port(skb->in_port))//send to inner device ,download
    {
        skb_dir = DOWN_DIR;
        inner_ip = iph->dst_addr;
        outer_ip = iph->src_addr;
    }
    else // send to outer device,upload
    {
        skb_dir = UP_DIR;
        inner_ip = iph->src_addr;
        outer_ip = iph->dst_addr;
    }
   
    l2fdb = skb->l2ct;
    if(!l2fdb) return 1;

    if(skb_dir == UP_DIR)
    {
        mr_num = l2fdb->up_mr_num;
        rule_idx_tab = l2fdb->up_mr_idx;
    }
    else
    {
        mr_num = l2fdb->down_mr_num;
        rule_idx_tab = l2fdb->down_mr_idx;
    }

    for(i = skb->mr_tab_idx ;i < mr_num;i++)
    {
        idx = rule_idx_tab[i];
        rule = priv->rule_active[idx];
        if(!rule || !rule->enable)
        {//do error process
            sft_queue_xmit(skb);
            return 0;
        }
        rule_flow_statics_rx_add(rule->flow_statics, rte_pktmbuf_data_len(skb));
        
        if(rule->action_type == ONE_IP_LIMIT)
        {
            int hash_idx = flow_ip_hash(inner_ip,rule->l4proto,rule->dir,
                                                 rule->app_proto,rule->index, 
                                                 KSFT_FLOW_HASH_ITEM);
            head = &ksft_flow_ip_hash[hash_idx].hhead;
            flow_ip_fdb = flow_ip_find(head,inner_ip,rule->l4proto,rule->dir,rule->app_proto,rule->index);
            if(!flow_ip_fdb)
            {
                flow_ip_fdb = flow_ip_create(priv,head,inner_ip,rule->l4proto,rule->ip_rate,rule->dir,rule->app_proto,rule->index);
                if(!flow_ip_fdb)
                {
                    RTE_DBG("%s: fail to create flow_obj_entry.\n",__FUNCTION__);
                    sft_queue_xmit(skb);
                    return 0;
                }
            }
            
            flow_ip_fdb->timestamp = rte_rdtsc() / TIMER_RESOLUTION_MS;
            flow_ip_fdb->ip_rate = rule->ip_rate;

            //rte_spinlock_lock(&flow_ip_fdb->token_entry.lock);
			if(rte_atomic32_read(&flow_ip_fdb->token_entry.token) < ((int)rte_pktmbuf_data_len(skb)))   
			//if(flow_ip_fdb->token_entry.token < ((int)rte_pktmbuf_data_len(skb)))   
			{
				//rte_spinlock_unlock(&flow_ip_fdb->token_entry.lock);
				//queue this flow_ip_fdb
				skb->mr_tab_idx = i;
//				if(printk_ratelimit())
//					printk("%s(%d),queue to one ip limit,rule %d.\n",__FUNCTION__,__LINE__,rule->index);
				sft_pkt_enqueue(skb,flow_ip_fdb);
                
				return 0;
			}
			else
			{
				rte_atomic32_sub(&flow_ip_fdb->token_entry.token, (int)rte_pktmbuf_data_len(skb));
				//flow_ip_fdb->token_entry.token -= (int)rte_pktmbuf_data_len(skb);
				//rte_spinlock_unlock(&flow_ip_fdb->token_entry.lock);
		//		{
		//		struct sft_flow_ip_entry *fdb = flow_ip_fdb; 
		//		if(printk_ratelimit())
		//		{
		//			printk("%s(%d),%pI4,l4proto:%d,ip_rate:%d,dir:%d,token:%d,skb->len:%d,data_len:%d.\n",
		//					__FUNCTION__,__LINE__,&fdb->inner_ip,fdb->l4proto,fdb->ip_rate,fdb->dir,
		//					fdb->token_entry.token,(int)skb->len,(int)skb->data_len);
		//		}
		//		}
                rule_flow_statics_cache_add(rule->flow_statics, rte_pktmbuf_data_len(skb));
                
				continue;
			}
        }
        else if(rule->action_type == DATAPIPE)
        {

            struct sft_flow_datapipe_entry *ppfdb = &priv->ksft_datapipe[rule->datapipe_index];

            //rte_spinlock_lock(&ppfdb->token_entry.lock);
            //if(atomic_read(&ppfdb->token) < ((int)skb->len))
            if(rte_atomic32_read(&ppfdb->token_entry.token) < (int)rte_pktmbuf_data_len(skb))
            {
                //rte_spinlock_unlock(&ppfdb->token_entry.lock);
                //queue this flow_ip_fdb
                skb->mr_tab_idx = i;
                RTE_DBG("%s(%d),queue to datapipe,rule %d.\n",__FUNCTION__,__LINE__,rule->index);
                sft_pkt_enqueue_pipe(priv, skb,rule->datapipe_index);

                return 0;
            }
            else
            {
                RTE_DBG("%s(%d),data pipe,rule %d, token %u, pkg %u.\n",__FUNCTION__,__LINE__,rule->index, 
                            rte_atomic32_read(&ppfdb->token_entry.token), rte_pktmbuf_data_len(skb));
                //atomic_sub((int)skb->len ,&ppfdb->token);
                //ppfdb->token_entry.token -= (int)rte_pktmbuf_data_len(skb);
                rte_atomic32_sub(&ppfdb->token_entry.token, (int)rte_pktmbuf_data_len(skb));
                //rte_spinlock_unlock(&ppfdb->token_entry.lock);
                rule_flow_statics_cache_add(rule->flow_statics, rte_pktmbuf_data_len(skb));
                continue;
            }
        }
        else
        {//never come here
            sft_queue_xmit(skb);
            return 0;
        }
    }

    sft_queue_xmit(skb);
  
    return 0;
}

/*
 *1:go orig path
 *0:I consume this skb
 * */
static int iterate_flow_rule(struct tc_private_t *priv, struct rte_mbuf *skb)
{
    int i;
//    int ret = 0;
    uint32_t inner_ip=0,outer_ip=0;//network order
    uint16_t inner_port=0,outer_port=0;//network order
    uint16_t sport = 0, dport = 0;
    uint32_t sip = 0, dip = 0;
    struct ipv4_hdr *iph; 
    struct tcp_hdr *tcp;
    struct udp_hdr *udp;
    int idx;
    struct sft_flow_rule *rule = NULL;
    struct sft_fdb_entry *l2fdb = NULL;  
    int break_flag = 0;
    int match_i = 0;
    int skb_dir = 0;
    int flag_ = 0;
    int matched_idx[MRULES_MAX+1];
    uint8_t num;

    if(!priv)
    {
        return 1;
    }
    
    memset(matched_idx,0,sizeof(matched_idx));

    if(priv->rule_update_timestamp == 0) return 1;

    struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(skb, struct ether_hdr *);
    uint16_t ether_type = eth_hdr->ether_type;
    size_t vlan_offset = get_vlan_offset(eth_hdr, &ether_type);
    iph = (struct ipv4_hdr *)((char *)(eth_hdr + 1) + vlan_offset);
    if(!iph) return 1;
    
    dip = iph->dst_addr;
    sip = iph->src_addr;

    //only tcp or udp
    switch (iph->next_proto_id) 
    {
        case IPPROTO_TCP:
            tcp = (struct tcp_hdr *)((unsigned char *)iph +
                    sizeof(struct ipv4_hdr));
            dport = tcp->dst_port;
            sport = tcp->src_port;
            break;

        case IPPROTO_UDP:
            udp = (struct udp_hdr *)((unsigned char *)iph +
                    sizeof(struct ipv4_hdr));
            dport = udp->dst_port;
            sport = udp->src_port;
            break;

        default:
            return 1;
    }
    
#if 0
    //test
    //printf("===IP:[%u--%u]\n", sip, dip);
    if(sip != 2147592384 && dip != 2147592384)
    {
        return 1;
    }
#endif   
 
    if(!is_inner_dev_by_port(skb->in_port))//send to inner device ,download
    {
        skb_dir = DOWN_DIR;
        inner_ip = dip;
        outer_ip = sip;
        inner_port = dport;
        outer_port = sport;
    }
    else // send to outer device,upload
    {
        skb_dir = UP_DIR;
        inner_ip = sip;
        outer_ip = dip;
        inner_port = sport;
        outer_port = dport;
    }
//printk("%s(%d),%pI4:%d---%pI4:%d,skb_dir:%d,skb:%p.\n",
//                    __FUNCTION__,__LINE__,&inner_ip,ntohs(inner_port),&outer_ip,ntohs(outer_port),skb_dir,skb);
    //RTE_DBG("%s(%d),%u.%u.%u.%u:%d---%u.%u.%u.%u:%d,skb_dir:%d,l2ct:0x%p.\n", __FUNCTION__,__LINE__,RTE_NIPQUAD(inner_ip),inner_port,RTE_NIPQUAD(outer_ip),outer_port,skb_dir,skb->l2ct);

    l2fdb = skb->l2ct;

    if(!l2fdb || l2fdb->magic != 'X')
    {
        sft_queue_xmit(skb);
        return 0;
    }

    if(l2fdb->proto_mark_state == 1)
        flag_ = 1;
    if(l2fdb->flag_timestamp < priv->rule_update_timestamp)
    {
        flag_ = 2;
        RTE_DBG("%s(%d),%u.%u.%u.%u:%d---%u.%u.%u.%u:%d,skb_dir:%d,l2ct:0x%p.\n",\
                __FUNCTION__,__LINE__,RTE_NIPQUAD(inner_ip),inner_port,RTE_NIPQUAD(outer_ip),outer_port,skb_dir,l2fdb);
    }

    if(flag_)
    {
        if(flag_ == 1)
        {
            l2fdb->proto_mark_state = 0;
        }
        else//==2
        {
            l2fdb->flag_timestamp = priv->rule_update_timestamp;
        }
    
        l2fdb->flag0 = 0;
        l2fdb->flag2 = 0;
        l2fdb->up_unmatch = 0;
        l2fdb->down_unmatch = 0;
        l2fdb->up_mr_num = 0;
        l2fdb->down_mr_num = 0;
    }
    
    RTE_DBG("%s(%d)flag: flag2:%d, up_unmatch:%d, up_mr_num:%d, down_unmatch:%d, down_mr_num:%d skb_dir:%d\n", __FUNCTION__, __LINE__, l2fdb->flag2, l2fdb->up_unmatch, l2fdb->up_mr_num, l2fdb->down_unmatch, l2fdb->down_mr_num, skb_dir);
    
    if(l2fdb->flag2 == 1)
    {
        struct sft_flow_rule *rule = priv->rule_active[l2fdb->match_rule];
        if(rule)
        {
            rule_flow_statics_rx_add(rule->flow_statics, rte_pktmbuf_data_len(skb));
            rule_flow_statics_cache_add(rule->flow_statics, rte_pktmbuf_data_len(skb));
        }
        
        sft_queue_xmit(skb);
        return 0;
    }

    if(l2fdb->flag0 == 1)
    {
        struct sft_flow_rule *rule = priv->rule_active[l2fdb->match_rule];
        if(rule)
        {
            rule_flow_statics_rx_add(rule->flow_statics, rte_pktmbuf_data_len(skb));
        }
        
        rte_pktmbuf_free(skb);
        return 0;
    }
    
    if(skb_dir == UP_DIR)
    {
        if(l2fdb->up_unmatch == 1)
        {
            sft_queue_xmit(skb);
            return 0;
        }
        num = l2fdb->up_mr_num;
    }
    else//down
    {
        if(l2fdb->down_unmatch == 1)
        {
            sft_queue_xmit(skb);
            return 0;
        }
        num = l2fdb->down_mr_num;
    }

    if(num)
    {
        /*
        ret = do_send_work(skb);
        if(likely(ret == 0))
        {
            return 0;
        }
        else
        {
            return 1;
        }
        */
        //if(do_send_by_worker(skb,(int)smp_processor_id()))
        {
            do_send_work(priv, skb);
        }
        return 0;
    }
 
//    if(printk_ratelimit())
//        printk("%s(%d),[rule_active:%p,rule_backup:%p].\n",__FUNCTION__,__LINE__,rule_active,rule_backup);
    for(i=priv->sft_flow_rule_start;(!break_flag) && (i<=priv->sft_flow_rule_end) && (match_i < MRULES_MAX);i = rule->next_index)
    {
//        if(printk_ratelimit())
//            printk("%s(%d),rule_index:[%d].\n",__FUNCTION__,__LINE__,i);
        if(i == priv->sft_flow_rule_end)
        {
            break_flag = 1;
        }

        rule = priv->rule_active[i];
        if(!rule)
        {//maybe switch rule strategy
            break;
        }

        //rule enable?
        if(!rule->enable)
            continue;

        //dir
        if(skb_dir == DOWN_DIR)
        {
            if(rule->dir == UP_DIR)
                continue;
        }
        else // send to outer device,upload
        {
            if(rule->dir == DOWN_DIR)
                continue;
        }

        //l4proto
        switch(rule->l4proto)
        {
            case PROTO_ANY:
                break;
            case PROTO_TCP:
                if(iph->next_proto_id != IPPROTO_TCP)
                    continue;
                break;
            case PROTO_UDP:
                if(iph->next_proto_id != IPPROTO_UDP)
                    continue;
                break;
            default:
                continue;
                break;
        }

//        {
//            if(ntohs(l2fdb->source) == 80 || ntohs(l2fdb->dest) == 80)
//                //    if(printk_ratelimit())
//                printk("%s(%d),[%u %d %d],proto_mark[%d],%pI4:%d-->%pI4:%d,rcvidx:%d,l4proto:%d.\n",
//                        __FUNCTION__,__LINE__,
//                        rule->app_proto,l2fdb->flag0,l2fdb->flag1,l2fdb->proto_mark,
//                        &l2fdb->sip,
//                        ntohs(l2fdb->source),&l2fdb->dip,ntohs(l2fdb->dest),l2fdb->recvdevn,l2fdb->protocol);
//        }
        //app_proto:0xFFFFFFFF:any app proto; 0x0 unknown app proto
        int proto_index = 0;
        int ismatch = 0;
        for(; !ismatch && proto_index < rule->app_proto_num; proto_index++)
        {
            switch(rule->app_proto[proto_index])
            {
                uint32_t p;
                case 0xFFFFFFFF:
                    ismatch = 1;
                    break;
                case 0x0:
                    if(l2fdb->proto_mark != 0)
                        continue;
                    ismatch = 1;
                    break;
                default:
                    p = l2fdb->proto_mark;
                    if((p & TOPCLASS_MASK) != (rule->app_proto[proto_index] & TOPCLASS_MASK))
                    {//not match topclass
                        continue;
                    }
                    if((rule->app_proto[proto_index] & MIDCLASS_MASK) == MIDCLASS_MASK)//mid class all 1
                    {
                        ismatch = 1;
                        break;
                    }
                    if((p & MIDCLASS_MASK) != (rule->app_proto[proto_index] & MIDCLASS_MASK))
                    {//not match midclass
                        continue;
                    }
                    if((rule->app_proto[proto_index] & SUBCLASS_MASK) == SUBCLASS_MASK)//sub class all 1
                    {
                        ismatch = 1;
                        break;
                    }
                    if((p & SUBCLASS_MASK) != (rule->app_proto[proto_index] & SUBCLASS_MASK))
                    {//not match subclass
                        continue;
                    }
                    ismatch = 1;
                    break;
            }
        }
        if(!ismatch)
        {
            continue;
        }

        //inner ip
        switch(rule->inner_ip_type)
        {
            case IPT_ANY_IP:
                break;
            case IPT_ONE_IP:
                if(rule->inner_ip != inner_ip)
                    continue;
                break;
            case IPT_NETWORK_MASK_IP:
                if(check_ip_mask(rule,(uint8_t *)&inner_ip,0) == 0)
                    continue;
                break;
            case IPT_START_END_IP:
                if(check_ip_range(rule,ntohl(inner_ip),0) == 0)
                    continue;
                break;
            case IPT_GROUP_IP:
                //ip group
                if(check_ipgroup(priv, rule,inner_ip,0) == 0)
                    continue;
                break;
            default:
                continue;
                break;
        }
        //outer ip
        switch(rule->outer_ip_type)
        {
            case IPT_ANY_IP:
                break;
            case IPT_ONE_IP:
                if(rule->outer_ip != outer_ip)
                    continue;
                break;
            case IPT_NETWORK_MASK_IP:
                if(check_ip_mask(rule,(uint8_t *)&outer_ip,1) == 0)
                    continue;
                break;
            case IPT_START_END_IP:
                if(check_ip_range(rule,ntohl(outer_ip),1) == 0)
                    continue;
                break;
            case IPT_GROUP_IP:
                //ip group
                if(check_ipgroup(priv, rule,outer_ip,1) == 0)
                    continue;
                break;
            default:
                continue;
                break;
        }
        //inner port
        switch(rule->inner_port_type)
        {
            case PT_ANY_PORT:
                break;
            case PT_ONE_PORT:
                if(rule->inner_port != inner_port)
                    continue;
                break;
            case PT_START_END_PORT:
                if(check_port_range(rule,ntohs(inner_port),0) == 0)
                    continue;
                break;
            default:
                continue;
                break;
        }
        //outer port
        switch(rule->outer_port_type)
        {
            case PT_ANY_PORT:
                break;
            case PT_ONE_PORT:
                if(rule->outer_port != outer_port)
                    continue;
                break;
            case PT_START_END_PORT:
                if(check_port_range(rule,ntohs(outer_port),1) == 0)
                    continue;
                break;
            default:
                continue;
                break;
        }

//action:
        if(rule->action_type == BLOCK)
        {
            l2fdb->flag0 = 1;
            l2fdb->match_rule = rule->index;
            rte_pktmbuf_free(skb);
            rte_atomic64_inc(&free_pkg_num);
            return 0;
        }
        if(rule->action_type == ACCEPT)
        {
            l2fdb->flag2 = 1;
            l2fdb->match_rule = rule->index;
            sft_queue_xmit(skb);
            return 0;
        }

//        if(printk_ratelimit())
//            printk("%s(%d),%pI4:%d---%pI4:%d,rule id :%d,skb_dir:%d.\n",
//                    __FUNCTION__,__LINE__,&inner_ip,ntohs(inner_port),&outer_ip,ntohs(outer_port),i,skb_dir);
        matched_idx[match_i] = i;

        match_i++;

        if(rule->ifgoon == RULE_STOP)
        {
            break;//break for loop
        }
        else
        {//continue
        }
    }//for() loop

    if(match_i)
    {
#if 0 //just print
        {
            int len = 0; 
            int i = 0; 
            char buf[32];
            memset(buf,0,sizeof(buf));
            for(i = 0;i < match_i;i++)
            {    
                len += sprintf(buf + len, "%d ", matched_idx[i]);  
            }    
            RTE_DBG("%s(%d),%u.%u.%u.%u:%d---%u.%u.%u.%u:%d,match num:{%d[%s]},skb_dir:%d,skb:%p.\n",\
                    __FUNCTION__,__LINE__,RTE_NIPQUAD(inner_ip),inner_port,RTE_NIPQUAD(outer_ip),outer_port,match_i,buf,skb_dir,skb);
        }
#endif        
        
        for(i = 0;i < match_i;i++)
        {
            idx = matched_idx[i];
            rule = priv->rule_active[idx];
            if(!rule || !rule->enable)
            {//do error process
                sft_queue_xmit(skb);
                return 0;
            }
            if(skb_dir == UP_DIR)
            {
                l2fdb->up_mr_idx[i] = idx;
            }
            else
            {
                l2fdb->down_mr_idx[i] = idx;
            }
        }

        if(skb_dir == UP_DIR)
        {
            l2fdb->up_mr_num = match_i;
        }
        else
        {
            l2fdb->down_mr_num = match_i;
        }
/*
        ret = do_send_work(skb);
        if(likely(ret == 0))
        {
            return 0;
        }
        else
        {
            return 1;
        }
*/        
        //if(do_send_by_worker(skb,(int)smp_processor_id()))
        {
            do_send_work(priv, skb);
        }
        return 0;

    }//matched rule
    else
    {//not matched rule
        if(skb_dir == UP_DIR)
        {
            l2fdb->up_unmatch = 1;
        }
        else
        {
            l2fdb->down_unmatch = 1;
        }

        sft_queue_xmit(skb);
        return 0;
    }
}

int sft_queue_xmit_hook(struct rte_mbuf *skb)
{
    if(unlikely(!test_flags_tc_enable()))
    {
        return 1;
    }
    
    return iterate_flow_rule(priv, skb);
}

void sft_tc_send_oneip_handler(struct rte_timer *tim, void *data)
{
    struct rte_mbuf *skb;
    struct sft_flow_ip_entry *fdb;
    uint64_t _1ms = rte_get_timer_hz() / 1000;
    uint64_t cur_tsc = 0, diff_tsc = 0;
    
    //long diff;
    if(data == 0)
        return;
    fdb = (struct sft_flow_ip_entry *)data;

    if(fdb->delete == 1)
        return;

#if 1   
    RTE_DBG("%s(%d),%u.%u.%u.%u,l4proto:%d,ip_rate:%d,dir:%d,token:%d,add:%d.\n",
                __FUNCTION__,__LINE__,RTE_NIPQUAD(fdb->inner_ip),fdb->l4proto,fdb->ip_rate,
                fdb->dir,rte_atomic32_read(&fdb->token_entry.token),fdb->ip_rate);
#endif
     
    //rte_spinlock_lock(&fdb->token_entry.lock);
	if(rte_atomic32_read(&fdb->token_entry.token) < fdb->ip_rate)
	{
//		if(unlikely(fdb->time_mark == 0))
//			diff = SEND_INTERVAL;
//		else
//			diff = (long)jiffies - (long)fdb->time_mark;

//		if(diff < SEND_INTERVAL) 
//			diff = SEND_INTERVAL;

//		fdb->token_entry.token += (fdb->ip_rate/HZ)*(int)diff;
		//fdb->token_entry.token += (fdb->ip_rate/HZ)*SEND_INTERVAL_ONEIP;
        rte_atomic32_add(&fdb->token_entry.token, (fdb->ip_rate/HZ)*SEND_INTERVAL_ONEIP);
//		fdb->time_mark = jiffies;
	}
	//rte_spinlock_unlock(&fdb->token_entry.lock);
    
#if 0
    while((skb = sft_skb_dequeue(fdb->pktQ, &fdb->token_entry)) != NULL)
    {
        //rte_spinlock_lock(&fdb->token_entry.lock);
        //fdb->token_entry.token -= (int)rte_pktmbuf_data_len(skb);
        rte_atomic32_sub(&fdb->token_entry.token, (int)rte_pktmbuf_data_len(skb));
        //rte_spinlock_unlock(&fdb->token_entry.lock);
        RTE_DBG("%s(%d),%u.%u.%u.%u,l4proto:%d,ip_rate:%d,dir:%d,token:%d,add:%d.\n",
            __FUNCTION__,__LINE__,RTE_NIPQUAD(fdb->inner_ip),fdb->l4proto,fdb->ip_rate,
            fdb->dir,fdb->token_entry.token,(fdb->ip_rate/HZ)*SEND_INTERVAL_ONEIP);
     
        skb->mr_tab_idx++;
        struct sft_flow_rule *rule = fdb->priv->rule_active[fdb->index];
        rule_flow_statics_cache_add(rule->flow_statics, rte_pktmbuf_data_len(skb));
       
        //do_send_work(skb);
        //if(do_send_by_worker(skb,(int)smp_processor_id()))
        {
            do_send_work(fdb->priv, skb);
        }       
    }
#else 
    uint64_t  ptr_buf;
    while(tc_dequeue(fdb->pktQ, (void *)&ptr_buf, sizeof(skb)) > 0) {

        skb = (struct rte_mbuf*)ptr_buf;
        if(unlikely(rte_pktmbuf_data_len(skb)  > rte_atomic32_read(&fdb->token_entry.token)))
        {
            if(tc_enqueue(fdb->pktQ, (void *)&skb, sizeof(skb)) == 0)
            {
                sft_queue_xmit(skb);
            }
            return ;
        }

        rte_atomic32_sub(&fdb->token_entry.token, (int)rte_pktmbuf_data_len(skb));
        RTE_DBG("%s(%d),%u.%u.%u.%u,l4proto:%d,ip_rate:%d,dir:%d,token:%d,add:%d.\n",
                __FUNCTION__,__LINE__,RTE_NIPQUAD(fdb->inner_ip),fdb->l4proto,fdb->ip_rate,
                fdb->dir,fdb->token_entry.token,(fdb->ip_rate/HZ)*SEND_INTERVAL_ONEIP);

        skb->mr_tab_idx++;
        struct sft_flow_rule *rule = fdb->priv->rule_active[fdb->index];
	if (likely(rule)) {
        	rule_flow_statics_cache_add(rule->flow_statics, rte_pktmbuf_data_len(skb));
	}
        //do_send_work(skb);
        //if(do_send_by_worker(skb,(int)smp_processor_id()))
        {
            do_send_work(fdb->priv, skb);
        }

    }
#endif
#if 1
    cur_tsc = rte_rdtsc() / TIMER_RESOLUTION_MS;
    diff_tsc = cur_tsc - fdb->timestamp;
    if(diff_tsc >= 2 * 60 * 1000)
    {
        struct ksft_hash_node * hash_node;
        struct cds_hlist_head *head;
    
        int hash_idx = flow_ip_hash(fdb->inner_ip,fdb->l4proto,
                                    fdb->dir,fdb->app_proto,fdb->index, KSFT_FLOW_HASH_ITEM);
        
        rcu_read_lock();
        head = &ksft_flow_ip_hash[hash_idx].hhead;
        
        hash_node = container_of(head,struct ksft_hash_node,hhead);
        RTE_DBG("%s(%d),delete fdb inner ip:%pI4,l4proto:%d.\n",\
                __FUNCTION__,__LINE__,&fdb->inner_ip,fdb->l4proto);
        
        rte_spinlock_lock(&hash_node->lock);

        flow_ip_delete(fdb->priv, fdb);

        rte_spinlock_unlock(&hash_node->lock);
        rcu_read_unlock();
        
        return;
    }
#endif
    //mod_timer(&fdb->timeout_send, jiffies + SEND_INTERVAL_ONEIP);
    rte_timer_reset(tim, _1ms * SEND_INTERVAL_ONEIP, 
                    SINGLE, rte_lcore_id(), sft_tc_send_oneip_handler, (void *)fdb);

}

void sft_tc_send_pipe_handler(struct rte_timer *tim, void *data)
{
    struct rte_mbuf* m = NULL;
    struct sft_flow_datapipe_entry *fdb;
    uint64_t _1ms = rte_get_timer_hz() / 1000;
    
    //long diff ;
    if(data == 0)
        return;
    fdb = (struct sft_flow_datapipe_entry *)data;

    RTE_DBG("%s(%d),rate:%d,token:%d,time:%u.\n", __FUNCTION__,__LINE__,fdb->rate,rte_atomic32_read(&fdb->token_entry.token), (int)time(NULL));
   
    if(fdb->rate == 0 || fdb->used == 0)
    {
        while(tc_dequeue(fdb->pktQ, (void*)m, sizeof(m)) > 0)
        {
            //pkg_num_dec();
            sft_queue_xmit(m);
        }
        tc_queue_free(fdb->pktQ);
        fdb->pktQ = NULL;
        return;
    }
    uint32_t token = rte_atomic32_read(&fdb->token_entry.token);
        
    if((int)token <= fdb->rate)
    {
        rte_atomic32_add(&fdb->token_entry.token, (fdb->rate/HZ)*SEND_INTERVAL);
    }
    
    //rte_spinlock_lock(&fdb->token_entry.lock);
    //if(fdb->token_entry.token < fdb->rate)
    //{
    //    if(unlikely(fdb->time_mark == 0))
    //        diff = SEND_INTERVAL;
    //    else
    //        diff = (long)jiffies - (long)fdb->time_mark;

    //    if(diff < SEND_INTERVAL) 
    //        diff = SEND_INTERVAL;
    //    fdb->token_entry.token += (fdb->rate/HZ)*(int)diff;
    //    fdb->token_entry.token += (fdb->rate/HZ)*SEND_INTERVAL;
    //    fdb->time_mark = jiffies;
    //}
    //rte_spinlock_unlock(&fdb->token_entry.lock);
#if 0 
    while((m = sft_skb_dequeue(fdb->pktQ,&fdb->token_entry)) != NULL)
    {
        //rte_spinlock_lock(&fdb->token_entry.lock);
        //fdb->token_entry.token -= (int)rte_pktmbuf_data_len(m);
        rte_atomic32_sub(&fdb->token_entry.token, (int)rte_pktmbuf_data_len(m));
        //rte_spinlock_unlock(&fdb->token_entry.lock);

        m->mr_tab_idx++;
        
        struct sft_flow_rule *rule = fdb->priv->rule_active[fdb->index];
	if (likely(rule)) {
           rule_flow_statics_cache_add(rule->flow_statics, rte_pktmbuf_data_len(m));
        }
        RTE_DBG("%s(%d), ip_rate:%d,dir:%d,token:%d,add:%d.\n",
            __FUNCTION__,__LINE__, fdb->rate,
            fdb->dir,rte_atomic32_read(&fdb->token_entry.token),(fdb->rate/HZ)*SEND_INTERVAL);

        //do_send_work(skb);
        //if(do_send_by_worker(skb,(int)smp_processor_id()))
        {
            do_send_work(fdb->priv, m);
        }

    }
#else
    uint64_t ptr_buf;
    while(tc_dequeue(fdb->pktQ, (void *)&ptr_buf, sizeof(m)) > 0) {
        m = (struct rte_mbuf*)ptr_buf;
        if(unlikely(rte_pktmbuf_data_len(m)  > rte_atomic32_read(&fdb->token_entry.token)))
        {
            if(tc_enqueue(fdb->pktQ, (void *)&m, 8) == 0)
            {
                sft_queue_xmit(m);
            }
            return ;
        }

        rte_atomic32_sub(&fdb->token_entry.token, (int)rte_pktmbuf_data_len(m));

        m->mr_tab_idx++;

        struct sft_flow_rule *rule = fdb->priv->rule_active[fdb->index];
        rule_flow_statics_cache_add(rule->flow_statics, rte_pktmbuf_data_len(m));

        RTE_DBG("%s(%d), ip_rate:%d,dir:%d,token:%d,add:%d.\n",
                __FUNCTION__,__LINE__, fdb->rate,
                fdb->dir,rte_atomic32_read(&fdb->token_entry.token),(fdb->rate/HZ)*SEND_INTERVAL);

        //do_send_work(skb);
        //if(do_send_by_worker(skb,(int)smp_processor_id()))
        {
            do_send_work(fdb->priv, m);
        }


    }
#endif
    //mod_timer(&fdb->timeout_send, jiffies + SEND_INTERVAL);
    rte_timer_reset(tim, SEND_INTERVAL * _1ms, SINGLE, rte_lcore_id(), sft_tc_send_pipe_handler, (void *)fdb);
}

int tc_flow_ip_init(void)
{
    uint64_t i = 0;
    ksft_fip_hashsize = KSFT_FIP_HASHSIZE * rte_lcore_count();
    KSFT_FLOW_HASH_ITEM = ksft_fip_hashsize; //   /sizeof(struct ksft_hash_node);
    //printk("%s(%d),fip_hash_num:%u.\n",__FUNCTION__,__LINE__,KSFT_FLOW_HASH_ITEM);

    ksft_flow_ip_hash = (struct ksft_hash_node *)rte_zmalloc(NULL, ksft_fip_hashsize * sizeof(struct ksft_hash_node), 0);
    if(!ksft_flow_ip_hash)
    {
        RTE_ERR("%s(%d), ksft_flow_ip_hash rte_zmalloc failed, hash size:%u\n", __FUNCTION__, __LINE__, ksft_fip_hashsize);
        return -1;
    }
    
    //memset(ksft_flow_ip_hash,0,ksft_fip_hashsize);
    for(i = 0 ; i < KSFT_FLOW_HASH_ITEM; i++) 
    {    
        CDS_INIT_HLIST_HEAD(&ksft_flow_ip_hash[i].hhead);
        rte_spinlock_init(&ksft_flow_ip_hash[i].lock);
    } 
    
    return 0;
}

