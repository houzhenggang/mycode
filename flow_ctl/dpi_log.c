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
#include <rte_config.h>
#include <sys/time.h>
#include <syslog.h>

#include "acsmx2.h"
#include "session_mgr.h"
#include "dpi_log.h"
#include "utils.h"
#include "debug.h"
#include "init.h"

//#define LOG_ANA_DEBUG
static struct rte_mempool *log_cache[RTE_MAX_LCORE];
extern uint16_t place_cnt;
//extern int snprintf(char * buf, size_t size, const char * fmt, ...);

uint8_t qqlog = 0;
uint8_t urlog = 0;

//extern int sft_ksyslog(char * buf, int buf_len);

int export_qq_log(struct ssn_skb_values * ssv)
{
    uint32_t qq = 0;
	char * name = NULL;
    struct timeval tv;
    char buf[256];
    char qq_hex[QQ_NUM_LEN+1] = { 0 };
	unsigned int *qq_num;
    strncpy(qq_hex, ssv->payload + 7, 4);

//	printk("%s(%d),%02x %02x %02x %02x.\n",__FUNCTION__,__LINE__,qq_hex[0],qq_hex[1],qq_hex[2],qq_hex[3]);
	qq_num = (unsigned int *)qq_hex;
//	printk("%s(%d),qq num:%u.\n",__FUNCTION__,__LINE__,ntohl(*qq_num));
	qq = ntohl(*qq_num);

	if(pv.sft_tc_dev_name[0] != 0)
	{
		name = pv.sft_tc_dev_name;
	}
	else
	{
		name = "SFT_TC";
	}
    
    gettimeofday(&tv,NULL);

    struct tm *p; 
    time_t timep; 
    time(&timep); 
    p = localtime(&timep); 
    buf[0] = '\0';
    snprintf(buf, 256 , "%u-%u-%u:%u:%u:%u<%s> pc-login  %ld QQ:%u %u.%u.%u.%u %u.%u.%u.%u\n", (1900+p->tm_year), (1+p->tm_mon), (p->tm_mday), 
            (p->tm_hour), (p->tm_min), (p->tm_sec),name, 
            (long)tv.tv_sec, qq, RTE_NIPQUAD(ssv->ssn->sess_key.ip_src), RTE_NIPQUAD(ssv->ssn->sess_key.ip_dst));
#ifdef LOG_ANA_DEBUG
    LOG("%s", buf);
#endif
    sendto(pv.syslog_sd, buf, strlen(buf), 0, (void*)&pv.syslog_peer_addr, sizeof(pv.syslog_peer_addr));
 

    return 0;
}

int export_weibo_log(struct ssn_skb_values * ssv, char *usename, int ulen, int lcore)
{
    struct timeval tv;
    char buf[256];
    char * name = NULL;
    
	if(pv.sft_tc_dev_name[0] != 0)
	{
		name = pv.sft_tc_dev_name;
	}
	else
	{
		name = "SFT_TC";
	}
    
    gettimeofday(&tv,NULL);

    buf[0] = '\0';
    int len = snprintf(buf, URL_LOG_LEN, "<%s>weibo_login %ld %s %u.%u.%u.%u %u.%u.%u.%u", name,(long)tv.tv_sec, usename, RTE_NIPQUAD(ssv->sip), RTE_NIPQUAD(ssv->dip));

    sendto(pv.syslog_sd, buf, len, 0, (void*)&pv.syslog_peer_addr, sizeof(pv.syslog_peer_addr));
    
    return 0;
}

