#ifndef __APP_CLASS_TBL_H__
#define __APP_CLASS_TBL_H__
#include "config.h"
#include "h_cache.h"
/* pattern */
typedef struct app_class_s
{
    struct h_scalar    h_scalar;
    char     type_name[NAME_LEN];
    /* id 0 is unknown, every mid or sub type started from 1 */
    uint32_t type_id;
}app_class_t;

struct app_class_key {
    uint32_t type_id;
};

int app_class_cache_init(void);
void app_class_cache_exit(void);
app_class_t *app_class_lookup_by_key(const struct app_class_key *key);
int get_app_class_id_by_name(const char * name);
app_class_t *get_app_class_by_name(const char * name);
app_class_t *app_class_cache_try_get(struct app_class_key *key, int *ret_status);
int init_app_stat();
void app_class_clear();
#endif
