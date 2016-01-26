#include <stdio.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
 
#include <rte_log.h>

#include "session_config.h"
#include "utils.h"
#include "global.h"

#define TRUE    1
#define FALSE   0

#define MAX_LINE_NUM 128

//#define DEBUG_L2CT_YHL

#define RTE_LOGTYPE_L2CT RTE_LOGTYPE_USER1

#define RTE_ERR(x,args...) \
    do{                         \
        RTE_LOG(ERR, L2CT, x, ##args);      \
    } while(0)

#define RTE_INF(x,args...) \
    do{                         \
        RTE_LOG(INFO, L2CT, x, ##args);      \
    } while(0)

#ifdef DEBUG_L2CT_YHL
#define RTE_DBG(x,args...) \
    do{                         \
        RTE_LOG(DEBUG, L2CT, x, ##args);      \
    } while(0)
#else
#define RTE_DBG(...)
#endif

static int parse_string_to_ip_address(uint8_t *ip, char *buffer)
{
	char *start = buffer;
	char temp[30];
	int temp_ptr = 0;
	int dot_count = 0;
	
	if(buffer==NULL) return FALSE;
	if(ip==NULL) return FALSE;

	//contains 3 "." ?
	{	dot_count = 0;
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
		{	temp[temp_ptr] = *start;
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
	ip[3] = (uint8_t)strtoul(temp, NULL, 10);
	RTE_DBG("parse_string_to_ip_address: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
	return TRUE;
} /* parse_string_to_ip_address */

static int execute_app_request(char *buf, int len)
{
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
    
/*
	if(!strcmp(command, "enableL2CT"))
	{
		 u32 enable = (u32)strtoul(value, NULL, 10);
		 if(enable == 1)
			set_flags_l2ct_enable();
		 else
			clear_flags_l2ct_enable();
	}
	
	else if(!strcmp(command, "enableDPI"))
	{
		 u32 enable = (u32)simple_strtoul(value, NULL, 10);
		 if(enable == 1)
			set_flags_dpi_enable();
		 else
			clear_flags_dpi_enable();
	}
	else if(!strcmp(command, "enableTC"))
	{
		 u32 enableTC = (u32)simple_strtoul(value, NULL, 10);
		 if(enableTC == 1)
			set_flags_tc_enable();
		 else
			clear_flags_tc_enable();
	}	
	else if(!strcmp(command, "unreg_surfront"))
	{
		//will unreg surfront module
		local_bh_disable();
		do_unreg_surfront_work();
		local_bh_enable();
	}
	else if(!strcmp(command, "debuglevel"))
	{
		sft_debug_flags = simple_strtoul(value, NULL, 10);
	}
	else 
*/    
    if(!strcmp(command, "set_localip"))
	{
		//echo "set_localip,192.168.1.70" > /proc/surfront/io
		if((*value) == 'X')
		{
			set_local_addr(0);
		}
		else
		{
			uint8_t network_id_octets[4];
			if((parse_string_to_ip_address(network_id_octets, value)==TRUE))
			{
				int last = network_id_octets[3] & 0xFF;
				RTE_DBG("%s(%d),last is %d.\n",__FUNCTION__,__LINE__,last);
				RTE_DBG("%s(%d),%u.%u.%u.%u.\n",__FUNCTION__,__LINE__,RTE_NIPQUAD(network_id_octets));
				set_local_addr(network_id_octets[0] | (network_id_octets[1] << 8) | (network_id_octets[2] << 16) | (network_id_octets[3] << 24));
				RTE_DBG("%s(%d),local ip %u.%u.%u.%u.\n",__FUNCTION__,__LINE__,RTE_NIPQUAD(get_local_addr));
			}
		}
	}
	else
	{
		RTE_DBG("%s(%d),noknown command:%s.\n",__FUNCTION__,__LINE__,command);
	}
	return 0;
}

static int get_local_ip(char *name, char *ip) 
{
    int ret = -1;
    struct ifaddrs *ifAddrStruct;
    void *tmpAddrPtr=NULL;
    
    getifaddrs(&ifAddrStruct);
    while (ifAddrStruct != NULL) {
            if (ifAddrStruct->ifa_addr->sa_family==AF_INET) {
                    RTE_DBG("get_local_ip name:%s, addr:%s\n", name, ifAddrStruct->ifa_name);
                    if(strcmp(name, ifAddrStruct->ifa_name) == 0)
                    {
                        tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
                        inet_ntop(AF_INET, tmpAddrPtr, ip, INET_ADDRSTRLEN);
                        RTE_DBG("%s IP Address:%s\n", ifAddrStruct->ifa_name, ip);
                        ret = 0;
                        //break;
                    }
                    
            }
            ifAddrStruct=ifAddrStruct->ifa_next;
    }
    //free ifaddrs
    freeifaddrs(ifAddrStruct);
    return ret;
}

extern uint8_t   pcap_dump_control_flag;
extern uint64_t pcap_dump_file_size;
extern uint64_t pcap_dump_packet_num;
extern uint8_t pcap_dump_special_flag;
extern uint64_t pcap_dump_special_proto_mark;
static int load_conf_from_file(const char *filename)
{
    char buf[MAX_LINE_NUM];
    char ip[MAX_LINE_NUM];
    char *p = NULL;
    char *key = NULL, *value = NULL;
    
    if(!filename)
    {
        return -1;
    }
    
    FILE *fs = fopen(filename, "rt");
    if(!fs)
    {
        RTE_ERR("%s(%d), can not open configue file, file:%s\n",  __FUNCTION__, __LINE__, filename);
        return -1;
    }
    
    while((p = fgets(buf, MAX_LINE_NUM, fs)) != NULL)
    {
        if(*p == '#' || *(p + 1) == '#')
            continue;

        RTE_DBG("%s(%d), Read new line:%s\n",  __FUNCTION__, __LINE__, buf);
        key = p;
        value = strchr(p, '=');
        if(!value)
            continue;
        if(strncmp(key, "mgt_device", 10) == 0)
        {
            p = strchr(value, '\n');
            if(p)
            {
                *p = '\0';
            }
            value = strchr(value, '"');
            if(value)
            {
                value++;
                p = strchr(value, '"');
                if(p)
                {
                    *p = '\0';
                }
            }

            if(get_local_ip(value, ip) < 0)
            {
                continue;
            }
            sprintf(buf, "set_localip,%s", ip);
            execute_app_request(buf, strlen(buf));
        }
        else if(strncmp(key, "enableL2CT", 10) == 0)
        {
            int flag = (int)strtoul(value + 1, NULL, 10);
            
            if(flag)
            {
                set_flags_l2ct_enable();
            }
            else
            {
                clear_flags_l2ct_enable();
            }
        }
        else if(strncmp(key, "enableDPI", 9) == 0)
        {
            int flag = (int)strtoul(value + 1, NULL, 10);
            
            if(flag)
            {
                set_flags_dpi_enable();
            }
            else
            {
                clear_flags_dpi_enable();
            }
        }
        else if(strncmp(key, "enableTC", 8) == 0)
        {
            int flag = (int)strtoul(value + 1, NULL, 10);
            
            if(flag)
            {
                set_flags_tc_enable();
            }
            else
            {
                clear_flags_tc_enable();
            }
        }
        else if(strncmp(key, "enableOFO", 9) == 0)
        {
            uint64_t flag = (int)strtoul(value + 1, NULL, 10);
            
            if(flag)
            {
                set_flags_ofo_enable(flag);
            }
        }
        else if(strncmp(key, "enableDynamic", 13) == 0)
        {
            uint64_t flag = (int)strtoul(value + 1, NULL, 10);
            
            if(flag)
            {
                set_flags_dynamic_enable(flag);
            }
        }
        else if(strncmp(key, "hashBucketSize", 14) == 0)
        {
            uint64_t flag = (int)strtoul(value + 1, NULL, 10);
            
            if(flag)
            {
                set_hash_size(flag);
            }
        }
        else if(strncmp(key, "enableDumpPcapFlag", 18) == 0)
        {
            uint64_t flag = (int)strtoul(value + 1, NULL, 10);
            
            pcap_dump_control_flag = flag;
        }
        else if(strncmp(key, "dumpPcapFileSize", 16) == 0)
        {
            uint64_t flag = (int)strtoul(value + 1, NULL, 10);
            
            pcap_dump_file_size = flag;
        }
        else if(strncmp(key, "dumpPcapPacketNum", 17) == 0)
        {
            uint64_t flag = (int)strtoul(value + 1, NULL, 10);
            
            pcap_dump_packet_num = flag;
        }
        else if(strncmp(key, "dumpPcapSpecialFlag", 19) == 0)
        {
            uint64_t flag = (int)strtoul(value + 1, NULL, 10);
            
            pcap_dump_special_flag = flag;
        }
        else if(strncmp(key, "dumpPcapSpecialProtoMark", 24) == 0)
        {
            uint64_t flag = (int)strtoul(value + 1, NULL, 10);
            
            pcap_dump_special_proto_mark = flag;
        }
    }
    
    fclose(fs);
    return 0;
}

static int load_running_dev_from_file(const char *filename)
{
    char buf[MAX_LINE_NUM];
    char *p = NULL;
    
    if(!filename)
    {
        return -1;
    }
    
    dev_name_init();
    
    FILE *fs = fopen(filename, "rt");
    if(!fs)
    {
        RTE_ERR("%s(%d), can not open configue file, file:%s\n",  __FUNCTION__, __LINE__, filename);
        return -1;
    }
    
    while((p = fgets(buf, MAX_LINE_NUM, fs)) != NULL)
    {
        if(*p == '#' || *(p + 1) == '#')
            continue;

        RTE_DBG("%s(%d), Read new line:%s\n",  __FUNCTION__, __LINE__, buf);
        p = strchr(p, '\n');
        if(p)
        {
            *p = '\0';
            add_dev_by_name(buf);
        }
    }
    
    fclose(fs);
    return 0;
}

static int load_inner_outer_dev_from_file(const char *filename)
{
    char buf[MAX_LINE_NUM];
    char *p = NULL;
    char *inner = NULL;
    char *outer = NULL;
    
    if(!filename)
    {
        return -1;
    }
    
    inner_outer_port_init();
    
    FILE *fs = fopen(filename, "rt");
    if(!fs)
    {
        RTE_ERR("%s(%d), can not open configue file, file:%s\n",  __FUNCTION__, __LINE__, filename);
        return -1;
    }
    
    while((p = fgets(buf, MAX_LINE_NUM, fs)) != NULL)
    {
        if(*p == '#' || *(p + 1) == '#')
            continue;

        RTE_DBG("%s(%d), Read new line:%s\n",  __FUNCTION__, __LINE__, buf);
        inner = p;
        outer = strchr(p, '|');
        if(!outer)
            continue;
        *outer = '\0';
        outer++;
        if((p = strchr(outer, '\n')) != NULL)
        {
            *p = '\0';
        }
        
        add_inner_outer_port(inner, outer);
    }
    
    fclose(fs);
    return 0;
}

void sess_config_init(const char *filepath)
{
    if(!filepath)
    {
        return;
    }
    
    char buf[1024];
    sprintf(buf, "%s/tc_nic.conf", filepath);
    load_conf_from_file(buf);
    sprintf(buf, "%s/utc.conf", filepath);
    load_conf_from_file(buf);
    if(!is_dev_load())
    {
        sprintf(buf, "%s/.running_nic.conf", filepath);
        load_running_dev_from_file(buf);
        sprintf(buf, "%s/nic.conf", filepath);
        load_inner_outer_dev_from_file(buf);
    }
}
