#include <string.h>
#include <dirent.h> 
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include<string.h>
#include<stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include <arpa/inet.h>
#include <sys/stat.h>

#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_mbuf.h>

#include "tc_config.h"
#include "tc_frame_fwd.h"
#include "global.h"
#include "tc_queue.h"
#include "utils.h"

extern struct rte_mempool *token_flow_mem_cache;
extern struct rte_mempool *fip_fifo_mem_cache;
extern struct rte_mempool *fip_data_mem_cache;
extern struct rte_mempool *datapipe_fifo_mem_cache;
extern struct rte_mempool *datapipe_data_mem_cache;
extern struct rte_mempool *flow_ip_mem_cache;

extern unsigned int KSFT_FLOW_HASH_ITEM;
extern unsigned int ksft_fip_hashsize;
extern struct ksft_hash_node *ksft_flow_ip_hash;
extern rte_atomic64_t nums_sft_fip_entry;

extern struct tc_private_t *priv;

rte_atomic64_t fip_num = RTE_ATOMIC64_INIT(0);
rte_atomic64_t ftoken_num = RTE_ATOMIC64_INIT(0);

inline uint32_t dton(uint32_t mask)
{
    uint32_t i;
    int bits = sizeof(uint32_t) * 8; 
    i = ~0;  
    bits -= mask;
    i <<= bits;
    return htonl(i);
}

inline int parse_string_to_ip_address(uint8_t *ip, char *buffer)
{
    char *start = buffer;
    char temp[30];
    int temp_ptr = 0;
    int dot_count = 0;
    
    if(buffer==NULL) return FALSE;
    if(ip==NULL) return FALSE;

    //contains 3 "." ?
    {    dot_count = 0;
        while( *start!='\0' && *start!='\n' )
        {
            if(*start=='.')
            {
                dot_count++;
            }
            start++;
        }
        if(dot_count!=3)
            return FALSE;
    }
    
    //Now parse into stats 
    start = buffer;
    temp[0] = '\0';
    temp_ptr = 0;
    dot_count = 0;
    while( *start!='\0' && *start!='\n' )
    {
        if( (*start)!='.' )
        {    temp[temp_ptr] = *start;
            temp_ptr++;
            temp[temp_ptr] = '\0';
        }
        else
        {
            switch(dot_count)
            {
                case 0:
                    ip[0] = (uint8_t) strtoul(temp, NULL, 10);
                    break;
                case 1:
                    ip[1] = (uint8_t) strtoul(temp, NULL, 10);
                    break;
                case 2:
                    ip[2] = (uint8_t) strtoul(temp, NULL, 10);
                    break;
            }
            temp_ptr = 0; temp[0] = '\0';
            dot_count++;
        }
        start++;
    }
    ip[3] = (uint8_t) strtoul(temp, NULL, 10);
//    printk("parse_string_to_ip_address: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    return TRUE;
}

