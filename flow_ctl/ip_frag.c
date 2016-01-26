#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_ether.h>
#include <rte_timer.h>
#include <rte_jhash.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_eth_ctrl.h>

#include "utils.h"
#include "list.h"
#include "ip_frag.h"
#include "session_mgr.h"
#include "dpi.h"

#define IP_OFFSET 2

//#define DEBUG_IPFRAG_YHL

#define RTE_LOGTYPE_IP_FRAG RTE_LOGTYPE_USER1

#ifdef DEBUG_IPFRAG_YHL
#define IP_FRAG_LOG(x,args...) \
    do{                         \
        RTE_LOG(DEBUG, IP_FRAG, x, ##args);      \
    } while(0)
#else
#define IP_FRAG_LOG(...)
#endif

static rte_atomic64_t ipfrag_pkg_num = RTE_ATOMIC64_INIT(0);

static inline int ipqhashfn(const unsigned int sip, 
                            const unsigned int dip,
                            uint8_t proto, uint16_t id, 
                            unsigned int sft_hash_rnd)
{
    return rte_jhash_3words((uint32_t)id << 16 | proto,
                        (uint32_t)sip, (uint32_t)dip,
                        sft_hash_rnd) & (HASH_IPFRAG_SIZE - 1);
}

static void frag_gc_handler(struct rte_timer *tim, void *data)
{
    frag_info_t *p = NULL;
    struct list_head *pos = NULL, *n = NULL;

    ip_frag_t *frag = (ip_frag_t *)data;

    if(unlikely(!frag))
    {
        return;
    }

    IP_FRAG_LOG("%s(%d), frag_gc_handler, tim:%p, frag:%p\n", __FUNCTION__, __LINE__, tim, data);
    //rte_spinlock_lock(&frag->lock);
#if 0
    list_for_each_safe(pos, n, &frag->frag_queue.frag_list)
    {
        p = list_entry(pos, frag_info_t, list);
#endif
    while(!list_empty(&frag->frag_queue.frag_list))
    {
        p = list_entry(frag->frag_queue.frag_list.next, frag_info_t, list);
        list_del(&p->list);
        //rte_spinlock_unlock(&frag->lock);

        //send ip frag packet
        //set_bit(5, &p->skb->vars);
        //netif_receive_skb(p->skb);
        // TO DO : call dpi module
        set_bit(5, &p->skb->vars);
        if(session_fwd_handle(p->skb, p->skb->in_port))
        {
            do_dpi_entry(p->skb, p->skb->in_port, rte_lcore_id());
        }

        uint8_t out = get_dport_by_sport(p->skb->in_port);
        if(out == (uint8_t)-1)
        {
            rte_pktmbuf_free(p->skb);
        }
        else
        {
            send_single_packet(p->skb, out);
        } 

        IP_FRAG_LOG("(%s) %p frag_gc_handler send ip:[%u:%u:%u:%u-%u:%u:%u:%u], proto:%u, id:%u, next_off:[%u-%u] \n",
                 __FUNCTION__, frag, RTE_NIPQUAD(frag->saddr), RTE_NIPQUAD(frag->daddr), frag->proto, 
                 frag->id, frag->frag_off_next, p->frag_off);

        frag->queue_size--;
        rte_free(p);
        //rte_atomic64_dec(&ipfrag_pkg_num);

        //rte_spinlock_lock(&frag->lock);
    }
    //rte_spinlock_unlock(&frag->lock);

    //rte_spinlock_lock(&hash_node->lock);
    hlist_del(&frag->hlist);
    //rte_spinlock_unlock(&hash_node->lock);
    
    free(frag);
}

void init_ip_frag(struct ip_frag_globle *globle)
{
    int i = 0;
    
    if(!globle)
    {
        return;
    }
    
    srand(time(NULL));
    globle->sft_hash_rnd = random();

    memset(globle->ksft_hash_ipfrag, 0, sizeof(struct ip_frag_hash_node) * HASH_IPFRAG_SIZE);
    for(i = 0 ; i < HASH_IPFRAG_SIZE; i++) 
    {    
        INIT_HLIST_HEAD(&globle->ksft_hash_ipfrag[i].hhead);
        //rte_spinlock_init(&ksft_hash_ipfrag[i].lock);
    }
}

static ip_frag_t * create_ip_frag_entry(void)
{
    ip_frag_t * frag = NULL;

    frag = calloc(sizeof(ip_frag_t), 1);
    if(!frag) return NULL;

    //rte_spinlock_init(&frag->lock);
    INIT_LIST_HEAD(&frag->frag_queue.frag_list);
    //setup_timer(&frag->timeout, frag_gc_handler, (unsigned long)frag);
    rte_timer_init(&frag->timeout);
    return frag;
}

static ip_frag_t * ip_frag_match(struct hlist_head *head, 
                         uint32_t sip, uint32_t dip,
                         uint8_t proto, uint16_t id)
{
    ip_frag_t *frag = NULL;

    hlist_for_each_entry(frag, head, hlist)
    {
        if(frag->saddr == sip && frag->daddr == dip && frag->proto == proto && frag->id == id)
        {
            return frag;
        }
    }

    return NULL;
}

static void frag_ofo_add(ip_frag_t * ip_frag, struct rte_mbuf *skb)
{
    struct list_head *pos = NULL;
    frag_info_t *p = NULL;
    frag_info_t *new_frag = NULL;
    struct ipv4_hdr *ipv4_hdr = NULL;

    new_frag = rte_zmalloc(NULL, sizeof(frag_info_t), 0);
    if(unlikely(!new_frag))  return;

    ipv4_hdr = (struct ipv4_hdr *)(rte_pktmbuf_mtod(skb, unsigned char *) + sizeof(struct ether_hdr));
    new_frag->frag_off = (ntohs(ipv4_hdr->fragment_offset ) & IPV4_HDR_OFFSET_MASK) * IPV4_HDR_OFFSET_UNITS;
    new_frag->frag_payload_len = (rte_pktmbuf_data_len(skb) - sizeof(struct ether_hdr) - sizeof(struct ipv4_hdr));
    new_frag->skb = skb;

    //rte_spinlock_lock(&ip_frag->lock);
    list_for_each(pos, &ip_frag->frag_queue.frag_list)
    {
        p = list_entry(pos, frag_info_t, list);
        if(p->frag_off > new_frag->frag_off)
        {
            break;
        }
        p = NULL;
    }
    if(p)
    {
        list_add_tail(&new_frag->list, &p->list);
    }
    else
    {
        list_add_tail(&new_frag->list, &ip_frag->frag_queue.frag_list);
    }
    rte_atomic64_inc(&ipfrag_pkg_num);
    
    IP_FRAG_LOG("(%s) %p add ip:[%u:%u:%u:%u-%u:%u:%u:%u], proto:%u, id:%u, off:[%u-%u], len:%u \n",
                 __FUNCTION__, ip_frag, RTE_NIPQUAD(ip_frag->saddr), RTE_NIPQUAD(ip_frag->daddr), ip_frag->proto, 
                 ip_frag->id, new_frag->frag_off, p?p->frag_off:0, new_frag->frag_payload_len);
    ip_frag->queue_size++;
    //rte_spinlock_unlock(&ip_frag->lock);
}

static void frag_ofo_handler(ip_frag_t *ip_frag)
{
    struct list_head *pos = NULL, *n = NULL;
    frag_info_t *p = NULL;

    //rte_spinlock_lock(&ip_frag->lock);
    list_for_each_safe(pos, n, &ip_frag->frag_queue.frag_list)
    {
        p = list_entry(pos, frag_info_t, list);
        if(ip_frag->frag_off_next < p->frag_off)
        {
            break;
        }
        else if(ip_frag->frag_off_next > p->frag_off)
        {
            //printk("(%s) WARNING: ip fragment order error!\n", __FUNCTION__);
        }
        list_del(&p->list);
        ip_frag->queue_size--;
        //rte_spinlock_unlock(&ip_frag->lock);

        //send ip fragment packet
        //set_bit(5, &p->skb->vars);
        //netif_receive_skb(p->skb);
        // TO DO :
        set_bit(5, &p->skb->vars);
        if(session_fwd_handle(p->skb, p->skb->in_port))
        {
            do_dpi_entry(p->skb, p->skb->in_port, rte_lcore_id());
        }

        uint8_t out = get_dport_by_sport(p->skb->in_port);
        if(out == (uint8_t)-1)
        {
            rte_pktmbuf_free(p->skb);
        }
        else
        {
            send_single_packet(p->skb, out);
        } 

        IP_FRAG_LOG("(%s) %p ofo send ip:[%u:%u:%u:%u-%u:%u:%u:%u], proto:%u, id:%u, next_off:%u \n",
                 __FUNCTION__, ip_frag, RTE_NIPQUAD(ip_frag->saddr), RTE_NIPQUAD(ip_frag->daddr), ip_frag->proto, 
                 ip_frag->id, ip_frag->frag_off_next);
                 
        ip_frag->frag_off_next = (p->frag_off + p->frag_payload_len);
        rte_free(p);
        //rte_atomic64_dec(&ipfrag_pkg_num);
        
        //rte_spinlock_lock(&ip_frag->lock);  
    }
    //rte_spinlock_unlock(&ip_frag->lock);
}

static int copy_header_from_frag(ip_frag_t *ip_frag, void *hdr, int proto)
{
    if(proto == IPPROTO_TCP)
    {
        memcpy(hdr, &ip_frag->hdr._tcph, sizeof(struct tcp_hdr));
        return 0;
    }
    else if(proto == IPPROTO_UDP)
    {
        memcpy(hdr, &ip_frag->hdr._udph, sizeof(struct udp_hdr));
        return 0;
    }
    else
    {
        return -1;
    }
}

/*
 *1:go orig path
 *0:I consume this skb
 *-1: error
 * */
int ip_frag_handle(struct rte_mbuf *skb, void *hdr, struct sess_hash *hash)
{
    struct ipv4_hdr *iph;
    struct ether_hdr *eth_hdr = NULL;
    struct tcp_hdr *tcphead = NULL; 
    struct udp_hdr *udphead = NULL; 
    ip_frag_t *ip_frag = NULL;
    struct hlist_head *head = NULL;
    unsigned short offset = 0;
    unsigned short iph_len = 0;
    
    if(!skb || !hdr || !hash)
    {
        return -1;
    }

    eth_hdr = rte_pktmbuf_mtod(skb, struct ether_hdr *);

    uint16_t ether_type = eth_hdr->ether_type;
    size_t vlan_offset = get_vlan_offset(eth_hdr, &ether_type);
    
    iph = (struct ipv4_hdr *)((char *)(eth_hdr + 1) + vlan_offset);
    if(!iph) return -1;
    
    offset = (ntohs(iph->fragment_offset ) & IPV4_HDR_OFFSET_MASK) * IPV4_HDR_OFFSET_UNITS;
    iph_len = (iph->version_ihl & 0x0F)<<2;
    int payload = rte_pktmbuf_data_len(skb) - sizeof(struct ether_hdr) - sizeof(struct ipv4_hdr) - vlan_offset;

    //IP_FRAG_LOG("(%s) start ip:[%u:%u:%u:%u-%u:%u:%u:%u], proto:%u, id:%u, off:%u, len:%u \n", 
    //             __FUNCTION__, RTE_NIPQUAD(iph->src_addr), RTE_NIPQUAD(iph->dst_addr), iph->next_proto_id, ntohs(iph->packet_id), offset, payload);

    head = &hash->globle.ksft_hash_ipfrag[ipqhashfn(iph->src_addr,iph->dst_addr,iph->next_proto_id,iph->packet_id, hash->globle.sft_hash_rnd)].hhead;
    ip_frag = ip_frag_match(head,iph->src_addr,iph->dst_addr,iph->next_proto_id,iph->packet_id);
    if(ip_frag)  
    {
    }
    else
    {
        ip_frag = create_ip_frag_entry();
        if(unlikely(!ip_frag))  goto ERROR_EXIT;
        ip_frag->saddr = iph->src_addr;
        ip_frag->daddr = iph->dst_addr;
        ip_frag->proto = iph->next_proto_id;
        ip_frag->id = iph->packet_id;
        ip_frag->hash = hash;

        //mod_timer(&ip_frag->timeout, jiffies + FRAG_GC_INTERVAL);
        uint64_t timer1ms = rte_get_timer_hz() / 1000; 
        rte_timer_reset(&ip_frag->timeout, timer1ms * FRAG_GC_INTERVAL, 
                    SINGLE, rte_lcore_id(), frag_gc_handler, 
                    (void *)ip_frag);
        //hash_node = container_of(head, struct ksft_hash_node, hhead);
        //rte_spinlock_lock(&hash_node->lock);
        hlist_add_head(&ip_frag->hlist, head);
        //rte_spinlock_unlock(&hash_node->lock);
    }

    if(offset == 0)
    {
        if(iph->next_proto_id == IPPROTO_TCP)
        { 
            tcphead = (struct tcp_hdr *)((unsigned char *)iph + sizeof(struct ipv4_hdr));
            if(unlikely(!tcphead)) goto ERROR_EXIT;

            memcpy(&ip_frag->hdr._tcph, tcphead, sizeof(struct tcp_hdr));
        }
        else if(iph->next_proto_id == IPPROTO_UDP)
        {
            udphead = (struct udp_hdr *)((unsigned char *)iph + sizeof(struct ipv4_hdr));
            if(unlikely(!udphead)) goto ERROR_EXIT;

            memcpy(&ip_frag->hdr._udph, udphead, sizeof(struct udp_hdr));
        }
        else
        {
            goto ERROR_EXIT;
        }
    }

    if(test_bit(5, &skb->vars))
    {
        goto NOMAL_EXIT; 
    }
    
#if 0
    if(ip_frag->queue_size)
    {
        frag_ofo_add(ip_frag, skb);
        frag_ofo_handler(ip_frag);
        goto OFO_EXIT;
    }
    
    if(ip_frag->frag_off_next < offset)
    {
        frag_ofo_add(ip_frag, skb);
        goto OFO_EXIT;
    }
#endif
    
    ip_frag->frag_off_next = (offset + payload);

NOMAL_EXIT:    
    if(!copy_header_from_frag(ip_frag, hdr, iph->next_proto_id))
    {
    }
    else
    {
        goto ERROR_EXIT;
    }

    IP_FRAG_LOG("(%s) ip:[%u:%u:%u:%u-%u:%u:%u:%u], proto:%u, id:%u, off:%u, next:%u, len:%u \n", 
                 __FUNCTION__, RTE_NIPQUAD(iph->src_addr), RTE_NIPQUAD(iph->dst_addr), iph->next_proto_id, ntohs(iph->packet_id), 
                 offset, ip_frag->frag_off_next, payload);
    return 1;

OFO_EXIT:
    return 0;

ERROR_EXIT:
    return -1;
}

int decode_skb_ipfrag(struct rte_mbuf * skb, struct ssn_skb_values * ssv, struct sft_fdb_entry * ssn)
{
    struct ether_hdr *eth_hdr = NULL;
    struct ipv4_hdr *iph = NULL;
    struct tcp_hdr *tcph = NULL;
    unsigned short payload_ofs;
    unsigned char l4proto;
    unsigned int sip, dip;
    unsigned short sport, dport;
    int offset = 0;

    eth_hdr = rte_pktmbuf_mtod(skb, struct ether_hdr *);

    uint16_t ether_type = eth_hdr->ether_type;
    size_t vlan_offset = get_vlan_offset(eth_hdr, &ether_type);
    
    iph = (struct ipv4_hdr *)((char *)(eth_hdr + 1) + vlan_offset);
    //iph = (struct ipv4_hdr *)(rte_pktmbuf_mtod(skb, unsigned char *) + sizeof(struct ether_hdr));
    if(!iph) return -1;

    sip = iph->src_addr;
    dip = iph->dst_addr;
    l4proto = iph->next_proto_id;
    offset = (ntohs(iph->fragment_offset ) & IPV4_HDR_OFFSET_MASK) * IPV4_HDR_OFFSET_UNITS;
    ssv->isinner = isinner_by_portid(skb->in_port);
    switch(l4proto)
    {
        case IPPROTO_TCP:
            if(offset)
            {
                payload_ofs = (iph->version_ihl & 0x0F)<<2;
                //payload_ofs = sizeof(struct ipv4_hdr);
            }
            else
            {
                //tcph = skb_header_pointer(skb, (iph->ihl<<2), sizeof(_tcph), &_tcph);
                tcph = (struct tcp_hdr *)((unsigned char *)iph + ((iph->version_ihl & 0x0F)<<2));
                if(!tcph) return -1;
                payload_ofs = ((tcph->data_off & 0xF0) >> 2) + ((iph->version_ihl & 0x0F)<<2);
                
                if((tcph->tcp_flags & TCP_SYN_FLAG) && !(tcph->tcp_flags & TCP_ACK_FLAG))
                    ssv->is_syn = 1;
                if((tcph->tcp_flags & TCP_SYN_FLAG) && (tcph->tcp_flags & TCP_ACK_FLAG))
                    ssv->is_syn_ack = 1;
                if(tcph->tcp_flags & TCP_FIN_FLAG)
                    ssv->is_fin = 1;
                if(tcph->tcp_flags & TCP_RST_FLAG)
                    ssv->is_rst = 1;

                ssv->seq_num = rte_cpu_to_be_32(tcph->sent_seq);
                ssv->ack_num = rte_cpu_to_be_32(tcph->recv_ack);
            }
            if(ssv->isinner)
            {
                sport = ssn->sess_key.port_src;
                dport = ssn->sess_key.port_dst;
            }               
            else
            {
                sport = ssn->sess_key.port_dst;
                dport = ssn->sess_key.port_src;
            }
            break;

        case IPPROTO_UDP:
            if(offset)
            {      
                payload_ofs = (iph->version_ihl & 0x0F)<<2;
            }      
            else
            {
                payload_ofs = sizeof(struct udp_hdr) + ((iph->version_ihl & 0x0F)<<2);
            }
            if(ssv->isinner)
            {
                sport = ssn->sess_key.port_src;
                dport = ssn->sess_key.port_dst;
            }
            else
            {
                sport = ssn->sess_key.port_dst;
                dport = ssn->sess_key.port_src;
            }
            break;

        default:
            return -1;
    }

    ssv->payload_len = rte_pktmbuf_data_len(skb) - payload_ofs;
    ssv->payload = (char *)((unsigned char *)iph + payload_ofs);
    ssv->skb = skb;
    ssv->sip = sip;
    ssv->dip = dip;
    ssv->sport = sport;
    ssv->dport = dport;
    ssv->l4proto = l4proto;

    IP_FRAG_LOG("(%s) %p ip:[%u:%u:%u:%u-%u:%u:%u:%u], port:[%u-%u], protid:%d, payload_len:%d\n", __FUNCTION__, ssn, 
           RTE_NIPQUAD(ssv->sip),RTE_NIPQUAD(ssv->dip),ntohs(ssv->sport),ntohs(ssv->dport), ssv->isinner, ssv->payload_len);

    return 0;
}
