/*
 * arbiter handler
 *
 * Author: junwei.dong
 */
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
#include <fcntl.h>  
#include <unistd.h> 

#include <stdio.h>  
#include <string.h>  
#include <sys/socket.h>  
#include <sys/ioctl.h>  
#include <linux/if.h>  
#include <linux/if_tun.h>  
#include <sys/types.h>  
#include <errno.h>  
#include <net/route.h>  
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "log.h"
#include "common.h"
#include "data_aggregation.h"
#include "data_sort.h"
#include "data_stream.h"
#include "data_test.h"
#include "arbiter_handler.h"

struct sockaddr_in arbiter_control_addr;
char arbiter_control_sock_flag;
int arbiter_control_sock; 
int ARBITER_PORT;
pthread_t arbiter_control_pthread;

extern uint32_t g_display_req_line; //defalut is 0
int STREAM_MESG_NUM_LIMIT_MAX = 0;
int STREAM_MESG_NUM_LIMIT_FLAG = 0;//default is 0 not limit, 1 is limit

/*1 is exist, 0 is not exist*/
int isArbiterExist()
{
    int ret = 0;
    
    FILE* fp;   
    int arbier_process_id;   
    char buf[50];  
    memset(buf, 0, 50);
    
    char command[50];   
    memset(command, 0, 50);
    sprintf(command, "pidof arbiter" );   
  
    if((fp = popen(command,"r")) != NULL)  
    {
        if((fgets(buf, 50, fp)) != NULL)   
        {  
            arbier_process_id = atoi(buf);   
            if(arbier_process_id  > 0)   
            {
                marbit_send_log(INFO,"arbier_process_id %d\n",arbier_process_id);   
                ret = 1;
             }
        }   
     }
    
    pclose(fp);  

    return ret;
}


void msg_stat(int msgid,struct msqid_ds  msg_info)
{
        int ret;
        ret = msgctl(msgid,IPC_STAT,&msg_info);
        if(ret == -1)
        {
                marbit_send_log(ERROR,"msgctl error :");
                //exit(EXIT_FAILURE);
        }
       // printf("\n");
        //printf("current number of bytes on queue is %d\n",msg_info.msg_cbytes);
       // printf(" max  of bytes on queue id %d\n",msg_info.msg_qbytes);
        //printf(" number of messages in queue is %d\n",msg_info.msg_qnum);
        //printf("last change time  is %s\n",ctime(&msg_info.msg_ctime));
       // printf("message uid is  %d\n",msg_info.msg_perm.uid);
       // printf("message gid is  %d\n",msg_info.msg_perm.gid);
       return;
 
}

uint32_t g_start_recv_from_arbiter = 0; //defalut is 0
uint32_t g_force_recv_from_arbiter_shutdown = 0; //defalut is 0

void timer_handler(int iSig)
{
    g_start_recv_from_arbiter++;
    if(g_start_recv_from_arbiter >= 3)
    {
        g_force_recv_from_arbiter_shutdown = 1;
    }
}


void set_timer()
{
    struct itimerval itimer;

    signal(SIGALRM, timer_handler);

    memset(&itimer, 0, sizeof(itimer));
    itimer.it_value.tv_sec = 1;
    itimer.it_value.tv_usec = 0;
    itimer.it_interval.tv_sec = 1;
    itimer.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL, &itimer, NULL);
}

