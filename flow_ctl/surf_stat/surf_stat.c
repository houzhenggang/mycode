#include <stdio.h>
#include <stdlib.h>  
#include <string.h>  
#include <errno.h> 

#include <sys/types.h>  
#include <sys/ipc.h>
#include  <sys/shm.h>
#include <sys/socket.h>  
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <netinet/in.h>  
#include <arpa/inet.h>  

#include <ifaddrs.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>

#include <dirent.h>
#include <sys/stat.h>

#include "log.h"
#include "common.h"
#include "data_aggregation.h"
#include "data_sort.h"
#include "data_stream.h"
#include "data_test.h"
#include "arbiter_handler.h"
#include "data_db.h"

extern int ARBITER_PORT;
extern int STREAM_MESG_NUM_LIMIT_MAX;
extern int STREAM_MESG_NUM_LIMIT_FLAG;


int parse_input_parameters(int argc, char **argv);

int init_sys_info();
int init_global_struct();
int init_cfg_info();
void destroy_info();

void do_process_data_aggregation();
void  do_process_sort();

void process_baseapp_data_from_db();
void process_baseip_data_from_db();
void process_baselink_data_from_db();
void save_data_aggregation_result_to_db();

void print_baselink_info(const char* dbname);

static inline void input_error()
{
    printf("parameter is not valid!\n");
    printf("/opt/utc/bin/surf_stat  baseip -s [link|up|down|total] -u [inner|outer] -l [begin line] [end line] -a [appid] -f [N]\n");
    printf("/opt/utc/bin/surf_stat  baselink -u [inner|outer|all] -i [ IP ] -l [begin line] [end line] -a [appid]\n");
    printf("/opt/utc/bin/surf_stat  baseapp -s [link|up|down|total] -l [begin line] [end line] -f [N]\n");
}

uint32_t g_aggregation_line = 0; //use to response to web
uint32_t g_display_res_line = 0; //use to response to web

uint32_t g_display_req_line = G_DISPLAY_LINE_DEFAULT; //web request display count
uint32_t g_display_req_begin_line = G_DISPLAY_LINE_DEFAULT; //web request display begin index
uint32_t g_display_req_end_line = G_DISPLAY_LINE_DEFAULT; //web request display end index

uint32_t g_flush_interval = G_FLUSH_INTERVAL_DEFAULT; //seconds, default is 0
int32_t  g_data_agg_class_condition_appid = G_DATA_AGG_APPID_DEFAULT;
uint32_t g_baselink_ip = G_BASELINK_IP_DEFAULT;

uint32_t PRINT_STAT_RESULT = 0;

merge_sorted_node_t* merglist = NULL;  