int do_parse_rule_work(struct tc_private_t *conf, char *rule_string)
{
    char *p;
    int cur_index = -1;
    struct sft_flow_rule *rule;
    char * paction ;
    int ret = 0;
    int i = 1;
    int flag = 0;

    RTE_DBG("%s(%d),[%s].\n",__FUNCTION__,__LINE__,rule_string);
    
    if(!conf)
    {
        return 1;
    }

    if(rule_string == NULL)
        return 1;
    if(strstr(rule_string,"INIT_START"))
    {
        int i;
        for(i=0;i<FLOW_RULE_NUM; i++)
        {    
            if(conf->rule_backup[i])    
            {  
            //    printk("%s(%d),free rule ID [%d].\n",__FUNCTION__,__LINE__,rule_backup[i]->index);
                rte_free(conf->rule_backup[i]);
            }
        }
        memset(conf->rule_backup,0,sizeof(conf->sft_flow_rule1));

        for(i = 0 ; i<DATAPIPE_NUM; i++) 
        {    
            conf->ksft_datapipe[i].used = 0;
        }

        RTE_DBG("%s(%d),[rule_active:%p,rule_backup:%p].\n",__FUNCTION__,__LINE__,conf->rule_active,conf->rule_backup);
        RTE_DBG("%s(%d),[sft_flow_rule1:%p,sft_flow_rule2:%p].\n",__FUNCTION__,__LINE__,conf->sft_flow_rule1,conf->sft_flow_rule2);
        goto ok_code;
    }
    else if(strstr(rule_string,"INIT_END"))
    {
        struct sft_flow_rule **rb;
        int i;
        int t = 0;
        int s=0,e=0;
        rb = conf->rule_backup;

        for(i=0;i<FLOW_RULE_NUM; i++)
        {    
            if(conf->rule_backup[i])    
            {  
                t = 1;
                s = i;
                break;
            }
        }
        if(t == 0)
        {
            s = 0;
        }

        t = 0;
        for(i=FLOW_RULE_NUM - 1;i >= 0; i--)
        {    
            if(conf->rule_backup[i]) 
            {  
                t = 1;
                e = i;
                break;
            }
        }
        if(t == 0)
        {
            e = 0;
        }
#if 0 //just print
        {
            int i;
            RTE_DBG("%s(%d),start/end:[%d----%d]\n",__FUNCTION__,__LINE__,s,e);
            for(i=0;i<FLOW_RULE_NUM; i++)
            {
                if(conf->rule_backup[i])    
                {
                    RTE_DBG("%s(%d),rule index:%d,prev/next:[%d----%d]\n",__FUNCTION__,__LINE__,i,
                                conf->rule_backup[i]->prev_index,conf->rule_backup[i]->next_index);
                }
            }
        }
#endif
        conf->rule_backup = conf->rule_active;
        //spin_lock_bh(&sft_flow_rule_lock);
        conf->sft_flow_rule_end = 0;
        conf->sft_flow_rule_start = 0;
        conf->rule_active = rb;
        conf->sft_flow_rule_start = s;
        conf->sft_flow_rule_end = e;
        //spin_unlock_bh(&sft_flow_rule_lock);

        conf->rule_update_timestamp = rte_rdtsc(); 
//        printk("%s(%d),[rule_active:%p,rule_backup:%p,rule_active updatetime:%lu].\n",
//                __FUNCTION__,__LINE__,rule_active,rule_backup,rule_update_timestamp);
//        printk("%s(%d),[sft_flow_rule1:%p,sft_flow_rule2:%p].\n",__FUNCTION__,__LINE__,sft_flow_rule1,sft_flow_rule2);
        goto ok_code;
    }

    paction = strstr(rule_string,"ACTION:");
    if(paction == NULL)
    {

        RTE_ERR("%s(%d),%s,bad rule_string format,miss ACTION.\n",__FUNCTION__,__LINE__,rule_string);
        return 1;
    }

    rule = (struct sft_flow_rule *)rte_zmalloc(NULL, sizeof(struct sft_flow_rule), 0);
    if(!rule)
    {
        RTE_ERR("%s(%d),alloc rule entry memory fail.\n",__FUNCTION__,__LINE__);
        return -ENOMEM;
    }

    while((p = strsep(&rule_string, " ")))
    {
        //RTE_DBG("%s(%d),[i:%d]%s.\n",__FUNCTION__,__LINE__,i,p);
        switch(i)
        {
            case 1:
                rule->index = (int)strtoul(p, NULL, 10);
                if((rule->index > (FLOW_RULE_NUM - 1)) || (rule->index < 0))
                {
                    rte_free(rule);
                    goto ok_code;
                }
                cur_index = rule->index;

                break;
            case 2:
                if(strcmp(p,"enable") == 0)
                    rule->enable = 1;
                else if(strcmp(p,"disable") == 0)
                    rule->enable = 0;
                else
                {
                    
                    RTE_ERR("%s(%d),bad enable or disable.\n",__FUNCTION__,__LINE__);
                    ret = 1;
                    goto fail_code;
                }
                break;
            case 3:
                if(strcmp(p,"any_dir") == 0)
                    rule->dir = ANY_DIR;
                else if(strcmp(p,"down_dir") == 0)
                    rule->dir = DOWN_DIR;
                else if(strcmp(p,"up_dir") == 0)
                    rule->dir = UP_DIR;
                else
                {
                    RTE_ERR("%s(%d),bad dir.\n",__FUNCTION__,__LINE__);
                    ret = 1;
                    goto fail_code;
                }
                break;
            case 4://inner ip
                if(strcmp(p,"any_ip") == 0)
                {
                    rule->inner_ip_type = IPT_ANY_IP;
                    break;
                }
                if(strstr(p,"*"))
                {//ip group
                    unsigned short idx = (unsigned short)strtoul(p+1, NULL, 10);
                    if(conf->sft_ipgroup_list[idx].enable == 0)
                    {
                        RTE_ERR("%s(%d),inner,NO ip group id:[%d].\n",__FUNCTION__,__LINE__,idx);
                        ret = 1;
                        goto fail_code;
                    }
                    rule->ipg_inner_idx = idx;
                    rule->inner_ip_type = IPT_GROUP_IP;
//                    printk("%s(%d),inner,ip group id:[%d].\n",__FUNCTION__,__LINE__,rule->ipg_idx);
                }
                else if(strstr(p,"/"))
                {
                    unsigned int subnet_msk_;
                    uint8_t network_id_octets[4];
                    uint8_t *network_id = NULL; //Pointer pointing command
                    uint8_t *subnet_msk = NULL; //Pointer pointing value    
                    int ii;
                    int value_len;

                    rule->inner_ip_type = IPT_NETWORK_MASK_IP;
                    network_id = (uint8_t *)p;
                    value_len = strlen(p);
                    for(ii=0;ii<value_len;ii++)
                    {
                        if(p[ii] != '/')
                        {
                            if(p[ii] == '\r' || p[ii] == '\n')
                                p[ii] = '\0';
                            else
                                continue;
                        }
                        else
                        {
                            p[ii] = '\0';
                            subnet_msk = (uint8_t *)&p[ii+1];
                        }
                    }

//                    printk("%s(%d),%s %s.\n",__FUNCTION__,__LINE__,network_id,subnet_msk);
                    subnet_msk_= dton((unsigned int )strtoul((char *)subnet_msk, NULL, 10));
//                    printk("%s(%d),%0x.\n",__FUNCTION__,__LINE__,subnet_msk_);
                    if((parse_string_to_ip_address(network_id_octets, (char *)network_id)==TRUE))
                    {
                        rule->inner_network[0] = network_id_octets[0];
                        rule->inner_network[1] = network_id_octets[1];
                        rule->inner_network[2] = network_id_octets[2];    
                        rule->inner_network[3] = network_id_octets[3];
                        rule->inner_mask[0] = subnet_msk_&0xFF;
                        rule->inner_mask[1] = (subnet_msk_>>8)&0xFF;
                        rule->inner_mask[2] = (subnet_msk_>>16)&0xFF;
                        rule->inner_mask[3] = (subnet_msk_>>24)&0xFF;
                    }
                }
                else if(strstr(p,"-"))
                {
                    int j = 1;
                    char *pp;
                    rule->inner_ip_type = IPT_START_END_IP;
                    while((pp = strsep(&p, "-")))
                    {
//                        printk("%s(%d),j is %d,%s.\n",__FUNCTION__,__LINE__,j,pp);
                        switch(j)
                        {
                            case 1:
                                rule->inner_ip_start = ntohl(inet_addr(pp));
                                break;
                            case 2:
                                rule->inner_ip_end = ntohl(inet_addr(pp));
                                break;
                        }
                        j++;
                    }
                }
                else
                {
                    rule->inner_ip_type = IPT_ONE_IP;
                    rule->inner_ip = inet_addr(p);
                }
                break;
            case 5://inner_port
                if(strcmp(p,"any_port") == 0)
                {
                    rule->inner_port_type = PT_ANY_PORT;
                    break;
                }
                if(strstr(p,"-"))
                {
                    int j = 1;
                    char *pp;
                    rule->inner_port_type = PT_START_END_PORT;
                    while((pp = strsep(&p, "-")))
                    {
                    //    printk("%s(%d),j is %d,%s.\n",__FUNCTION__,__LINE__,j,pp);
                        switch(j)
                        {
                            case 1:
                                rule->inner_port_start = (unsigned short)strtoul(pp, NULL, 10);
                                break;
                            case 2:
                                rule->inner_port_end = (unsigned short)strtoul(pp, NULL, 10);
                                break;
                        }
                        j++;
                    }
                }
                else
                {
                    rule->inner_port_type = PT_ONE_PORT;
                    rule->inner_port = htons((unsigned short)strtoul(p, NULL, 10));
                }
                break;
            case 6://outer ip
                if(strcmp(p,"any_ip") == 0)
                {
                    rule->outer_ip_type = IPT_ANY_IP;
                    break;
                }
                if(strstr(p,"*"))
                {//ip group
                    unsigned short idx = (unsigned short)strtoul(p+1, NULL, 10);
                    if(conf->sft_ipgroup_list[idx].enable == 0)
                    {
                        RTE_ERR("%s(%d),outer,NO ip group id:[%d].\n",__FUNCTION__,__LINE__,idx);
                        ret = 1;
                        goto fail_code;
                    }
                    rule->ipg_outer_idx = idx;
                    rule->outer_ip_type = IPT_GROUP_IP;
                    //printk("%s(%d),outer,ip group id:[%d].\n",__FUNCTION__,__LINE__,rule->ipg_idx);
                }
                else if(strstr(p,"/"))
                {
                    unsigned int subnet_msk_;
                    uint8_t network_id_octets[4];
                    int ii;
                    int value_len;
                    uint8_t *network_id = NULL; //Pointer pointing command
                    uint8_t *subnet_msk = NULL; //Pointer pointing value    

                    rule->outer_ip_type = IPT_NETWORK_MASK_IP;
                    network_id = (uint8_t *)p;
                    value_len = strlen(p);
                    for(ii=0;ii<value_len;ii++)
                    {
                        if(p[ii] != '/')
                        {
                            if(p[ii] == '\r' || p[ii] == '\n')
                                p[ii] = '\0';
                            else
                                continue;
                        }
                        else
                        {
                            p[ii] = '\0';
                            subnet_msk = (uint8_t *)&p[ii+1];
                        }
                    }
        //            printk("%s(%d),%s %s.\n",__FUNCTION__,__LINE__,network_id,subnet_msk);
                    subnet_msk_= dton((unsigned int )strtoul((char *)subnet_msk, NULL, 10));
        //            printk("%s(%d),%0x.\n",__FUNCTION__,__LINE__,subnet_msk_);

                    if((parse_string_to_ip_address(network_id_octets, (char *)network_id)==TRUE))
                    {
                        rule->outer_network[0] = network_id_octets[0];
                        rule->outer_network[1] = network_id_octets[1];
                        rule->outer_network[2] = network_id_octets[2];    
                        rule->outer_network[3] = network_id_octets[3];
                        rule->outer_mask[0] = subnet_msk_&0xFF;
                        rule->outer_mask[1] = (subnet_msk_>>8)&0xFF;
                        rule->outer_mask[2] = (subnet_msk_>>16)&0xFF;
                        rule->outer_mask[3] = (subnet_msk_>>24)&0xFF;
                    }
                }
                else if(strstr(p,"-"))
                {
                    int j = 1;
                    char *pp;
                    rule->outer_ip_type = IPT_START_END_IP;
                    while((pp = strsep(&p, "-")))
                    {
                //        printk("%s(%d),j is %d,%s.\n",__FUNCTION__,__LINE__,j,pp);
                        switch(j)
                        {
                            case 1:
                                rule->outer_ip_start = ntohl(inet_addr(pp));
                                break;
                            case 2:
                                rule->outer_ip_end = ntohl(inet_addr(pp));
                                break;
                        }
                        j++;
                    }
                }
                else
                {
                    rule->outer_ip_type = IPT_ONE_IP;
                    rule->outer_ip = inet_addr(p);
                }
                break;
            case 7://outer_port
                if(strcmp(p,"any_port") == 0)
                {
                    rule->outer_port_type = PT_ANY_PORT;
                    break;
                }
                if(strstr(p,"-"))
                {
                    int j = 1;
                    char *pp;
                    rule->outer_port_type = PT_START_END_PORT;
                    while((pp = strsep(&p, "-")))
                    {
                //        printk("%s(%d),j is %d,%s.\n",__FUNCTION__,__LINE__,j,pp);
                        switch(j)
                        {
                            case 1:
                                rule->outer_port_start = (unsigned short)strtoul(pp, NULL, 10);
                                break;
                            case 2:
                                rule->outer_port_end = (unsigned short)strtoul(pp, NULL, 10);
                                break;
                        }
                        j++;
                    }
                }
                else
                {
                    rule->outer_port_type = PT_ONE_PORT;
                    rule->outer_port = htons((unsigned short)strtoul(p, NULL, 10));
                }
                break;
            case 8:
                if(strcmp(p,"any_proto") == 0)
                    rule->l4proto = PROTO_ANY;
                else if(strcmp(p,"tcp") == 0)
                    rule->l4proto = PROTO_TCP;
                else if(strcmp(p,"udp") == 0)
                    rule->l4proto = PROTO_UDP;
                else
                {
                    RTE_ERR("%s(%d),bad L4 proto.\n",__FUNCTION__,__LINE__);
                    ret = 1;
                    goto fail_code;
                }
                break;
            case 9://APP PROTO ID
            {
                char *proto = p;
                char *tmp = 0;
                do
                {
                    tmp = strsep(&proto, "+");
                    if(!tmp)
                    {
                        tmp = proto;
                    }
                    
                    if(rule->app_proto_num < MAX_RULE_PROTO_NUM)
                    {
                        if(strcmp(p,"any_app") == 0)
                            rule->app_proto[rule->app_proto_num] = 0xFFFFFFFF;
                        else if(strcmp(p,"unknown_app") == 0)
                            rule->app_proto[rule->app_proto_num] = 0x0;
                        else
                            rule->app_proto[rule->app_proto_num] = (uint32_t)strtoul(tmp, NULL, 10);
                        
                        rule->app_proto_num++;
                    }
                }while(proto);
                
                
                break;
            }
            case 10:
                if(strcmp(p,"ACTION:") == 0)
                {
                    break;
                }
                else
                {
                    RTE_ERR("%s(%d),bad ACTION.\n",__FUNCTION__,__LINE__);
                    ret = 1;
                    goto fail_code;
                }
                break;
            case 11:
                if(strcmp(p,"stop") == 0)
                {
                    rule->ifgoon = RULE_STOP;
                }
                else if(strcmp(p,"continue") == 0)
                {
                    rule->ifgoon = RULE_CONTINUE;
                }
                else
                {
                    RTE_ERR("%s(%d),bad stop/continue.\n",__FUNCTION__,__LINE__);
                    ret = 1;
                    goto fail_code;
                }
                break;
            case 12:
                if(strcmp(p,"accept") == 0)
                {
                    rule->action_type = ACCEPT;
                }
                else if(strcmp(p,"block") == 0)
                {
                    rule->action_type = BLOCK;
                }
                else if(strcmp(p,"ip_rate") == 0)
                {
                    rule->action_type = ONE_IP_LIMIT;
                }
                else if(strcmp(p,"datapipe") == 0)
                {
                    rule->action_type = DATAPIPE;
                }
                else
                {
                    RTE_ERR("%s(%d),bad stop/continue.\n",__FUNCTION__,__LINE__);
                    ret = 1;
                    goto fail_code;
                }
                break;
            case 13: 
                if(rule->action_type == ONE_IP_LIMIT)
                {
                    rule->ip_rate = ((int)strtoul(p, NULL, 10))*125;
                }
                else if(rule->action_type == DATAPIPE)
                {
                    int idx = (int)strtoul(p, NULL, 10);
                    if(conf->ksft_datapipe[idx].rate)
                    {
                        if(conf->ksft_datapipe[idx].used == 0)
                        {
                            if(!conf->ksft_datapipe[idx].pktQ) {
                                conf->ksft_datapipe[idx].pktQ = tc_queue_init();
                            }
                            if(conf->ksft_datapipe[idx].pktQ && !rte_timer_pending(&conf->ksft_datapipe[idx].timeout_send))
                            {
                                rte_timer_init(&conf->ksft_datapipe[idx].timeout_send);
                                
                                uint64_t hz1000m = rte_get_timer_hz() / 1000; //1ms
                                static uint8_t lcore = 0;
                                if(lcore <= 0)
                                {
                                    lcore = rte_lcore_count() - 1;
                                }
                                else
                                {
                                    lcore--;
                                }
                                int ret_timer = rte_timer_reset(&conf->ksft_datapipe[idx].timeout_send, hz1000m, 
                                                            SINGLE, lcore, sft_tc_send_pipe_handler, 
                                                            (void *)&conf->ksft_datapipe[idx]);
                                if(ret_timer < 0)
                                {
                                    RTE_ERR("%s(%d), datapipe[%d] timer reset failde.\n", __FUNCTION__, __LINE__, idx);
                                }
                                RTE_DBG("%s(%d),start datapipeID:%d timer.\n",__FUNCTION__,__LINE__,idx);
                                
                                conf->ksft_datapipe[idx].index = rule->index;
                            }
                            conf->ksft_datapipe[idx].used = 1;
                        }
                        rule->datapipe_index = idx;
                    }
                    else
                    {
                        RTE_ERR("%s(%d),bad datapipe index,%s.\n",__FUNCTION__,__LINE__,p);
                        ret = 1;
                        goto fail_code;
                    }
                }
                else
                {
                    RTE_ERR("%s(%d),bad oneip/datapipe,%s.\n",__FUNCTION__,__LINE__,p);
                    ret = 1;
                    goto fail_code;
                }
                break;
            default :
                break;
        }
        i++;
    }//while()

    rule->flow_statics = init_rule_flow(conf->disp_conf.curr_rule_name, rule->index);
    conf->rule_backup[rule->index] = rule;

    //match prev
    if(rule->index > 0)
    {
        for(i=rule->index-1;i >= 0; i--)
        {
            if(conf->rule_backup[i])
            {
                flag = 1;
                conf->rule_backup[i]->next_index = rule->index;
                break;
            }
        }
        if(flag == 1)
        {
            rule->prev_index = i;
        }
        else
        {
            rule->prev_index = rule->index;
        }
    }
    else//==0
    {
        rule->prev_index = 0;
    }

//match next
    flag = 0;

    if(rule->index < (FLOW_RULE_NUM -1))
    {
        for(i=rule->index+1;i<FLOW_RULE_NUM; i++)
        {
            if(conf->rule_backup[i])
            {
                flag = 1;
                conf->rule_backup[i]->prev_index = rule->index;
                break;
            }
        }
        if(flag == 1)
        {
            rule->next_index = i;
        }
        else
        {
            rule->next_index = rule->index;
        }
    }
    else//== (65535)
    {
        rule->next_index = FLOW_RULE_NUM - 1;
    }

#if 0 // JUST PRINT RULE INFO
    {
        uint32_t a = htonl(rule->inner_ip_start);
        uint32_t b = htonl(rule->inner_ip_end);
        uint32_t c = htonl(rule->outer_ip_start);
        uint32_t d = htonl(rule->outer_ip_end);
      
        RTE_DBG("%s(%d)------------------.\n",__FUNCTION__,__LINE__);
        switch(rule->inner_ip_type)
        {
            case IPT_ANY_IP:
                RTE_ERR("inner ip ANY.\n");
                break;
            case IPT_ONE_IP:
                RTE_ERR("one inner ip:%pI4.\n",&rule->inner_ip);
                break;
            case IPT_NETWORK_MASK_IP:
                RTE_ERR("inner ip mask:%u.%u.%u.%u/%u.%u.%u.%u.\n",rule->inner_network[0],rule->inner_network[1],rule->inner_network[2],rule->inner_network[3],rule->inner_mask[0],rule->inner_mask[1],rule->inner_mask[2],rule->inner_mask[3]);
                break;
            case IPT_START_END_IP:
                RTE_ERR("range inner ip:%pI4--%pI4,[hostorder:%u--%u]\n",&a,&b,rule->inner_ip_start,rule->inner_ip_end);
                break;
        }
        switch(rule->inner_port_type)
        {
            case PT_ANY_PORT:
                RTE_ERR("inner port ANY.\n");
                break;
            case PT_ONE_PORT:
                RTE_ERR("one inner port:%d.\n",ntohs(rule->inner_port));
                break;
            case PT_START_END_PORT:
                RTE_ERR("range inner port:%d--%d.\n",rule->inner_port_start,rule->inner_port_end);
                break;
        }

        switch(rule->outer_ip_type)
        {
            case IPT_ANY_IP:
                RTE_ERR("outer ip ANY.\n");
                break;
            case IPT_ONE_IP:
                RTE_ERR("one outer ip:%pI4.\n",&rule->outer_ip);
                break;
            case IPT_NETWORK_MASK_IP:
                RTE_ERR("outer ip mask:%u.%u.%u.%u/%u.%u.%u.%u.\n",rule->outer_network[0],rule->outer_network[1],rule->outer_network[2],rule->outer_network[3],rule->outer_mask[0],rule->outer_mask[1],rule->outer_mask[2],rule->outer_mask[3]);
                break;
            case IPT_START_END_IP:
                RTE_ERR("range outer ip:%pI4--%pI4,[hostorder:%u--%u]\n",&c,&d,rule->outer_ip_start,rule->outer_ip_end);
                break;
        }
        switch(rule->outer_port_type)
        {
            case PT_ANY_PORT:
                RTE_ERR("outer port ANY.\n");
                break;
            case PT_ONE_PORT:
                RTE_ERR("one outer port:%d.\n",ntohs(rule->outer_port));
                break;
            case PT_START_END_PORT:
                RTE_ERR("range outer port:%d--%d.\n",rule->outer_port_start,rule->outer_port_end);
                break;
        }

        RTE_ERR("rule index:%d,dir:%d,enable:%d,l4proto:%d,action_type:%d.\n",
                rule->index,rule->dir,rule->enable,rule->l4proto,rule->action_type);
        RTE_ERR("rate_limit:%u bytes,datapipe_id:%d.\n",rule->ip_rate,rule->datapipe_index);
        RTE_ERR("datapipe_index:%d,app_proto:%u. rate:%d\n",rule->datapipe_index,rule->app_proto, conf->ksft_datapipe[rule->datapipe_index].rate);
        RTE_ERR("%d<--[%d]-->%d.\n",rule->prev_index,rule->index,rule->next_index);
        RTE_ERR("----------------%s(%d).\n",__FUNCTION__,__LINE__);
    }
#endif    
ok_code:
    return ret;

fail_code:
    if(rule) rte_free(rule);
    return ret;
}

