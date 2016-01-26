#include <stdint.h>
#include <rte_malloc.h>
#include "utils.h"
#include "session_mgr.h"
#include "debug.h"
#include "init.h"
#include "com.h"
#include "dpi_log.h"
#include "dpi_login_analysis.h"
#if 1
void login_parse(void *data, pattern_t *pattern, size_t offset, uint8_t rule_status) //rule_status:rulse 规则跑完为1，中间状态为0
{

        struct ssn_skb_values * ssv = ( struct ssn_skb_values *)data;
        //get dns or ip + port list
        //char *sp = ssv->payload + offset + pattern->pattern_len;
        char *sp = ssv->payload + offset;
        char *ep = ssv->payload + ssv->payload_len;
        char *login_start, *login_end;
        char login_name[128];
        int pos = 0;
        int minlen = 0;
        if (pattern->pattern_key.dynamic_type == 0x89) {
                if (ssv->ssn->ssn_ptr1 == NULL)
                        return;
                export_login(ssv, pattern, ssv->ssn->ssn_ptr1);
                rte_free(ssv->ssn->ssn_ptr1);
                ssv->ssn->ssn_ptr1 = NULL;
                return;
        }

        pos = subhex_in_mainhex(sp, ep - sp, pattern->pattern_key.ip_key, pattern->pattern_key.ip_key_len, 0);
        if (pos <= 0)
                return;
        sp += pos;
        for(; __isspace(*sp)||*sp == '"'||*sp == ':'||*sp == ','; sp++) {
                if (sp >= ep)
                        return;
        }
        login_start = sp;
        if (pattern->pattern_key.port_key_len > 0) {
                pos = subhex_in_mainhex(sp, ep - sp, pattern->pattern_key.port_key, pattern->pattern_key.port_key_len, 0);
                if (pos <= 0) {
                        pos = substr_in_mainstr_nocase(sp, ep - sp, "\r",  0);
                        if (pos <= 0)
                                return ;
                }
                sp += pos;
                login_end = sp - pattern->pattern_key.port_key_len;
                if (*(login_end - 1) == ',') {
                        login_end -= 1;
                }
                minlen = login_end - login_start;
                if (minlen > 127) {
                        minlen = 127;
                }

        } else {
                //pos = substr_in_mainstr_nocase(sp, ep - sp, "\r",  0);
                //if (pos <= 0)
                //        return ;
        }
        if (rule_status == 1) {
                switch (pattern->pattern_key.dynamic_type) {
                    case 0x81:
                    case 0x82:
                    {
                        if (minlen == 0)
                            return;
                        login_name[minlen + 4] = '\0';
                        strncpy(login_name, "UID:", 4);
                        memcpy(login_name + 4, login_start, minlen);
                        break;
                    }
                    case 0x84:
                    {
                        if (minlen == 0)
                            return;
                        login_name[minlen + 4] = '\0';
                        strncpy(login_name, "TID:", 4);
                        memcpy(login_name + 4, login_start, minlen);
                        break;
                    }
                    case 0x88:
                    {
                            minlen = *(uint8_t *)(login_start - 1) - 4;
                            login_name[minlen + 3] = '\0';
                            strncpy(login_name, "QQ:", 3);
                            memcpy(login_name + 3, login_start, minlen);
                            break;
                    }


                } 
        } else {
                ssv->ssn->ssn_ptr1 = rte_zmalloc(NULL, minlen, 0);
                if (NULL == ssv->ssn->ssn_ptr1) {
                        LOG("no enough mem for parse login user\n");
                        return;
                }

                switch (pattern->pattern_key.dynamic_type) {
                    case 0x81:
                    case 0x82:
                    {
                        if (minlen == 0)
                            return;
                        ssv->ssn->ssn_ptr1[minlen + 4] = '\0';
                        strncpy(ssv->ssn->ssn_ptr1, "UID:", 4);
                        memcpy(ssv->ssn->ssn_ptr1 + 4, login_start, minlen);
                        break;
                    }
                    case 0x84:
                    {
                        if (minlen == 0)
                            return;
                        ssv->ssn->ssn_ptr1[minlen + 4] = '\0';
                        strncpy(ssv->ssn->ssn_ptr1, "TID:", 4);
                        memcpy(ssv->ssn->ssn_ptr1 + 4, login_start, minlen);
                        break;
                    }
                    case 0x88:
                    {
                            minlen = *(uint8_t *)(login_start - 1) - 4;
                            login_name[minlen + 3] = '\0';
                            strncpy(ssv->ssn->ssn_ptr1, "QQ:", 3);
                            memcpy(ssv->ssn->ssn_ptr1 + 3, login_start, minlen);
                            break;
                    }

            }

        } 
        if (rule_status) {
                export_login(ssv, pattern, login_name);
        }
}

