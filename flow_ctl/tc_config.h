#ifndef _TC_CONFIG_H_
#define _TC_CONFIG_H_

#include "tc_private.h"

void sft_reload_conf(struct tc_private_t *priv);

struct tc_private_t * sft_init_tc_work(void);
void free_sft_init_tc_work(void);

int do_parse_rule_work(struct tc_private_t *conf, char *rule_string);
inline int clean_ipgroup_entry(struct tc_private_t *conf, unsigned short ipg_idx);

#endif
