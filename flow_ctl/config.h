#ifndef __CONFIG_H
#define __CONFIG_H
#define MAX_PATTRN_LEN  40
#define DB_RULE_INFO_CB(__b) ((struct db_rule *)&((__b)->cb[0]))
#define WEB_RULE_INFO_CB(__b) ((struct web_rule *)&((__b)->cb[0]))
#define FTP_RULE_INFO_CB(__b) ((struct ftp_rule *)&((__b)->cb[0]))
#define MAIL_RULE_INFO_CB(__b) ((struct mail_rule *)&((__b)->cb[0]))

/* 关键字最大支持64个字节 */
#define PATTERN_SIZE_MAX 64
#define DYNAMIC_KEYWORD_SIZE_MAX 64
#define NAME_LEN 32
#define IPKEY_LEN 32
#define PORTKEY_LEN 32
#define FIRSTN_TOKEN 4
#define APP_CLASS_MAX 6000

#define BIG_TYPE_MASK 0xF0000000
#define MID_TYPE_MASK 0x0F000000
#define SUB_TYPE_MASK 0x00FF0000

#define SFT_MEM_POOL_SIZE 50*1024*1024    /*DO NOT CHANGE IT.pointer:sft_mem_pool,size is 50MB*/
#define L2CT_VAR_MAX 5
#define DPI_STATE_TBL_MAX 6144
#define DPI_PATTERN_MAX 5120
//#define MAX_SSN_STATE_NUM 32
#define MRULES_MAX 8

#define DPI_DISABLE 0
#define DPI_ENABLE 1

#define MAX_PKT_NUM 8
#define HTTP_MAX_PKT_NUM (MAX_PKT_NUM + 3)
#define COMMON_MAX_PKT_NUM  (MAX_PKT_NUM + 15)
#define APP_CLASS_ID_NOT_DEFINED (-1)
#endif
