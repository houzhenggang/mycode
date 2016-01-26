#ifndef __DPI_DYNAMIC__
#define __DPI_DYNAMIC__ 
#include "pattern_tbl.h"
#define DYNAMIC_RULE_HASH_SIZE (1<<14)

int init_dynamic_protocol(); 
void remove_dynamic_protocol();
void dynamic_parse(void *data, pattern_t *pattern, size_t offset);

#endif

