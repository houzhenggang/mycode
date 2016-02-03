#include <arpa/inet.h>

#include <rte_ether.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_jhash.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_ip_frag.h>
#include "debug.h"
#include "sft_statictics.h"
#include "utils.h"
#include "study_tbl.h"
#include "dynamic_tbl.h"
#include "init.h"
#include "ftp.h"
#include "com.h"
#include "session_mgr.h"
#include "session_tbl.h"
#include "rules.h"
#include "dpi_log.h"
#include "fixed_length_dpi.h"

#define DNS_STUDY_TIMEO (3600 * 24)
//static uint8_t dpi_ctrl = DPI_DISABLE;
//static uint8_t dpi_ctrl = DPI_ENABLE;

static inline void update_proto_mark(struct ssn_skb_values *ssv,unsigned int newmark,int flag)
{
//    if(NULL == ssv)
//        return;

//    if(NULL == ssv->ssn)
//        return;
    
    if(newmark == ssv->ssn->proto_mark)
    {
        return;
    }

    ssv->ssn->proto_mark = newmark;
    ssv->ssn->proto_mark_state = 1;
//    ssv->ssn->identifies_proto = 1;
//    ssv->ssn->need_update_link = 1;

#if 0
    if(sft_dpi_print_flag == 1)
    {
        if(flag == 0)
        {
            int big_type,mid_type,sub_type;
            big_type = (ssv->ssn->proto_mark & BIG_TYPE_MASK) >> 28;
            mid_type = (ssv->ssn->proto_mark & MID_TYPE_MASK) >> 24;
            sub_type = (ssv->ssn->proto_mark & SUB_TYPE_MASK) >> 16;
            D("l4proto:%d,%pI4:%d-->%pI4:%d,proto_mark:%u[0x%02x],[%s],state_cnt:%d.\n",
                    ssv->l4proto,&ssv->sip,ntohs(ssv->sport),
                    &ssv->dip,ntohs(ssv->dport),ssv->ssn->proto_mark,ssv->ssn->proto_mark,
                    DPI_Statistics[big_type][mid_type][sub_type].name,
                    ssv->ssn->state_cnt);
        }
    }
#endif
    return;
}

static inline int do_user_proto_work(struct ssn_skb_values * ssv)
{
//    if(NULL == ssv)
//        return 0;
//    if(NULL == ssv->ssn)
//        return 0;

    int ret = 0;

    if(!ssv->isinner)
    {//downlink
        return ret;
    }
/*
    if(ssv->ssn->proto_mark)
    {
        return ret;
    }
*/
    if(ssv->l4proto == IPPROTO_TCP)
    {
        if(user_tcp_proto[ssv->dport].type_id)
        {
            update_proto_mark(ssv,user_tcp_proto[ssv->dport].type_id,0);
            ret = 1;
        }
    }
    else
    {
        if(user_udp_proto[ssv->dport].type_id)
        {
            update_proto_mark(ssv,user_udp_proto[ssv->dport].type_id,0);
            ret = 1;
        }
    }

    return ret;
}


//:1:already identificatied
static inline int do_user_proto_node_work(struct ssn_skb_values * ssv)
{
//    if(NULL == ssv)
//        return 0;
 //   if(NULL == ssv->ssn)
 //       return 0;
 
    int i;
    int ret = 0;
    unsigned int proto_mark = 0;
    
    if(!ssv->isinner)
    {//downlink
        return ret;
    }
/*
    if(ssv->ssn->proto_mark)
    {
        return ret;
    }
*/
    if(ssv->l4proto == IPPROTO_TCP)
    {
        for(i=0;i<SAMEPORT_IPNUM;i++)
        {
            if(user_tcp_proto_node[ssv->dport][i].ip == 0)
            {
                break;
            }
            if(user_tcp_proto_node[ssv->dport][i].ip == ssv->dip)
            {
                proto_mark = user_tcp_proto_node[ssv->dport][i].type_id;
                break;
            }
        }
    }
    else
    {
        for(i=0;i<SAMEPORT_IPNUM;i++)
        {
            if(user_udp_proto_node[ssv->dport][i].ip == 0)
            {
                break;
            }
            if(user_udp_proto_node[ssv->dport][i].ip == ssv->dip)
            {
                proto_mark = user_udp_proto_node[ssv->dport][i].type_id;
                break;
            }
        }
    }

    if(0 != proto_mark)
    {
        update_proto_mark(ssv,user_udp_proto_node[ssv->dport][i].type_id,0);
        ret = 1;
    }
    
    return ret;
}

