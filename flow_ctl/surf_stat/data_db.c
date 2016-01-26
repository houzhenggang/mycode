#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h> 
#include <dirent.h>
#include <sys/stat.h>
 
#include "data_db.h" 

extern uint32_t g_display_req_line; //defalut is 0
extern uint32_t g_flush_interval; //seconds, default is200
extern int32_t  g_data_agg_class_condition_appid;//defalut is -1
extern uint32_t g_baselink_ip;//defalut is 0

/*1 is read config file, 0 is get from arbiter*/
int isNeedReadFromDB(const char *dbname, uint32_t flush_interval)
{
    int ret;
    struct stat buf;
    time_t last_time = 0;
    
    DIR              *pDir ;  
    struct dirent    *ent  ;  
    char              filename[MAX_FILE_PATH_LEN];  
 
    pDir=opendir(dbname); 
    if(!pDir)
    {
        marbit_send_log(ERROR,"config dir is not exist, dir:%s\n", dbname);
        return 0;
    }
    memset(filename, 0, sizeof(filename)); 
    
    while((ent=readdir(pDir))!=NULL)      
    {  
        memset(filename, 0, sizeof(filename)); 
        
        if(ent->d_type & DT_DIR)  
        {  
            //if(strcmp(ent->d_name,".")==0 || strcmp(ent->d_name,"..")==0)  
               // continue;  
            continue;
        }  
        else
        {
            sprintf(filename,"%s/%s",dbname,ent->d_name);  
                        
            ret = stat(filename, &buf);
            if (ret != 0)
            {        
                continue;
            }
            if(buf.st_mtime > last_time)
            {
                last_time = buf.st_mtime;
            }
        }
    }  
    
    closedir(pDir);

    time_t curr_time = time(NULL);

    if (curr_time - last_time >= flush_interval) 
    {
         marbit_send_log(DEBUG, "curr_time - last_modify_time >= g_flush_interval, should get from arbiter again!\n");
        return 0;
    }
    else
    {
        marbit_send_log(DEBUG,"curr_time - last_modify_time < g_flush_interval, should read from current configure file!\n");
        return 1;
    }
  
    return 1;
}

void do_data_aggregation_from_file(const char* filename)
{    
    struct msg_st mesg;

    //read configure file
    char **toks;
    int num_toks;
    FILE *fp;
    char line[1024] = {0};
    int i = 0;
    char buf[1024] = {0};
#if 1
    if ((fp = fopen(filename, "rb")) == NULL) 
    {
        marbit_send_log(ERROR,"-------open file [%s] fail.\n", filename);
        return;
    }
#else
    if ((fp = fopen(DATA_STREAM_FILE_NAME, "rb")) == NULL) 
    {
        marbit_send_log(ERROR,"-------open file [%s] fail.\n", DATA_STREAM_FILE_NAME);
        return;
    }
#endif
    char   *stop_at  =NULL ;  
    
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

        if( mesg.ip_src <= 0 || mesg.port_src <= 0 || 
            mesg.ip_dst <= 0 || mesg.port_dst <= 0 ||
            mesg.up_bytes <= 0 || 
            mesg.proto <= 0 || mesg.duration_time <= 0)
        {
            continue;
        }
        
        process_data_aggregation_list(&mesg,
                                                    g_data_agg_class, 
                                                    SORTED_BASE_AGG, 
                                                    g_data_sort_condition);
   
     }

    fclose(fp); 

}

void save_to_db(const char *filename, struct list_head *head)
{
    //if stream is empty, then just return
    if(list_empty(head))
    {
        return;
    }

    //char buf[512];
    FILE *fp = NULL;

    //sprintf(buf,"%s\r\n", str) ;
 
    fp = fopen(filename,"w+");
    if(NULL == fp)
    {
        //fseek(fp,0,2);
        fclose(fp);
        return;
    }

    stream_node_t *tmp = NULL;
    struct list_head *pos = NULL;

    char buf[500];
    list_for_each(pos, head)
    {
        tmp = list_entry(pos, stream_node_t, list);
        if(NULL == tmp)
        {
            continue;
        }
        
        memset(buf, 0, 500);
        sprintf(buf, "%15lu,%15lu,%15lu,%15lu,%15lu,%15lu,%15lu,%15lu,%15lu\n",
              tmp ->data.ip_src, tmp ->data.port_src, 
              tmp ->data.ip_dst , tmp ->data.port_dst,
              tmp ->data.proto,  tmp ->data.proto_mark,
              tmp ->data.up_bytes, tmp ->data.down_bytes, 
              tmp ->data.duration_time);
        
        fwrite(buf,1,strlen(buf),fp);
    }
    
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);

    return;
}

void save_mesg_to_db(const char *filename, struct msg_st *mesg)
{
    //if stream is empty, then just return
    if(NULL == mesg)
    {
        return;
    }

    //char buf[512];
    FILE *fp = NULL;

    //sprintf(buf,"%s\r\n", str) ;
 
    fp = fopen(filename,"w+");
    if(NULL == fp)
    {
        //fseek(fp,0,2);
        fclose(fp);
        return;
    }


    char buf[500];

    memset(buf, 0, 500);
    sprintf(buf, "%15lu,%15lu,%15lu,%15lu,%15lu,%15lu,%15lu,%15lu,%15lu,%15lu\n",
                mesg->ip_src, mesg->port_src, 
                mesg->ip_dst , mesg->port_dst,
                mesg->proto,  mesg->proto_mark,
                mesg->up_bytes, mesg->down_bytes, 
                mesg->duration_time);

    fwrite(buf, 1, strlen(buf),fp);
   
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);

    return;
}




