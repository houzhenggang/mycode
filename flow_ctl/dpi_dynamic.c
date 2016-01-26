#include "domain_tbl.h"
#include <stdint.h>
#include "study_tbl.h"
#include "dynamic_tbl.h"
#include "h_cache.h"
#include "utils.h"
#include "dpi_dynamic.h"
//#include "dynamic_debug.h"
#include "dpi.h"
#include "dns.h"
#include "ftp.h"
#include "debug.h"
#include "init.h"
#include "session_mgr.h"


#define DNS_STUDY_TIMEO (3600 * 24)
//#define DYNAMIC_DEBUG

void dynamic_parse(void *data, pattern_t *pattern, size_t offset)
{
    if(unlikely(!get_flags_dynamic_enable()))
        return;
    struct ssn_skb_values * ssv = ( struct ssn_skb_values *)data;
    //struct l2ct_var_dpi * lvd =  &(ssv->ssn->vars_dpi);
#if 0 
    if (unlikely(pattern->pattern_key.dynamic_current_phase - 1 & lvd->ac_state_tbl != pattern->pattern_key.dynamic_current_phase - 1)) {
        if (i != 4) {
        D("current_phase[%u], ldv[%p], ac_state_tbl[%u]\n", pattern->pattern_key.dynamic_current_phase,lvd, lvd->ac_state_tbl);
            aaa =4;
        }
        lvd->ac_state_tbl = 0;
        return;
    }
    lvd->ac_state_tbl |= pattern->pattern_key.dynamic_current_phase;

    if (lvd->ac_state_tbl < pattern->pattern_key.dynamic_need_phase)
        return;
    	lvd->ac_state_tbl = 0;
#endif
//get dns or ip + port list
    //char *sp = ssv->payload + offset + pattern->pattern_len;
    char *sp = ssv->payload + offset;
    char *ep = ssv->payload + ssv->payload_len;
    int pos = 0;
    uint32_t ip;
    uint32_t proto_mark;
    uint16_t port = 0;

    switch(pattern->pattern_key.dynamic_type) {
            case 1:
                    {
                            if (pattern->pattern_key.dynamic_port && pattern->pattern_key.dynamic_port != (uint16_t)-1)
                            {
                                    if(pattern->pattern_key.dynamic_dir && pattern->pattern_key.dynamic_port != ssv->ssn->sess_key.port_dst)
                                    {
                                            return;
                                    }

                                    if(0 == pattern->pattern_key.dynamic_dir && pattern->pattern_key.dynamic_port != ssv->ssn->sess_key.port_src)
                                    {
                                            return;
                                    }

                            } 
                           
                            if (pattern->pattern_key.dynamic_dir) 
                            {    
                                    if (pattern->pattern_key.dynamic_port == (uint16_t)-1) {

                                        study_cache_try_get(ssv->ssn->sess_key.ip_dst, ssv->dport, ssv->ssn->proto_mark, 0, 0); 	
 #ifdef DYNAMIC_DEBUG
                                    LOG("pattern[%s]add ip [%u]and port [%u.%u.%u.%u:%u] proto[%u] dynamic_indirect[%d]to study cache, common type, \n",
                                                    pattern->pattern_name, ssv->ssn->sess_key.ip_dst, IPQUADS(ssv->ssn->sess_key.ip_dst), ntohs(ssv->dport),  ssv->ssn->proto_mark,pattern->pattern_key.dynamic_indirect); 
#endif
                                   } else {
                                    study_cache_try_get(ssv->ssn->sess_key.ip_dst, pattern->pattern_key.dynamic_port, ssv->ssn->proto_mark, 0, 0); 	
#ifdef DYNAMIC_DEBUG
                                    LOG("pattern[%s]add ip [%u]and port [%u.%u.%u.%u:%u] proto[%u] dynamic_indirect[%d]to study cache, common type, \n",
                                                    pattern->pattern_name, ssv->ssn->sess_key.ip_dst, IPQUADS(ssv->ssn->sess_key.ip_dst), ntohs(pattern->pattern_key.dynamic_port),  ssv->ssn->proto_mark,pattern->pattern_key.dynamic_indirect); 
#endif
                                }
                            } 
                            else 
                            {
                                    if (pattern->pattern_key.dynamic_port == (uint16_t)-1) {

                                        study_cache_try_get(ssv->ssn->sess_key.ip_src, ssv->sport, ssv->ssn->proto_mark, 0, 0); 	
 #ifdef DYNAMIC_DEBUG
                                    LOG("pattern[%s]add ip [%u]and port [%u.%u.%u.%u:%u] proto[%u] dynamic_indirect[%d]to study cache, common type, \n",
                                                    pattern->pattern_name, ssv->ssn->sess_key.ip_src, IPQUADS(ssv->ssn->sess_key.ip_src), ntohs(ssv->sport),  ssv->ssn->proto_mark,pattern->pattern_key.dynamic_indirect); 
#endif
                                    } else {
                                    study_cache_try_get(ssv->ssn->sess_key.ip_src, pattern->pattern_key.dynamic_port, ssv->ssn->proto_mark, 0, 0); 
#ifdef DYNAMIC_DEBUG
                                    LOG("pattern[%s]add ip and port [%u.%u.%u.%u:%u] proto[%u] dynamic_indirect[%d]to study cache, common type\n",
                                                    pattern->pattern_name, IPQUADS(ssv->ssn->sess_key.ip_src), ntohs(pattern->pattern_key.dynamic_port),  ssv->ssn->proto_mark,pattern->pattern_key.dynamic_indirect); 
#endif
                                    }
                            }

                            break;
                    }
            case 2:
                    {
                            //printf("dns type\n");
                            parse_dns(data, pattern, offset); 
                            break;
                    }
            case 3:// ip:port[10.211.55.88:88]
                    {
                            do{
                                    pos = subhex_in_mainhex(sp, ep - sp, pattern->pattern_key.ip_key, pattern->pattern_key.ip_key_len, 0);
                                    if (pos <= 0)
                                            return;
                                    sp += pos;
                                    for(; __isspace(*sp)||*sp == '"'||*sp == ':'; sp++) {
                                            if (sp >= ep)
                                                    return;
                                    }
                                    ip = ipv4_stonl(sp);
                                    if (unlikely(ip == 0xFFFFFFFF))
                                            return;
                                    sp += 6;
                                    if (pattern->pattern_key.dynamic_port) {
                                            port = pattern->pattern_key.dynamic_port;
                                    } else { 
                                            pos = substr_in_mainstr_nocase(sp, 16, ":", 0);
                                            if (pos > 0) {
                                                    sp += pos;
                                                    port = port_stons(sp);
                                                    if (port == 65535) {
                                                            port = 0;
                                                    }
                                            } 
                                    }
                                    if (likely(pattern->pattern_key.dynamic_indirect == 0) ) {
                                            if (pattern->pattern_key.dynamic_dir) {    
                                                    study_cache_try_get(ip, port, ssv->ssn->proto_mark, 0, 0); 
                                            } else {
                                                    dynamic_cache_try_get(ip, port, ssv->ssn->proto_mark, 0); 
                                            }

#ifdef DYNAMIC_DEBUG
                                            LOG("[%s]add ip and port [%u.%u.%u.%u:%u]proto[%u]to dynamic cache isinner[%d]\n",pattern->pattern_name, IPQUADS(ip), ntohs(port), ssv->ssn->proto_mark, ssv->isinner);
#endif
                                    } else {
                                            if ((proto_mark = dns_study_lookup_behavior(ssv->dip, ssv->dport)) > 0) {

                                                    if (pattern->pattern_key.dynamic_dir) {    
                                                            study_cache_try_get(ip, port, proto_mark, 0, DNS_STUDY_TIMEO); 
                                                    } else {
                                                            dynamic_cache_try_get(ip, port, proto_mark, 0); 
                                                    }

#ifdef DYNAMIC_DEBUG
                                                    LOG("[%s] dynamic indirect add ip and port [%u.%u.%u.%u:%u]proto[%u]to dynamic cache isinner[%d]\n", pattern->pattern_name, IPQUADS(ip), ntohs(port), proto_mark, ssv->isinner);
#endif
                                            }  else if ((proto_mark = dns_study_lookup_behavior(ssv->sip, ssv->sport)) > 0) {
                                                    if (pattern->pattern_key.dynamic_dir) {    
                                                            study_cache_try_get(ip, port, proto_mark, 0, DNS_STUDY_TIMEO); 
                                                    } else {
                                                            dynamic_cache_try_get(ip, port, proto_mark, 0); 
                                                    }

#ifdef DYNAMIC_DEBUG
                                                    LOG("[%s] dynamic indirect add ip and port [%u.%u.%u.%u:%u]proto[%u]to dynamic cache isinner[%d]\n",pattern->pattern_name, IPQUADS(ip), ntohs(port), proto_mark, ssv->isinner);
#endif
                                            }
                                    }
                            } while (pattern->pattern_key.mult_iplist && sp < ep);
                            //   lvd->is_dynamic = 1;
                            break;
                    }
            case 4://such as: "ip":"111.161.80.157","port":1935,
                    {
                            do{
                                    pos = subhex_in_mainhex(sp, ep - sp, pattern->pattern_key.ip_key, pattern->pattern_key.ip_key_len, 0);
                                    if (pos <= 0)
                                            return;
                                    sp += pos;
                                    for(; __isspace(*sp)||*sp == '"'||*sp == ':'; sp++) {
                                            if (sp >= ep)
                                                    return;
                                    }

                                    ip = ipv4_stonl(sp);
                                    if (unlikely(ip == 0xFFFFFFFF))
                                            return;
                                    sp += 6; 
                                    pos = subhex_in_mainhex(sp, ep - sp, pattern->pattern_key.port_key, pattern->pattern_key.port_key_len, 0);
                                    if (pos <= 0)
                                            return;
                                    sp += pos;
                                    for(; __isspace(*sp)||*sp == '"'||*sp == ':'; sp++) {
                                            if (sp >= ep)
                                                    return;
                                    }
                                    port = port_stons(sp);
                                    if (likely(pattern->pattern_key.dynamic_indirect == 0) ) {
                                            if (pattern->pattern_key.dynamic_dir) {    
                                                    study_cache_try_get(ip, port, ssv->ssn->proto_mark, 0, 0); 
                                            } else {
                                                    dynamic_cache_try_get(ip, port, ssv->ssn->proto_mark, 0); 
                                            }
#ifdef DYNAMIC_DEBUG
                                            LOG("[%s]add ip and port [%u.%u.%u.%u:%u]proto[%u]to dynamic cache isinner[%d]\n", pattern->pattern_name, IPQUADS(ip), ntohs(port), ssv->ssn->proto_mark, ssv->isinner);
#endif 
                                    } else {
                                            if ((proto_mark = dns_study_lookup_behavior(ssv->dip, ssv->dport)) > 0) {

                                                    if (pattern->pattern_key.dynamic_dir) {    
                                                            study_cache_try_get(ip, port, proto_mark, 0, DNS_STUDY_TIMEO); 
                                                    } else {
                                                            dynamic_cache_try_get(ip, port, proto_mark, 0); 
                                                    }

#ifdef DYNAMIC_DEBUG
                                                    LOG("[%s] dynamic indirect add ip and port [%u.%u.%u.%u:%u]proto[%u]to dynamic cache  isinner[%d]\n", pattern->pattern_name, IPQUADS(ip), ntohs(port), proto_mark, ssv->isinner);
#endif
                                            } else if ((proto_mark = dns_study_lookup_behavior(ssv->sip, ssv->sport)) > 0) {


                                                    if (pattern->pattern_key.dynamic_dir) {    
                                                            study_cache_try_get(ip, port, proto_mark, 0, DNS_STUDY_TIMEO); 
                                                    } else {
                                                            dynamic_cache_try_get(ip, port, proto_mark, 0); 
                                                    }

#ifdef DYNAMIC_DEBUG
                                                    LOG("[%s] dynamic indirect add ip and port [%u.%u.%u.%u:%u]proto[%u]to dynamic cache  isinner[%d]\n", pattern->pattern_name, IPQUADS(ip), ntohs(port), proto_mark, ssv->isinner);
#endif
                                            }

                                    }
                            } while (pattern->pattern_key.mult_iplist && sp < ep);
                            //                lvd->is_dynamic = 1;
                            break;

                    }

            case 5:
                    {
                            do{
                                    for(; __isspace(*sp)||*sp == '"'||*sp == ':'; sp++) {
                                            if (sp >= ep)
                                                    return;
                                    }

                                    ip = ipv4_stonl(sp);
                                    if (unlikely(ip == 0xFFFFFFFF))
                                            return;
                                    sp += 6;

                                    if (pattern->pattern_key.dynamic_port) {
                                            port = pattern->pattern_key.dynamic_port;
                                    } else { 
                                            pos = subhex_in_mainhex(sp, 16, pattern->pattern_key.port_key,pattern->pattern_key.port_key_len, 0);
                                            if (pos > 0) {
                                                    sp += pos;
                                                    port = port_stons(sp);
                                                    if (port == 65535) {
                                                            port = 0;
                                                    }
                                            } 
                                    }

                                    if (likely(pattern->pattern_key.dynamic_indirect == 0) ) {
                                            if (pattern->pattern_key.dynamic_dir) {    
                                                    study_cache_try_get(ip, port, ssv->ssn->proto_mark, 0, 0); 
                                            } else {
                                                    dynamic_cache_try_get(ip, port, ssv->ssn->proto_mark, 0); 
                                            }
#ifdef DYNAMIC_DEBUG
                                            LOG("[%s]add ip and port [%u.%u.%u.%u:%u]proto[%u]to dynamic cache isinner[%d]\n", pattern->pattern_name, IPQUADS(ip), ntohs(port), ssv->ssn->proto_mark, ssv->isinner);
#endif 
                                    } else {
                                            if ((proto_mark = dns_study_lookup_behavior(ssv->dip, ssv->dport)) > 0) {

                                                    if (pattern->pattern_key.dynamic_dir) {    
                                                            study_cache_try_get(ip, port, proto_mark, 0, DNS_STUDY_TIMEO); 
                                                    } else {
                                                            dynamic_cache_try_get(ip, port, proto_mark, 0); 
                                                    }

#ifdef DYNAMIC_DEBUG
                                                    LOG("[%s] dynamic indirect add ip and port [%u.%u.%u.%u:%u]proto[%u]to dynamic cache  isinner[%d]\n", pattern->pattern_name, IPQUADS(ip), ntohs(port), proto_mark, ssv->isinner);
#endif
                                            } else if ((proto_mark = dns_study_lookup_behavior(ssv->sip, ssv->sport)) > 0) {


                                                    if (pattern->pattern_key.dynamic_dir) {    
                                                            study_cache_try_get(ip, port, proto_mark, 0, DNS_STUDY_TIMEO); 
                                                    } else {
                                                            dynamic_cache_try_get(ip, port, proto_mark, 0); 
                                                    }

#ifdef DYNAMIC_DEBUG
                                                    LOG("[%s] dynamic indirect add ip and port [%u.%u.%u.%u:%u]proto[%u]to dynamic cache  isinner[%d]\n", pattern->pattern_name, IPQUADS(ip), ntohs(port), proto_mark, ssv->isinner);
#endif
                                            }

                                    }
                            } while (pattern->pattern_key.mult_iplist && sp < ep);
                            //                lvd->is_dynamic = 1;
                            break;

                    }


#if 0
            case 5://such as: ch1.dnf.qq.com,
                    {
                            do{
                                    for(; __isspace(*sp); sp++) {
                                            if (sp >= ep)
                                                    return;
                                    }
                                    pos = substr_in_mainstr_nocase(sp, ep - sp, pattern->port_key, 0);
                                    //pos = substr_in_mainstr_nocase(sp, 16, ":", 0);
                                    if (pos > 0) {
                                            strncopy(domain, sp, pos - strlen(pattern->port_key));
                                            sp += pos;
                                            port = simple_strtol(sp, NULL, 0);
                                            domain_cache_try_get(domain, ssv->ssn->proto_mark); 
                                            printk("---port=%u\n", port);
                                    }
                                    // else {
                                    //     port = pattern->dynamic_port;
                                    // }
                                    DEG("add ip and port [%u.%u.%u.%u:%u]proto[%u]to dynamic cache\n", IPQUADS(ip), ntohs(port), ssv->ssn->proto_mark); 
                            } while (pattern->mult_iplist && sp < ep);
                            lvd->is_dynamic = 1;
                            break;
                    }
#endif
            default: 
                    break;

    }

}

/* --------------------------------------------------------------- */

/*
 * Operations for setting or displaying dynamic dns.
 */

int init_dynamic_protocol() 
{
    if(unlikely(domain_cache_init() != 0)) {
        goto err;
    }
    if (unlikely(study_cache_init() != 0)) {
        goto err;
    }
    if (unlikely(dns_study_cache_init() != 0)) {
        goto err;
    }
    if (unlikely(dynamic_cache_init() != 0)) {
        goto err;
    }
    if (unlikely(ftp_cache_init() != 0)) {
        goto err;
    }
    //indirect_cache_init();
   // pack_test_init();
//test h_cache
#if 0
    if (unlikely(session_cache_init() != 0)) {
        goto err;
    }
#endif
    return 0;
err:
    E("init_dynamic_protocol fail\n");
    exit(1);
}

void remove_dynamic_protocol()
{
    domain_cache_exit();
    study_cache_exit();
    dynamic_cache_exit();
}