static inline int do_tcp_port_dpi_work(struct ssn_skb_values * ssv)
{
//    if(NULL == ssv)
//        return 0;

    unsigned int proto_mark = 0;

#if 0
    if(tcp_server_port[ssv->server_port])
    {
        update_proto_mark(ssv,tcp_server_port[ssv->server_port],0);
        //ssv->ssn->proto_mark = tcp_server_port[ntohs(ssv->dport)];
        //ssv->ssn->proto_mark_state = 1;
        return 1;
    }
    if(tcp_client_port[ssv->client_port])
    {
        update_proto_mark(ssv,tcp_client_port[ssv->client_port],0);
        //ssv->ssn->proto_mark = tcp_client_port[ntohs(ssv->sport)];
        //ssv->ssn->proto_mark_state = 1;
        return 1;
    }

    return 0;
#endif
    if(ssv->isinner)
    {//uplink
        if(tcp_server_port[ssv->dport])
        {
            proto_mark = tcp_server_port[ssv->dport];
        }
        if(tcp_client_port[ssv->sport])
        {
            proto_mark = tcp_client_port[ssv->sport];
        }
    }
    else
    {//downlink
        if(tcp_server_port[ssv->sport])
        {
            proto_mark = tcp_server_port[ssv->sport];
        }
        if(tcp_client_port[ssv->dport])
        {
            proto_mark = tcp_client_port[ssv->dport];
        }
    }

    if(0 != proto_mark)
    {
        update_proto_mark(ssv, proto_mark, 0);
        return 1;
    }

    return 0;
}
static inline int do_udp_port_dpi_work(struct ssn_skb_values * ssv)
{
//    if(NULL == ssv)
//        return 0;
    unsigned int proto_mark = 0;
#if 0
    if(udp_server_port[ssv->server_port])
    {
    update_proto_mark(ssv,udp_server_port[ssv->server_port],0);
    //ssv->ssn->proto_mark = udp_server_port[ntohs(ssv->dport)];
    //ssv->ssn->proto_mark_state = 1;
    return 1;
    }
    if(udp_client_port[ssv->client_port])
    {
    update_proto_mark(ssv,udp_client_port[ssv->client_port],0);
    //ssv->ssn->proto_mark = udp_client_port[ntohs(ssv->sport)];
    //ssv->ssn->proto_mark_state = 1;
    return 1;
    }

    return 0;
#endif

    if(ssv->isinner)
    {//uplink
        if(udp_server_port[ssv->dport])
        {
            proto_mark = udp_server_port[ssv->dport];
        }
        if(udp_client_port[ssv->sport])
        {
            proto_mark = udp_client_port[ssv->sport];
        }
    }
    else
    {//downlink
        if(udp_server_port[ssv->sport])
        {
            proto_mark = udp_server_port[ssv->sport];
        }
        if(udp_client_port[ssv->dport])
        {
            proto_mark = udp_client_port[ssv->dport];
        }
    }

    if(0 != proto_mark)
    {
        update_proto_mark(ssv, proto_mark, 0);
        return 1;
    }
    
    return 0;
}