int main(int argc, char **argv)  
{   
#if 1
     if(0 == isArbiterExist())
    {
        marbit_send_log(ERROR,"arbiter is not start!\n");
        exit(1);
    }
#endif 
    //init sys info
    if(0 != init_sys_info())
    {
        marbit_send_log(ERROR,"Failed to init_sys_info!\n");
        exit(1);
    }
  
    //parse the parameters
    if(0 != parse_input_parameters(argc, argv))
    {
        destroy_info();
        
        input_error();
        
        exit(1);
    }

    //time_t begin_time = time(NULL);
  //  time_t do_process_data_stream_time= time(NULL);
   // time_t do_process_data_aggregation_time= time(NULL);
   // time_t do_process_sort_time = time(NULL);
   // time_t print_baselink_time = time(NULL);
    //time_t print_sorted_list_time = time(NULL);


    struct timeval start, getFromArbiter, printLink, dataAggregation, quickSort, printSort, end;
    gettimeofday( &start, NULL );
        
#if 1
    //data_stream, check need get data stream from arbiter or configure file
    if(0 == isNeedReadFromDB(DATA_STREAM_FILE_PATH, g_flush_interval))
    {  
        //set_timer();
        if(0 != do_process_from_arbiter())
        {
            marbit_send_log(ERROR,"Failed to get data stream from arbiter\n");
            exit(1);
        }
    }
    gettimeofday( &getFromArbiter, NULL );
#endif

    if(SORTED_BASE_LINK == g_sorted_list_index)
    {
        print_baselink_info(DATA_STREAM_FILE_PATH);
        gettimeofday( &printLink, NULL );
    }
    else if(SORTED_BASE_APP == g_sorted_list_index || SORTED_BASE_IP == g_sorted_list_index)
    {
        //data_aggregation
        do_process_data_aggregation();
        gettimeofday( &dataAggregation, NULL );

        #if 1
        
        MergeSort(&merglist);  
        gettimeofday( &quickSort, NULL );
        
        printMergeList(merglist);
         gettimeofday( &printSort, NULL );
         
        destroyMergelist(merglist);

        #else
        struct list_head *head = &sorted_list_arry[SORTED_BASE_AGG].list;
        //struct list_head *head = &sorted_list_arry[g_sorted_list_index].list;
        
        struct list_head *first = head->next;
        struct list_head *last = head->prev;

        sorted_node_t *pstHead = list_entry(head, sorted_node_t, list); 

        quick_sort(head, first, last);
        gettimeofday( &quickSort, NULL );

        print_sorted_list(SORTED_BASE_AGG);
        //print_sorted_list(g_sorted_list_index);
        gettimeofday( &printSort, NULL );
        #endif
    }

    destroy_info();
    gettimeofday( &end, NULL );

    if(logdebug.g_trace_enable_flag > 0)
    {
        marbit_send_log(INFO,"=======================================\n");
        int timeuse = 0;
        
        if(SORTED_BASE_LINK == g_sorted_list_index)
        {
            timeuse = 1000000 * ( end.tv_sec - start.tv_sec ) + end.tv_usec - start.tv_usec;
            marbit_send_log(INFO,"all used time = %lu us\n", timeuse);

            timeuse = 1000000 * ( getFromArbiter.tv_sec - start.tv_sec ) + getFromArbiter.tv_usec - start.tv_usec;
            marbit_send_log(INFO,"getFromArbiter used time = %lu us\n", timeuse);


            timeuse = 1000000 * ( printLink.tv_sec - getFromArbiter.tv_sec ) + printLink.tv_usec - getFromArbiter.tv_usec;
            marbit_send_log(INFO,"printLink used time = %lu us\n", timeuse);

            timeuse = 1000000 * ( end.tv_sec - printLink.tv_sec ) + end.tv_usec - printLink.tv_usec;
            marbit_send_log(INFO,"destroy used time = %lu us\n", timeuse);
        }
        else
        {
             timeuse = 1000000 * ( end.tv_sec - start.tv_sec ) + end.tv_usec - start.tv_usec;
            marbit_send_log(INFO,"all used time = %lu us\n", timeuse);

            timeuse = 1000000 * ( getFromArbiter.tv_sec - start.tv_sec ) + getFromArbiter.tv_usec - start.tv_usec;
            marbit_send_log(INFO,"getFromArbiter used time = %lu us\n", timeuse);

            timeuse = 1000000 * ( dataAggregation.tv_sec - getFromArbiter.tv_sec ) + dataAggregation.tv_usec - getFromArbiter.tv_usec;
            marbit_send_log(INFO,"dataAggregation used time = %lu us\n", timeuse);

            timeuse = 1000000 * ( quickSort.tv_sec - dataAggregation.tv_sec ) + quickSort.tv_usec - dataAggregation.tv_usec;
            marbit_send_log(INFO,"quickSort used time = %lu us\n", timeuse);


            timeuse = 1000000 * ( printSort.tv_sec - quickSort.tv_sec ) + printSort.tv_usec - quickSort.tv_usec;
            marbit_send_log(INFO,"printSort used time = %lu us\n", timeuse);

            timeuse = 1000000 * ( end.tv_sec - printSort.tv_sec ) + end.tv_usec - printSort.tv_usec;
            marbit_send_log(INFO,"destroy used time = %lu us\n", timeuse);
        }
    }
    
    
    return 0;  
}  

void process_baseapp_data_from_db()
{    
    int insert_num = 0;

    FILE *fp;
    char line[1024] = {0};
    if ((fp = fopen(DATA_BASEAPP_FILE_NAME, "rb")) == NULL) 
    {
        marbit_send_log(ERROR,"-------open file [%s] fail.\n", DATA_BASEAPP_FILE_NAME);
        return;
    }

    char   *stop_at  =NULL ;  
    sorted_node_t *tmp;
    sorted_node_t *node;
    struct list_head *pos = NULL;
    
    struct list_head *head  =  &sorted_list_arry[g_sorted_list_index].list;
    uint32_t i = 0;
    while(fgets(line, 1024, fp))  
    {
         tmp = (sorted_node_t *)malloc(sizeof(sorted_node_t));
         if(NULL != tmp)
         {
            tmp->data.appid = strtoul(&line[0], &stop_at, 10);
            tmp->data.con_nums = strtoul(&line[16], &stop_at, 10);
            tmp->data.up_bps = strtoul(&line[32], &stop_at, 10);
            tmp->data.down_bps = strtoul(&line[48], &stop_at, 10);
            tmp->data.total_bits = strtoul(&line[64], &stop_at, 10);

             //fill sorted_list
             list_add(&tmp->list, &sorted_list_arry[g_sorted_list_index].list);

             // do_process_insert_sorted_list(tmp, g_data_agg_class, g_sorted_list_index, g_data_sort_condition);

             insert_num++;
          }
     }
    
    marbit_send_log(INFO,"insert_num = %d\n", insert_num);

    fclose(fp); 

    return;
}