static int do_parse_datapipe_work(struct tc_private_t *conf, char *string)
{
    char *p;
    int ret = 0;
    int i = 1;
    int flag = 0;
    int index = 0;

//    if(printk_ratelimit())
//        printk("%s(%d),[%s].\n",__FUNCTION__,__LINE__,string);
    if(!conf)
    {
        return 1;
    }
    
    if(string == NULL)
        return 1;

    while((p = strsep(&string, " ")))
    {
        switch(i)
        {
            case 1:
                index = (int)strtoul(p, NULL, 10);
                if((index > (DATAPIPE_NUM - 1)) || (index < 0))
                {
                    ret = 1;
                    goto fail_code_fun;
                }
                break;
            case 2:
                if(strcmp(p,"add") == 0)
                    flag = 1;
                else if(strcmp(p,"update") == 0)
                    flag = 2;
                else if(strcmp(p,"del") == 0)
                {
                    flag = 3;
                    if(conf->ksft_datapipe[index].used == 1)
                    {
                        RTE_ERR("%s(%d),datapipe ID %d in used.\n",__FUNCTION__,__LINE__,index);
                        ret = 1;
                        goto fail_code_fun;
                    }
                    else
                    {
                        conf->ksft_datapipe[index].rate = 0;
                    }
                }
                else
                {
                    RTE_ERR("%s(%d),bad add/del/update.\n",__FUNCTION__,__LINE__);
                    ret = 1;
                    goto fail_code_fun;
                }
                break;
            case 3:
                if(flag == 1) // add
                {
                    if(conf->ksft_datapipe[index].rate)
                    {
                        int r = ((int)strtoul(p, NULL, 10))*125;
                        RTE_DBG("%s(%d),add key but upate pipe rate:[%d-->%d] bytes.\n",__FUNCTION__,__LINE__,conf->ksft_datapipe[index].rate,r);
                        conf->ksft_datapipe[index].rate = r;
                        //atomic_set(&ksft_datapipe[index].token, ksft_datapipe[index].rate);

                        //rte_spinlock_lock(&conf->ksft_datapipe[index].token_entry.lock);
                        //ksft_datapipe[index].token_entry.token =  ksft_datapipe[index].rate;
                        rte_atomic32_init(&conf->ksft_datapipe[index].token_entry.token);
                        //rte_spinlock_unlock(&conf->ksft_datapipe[index].token_entry.lock);
                    }
                    else
                    {
                        conf->ksft_datapipe[index].rate = ((int)strtoul(p, NULL, 10))*125;
                        //atomic_set(&ksft_datapipe[index].token, ksft_datapipe[index].rate);
                        
                        conf->ksft_datapipe[index].priv = conf;

                        //rte_spinlock_lock(&conf->ksft_datapipe[index].token_entry.lock);
                        //ksft_datapipe[index].token_entry.token =  ksft_datapipe[index].rate;
                        //conf->ksft_datapipe[index].token_entry.token = 0;
                        //rte_spinlock_unlock(&conf->ksft_datapipe[index].token_entry.lock);
                        rte_atomic32_init(&conf->ksft_datapipe[index].token_entry.token);

                        RTE_DBG("%s(%d),add pipe rate:%d bytes.\n",__FUNCTION__,__LINE__,conf->ksft_datapipe[index].rate);
                    }
                }
                else if(flag == 2)//update
                {
                    conf->ksft_datapipe[index].rate = ((int)strtoul(p, NULL, 10))*125;
                    //atomic_set(&ksft_datapipe[index].token, ksft_datapipe[index].rate);

                    //rte_spinlock_lock(&conf->ksft_datapipe[index].token_entry.lock);
                    //ksft_datapipe[index].token_entry.token =  ksft_datapipe[index].rate;
                    //conf->ksft_datapipe[index].token_entry.token = 0;
                    //rte_spinlock_unlock(&conf->ksft_datapipe[index].token_entry.lock);
                    rte_atomic32_init(&conf->ksft_datapipe[index].token_entry.token);

                    RTE_DBG("%s(%d),upate pipe rate:%d bytes.\n",__FUNCTION__,__LINE__,conf->ksft_datapipe[index].rate);
                }

                break;
            default :
                break;
        }
        i++;
    }
fail_code_fun:
    return ret;
}