//:1:already identificatied
static inline int do_port_dpi_work(struct ssn_skb_values * ssv)
{
//    if(NULL == ssv)
//        return 0;
//    if(NULL == ssv->ssn)
//        return 0;
    
	int ret = 0;
/*
	if(ssv->ssn->proto_mark)
	{
		return ret;
	}
*/
    if(ssv->l4proto == IPPROTO_TCP)
	{
		ret = do_tcp_port_dpi_work(ssv);
	}
	else
	{
		ret = do_udp_port_dpi_work(ssv);
	}

	return ret;
}
static inline int do_dynamic_proto_work(struct ssn_skb_values *ssv)
{
//    if(NULL == ssv)
//        return 0;
//    if(NULL == ssv->ssn)
//        return 0;
    
    int ret = 0;
    //ret = detect_ftp_proto(ssv);
    //if(ret == 1) return 1;

    uint32_t  proto_mark;

//#define DYNAMIC_DEBUG


    //    l2ct_var_dpi * lvd =  ssv->ssn->vars[L2CT_VAR_DPI];
    if ((proto_mark = ftp_lookup_behavior(ssv->sip, ssv->dip, ssv->dport))) {
	 update_proto_mark(ssv,proto_mark,0);
        return 1;
    }
#if 0
    if ((proto_mark = study_lookup_behavior(ssv->sip, ssv->sport))) {
        ssv->ssn->proto_mark = proto_mark;
        D("source [%u.%u.%u.%u:%u]->[%u.%u.%u.%u:%u]proto[%x]to study cache  isinner[%d]\n", IPQUADS(ssv->sip), ntohs(ssv->sport),IPQUADS(ssv->dip), ntohs(ssv->dport), proto_mark, ssv->isinner);
        return 1;
    } else if ((proto_mark = study_lookup_behavior(ssv->dip, ssv->dport))) {
        ssv->ssn->proto_mark = proto_mark;
        D("dest [%u.%u.%u.%u:%u]->[%u.%u.%u.%u:%u]proto[%x]to study cache  isinner[%d]\n", IPQUADS(ssv->sip), ntohs(ssv->sport),IPQUADS(ssv->dip), ntohs(ssv->dport), proto_mark, ssv->isinner);
        return 1;
    }  else if ((proto_mark = study_lookup_behavior(ssv->dip, 0))) {
        ssv->ssn->proto_mark = proto_mark;
        D("dest [%u.%u.%u.%u:%u]->[%u.%u.%u.%u:%u]proto[%x]to study cache  isinner[%d]\n", IPQUADS(ssv->sip), ntohs(ssv->sport),IPQUADS(ssv->dip), ntohs(ssv->dport), proto_mark, ssv->isinner);
        return 1;
    }

#endif

    if ((proto_mark = study_lookup_behavior(ssv->sip, ssv->sport))) {
	 update_proto_mark(ssv,proto_mark,0);
#ifdef DYNAMIC_DEBUG
        LOG("source [%u.%u.%u.%u:%u]->[%u.%u.%u.%u:%u]proto[%x]to study cache  isinner[%d]\n", IPQUADS(ssv->sip), ntohs(ssv->sport),IPQUADS(ssv->dip), ntohs(ssv->dport), proto_mark, ssv->isinner);
#endif
        return 1;
    }
    if ((proto_mark = study_lookup_behavior(ssv->dip, ssv->dport))) {
        update_proto_mark(ssv,proto_mark,0);
#ifdef DYNAMIC_DEBUG
        LOG("dest [%u.%u.%u.%u:%u]->[%u.%u.%u.%u:%u]proto[%x]to study cache  isinner[%d]\n", IPQUADS(ssv->sip), ntohs(ssv->sport),IPQUADS(ssv->dip), ntohs(ssv->dport), proto_mark, ssv->isinner);
#endif
        return 1;
    }  

#if 0

    if ((proto_mark = study_lookup_behavior(ssv->server_ip, 0))) {
        ssv->ssn->proto_mark = proto_mark;
        //D("dest [%u.%u.%u.%u:%u]->[%u.%u.%u.%u:%u]proto[%x]to study cache  isinner[%d]\n", IPQUADS(ssv->sip), ntohs(ssv->sport),IPQUADS(ssv->dip), ntohs(ssv->dport), proto_mark, ssv->isinner);
        return 1;
    }

    if ((proto_mark = dynamic_lookup_behavior(ssv->server_ip, ssv->server_port)) > 0) {
        study_cache_try_get(ssv->client_ip, ssv->client_port, proto_mark);
        ssv->ssn->proto_mark = proto_mark;
        //D("dest [%u.%u.%u.%u:%u]->[%u.%u.%u.%u:%u]proto[%x]to dynamic cache  isinner[%d]\n", IPQUADS(ssv->sip), ntohs(ssv->sport), IPQUADS(ssv->dip), ntohs(ssv->dport),proto_mark, ssv->isinner);
        return 1;
    } 
    if ((proto_mark = dynamic_lookup_behavior(ssv->server_ip, 0)) > 0) {
        study_cache_try_get(ssv->client_ip, ssv->client_port, proto_mark);
        ssv->ssn->proto_mark = proto_mark;
        //D("source [%u.%u.%u.%u:%u]->[%u.%u.%u.%u:%u]proto[%x]to dynamic cache  isinner[%d]\n", IPQUADS(ssv->sip), ntohs(ssv->sport), IPQUADS(ssv->dip), ntohs(ssv->dport),proto_mark, ssv->isinner);
        return 1;
    }

#else
    if (ssv->isinner) {
#if 0
    if (str2ip("192.168.1.120") == ntohl(ssv->dip) &&str2ip("192.168.1.108") == ntohl(ssv->sip))
    {
        int aaaa = 33;
        printf("source [%u.%u.%u.%u]->[%u.%u.%u.%u] isinner[%d]    [%u]\n", IPQUADS(ssv->sip), IPQUADS(ssv->dip),  ssv->isinner, ssv->dip);
    }
#endif


        if ((proto_mark = study_lookup_behavior(ssv->dip, 0))) {
            update_proto_mark(ssv,proto_mark,0);
#ifdef DYNAMIC_DEBUG
            LOG("dest [%u.%u.%u.%u:%u]->[%u.%u.%u.%u:%u]proto[%x]to study cache  isinner[%d]\n", IPQUADS(ssv->sip), ntohs(ssv->sport),IPQUADS(ssv->dip), ntohs(ssv->dport), proto_mark, ssv->isinner);
#endif
            return 1;
        }

        if ((proto_mark = dynamic_lookup_behavior(ssv->dip, ssv->dport)) > 0) {
            if (proto_mark&0x8000) {
                study_cache_try_get(ssv->sip, ssv->sport, (proto_mark & 0xffff00ff), 0, DNS_STUDY_TIMEO);
            } else {
                study_cache_try_get(ssv->sip, ssv->sport, proto_mark, 0, 0);
            }
            update_proto_mark(ssv,proto_mark,0);
#ifdef DYNAMIC_DEBUG
            LOG("dest [%u.%u.%u.%u:%u]->[%u.%u.%u.%u:%u]proto[%x]to dynamic cache  isinner[%d]\n", IPQUADS(ssv->sip), ntohs(ssv->sport), IPQUADS(ssv->dip), ntohs(ssv->dport),proto_mark, ssv->isinner);
#endif
            return 1;
        } 
        if ((proto_mark = dynamic_lookup_behavior(ssv->dip, 0)) > 0) {
            if (proto_mark&0x8000) {
                study_cache_try_get(ssv->sip, ssv->sport, (proto_mark & 0xffff00ff), 0, DNS_STUDY_TIMEO);
            } else {
                study_cache_try_get(ssv->sip, ssv->sport, proto_mark, 0, 0);
            }
            update_proto_mark(ssv,proto_mark,0);
#ifdef DYNAMIC_DEBUG
            LOG("source [%u.%u.%u.%u:%u]->[%u.%u.%u.%u:%u]proto[%x]to dynamic cache  isinner[%d]\n", IPQUADS(ssv->sip), ntohs(ssv->sport), IPQUADS(ssv->dip), ntohs(ssv->dport),proto_mark, ssv->isinner);
#endif
            return 1;
        }

    } else {

        if ((proto_mark = study_lookup_behavior(ssv->sip, 0))) {
            update_proto_mark(ssv,proto_mark,0);
#ifdef DYNAMIC_DEBUG
            LOG("dest [%u.%u.%u.%u:%u]->[%u.%u.%u.%u:%u]proto[%x]to study cache  isinner[%d]\n", IPQUADS(ssv->sip), ntohs(ssv->sport),IPQUADS(ssv->dip), ntohs(ssv->dport), proto_mark, ssv->isinner);
#endif
            return 1;
        }

        if ((proto_mark = dynamic_lookup_behavior(ssv->sip, ssv->sport)) > 0) {
            if (proto_mark&0x8000) {
                study_cache_try_get(ssv->dip, ssv->dport, (proto_mark & 0xffff00ff), 0, DNS_STUDY_TIMEO);
            } else {
                study_cache_try_get(ssv->dip, ssv->dport, proto_mark, 0, 0);
            }
           update_proto_mark(ssv,proto_mark,0);
#ifdef DYNAMIC_DEBUG
            LOG("dest [%u.%u.%u.%u:%u]->[%u.%u.%u.%u:%u]proto[%x]to dynamic cache  isinner[%d]\n", IPQUADS(ssv->sip), ntohs(ssv->sport), IPQUADS(ssv->dip), ntohs(ssv->dport),proto_mark, ssv->isinner);
#endif
            return 1;
        } 
        if ((proto_mark = dynamic_lookup_behavior(ssv->sip, 0)) > 0) {
            if (proto_mark&0x8000) {
                study_cache_try_get(ssv->dip, ssv->dport, (proto_mark & 0xffff00ff), 0, DNS_STUDY_TIMEO);
            } else {
                study_cache_try_get(ssv->dip, ssv->dport, proto_mark, 0, 0);
            }
           update_proto_mark(ssv,proto_mark,0);
#ifdef DYNAMIC_DEBUG
            LOG("source [%u.%u.%u.%u:%u]->[%u.%u.%u.%u:%u]proto[%x]to dynamic cache  isinner[%d]\n", IPQUADS(ssv->sip), ntohs(ssv->sport), IPQUADS(ssv->dip), ntohs(ssv->dport),proto_mark, ssv->isinner);
#endif
            return 1;
        }

    }
#endif
    return 0;
}