int do_process_from_arbiter()
{
    time_t begin_time = time(NULL);
    time_t finish_time = 0;
    
    uint32_t  count_stat = 0;

    int  shmid = 0;
    skbstat_t    *skbstat = NULL;

     if ( (shmid = shmget(STAT_SHM_KEY, SHM_SIZE, SHM_MODE)) < 0)
    {
        marbit_send_log(INFO,"shmget");
        return -1;
    }

    if ( ( skbstat = (skbstat_t *)shmat(shmid, NULL, 0)) == (void *) -1)
    {
        marbit_send_log(INFO, "shmat");
        return -1;
    }
    
    set_timer();
    
    //if is 0, then start to process....
    if(0 == skbstat->web_mesg_flag)
    {
        skbstat->web_mesg_flag = 1;
        marbit_send_log(INFO,"set shm web_mesg_flag = 0xFF\n");

        while(0 != skbstat->web_mesg_flag)
        {
            if(1 == g_force_recv_from_arbiter_shutdown)
            {
                marbit_send_log(ERROR,"do_process_from_arbiter time out\n");
                skbstat->web_mesg_flag = 0;

                break;
            }

            usleep(10*1000);
            continue;
        }
    }
    //if not 0, maybe other process is running, just waiting ......
    else
    {
        while(0 != skbstat->web_mesg_flag)
        {
            if(1 == g_force_recv_from_arbiter_shutdown)
            {
                marbit_send_log(ERROR,"do_process_from_arbiter time out\n");
                skbstat->web_mesg_flag = 0;
    
                break;
            }
     
            usleep(10*1000);
            continue;
        }
    }

    
    time_t end_time = time(NULL);
    marbit_send_log(INFO,"begin_time = %d\n", begin_time);
    marbit_send_log(INFO,"end_time = %d\n", end_time);

    return 0;
}

#if 0
void do_recv_messgae_from_arbiter()
{
    char buff[65535];
    struct sockaddr_in recv_control_addr;
    socklen_t recv_control_addr_len = sizeof(recv_control_addr);
    int len = 0;
    //struct msg_st *mesg = NULL;
    char buf[500];
    int ret = 0;
    struct msg_st mesg;
    int msgid = 0;
    uint32_t  count_stat = 0;


    //char buf[512];
    char file_buf[512];
    FILE *fp = NULL; 
    fp = fopen(DATA_STREAM_FILE_NAME,"w+");
    if(NULL == fp)
    {
        marbit_send_log(INFO,"failed to open file %s\n", DATA_STREAM_FILE_NAME);
        return;
    }


    int  shmid = 0;
    skbstat_t    *skbstat = NULL;

#if 1
     struct msqid_ds msg_ginfo,msg_sinfo;
     msg_stat(msgid,msg_ginfo);
     msg_sinfo.msg_perm.uid=8;//just a try
     msg_sinfo.msg_perm.gid=8;//
     msg_sinfo.msg_qbytes=16384000;
    //此处验证超级用户可以更改消息队列的缺省msg_qbytes
#endif

    key_t key;

    key = ftok(KEYPATH, KEYPROJ);
    if (key<0)     {
       marbit_send_log(INFO,"ftok()");
       return;
   }

    msgid = msgget(key, IPC_CREAT|0600);
    if (msgid<0) 
    {
        marbit_send_log(INFO,"msgget() error ");
        return ;
    }

     if ( (shmid = shmget(STAT_SHM_KEY, SHM_SIZE, SHM_MODE)) < 0)
    {
        marbit_send_log(INFO,"shmget");
        return;
    }

    if ( ( skbstat = (skbstat_t *)shmat(shmid, NULL, 0)) == (void *) -1)
    {
        marbit_send_log(INFO, "shmat");
        return;
    }
    
    skbstat->web_msgid = msgid;
    skbstat->web_mesg_flag = 1;
    
    marbit_send_log(INFO,"msgid = %d\n", msgid);
    marbit_send_log(INFO,"set shm web_mesg_flag = 1\n");

    time_t begin_time = time(NULL);
    time_t finish_time = 0;
    marbit_send_log(INFO,"begin_time = %d\n", begin_time);
    
    while (1) 
    {
        if(1 == STREAM_MESG_NUM_LIMIT_FLAG)
        {
            if(count_stat >= STREAM_MESG_NUM_LIMIT_MAX)
            {
                marbit_send_log(INFO, "count_stat[%d] >= STREAM_MESG_NUM_LIMIT_MAX[%d], just exit\n",count_stat, STREAM_MESG_NUM_LIMIT_MAX);
                break;  
            }
        }

        if(1 == g_force_recv_from_arbiter_shutdown)
        {
            marbit_send_log(INFO,"g_force_recv_from_arbiter_shutdown = 1, break\n");
            break;
        }
        
        ret = msgrcv(msgid, &mesg, sizeof(mesg)-sizeof(long), 0, IPC_NOWAIT);
        if (ret<0) 
        {   
            if(errno == ENOMSG)
            {
                continue;
            }
            
            marbit_send_log(ERROR,"msgrcv() error errno = %d\n", errno);
            
            break;
        }
        
         if(1 == mesg.finish_flag)
        {
            count_stat++;
            
            marbit_send_log(INFO,"recv message from arbiter finished! %u\n", count_stat);

            finish_time = time(NULL);
            marbit_send_log(INFO,"finish_time = %d\n", finish_time);

            
            break;  
        }
       
        else //if(0 == mesg->finish_flag)
        {               
        #if 1
            fprintf(fp, "%-15u,%-15u,%-15u,%-15u,%-15u,%-15u,%-15u,%-15u,%-15u,%-15u\n",
                    mesg.ip_src, mesg.port_src, 
                    mesg.ip_dst , mesg.port_dst,
                    mesg.proto,  mesg.proto_mark,
                    mesg.up_bytes, mesg.down_bytes, 
                    mesg.duration_time, mesg.finish_flag);
        #endif
            count_stat++;
         }   

    }
    
    time_t end_time = time(NULL);
    marbit_send_log(INFO,"end_time = %d\n", end_time);
    
    msgctl(msgid, IPC_RMID, NULL);

    fclose(fp);

    return;
}
#endif