static int do_parse_ipgroup_work(struct tc_private_t *conf, char *string)
{
    char *p;
    int ret = 0;
    int i = 1;
    int flag = 0;
    int index = 0;

//    if(printk_ratelimit())
//        printk("%s(%d),[%s].\n",__FUNCTION__,__LINE__,string);
    if(!conf)
    {
        return 1;
    }
    
    if(string == NULL)
        return 1;

    while((p = strsep(&string, " ")))
    {
        switch(i)
        {
            case 1:
                index = (int)strtoul(p, NULL, 10);
//                printk("%s(%d),ipgroup index:%d.\n",__FUNCTION__,__LINE__,index);
                if((index > (IPGROUP_NUM - 1)) || (index < 0))
                {
                    ret = 1;
                    goto fail_code_fun;
                }
                break;
            case 2:

                if(strcmp(p,"INIT_START") == 0)
                {
                    if(conf->sft_ipgroup_list[index].enable == 1)
                    {
                        conf->sft_ipgroup_list[index].enable = 0;
                        clean_ipgroup_entry(conf, index);
                    }
                    return ret;
                }
                else if(strcmp(p,"INIT_END") == 0)
                {
                    conf->sft_ipgroup_list[index].enable = 1;
                    return ret;
                }
                else if(strcmp(p,"add") == 0)
                    flag = 1;
                else if(strcmp(p,"del") == 0)
                    flag = 2;
                else
                {
                    RTE_ERR("%s(%d),bad add/del.\n",__FUNCTION__,__LINE__);
                    ret = 1;
                    goto fail_code_fun;
                }
                break;
            case 3:
                if(flag == 1) // add
                {
                    struct sft_ipgroup_entry *ipg;
                    struct hlist_head *head;
                    ipg = (struct sft_ipgroup_entry *)rte_zmalloc(NULL, sizeof(struct sft_ipgroup_entry), 0);
                    if(!ipg)
                    {
                        RTE_ERR("%s(%d),create ipgroup entry fail.\n",__FUNCTION__,__LINE__);
                        ret = 1;
                        goto fail_code_fun;
                    }
                    head = &conf->sft_ipgroup_list[index].hhead;

                    if(strstr(p,"/"))
                    {
                        unsigned int subnet_msk_;
                        uint8_t network_id_octets[4];
                        uint8_t *network_id = NULL; //Pointer pointing command
                        uint8_t *subnet_msk = NULL; //Pointer pointing value    
                        int ii;
                        int value_len;

                        ipg->ip_type = IPT_NETWORK_MASK_IP;
                        network_id = (uint8_t *)p;
                        value_len = strlen(p);
                        for(ii=0;ii<value_len;ii++)
                        {
                            if(p[ii] != '/')
                            {
                                if(p[ii] == '\r' || p[ii] == '\n')
                                    p[ii] = '\0';
                                else
                                    continue;
                            }
                            else
                            {
                                p[ii] = '\0';
                                subnet_msk = (uint8_t *)&p[ii+1];
                            }
                        }

                        //    printk("%s(%d),%s %s.\n",__FUNCTION__,__LINE__,network_id,subnet_msk);
                        subnet_msk_= dton((unsigned int )strtoul((char *)subnet_msk, NULL, 10));
                        //    printk("%s(%d),%0x.\n",__FUNCTION__,__LINE__,subnet_msk_);
                        if((parse_string_to_ip_address(network_id_octets, (char *)network_id)==TRUE))
                        {
                            ipg->network[0] = network_id_octets[0];
                            ipg->network[1] = network_id_octets[1];
                            ipg->network[2] = network_id_octets[2];    
                            ipg->network[3] = network_id_octets[3];
                            ipg->mask[0] = subnet_msk_&0xFF;
                            ipg->mask[1] = (subnet_msk_>>8)&0xFF;
                            ipg->mask[2] = (subnet_msk_>>16)&0xFF;
                            ipg->mask[3] = (subnet_msk_>>24)&0xFF;
                        }
                    }
                    else if(strstr(p,"-"))
                    {
                        int j = 1;
                        char *pp;
                        ipg->ip_type = IPT_START_END_IP;
                        while((pp = strsep(&p, "-")))
                        {
                            //printk("%s(%d),j is %d,%s.\n",__FUNCTION__,__LINE__,j,pp);
                            switch(j)
                            {
                                case 1:
                                    ipg->ip_start = ntohl(inet_addr(pp));
                                    break;
                                case 2:
                                    ipg->ip_end = ntohl(inet_addr(pp));
                                    break;
                            }
                            j++;
                        }
                    }
                    else
                    {
                        ipg->ip_type = IPT_ONE_IP;
                        ipg->ip = inet_addr(p);
                    }
                    //rte_spinlock_lock(&conf->sft_ipgroup_list[index].lock);
                    hlist_add_head(&ipg->hlist, head);
                    //rte_spinlock_unlock(&conf->sft_ipgroup_list[index].lock);
                }
                else if(flag == 2)//del
                {
                    //TODO free work
                    clean_ipgroup_entry(conf, index);
                }

                break;
            default :
                break;
        }
        i++;
    }
fail_code_fun:
    return ret;
}