static inline int do_tuple_dpi_work(struct ssn_skb_values * ssv)
{
//    if(NULL == ssv)
//        return 0;
    
    if(do_user_proto_work(ssv) == 1)//:1:already identificatied,full user proto define
    {
        return 1;
    }

    if(do_user_proto_node_work(ssv) == 1)//:1:already identificatied,exist proto user define
    {
        return 1;
    }
#if 1
    if(get_flags_dynamic_enable()&&do_dynamic_proto_work(ssv) == 1)//:1:already identificatied,exist proto user define
    {
        return 1;
    }
#endif
    if(do_port_dpi_work(ssv) == 1)//:1:already identificatied,pattern only has port
    {
        return 1;
    }

    return 0;
}


static inline int decode_skb(struct rte_mbuf  * skb, struct ssn_skb_values * ssv) 
{
//    if(NULL == skb || NULL == ssv)
//        return -1;
    
    struct ether_hdr *eth_hdr;
    struct tcp_hdr *tcp;
    struct udp_hdr *udp;
    uint16_t l4_len;
    
    eth_hdr = rte_pktmbuf_mtod(skb, struct ether_hdr *);
//     if(NULL == eth_hdr)
//        return -1;
     
    uint16_t ether_type = eth_hdr->ether_type;
    size_t vlan_offset = get_vlan_offset(eth_hdr, &ether_type);

    struct ipv4_hdr *ip_hdr = (struct ipv4_hdr *)((char *)(eth_hdr + 1) + vlan_offset);
//    if(NULL == ip_hdr)
//        return -1;

    ssv->skb = skb;

    ssv->sip = ip_hdr->src_addr;
    ssv->dip = ip_hdr->dst_addr;
//    if (503425216 == ssv->sip)
//        printf("[%u.%u.%u.%u->%u.%u.%u.%u][id:%d][ttl:%u]\n", IPQUADS(ssv->sip),  IPQUADS(ssv->dip),  ntohs(ip_hdr->packet_id), ip_hdr->time_to_live);
    ssv->isinner = isinner_by_portid(skb->in_port);
    //ssv->isinner = !(skb->in_port & 1);
    if (unlikely(rte_ipv4_frag_pkt_is_fragmented(ip_hdr))) 
    {
        if(decode_skb_ipfrag(skb, ssv, ssv->ssn) < 0)
        {
            return -1;
        }
    } 
    else
    {
        switch (ip_hdr->next_proto_id) 
        {
            case IPPROTO_TCP:
                tcp = (struct tcp_hdr *)((unsigned char *)ip_hdr + (ip_hdr->version_ihl & 0x0f) * 4);
                ssv->sport = tcp->src_port;
                ssv->dport = tcp->dst_port;
#if 0
                if (ssv->isinner) {
                ssv->client_ip = ip_hdr->src_addr;
                ssv->server_ip = ip_hdr->dst_addr;
                ssv->client_port = tcp->src_port;
                ssv->server_port = tcp->dst_port;
                } else {
                ssv->server_ip = ip_hdr->src_addr;
                ssv->client_ip = ip_hdr->dst_addr;
                ssv->server_port = tcp->src_port;
                ssv->client_port = tcp->dst_port;
                }
#endif
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
                if (ssv->payload_len && ssv->ssn->proto_mark == pv.syn_ack) {
                    ssv->ssn->proto_mark = 0;
                }
                break;

            case IPPROTO_UDP:
                udp = (struct udp_hdr *)((unsigned char *)ip_hdr + (ip_hdr->version_ihl & 0x0f) * 4);
                ssv->sport = udp->src_port;
                ssv->dport = udp->dst_port;
                ssv->payload = (char *)udp + sizeof(struct udp_hdr);
                ssv->payload_len = rte_cpu_to_be_16(ip_hdr->total_length) - 
                                              ((ip_hdr->version_ihl & 0x0f) * 4) - sizeof(struct udp_hdr);
#if 0
                if (ssv->isinner) {
                ssv->client_ip = ip_hdr->src_addr;
                ssv->server_ip = ip_hdr->dst_addr;
                ssv->client_port = udp->src_port;
                ssv->server_port = udp->dst_port;
                } else {
                ssv->server_ip = ip_hdr->src_addr;
                ssv->client_ip = ip_hdr->dst_addr;
                ssv->server_port = udp->src_port;
                ssv->client_port = udp->dst_port;
                }
#endif
                break;

            default:
                return -1;
        }
        ssv->l4proto =  ip_hdr->next_proto_id;
    }
    
    return 0;
}