#else
void login_parse(void *data, pattern_t *pattern, size_t offset, uint8_t rule_status) //rule_status:rulse 规则跑完为1，中间状态为0
{

    struct ssn_skb_values * ssv = ( struct ssn_skb_values *)data;
    //get dns or ip + port list
    //char *sp = ssv->payload + offset + pattern->pattern_len;
    char *sp = ssv->payload + offset;
    char *ep = ssv->payload + ssv->payload_len;
    char *login_start, *login_end;
    char login_name[128];
    int pos = 0;
    int minlen;
   

    switch(pattern->pattern_key.dynamic_type) {
        case 0x81:
            { 
                pos = subhex_in_mainhex(sp, ep - sp, pattern->pattern_key.ip_key, pattern->pattern_key.ip_key_len, 0);
                if (pos <= 0)
                    return;
                sp += pos;
                for(; __isspace(*sp)||*sp == '"'||*sp == ':'; sp++) {
                    if (sp >= ep)
                        return;
                }
                login_start = sp;
                if (pattern->pattern_key.port_key_len > 0) {
                    pos = subhex_in_mainhex(sp, ep - sp, pattern->pattern_key.port_key, pattern->pattern_key.port_key_len, 0);
                    if (pos <= 0) {
                        pos = substr_in_mainstr_nocase(sp, ep - sp, "\r",  0);
                        if (pos <= 0)
                            return ;
                    }
                } else {
                    pos = substr_in_mainstr_nocase(sp, ep - sp, "\r",  0);
                    if (pos <= 0)
                        return ;
                }
                sp += pos;
                login_end = sp - pattern->pattern_key.port_key_len;

                minlen = login_end - login_start;
                if (minlen > 127) {
                    minlen = 127;
                }
                if (rule_status == 1) {
                    login_name[minlen] = '\0';
                    memcpy(login_name, login_start, minlen);
                    urldecode(login_name, 0);
                } else {
                    ssv->ssn->ssn_ptr1 = (char *)rte_zmalloc(NULL, minlen, 0);
                    if (NULL == ssv->ssn->ssn_ptr1) {
                        LOG("no enough mem for parse login user\n");
                        return;
                    }
                    ssv->ssn->ssn_ptr1[minlen] = '\0';
                    memcpy(ssv->ssn->ssn_ptr1, login_start, minlen);
                    urldecode(ssv->ssn->ssn_ptr1, 0);

                } 
                break; 
            }
        case 0x82:
            { 
                pos = subhex_in_mainhex(sp, ep - sp, pattern->pattern_key.ip_key, pattern->pattern_key.ip_key_len, 0);
                if (pos <= 0)
                    return;
                sp += pos;
                for(; __isspace(*sp)||*sp == '"'||*sp == ':'; sp++) {
                    if (sp >= ep)
                        return;
                }
                login_start = sp;
                if (pattern->pattern_key.port_key_len > 0) {
                    pos = subhex_in_mainhex(sp, ep - sp, pattern->pattern_key.port_key, pattern->pattern_key.port_key_len, 0);
                    if (pos <= 0) {
                        pos = substr_in_mainstr_nocase(sp, ep - sp, "\r",  0);
                        if (pos <= 0)
                            return ;
                    }
                } else {
                    pos = substr_in_mainstr_nocase(sp, ep - sp, "\r",  0);
                    if (pos <= 0)
                        return ;
                }
                sp += pos;
                login_end = sp - pattern->pattern_key.port_key_len;

                minlen = login_end - login_start;
                if (minlen > 127) {
                    minlen = 127;
                }

                if (rule_status == 1) {
                    login_name[minlen] = '\0';
                    memcpy(login_name, login_start, minlen);
                    urldecode(login_name, 0);
                    urldecode(login_name, 0);  
                } else {
                    ssv->ssn->ssn_ptr1 = rte_zmalloc(NULL, minlen, 0);
                    if (NULL == ssv->ssn->ssn_ptr1) {
                        LOG("no enough mem for parse login user\n");
                        return;
                    }
                    ssv->ssn->ssn_ptr1[minlen] = '\0';
                    memcpy(ssv->ssn->ssn_ptr1, login_start, minlen);
                    urldecode(ssv->ssn->ssn_ptr1, 0);

                } 
                break; 
            }
        case 0x83:
            { 
                pos = subhex_in_mainhex(sp, ep - sp, pattern->pattern_key.ip_key, pattern->pattern_key.ip_key_len, 0);
                if (pos <= 0)
                    return;
                sp += pos;
                for(; __isspace(*sp)||*sp == '"'||*sp == ':'; sp++) {
                    if (sp >= ep)
                        return;
                }
                login_start = sp;
                if (pattern->pattern_key.port_key_len > 0) {
                    pos = subhex_in_mainhex(sp, ep - sp, pattern->pattern_key.port_key, pattern->pattern_key.port_key_len, 0);
                    if (pos <= 0) {
                        pos = substr_in_mainstr_nocase(sp, ep - sp, "\r",  0);
                        if (pos <= 0)
                            return ;
                    }
                } else {
                    pos = substr_in_mainstr_nocase(sp, ep - sp, "\r",  0);
                    if (pos <= 0)
                        return ;
                }
                sp += pos;
                login_end = sp - pattern->pattern_key.port_key_len;

                minlen = login_end - login_start;
                if (minlen > 127) {
                    minlen = 127;
                }
                if (rule_status == 1) {
                    login_name[minlen] = '\0';
                    memcpy(login_name, login_start, minlen);
                } else {
                    ssv->ssn->ssn_ptr1 = rte_zmalloc(NULL, minlen, 0);
                    if (NULL == ssv->ssn->ssn_ptr1) {
                        LOG("no enough mem for parse login user\n");
                        return;
                    }
                    ssv->ssn->ssn_ptr1[minlen] = '\0';
                    memcpy(ssv->ssn->ssn_ptr1, login_start, minlen);
                } 


                break; 
            }
        case 0x84:
            { 
                pos = subhex_in_mainhex(sp, ep - sp, pattern->pattern_key.ip_key, pattern->pattern_key.ip_key_len, 0);
                if (pos <= 0)
                    return;
                sp += pos;
                for(; __isspace(*sp)||*sp == '"'||*sp == ':'; sp++) {
                    if (sp >= ep)
                        return;
                }
                //                login_start = sp - 1;

                //int tlv_len = (uint8_t)(sp - 1);
                minlen = (uint8_t)(sp - 1);

                if (minlen > 127) {
                    minlen = 127;
                }

                if (rule_status == 1) {
                    login_name[minlen] = '\0';
                    memcpy(login_name, sp, minlen);
                } else {
                    ssv->ssn->ssn_ptr1 = rte_zmalloc(NULL, minlen, 0);
                    if (NULL == ssv->ssn->ssn_ptr1) {
                        LOG("no enough mem for parse login user\n");
                        return;
                    }
                    ssv->ssn->ssn_ptr1[minlen] = '\0';
                    memcpy(ssv->ssn->ssn_ptr1, sp, minlen);
                } 
                break; 
            }
            case 0x89:
            {
                if (ssv->ssn->ssn_ptr1 == NULL)
                    return;
                export_login(ssv, pattern, ssv->ssn->ssn_ptr1);
                rte_free(ssv->ssn->ssn_ptr1);
                ssv->ssn->ssn_ptr1 = NULL;
                return;
            }
            default:
                return;
    }
    if (rule_status) {
        export_login(ssv, pattern, login_name);
    }
}
#endif