void process_baseip_data_from_db()
{    
    int insert_num = 0;
    //read configure file
    FILE *fp;
    char line[1024] = {0};
    if ((fp = fopen(DATA_BASEIP_FILE_NAME, "rb")) == NULL) 
    {
        marbit_send_log(ERROR,"-------open file [%s] fail.\n", DATA_BASEIP_FILE_NAME);
        return;
    }

    char   *stop_at  =NULL ;  
    sorted_node_t *tmp;
    sorted_node_t *node;
    struct list_head *pos = NULL;
    
    struct list_head *head  =  &sorted_list_arry[g_sorted_list_index].list;
    uint32_t i = 0;
    sorted_node_t *p;
    while(fgets(line, 1024, fp))  
    {
        tmp = (sorted_node_t *)malloc(sizeof(sorted_node_t));
        if(NULL != tmp)
        {
            tmp->data.ip = strtoul(&line[0], &stop_at, 10);
            tmp->data.ip_con_nums = strtoul(&line[16], &stop_at, 10);
            tmp->data.ip_up_size = strtoul(&line[32], &stop_at, 10);
            tmp->data.ip_down_size = strtoul(&line[48], &stop_at, 10);
            tmp->data.ip_total_size = strtoul(&line[64], &stop_at, 10);

            list_add(&tmp->list, &sorted_list_arry[g_sorted_list_index].list);
            //do_process_insert_sorted_list(tmp, g_data_agg_class, g_sorted_list_index, g_data_sort_condition);

            insert_num++;
        }
     }
    
    marbit_send_log(INFO,"insert_num = %d\n", insert_num);

    fclose(fp); 

    return;
}

void process_baselink_data_from_db()
{    
    struct msg_st mesg;

    //read configure file
    FILE *fp;
    char line[1024] = {0};
    if ((fp = fopen(DATA_STREAM_FILE_NAME, "rb")) == NULL) 
    {
        marbit_send_log(ERROR,"-------open file [%s] fail.\n", DATA_STREAM_FILE_NAME);
        return;
    }
    
    char   *stop_at  =NULL ;  
    int insert_num = 0;
    sorted_node_t *tmp = NULL;
    uint32_t data_link_ip = 0;
    
    while(fgets(line, 1024, fp))  
     {
        mesg.ip_src = strtoul(&line[0], &stop_at, 10);
        mesg.port_src = strtoul(&line[16], &stop_at, 10);
        mesg.ip_dst = strtoul(&line[32], &stop_at, 10);
        mesg.port_dst = strtoul(&line[48], &stop_at, 10);
        mesg.proto = strtoul(&line[64], &stop_at, 10);
        mesg.proto_mark = strtoul(&line[80], &stop_at, 10);
        mesg.up_bytes = strtoul(&line[96], &stop_at, 10);
        mesg.down_bytes = strtoul(&line[112], &stop_at, 10);
        mesg.duration_time = strtoul(&line[128], &stop_at, 10);

         //appid condition
        if(g_data_agg_class_condition_appid != -1)
        {
            if(g_data_agg_class_condition_appid != mesg.proto_mark)
            {
                continue;
            }
        }

        if (DATA_AGG_CLASS_LINK_IP_ALL != g_data_agg_class)
        {
            //appid condition
            if(DATA_AGG_CLASS_LINK_IP_INNER == g_data_agg_class)
            {
                data_link_ip = mesg.ip_src;
            }
            else if (DATA_AGG_CLASS_LINK_IP_OUTER == g_data_agg_class)
            {
                data_link_ip = mesg.ip_dst;
            }

            if(data_link_ip != g_baselink_ip)
            {
                continue;
            }
        }
        
        tmp = (sorted_node_t *)malloc(sizeof(sorted_node_t));
        if(NULL != tmp)
        {
            bzero(tmp, sizeof(sorted_node_t));
            
            tmp->data.link_app = mesg.proto_mark;
            tmp->data.link_proto = mesg.proto;

            sprintf( tmp->data.link_con_info, "%u.%u.%u.%u:%u--%u.%u.%u.%u:%u", 
                      IPQUADS(mesg.ip_src), ntohs(mesg.port_src), IPQUADS(mesg.ip_dst),ntohs(mesg.port_dst));
            
            tmp->data.link_duration_time = mesg.duration_time;
            tmp->data.link_up_size = mesg.up_bytes * 8;
            tmp->data.link_down_size = mesg.down_bytes * 8;

             //fill sorted_list
            do_process_insert_sorted_list(tmp, g_data_agg_class, g_sorted_list_index, g_data_sort_condition); 
        }
     }

    marbit_send_log(INFO,"insert_num = %d\n", insert_num);

    fclose(fp); 

    return;
}