inline int clean_ipgroup_entry(struct tc_private_t *conf, unsigned short ipg_idx)
{
    struct sft_ipgroup_entry *fdb;
    struct hlist_head *head;
    struct hlist_node *n;
    
    if(!conf)
    {
        return 0;
    }

    conf->sft_ipgroup_list[ipg_idx].enable = 0;

    head = &conf->sft_ipgroup_list[ipg_idx].hhead;
    if(head->first == NULL)
        return 0;

    RTE_DBG("%s(%d),delete ip group ID [%d].\n",__FUNCTION__,__LINE__,ipg_idx);

    //rte_spinlock_lock(&conf->sft_ipgroup_list[ipg_idx].lock);
    hlist_for_each_entry_safe(fdb, n, head, hlist){
        hlist_del(&fdb->hlist);
        rte_free(fdb);
    }
    //rte_spinlock_unlock(&conf->sft_ipgroup_list[ipg_idx].lock);

    return 0;
}

void pkg_queue_purge(struct sft_queue *queue)
{
    //struct rte_mbuf *skb;
    uint64_t skb;
    while (tc_dequeue(queue, (void *)&skb, sizeof(skb)) > 0)
    {
        rte_pktmbuf_free((struct rte_mbuf *)skb);
    } 
}

static void fdb_rcu_free(struct rcu_head *head)
{
	struct sft_flow_ip_entry *fdb = container_of(head, struct sft_flow_ip_entry,rcu);
    
    if(rte_timer_pending(&fdb->timeout_send))
    {
        rte_timer_stop_sync(&fdb->timeout_send);
    }
    
    if(fdb->pktQ)
    {
        pkg_queue_purge(fdb->pktQ);
        tc_queue_free(fdb->pktQ);
    }
    
    //rte_free(fdb);
    if(flow_ip_mem_cache)
    {
        rte_mempool_mp_put(flow_ip_mem_cache, fdb);
    }
    rte_atomic64_dec(&nums_sft_fip_entry);
    rte_atomic64_dec(&fip_num);
}

inline void flow_ip_delete(struct tc_private_t *conf, struct sft_flow_ip_entry *fdb)
{
    if(fdb->delete == 1)
        return;

    fdb->delete = 1;
    cds_hlist_del_rcu(&fdb->hlist);
    //hlist_del(&fdb->hlist);

    call_rcu(&fdb->rcu, fdb_rcu_free);
}

static void ksft_cleanup_oneip_handler(struct tc_private_t *conf, int flag)
{
    unsigned int i;
    struct sft_flow_ip_entry *fdb;
    struct cds_hlist_head *head;
    struct cds_hlist_node *n, *pos;
    struct ksft_hash_node * hash_node;
    
    if(!conf)
    {
        return;
    }

    for(i = 0 ; i < KSFT_FLOW_HASH_ITEM; i++) 
    {
        head = &ksft_flow_ip_hash[i].hhead;
        if(head->next == NULL)
            continue;

        rcu_read_lock();
        cds_hlist_for_each_entry_safe(fdb, pos, n, head, hlist)
        {
            if(flag == UN_TIMER)
            {
                //del_timer(&fdb->timeout_send);
                rte_timer_stop_sync(&fdb->timeout_send);
                //del_timer(&fdb->timeout_kill); 
                //rte_timer_stop_sync(&fdb->timeout_kill);
            }
            else if(flag == UN_ENTRY)
            {
                if(fdb->delete != 1)
                {
                    hash_node = container_of(head,struct ksft_hash_node,hhead);
                    rte_spinlock_lock(&hash_node->lock);
                    flow_ip_delete(conf, fdb);
                    rte_spinlock_unlock(&hash_node->lock);
                }
            }
            else
            {
                rcu_read_unlock();
                return;
            }
        }
        rcu_read_unlock();
    }
}

static void ksft_cleanup_pipe_handler(struct tc_private_t *conf, int flag)
{
    unsigned int i;
    struct sft_flow_datapipe_entry *fdb;
    
    if(!conf)
    {
        return;
    }

    for(i = 0 ; i < DATAPIPE_NUM; i++) 
    {
        fdb = &conf->ksft_datapipe[i];
        if(fdb->rate == 0) 
            continue;
        
        if(flag == UN_TIMER)
        {
            //del_timer(&fdb->timeout_send);
            //rte_timer_stop_sync(&fdb->timeout_send);
        }
        else if(flag == UN_ENTRY)
        {
            //skb_queue_purge(&fdb->pktQ);
            //if(fdb->pktQ)
            //{
            //    pkg_queue_purge(fdb->pktQ);
            //    kfifo_free(datapipe_fifo_mem_cache, datapipe_data_mem_cache, fdb->pktQ);
            //}
            
            fdb->rate = 0;
            //fdb->used = 0;
        }
    }
}

static void do_unreg_tc_work(struct tc_private_t *conf)
{
    int i;
    
    if(!conf)
    {
        return;
    }
    
    RTE_DBG("%s(%d).\n",__FUNCTION__,__LINE__);
    //clear_flags_tc_enable();
    //rcu_assign_pointer(sft_tx_handler, NULL);
    //ksft_cleanup_oneip_handler(conf, UN_TIMER);
    ksft_cleanup_pipe_handler(conf, UN_TIMER);
    //ksft_cleanup_oneip_handler(conf, UN_ENTRY);
    ksft_cleanup_pipe_handler(conf, UN_ENTRY);

    //free rule node
    for(i=0;i<FLOW_RULE_NUM; i++)
    {    
        if(conf->rule_active[i])    
        {  
            RTE_DBG("%s(%d),free active rule ID [%d].\n",__FUNCTION__,__LINE__,conf->rule_active[i]->index);
            rte_free(conf->rule_active[i]);
        }
        if(conf->rule_backup[i])    
        {  
            RTE_DBG("%s(%d),free backup rule ID [%d].\n",__FUNCTION__,__LINE__,conf->rule_backup[i]->index);
            rte_free(conf->rule_backup[i]);
        }
    }
    //free ip group node
    for(i = 0 ; i < IPGROUP_NUM; i++) 
    {    
        clean_ipgroup_entry(conf, i);
    }

    //rte_timer_stop(&conf->disp_conf.timeout);
    //rte_timer_stop(&conf->reload_tim);
    
    //rte_free(conf->ksft_flow_ip_hash);
    //memset(&conf->ksft_datapipe, 0, sizeof(struct sft_flow_datapipe_entry) * DATAPIPE_NUM);
    memset(&conf->sft_ipgroup_list, 0, sizeof(struct sft_ipgroup_list) * IPGROUP_NUM);
    memset(&conf->sft_flow_rule1, 0, sizeof(struct sft_flow_rule *) * FLOW_RULE_NUM);
    memset(&conf->sft_flow_rule2, 0, sizeof(struct sft_flow_rule *) * FLOW_RULE_NUM);
    //memset(&conf->disp_conf, 0, sizeof(struct dispatch_conf));
    conf->sft_flow_rule_start = 0;
    conf->sft_flow_rule_end = 0;
}


