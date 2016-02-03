#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <rte_ring.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_timer.h>
#include <rte_mempool.h>
#include <rte_spinlock.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>

#include "sft_statictics.h"
#include "app_class_tbl.h"
#include "init.h"
#include "debug.h"
#include "session_mgr.h"
#include "config.h"
#include "export.h"
#include "utils.h"
#include "global.h"

uint8_t statictics_flag = 0;
//extern struct dpi_statistics_struct ***DPI_Statistics;
struct dpi_statistics_struct (*DPI_Statistics)[16][256];
char *static_buf = NULL;
//extern uint8_t dpi_ctrl;

//hook at send dir
#if 0
unsigned int get_amend_unknown_skb_proto_mark(struct sft_fdb_entry * ssn, 
			uint64_t skb_length, uint8_t  proto_id)
{	
	time_t curr_time = 0; 
	struct l2ct_private_struct *p = NULL;
	
	if (0 != ssn->proto_mark) 
		return ssn->proto_mark;

	//amend unknown_80
	if ( (80 == ntohs(ssn->sess_key.port_src) || 80 == ntohs(ssn->sess_key.port_dst)) && (APP_CLASS_ID_NOT_DEFINED != pv.unknown_80_appid) )
	{
		return	pv.unknown_80_appid;
	}

	p = &ssn->vars_l2ct;
	
	//amend unknown udp
	if (IPPROTO_UDP == proto_id) 
	{
		curr_time = time(NULL);
		//printf("skb_length = %d, time = %d  - %d - %d\n", skb_length, curr_time, p->begin_timestamp, curr_time-p->begin_timestamp);
		if(curr_time - p->begin_timestamp > 30) 
		{	
			//length < 250 is udp_application_mutual
			if ( (skb_length < 250) && (APP_CLASS_ID_NOT_DEFINED != pv.udp_application_mutual) )
			{
				return pv.udp_application_mutual;
			}
			
			//length belong to [250,1024) is unknown
			if ( skb_length>= 250 && skb_length < 1024 )
			{
				return 0;
			}

			//length > 1024  is udp_video_download
			if ( (skb_length > 1024) && (APP_CLASS_ID_NOT_DEFINED != pv.udp_video_download) )
			{
				return pv.udp_video_download;
			}
		}
	}
	else if (IPPROTO_TCP == proto_id) 
	{
		curr_time = time(NULL);
		//printf("skb_length = %d, time = %d  - %d - %d\n", skb_length, curr_time, p->begin_timestamp, curr_time-p->begin_timestamp);
		if(curr_time - p->begin_timestamp > 30) 
		{	
			//length < 250 is tcp_application_mutual
			if ( (skb_length < 250) && (APP_CLASS_ID_NOT_DEFINED != pv.tcp_application_mutual) )
			{
				return pv.tcp_application_mutual;
			}
			
			//length belong to [250,1024) is unknown
			if ( skb_length>= 250 && skb_length < 1024 )
			{
				return 0;
			}

			//length > 1024  is tcp_video_download
			if ( (skb_length > 1024) && (APP_CLASS_ID_NOT_DEFINED != pv.tcp_application_mutual) )
			{
				return pv.tcp_video_download;
			}
		}
	}
       
	return ssn->proto_mark;
}
#endif
int sft_do_statistics_flow_work(struct rte_mbuf *skb) 
{ 
	int ifindex;
    int is_inner_dev;
	int sub_type;
	int mid_type;
	int big_type;
	int prev_sub_type;
	int prev_mid_type;
	int prev_big_type;
    int update_flow = 1;
    int update_link = 0;

    struct sft_fdb_entry * ssn;
	struct ipv4_hdr *ip_hdr;
	uint32_t skb_proto_mark = 0;
    
//    if(!test_bit(0, &skb->vars))
//        return 0;

//    if(pv.dpi_ctrl != DPI_ENABLE) 
//		return 0;
    
//    if(!statictics_flag)
//        return 0;
	struct ether_hdr *eth_hdr;
    eth_hdr = rte_pktmbuf_mtod(skb, struct ether_hdr *);
    uint16_t ether_type = eth_hdr->ether_type;
    size_t vlan_offset = get_vlan_offset(eth_hdr, &ether_type);
    
    ip_hdr = (struct ipv4_hdr *)((char *)(eth_hdr + 1) + vlan_offset);
	if(!ip_hdr)
	{
		return 0;
	}
	ssn = (struct sft_fdb_entry *)skb->l2ct;
	if(!ssn) //ssn == NULL
	{
	       /*connectionless_tcp*/		
		if ( (IPPROTO_TCP == ip_hdr->next_proto_id) && (APP_CLASS_ID_NOT_DEFINED != pv.connectionless_tcp_appid) )
		{
			skb_proto_mark = pv.connectionless_tcp_appid;
		}
		else
		{
			return 0;
		}
	}
	else//ssn != NULL
	{ 
		if (ssn->pkt_cnt < MAX_PKT_NUM && DPI_ENABLE == pv.dpi_ctrl) 
		{//first MAX_PKT_NUM
			if(ssn->proto_mark == 0 || !(ssn->proto_mark ^ ssn->init_proto_mark))
			{
                update_flow = 0;
				//return 0;
			}
		}
#if 1
            skb_proto_mark = ssn->proto_mark;
#else
		
		if (0 != ssn->proto_mark) //already known
		{
			skb_proto_mark = ssn->proto_mark;
		}
		else //amend unknown
		{
			skb_proto_mark = get_amend_unknown_skb_proto_mark(ssn, (uint64_t)rte_pktmbuf_pkt_len(skb), ip_hdr->next_proto_id);
		}
#endif
       }
	
#if 0
	if (pv.udp_video_download == skb_proto_mark)
	{
		struct in_addr addr_src, addr_dst;
		memset(&addr_src,0,sizeof addr_src);
		memset(&addr_dst,0,sizeof addr_dst);
		memcpy(&addr_src, &ssn->ip_src, 4);
		memcpy(&addr_dst, &ssn->ip_dst, 4);
		printf("[%s][%d]---->", inet_ntoa(addr_src), ntohs(ssn->port_src));
		printf("[%s][%d]\n",  inet_ntoa(addr_dst), ntohs(ssn->port_dst));
	}
#endif
	big_type = (skb_proto_mark & BIG_TYPE_MASK) >> 28;
	mid_type = (skb_proto_mark & MID_TYPE_MASK) >> 24;
	sub_type = (skb_proto_mark & SUB_TYPE_MASK) >> 16;
    if (ssn) {
        prev_big_type = (ssn->prev_proto_mark & BIG_TYPE_MASK) >> 28;
        prev_mid_type = (ssn->prev_proto_mark & MID_TYPE_MASK) >> 24;
        prev_sub_type = (ssn->prev_proto_mark & SUB_TYPE_MASK) >> 16;
    }
    uint64_t current_time = rte_rdtsc();
    int64_t link;

    if (!isinner_by_portid(skb->in_port))
    {//downlink
        if (update_flow)
            rte_atomic64_add( &DPI_Statistics[big_type][mid_type][sub_type].down, (uint32_t)rte_pktmbuf_pkt_len(skb));
        if (ssn && (ssn->proto_mark ^ ssn->prev_proto_mark || current_time - ssn->link_timestamp >= 5 * pv.hz )) {

            rte_atomic64_inc(&DPI_Statistics[big_type][mid_type][sub_type].link);
            
            if (ssn->proto_mark ^ ssn->prev_proto_mark) {
                ssn->prev_proto_mark = ssn->proto_mark;
                link = rte_atomic64_read(&DPI_Statistics[prev_big_type][prev_mid_type][prev_sub_type].link);
                if (link > 0)
                    rte_atomic64_dec(&DPI_Statistics[prev_big_type][prev_mid_type][prev_sub_type].link);
            } else {
                ssn->link_timestamp = current_time;
            }
            update_link = 1;
        }

        if(big_type == 0)
            goto FUNC_END;

        if(sub_type != 255)
        {
            if (update_flow)
                rte_atomic64_add(&DPI_Statistics[big_type][mid_type][255].down, (uint32_t)rte_pktmbuf_pkt_len(skb));
            if (update_link == 1) {
                rte_atomic64_inc( &DPI_Statistics[big_type][mid_type][255].link);
            }

        }
        if(mid_type == 15)
        {
            if(sub_type != 255)
            {
                if (update_flow)
                    rte_atomic64_add(&DPI_Statistics[big_type][15][255].down, (uint32_t)rte_pktmbuf_pkt_len(skb)); 
                if (update_link == 1) {
                    rte_atomic64_inc( &DPI_Statistics[big_type][15][255].link);
                }
            }
        }
        else
        {
            if (update_flow)
                rte_atomic64_add( &DPI_Statistics[big_type][15][255].down, (uint32_t)rte_pktmbuf_pkt_len(skb));
            if (update_link == 1) {
                rte_atomic64_inc( &DPI_Statistics[big_type][15][255].link);
            }
        }
    }
	else
	{//uplink

        if (update_flow)
            rte_atomic64_add(&DPI_Statistics[big_type][mid_type][sub_type].up,(uint32_t)rte_pktmbuf_pkt_len(skb));
        if (ssn && (ssn->proto_mark ^ ssn->prev_proto_mark || current_time - ssn->link_timestamp >= 5 * pv.hz )) {
           
            rte_atomic64_inc(&DPI_Statistics[big_type][mid_type][sub_type].link);

            if (ssn->proto_mark ^ ssn->prev_proto_mark && link > 0) {
                ssn->prev_proto_mark = ssn->proto_mark;
                link = rte_atomic64_read(&DPI_Statistics[prev_big_type][prev_mid_type][prev_sub_type].link);
                if (link >0)
                    rte_atomic64_dec(&DPI_Statistics[prev_big_type][prev_mid_type][prev_sub_type].link);
            } else {
                ssn->link_timestamp = current_time;
            }
            update_link = 1;
        }


		if(big_type == 0)
			goto FUNC_END;
        if(sub_type != 255)
		{
            if (update_flow)
			    rte_atomic64_add(&DPI_Statistics[big_type][mid_type][255].up, (uint32_t)rte_pktmbuf_pkt_len(skb));
            if (update_link == 1) {
                rte_atomic64_inc( &DPI_Statistics[big_type][mid_type][255].link);
            }
		}
		if(mid_type == 15)
		{
			if(sub_type != 255)
			{
                if (update_flow)
				    rte_atomic64_add(&DPI_Statistics[big_type][15][255].up, (uint32_t)rte_pktmbuf_pkt_len(skb));
                if (update_link == 1) {
                    rte_atomic64_inc( &DPI_Statistics[big_type][15][255].link);
                }
            }
		}
		else
		{
            if (update_flow)
                rte_atomic64_add(&DPI_Statistics[big_type][15][255].up, (uint32_t)rte_pktmbuf_pkt_len(skb));
            if (update_link == 1) {
                rte_atomic64_inc( &DPI_Statistics[big_type][15][255].link);
            }
        }
    }

FUNC_END:
    return 0; 
}


