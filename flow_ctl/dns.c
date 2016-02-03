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

#include "domain_tbl.h"
#include "dynamic_tbl.h"
#include "utils.h"
//#include "indirect_tbl.h"
#include "study_tbl.h"
#include "dns_study_tbl.h"
#include "pattern_tbl.h"
#include"debug.h"
#include "dns.h"
#include "dpi.h"
#include "session_mgr.h"

//#define DNS_LOG_TEST	
#define DNS_STUDY_TIMEO (3600 * 24)

static u_int getNameLength(u_int i, const u_int8_t *payload, u_int payloadLen) {
  if(payload[i] == 0x00)
    return(1);
  else if(payload[i] == 0xC0)
    return(2);
  else {
    u_int8_t len = payload[i];
    u_int8_t off = len + 1;

    if(off == 0) /* Bad packet */
      return(0);
    else
      return(off + getNameLength(i+off, payload, payloadLen));
  }	
}

/* *********************************************** */

static char* utc_intoa_v4(unsigned int addr, char* buf, u_short bufLen) {
  char *cp, *retStr;
  uint byte;
  int n;

  cp = &buf[bufLen];
  *--cp = '\0';

  n = 4;
  do {
    byte = addr & 0xff;
    *--cp = byte % 10 + '0';
    byte /= 10;
    if(byte > 0) {
      *--cp = byte % 10 + '0';
      byte /= 10;
      if(byte > 0)
	*--cp = byte + '0';
    }
    *--cp = '.';
    addr >>= 8;
  } while (--n > 0);

  /* Convert the string to lowercase */
  retStr = (char*)(cp+1);

  return(retStr);
}

/* *********************************************** */

static uint16_t get16(int *i, const u_int8_t *payload) {
  uint16_t v = *(uint16_t*)&payload[*i];

  (*i) += 2;

  return(ntohs(v));
}

/* *********************************************** */

struct dns_header {
  uint16_t transaction_id, flags, num_queries, answer_rrs, authority_rrs, additional_rrs;
} __attribute__((packed));

