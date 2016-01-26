#ifndef __DEBUG__H
#define __DEBUG__H
#include<syslog.h>
//#include <rte_cycles.h>

//#define DEBUG
#ifdef DEBUG

    extern char *DebugMessageFile;
    extern int DebugMessageLine;

    #define    DebugMessage    DebugMessageFile = __FILE__; DebugMessageLine = __LINE__; DebugMessageFunc

    void DebugMessageFunc(int , char *, ...);

    int GetDebugLevel (void);
    int DebugThis(int level);

#endif /* DEBUG */


#ifdef DEBUG
#define DEBUG_WRAP(code) code
void DebugMessageFunc(int dbg,char *fmt, ...);
#else
#define DEBUG_WRAP(code)
/* I would use DebugMessage(dbt,fmt...) but that only works with GCC */

#endif

#ifndef HAVE_ARCH_BUG
#define BUG() do { \
        printf("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __FUNCTION__); \
} while (0)
#endif



#ifndef HAVE_ARCH_BUG_ON
#define BUG_ON(condition) do { if (unlikely((condition)!=0)) BUG(); } while(0)
#else
#define BUG_ON(condition) 
#endif

#define LOG(fmt,args...) syslog(LOG_INFO,"ERROR: [%s - %d - %s]: "fmt, __FILE__, __LINE__, __func__, ##args)

#ifdef DEBUG
#define D(fmt,args...) fprintf(stderr, "DEBUG: [%s - %d - %s]: "fmt, __FILE__, __LINE__, __func__, ##args)
#else
#define D(fmt,args...)
#endif

#define E(fmt,args...) fprintf(stderr, "ERROR: [%s - %d - %s]: "fmt, __FILE__, __LINE__, __func__, ##args)
//#define E(fmt,args...) syslog(LOG_INFO,"ERROR: [%s - %d - %s]: "fmt, __FILE__, __LINE__, __func__, ##args)
#endif
