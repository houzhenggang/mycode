#include <stdio.h>
#include <string.h>
#include <time.h>

void write_log(char *str)
{
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
