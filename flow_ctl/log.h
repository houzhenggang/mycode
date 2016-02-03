#ifndef __LOG_H_
#define __LOG_H_

#define LEN_LOG_FILE_NAME      512
#define LEN_LOG_MSG                 4096
#define LEN_LOG_LEVEL_BUFF     10
#define LEN_LOG_DATE                10
#define LEN_LOG_TIME                 8

#define ERROR   9, __FILE__, __LINE__
#define INFO      6, __FILE__, __LINE__
#define DEBUG   3, __FILE__, __LINE__

#define ERROR_F     9
#define INFO_F        6
#define DEBUG_F     3

typedef struct log_debug_st
{
    int g_trace_enable_flag;
    int g_trace_level;
    char g_trace_file[LEN_LOG_FILE_NAME + 1];
}log_debug_t;

#endif