void print_baselink_info(const char* dbname)
{    
    struct msg_st mesg;

    //read configure file
    FILE *fp;
    char line[1024] = {0};
    
    char   *stop_at  =NULL ;  
    uint32_t data_link_ip = 0;
    unsigned char link_con_info[LINK_CON_INFO_SIZE_MAX]; 

    uint32_t display_index = 0; 
    g_display_res_line = 0;
    
    DIR              *pDir ;  
    struct dirent    *ent  ;  
    char              filename[MAX_FILE_PATH_LEN];  

    pDir=opendir(dbname); 
    if(!pDir)
    {
        marbit_send_log(ERROR,"config dir is not exist, dir:%s\n", dbname);
        return;
    }
    memset(filename, 0, sizeof(filename)); 
    
    while((ent=readdir(pDir))!=NULL)      
    {  
        if(ent->d_type & DT_DIR)  
        {  
            //if(strcmp(ent->d_name,".")==0 || strcmp(ent->d_name,"..")==0)  
               // continue;  
            continue;
        }  
        else
        {
            memset(filename, 0, sizeof(filename)); 
            sprintf(filename,"%s/%s",dbname,ent->d_name);  

            if ((fp = fopen(filename, "rb")) == NULL) 
            {
                marbit_send_log(ERROR,"-------open file [%s] fail.\n", filename);
                continue;
            }
            while(fgets(line, 1024, fp))  
            {
                mesg.ip_src = strtoul(&line[0], &stop_at, 10);
                mesg.port_src = strtoul(&line[16], &stop_at, 10);
                mesg.ip_dst = strtoul(&line[32], &stop_at, 10);
                mesg.port_dst = strtoul(&line[48], &stop_at, 10);
                mesg.proto = strtoul(&line[64], &stop_at, 10);
                mesg.proto_mark = strtoul(&line[80], &stop_at, 10);
                mesg.up_bytes = strtoul(&line[96], &stop_at, 10);
                mesg.down_bytes = strtoul(&line[112], &stop_at, 10);
                mesg.duration_time = strtoul(&line[128], &stop_at, 10);

                 //appid condition
                if(g_data_agg_class_condition_appid != G_DATA_AGG_APPID_DEFAULT)
                {
                    if(g_data_agg_class_condition_appid != mesg.proto_mark)
                    {
                        continue;
                    }
                }
                
                if (DATA_AGG_CLASS_LINK_IP_ALL != g_data_agg_class)
                {
                    //appid condition
                    if(DATA_AGG_CLASS_LINK_IP_INNER == g_data_agg_class)
                    {
                        
                        if(mesg.ip_src != g_baselink_ip)
                        {
                            continue;
                        }
                        
                    }
                    else if (DATA_AGG_CLASS_LINK_IP_OUTER == g_data_agg_class)
                    {
                        
                        if(mesg.ip_dst != g_baselink_ip)
                        {
                            continue;
                        }
                        
                    }
                    else
                    {
                        marbit_send_log(ERROR,"-------baselink g_data_agg_class is not valid.\n");
                        continue;
                    }
                }


                g_display_res_line++;
            
                if(G_DISPLAY_LINE_DEFAULT != g_display_req_line )
                {
                    display_index++;

                    if(display_index < g_display_req_begin_line)
                    {
                        continue;
                    }

                    if(display_index > g_display_req_end_line)
                    {
                        continue;
                    }
                }
                
                memset(link_con_info, 0, LINK_CON_INFO_SIZE_MAX);
                sprintf( link_con_info, "%u.%u.%u.%u:%u--%u.%u.%u.%u:%u", 
                          IPQUADS(mesg.ip_src), ntohs(mesg.port_src), IPQUADS(mesg.ip_dst),ntohs(mesg.port_dst));

                            
                if(IPPROTO_TCP == mesg.proto)
                {
                    printf("%-15lu | %-15s | %-80s | %-15lu | %-15lu |%-15lu\n", 
                            mesg.proto_mark,  IPPROTO_TCP_NAME, 
                            link_con_info, mesg.duration_time, mesg.up_bytes * 8, mesg.down_bytes * 8);


                }
                else if(IPPROTO_UDP == mesg.proto)
                {
                    printf("%-15lu | %-15s | %-80s | %-15lu | %-15lu |%-15lu\n", 
                            mesg.proto_mark,  IPPROTO_UDP_NAME, 
                            link_con_info, mesg.duration_time, mesg.up_bytes * 8, mesg.down_bytes * 8);
                }
            }
            fclose(fp); 
        }
    }  
    
    closedir(pDir);

    printf("%-15lu\n",g_display_res_line); 
    
    return;
}

