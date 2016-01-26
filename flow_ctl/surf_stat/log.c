//#ifdef __cplusplus
//extern "C" {
// TO BE DONE
//#endif
//#ifdef __cplusplus
//}
//#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include "log.h"

extern log_debug_t logdebug;

void get_time(char *time_buff)
{
    time_t time(), now;
    struct tm *t_time, *localtime();
    time(&now);
    t_time = localtime(&now);
    sprintf(time_buff, "%.2d:%.2d:%.2d", t_time->tm_hour, t_time->tm_min, t_time->tm_sec);
}

void get_date(char *date_buff)
{
    time_t time(), now;
    struct tm *t_time, *localtime();
    time(&now);
    t_time = localtime(&now);
    sprintf(date_buff, "%.4d%.2d%.2d", t_time->tm_year + 1900, t_time->tm_mon + 1, t_time->tm_mday);
}

int mab_log(char *g_trace_file, int log_level, char *log_src_file, int log_src_line, char *log_msg)
{
    FILE *fp = NULL;
    char level_buff[LEN_LOG_LEVEL_BUFF + 1];
    char current_date[LEN_LOG_DATE + 1];
    char current_time[LEN_LOG_TIME + 1];
    char log_file_name[LEN_LOG_FILE_NAME + 1];


    if(logdebug.g_trace_level < 0 || logdebug.g_trace_level > log_level)
    {
        return 0;
    }

    memset(level_buff, 0, sizeof(level_buff));
    switch(log_level)
    {
        case ERROR_F:
            sprintf(level_buff, "ERROR");
            break;
        case INFO_F:
            sprintf(level_buff, "INFO");
            break;
        case DEBUG_F:
            sprintf(level_buff, "DEBUG");
            break;
        default:
            sprintf(level_buff, "DEFAULT");
            break;
    }

    memset(current_date, 0, sizeof(current_date));
    get_date(current_date);

    memset(log_file_name, 0, sizeof(log_file_name));
    sprintf(log_file_name, "%s_%s.log", g_trace_file, current_date);
 
    fp = fopen(log_file_name, "a+");
    if (fp == (FILE *) NULL)
    {
        //printf("open log error\n");
        return 0;
    }

    memset(current_time, 0, sizeof(current_time));
    get_time(current_time);

    fprintf(fp, "[%s][%s:%d] %s\n",
            current_time, log_src_file, log_src_line, log_msg);
    
    fclose(fp);
    
    return 0;
}


int marbit_send_log(char *fmt, ...)
{
    if(0 == logdebug.g_trace_enable_flag)
    {  
        return 0;
    }
    
    
    va_list args;
    int log_src_line = -1;
    int log_level = -1;
    char *log_src_file = NULL;
    char log_msg[LEN_LOG_MSG + 1];

    log_level = (int)fmt;
    va_start(args, fmt);
    log_src_file = va_arg(args, char *);
    log_src_line = va_arg(args, int);
    fmt = va_arg(args, char *);
    memset(log_msg, 0, sizeof(log_msg));
    vsprintf(log_msg, fmt, args);
    va_end(args);

    mab_log(logdebug.g_trace_file, log_level, log_src_file, log_src_line, log_msg);
}

#if 0
void write_log(char *str)
{
   // return;
    
    char buf[512];
    char buf1[512];
    char log_name[512];
    long MAXLEN = 10*1024*1024;
    time_t timep; 
    FILE *fp = NULL;
    struct tm *p; 

    time(&timep); 
    p = localtime(&timep); 
    memset(buf,0,sizeof(buf));
    memset(buf1,0,sizeof(buf));
    memset(log_name,0,sizeof(log_name));
    sprintf(buf1, "%d",(1900+p->tm_year));
    if ((1+p->tm_mon) < 10) {
        sprintf(buf1+strlen(buf1), "-0%d",(1+p->tm_mon));
    } else {
        sprintf(buf1+strlen(buf1), "-%d",(1+p->tm_mon));
    }
    if ((p->tm_mday) < 10) {
        sprintf(buf1+strlen(buf1), "-0%d",(p->tm_mday));
    } else {
        sprintf(buf1+strlen(buf1), "-%d",(p->tm_mday));
    }
    if ((p->tm_hour) < 10) {
        sprintf(buf1+strlen(buf1), ":0%d",(p->tm_hour));
    } else {
        sprintf(buf1+strlen(buf1), ":%d",(p->tm_hour));
    }
    if ((p->tm_min) < 10) {
        sprintf(buf1+strlen(buf1), ":0%d",(p->tm_min));
    } else {
        sprintf(buf1+strlen(buf1), ":%d",(p->tm_min));
    }
    if ((p->tm_sec) < 10) {
        sprintf(buf1+strlen(buf1), ":0%d",(p->tm_sec));
    } else {
        sprintf(buf1+strlen(buf1), ":%d",(p->tm_sec));
    }


    //sprintf(buf1,"%d-%2d-%2d %2d:%2d:%2d: ",(1900+p->tm_year),(1+p->tm_mon),
    //        p->tm_mday,p->tm_hour, p->tm_min, p->tm_sec);
    sprintf(buf,"%20s %s\r\n", buf1, str) ;
//    strcat(buf,str);
    //strcat(buf,"\r\n");
    
    sprintf(log_name,"%d-%d-%d-syslog.log",(1900+p->tm_year),(1+p->tm_mon),
            p->tm_mday);
    fp = fopen(log_name,"r");
    if(fp==NULL)
    {
        fp = fopen(log_name,"w+");
    }
    else
    {
        fseek(fp,0,2);
        fclose(fp);
        fp = fopen(log_name, "a");
    }
    fwrite(buf,1,strlen(buf),fp);
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);
}
#endif

#if 0
void save2file(char *filename, char *str)
{
    char buf[512];
    FILE *fp = NULL;

    sprintf(buf,"%s\r\n", str) ;
    
    fp = fopen(filename,"r");
    if(fp==NULL)
    {
        fp = fopen(filename,"w+");
    }
    else
    {
        fseek(fp,0,2);
        fclose(fp);
        fp = fopen(filename, "a");
    }
    fwrite(buf,1,strlen(buf),fp);
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);
}
#endif
