#ifndef __PATTERN_TBL_H__
#define __PATTERN_TBL_H__
#include "config.h"
#include "h_cache.h"
#include "dpi.h"

struct pattern_key
{
    uint8_t repeated;
	uint16_t pattern_len;
	int16_t  offset;
	uint16_t depth;
	uint16_t	sport;
	uint16_t dport;
	
	uint16_t pkt_dir : 1, //1:req, 0:res
	         proto_type : 3,
	         hex : 1,
	         nocase : 1,
             dynamic_indirect:3,
             mult_iplist:1,
             tlv:1;
	int place_token_id;
	uint16_t pkt_num;
	uint16_t pkt_len_min;//payload length
	uint16_t pkt_len_max;//payload length
	int first;//1:yes rule
	unsigned char pattern[PATTERN_SIZE_MAX];

	char ip_key[DYNAMIC_KEYWORD_SIZE_MAX];
    uint8_t ip_key_len;
	char port_key[DYNAMIC_KEYWORD_SIZE_MAX];
    uint8_t port_key_len;
//	uint8_t  dynamic_need_phase;
//	uint8_t  dynamic_current_phase;
    uint8_t tlv_start;
    uint8_t tlv_len;
    uint8_t tlv_type;
    int32_t tlv_offset;
	uint8_t dynamic_type;
	//uint8_t dynamic_domain;
	uint16_t dynamic_port;
	//uint16_t study_port;
	uint16_t dynamic_dir;
	uint8_t fixed_payload_len;
	uint16_t payload_len;
};


/* pattern */
typedef struct pattern_s
{
    struct h_scalar    h_scalar;
    struct pattern_key pattern_key;
    union
    {
        unsigned char place_name[MAX_PATTRN_LEN];
        unsigned char pattern_name[MAX_PATTRN_LEN];
    };
    uint8_t repeated_cnt;
	uint16_t id;
    void *priv_ptr;    
}pattern_t;

typedef pattern_t place_t;

int rules_init(void);
pattern_t *pattern_lookup_by_key(const struct pattern_key *key);
int get_place_id_by_name(const char * name);
pattern_t *get_pattern_by_name(const char * name);
pattern_t *pattern_cache_try_get(struct pattern_key *key, int *ret_status);
int build_pattern_acsm();
int pattern_cache_init(void);
void dpi_acsm_search(struct ssn_skb_values * ssv);
void pattern_clear();
int cleanup_acsm(void);
int init_acsm();
#endif
