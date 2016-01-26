#ifndef DPI_LOG
#define DPI_LOG
#include "com.h"
#include "pattern_tbl.h"

#define URL_LOG_LEN 256
#define URL_WIDTH 128 
#define HOST_WIDTH 128

#define QQ_LOG_LEN 128
#define QQ_NUM_LEN 4
#define DPI_LOG_MAX_LEN 2048

struct log_s
{
    union
    {
        char url[URL_LOG_LEN];
        char qq[QQ_LOG_LEN];
    };
    uint16_t len;
}; 

//struct log_s *get_log_mem(uint32_t lcore_id);
//void put_log_mem(struct log_s *log, uint32_t lcore_id);
//int init_dpi_log_meme(uint32_t lcore_id);
int export_url_log(struct ssn_skb_values * ssv);
int export_qq_log(struct ssn_skb_values * ssv);
int export_login(struct ssn_skb_values * ssv, pattern_t *pattern, char *usename);


#endif