static inline int judge_http_request(char * p)
{
//    if(NULL == p)
//        return 0;
    
    if(p[1] == 'E')
    {
        if(p[0] == 'G' && p[2] == 'T' && p[3] == ' ')
        {
            return 1;
        }
        if(p[0] == 'H' && p[2] == 'A' && p[3] == 'D' && p[4] == ' ')
        {
            return 1;
        }
    }
    else if(p[0] == 'P' && p[1] == 'O' && p[2] == 'S' && p[3] == 'T' && p[4] == ' ')
    {
        return 1;
    }
    else if(p[0] == 'O' && p[1] == 'P' && p[2] == 'T' && p[3] == 'I' && p[4] == 'O' && p[5] == 'N' && p[6] == 'S' && p[7] == ' ')
    {
        return 1;
    } 
    return 0;
}

static inline int judge_http_respose(char * p)
{
//    if(NULL == p)
//        return 0;
    
    if(p[0] == 'H' && p[1] == 'T' && p[2] == 'T' && p[3] == 'P' && p[4] == '/' && 
        p[5] == '1' && p[6] == '.' && (p[7] == '1' || p[7] == '0') && p[8] == ' ')
    {
        return 1;
    }
    
    return 0;
}

static inline int judge_http(char * payload, struct sft_fdb_entry * ssn, struct ssn_skb_values * ssv)
{
//    if((NULL == ssn) || (NULL == ssv))
//        return 0;
    
    //    printf("ssn->proto_mark=%x\n",ssn->proto_mark);
    if((ssn->proto_mark&0xF0000000) == (pv.http_proto_appid&0xF0000000))
    {
        return 1;
    }
    
    if(ssv->isinner)
    {
        if(ssv->payload_len > 5 && judge_http_request(payload))
        {
            if(ssn->proto_mark == 0)
            {
                update_proto_mark(ssv, pv.http_proto_appid,0);
                ssn->max_pkt_cnt = HTTP_MAX_PKT_NUM;
            }
            return 1;
        }
    }
    else
    {
        if(ssv->payload_len > 9 && judge_http_respose(payload))
        {
             if(ssn->proto_mark == 0)
             {
                update_proto_mark(ssv,pv.http_proto_appid,0);
                ssn->max_pkt_cnt = HTTP_MAX_PKT_NUM;
             }
  
            return 1;
        }
    }
    
    return 0;
}


static int deal_search_httptoken(void * _pattern, int offset, void * data, void * arg)
{
	pattern_t * pattern = (pattern_t *)_pattern;
	struct ssn_skb_values * ssv = (struct ssn_skb_values *)arg;

	if(pattern->id < FIRSTN_TOKEN)
	{//place pattern
		/*
		if(ssv->place_token_list_cnt >= (TOKEN_MAX-1))
		{
			return 0;
		}

		ssv->place_token_list[ssv->place_token_list_cnt] = pattern->id;
		ssv->place_token_list_cnt++;
		*/
		switch(pattern->id)
		{
			case HTTP_TOKEN_GET_ID:
				ssv->http_token_method = HTTP_TOKEN_GET;
				break;
			case HTTP_TOKEN_HEAD_ID:
				ssv->http_token_method = HTTP_TOKEN_HEAD;
				break;
			case HTTP_TOKEN_POST_ID:
				ssv->http_token_method = HTTP_TOKEN_POST;
				break;
			case HTTP_TOKEN_LOCATION_ID:
				ssv->http_token_method = HTTP_TOKEN_LOCATION;
				break;

			case HTTP_TOKEN_HOST_ID:
				ssv->http_token_host_offset = offset;
				break;
			default:
				break;
		}
	}
	return 0;
}

static inline int get_weibo_username(char *payload, int len, char *username, int ulen)
{
    int pos = 0;
    int index = 0;
    int minlen = 0;
    int ret = 0;
    while(index < len)
    {
        pos = substr_in_mainstr_nocase(payload + index, len - index, "Set-Cookie:", 0);
        if(pos < 0)
        {
            break;
        }
        index += pos;
        if(index >= len)
        {
            break;
        }
        
        pos = substr_in_mainstr_nocase(payload + index, len - index, "%26name%3D", '\r');
        if(pos < 0)
        {
            continue;
        }
        index += pos;
        if(index >= len)
        {
            break;
        }
        
        pos = substr_in_mainstr_nocase(payload + index, len - index, "%26", '\r');
        if(pos < 0)
        {
            pos = substr_in_mainstr_nocase(payload + index, len - index, "\r", 0);
            if(pos < 0)
            {
                break;
            }
        }
        
        minlen = ((ulen - 1) < pos) ? (ulen - 1) : (pos - 1);
        
        memcpy(username, payload + index, minlen);
        urldecode(username, minlen);
        urldecode(username, minlen);
        
        return strlen(username);
    }
    
    return 0;
}