#if 0
void process_data_from_arbiter()
{
    if(0 != pthread_create(&arbiter_control_pthread, NULL, arbiter_control_handler,NULL))
    {
        //printf("create arbiter_control_pthread failed!\r\n"); 
        return; 
    }  
    pthread_join(arbiter_control_pthread,NULL);
    
    return;
}

void setnonblocking(int sock)
{
     int opts;
     opts = fcntl(sock, F_GETFL);
     if(opts < 0)
     {
         marbit_send_log(INFO,"fcntl(sock,GETFL)");
         exit(1);
     }
     opts = opts | O_NONBLOCK;
     if(fcntl(sock, F_SETFL, opts) < 0)
     {
         marbit_send_log(INFO,"fcntl(sock,SETFL,opts)");
         exit(1);
     }
}


#define MAX_EVENTS    5
#define BUFFER_SIZE   2048  
void* arbiter_control_handler(void *arg)
{
    char buff[65535];
    struct sockaddr_in recv_control_addr;
    socklen_t recv_control_addr_len = sizeof(recv_control_addr);
    int len = 0;
    struct msg_st *mesg = NULL;
    char buf[500];

    uint32_t  count = 0;
    uint32_t  count_stat = 0;

    int  shmid = 0;
    skbstat_t    *skbstat = NULL;
    
    if ( (shmid = shmget(STAT_SHM_KEY, SHM_SIZE, SHM_MODE)) < 0)
    {
        perror("shmget");
        return NULL;
    }

    if ( ( skbstat = (skbstat_t *)shmat(shmid, NULL, 0)) == (void *) -1)
    {
        perror("shmat");
        return NULL;
    }

    skbstat->web_mesg_flag = 1;
    marbit_send_log(INFO,"set shm web_mesg_flag = 1\n");


    struct timeval tv;
    int ret;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    if(setsockopt(arbiter_control_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))<0)
    {
    	marbit_send_log(ERROR,"socket option SO_RCVTIMEO not support\n");
    	return;
    }
       
    while(1)  
    {  
        if(1 == STREAM_MESG_NUM_LIMIT_FLAG)
        {
            if(count >= STREAM_MESG_NUM_LIMIT_MAX)
            {
                marbit_send_log(DEBUG, "count_stat[%d] >= STREAM_MESG_NUM_LIMIT_MAX[%d], just exit\n",count_stat, STREAM_MESG_NUM_LIMIT_MAX);
                return NULL;  
            }
        }
        
        memset(buff, 0, sizeof(buff));
        
        /* 1. recv message from arbiter */
        len = recvfrom(arbiter_control_sock, buff, sizeof(buff), 0, (struct sockaddr *)&recv_control_addr,&recv_control_addr_len);  
        marbit_send_log(ERROR, "recvfrom len = %d", len );
        if(len < 0)
        {
            marbit_send_log(ERROR, "recvfrom errno = %d, strerror(errno) = %s,  count_stat = %u\n", errno, strerror(errno), count_stat );
           // return NULL;
           continue;
        }
        if(len == 0)
        {
             marbit_send_log(ERROR, "count_stat = %u\n", count_stat );
           // return NULL;
            return NULL;
        }
        buff[len]='\0';  
      //  printf("receive come from %s:%d message:%s\n",  
       //     inet_ntoa(recv_control_addr.sin_addr), ntohs(recv_control_addr.sin_port), buff);

        mesg = (struct msg_st *)buff;
        if(mesg == NULL)
        {
            continue;
        }
        
        /* 2. recv stream data finished, prepare send data to web ...to be done!!!!*/
        if(1 == mesg->finish_flag)
        {
            count_stat++;
            marbit_send_log(INFO,"recv message from arbiter finished! %u\n", count_stat);
            marbit_send_log(DEBUG,"mesg->data.ip_src[%u] port_src[%u] ip_dst[%u] port_dst[%u] proto[%u] proto_mark[%u] up_bytes[%u] down_bytes[%u] duration_time[%u] finish_flag[%u]\n",
                      mesg->ip_src, mesg->port_src, 
                      mesg->ip_dst , mesg->port_dst,
                      mesg->proto,  mesg->proto_mark,
                      mesg->up_bytes, mesg->down_bytes, 
                      mesg->duration_time, mesg->finish_flag);
            
            return NULL;  
        }
       
        else //if(0 == mesg->finish_flag)
        {
            if(1 == STREAM_MESG_NUM_LIMIT_FLAG)
            {
                count++;
            }
            
            marbit_send_log(DEBUG, "mesg->data.ip_src[%u] port_src[%u] ip_dst[%u] port_dst[%u] proto[%u] proto_mark[%u] up_bytes[%u] down_bytes[%u] duration_time[%u] finish_flag[%u]\n",
                      mesg->ip_src, mesg->port_src, 
                      mesg->ip_dst , mesg->port_dst,
                      mesg->proto,  mesg->proto_mark,
                      mesg->up_bytes, mesg->down_bytes, 
                      mesg->duration_time, mesg->finish_flag);
            count_stat++;

            /* 3. recv stream data, and store into stream list! */
            insert_stream_list(mesg, &(stream_list_head.list));

             /* 4. fill data_aggregation */
            do_process_data_aggregation(mesg, g_data_agg_class, g_sorted_list_index, g_data_sort_condition);
         }   
    }  
  
    return NULL;
}