void do_process_data_aggregation()
{
    if(SORTED_BASE_APP == g_sorted_list_index || SORTED_BASE_IP == g_sorted_list_index)
    {
        //loop data stream file
        do_data_aggregation_from_db(DATA_STREAM_FILE_PATH);
        g_display_res_line = g_aggregation_line;

#if 0 //ues for insert sort
        //write data aggregation file
        save_data_aggregation_result_to_db();

        //data sort
        do_process_sort();
      //  do_process_sort_time = time(NULL);

        //print result
        //print_sorted_list(g_sorted_list_index);
       // print_sorted_list_time = time(NULL);
#endif
    }

    return;
}
void do_process_sort()
{
#if 0
    resort_list( SORTED_BASE_AGG,  
                      g_sorted_list_index,
                      g_data_sort_condition,
                      g_data_agg_class);
#else

    // base app process
    if(SORTED_BASE_APP == g_sorted_list_index)
    {
        process_baseapp_data_from_db();
    }
    
    //base link process, not need sort
    else if(SORTED_BASE_LINK == g_sorted_list_index)
    {      
        process_baselink_data_from_db();
    }

    //base ip process
    else if(SORTED_BASE_IP == g_sorted_list_index)
    {
        process_baseip_data_from_db();
    }
    
    // error
    else
    {
        input_error();
    }
#endif

    return;
}

void save_data_aggregation_result_to_db()
{
    struct list_head *head = &sorted_list_arry[SORTED_BASE_AGG].list;
    if(list_empty(head))
    {
        marbit_send_log(DEBUG, "head is empty\n");
        return;
    }

    FILE *fp = NULL; 
    if(SORTED_BASE_IP == g_sorted_list_index)
    {
        fp = fopen(DATA_BASEIP_FILE_NAME,"w+");
        if(NULL == fp)
        {
            marbit_send_log(INFO,"failed to open file %s\n", DATA_BASEIP_FILE_NAME);
            return;
        }
     }
    else if(SORTED_BASE_APP == g_sorted_list_index)
    {
        fp = fopen(DATA_BASEAPP_FILE_NAME,"w+");
        if(NULL == fp)
        {
            marbit_send_log(INFO,"failed to open file %s\n", DATA_BASEAPP_FILE_NAME);
            return;
        }
     }

    sorted_node_t *tmp = NULL;
    struct list_head *pos = NULL;
    list_for_each_prev(pos, head)
    {
        tmp = list_entry(pos, sorted_node_t, list);
        if(NULL == tmp)
        {
            continue;
        }   

        if(SORTED_BASE_IP == g_sorted_list_index)
        {
            fprintf(fp,"%-15lu,%-15lu,%-15lu,%-15lu,%-15lu\n", 
                  tmp->data.ip,  tmp->data.ip_con_nums, tmp->data.ip_up_size, tmp->data.ip_down_size, tmp->data.ip_total_size);
        }
        else if(SORTED_BASE_APP == g_sorted_list_index)
        {
            fprintf(fp,"%-15lu,%-15lu,%-15lu,%-15lu,%-15lu\n", 
                 tmp->data.appid,  tmp->data.con_nums, tmp->data.up_bps, tmp->data.down_bps, tmp->data.total_bits);
        }
    }
    
    fclose(fp);

    return;
}