static inline int get_tieba_username(char *payload, int len, char *username, int ulen)
{
    int pos = 0;
    int index = 0;
    int minlen = 0;
    int ret = 0;

    pos = substr_in_mainstr_nocase(payload + index, len - index, "&userName=", '\r');
#if 0    
    if(pos < 0)
    {
        pos = substr_in_mainstr_nocase(payload + index, len - index, "&uname=", '\r');
    }
#endif
    
    if(pos > 0)
    {
        index += pos;
        if(index < len)
        {
            pos = substr_in_mainstr_nocase(payload + index, len - index, "&", '\r');
            if(pos < 0)
            {
                pos = substr_in_mainstr_nocase(payload + index, len - index, "\r", 0);
                if(pos < 0)
                {
                    return 0;
                }
            }
            minlen = ((ulen - 1) < pos) ? (ulen - 1) : (pos - 1);
            memcpy(username, payload + index, minlen);
            urldecode(username, minlen);
            return strlen(username);
        }
    }
    
    
    
    return 0;
}

/* return: 1 success, 0 nothing, -1 err */
static inline int export_log(struct ssn_skb_values * ssv, struct sft_fdb_entry * ssn,int acsmflag, uint32_t lcore_id)
{
	if(pv.syslog_sd == 0)
	{
		return 0;
	}
//all uplink data
	if((ssn->proto_mark&0xF0000000) == (pv.http_proto_appid&0xF0000000) && ssn->log_flag == 0)
    {
		if(!pv.urlog)
		{
			return 0;
		}
#if 0
        if(ssn->log == NULL)
		{
            if(unlikely((ssn->log = get_log_mem(lcore_id)) == NULL)) 
                return -1; 
		}
#endif
		if(acsmflag == 0)
		{
			if(!pv.acsm_enable)
			{
				return 0;
			}
			acsmSearch2(pv._acsm[REQ_HTTP_URL], ssv->payload, ssv->payload_len, deal_search_httptoken, NULL, ssv);
		}
#if 0
        if(export_url_log(ssv, (struct log_s *)ssn->log) < 0)
            return -1;
        put_log_mem(ssn->log, lcore_id);
#endif
        export_url_log(ssv);
        ssn->log_flag = 1;
    }
    else if(ssn->proto_mark == pv.qq_login_appid && ssn->log_flag == 0)
    {
		if(!pv.qqlog)
		{
			return 0;
		}
#if 0
        if(ssn->log == NULL)
		{
            if(unlikely((ssn->log = get_log_mem(lcore_id)) == NULL)) 
                return -1; 
		}
        if(export_qq_log(ssv, (struct log_s *)ssn->log) < 0)
            return -1;
        put_log_mem(ssn->log, lcore_id);
#endif
        export_qq_log(ssv);
        ssn->log_flag = 1;
    }

#if 0
    else if(ssn->proto_mark == pv.weibo_appid && ssn->log_flag == 0)
    {
        int ret = 0;
        char username[128];
        memset(username, 0, sizeof(username));
        if((ret = get_weibo_username(ssv->payload, ssv->payload_len, username, sizeof(username))) > 0)
        {
            export_weibo_log(ssv, username, ret, lcore_id);
            ssn->log_flag = 1;
        }
    }
    else if(ssn->proto_mark == pv.tieba_appid)
    {
        int ret = 0;
        char username[128];
        memset(username, 0, sizeof(username));

        if((ret = get_tieba_username(ssv->payload, ssv->payload_len, username, sizeof(username))) > 0)
        {
            export_tieba_log(ssv, username, ret, lcore_id);
        }
    }
#endif
    return 1;
}