void parse_dns(void *data, pattern_t *pattern, size_t offset) 
{
    struct ssn_skb_values *ssv = (struct ssn_skb_values *)data;
    uint8_t host_server_name[256]; 
#define MAX_DNS_REQUESTS			16
    if((ssv->sport == 0x3500)
            && (ssv->payload_len > sizeof(struct dns_header))) {
         //   printk("debug\n");
        int i = ssv->l4proto == IPPROTO_TCP ? 2 : 0;                                
        struct dns_header *dns = (struct dns_header*)&ssv->payload[i];
        uint8_t is_query,  is_dns = 0;
        uint32_t ret_code;
#ifdef DNS_DEBUG
        u_int32_t a_record[MAX_DNS_REQUESTS] = { 0 };
#endif
         u_int32_t query_offset, num_a_records = 0;

        ret_code = dns->flags& 0x80 ? 0 : (dns->flags & 0x0F00);
        i += sizeof(struct dns_header);
        query_offset = i;
        if( dns->flags& 0x80) {
       // printk("---------[%s(%d)]is dns:%u.%u.%u.%u:%x->%u.%u.%u.%u:%x\n", __FUNCTION__,__LINE__,IPQUADS(ssv->sip), ssv->sport, IPQUADS(ssv->dip), ssv->dport);
            /* DNS Reply */
            if((ntohs(dns->num_queries) <= MAX_DNS_REQUESTS) 
                    && (((ntohs(dns->answer_rrs) > 0) && (ntohs(dns->answer_rrs) <= MAX_DNS_REQUESTS))
                        || (( ntohs(dns->authority_rrs) > 0) && ( ntohs(dns->authority_rrs) <= MAX_DNS_REQUESTS))
                        || ((ntohs(dns->additional_rrs) > 0) && (ntohs(dns->additional_rrs) <= MAX_DNS_REQUESTS)))
              ) {
                /* This is a good reply */
                is_dns = 1;

                i++;

                if(ssv->payload[i] != '\0') {
                    while((i < ssv->payload_len)
                            && (ssv->payload[i] != '\0')) {
                        i++;
                    }

                    i++;
                }

                i += 4;

                if(ntohs(dns->answer_rrs) > 0) {
                    uint16_t rsp_type, rsp_class;
                    uint16_t num;
                    uint16_t answer_rrs = ntohs(dns->answer_rrs);
                        
                    for(num = 0; num < answer_rrs; num++) {
                        uint16_t data_len;

                        if((i+6) >= ssv->payload_len) {
                            break;
                        }

                        if((data_len = getNameLength(i, ssv->payload, ssv->payload_len)) == 0) {
                            break;
                        } else
                            i += data_len;

                        rsp_type = get16(&i, ssv->payload);
                        rsp_class = get16(&i, ssv->payload);

                        i += 4;
                        data_len = get16(&i, ssv->payload);

                        if((data_len <= 1) || (data_len > (ssv->payload_len-i))) {
                            break;
                        }

                        if(rsp_type == 1 /* A */) {
                            if(data_len == 4) {
                                //u_int32_t v = ntohl(*((u_int32_t*)&ssv->payload[i]));
                                u_int32_t v = *((u_int32_t*)&ssv->payload[i]);
                                //LOG("dns proto_mark=%u ip:[%u]%u.%u.%u.%u\n",ssv->ssn->proto_mark, v,IPQUADS(v));
                                if (likely(pattern->pattern_key.dynamic_indirect == 0)) {
                                    if (pattern->pattern_key.dynamic_dir) {    
                                        study_cache_try_get(*((u_int32_t*)&ssv->payload[i]), pattern->pattern_key.dynamic_port, ssv->ssn->proto_mark, 0, DNS_STUDY_TIMEO); 
                                    } else {
                                        dynamic_cache_try_get(*((u_int32_t*)&ssv->payload[i]), pattern->pattern_key.dynamic_port, (ssv->ssn->proto_mark|0x8000), 0); 
                                    }
#ifdef DNS_LOG_TEST
                                       LOG("dns proto_mark=%u ip:[%u]%u.%u.%u.%u:%u\n",ssv->ssn->proto_mark, v,IPQUADS(v), ntohs(pattern->pattern_key.dynamic_port));
#endif
                                } else {
                                        dns_study_cache_try_get(*((u_int32_t*)&ssv->payload[i]), pattern->pattern_key.dynamic_port, (ssv->ssn->proto_mark|0x8000)); 
#ifdef DNS_LOG_TEST
                                       LOG("dns proto_mark=%u ip:[%u]%u.%u.%u.%u:%u\n",ssv->ssn->proto_mark, v,IPQUADS(v), ntohs(pattern->pattern_key.dynamic_port));
#endif
                                       // D("add to dns study cache\n");
                                }

#ifdef DNS_DEBUG
                                if(num_a_records < (MAX_DNS_REQUESTS-1)) {
                                    a_record[num_a_records++] = v;
                                }
                                else
                                    break; /* One record is enough */
#endif
                            }
                        }

                        if(data_len == 0) {
                            break;
                        }

                        i += data_len;
                    } /* for */
                }
            }

#ifdef DNS_DEBUG
            if((ntohs(dns->num_queries) <= MAX_DNS_REQUESTS)
                    && ((dns->answer_rrs == 0)
                        || (dns->authority_rrs == 0)
                        || (dns->additional_rrs == 0))
                    && (ret_code != 0 /* 0 == OK */)
              ) {
                /* This is a good reply */
                is_dns = 1;
            }
#endif
        }

#ifdef DNS_DEBUG
        if(is_dns) {
            int j = 0;
            uint16_t query_type, query_class;

            i = query_offset+1;

            while((i < ssv->payload_len)
                    && (j < (sizeof(host_server_name)-1))	  
                    && (ssv->payload[i] != '\0')) {
                host_server_name[j] = tolower(ssv->payload[i]);
                if(host_server_name[j] < ' ')
                    host_server_name[j] = '.';	
                j++, i++;
            }
            host_server_name[j] = '\0';
            char host_server_ip[256] = {0};
            j = 0;
            if(a_record != 0) 
            {
                char a_buf[32];
                int i;
                for(i=0; i<num_a_records; i++) {
                    j += snprintf(&host_server_ip[j], sizeof(host_server_ip)-1, "%s%s",
                            (i == 0) ? "@" : ";",
                            utc_intoa_v4(a_record[i], a_buf, sizeof(a_buf)));
                }
            }

            host_server_ip[j] = '\0';
            if(j > 0) {
                printk("==> [%s][%s]\n", host_server_name, host_server_ip);
               
            }

#if 0
            i++;
            memcpy(&query_type, &ssv->payload[i], 2); query_type  = ntohs(query_type), i += 2;
            memcpy(&query_class, &ssv->payload[i], 2); query_class  = ntohs(query_class), i += 2;

            printk("%s [type=%04X][class=%04X]\n", host_server_name, query_type, query_class);
#endif
        } else {
           // printk( "exclude DNS.\n");
        }
#endif
    }
}