//0 is ok, -1 is error
int parse_input_parameters(int argc, char **argv)
{
    char *para = NULL;
    if(argc < 2)
    {
        return -1;
    }

    int i = 0;
    for(i=0;i<argc; i++)
    {
        marbit_send_log(DEBUG, "argv[%d] = %s\n", i, argv[i]);
    }


    para = argv[1];
     _strim(para);

    /*
      * 基于ＩＰ统计
      *
      * 前端调用接口:
      * /opt/utc/bin/surf_stat  baseip -s [link|up|down|total] -u [inner|outer] -l [begin line] [end line] -a [appid] -f [N] 
      *
      * 参数：
      * -u [inner/outer], 其中inner表示内网用户， outer表示外网用户。
      * 
      * -l [begin line] [end line], 输出前N行排名， 全部输出为0。
      *     g_display_req_line = 0 means all lines
      * 
      * -a [appid], 现在要查看的APPID， 默认为所有协议。
      *      g_data_agg_class_condition_appid = -1 means all app, 协议ID取自协议列表。
      * 
      * -f [N], N表示后台数据采集刷新时间，默认不刷新。
      *      g_flush_interval = 200s means default , 后台则在200秒后自动刷新。
      *
      *  输出：行格式（按照-s指定的列做降序排列）
      * IP|连接数|上行流量|下行流量|流量统计
     */
    if (!strcmp_nocase(para, "baseip")  && 13 == argc) 
    {  
        g_sorted_list_index = SORTED_BASE_IP;
        
        //[link/up/down/total]
        para = argv[3];
         _strim(para);
         if (!strcmp_nocase(para, "link")) 
         {
            g_data_sort_condition = DATA_SORT_CONDITION_IP_CON_NUM;
         }
         else if (!strcmp_nocase(para, "up")) 
         {
            g_data_sort_condition = DATA_SORT_CONDITION_IP_UP_SIZE;
         }
         else if (!strcmp_nocase(para, "down")) 
         {
            g_data_sort_condition = DATA_SORT_CONDITION_IP_DOWN_SIZE;
         }
         else if (!strcmp_nocase(para, "total")) 
         {
            g_data_sort_condition = DATA_SORT_CONDITION_IP_TOTAL_SIZE;
         }

        //[inner/outer]
        para = argv[5];
         _strim(para);
         if (!strcmp_nocase(para, "inner")) 
         {
            g_data_agg_class = DATA_AGG_CLASS_IP_INNER;
         }
         else if (!strcmp_nocase(para, "outer")) 
         {
            g_data_agg_class = DATA_AGG_CLASS_IP_OUTER;
         }
         
        //[begin line] [end line], default value is 0 which means all lines
        para = argv[7];
        _strim(para);
        g_display_req_begin_line = (uint32_t) atoi(para);
        
        para = argv[8];
        _strim(para);
        g_display_req_end_line = (uint32_t) atoi(para);
        
        if(0 == g_display_req_begin_line)
        {
            g_display_req_line = G_DISPLAY_LINE_DEFAULT; //default 0 is all lines
        }
        else
        {
            g_display_req_line = g_display_req_end_line - g_display_req_begin_line + 1;
        }     

        //[appid], default value is -1 which means all app
        para = argv[10];
        _strim(para);
        g_data_agg_class_condition_appid = (int32_t) atoi(para);
         
        //[N], default value is 200 which means udpate time
        para = argv[12];
        _strim(para);
        g_flush_interval = (uint32_t) atoi(para);  
        
    }
    /*
      * 连接信息统计
      *
      * 前端调用接口:
      *  /opt/utc/bin/surf_stat  baselink -u [inner/outer] -i [ IP ] -l [begin line] [end line] -a [appid] 
      * 
      * 参数：
      * -u [inner|outer|all], 与-i配合使用，其中inner表示内网用户， outer表示外网用户。
      *   g_data_agg_class  is DATA_AGG_CLASS_IP_INNER or DATA_AGG_CLASS_IP_OUTER
      *
      * -i [ IP ], 该参数与-u配合使用，指按照内网或者外网用户IP过滤统计信息。 
      *   g_baselink_ip is ip
      * 
      * -l [begin line] [end line], 输出前N行排名， 全部输出为0。
      *     g_display_req_line = 0 means all lines
      *
      * -a [appid]， 现在要查看的APPID， 默认为所有协议。
      *   g_data_agg_class_condition_appid = -1 means all app, 协议ID取自协议列表。
      * 
      * 
      *  输出：行格式（按照下行流量的降序排列）
      *  应用|协议|连接信息|时长|上行流量|下行流量
      */
    else if (!strcmp_nocase(para, "baselink") && 11 == argc) 
    {  
        g_sorted_list_index = SORTED_BASE_LINK;
        g_data_sort_condition = DATA_SORT_CONDITION_NOT_USED;

        //[inner/outer]
        para = argv[3];
         _strim(para);
         if (!strcmp_nocase(para, "inner")) 
         {
            g_data_agg_class = DATA_AGG_CLASS_LINK_IP_INNER;
         }
         else if (!strcmp_nocase(para, "outer")) 
         {
            g_data_agg_class = DATA_AGG_CLASS_LINK_IP_OUTER;
         }
         else if (!strcmp_nocase(para, "all")) 
         {
            g_data_agg_class = DATA_AGG_CLASS_LINK_IP_ALL;
         }
    
        //[ IP ]
        if(DATA_AGG_CLASS_LINK_IP_ALL == g_data_agg_class)
        {
            g_baselink_ip == G_BASELINK_IP_DEFAULT;
        }
        else
        {
            para = argv[5];
            _strim(para);
            g_baselink_ip = (uint32_t) atoi(para);
         }

        //[begin line] [end line], default value is 0 which means all lines
        para = argv[7];
        _strim(para);
        g_display_req_begin_line = (uint32_t) atoi(para);
        
        para = argv[8];
        _strim(para);
        g_display_req_end_line = (uint32_t) atoi(para);

        if(0 == g_display_req_begin_line)
        {
            g_display_req_line = G_DISPLAY_LINE_DEFAULT; //default 0 is all lines
        }
        else
        {
            g_display_req_line = g_display_req_end_line - g_display_req_begin_line + 1;
        }   
        
        //[appid], default value is -1 which means all app
        para = argv[10];
        _strim(para);
        g_data_agg_class_condition_appid = (int32_t) atoi(para);  
        
    }
    /*
      * 应用流量排名
      *
      * 前端调用接口：
      * /opt/utc/bin/surf_stat  baseapp -s [link/up/down/total] -l [begin line] [end line] -f [N] 
      * 
      * 参数：
      * -s [link/up/down/total], 该参数用来设置输出排序的列， 
      *     link指按链路数排序，up指按上行速率排序， 
      *     down指按下行速率排序， total指按总流量排序。
      *  g_data_sort_condition
      * 
      * -l [begin line] [end line], 输出前N行排名， 全部输出为0。默认为0.
      *   g_display_req_line = 0 means all lines
      * 
      * -f [N], N表示后台数据采集刷新时间，默认不刷新。
      *   g_flush_interval = 200s means default , 后台则在200秒后自动刷新。
      * 
      *  输出：行格式（按照-s指定的列做降序排列）
      *  应用|连接数|上行速率|下行速率|累计流量
      * 
      */
    else if (!strcmp_nocase(para, "baseapp") && 9 == argc) 
    {        
        g_data_agg_class = DATA_AGG_CLASS_APP;
        g_sorted_list_index = SORTED_BASE_APP;

        //[link/up/down/total]
        para = argv[3];
         _strim(para);
         if (!strcmp_nocase(para, "link")) 
         {
            g_data_sort_condition = DATA_SORT_CONDITION_CON_NUM;
         }
         else if (!strcmp_nocase(para, "up")) 
         {
            g_data_sort_condition = DATA_SORT_CONDITION_UP_BPS;
         }
         else if (!strcmp_nocase(para, "down")) 
         {
            g_data_sort_condition = DATA_SORT_CONDITION_DOWN_BPS;
         }
         else if (!strcmp_nocase(para, "total")) 
         {
            g_data_sort_condition = DATA_SORT_CONDITION_TOTAL_BITS;
         }

       //[begin line] [end line], default value is 0 which means all lines
        para = argv[5];
         _strim(para);
        g_display_req_begin_line = (uint32_t) atoi(para);

        para = argv[6];
         _strim(para);
        g_display_req_end_line = (uint32_t) atoi(para);

        if(0 == g_display_req_begin_line)
        {
            g_display_req_line = G_DISPLAY_LINE_DEFAULT; //default 0 is all lines
        }
        else
        {
            g_display_req_line = g_display_req_end_line - g_display_req_begin_line + 1;
        }   
        
       
        //[N] , default value is 200 which means udpate time
        para = argv[8];
        g_flush_interval = (uint32_t) atoi(para);  
       
    }
    else
    {
        marbit_send_log(ERROR, "surf_stat parameters is not valid\n");
        return -1;
    }
    
    return 0;
}