static int execute_app_request(struct tc_private_t *conf, char *buf, int len)
{
    int ret = 0;
    char *command=NULL; //Pointer pointing command
    char *value=NULL;   //Pointer pointing value
    RTE_DBG("%s(%d),buf:%s.\n",__FUNCTION__,__LINE__,buf);
    //add \0 at the end of buffer
    *(buf+len) = '\0'; len++;
    //Contents format: "<command>,<value>\n"
    command = buf;        
    {
        int i=0;
        int return_char_ctr = 0;

        for(i=0;i<len;i++)
        {
            if(*(buf+i)==',' && (i+1)<len)
            {
                value=(buf+i+1);
            }

            if(*(buf+i)==',')
            {
                *(buf+i)='\0';
            }

            if((*(buf+i)=='\n') || (*(buf+i)=='\r')) 
            {
                *(buf+i)='\0';
                return_char_ctr++;
            }
        }
    }

    RTE_DBG("%s(%d),command:%s,value:%s.\n",__FUNCTION__,__LINE__,command,value);

#if 0
    if(!strcmp(command, "enableTC"))
    {
         uint32_t enableTC = (uint32_t)strtoul(value, NULL, 10);
         if(enableTC == 1)
            set_flags_tc_enable();
         else
            clear_flags_tc_enable();
    }
    else if(!strcmp(command, "unreg_tc"))
    {
        //will unreg tc module
        local_bh_disable();
        do_unreg_tc_work();
        local_bh_enable();
    }
    else if(!strcmp(command, "unreg_cache"))
    {
        //will destroy fip cache
        //local_bh_disable();
        //do_uncache_tc_work();
        //local_bh_enable();
    }
    else 
#endif        
    if(!strcmp(command, "set_rule"))
    {
        ret = do_parse_rule_work(conf, value);
    }
    else if(!strcmp(command, "set_datapipe"))
    {
        ret = do_parse_datapipe_work(conf, value);
    }
    else if(!strcmp(command, "set_ipgroup"))
    {
        ret = do_parse_ipgroup_work(conf, value);
    }
    else
    {
        RTE_ERR("%s(%d),noknown command:%s.\n",__FUNCTION__,__LINE__,command);
    }
    return ret;
}

static int get_rule_conf(char *out, const char *cmd, char *buf)
{
    char *start = NULL, *end = NULL;
    
    start = strchr(buf, '[');
    if(!start)
        return -1;
    end = strchr(start, ']');
    if(!end)
        return -1;
    if(strstr(start, "ACTION:") == NULL)
        return -1;
        
    sprintf(out, "%s,%.*s", cmd, (int)(end - start - 1), start + 1);
    
    return end - start + strlen(cmd);
}

static int get_datapipe_conf(char *out, const char *cmd, char *buf)
{
    char *idx = NULL, *rate = NULL, *mid = NULL;
    
    //sscanf(buf, "%s|%s|%s", idx, mid, rate);
    idx = buf;
    
    mid = strchr(idx, '|');
    if(!mid)
    {
        return -1;
    }
    
    rate = strchr(mid + 1, '|');
    if(!rate)
    {
        return -1;
    }
 
    sprintf(out, "%s,%.*s add %s", cmd, (int)(mid - idx), idx, rate + 1);
    
    return strlen(out);
}

static int get_ipgroup_conf(char *out, const char *cmd, int *idx, int *start, char *buf)
{
    char *str = NULL;
    char *p = strstr(buf, "INDEX");
    if(p)
    {
        str = strchr(p, ' ');
        if(!str)
            return -1;
        
        *idx = atoi(str + 1);
        *start = 1;
        sprintf(out, "%s,%d INIT_START", cmd, *idx);
    }
    else
    {
        if(*start != 1)
        {
            return -1;
        }
        p = strstr(buf, "-->");
        if(p)
        {
            sprintf(out, "%s,%d INIT_END", cmd, *idx);
        }
        else
        {
            sprintf(out, "%s,%d add %s", cmd, *idx, buf);
        }
    }
        
    return strlen(out);
}

static int get_dispatcher_conf(struct tc_private_t *conf, char *buf)
{
    
    char *id = NULL, *stat = NULL, *per_id = NULL, *per_tim = NULL, *tim = NULL, *ruleset = NULL, *end = NULL;
    
    int smon = 0, emon = 0, cmon = 0;
    int sweek = 0, eweek = 0;

    id = buf;
    end = strchr(buf, '\n');
    if(!end)
    {
        return -1;
    }
    *end = '\0';
    
    stat = strstr(id, "active_id");
    if(stat)
    {
        return 0;
    }
    
    stat = strchr(id, ' ');
    if(!stat || stat + 1 > end)
    {
        return -1;
    }
    
    stat++;
    
    if(strncmp(stat, "enable", 6) != 0)
    {
        return 0;
    }
    
    per_id = strchr(stat, ' ');
    if(!per_id || per_id + 1 > end)
    {
        return -1;
    }
    
    per_id++;
    
    per_tim = strchr(per_id, ' ');
    if(!per_tim || per_tim + 1 > end)
    {
        return -1;
    }
    
    per_tim++;
    
    if(strncmp(per_id, "week", 4) == 0)
    {
        sscanf(per_tim, "%d-%d", &sweek, &eweek);
    }
    else if(strncmp(per_id, "month", 5) == 0)
    {
        sscanf(per_tim, "%d-%d-%d", &cmon, &smon, &emon);
    }
    
    tim = strchr(per_tim, ' ');
    if(!tim || tim + 1 > end)
    {
        return -1;
    }
    
    tim++; 

    ruleset = strchr(tim, ' ');
    if(!ruleset || ruleset + 1 > end)
    {
        return -1;
    }
    
    ruleset++;
    
    char *stime = NULL, *etime = NULL;
    stime = tim;
    etime = strchr(stime, '-');
    if(!etime || etime + 1 >= ruleset)
    {
        return -1;
    }
    
    etime++;
    
    struct dispat_rule *p = &conf->disp_conf.dis_ruleset[conf->disp_conf.ruleset_count];
    p->id = atoi(id);
    strncpy(p->stat, stat, per_id - stat - 1);
    strncpy(p->period_type, per_id, per_tim - per_id - 1);
    
    p->per_mon = cmon;
    p->start = strncmp(p->period_type, "week", 4) == 0 ? sweek : smon;
    p->end = strncmp(p->period_type, "week", 4) == 0 ? eweek : emon;
    
    strncpy(p->stime, stime, etime - stime - 1);
    strncpy(p->etime, etime, ruleset - etime -1);
    strcpy(p->rule_set_name, ruleset);
    
    conf->disp_conf.ruleset_count++;
    
    return 0;
}