int init_arbiter_socket_info()
{
    /* define socket for aribter */
    if((arbiter_control_sock = socket(AF_INET,SOCK_DGRAM,0))==-1)  
    {  
        marbit_send_log(ERROR,"Failed to create arbiter_control_sock\n");
        arbiter_control_sock_flag = 0;
        return -1;  
    }  
    else
    {
        arbiter_control_sock_flag = 1;
        marbit_send_log(INFO,"Success to create arbiter_control_sock.\n"); 
    }
    
    memset(&arbiter_control_addr,0,sizeof(arbiter_control_addr));  
    arbiter_control_addr.sin_family = AF_INET;//AF_UNIX;//AF_INET;  
    arbiter_control_addr.sin_port   = htons(ARBITER_PORT);  
    arbiter_control_addr.sin_addr.s_addr = INADDR_ANY;  
    if(bind(arbiter_control_sock,(struct sockaddr *)&arbiter_control_addr,sizeof(arbiter_control_addr)) == -1)  
    {  
        marbit_send_log(ERROR,"Failed to bind arbiter_control_sock\n");  
        return -1;  
    }  
    else 
    {
        marbit_send_log(INFO,"Success to bind arbiter_control_sock.\n");  
    }

    return 0;
    
}

void destroy_arbiter_socket_info()
{
    if(arbiter_control_sock_flag)
    {
        close(arbiter_control_sock);
    }

    return;
}

#endif