int init_sys_info()
{
    if(0 != init_cfg_info())
    {
        marbit_send_log(ERROR,"Failed to init_cfg_info!\n");
        return -1;
    }
    
    if (0 != init_global_struct())
    {
        marbit_send_log(ERROR,"Failed to init_global_struct!\n");
        return -1;
    }


    g_sorted_list_index = SORTED_LIST_NOT_USED;
    g_data_sort_condition = DATA_SORT_CONDITION_NOT_USED;
    g_data_agg_class = DATA_AGG_CLASS_NOT_USED;

    g_baselink_ip = G_BASELINK_IP_DEFAULT;
    g_display_req_line = G_DISPLAY_LINE_DEFAULT;
    g_flush_interval = G_FLUSH_INTERVAL_DEFAULT;
    g_data_agg_class_condition_appid = G_DATA_AGG_APPID_DEFAULT;

    return 0;
}
int init_global_struct()
{
    if(0 != data_aggregation_cache_init())
    {
        return -1;
    }

    if(0 != init_stream_info())
    {
        return -1;
    }

    if(0 != init_sort_info())
    {
        return -1;
    }
    
    return 0;
}

int init_cfg_info()
{
    int i,find;
    char    filename[355];
    char    name[100],value[100];
    char    buff[255];  
    FILE   *fp;
    memset(filename,0, sizeof(filename));
    strncpy(filename, CONFIG_FILE_NAME, 355);
    //strncat(filename, CONFIG_FILE_NAME, 355 - strlen(CONFIG_FILE_NAME));

    logdebug.g_trace_enable_flag = 0;
    
    if((fp = fopen(filename,"r")) == NULL)
    {
        memset(filename,0,sizeof(filename));
        sprintf(filename,"%s", CONFIG_FILE_NAME);
        if((fp = fopen(filename,"r")) == NULL)
        {
            marbit_send_log(ERROR,"Failed to open configure file\n");
            return -1;
        }
    }
    
    char **toks;
    int num_toks;
    memset(buff,0,sizeof(buff));
    while( fgets( buff, sizeof( buff ), fp ) != NULL )
    {
        if (*buff == '#') 
        {
            memset(buff,0,sizeof(buff));
            continue;
        }
        toks = mSplit(buff, "=", 3, &num_toks, 0);
        if(num_toks < 1)
            goto free_toks;
        
        for (i = 0; i < num_toks; i++) 
        {
            _strim(toks[i]);
        }
        
        if (!strcmp_nocase(toks[0], "ARBITER_PORT") ) 
        {
            ARBITER_PORT = atoi(toks[1]);
        }
        else if (!strcmp_nocase(toks[0], "LOG_FILE") ) 
        {
            strcpy(logdebug.g_trace_file, toks[1]);
        }
        else if (!strcmp_nocase(toks[0], "LOG_LEVEL") ) 
        {  
            logdebug.g_trace_level = atoi(toks[1]);
            if(logdebug.g_trace_level <= 0 || logdebug.g_trace_level > 10)
            {
                    logdebug.g_trace_level = 3;
            }
        }
        else if (!strcmp_nocase(toks[0], "LOG_ENABLE_FLAG") ) 
        {
            logdebug.g_trace_enable_flag = atoi(toks[1]);
        }
        else if (!strcmp_nocase(toks[0], "STREAM_MESG_NUM_LIMIT_FLAG") ) 
        {
            STREAM_MESG_NUM_LIMIT_FLAG = atoi(toks[1]);
        }
        else if (!strcmp_nocase(toks[0], "STREAM_MESG_NUM_LIMIT_MAX") ) 
        {
            STREAM_MESG_NUM_LIMIT_MAX = atoi(toks[1]);
        }
        else if (!strcmp_nocase(toks[0], "PRINT_STAT_RESULT") ) 
        {
            PRINT_STAT_RESULT = atoi(toks[1]);
        }
        
        free_toks:
            memset(buff,0,sizeof(buff));
            mSplitFree(&toks, num_toks);
    }

    marbit_send_log(INFO,"%s: ARBITER_PORT[%d] LOG_FILE[%s],LOG_LEVEL[%d],"\
                                "STREAM_MESG_NUM_LIMIT_FLAG [%d], STREAM_MESG_NUM_LIMIT_MAX[%d]\n",
                                __FUNCTION__, ARBITER_PORT,
                                logdebug.g_trace_file, logdebug.g_trace_level, 
                                STREAM_MESG_NUM_LIMIT_FLAG, STREAM_MESG_NUM_LIMIT_MAX);
    
    fclose(fp);
    
    return 0;
}


void destroy_info()
{
    destroy_aggregation_info();

    destroy_stream_info();

    destroy_sort_info();

    return;
}