static int read_conf_from_file(struct tc_private_t *conf, const char *filename, const char *cmd)
{
    char buf[MAX_FILE_PATH_LEN];
    char out[MAX_FILE_PATH_LEN];
    char *p = NULL;
    int len = 0;
    int idx = 0, start = 0;
    
    if(!filename || !cmd)
    {
        return -1;
    }
    
    FILE *fs = fopen(filename, "rt");
    if(!fs)
    {
        RTE_ERR("%s(%d), can not open configue file, file:%s\n",  __FUNCTION__, __LINE__, filename);
        return -1;
    }
    
    while((p = fgets(buf, MAX_FILE_PATH_LEN, fs)) != NULL)
    {
        RTE_DBG("cmd:%s, line:%s\n", cmd, p);
        
        if(*p == '#' || *(p + 1) == '#')
            continue;
        
        if(strcmp(cmd, CMD_SET_RULE) == 0)
        {
            if((len = get_rule_conf(out, cmd, p)) <= 0)
            {
                continue;
            }
        }
        else if(strcmp(cmd, CMD_SET_DATAPIPE) == 0)
        {
            if((len = get_datapipe_conf(out, cmd, p)) <= 0)
            {
                continue;
            }
        }
        else if(strcmp(cmd, CMD_SET_IPGROUP) == 0)
        {
            if((len = get_ipgroup_conf(out, cmd, &idx, &start, p)) <= 0)
            {
                continue;
            }
        }
        else if(strcmp(cmd, CMD_DISPATCHER) == 0)
        {
            if((len = get_dispatcher_conf(conf, p)) <= 0)
            {
                continue;
            }
            
        }
        else if(strcmp(cmd, CMD_DEFAULT_RULE) == 0)
        {
            char *end = strchr(p, '\n');
            if(end)
                *end = '\0';
            p[MAX_FILE_PATH_LEN - 2] = '\0';
            strcpy(conf->disp_conf.default_rule_name, p);
            continue;
        }
        RTE_DBG("%s(%d), wirte to conf cmd:%s\n",  __FUNCTION__, __LINE__, out);
        
        execute_app_request(conf, out, len);
    }
    
    fclose(fs);
    return 0;
}

static time_t get_file_last_modify_time(const char *file)
{
    struct stat buf;
    int result;
 
    //获得文件状态信息
    result =stat(file, &buf);
 
    //显示文件状态信息
    if(result != 0)
    {
        return 0;
    }
    else
    {
        return buf.st_mtime;
    }
}
static void set_activeid_for_dispatch(char * filename, int active_id)
{
    static int curr_id = 0;
    if(curr_id == active_id)
    {
        return;
    }
#if 0
    char cmd[1024];
    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "sed -i /active_id/cactive_id=%d %s", active_id, filename);
    system(cmd);
    curr_id = active_id;
#else    
    int fd = open(filename, O_RDWR);
    if(fd < 0)
        return;
  
    //获取文件大小
    struct stat sb;
    fstat(fd, &sb);

    unsigned char *buff = malloc(sb.st_size + 20);
    if(!buff)
        goto CLOSE_ALL;
  
  
    char strid[20];
    memset(strid, 0, sizeof(strid));
    sprintf(strid, "%d", active_id);
    int len = read(fd, buff, sb.st_size);
    if(len < sb.st_size)
        goto CLOSE_ALL;
    
    unsigned char *p = strstr(buff, "active_id=");
    int index = p - buff;
    if(p && index < sb.st_size)
    {
        p += 10;
        unsigned char *end = strchr(p, '\n');
        int short_len = end - (p + strlen(strid));
        if(end && end - buff < sb.st_size)
        {
            memmove(p + strlen(strid), end, sb.st_size - (end - buff));
            memcpy(p, strid, strlen(strid));

            len -= short_len;
           
            ftruncate(fd, 0);
            lseek(fd, 0, SEEK_SET);
            write(fd, buff, len);
            curr_id = active_id;
        }
    }

	free(buff);
  
CLOSE_ALL: 
    close(fd);
#endif    
}

static void dispatcher_conf_cb(struct rte_timer *tim, void *data)
{
    struct tm *ptr;
    time_t lt;
    
    int year = 0, mon = 0, day = 0, hour = 0, min = 0, sec = 0, week = 0;
    int i = 0;
    struct dispat_rule *p = NULL;
    struct dispat_rule *default_rule = NULL;
    char buf[MAX_FILE_PATH_LEN];
    
    struct tc_private_t *conf = (struct tc_private_t *)data;
    if(!conf)
    {
        return;
    }

    struct dispatch_conf *disp = &conf->disp_conf;
    
    if(disp->last_modify_time != 0)
    {
        sprintf(buf,"%s/%s",conf->rule_conf_path, disp->curr_rule_name); 
        time_t curr = get_file_last_modify_time(buf);
        if(curr != disp->last_modify_time)
        {
            strcpy(buf, "set_rule,INIT_START");
            execute_app_request(conf, buf, 19);

            sprintf(buf,"%s/%s",conf->rule_conf_path, disp->curr_rule_name);  
            RTE_DBG("%s(%d),rule group file: %s\n", __FUNCTION__, __LINE__, buf);  
            read_conf_from_file(conf, buf, CMD_SET_RULE);
            //disp->last_modify_time = get_file_last_modify_time(buf);
       
            strcpy(buf, "set_rule,INIT_END");
            execute_app_request(conf, buf, 17);
            disp->last_modify_time = curr;
        }
    }
   
    time_t curr = get_file_last_modify_time(conf->dispatch_conf_path);
    if(disp->last_dispatch_time != curr)
    {
        //memset(disp->curr_rule_name, 0, sizeof(disp->curr_rule_name));
        memset(disp->dis_ruleset, 0, sizeof(disp->dis_ruleset));
        disp->curr_rule_name[0] = '\0';
        
        disp->ruleset_count = 0;
        read_conf_from_file(conf, conf->dispatch_conf_path, CMD_DISPATCHER);
        
        disp->last_dispatch_time = curr;
    }
    
    curr = get_file_last_modify_time(conf->default_rule_conf_path);
    if(disp->last_default_time != curr)
    {
        //memset(disp->curr_rule_name, 0, sizeof(disp->curr_rule_name));
        memset(disp->default_rule_name, 0, sizeof(disp->default_rule_name));
        
        read_conf_from_file(priv, priv->default_rule_conf_path, CMD_DEFAULT_RULE);
        
        disp->last_default_time = curr;
    }

    lt =time(NULL);
    ptr=localtime(&lt);
 
    RTE_DBG("tim:%p, DATE:%d-%d-%d %d:%d:%d [%d]\n",tim, ptr->tm_year+1900, ptr->tm_mon+1, 
                            ptr->tm_mday, ptr->tm_hour, ptr->tm_min, ptr->tm_sec, ptr->tm_wday);

    year = ptr->tm_year+1900;
    mon = ptr->tm_mon + 1;
    day = ptr->tm_mday;
    hour = ptr->tm_hour;
    min = ptr->tm_min;
    sec = ptr->tm_sec;
    week = ptr->tm_wday;
	week = week ? week : 7;
    
    for(; i < disp->ruleset_count; i++)
    {
        p = &disp->dis_ruleset[i];
        
        if(strcmp(p->rule_set_name, disp->default_rule_name) == 0)
        {
            default_rule = p;
        }
        
        if(strncmp(p->period_type, "week", 4) == 0)
        {
            if(p->start <= week && p->end >= week)
            {
                RTE_DBG("%s(%d),WEEK:%d<=%d<=%d\n", __FUNCTION__, __LINE__, p->start, week, p->end);
            }
            else
            {
                continue;
            }
        }
        else if(strncmp(p->period_type, "month", 5) == 0)
        {
            if(p->per_mon != mon)
            {
                continue;
            }
        
            if(p->start <= mon && p->end >= mon)
            {
                RTE_DBG("%s(%d),MONTH:%d<=%d<=%d\n", __FUNCTION__, __LINE__, p->start, mon, p->end);
            }
            else
            {
                continue;
            }
        }

        char nowtime[32];
        sprintf(nowtime, "%02d:%02d:%02d", hour, min, sec);
        RTE_DBG("%s(%d), time:%s---%s---%s\n", __FUNCTION__, __LINE__, p->stime, nowtime, p->etime);

        if(strncmp(p->stime, nowtime, 8) <= 0 && strncmp(p->etime, nowtime, 8) >= 0)
        {
            if(strcmp(p->rule_set_name, disp->curr_rule_name) == 0)
            {
                return;
            }
            strcpy(disp->curr_rule_name, p->rule_set_name);
            
            set_activeid_for_dispatch(conf->dispatch_conf_path, p->id);
            disp->last_dispatch_time = get_file_last_modify_time(conf->dispatch_conf_path);
            
            strcpy(buf, "set_rule,INIT_START");
            execute_app_request(conf, buf, 19);

            sprintf(buf,"%s/%s",conf->rule_conf_path, p->rule_set_name);  
            RTE_DBG("%s(%d),rule group file: %s\n", __FUNCTION__, __LINE__, buf);  
            read_conf_from_file(conf, buf, CMD_SET_RULE);
            disp->last_modify_time = get_file_last_modify_time(buf);
       
            strcpy(buf, "set_rule,INIT_END");
            execute_app_request(conf, buf, 17);

            return;
        } 
    }
    
    if(default_rule)
    {
        strcpy(disp->curr_rule_name, default_rule->rule_set_name);
            
        //set_activeid_for_dispatch(conf->dispatch_conf_path, default_rule->id);
            
        strcpy(buf, "set_rule,INIT_START");
        execute_app_request(conf, buf, 19);

        sprintf(buf,"%s/%s",conf->rule_conf_path, default_rule->rule_set_name);  
        RTE_DBG("%s(%d),rule group file: %s\n", __FUNCTION__, __LINE__, buf);  
        read_conf_from_file(conf, buf, CMD_SET_RULE);
        disp->last_modify_time = get_file_last_modify_time(buf);
       
        strcpy(buf, "set_rule,INIT_END");
        execute_app_request(conf, buf, 17);
        set_activeid_for_dispatch(conf->dispatch_conf_path, default_rule->id);
        disp->last_dispatch_time = get_file_last_modify_time(conf->dispatch_conf_path);
    }
    else
    {
        // No find rule for utc
        strcpy(buf, "set_rule,INIT_START");
        execute_app_request(conf, buf, 19);
        strcpy(buf, "set_rule,INIT_END");
        execute_app_request(conf, buf, 17);
        set_activeid_for_dispatch(conf->dispatch_conf_path, -1);
        disp->last_dispatch_time = get_file_last_modify_time(conf->dispatch_conf_path);
    } 
}