#if 1
static inline void amend_unknown_stream_for_others(struct ssn_skb_values *ssv)
{	
    struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(ssv->skb, struct ether_hdr *);
    uint16_t ether_type = eth_hdr->ether_type;
    size_t vlan_offset = get_vlan_offset(eth_hdr, &ether_type);
   time_t curr_time = 0; 

    struct ipv4_hdr * ip_hdr = (struct ipv4_hdr *)((char *)(eth_hdr + 1) + vlan_offset);
    if(!ip_hdr)
    {
        return;
    }

   uint64_t skb_length = (uint64_t)rte_pktmbuf_pkt_len(ssv->skb);

    //amend unknown_80
    if ( (20480 == ssv->ssn->sess_key.port_src || 20480 == ssv->ssn->sess_key.port_dst) &&
          (APP_CLASS_ID_NOT_DEFINED != pv.unknown_80_appid) )
    {
        update_proto_mark(ssv, pv.unknown_80_appid, 0);
        //printf("update !! admend unknown unknown_80_appid\n");
        return;
    }
	
    //amend unknown udp
    if (IPPROTO_UDP == ip_hdr->next_proto_id) 
    {
/*
        curr_time = time(NULL);
        //printf("skb_length = %d, time = %d  - %d - %d\n", skb_length, curr_time, p->begin_timestamp, curr_time-p->begin_timestamp);
        if(curr_time - ssv->ssn->vars_l2ct.begin_timestamp < 30)
        {
            return;
        }
*/      
        if(ssv->ssn->vars_l2ct.unknow_udp_reidentify_num > 100)
        {
            return;
        }
	
        ssv->ssn->vars_l2ct.unknow_udp_reidentify_num++;

        //length < 250 is udp_application_mutual
        if ( (skb_length < 250) && (APP_CLASS_ID_NOT_DEFINED != pv.udp_application_mutual) )
        {
            ssv->ssn->vars_l2ct.unknow_udp_app_mutual_num++;
        }
        //length > 1024  is udp_video_download
        else if ( (skb_length > 1024) && (APP_CLASS_ID_NOT_DEFINED != pv.udp_video_download) )
        {
            ssv->ssn->vars_l2ct.unknow_udp_video_download++;
        }
#if 0
        //length belong to [250,1024) is unknown
        else if ( skb_length>= 250 && skb_length < 1024 )
        {
            return;
        }
#endif
        if(100 == ssv->ssn->vars_l2ct.unknow_udp_reidentify_num)
        {
            if(ssv->ssn->vars_l2ct.unknow_udp_app_mutual_num > 50)
            {
                update_proto_mark(ssv, pv.udp_application_mutual, 0);
                //printf("update !! admend unknown unknow_udp_reidentify_num[%u], unknow_udp_app_mutual_num [%u]\n",
                 //   ssv->ssn->vars_l2ct.unknow_udp_reidentify_num, ssv->ssn->vars_l2ct.unknow_udp_app_mutual_num);
            }
            else if(ssv->ssn->vars_l2ct.unknow_udp_video_download > 50)
            {
                update_proto_mark(ssv, pv.udp_video_download, 0);
                //printf("update !! admend unknown unknow_udp_reidentify_num[%u], unknow_udp_video_download [%u]\n",
                   // ssv->ssn->vars_l2ct.unknow_udp_reidentify_num, ssv->ssn->vars_l2ct.unknow_udp_video_download);
            }
         }

        return;
    }

    //amend unknown tcp
    if (IPPROTO_TCP == ip_hdr->next_proto_id) 
    {

        curr_time = time(NULL);
        //printf("skb_length = %d, time = %d  - %d - %d\n", skb_length, curr_time, p->begin_timestamp, curr_time-p->begin_timestamp);
        if(curr_time - ssv->ssn->vars_l2ct.begin_timestamp < 30) 
        {
            return;
        }
      
        if(ssv->ssn->vars_l2ct.unknow_tcp_reidentify_num > 100)
        {
            return;
        }
         
        ssv->ssn->vars_l2ct.unknow_tcp_reidentify_num++;

        
        //length < 250 is tcp_application_mutual
        if ( (skb_length < 250) && (APP_CLASS_ID_NOT_DEFINED != pv.tcp_application_mutual) )
        {
            ssv->ssn->vars_l2ct.unknow_tcp_app_mutual_num++;
        }
        //length > 1024  is tcp_video_download
        else if ( (skb_length > 1024) && (APP_CLASS_ID_NOT_DEFINED != pv.tcp_application_mutual) )
        {
            ssv->ssn->vars_l2ct.unknow_tcp_video_download++;
        }
#if 0			
        //length belong to [250,1024) is unknown
        else if ( skb_length>= 250 && skb_length < 1024 )
        {
            return;
        }
#endif
            
        if(100 == ssv->ssn->vars_l2ct.unknow_tcp_reidentify_num)
        {
            if(ssv->ssn->vars_l2ct.unknow_tcp_app_mutual_num > 50)
            {
                update_proto_mark(ssv, pv.tcp_application_mutual, 0);
               // printf("update !! admend unknown unknow_tcp_reidentify_num[%u], unknow_tcp_app_mutual_num [%u]\n",
                  //  ssv->ssn->vars_l2ct.unknow_tcp_reidentify_num, ssv->ssn->vars_l2ct.unknow_tcp_app_mutual_num);
            }
            
            if(ssv->ssn->vars_l2ct.unknow_tcp_video_download > 50)
            {
                update_proto_mark(ssv, pv.tcp_video_download, 0);
               // printf("update !! admend unknown unknow_tcp_reidentify_num[%u], unknow_tcp_video_download [%u]\n",
                  //  ssv->ssn->vars_l2ct.unknow_tcp_reidentify_num, ssv->ssn->vars_l2ct.unknow_tcp_video_download);
            }
        }
        
        return;
    }

    return;
}
#endif

#if 1
/* ---  "Original" uTP Header ("version 0" ?) --------------

See utp.cpp source code @ https://github.com/bittorrent/libutp

-- Fixed Header --
0       4       8               16              24              32
+-------+-------+---------------+---------------+---------------+
| connection_id                                                 |
+-------+-------+---------------+---------------+---------------+
| timestamp_seconds                                             |
+---------------+---------------+---------------+---------------+
| timestamp_microseconds                                        |
+---------------+---------------+---------------+---------------+
| timestamp_difference_microseconds                             |
+---------------+---------------+---------------+---------------+
| wnd_size      | ext           | flags         | seq_nr [ho]   |
+---------------+---------------+---------------+---------------+
| seq_nr [lo]   | ack_nr                        |
+---------------+---------------+---------------+

-- Extension Field(s) --
0               8               16
+---------------+---------------+---------------+---------------+
| extension     | len           | bitmask
+---------------+---------------+---------------+---------------+
                                |
+---------------+---------------+....

*/