int export_tieba_log(struct ssn_skb_values * ssv, char *usename, int ulen, int lcore)
{
    struct timeval tv;
    char buf[256];
    char * name = NULL;
    
	if(pv.sft_tc_dev_name[0] != 0)
	{
		name = pv.sft_tc_dev_name;
	}
	else
	{
		name = "SFT_TC";
	}
    
    gettimeofday(&tv,NULL);

    buf[0] = '\0';
    int len = snprintf(buf, URL_LOG_LEN, "<%s>baidutieba_login %ld %s %u.%u.%u.%u %u.%u.%u.%u", name,(long)tv.tv_sec, usename, RTE_NIPQUAD(ssv->sip), RTE_NIPQUAD(ssv->dip));

    sendto(pv.syslog_sd, buf, len, 0, (void*)&pv.syslog_peer_addr, sizeof(pv.syslog_peer_addr));
    
    return 0;
}
int export_login(struct ssn_skb_values * ssv, pattern_t *pattern, char *usename)
{
    struct timeval tv;
    char buf[256];
    char * name = NULL;
    
	if(pv.sft_tc_dev_name[0] != 0)
	{
		name = pv.sft_tc_dev_name;
	}
	else
	{
		name = "SFT_TC";
	}
    
    gettimeofday(&tv,NULL);

    buf[0] = '\0';

    int specify_pos = 0;
    if (pattern->repeated_cnt) {
        specify_pos = substr_in_mainstr_nocase(pattern->pattern_name, strlen(pattern->pattern_name), "-", 0);
        if (specify_pos < 0)
            specify_pos = 0;
    }

#if 1
    int pos = substr_in_mainstr_nocase(pattern->pattern_name, strlen(pattern->pattern_name), "_", 0);
//    if (pos < 0 )
//        return 1;
#endif    
    struct tm *p; 
    time_t timep; 
    time(&timep); 
    p = localtime(&timep); 
    if (pos < 0) {
        if (likely(specify_pos == 0)) {
            snprintf(buf, 256 , "%u-%u-%u:%u:%u:%u<%s> %s %ld %s %u.%u.%u.%u %u.%u.%u.%u\n", (1900+p->tm_year), (1+p->tm_mon), (p->tm_mday), 
                    (p->tm_hour), (p->tm_min), (p->tm_sec),name, pattern->pattern_name,
                    (long)tv.tv_sec, usename, RTE_NIPQUAD(ssv->ssn->sess_key.ip_src), RTE_NIPQUAD(ssv->ssn->sess_key.ip_dst));
        } else {
            snprintf(buf, 256 , "%u-%u-%u:%u:%u:%u<%s> Mobile-%s %ld %s %u.%u.%u.%u %u.%u.%u.%u\n", (1900+p->tm_year), (1+p->tm_mon), (p->tm_mday), 
                    (p->tm_hour), (p->tm_min), (p->tm_sec),name, pattern->pattern_name + specify_pos,
                    (long)tv.tv_sec, usename, RTE_NIPQUAD(ssv->ssn->sess_key.ip_src), RTE_NIPQUAD(ssv->ssn->sess_key.ip_dst));

        }
    } else { 
            int ret = snprintf(buf, 256 , "%u-%u-%u:%u:%u:%u<%s> ", (1900+p->tm_year), (1+p->tm_mon), (p->tm_mday), 
                            (p->tm_hour), (p->tm_min), (p->tm_sec),name );

            if (likely(specify_pos == 0)) {
                snprintf(buf + ret, pos , "%s",  pattern->pattern_name);
            } else {
                snprintf(buf + ret, pos - specify_pos + 7, "Mobile-%s",  pattern->pattern_name + specify_pos);
            }
            snprintf(buf + strlen(buf),  256- ret, " %ld %s %u.%u.%u.%u %u.%u.%u.%u\n", (long)tv.tv_sec, 
                    usename, RTE_NIPQUAD(ssv->ssn->sess_key.ip_src), RTE_NIPQUAD(ssv->ssn->sess_key.ip_dst)); 
    }
#ifdef LOG_ANA_DEBUG
    LOG("%s", buf);
#endif
    sendto(pv.syslog_sd, buf, strlen(buf), 0, (void*)&pv.syslog_peer_addr, sizeof(pv.syslog_peer_addr));
   
    return 0;
}

int export_url_log(struct ssn_skb_values * ssv)
{
	char * name = NULL;
    char * head = NULL;
    char * host = NULL;
    char * url = NULL;
    int uri_len = 0;
    int host_len = 0;
    struct timeval tv;
	int head_len = 0;
    char buf[256];

	if(ssv->http_token_host_offset < 6)
		return -1;

	switch(ssv->http_token_method)
	{
		case HTTP_TOKEN_GET:
			head = "GET";
			head_len = 3;
			break;
		case HTTP_TOKEN_HEAD:
			head  = "HEAD";
			head_len = 4;
			break;
		case HTTP_TOKEN_POST:
			head = "POST";
			head_len = 4;
			break;
		case HTTP_TOKEN_LOCATION:
			head = "LOCATION";
			head_len = 8;
			break;

		default:
			break;
	}

    if (head == NULL)
        return -1;
    
    uri_len = sft_memmem(ssv->payload, ssv->payload_len, "\r\n", 2);
    if (uri_len > 0) 
    {
        url = ssv->payload + head_len + 1; 
        uri_len -= (head_len + 10);
        if (uri_len > URL_WIDTH)
            uri_len = URL_WIDTH;
    }
    else
        return -1;

    host_len = sft_memmem(ssv->payload + ssv->http_token_host_offset + 6, ssv->payload_len - ssv->http_token_host_offset - 6, "\r\n", 2);
    if (host_len > 0)
    {
        host = ssv->payload + ssv->http_token_host_offset + 6; 
        //host_len -= 6;
        if (host_len > HOST_WIDTH)
            host_len = HOST_WIDTH;
    }
    else
    {
        return -1;
    }
    
    gettimeofday(&tv, NULL);
	if(pv.sft_tc_dev_name[0] != 0)
	{
		name = pv.sft_tc_dev_name;
	}
	else
	{
		name = "SFT_TC";
	}
    struct tm *p; 
    time_t timep; 
    gettimeofday(&tv,NULL);
    time(&timep); 
    p = localtime(&timep); 
    buf[0] = '\0';
    snprintf(buf, 256 , "%u-%u-%u:%u:%u:%u<%s> www  %ld url: %s %.*s%.*s  %u.%u.%u.%u %u.%u.%u.%u\n", (1900+p->tm_year), (1+p->tm_mon), (p->tm_mday), 
            (p->tm_hour), (p->tm_min), (p->tm_sec),name, 
            (long)tv.tv_sec, head, host_len, host, uri_len, url, RTE_NIPQUAD(ssv->ssn->sess_key.ip_src), RTE_NIPQUAD(ssv->ssn->sess_key.ip_dst));
#ifdef LOG_ANA_DEBUG
    LOG("%s", buf);
#endif
    sendto(pv.syslog_sd, buf, strlen(buf), 0, (void*)&pv.syslog_peer_addr, sizeof(pv.syslog_peer_addr));
    return 0;
}