#if 0
static void rule_group_dir_list(struct tc_private_t *conf, const char* file, char *name)  
{  
    DIR              *pDir ;  
    struct dirent    *ent  ;  
    char              childpath[MAX_FILE_PATH_LEN];  
    
    if(!conf || !file)
        return;
    
    pDir=opendir(file); 
    if(!pDir)
    {
        RTE_ERR("%s(%d), rule group config dir is not existed, dir:%s\n", __FUNCTION__, __LINE__, file);
        return;
    }
    memset(childpath,0,sizeof(childpath)); 
    
    char buf[32];
    strcpy(buf, "set_rule,INIT_START");
    execute_app_request(conf, buf, 19);
    
    while((ent=readdir(pDir))!=NULL)      
    {  
        if(ent->d_type & DT_DIR)  
        {  
            if(strcmp(ent->d_name,".")==0 || strcmp(ent->d_name,"..")==0)  
                continue;  

            sprintf(childpath,"%s/%s",file,ent->d_name);  
            RTE_DBG("%s(%d),path:%s is a sub dir\n", __FUNCTION__, __LINE__, childpath);  
            //rule_group_dir_list(conf, childpath);  
        }  
        else
        {
            sprintf(childpath,"%s/%s",file,ent->d_name);  
            RTE_DBG("%s(%d),rule group file: %s\n", __FUNCTION__, __LINE__, childpath);  
            read_conf_from_file(conf, childpath, CMD_SET_RULE);
        }
    }  
    strcpy(buf, "set_rule,INIT_END");
    execute_app_request(conf, buf, 17);
    
    closedir(pDir);
}
#endif

static void reload_conf_cb(struct rte_timer *tim, void *data)
{
    struct tc_private_t *priv = (struct tc_private_t *)data;
    static uint64_t load_idx = 0;
    
    if(!priv)
        return;

    if(!is_tc_reloading())
        return;
    
    read_conf_from_file(priv, priv->ipgroup_conf_path, CMD_SET_IPGROUP);
    read_conf_from_file(priv, priv->datapipe_conf_path, CMD_SET_DATAPIPE);
    
    //priv->disp_conf.last_dispatch_time = 0;
    //dispatcher_conf_cb(NULL, (void *)priv);
    
    check_fin_reload_conf();
}

struct tc_private_t * sft_init_tc_work()
{
    uint32_t i;
    
    RTE_ERR("lcore [%d] tc config file load start ...\n", rte_lcore_id());


    priv = (struct tc_private_t *)rte_zmalloc(NULL, sizeof(struct tc_private_t), 0);
    if(!priv)
    {
        RTE_ERR("%s(%d), tc_private_t rte_zmalloc failed, size:%lu\n", __FUNCTION__, __LINE__, sizeof(struct tc_private_t));
        return NULL;
    }
    
    priv->rule_active = priv->sft_flow_rule1;
    priv->rule_backup = priv->sft_flow_rule2;
    
    //memset(ksft_datapipe,0,sizeof(ksft_datapipe));
    for(i = 0 ; i < DATAPIPE_NUM; i++) 
    {    
        rte_atomic32_init(&priv->ksft_datapipe[i].token_entry.token);
    }

    //memset(rule_active,0,sizeof(sft_flow_rule2));
    //memset(rule_backup,0,sizeof(sft_flow_rule1));
    //memset(sft_ipgroup_list,0,sizeof(sft_ipgroup_list));
    for(i = 0 ; i < IPGROUP_NUM; i++) 
    {    
        INIT_HLIST_HEAD(&priv->sft_ipgroup_list[i].hhead);
        //rte_spinlock_init(&priv->sft_ipgroup_list[i].lock);
    } 

#if 0    
    if(hlist_empty(&priv->free_queue_head))
    {
        INIT_HLIST_HEAD(&priv->free_queue_head);
    }
#endif
/*   
#define RULE_CONF_PATH "/root/angus/dpdk/arbiter_dpdk/dpdk-2.0.0/examples/arbiter/build/conf/rulesgroup"  
#define IPGROUP_CONF_PATH "/root/angus/dpdk/arbiter_dpdk/dpdk-2.0.0/examples/arbiter/build/conf/ipgroup.conf"  
#define DATAPIPE_CONF_PATH "/root/angus/dpdk/arbiter_dpdk/dpdk-2.0.0/examples/arbiter/build/conf/datapipe.conf"  
#define DISCONF_CONF_PATH "/root/angus/dpdk/arbiter_dpdk/dpdk-2.0.0/examples/arbiter/build/conf/rule_dispath.conf"
    
    strcpy(priv->rule_conf_path, RULE_CONF_PATH);
    strcpy(priv->ipgroup_conf_path, IPGROUP_CONF_PATH);
    strcpy(priv->datapipe_conf_path, DATAPIPE_CONF_PATH);
    strcpy(priv->dispatch_conf_path, DISCONF_CONF_PATH);
 */
 
    const char *path = "/opt/utc/conf/";
    if(get_conf_path())
    {
        path = get_conf_path();
    }

    sprintf(priv->rule_conf_path, "%s/rulesgroup", path);
    sprintf(priv->ipgroup_conf_path, "%s/ipgroup.conf", path);
    sprintf(priv->datapipe_conf_path, "%s/datapipe.conf", path);
    sprintf(priv->dispatch_conf_path, "%s/rule_dispath.conf", path);
    sprintf(priv->default_rule_conf_path, "%s/default_ruleset.conf", path);
    
    read_conf_from_file(priv, priv->ipgroup_conf_path, CMD_SET_IPGROUP);
    read_conf_from_file(priv, priv->datapipe_conf_path, CMD_SET_DATAPIPE);
    read_conf_from_file(priv, priv->dispatch_conf_path, CMD_DISPATCHER);
    read_conf_from_file(priv, priv->default_rule_conf_path, CMD_DEFAULT_RULE);
    priv->disp_conf.last_dispatch_time = get_file_last_modify_time(priv->dispatch_conf_path);
    priv->disp_conf.last_default_time = get_file_last_modify_time(priv->default_rule_conf_path);
    //rule_group_dir_list(priv, RULE_CONF_PATH);
    dispatcher_conf_cb(NULL, (void *)priv);
   
    uint64_t timer10s = rte_get_timer_hz() * 5; 
    if(!rte_timer_pending(&priv->disp_conf.timeout))
    {
        rte_timer_init(&priv->disp_conf.timeout);
        rte_timer_reset(&priv->disp_conf.timeout, timer10s, 
                    PERIODICAL, rte_lcore_id(), dispatcher_conf_cb, 
                    (void *)priv);
    }
    
    if(!rte_timer_pending(&priv->reload_tim))
    {
        uint64_t _1s = rte_get_timer_hz();
        rte_timer_init(&priv->reload_tim);
        rte_timer_reset(&priv->reload_tim, _1s * 2, PERIODICAL, rte_lcore_id(), reload_conf_cb, (void *)priv);
    }

    RTE_ERR("lcore [%d] tc config file load success ...\n", rte_lcore_id());
    return priv;
}

void free_sft_init_tc_work(void)
{
    if(!priv)
        return;
    
    do_unreg_tc_work(priv);

    rte_free(priv);
}



