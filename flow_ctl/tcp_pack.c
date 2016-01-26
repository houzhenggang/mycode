#include <assert.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_eth_ctrl.h>
#include <rte_ip_frag.h>
#include <rte_spinlock.h>

#include "tcp_pack.h"
#include "list.h"
#include "utils.h"
#include "global.h"
#include "dpi.h"

//#define DEBUG_OFO_YHL

static struct rte_mempool *ofo_mem_cache = NULL;

#define RTE_LOGTYPE_TCP_OFO RTE_LOGTYPE_USER1

#ifdef DEBUG_OFO_YHL
#define TCP_PACK_LOG(x,args...) \
    do{				\
        RTE_LOG(ERR, TCP_OFO, x, ##args);	\
    } while(0)
#else
#define TCP_PACK_LOG(...)
#endif

typedef struct tcp_pack_s
{
    struct hlist_node hlist;
    uint32_t up_seq_next;
    uint32_t up_seq;
    uint32_t down_seq_next;
    uint32_t down_seq;
    uint8_t isinner;
    struct rte_mbuf *pskb;
}tcp_pack_t; 

typedef struct tcp_info_s
{
    struct rte_mbuf  * skb;
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t payload_len;

    uint8_t  isinner:1,
        is_http:1,
        is_syn:1,
        is_syn_ack:1,
        is_fin:1,
        is_rst:1;
}tcp_info_t;

static void print_list(struct l2ct_var_dpi *var)
{
    tcp_pack_t *p = NULL, *priv = NULL;
    struct hlist_node *pos = NULL, *n = NULL;
    struct tcp_ofo_node *node = NULL;
return;
    
    uint64_t i = 0;
    hlist_for_each_safe(pos, n, &var->tcp_ofo_head)
    {
        p = hlist_entry(pos, tcp_pack_t, hlist);

        TCP_PACK_LOG("(%s)%p tcp print isinner:%d, last:->[%u, %u] seq:->[%u, %u] next:->[%u, %u]\n", __FUNCTION__, var,
                   p->isinner, var->last_up_seq_next, var->last_down_seq_next,
                   p->up_seq,
                   p->down_seq, 
                   p->up_seq_next,
                   p->down_seq_next);

        i++;
        assert(i <= var->tcp_ofo_size);
        
        if(p && priv && p->isinner == priv->isinner &&((p->isinner && priv->up_seq > p->up_seq) || (!p->isinner && priv->down_seq > p->down_seq)))
        {
            assert(0);
        }
        priv = p;
    }
}

void tcp_ofo_list_packet_free(struct l2ct_var_dpi *var)
{
    tcp_pack_t *p = NULL;
    if(var->tcp_ofo_size == 255 || var->tcp_ofo_size == 0)
        return;

    //printf("%s start---%u, %p\n", __FUNCTION__, var->tcp_ofo_size, var->tcp_ofo_head.first);
    while(!hlist_empty(&var->tcp_ofo_head))
    {
        p = hlist_entry(var->tcp_ofo_head.first, tcp_pack_t, hlist);
        hlist_del_init(&p->hlist);
        
        var->tcp_ofo_size--;

        if(p->pskb)
        {
            do_dpi_entry(p->pskb, p->pskb->in_port, rte_lcore_id());
            rte_pktmbuf_free(p->pskb);
        }
        TCP_PACK_LOG("(%s)%p tcp last:->[%ld, %ld], seq:->[%u, %u] next:->[%u, %u]\n", __FUNCTION__, var,
                   var->last_up_seq_next, var->last_down_seq_next,
                   p->up_seq,
                   p->down_seq,
                   p->up_seq_next,
                   p->down_seq_next);
        rte_mempool_mp_put(ofo_mem_cache, p);
    }
    
    //printf("%s end---%u, %p\n", __FUNCTION__, var->tcp_ofo_size, var->tcp_ofo_head.first);
}

static int tcp_ofo_enqueue(struct l2ct_var_dpi *var, tcp_info_t * ssv)
{
    tcp_pack_t *entity = NULL;
    tcp_pack_t *pos = NULL, *prev = NULL;
    int addition = 0;

    //printf(">%u#[%u.%u.%u.%u|%u.%u.%u.%u, %u|%u]#%u, %u-%u\n", time(NULL), RTE_NIPQUAD(ssv->ssn->ip_src), RTE_NIPQUAD(ssv->ssn->ip_dst), ntohs(ssv->ssn->port_src), ntohs(ssv->ssn->port_dst),var->tcp_ofo_idx, rte_atomic64_read(&g_ofo_list_size), var->tcp_ofo_size);
    //entity = rte_zmalloc(NULL, sizeof(tcp_pack_t), 0);
    if(rte_mempool_mc_get(ofo_mem_cache, (void **)&entity) <0)
		return -1;
    if(entity == NULL)
    {
        RTE_LOG(ERR, TCP_OFO, "%s(%d),tcp order entity malloc err.\n",__FUNCTION__,__LINE__);
        return -1;
    }

    INIT_HLIST_NODE(&entity->hlist);

    if(ssv->is_syn || ssv->is_syn_ack)
    {
        addition = 1;
    }

    if(ssv->isinner)
    {
        entity->up_seq_next = ssv->seq_num + ssv->payload_len + addition;
        entity->up_seq = ssv->seq_num;
        entity->down_seq_next = entity->down_seq = ssv->ack_num;
        entity->isinner = 1;
    }
    else
    {
        entity->down_seq_next = ssv->seq_num + ssv->payload_len + addition;
        entity->down_seq = ssv->seq_num;
        entity->up_seq_next = entity->up_seq = ssv->ack_num;
        entity->isinner = 0;
    }

    struct rte_mempool * clone_pool = get_clone_pool();
    if(clone_pool && ssv->skb)
    {
        entity->pskb = rte_pktmbuf_clone(ssv->skb, clone_pool);
        if(entity->pskb)
        {
            entity->pskb->l2ct = ssv->skb->l2ct;
            entity->pskb->in_port = ssv->skb->in_port;
        }
    }

    hlist_for_each_entry(pos, &var->tcp_ofo_head, hlist)
    {
        if(entity->isinner && pos->isinner)
        {
            if(entity->up_seq < pos->up_seq)
            {
                break;
            }
            else if(entity->up_seq == pos->up_seq)
            {
                if(entity->up_seq_next <= pos->up_seq_next)
                {
                    break;
                }
            }
        }
        
        if(!entity->isinner && !pos->isinner)
        {
            if(entity->down_seq < pos->down_seq)
            {
                break;
            }
            else if(entity->down_seq == pos->down_seq)
            {
                if(entity->down_seq_next <= pos->down_seq_next)
                {
                    break;
                }
            }
        }
        
        prev = pos;
    }

    if(prev)
    {
        hlist_add_after(&prev->hlist, &entity->hlist);
    }
    else
    {
        hlist_add_head(&entity->hlist, &var->tcp_ofo_head);
    }

    var->tcp_ofo_size++;

    TCP_PACK_LOG("(%s)%p tcp add pos:->[%u, %u] seq:->[%u, %u], pskb[%p]\n", __FUNCTION__, var,
                   pos ? pos->up_seq : 0,
                   pos ? pos->down_seq : 0,
                   entity->up_seq,
                   entity->down_seq, entity->pskb);
    
    return 0;
}

static void tcp_ofo_dequeue(struct l2ct_var_dpi *var, tcp_info_t * ssv)
{
    tcp_pack_t *p = NULL;
    struct hlist_node *pos = NULL, *n = NULL;

    //rte_spinlock_lock(&var->lock);
    hlist_for_each_safe(pos, n, &var->tcp_ofo_head)
    {
        p = hlist_entry(pos, tcp_pack_t, hlist);

        if(ssv->isinner != p->isinner)
        {
            continue;
        }
        
        if((p->isinner && var->last_up_seq_next < p->up_seq) || (!p->isinner && var->last_down_seq_next < p->down_seq))
        {
            break;
        }
   
        hlist_del_init(&p->hlist);

        var->tcp_ofo_size--;
        TCP_PACK_LOG("(%s)%p tcp ofo last:->[%ld, %ld] next:->[%u, %u] seq:->[%u, %u],pskb[%p]\n", __FUNCTION__, var,
                   var->last_up_seq_next, var->last_down_seq_next,
                   p->up_seq_next,
                   p->down_seq_next,
                   p->up_seq,
                   p->down_seq, p->pskb);
        if(p->isinner && var->last_up_seq_next == p->up_seq)
        {
            var->last_up_seq_next = p->up_seq_next;
        }
        else if(!p->isinner && var->last_down_seq_next == p->down_seq)
        {
            var->last_down_seq_next = p->down_seq_next;
        }  
            
        if(p->pskb)
        {
            do_dpi_entry(p->pskb, p->pskb->in_port, rte_lcore_id());
            rte_pktmbuf_free(p->pskb);
        }          

        rte_mempool_mp_put(ofo_mem_cache, p);

    }
}

static int check_retransmit(struct l2ct_var_dpi *var, tcp_info_t * ssv)
{
    if(ssv->isinner)
    {
        //upstream
        if(!var->last_up_seq_next)
        {
            return 0;
        }
        else
        {
            if(var->last_up_seq_next > ssv->seq_num )
            {
                //retransmit
                return 1;
            }
        }
    }
    else
    {
        //downstream
        if(!var->last_down_seq_next)
        {
            return 0;
        }
        else
        {
            if(var->last_down_seq_next > ssv->seq_num)
            {
                //retransmit
                return 1;
            }
        }
    }
    return 0;
}

//Return: 0->Sucess, -1->consume this pkg 
int tcp_ofo_retrans_handle(struct ipv4_hdr *iph, struct sft_fdb_entry * ssn, struct rte_mbuf  * skb)
{
    struct l2ct_var_dpi *var = NULL;
    int addition = 0;

    tcp_info_t tcp_info;

    if(!get_flags_ofo_enable())
        goto OUTGOING;
    
    if(unlikely(!ofo_mem_cache))
    {
        goto OUTGOING;
    }
    
    int frag_off = (ntohs(iph->fragment_offset ) & IPV4_HDR_OFFSET_MASK) * IPV4_HDR_OFFSET_UNITS;
    if(rte_ipv4_frag_pkt_is_fragmented(iph) && frag_off != 0)
	{
        //IP fragment packet, and not first.
        goto OUTGOING;
    }

    var = (struct l2ct_var_dpi *)&ssn->vars_dpi;
    
    if(iph->next_proto_id != IPPROTO_TCP || var->tcp_ofo_size == (uint8_t)-1)
    {
        goto OUTGOING;
    }  

    struct tcp_hdr * tcph = (struct tcp_hdr *)((unsigned char *)iph + (iph->version_ihl & 0x0f) * 4);

    memset(&tcp_info, 0, sizeof(tcp_info_t));
    tcp_info.payload_len = rte_cpu_to_be_16(iph->total_length) - ((iph->version_ihl & 0x0f) * 4) - ((tcph->data_off & 0xf0) >> 2);
    tcp_info.seq_num = rte_cpu_to_be_32(tcph->sent_seq);
    tcp_info.ack_num = rte_cpu_to_be_32(tcph->recv_ack);
    if((tcph->tcp_flags & TCP_SYN_FLAG) && !(tcph->tcp_flags & TCP_ACK_FLAG))
        tcp_info.is_syn = 1;
    if((tcph->tcp_flags & TCP_SYN_FLAG) && (tcph->tcp_flags & TCP_ACK_FLAG))
        tcp_info.is_syn_ack = 1;
    if(tcph->tcp_flags & TCP_FIN_FLAG)
        tcp_info.is_fin = 1;
    if(tcph->tcp_flags & TCP_RST_FLAG)
        tcp_info.is_rst = 1;
    tcp_info.isinner = isinner_by_portid(skb->in_port);
    tcp_info.skb = skb;
          
#if 1
    TCP_PACK_LOG("(%s)%p start session:[%u.%u.%u.%u|%u.%u.%u.%u, %u|%u], seq num:last->[%ld, %ld], curr->[%u, %u], len:[%u],\
                  [syn:%d|syn-ack:%d|fin:%d], isinner:%d, ofo size:%lu\n", 
                   __FUNCTION__,
                   var, RTE_NIPQUAD(ssn->sess_key.ip_src), RTE_NIPQUAD(ssn->sess_key.ip_dst), ntohs(ssn->sess_key.port_src), ntohs(ssn->sess_key.port_dst), 
                   var->last_up_seq_next, var->last_down_seq_next,
                   tcp_info.seq_num, tcp_info.ack_num, tcp_info.payload_len, 
                   tcp_info.is_syn, tcp_info.is_syn_ack, tcp_info.is_fin, tcp_info.isinner, var->tcp_ofo_size);
#endif

    if(check_retransmit(var, &tcp_info))
    {
        goto CONSUME;
    }

    if(ssn->pkt_cnt >= MAX_PKT_NUM || var->tcp_ofo_size >= MAX_PKT_NUM * 2 || tcp_info.is_rst || tcp_info.is_fin)
    {
        var->tcp_ofo_size = (uint8_t)-1;
        tcp_ofo_list_packet_free(var);
        goto OUTGOING;
    }

    if(var->tcp_ofo_size)
    {
        if(tcp_ofo_enqueue(var, &tcp_info) < 0)
        {
            goto OUTGOING;
        }
        tcp_ofo_dequeue(var, &tcp_info);
        goto CONSUME;
    }

    if(tcp_info.is_syn || tcp_info.is_syn_ack)
    {
        addition = 1;
    }

    if(tcp_info.isinner) 
    {
        //upstream
        if(tcp_info.is_syn || tcp_info.is_syn_ack) 
        {
            var->last_up_seq_next = tcp_info.seq_num;
        }

        if(!var->last_up_seq_next)
        {
            var->tcp_ofo_size = (uint8_t)-1;
            goto OUTGOING;
        }
        else
        {
            if(var->last_up_seq_next < tcp_info.seq_num /*|| var->last_down_seq_next < ssv->ack_num*/)
            {
                //out of order
                if(tcp_ofo_enqueue(var, &tcp_info) < 0)
                {
                    goto OUTGOING;
                }
                goto CONSUME;
            }
        }

        var->last_up_seq_next += (tcp_info.payload_len + addition); 
    }
    else
    {
        //downstream
        if(tcp_info.is_syn || tcp_info.is_syn_ack)
        {
            var->last_down_seq_next = tcp_info.seq_num;
        } 

        if(!var->last_down_seq_next)
        {
            var->tcp_ofo_size = (uint8_t)-1;
            goto OUTGOING;
        }
        else
        {
            if(var->last_down_seq_next < tcp_info.seq_num /*|| var->last_up_seq_next < ssv->ack_num*/)
            {
                if(tcp_ofo_enqueue(var, &tcp_info) < 0)
                {
                    goto OUTGOING;
                }
                goto CONSUME;
            }
        }

        var->last_down_seq_next += (tcp_info.payload_len + addition);
    }

CONSUME:
    set_bit(1, &skb->vars);
OUTGOING:
    return 0;
    
}

void tcp_ofo_mem_init(void)
{
    if(!ofo_mem_cache)
    {
        ofo_mem_cache = rte_mempool_create("ofo_mem_cache", get_hash_size(),
                sizeof(tcp_pack_t),
                64, 0,
                cache_mp_init, NULL,
                cache_obj_init, NULL,
                rte_socket_id(),  0);
    }
  
}

#if 0
static inline int decode_skb(struct rte_mbuf  * skb, struct ssn_skb_values * ssv)
{
    struct tcp_hdr *tcp;
    struct udp_hdr *udp;
    uint16_t l4_len;

    struct ipv4_hdr *ip_hdr = (struct ipv4_hdr *)(rte_pktmbuf_mtod(skb, struct ether_hdr *) + 1);
    //if (unlikely(!ip_hdr))
    //    return -1;

//    uint16_t l3_len = (ip_hdr->version_ihl & 0x0f) * 4;
    ssv->skb = skb;
    ssv->sip = ip_hdr->src_addr;
    ssv->dip = ip_hdr->dst_addr;
    ssv->isinner = isinner_by_portid(skb->in_port);

    if (unlikely(rte_ipv4_frag_pkt_is_fragmented(ip_hdr))) {
            return -1;
    } else {
        switch (ip_hdr->next_proto_id)
        {
            case IPPROTO_TCP:
                tcp = (struct tcp_hdr *)((unsigned char *)ip_hdr +
                        sizeof(struct ipv4_hdr));
                ssv->sport = tcp->src_port;
                ssv->dport = tcp->dst_port;
                l4_len = (tcp->data_off & 0xf0) >> 2;
//                printf("tcp->data_off=%d\n", (tcp->data_off & 0xf0) >> 2);
                ssv->payload = (char *)tcp + l4_len;
                ssv->payload_len = rte_cpu_to_be_16(ip_hdr->total_length) -
                        ((ip_hdr->version_ihl & 0x0f) * 4) - l4_len;
                ssv->seq_num = rte_cpu_to_be_32(tcp->sent_seq);
                ssv->ack_num = rte_cpu_to_be_32(tcp->recv_ack);
                if((tcp->tcp_flags & TCP_SYN_FLAG) && !(tcp->tcp_flags & TCP_ACK_FLAG))
                    ssv->is_syn = 1;
                if((tcp->tcp_flags & TCP_SYN_FLAG) && (tcp->tcp_flags & TCP_ACK_FLAG))
                    ssv->is_syn_ack = 1;
                if(tcp->tcp_flags & TCP_FIN_FLAG)
                    ssv->is_fin = 1;
                if(tcp->tcp_flags & TCP_RST_FLAG)
                    ssv->is_rst = 1;

                break;

            case IPPROTO_UDP:
                udp = (struct udp_hdr *)((unsigned char *)ip_hdr +
                        sizeof(struct ipv4_hdr));
                ssv->sport = udp->src_port;
                ssv->dport = udp->dst_port;
                ssv->payload = (char *)udp + sizeof(struct udp_hdr);
                ssv->payload_len = rte_cpu_to_be_16(ip_hdr->total_length) -
                        ((ip_hdr->version_ihl & 0x0f) * 4) - sizeof(struct udp_hdr);
                break;

            default:
                return -1;
        }
        ssv->l4proto =  ip_hdr->next_proto_id;

    }
    return 0;
}

void dpi_test(struct rte_mbuf *m)
{
    struct ssn_skb_values  ssv;
    struct sft_fdb_entry * ssn = (struct sft_fdb_entry *)m->l2ct;
    if(!ssn) return;
    
    if(decode_skb(m, &ssv) >= 0)
    {
        //tcp_packet_handle(&ssv, ssn);
        if(tcp_packet_handle(ssv, ssv->ssn) == 0)
        {
            struct l2ct_var_dpi *l2ct = &ssv->ssn->vars_dpi;
            if(l2ct->last_up_seq_next == ssv->seq_num + ssv->payload_len ||
                l2ct->last_down_seq_next == ssv->seq_num + ssv->payload_len ||
                l2ct->last_up_seq_next == ssv->ack_num + ssv->payload_len ||
                l2ct->last_down_seq_next == ssv->ack_num + ssv->payload_len ||
                ssv->is_syn || ssv->is_syn_ack)
            {
                    
            }
            else
            {
#define RTE_NIPQUAD(addr) \
((unsigned char *)&addr)[0], \
((unsigned char *)&addr)[1], \
((unsigned char *)&addr)[2], \
((unsigned char *)&addr)[3]
                //printf("%p===>[%u.%u.%u.%u-%u.%u.%u.%u]%u-%u, %u-%u %u\n", l2ct, RTE_NIPQUAD(ssv->sip), RTE_NIPQUAD(ssv->dip),l2ct->last_up_seq_next, l2ct->last_down_seq_next, ssv->seq_num, ssv->ack_num, ssv->payload_len);
            }
            return 0;
        }
    }
}
#endif

void check_tcp_ofo(struct ssn_skb_values  *ssv)
{
    struct l2ct_var_dpi *l2ct = &ssv->ssn->vars_dpi;
            if(l2ct->last_up_seq_next == ssv->seq_num + ssv->payload_len ||
                l2ct->last_down_seq_next == ssv->seq_num + ssv->payload_len ||
                l2ct->last_up_seq_next == ssv->ack_num + ssv->payload_len ||
                l2ct->last_down_seq_next == ssv->ack_num + ssv->payload_len ||
                ssv->is_syn || ssv->is_syn_ack || l2ct->last_down_seq_next == 0 || l2ct->last_up_seq_next == 0)
            {
                    
            }
            else
            {
#define RTE_NIPQUAD(addr) \
((unsigned char *)&addr)[0], \
((unsigned char *)&addr)[1], \
((unsigned char *)&addr)[2], \
((unsigned char *)&addr)[3]
                printf("%p===>[%u.%u.%u.%u-%u.%u.%u.%u]%u-%u, %u-%u %u\n", l2ct, RTE_NIPQUAD(ssv->sip), RTE_NIPQUAD(ssv->dip),l2ct->last_up_seq_next, l2ct->last_down_seq_next, ssv->seq_num, ssv->ack_num, ssv->payload_len);
            }
}