static struct rte_timer statistics_timer;

/* timer0 callback */
static void
timer0_cb(__attribute__((unused)) struct rte_timer *tim,
      __attribute__((unused)) void *arg)
{
    static unsigned counter = 0;
    int i, j, k;
    int64_t up,down, link, total_link;
    char export_buf[256]; 
    int flag = 0;
    int n = 0;
    static_buf[0] = '\0';

    for (i = 1; i < 16; i++) { 
        for (j = 15; j > 0; j--) {
            for (k = 255; k > 0; k--) {   
                if(DPI_Statistics[i][j][k].name[0]) {
                    up = rte_atomic64_read(&DPI_Statistics[i][j][k].up);
                    down = rte_atomic64_read(&DPI_Statistics[i][j][k].down);
                    link = rte_atomic64_read(&DPI_Statistics[i][j][k].link);
//                    printf("[%u][%u][%u]=%u\n",i,j,k,link);
                    //rte_atomic64_set(&DPI_Statistics[i][j][k].link, 0);
                    rte_atomic64_clear(&DPI_Statistics[i][j][k].link);
                        total_link = rte_atomic64_add_return(&DPI_Statistics[i][j][k].total_link,link);
                        if (up > 0||down > 0||(total_link > 0 && !((pv.syn_ack &0xffff0000) ^ (i<<28|j<<24|k<<16))))
                        {
                            //      bzero(export_buf,256);
                            //      sprintf( export_buf, "%s|%ld/%ld\n",DPI_Statistics[i][j][k].name, up, down);
                            //      printf("%s", export_buf);
                            n += snprintf(static_buf+n, 256, "%s|%lu/%lu/%lu\n",DPI_Statistics[i][j][k].name, up, down, total_link);
                            //export_file(STATICTICS_FILE_TYPE, static_buf,flag);
                            //flag = 1;
                        }
                }
            }
        }
    }

            link = rte_atomic64_read(&DPI_Statistics[0][0][0].link);
            rte_atomic64_clear(&DPI_Statistics[0][0][0].link);
                total_link = rte_atomic64_add_return(&DPI_Statistics[0][0][0].total_link, link);
                n += snprintf(static_buf+n, 256, "%s|%lu/%lu/%lu\n",
                        DPI_Statistics[0][0][0].name, rte_atomic64_read(&DPI_Statistics[0][0][0].up), rte_atomic64_read(&DPI_Statistics[0][0][0].down), total_link);
                export_file(STATICTICS_FILE_TYPE, static_buf,3);
           // export_file(STATICTICS_FILE_TYPE, NULL, 2);
           
    output_rule_flow_statics();
}

int init_dpi_statistics(void)
{
    uint64_t hz;
    unsigned lcore_id;

    statictics_flag = 0;
//    printf("%s(%d),start,%d,app_class_cnt:%d,\n",__FUNCTION__,__LINE__,app_class_cnt);

    static_buf = rte_zmalloc(NULL, 1024*1024, 0);
     if (unlikely(!static_buf))
        return -1;
   
    DPI_Statistics = rte_zmalloc(NULL, sizeof(struct dpi_statistics_struct)*16*16*256, 0);
    //memset(DPI_Statistics,0,sizeof(struct dpi_statistics_struct)*16*16*256);
    if (unlikely(!DPI_Statistics)) {
        E("no enough mem for DPI_Statistics\n");
        exit(1);
        return -1;
    }
    
    init_app_stat();
    init_export_file();
    statictics_flag = 1;
    rte_timer_init(&statistics_timer);

    /* load timer0, every second, on master lcore, reloaded automatically */
    hz = rte_get_timer_hz();
    lcore_id = rte_lcore_id();
    rte_timer_reset(&statistics_timer, 5 * hz, PERIODICAL, lcore_id, timer0_cb, NULL);

    return 0;
}
