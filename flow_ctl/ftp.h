#ifndef __FTP_TBL_H
#define __FTP_TBL_H
#include <linux/types.h>
#include "com.h"
    


#define FTP_HASH_SIZE    (1 << 9)
#define FTP_MAX_LEN      (FTP_HASH_SIZE * 4)
#define FTP_TIMEOUT      (120)
#define FTP_OUTPUT_INTERVAL    (300)
    
#define FTP_RULE_RECHECK_TIMEOUT  10
//struct study_cache *study_cache_try_get(const __u32 ip, const __u16 port, unsigned int proto_mark);
//int ftp_cache_try_get(const __u32 sip, const __u32 dip, const __u16 dport, unsigned int proto_mark);
void detect_ftp_proto(struct ssn_skb_values *ssv);
unsigned int ftp_lookup_behavior(const __u32 sip, const __u32 dip, __u16 dport);
int ftp_cache_init(void);
void ftp_cache_exit(void);
#endif                                                                                                                         