/* --- Version 1 Header ----------------

Specifications: BEP-0029
http://www.bittorrent.org/beps/bep_0029.html

-- Fixed Header --
Fields Types
0       4       8               16              24              32
+-------+-------+---------------+---------------+---------------+
| type  | ver   | extension     | connection_id                 |
+-------+-------+---------------+---------------+---------------+
| timestamp_microseconds                                        |
+---------------+---------------+---------------+---------------+
| timestamp_difference_microseconds                             |
+---------------+---------------+---------------+---------------+
| wnd_size                                                      |
+---------------+---------------+---------------+---------------+
| seq_nr                        | ack_nr                        |
+---------------+---------------+---------------+---------------+

-- Extension Field(s) --
0               8               16
+---------------+---------------+---------------+---------------+
| extension     | len           | bitmask
+---------------+---------------+---------------+---------------+
                                |
+---------------+---------------+....
*/
static inline void enhance_encrypt_stream_for_bt(struct ssn_skb_values *ssv)
{
    if((ntohs(ssv->sport) < 1024) || (ntohs(ssv->dport) < 1024) || ssv->payload_len < 23 /* min header size */ )
    {
        return;
    }

    /*
    Check for uTP http://www.bittorrent.org/beps/bep_0029.html

    wireshark/epan/dissectors/packet-bt-utp.c
    */
     //way1
    const char *bt_search = "BT-SEARCH * HTTP/1.1\r\n";
    if(strncmp((const char*)ssv->payload, bt_search, strlen(bt_search)) == 0) 
    {
        update_proto_mark(ssv, pv.enhance_encrypt_bt, 0);
        return;
    }

    
    //way2
    if((ssv->payload[0]== 0x60)
    && (ssv->payload[1]== 0x0)
    && (ssv->payload[2]== 0x0)
    && (ssv->payload[3]== 0x0)
    && (ssv->payload[4]== 0x0)) 
    {
        update_proto_mark(ssv, pv.enhance_encrypt_bt, 0);
        return;
    }

     //way3
    /* Check if this is protocol v1 */
    uint8_t v1_version     = ssv->payload[0];
    uint8_t v1_extension   = ssv->payload[1];
    uint32_t v1_window_size = *((uint32_t*)&ssv->payload[12]);
    if(((v1_version & 0x0f) == 1)
    && ((v1_version >> 4) < 5 /* ST_NUM_STATES */)
    && (v1_extension      < 3 /* EXT_NUM_EXT */)
    && (v1_window_size    < 32768 /* 32k */)
    ) 
    {
        update_proto_mark(ssv, pv.enhance_encrypt_bt, 0);
        return;
    } 

    //way4
    /* Check if this is protocol v0 */
    uint8_t v0_extension = ssv->payload[17];
    uint8_t v0_flags     = ssv->payload[18];
    if((v0_flags < 6 /* ST_NUM_STATES */) && (v0_extension < 3 /* EXT_NUM_EXT */)) 
    {
        uint32_t ts = ntohl(*((uint32_t*)&(ssv->payload[4])));
        uint32_t now;

        now = (uint32_t)time(NULL);

        if((ts < (now+86400)) && (ts > (now-86400))) 
        {
            update_proto_mark(ssv, pv.enhance_encrypt_bt, 0);
            return;
        }
     }

    return;
}
#endif
static int max_pkt_cnt = MAX_PKT_NUM;
int do_dpi_entry(struct rte_mbuf *skb, unsigned portid, unsigned lcore_id)
{
    int acsmflag = 0;
    if(unlikely(pv.dpi_ctrl != DPI_ENABLE)) 
        goto do_dpi_entry_end;
   
    if(!test_bit(0, &skb->vars))
        goto do_dpi_entry_end;
  
    struct sft_fdb_entry * ssn = (struct sft_fdb_entry *)skb->l2ct;
    if(unlikely(ssn == NULL)) 
        goto do_dpi_entry_end;

    if(ssn->magic != 'X') 
        goto do_dpi_entry_end;

    if ((ssn->proto_mark &0x3) == 0x1) {
        goto do_dpi_entry_end;
    }
    struct ssn_skb_values ssv; 

    bzero(&ssv,sizeof(struct ssn_skb_values));
    ssv.http_token_method = HTTP_TOCKE_INVALID;

    ssv.ssn = ssn;
   
    if(decode_skb(skb, &ssv) < 0)
        goto do_dpi_entry_end;
    if (ssn->pkt_cnt >= ssn->max_pkt_cnt && ssv.sport != 0x3500 && (ssn->proto_mark&0x3) != 0x3) {
        //reidentify unknow for  others:
        //unknown_80_appid, udp_application_mutual, udp_video_download,tcp_application_mutual,tcp_video_download
        if (!ssn->proto_mark) {
            amend_unknown_stream_for_others(&ssv);
        }
        goto do_dpi_entry_end;
    }

    if(do_tuple_dpi_work(&ssv) == 1 && (ssn->proto_mark&0x3) == 0x1)//:1:already identificatied
    {
        goto do_dpi_entry_end;
    } 

    if(ssv.payload_len == 0)//syn+ack
    {
        goto do_dpi_entry_end;
    }

    ssn->pkt_cnt ++;

    if(IPPROTO_TCP == ssv.l4proto && ssv.ssn->is_http == 0) 
    {
        ssv.ssn->is_http = judge_http(ssv.payload, ssn,&ssv);
    }
    dpi_acsm_search(&ssv);
    acsmflag = 1;

    //study the ftp
    if(pv.ftp_proto_appid == ssn->proto_mark)
    {
        detect_ftp_proto(&ssv);
    }

    //fixed length dpi 
    if (!(ssn->proto_mark))
    {
        identify_fixed_length_packet(&ssv);
    }

#if 0
    if(0==ssn->proto_mark && IPPROTO_UDP == ssv.l4proto)
    {
        enhance_encrypt_stream_for_bt(&ssv);
    }
#endif

    export_log(&ssv, ssn, acsmflag ,lcore_id);

    do_dpi_entry_end:
        return 1;
}
