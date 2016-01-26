#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_timer.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "init.h"
#include "utils.h"
#include "rules.h"
#include "mstring.h"
#include "pattern_tbl.h"
#include "sft_statictics.h"
#include "app_class_tbl.h"
#include "debug.h"
#include "export.h"
#include "dpi_dynamic.h"
#include "dynamic_tbl.h"
#include "global.h"
#include "fixed_length_dpi.h"
#define MAX_FILE_ITEM 4

struct state_array *DPI_State_TBL;
int *DPI_State_Flag_TBL;
static time_t pre_modify_time = 0;

static int sft_dpi_print_flag = 0;
struct dpi_user_proto user_tcp_proto[0XFFFF+1];
struct dpi_user_proto user_udp_proto[0XFFFF+1];
struct dpi_user_proto_node user_tcp_proto_node[0XFFFF+1][SAMEPORT_IPNUM];
struct dpi_user_proto_node user_udp_proto_node[0XFFFF+1][SAMEPORT_IPNUM];

static struct rte_timer timer;

unsigned int tcp_server_port[0XFFFF+1];
unsigned int tcp_client_port[0XFFFF+1];
unsigned int udp_server_port[0XFFFF+1];
unsigned int udp_client_port[0XFFFF+1];

uint32_t http_proto_appid = 0;
//uint16_t place_cnt = 0;
/* pattern count */
static uint32_t pattern_cnt = TOKEN_MAX;

static int G_place_token_id = 0;
static uint16_t G_state_id_inc = 1;
/* PROTO_TYPE_MAX is 7 */
struct proto_type_s
{
    uint16_t id;
    char proto_name[NAME_LEN];
};
struct proto_type_s proto_type[DPI_PROTO_TYPE_MAX] =
{
    {DPI_PROTO_TCP, "TCP"},
    {DPI_PROTO_UDP, "UDP"},
    {DPI_PROTO_HTTP, "HTTP"},
    {DPI_PROTO_ICMP, "ICMP"},
};


void move_step(char * string, int str_len)
{
    int i;
    for (i = 0; i < str_len-1; i++)
    {
        string[i] = string[i + 1];
    }
    string[i] = 0;
}

int remove_backlash(char * string)
{

#if 0
    int len = strlen(string);
    int retv = -1;
    int i;
 
    if ((retv = substr_in_mainstr_nocase(string, len, "\\", ',')) > 0) {

        string[retv - 1] = ' ';
        for (i = retv ; i < len; i ++)
            string[i] = string[i + 1];
        
        len =- 1;
        //string[retv - 1] = ' ';
        //string[len] = '\0';
    }
    return len;
#else
    int len = strlen(string);
    int i;

    for (i = 0; i < len; i++)
    {
        if (string[i] == '\\')
        {
            move_step(string + i, len - i);
            len -= 1;
        }
    }
#endif
    return len;
}
/* str example:34-5d-7f-06 */
static int sft_hstoh(char * str, unsigned char * hex)
{
    int len;
    int idx;
    int offset;
    unsigned char value;

    len = strlen(str);
    for (offset = 0, idx = 0; offset < len; offset++)
    {

        if (str[offset] == '-')
        {
            continue;
        }

        if ((str[offset] >= 'a') && (str[offset] <= 'f'))
        {
            value = str[offset] - 'a' + 10;
        }
        else if ((str[offset] >= 'A') && (str[offset] <= 'F'))
        {
            value = str[offset] - 'A' + 10;
        }
        else if ((str[offset] >= '0') && (str[offset] <= '9'))
        {
            value = str[offset] - '0';
        }
        else
        {
            continue;
        }
        if ((idx % 2) == 0)
        {
            hex[idx / 2] = value << 4;
        }
        else
        {
            hex[idx / 2] += value;
        }
        idx++;
    }

    if (idx % 2)
    {
        return (idx / 2) + 1;
    }
    else
    {
        return idx / 2;
    }
}

static time_t get_last_conf_modify_time()
{
    struct conf_node *node = NULL;
    char app_class_path[128] = {0};
    struct stat buf1,buf2, buf3;
    int i, ret1,ret2, ret3;
    time_t last_time, cur_time;
        for (i = 0; i < MAX_DPI_FILE_TYPE; i++) {
            list_for_each_entry(node, &pv.dpi_conf[i].conf_head, list) {
                strncpy(app_class_path, pv.key_lib_path, 128);
                strncat(app_class_path, node->conf_name, 128 - strlen(node->conf_name));

                ret1 =stat(app_class_path, &buf1);
                if (ret1 != 0)
                    return 0;
                cur_time = buf1.st_mtime;
                if (last_time == 0) {
                    last_time = cur_time;
                    continue;
                }
                last_time = (last_time > cur_time ? last_time :cur_time);

            }
        }

    //获得文件状态信息
    ret2 =stat("/opt/utc/conf/proto_useradd.conf", &buf2);
    ret3 =stat("/opt/utc/conf/proto_nodemgt.conf", &buf3);

    //显示文件状态信息
    if(ret2 != 0||ret3 != 0)
    {
        return 0;
    }
    if (last_time > buf2.st_mtime) {
        return (last_time > buf3.st_mtime ? last_time : buf3.st_mtime);
    } else {
        return (buf2.st_mtime > buf3.st_mtime ? buf2.st_mtime : buf3.st_mtime);
    }
}

/* if not found return -1£¬ else return id */
static int get_proto_id_by_name(char * name)
{
    int i;
    for(i = 0; i < DPI_PROTO_TYPE_MAX; i++)
    {
        
        if(!strcmp_nocase(name, proto_type[i].proto_name))
        {
           //D("name: %s, id:%d\n", name, proto_type[i].id);
            return proto_type[i].id;
        }
    }
    return -1;
}

static int init_dpi_conf()
{
    char keylib_index[1024] = {0};
    FILE *index_fp;
    char line[512];
    char *p;
    char **toks;
    int num_toks;
    struct conf_node *node = NULL;
    int retv = -1;
    int index, j;
    
    strncpy(keylib_index, pv.key_lib_path, 1024);
    strncat(keylib_index, "index.sft", 1024 - strlen(pv.key_lib_path));
    if ((index_fp = fopen(keylib_index, "rb")) == NULL) {
        E("-------open keylib index.sft fail.\n");
        return -1;
    }
    while(fgets(line, 512, index_fp))  {
        if (*line == '#')
            continue;
        toks = mSplit(line, "=", 3, &num_toks, 0);
        if(num_toks == 3) {
            if ((p = strchr(toks[1], ','))) {
                *p = 0x00;
                str_trim(toks[1]);
            } else {
                    goto free_toks;
            }


            for (index = 0; index < MAX_DPI_FILE_TYPE; index ++) {
                if (strncmp(toks[1], pv.dpi_conf[index].conf_type_name, strlen(pv.dpi_conf[index].conf_type_name)) ==0){
                    break;
                }

            }
            if (index >= MAX_DPI_FILE_TYPE)
                    goto free_toks;
                
            node = rte_zmalloc(NULL, sizeof(struct conf_node), 0);           
            if (unlikely(node == NULL)) {
                mSplitFree(&toks, num_toks);
                return -1;
            }
            node->conf_name = strdup(toks[2]);
            list_add(&node->list, &pv.dpi_conf[index].conf_head);

        }
free_toks:
        memset(line, 0x00, 512);
        mSplitFree(&toks, num_toks);
    }
    fclose(index_fp);
    for (j = 0; j < MAX_DPI_FILE_TYPE; j ++) {
        list_for_each_entry(node, &pv.dpi_conf[j].conf_head, list) {
            printf("name[%s]\n", node->conf_name);
        }
    }

    return 0;
}
/*******************************************************************/

enum app_class_field_item {
    BIG_APP_CLASS_ITEM = 0, MID_APP_CLASS_ITEM, SUB_APP_CLASS_ITEM,MAX_APP_CLASS_FIELD_ITEM,
};
static const char *app_class_field[] = {
    "big", "mid", "sub",
};

static int load_app_class()
{
    app_class_t *app_class = NULL;
    char app_class_path[128] = {0};
    FILE *fp;
    char line[1024];
    char export_buf[256];
    int total_app_class_cnt = 0;
    int flag = 0;
    uint32_t type_id,  big_type_id = 0, mid_type_id = 0, sub_type_id = 0;
    char *type_name_field;
    char type_name[NAME_LEN];
    struct app_class_key key;
    char *p;
    char **toks;
    int num_toks;
    int ret_status = 0;
    struct conf_node *node = NULL;
    int retv = 0;
    int index, j;

    list_for_each_entry(node, &pv.dpi_conf[APP_CLASS_TYPE].conf_head, list) {


        strncpy(app_class_path, pv.key_lib_path, 128);
        strncat(app_class_path, node->conf_name, 128 - strlen(node->conf_name));
        if ((fp = fopen(app_class_path, "rb")) == NULL) {
            E("-------open app class file [%s] fail.\n", app_class_path);
            continue;
        }
        while(fgets(line, 1024, fp))  {
            if (*line == '#')
                continue;
            trim_specific(line, "<");
            toks = mSplit(line, "=", 4, &num_toks, 0);
            if(unlikely(num_toks != 4))
                goto free_toks;
            //parse type 
            if ((p = strchr(toks[0], ' '))) {
                *p = 0x00;
                _strim(toks[0]);
            } else {
                goto free_toks;
            }
            //parse type 
            if ((p = strchr(toks[1], ' '))) {
                *p = 0x00;
                trim_specific(toks[1], "\"");
                p++;
                _strim(p);

                type_id = (uint32_t) atoi(toks[1]);
                type_name_field = p;
            }else {
                    goto free_toks;
            }
            if ((p = strchr(toks[2], ' '))) {
                *p = 0x00;
                _strim(toks[2]);
            } else {
                goto free_toks;
            }
            //  printf("type[%s], type_id=%u, [%s]=[%s]\n", toks[0],type_id, type_name_field, toks[2]);

            for (index = 0; index < MAX_APP_CLASS_FIELD_ITEM; index ++) {
                if (strncmp(toks[0], app_class_field[index], strlen(app_class_field[index])) ==0){
                    break;
                }

            }
            if (index >= MAX_APP_CLASS_FIELD_ITEM)
                goto free_toks;

             
            switch(index) {
                case BIG_APP_CLASS_ITEM:
                    {
                        if (unlikely(strncmp(type_name_field, "type_name", 9))) {
                            E("---big type name error-----\n");
                            goto free_toks;
                        }
                        if (type_id > 15 /*|| type_id == 0*/) {
                            E("---big type id gt 15-----\n");
                            goto free_toks;
                        }
                        //big_type_id = 0x0FFF0000; 
                        big_type_id = type_id << 28; 
                        key.type_id = big_type_id | 0x0FFF0000 ;

                         pv.app_class_cnt++; 
                    }
                    break;
                    //===============================================
                case MID_APP_CLASS_ITEM:
                    {
                        if (unlikely(strncmp(type_name_field, "mid_type_name", 13))) {
                            E("mid type name error\n");
                            goto free_toks;
                        }

                        if (type_id > 14 /*|| type_id == 0*/) {
                            E("---mid type id gt 14-----\n");
                            goto free_toks;
                        }
                        key.type_id = big_type_id;
                        mid_type_id = type_id << 24;
                        key.type_id |= mid_type_id | 0xff0000;
                        
                        pv.app_class_cnt++; 

                    }
                    break;
                    //===============================================
                case SUB_APP_CLASS_ITEM:
                    {
                        if (unlikely(strncmp(type_name_field, "sub_type_name", 13))) {
                            E("type name error\n");
                            goto free_toks;
                        }

                        if (type_id > 254/*||type_id == 0*/) {
                            E("---mid type id gt 254-----\n");
                            goto free_toks;
                        }
                        key.type_id = big_type_id;
                        key.type_id |= mid_type_id;
                        sub_type_id = type_id << 16;
                        key.type_id |= sub_type_id;
                        key.type_id &= 0xFFFF0000;
                        pv.app_class_cnt++; 
                       // printf("[%x]->[%s]\n", key.type_id, toks[2]);
                    }
                    break;
                    //================================================
                default:
                    goto free_toks;
            }
            app_class = app_class_cache_try_get(&key, &ret_status);
            if (likely(ret_status == 0)) {
                strncpy(app_class->type_name, trim_specific(toks[2], "\""), NAME_LEN);
                bzero(export_buf, 256);
                sprintf(export_buf, "[%d]type_id=%u[0x%02x],type_name=%s.\n", total_app_class_cnt++ ,app_class->type_id,app_class->type_id, app_class->type_name);
                export_file(APP_CLASS_FILE_TYPE, export_buf, flag);
                flag = 1;
            } else {
                D("already exist [%s]-[%s]\n", app_class->type_name, toks[2]);
            }
//            printf("===app[%s]\n", app_class->type_name); 
free_toks:
            memset(line, 0x00, 1024);
            mSplitFree(&toks, num_toks);
        }

        fclose(fp); 
    }
#if 0
    key.type_id = 0;
    app_class = app_class_cache_try_get(&key, &ret_status);
    if (likely(ret_status == 0)) {
        strncpy(app_class->type_name, "unknown", NAME_LEN);
        bzero(export_buf, 256);
        sprintf(export_buf, "[%d]type_id=%u[0x%02x],type_name=%s.\n", total_app_class_cnt++ ,app_class->type_id,app_class->type_id, app_class->type_name);
        export_file(APP_CLASS_FILE_TYPE, export_buf, 1);
    } else {
        D("already exist unknown app class.\n");
    }
#endif
    export_file(APP_CLASS_FILE_TYPE, NULL, 2);
}


/*******************************************************************/

enum place_field_item {
    PROTO_TYPE_PLACE_ITEM = 0, PLACE_NAME_PLACE_ITEM, NOCASE_PLACE_ITEM, HEX_PLACE_ITEM, 
    PATTERN_PLACE_ITEM,PATTERN_LEN_PLACE_ITEM, MAX_PLACE_FIELD_ITEM,
};
static const char *place_field[] = {
    "proto_type", "place_name", "nocase", "hex", "pattern",
     "pattern_len", 
};

static int load_place()
{
    place_t *place_node = NULL;
    char place_path[128] = {0};
    FILE *fp;
    char line[1024] = {0};
    char place_name[MAX_PATTRN_LEN];
    char pattern_str[PATTERN_SIZE_MAX];
    struct pattern_key key;
    char *p;
    char **toks;
    int num_toks;
    int ret_status = 0;
    struct conf_node *node = NULL;
    int retv = 0;
    int i, j;

    pv.place_cnt = 0;
    list_for_each_entry(node, &pv.dpi_conf[PLACE_TYPE].conf_head, list) {
        
        
        strncpy(place_path, pv.key_lib_path, 128);
        strncat(place_path, node->conf_name, 128 - strlen(node->conf_name));
        if ((fp = fopen(place_path, "rb")) == NULL) {
            E("-------open place file [%s]  fail.\n", place_path);
            continue;
        }
        printf("========pattern file[%s]\n", place_path);
        while(fgets(line, 1024, fp))  {
            if (*line == '#')
                continue;
//        printf("line[%s]\n", line);
            toks = mSplit(line, ",", 18, &num_toks, 0);
            if(unlikely(num_toks < 5))
                goto free_toks;

//            printf("=======num_toks=%d=======\n",num_toks);


            bzero(&key, sizeof(struct pattern_key));
            bzero(place_name, MAX_PATTRN_LEN);
            bzero(pattern_str, PATTERN_SIZE_MAX);
            key.place_token_id = -1;
            retv = 0;
            for (i = 0; i < num_toks; i++) {

                
                if ((p = strchr(toks[i], '='))) {
                    *p = 0x00;
                    //str_trim(toks[i]);
                    _strim(toks[i]);
                    p++;
              //      printf("--------p=[%s]\n",  p); 
                }
//                printf("[%s]-->[%s]\n", toks[i], p);
                for (j = 0; j < MAX_PLACE_FIELD_ITEM; j++) {
                    int cmp_len = strlen(place_field[j]) > strlen(toks[i]) ? strlen(place_field[j]) : strlen(toks[i]);
                    if (!strncmp(toks[i], place_field[j], cmp_len)) {
                        break;
                    }
                }
                if (j >= MAX_PLACE_FIELD_ITEM)
                    goto free_toks;


                switch(j) {
                    case PLACE_NAME_PLACE_ITEM:
                        {
                            _strim(p);
                            strncpy(place_name,  p, MAX_PATTRN_LEN - 1);
                            if (get_pattern_by_name(p)) {
                                retv = -1;
                                D("already exist place:%s.\n",place_name);
                            }
                        }
                        break;
                    case PROTO_TYPE_PLACE_ITEM:
                        {
                            _strim(p);
                            uint8_t tmp = get_proto_id_by_name(p);
                            if (unlikely(tmp < 0 || tmp >= DPI_PROTO_TYPE_MAX))
                            {
                                D("[no this proto] -> %s\n", tmp);
                                retv = -1;
                                break;
                            }
                            key.proto_type = tmp;
                        }
                        break;
                    case PATTERN_LEN_PLACE_ITEM:
                        {
                            _strim(p);
                            key.pattern_len = (uint16_t) atoi(p);
                        }
                        break;

                    case PATTERN_PLACE_ITEM:
                        {
                            strim_except_space(p);
                            strncpy(pattern_str,  p, PATTERN_SIZE_MAX - 1);
                        }
                        break;

                    case HEX_PLACE_ITEM:
                        {
                            _strim(p);
                            key.hex = !!atoi(p);

                        }
                        break;

                    case NOCASE_PLACE_ITEM:
                        {
                            _strim(p);
                            key.nocase = !!atoi(p);

                        }
                        break;

                    //default:

                }
                if (retv == -1)
                    break;
            }

            if (retv == 0) {
                if (strlen(pattern_str) > 0) {
                    if (unlikely(key.pattern_len <= 0))
                        continue;

                    if (key.hex == 1) {
                        key.pattern_len = sft_hstoh(pattern_str, key.pattern);
                    } else {
                        remove_backlash(pattern_str);
                        if(key.pattern_len >= PATTERN_SIZE_MAX)
                        {
                            key.pattern_len = PATTERN_SIZE_MAX - 1;
                        }
                        strncpy(key.pattern, pattern_str, key.pattern_len);
                    }
                }
                key.place_token_id = G_place_token_id ++ ;
                place_node = pattern_cache_try_get(&key, &ret_status);
                if (ret_status == 0) {
                    place_node->id = pv.place_cnt++;
                    strncpy(place_node->place_name, place_name, MAX_PATTRN_LEN - 1);
                } else {
                    D("It is already exist place:%s\n", place_node->place_name);
                }
                //printf("place_token_id=%d,place id=%d,place name[%s]\n",key.place_token_id,place_node->id, place_name);
 //               D("ret_status=%d\n",ret_status);
            }
#if 0
            if ((p = strchr(toks[1], ','))) {
                *p = 0x00;
                str_trim(toks[1]);
            }
#endif
#if 0
    D("<<pattern_len=%d, proto_type=%d, hex=%d, nocase=%d\nplace_token_id=%d, pattern=%s>>\n",
                key.pattern_len, key.proto_type, 
                key.hex, key.nocase, key.place_token_id, 
                key.pattern); 
#endif
free_toks:
            memset(line, 0x00, 1024);
            mSplitFree(&toks, num_toks);
        }

        fclose(fp); 
    }
}

/*******************************************************************/

enum pattern_field_item {
    PATTERN_NAME_ITEM = 0, PKT_DIR_ITEM, PKT_NUM_ITEM, PKT_LEN_MIN_ITEM, PKT_LEN_MAX_ITEM, 
    PROTO_TYPE_ITEM, SPORT_ITEM, DPORT_ITEM, PLACE_ITEM, PATTERN_LEN_ITEM, PATTERN_ITEM, 
    HEX_ITEM, NOCASE_ITEM, OFFSET_ITEM, DEPTH_ITEM, DYNAMIC_ITEM, DYNAMIC_NEED_PHASE_ITEM, 
    DYNAMIC_CURRENT_PHASE_ITEM, DYNAMIC_PORT_ITEM, DYNAMIC_DIR_ITEM, DYNAMIC_PORTKEY_ITEM, 
    DYNAMIC_PORTKEY_LEN_ITEM, DYNAMIC_IPKEY_ITEM, DYNAMIC_IPKEY_LEN_ITEM,DYNAMIC_DOMAIN_ITEM, 
    DYNAMIC_MUTIL_IPLIST_ITEM, DYNAMIC_INDIRECT_ITEM, FIXED_PAYLOAD_LEN_ITEM,PAYLOAD_LEN_ITEM,
    LOG_TYPE_ITEM, 
    LOGIN_UID_START_KEY_ITEM,LOGIN_UID_START_KEY_LEN_ITEM, LOGIN_UID_END_KEY_ITEM, LOGIN_UID_END_KEY_LEN_ITEM,
    POST_UID_START_KEY_ITEM,POST_UID_START_KEY_LEN_ITEM, POST_UID_END_KEY_ITEM, POST_UID_END_KEY_LEN_ITEM,
    POST_TID_START_KEY_ITEM,POST_TID_START_KEY_LEN_ITEM, POST_TID_END_KEY_ITEM, POST_TID_END_KEY_LEN_ITEM,
    TLV_START_ITEM, TLV_LEN_ITEM, TLV_TYPE_ITEM, TLV_OFFSET_ITEM,
    MAX_PATTERN_FIELD_ITEM
};
static const char *pattern_field[] = {
    "pattern_name", "pkt_dir", "pkt_num", "pkt_len_min", "pkt_len_max",
    "proto_type", "sport", "dport", "place", "pattern_len", "pattern",
    "hex", "nocase", "offset", "depth", "dynamic", "dynamic_need_phase",
    "dynamic_current_phase", "dynamic_port", "dynamic_dir", "dynamic_portkey",
    "dynamic_portkey_len", "dynamic_ipkey", "dynamic_ipkey_len","dynamic_domain", 
    "dynamic_mutil_iplist", "dynamic_indirect","fixed_payload_len","payload_len", 
    "log_type", 
     "login_uid_start_key", "login_uid_start_key_len","login_uid_end_key", "login_uid_end_key_len",
     "post_uid_start_key", "post_uid_start_key_len","post_uid_end_key", "post_uid_end_key_len",
     "post_tid_start_key", "post_tid_start_key_len","post_tid_end_key", "post_tid_end_key_len",
     "tlv_start", "tlv_len", "tlv_type", "tlv_offset"
};

static int load_pattern()
{
    pattern_t *pattern_node = NULL;
    char pattern_path[128] = {0};
    char export_buf[256];
    int flag = 0;

    FILE *fp;
    char line[1024] = {0};
    char pattern_name[MAX_PATTRN_LEN];
    char pattern_str[PATTERN_SIZE_MAX];
    struct pattern_key key;
    char *p;
    char **toks;
    int num_toks;
    int ret_status = 0;
    struct conf_node *node = NULL;
    int retv = 0;
    int i, j;
	uint16_t repeated_pattern_id = 0;
	struct fixed_length_cache_key flck;
	struct fixed_length_cache *flc = NULL;
    pattern_cnt = TOKEN_MAX;
    list_for_each_entry(node, &pv.dpi_conf[PATTERN_TYPE].conf_head, list) {
        
        
        strncpy(pattern_path, pv.key_lib_path, 128);
        strncat(pattern_path, node->conf_name, 128 - strlen(node->conf_name));
        if ((fp = fopen(pattern_path, "rb")) == NULL) {
            E("-------open pattern file [%s] fail.\n", pattern_path);
            continue;
        }
//        printf("========pattern file[%s]\n", pattern_path);
        while(fgets(line, 1024, fp))  {
            if (*line == '#') {
                memset(line, 0x00, 1024);
                continue;
            }
//        printf("line[%s]\n", line);
            toks = mSplit(line, ",", 18, &num_toks, 0);
            if(unlikely(num_toks < 4))
                goto free_toks;

  //          printf("=======num_toks=%d=======\n",num_toks);


            bzero(&key, sizeof(struct pattern_key));
            bzero(pattern_name, MAX_PATTRN_LEN);
            bzero(pattern_str, PATTERN_SIZE_MAX);
            key.place_token_id = -1;
            retv = 0;
            for (i = 0; i < num_toks; i++) {

                if ((p = strchr(toks[i], '='))) {
                    *p = 0x00;
                    //str_trim(toks[i]);
                    _strim(toks[i]);
                    p++;
              //      printf("--------p=[%s]\n",  p); 

                } else {
                    continue;
                }
                if (i == num_toks - 1) {
                    strim_except_space(p);
                }
                for (j = 0; j < MAX_PATTERN_FIELD_ITEM; j++) {
                    int cmp_len = strlen(pattern_field[j]) > strlen(toks[i]) ? strlen(pattern_field[j]) : strlen(toks[i]);
                    if (!strncmp(toks[i], pattern_field[j], cmp_len)) {
                        break;
                    }
                }
                if (j >= MAX_PATTERN_FIELD_ITEM)
                    goto free_toks;


                switch(j) {
                    case PATTERN_NAME_ITEM:
                        {
                            _strim(p);
                            strncpy(pattern_name,  p, MAX_PATTRN_LEN - 1);
                            if (get_pattern_by_name(p)) {
                                retv = -1;
                                D("already exist pattern:%s.\n",pattern_name);
                            }
                        }
                        break;
                    case PKT_DIR_ITEM:
                        {
                            _strim(p);
                            if (!strcmp_nocase(p, "res")) 
                                key.pkt_dir = 0;
                            else if (!strcmp_nocase(p, "req"))
                                key.pkt_dir = 1;
                            else {
                                retv = -1;
                                D("[no pkt_dir]\n");
                            }
                        }
                        break;
                    case PKT_LEN_MIN_ITEM:
                        {
                            _strim(p);
                            key.pkt_len_min = (uint16_t) atoi(p);
                        }
                        break;
                    case PKT_LEN_MAX_ITEM:
                        {
                            _strim(p);
                            key.pkt_len_max = (uint16_t) atoi(p);
                        }
                        break;

                    case PROTO_TYPE_ITEM:
                        {
                            _strim(p);
                            uint8_t tmp = get_proto_id_by_name(p);
                            if (unlikely(tmp < 0 || tmp >= DPI_PROTO_TYPE_MAX))
                            {
                                D("[no this proto] -> %s\n", tmp);
                                retv = -1;
                                break;
                            }
                            key.proto_type = tmp;
                        }
                        break;
                    case SPORT_ITEM:
                        {
                            _strim(p);
                            key.sport = htons((uint16_t) atoi(p));
                        }
                        break;

                    case DPORT_ITEM:
                        {
                            _strim(p);
                            key.dport = htons((uint16_t) atoi(p));
                        }
                        break;

                    case PLACE_ITEM:
                        {
                            _strim(p);
                            //printf("place[%s]\n", p);
#if 1
                            if ((key.place_token_id = get_place_id_by_name(p)) == -1)
                               retv = -1;
                           // D("key.place_token_id=%d\n", key.place_token_id);
#endif
    
                           // key.sport = (uint16_t) atoi(p);
                        }
                        break;

                    case PATTERN_LEN_ITEM:
                        {
                            _strim(p);
                            key.pattern_len = (uint16_t) atoi(p);
                        }
                        break;

                    case PATTERN_ITEM:
                        {
                            strim_except_space(p);
                            strncpy(pattern_str,  p, PATTERN_SIZE_MAX - 1); 
                            //printf("pattern_str[%s]\n",pattern_str);
                        }
                        break;

                    case HEX_ITEM:
                        {
                            _strim(p);
                            key.hex = !!atoi(p);

                        }
                        break;

                    case NOCASE_ITEM:
                        {
                            _strim(p);
                            key.nocase = !!atoi(p);

                        }
                        break;

                    case OFFSET_ITEM:
                        {
                            _strim(p);
                            key.offset = (int16_t)atoi(p);
                        }
                        break;

                    case DEPTH_ITEM: 
                        {
                            _strim(p);
                            key.depth = (int16_t)atoi(p);

                        }
                        break;

                    case DYNAMIC_ITEM:
                        {
                            _strim(p);
                            key.dynamic_type = (int8_t)atoi(p);

                        }
                        break;

                    case LOG_TYPE_ITEM:
                        {
                            _strim(p);
                            key.dynamic_type = (0x80)|(int8_t)atoi(p);
                            printf("key.dynamic_type=0x%x\n",key.dynamic_type);
                        }
                        break;
#if 0
                    case DYNAMIC_NEED_PHASE_ITEM: 
                        {
                            _strim(p);
                            key.dynamic_need_phase = (uint8_t) atoi(p);
                        }
                        break;

                    case DYNAMIC_CURRENT_PHASE_ITEM:
                        {
                            _strim(p);
                            key.dynamic_current_phase = (uint8_t) atoi(p);
                        }
                        break;
#endif
                    case DYNAMIC_PORT_ITEM:
                        {
                            _strim(p);
                            key.dynamic_port = htons((uint16_t) atoi(p));
                        }
                        break;

                    case DYNAMIC_DIR_ITEM:
                        {
                            _strim(p);
                            key.dynamic_dir = !!atoi(p);
                        }
                        break;
                    case DYNAMIC_PORTKEY_LEN_ITEM:
                        {
                            _strim(p);
                            key.port_key_len = (uint8_t) atoi(p);
                        }
                        break;

                    case DYNAMIC_PORTKEY_ITEM:
                        {

                            strim_except_space(p);
                            if (key.port_key_len == 0) { //in this case, port key is string
                                key.port_key_len = strlen(p);
                                if (key.port_key_len > DYNAMIC_KEYWORD_SIZE_MAX-1) {
                                    key.port_key_len = DYNAMIC_KEYWORD_SIZE_MAX-1;
                                }
                                strncpy(key.port_key, p, key.port_key_len);
                                printf("ip_key[%s]\n", key.ip_key);
                            } else {
                                key.port_key_len = sft_hstoh(p, key.port_key);

                            }
                        }
                        break;
                    case DYNAMIC_IPKEY_LEN_ITEM:
                        {
                            _strim(p);
                            key.ip_key_len = (uint8_t) atoi(p);
                        }
                        break;

                    case DYNAMIC_IPKEY_ITEM:
                        {
                                strim_except_space(p);
                            if (key.ip_key_len == 0) { //in this case, port key is string
                                key.ip_key_len = strlen(p);
                                if (key.ip_key_len > DYNAMIC_KEYWORD_SIZE_MAX-1) {
                                    key.ip_key_len = DYNAMIC_KEYWORD_SIZE_MAX-1;
                                }
                                strncpy(key.ip_key, p, key.ip_key_len);
                                printf("ip_key[%s]\n", key.ip_key);
                            } else {
                                key.ip_key_len = sft_hstoh(p, key.ip_key);

                            }
                        }
                        break;
                    case LOGIN_UID_START_KEY_LEN_ITEM:
                        {
                            _strim(p);
                            key.ip_key_len = (uint8_t) atoi(p);
                        }
                        break;

                    case LOGIN_UID_START_KEY_ITEM:
                        {
                                strim_except_space(p);
                            if (key.ip_key_len == 0) { //in this case, port key is string
                                key.ip_key_len = strlen(p);
                                if (key.ip_key_len > DYNAMIC_KEYWORD_SIZE_MAX-1) {
                                    key.ip_key_len = DYNAMIC_KEYWORD_SIZE_MAX-1;
                                }
                                strncpy(key.ip_key, p, key.ip_key_len);
                                printf("login start key[%s]\n", key.ip_key);
                            } else {
                                key.ip_key_len = sft_hstoh(p, key.ip_key);

                            }
                            if (!key.dynamic_type)
                                key.dynamic_type = 0x81;
                        }
                        break;
                    case LOGIN_UID_END_KEY_LEN_ITEM:
                        {
                            _strim(p);
                            key.port_key_len = (uint8_t) atoi(p);
                        }
                        break;

                    case LOGIN_UID_END_KEY_ITEM:
                        {
                                strim_except_space(p);
                            if (key.port_key_len == 0) { //in this case, port key is string
                                key.port_key_len = strlen(p);
                                if (key.port_key_len > DYNAMIC_KEYWORD_SIZE_MAX-1) {
                                    key.port_key_len = DYNAMIC_KEYWORD_SIZE_MAX-1;
                                }
                                strncpy(key.port_key, p, key.port_key_len);
                                printf("login end key[%s]\n", key.port_key);
                            } else {
                                key.port_key_len = sft_hstoh(p, key.port_key);

                            }
                        }
                        break;
//=====================================================================================
                    case POST_UID_START_KEY_LEN_ITEM:
                        {
                            _strim(p);
                            key.ip_key_len = (uint8_t) atoi(p);
                        }
                        break;

                    case POST_UID_START_KEY_ITEM:
                        {
                                strim_except_space(p);
                            if (key.ip_key_len == 0) { //in this case, port key is string
                                key.ip_key_len = strlen(p);
                                if (key.ip_key_len > DYNAMIC_KEYWORD_SIZE_MAX-1) {
                                    key.ip_key_len = DYNAMIC_KEYWORD_SIZE_MAX-1;
                                }
                                strncpy(key.ip_key, p, key.ip_key_len);
                                printf("login start key[%s]\n", key.ip_key);
                            } else {
                                key.ip_key_len = sft_hstoh(p, key.ip_key);

                            }
                            if (!key.dynamic_type)
                                key.dynamic_type = 0x82;
                        }
                        break;
                    case POST_UID_END_KEY_LEN_ITEM:
                        {
                            _strim(p);
                            key.port_key_len = (uint8_t) atoi(p);
                        }
                        break;

                    case POST_UID_END_KEY_ITEM:
                        {
                                strim_except_space(p);
                            if (key.port_key_len == 0) { //in this case, port key is string
                                key.port_key_len = strlen(p);
                                if (key.port_key_len > DYNAMIC_KEYWORD_SIZE_MAX-1) {
                                    key.port_key_len = DYNAMIC_KEYWORD_SIZE_MAX-1;
                                }
                                strncpy(key.port_key, p, key.port_key_len);
                                printf("login end key[%s]\n", key.port_key);
                            } else {
                                key.port_key_len = sft_hstoh(p, key.port_key);

                            }
                        }
                        break;
//=====================================================================================
                    case POST_TID_START_KEY_LEN_ITEM:
                        {
                            _strim(p);
                            key.ip_key_len = (uint8_t) atoi(p);
                        }
                        break;

                    case POST_TID_START_KEY_ITEM:
                        {
                                strim_except_space(p);
                            if (key.ip_key_len == 0) { //in this case, port key is string
                                key.ip_key_len = strlen(p);
                                if (key.ip_key_len > DYNAMIC_KEYWORD_SIZE_MAX-1) {
                                    key.ip_key_len = DYNAMIC_KEYWORD_SIZE_MAX-1;
                                }
                                strncpy(key.ip_key, p, key.ip_key_len);
                                printf("login start key[%s]\n", key.ip_key);
                            } else {
                                key.ip_key_len = sft_hstoh(p, key.ip_key);

                            }
                            if (!key.dynamic_type)
                                key.dynamic_type = 0x84;
                        }
                        break;
                    case POST_TID_END_KEY_LEN_ITEM:
                        {
                            _strim(p);
                            key.port_key_len = (uint8_t) atoi(p);
                        }
                        break;

                    case POST_TID_END_KEY_ITEM:
                        {
                                strim_except_space(p);
                            if (key.port_key_len == 0) { //in this case, port key is string
                                key.port_key_len = strlen(p);
                                if (key.port_key_len > DYNAMIC_KEYWORD_SIZE_MAX-1) {
                                    key.port_key_len = DYNAMIC_KEYWORD_SIZE_MAX-1;
                                }
                                strncpy(key.port_key, p, key.port_key_len);
                                printf("login end key[%s]\n", key.port_key);
                            } else {
                                key.port_key_len = sft_hstoh(p, key.port_key);

                            }
                        }
                        break;


                    case DYNAMIC_DOMAIN_ITEM:
                        {
                        }
                        break;

                    case DYNAMIC_MUTIL_IPLIST_ITEM:
                        {
                            _strim(p);
                            key.mult_iplist = !!atoi(p);
                        }
                        break;

                    case DYNAMIC_INDIRECT_ITEM:
                        {
                            _strim(p);
                            key.dynamic_indirect = atoi(p);
                        }
                        break;
                    case FIXED_PAYLOAD_LEN_ITEM:
                        {
                            _strim(p);
                            key.fixed_payload_len = atoi(p);
                        }
                        break;
                    case PKT_NUM_ITEM:
                        {
                            _strim(p);
                            key.pkt_num = atoi(p);
                        }
                        break;
                    case PAYLOAD_LEN_ITEM:
                        {
                            _strim(p);
                            key.payload_len = (uint16_t) atoi(p);//(uint32_t) atoi(p);//
                        }
                        break;
                    case TLV_START_ITEM:
                    {
                        key.tlv_start = (uint8_t)atoi(p);
                       // printf("tlv start[%d]\n", key.tlv_start);
                    }
                    break;
                    case TLV_LEN_ITEM:
                    {
                        key.tlv_len = (uint8_t)atoi(p);
                     //   printf("tlv len[%d]\n", key.tlv_len);
                    }
                    break;
                    case TLV_TYPE_ITEM:
                    {
                        key.tlv_type = (uint8_t)atoi(p);
                      //  printf("tlv type[%d]\n", key.tlv_type);
                    }
                    break;
                    case TLV_OFFSET_ITEM:
                    {
                        key.tlv_offset = (int32_t)atoi(p);
                 //       printf("tlv offset[%d]\n", key.tlv_offset);
                    }

                        //default:

                }
                if (retv == -1)
                    break;
            }

            if (retv == 0) {
                if (strlen(pattern_str) > 0) {
                    if (unlikely(key.pattern_len <= 0))
                        continue;

                    if (key.hex == 1) {
                        key.pattern_len = sft_hstoh(pattern_str, key.pattern);
                    } else {
                        remove_backlash(pattern_str);
                        if(key.pattern_len >= PATTERN_SIZE_MAX)
                        {
                            key.pattern_len = PATTERN_SIZE_MAX - 1;
                        }
                        strncpy(key.pattern, pattern_str, key.pattern_len);
                        //printf("pattern_str[%s]len[%d]\n", key.pattern, key.pattern_len);
                    }
                }
                
                
                if (key.dynamic_type > 0 && key.dynamic_type < 5) {

                    key.dynamic_type = 1;

                    if (key.hex == 1) {
                        key.dynamic_type = 2; //dns type
                    }

                    if (strlen(key.ip_key) > 0)
                    {
                        key.dynamic_type = 3;
                    }
                    if (strlen(key.port_key) > 0)
                    {
                        key.dynamic_type = 4;
                    }

#if 0
                    if (key.dynamic_need_phase > 0)
                    {
                        if (key.dynamic_indirect) {
                            key.dynamic_need_phase = 1;
                        } 
                        key.dynamic_need_phase = (1<<key.dynamic_need_phase) - 1;
                    } else {
                        key.dynamic_need_phase = 1;
                    }
                    if (key.dynamic_current_phase > 0)
                    {
                        if (key.dynamic_indirect) {
                            key.dynamic_need_phase = 1;
                            key.dynamic_current_phase = 1;
                        } 
                        key.dynamic_current_phase = 1<<(key.dynamic_current_phase - 1);
                    } else {
                        key.dynamic_current_phase = 1;
                    }
#endif
#if 0
                    tmp = simple_strtol(_strim(field_str[FIELD_DYNAMIC_STUDY_PORT].field_value), NULL, 0);
                    if (tmp < UINT16_MAX)
                    {
                        key.study_port = htons(tmp);
                    }
                    DEG("pattern name[%s] dynamic_dir=%d,dynamic_type=%d,dynamic_need_phase=%x,dynamic_current_phase=%x  ip_key=%s,port_key=%s,dynamic_port=%u,mult_iplist=%u\n", key.pattern_name, key.dynamic_dir, key.dynamic_type, 
                            key.dynamic_need_phase,key.dynamic_current_phase,
                            key.ip_key,key.port_key, key.dynamic_port,key.mult_iplist);
#endif
                }

             
		  //pattern_cache_tbl.h_table
                pattern_node = pattern_cache_try_get(&key, &ret_status);
                if (ret_status == 0) {
                    pattern_node->id = pattern_cnt++;
                    strncpy(pattern_node->pattern_name, pattern_name, MAX_PATTRN_LEN - 1);
                } else if (ret_status == 1){
                    repeated_pattern_id = pattern_node->id;
                    key.repeated = ++pattern_node->repeated_cnt;
                    pattern_node = pattern_cache_try_get(&key, &ret_status);
                    
                    if (ret_status == 0) {
                        pattern_node->id = repeated_pattern_id;
                        strncpy(pattern_node->pattern_name, pattern_name, MAX_PATTRN_LEN - 1);
                        printf("[%s]key.repeated=%d [%s]\n", pattern_name, key.repeated,pattern_node->pattern_key.pattern);
                    }
                } else {
                   LOG("no enought mem for pattern_node\n"); 
                    return retv; 
                }
                bzero(export_buf, 256);
                sprintf(export_buf, "[id:%u,name=%s, pkt_dir=%s, pattern=%s]\n", pattern_node->id, pattern_node->pattern_name, pattern_node->pattern_key.pkt_dir ? "req":"res", 
                pattern_node->pattern_key.pattern_len ? pattern_node->pattern_key.pattern: " ");
                export_file(PATTERN_FILE_TYPE, export_buf, flag);
                flag = 1;

//                printf("pattern=[%s]\n",pattern_node->pattern_key.pattern);
//                printf("ret_status=%d\n",ret_status);
		
		
		/*identify fixed_length_packet */
		if(1 == key.fixed_payload_len)
		{	
			bzero(&flck, sizeof(struct fixed_length_cache_key));
			
			flck.proto_type = key.proto_type;
			flck.pkt_dir = key.pkt_dir;
			flck.pkt_num = key.pkt_num;
			flck.payload_len = key.payload_len;
			if(key.dport)
			{
				flck.port = key.dport;
			}
			else if(key.sport)
			{
				flck.port =key.sport;
			}
			else
			{
				flck.port = 0;
			}
			
			flc = fixed_length_cache_try_get(&flck);
			if(flc)
			{
				flc->flcv.pattern = pattern_node;
			}
		}
	}
		
#if 0
            if ((p = strchr(toks[1], ','))) {
                *p = 0x00;
                str_trim(toks[1]);
            }
#endif
#if 0
    D("<<pattern_len=%d, offset=%d, depth=%d, sport=%u,dport=%u,pkt_dir=%d, \nproto_type=%d, hex=%d, nocase=%d,mult_iplist=%d, \nplace_token_id=%d, pkt_num=%d,pkt_len_min=%d, \npkt_len_max=%d,pattern=%s,ip_key=%s,port_key=%s,dynamic_need_phase=%d,\ndynamic_current_phase=%d,dynamic_type=%d>>\n",
                key.pattern_len, key.offset, key.depth,
                 key.sport,key.dport,key.pkt_dir, key.proto_type, 
                key.hex, key.nocase, key.mult_iplist, 
                key.place_token_id, key.pkt_num, key.pkt_len_min,
                 key.pkt_len_max, key.pattern,key.ip_key,key.port_key, 
                key.dynamic_need_phase,key.dynamic_current_phase, key.dynamic_type);
#endif
free_toks:
            memset(line, 0x00, 1024);
            mSplitFree(&toks, num_toks);
        }

        fclose(fp); 
    }
    export_file(PATTERN_FILE_TYPE, NULL, 2);


    return retv; 
}




static int build_state_table(int state_array_id,int pattern_id,int first,int endflag, uint32_t type_id, char *pattern_name)
{
    uint16_t id;
    
 //     printf("+++++state_array_id:%d,pattern_id:%d,type_id:%02x,G_state_id_inc:%d,endflag:%d,nextstate:%d.\n"
//            ,state_array_id,pattern_id,type_id,G_state_id_inc,endflag,\
              DPI_State_TBL[state_array_id].pattern[pattern_id].nextstate_id);
                  
    if(state_array_id < 0)
    {
        return -1;
    }

    if(G_state_id_inc >= DPI_STATE_TBL_MAX)
    {
            E("no room for more state node.[%d,%d]\n",G_state_id_inc,DPI_STATE_TBL_MAX);
        return -1;
    }

    if(pattern_id >= (DPI_PATTERN_MAX -1))
    {
            E("no room for more pattern node.[%d,%d]\n", pattern_id,DPI_PATTERN_MAX);
        return -1;
    }

    DPI_State_TBL[state_array_id].enable = 1;
    if(DPI_State_TBL[state_array_id].pattern[pattern_id].nextstate_id == 0)
    {
        DPI_State_TBL[state_array_id].pattern[pattern_id].nextstate_id = G_state_id_inc;
        id = G_state_id_inc;
        G_state_id_inc++;
    }
    else
    {
        id = DPI_State_TBL[state_array_id].pattern[pattern_id].nextstate_id;
    }
    if(endflag == 1)
    {
        DPI_State_Flag_TBL[id] |= 0x1;
        DPI_State_TBL[id].type_id = type_id;
    //    if (strcmp(pattern_name, "content_type_app_octet_stream") == 0)
    //         printf("%s(%d),state_array_id:%d is final state,type_id:%02x.[%s]\n",__FUNCTION__,__LINE__,id,type_id, pattern_name);
    }
    if(first)
    {
        DPI_State_Flag_TBL[id] |= 0x2;
    }

    return id;
}

static int  __load_rule(char *rule, char *pattern_list, uint8_t level)
{
    int retv = 0;
    int state_array_id = 0;
    int endflag = 0;
    char *p;
    pattern_t *pattern = NULL;
    char **toks;
    int num_toks;
    uint32_t target_id = 0;
    int i;
     
    app_class_t *app_class = get_app_class_by_name(rule);
    if (app_class) {
        app_class->type_id &= 0xffff0000;
        app_class->type_id |= (level & 0x3);
        //target_id =  app_class->type_id;
        toks = mSplit(pattern_list, ">", 12, &num_toks, 0);
        if(unlikely(num_toks == 0)) {
            retv = -1; 
            E("no pattern list\n");
           goto out;
        }
        for (i = 0; i < num_toks; i++) {
            str_trim(toks[i]);
            pattern = get_pattern_by_name(toks[i]);
            if (!pattern) {
                E("invalid pattern[%s]\n", toks[i]);
                goto out;
            }

            if(pattern->pattern_key.pattern_len == 0 && pattern->pattern_key.fixed_payload_len == 0)//tuple
            {
                if(pattern->pattern_key.pkt_dir == 1)//req
                {
                    if(pattern->pattern_key.proto_type == DPI_PROTO_TCP)
                    {
                        if(pattern->pattern_key.dport)
                        {
                            tcp_server_port[pattern->pattern_key.dport] = app_class->type_id;
                        }

                        if(pattern->pattern_key.sport)
                        {
                            tcp_client_port[pattern->pattern_key.sport] = app_class->type_id;
                        }
                    }
                    else if(pattern->pattern_key.proto_type == DPI_PROTO_UDP)
                    {
                        if(pattern->pattern_key.dport)
                        {
                            udp_server_port[pattern->pattern_key.dport] = app_class->type_id;
                        }

                        if(pattern->pattern_key.sport)
                        {
                            udp_client_port[pattern->pattern_key.sport] = app_class->type_id;
                        }
                    }
                }
                else//res
                {
                    if(pattern->pattern_key.proto_type == DPI_PROTO_TCP)
                    {
                        if(pattern->pattern_key.dport)
                        {
                            tcp_client_port[pattern->pattern_key.dport] = app_class->type_id;
                        }

                        if(pattern->pattern_key.sport)
                        {
                            tcp_server_port[pattern->pattern_key.sport] = app_class->type_id;
                        }
                    }
                    else if(pattern->pattern_key.proto_type == DPI_PROTO_UDP)
                    {
                        if(pattern->pattern_key.dport)
                        {
                            udp_client_port[pattern->pattern_key.dport] = app_class->type_id;
                        }

                        if(pattern->pattern_key.sport)
                        {
                            udp_server_port[pattern->pattern_key.sport] = app_class->type_id;
                        }
                    }
                }
            }    
            if(i == 0|| pattern->pattern_key.dynamic_indirect)
                pattern->pattern_key.first = 1;
            if(i == (num_toks - 1) || pattern->pattern_key.dynamic_indirect)
                endflag = 1;
            state_array_id = build_state_table(state_array_id,pattern->id,pattern->pattern_key.first,endflag, app_class->type_id, pattern->pattern_name);

        }
out:
    mSplitFree(&toks, num_toks);

    }
    return retv;
}
static int load_rules()
{
    char rules_path[128] = {0};
    FILE *fp;
    char line[1024];
    char *p;
    char **toks;
    uint8_t level = 0;
    int num_toks;
    struct conf_node *node = NULL;
    int retv = -1;
    char *rule = NULL;
    char *pattern_list = NULL;    
    
    G_state_id_inc = 1;
    D("------load rules-------------\n");
    list_for_each_entry(node, &pv.dpi_conf[RULE_TYPE].conf_head, list) {

        strncpy(rules_path, pv.key_lib_path, 128);
        strncat(rules_path, node->conf_name, 128 - strlen(node->conf_name));
        printf("rules_path[%s]\n", rules_path);
        if ((fp = fopen(rules_path, "rb")) == NULL) {
            E("-------open rule file [%s] fail.\n", rules_path);
            continue;
        }
        while(fgets(line, 1024, fp))  {
            if (*line == '#') {
                memset(line, 0x00, 1024);
                continue;
            }
            trim_specific(line, "{}");
            toks = mSplit(line, ",", 3, &num_toks, 0);
            if(unlikely(num_toks < 2))
                goto free_toks;
            //parse type 
            if ((p = strchr(toks[0], '='))) {
                *p = 0x00;
                 p++;
                _strim(p);
                rule = p;
            } else {
                goto free_toks;
            }
            //parse type 
            if ((p = strchr(toks[1], '='))) {
                *p = 0x00;
                p++;
                _strim(p);
                pattern_list = p;
            }
            else {
                    goto free_toks;
            }

            if (num_toks == 3) {
                level = atoi(str_trim(toks[2]));
            } else if (num_toks == 2) {
                level = 1;
            }
//            printf("target=[%s] pattern_list=[%s]\n", rule, pattern_list);
            __load_rule(rule, pattern_list, level);
free_toks:
            memset(line, 0x00, 1024);
            mSplitFree(&toks, num_toks);
        }

        fclose(fp); 
    }


    return retv;
}

void rules_clear()
{
    bzero(DPI_State_TBL, sizeof(struct state_array)*DPI_STATE_TBL_MAX);
    bzero(DPI_State_Flag_TBL, sizeof(int)*DPI_STATE_TBL_MAX );
    bzero(user_tcp_proto, sizeof(struct dpi_user_proto) *(0XFFFF+1));
    bzero(user_udp_proto, sizeof(struct dpi_user_proto) *(0XFFFF+1));
    bzero(user_tcp_proto_node, sizeof(struct dpi_user_proto_node) *(0XFFFF+1) * (SAMEPORT_IPNUM));
    bzero(user_udp_proto_node, sizeof(struct dpi_user_proto_node) * (0XFFFF+1) * (SAMEPORT_IPNUM));
    bzero(tcp_server_port, 4 * (0XFFFF+1));
    bzero(tcp_client_port, 4 * (0XFFFF+1));
    bzero(udp_server_port, 4 * (0XFFFF+1));
    bzero(udp_client_port, 4 * (0XFFFF+1));

}
static int dpi_state_tbl_init()
{
    DPI_State_TBL = rte_zmalloc(NULL, sizeof(struct state_array)*DPI_STATE_TBL_MAX, 0);
    if (unlikely(!DPI_State_TBL)) {
        E("malloc dpi_state_tbl fail\n");
        return -1;
    }
    DPI_State_Flag_TBL = rte_zmalloc(NULL, sizeof(int)*DPI_STATE_TBL_MAX, 0);
    if (unlikely(!DPI_State_Flag_TBL)) {
        E("malloc DPI_State_Flag_TBLl fail\n");
        return -1;
    }

    printf("dpi_state_tbl size is %u byte\n", sizeof(struct state_array)*DPI_STATE_TBL_MAX);

    return 0;
}
#if 1
static int load_proto_nodemgt()
{
    char proto_nodemg_file[1024] = {0};
    FILE *fp;
    char line[512];
    uint32_t type_id;
    uint16_t port;
    char **toks;
    int num_toks;
    int retv = -1;
    int i;
    
    strncpy(proto_nodemg_file, pv.conf_path, 1024);
    strncat(proto_nodemg_file, "proto_nodemgt.conf", 1024 - strlen(pv.conf_path));
    if ((fp = fopen(proto_nodemg_file, "rb")) == NULL) {
        E("-------open %s fail.\n", proto_nodemg_file);
        return -1;
    }
    while(fgets(line, 512, fp))  {
        if (*line == '#')
            continue;
        toks = mSplit(line, "|", 5, &num_toks, 0);
        if(num_toks != 5) {
            E("invaild proto_nodemgt line\n");
            goto free_toks;
        }

        type_id = get_app_class_id_by_name(str_trim(toks[0]));     
        if (unlikely(type_id == -1)) {
            E("app_class_user.sft has no [%s]\n", toks[0]);
            goto free_toks;
        } 
        str_trim(toks[1]); 
        port = port_stons(str_trim(toks[2]));
        if (!strncmp(str_trim(toks[3]), "yes", 3)) { //user tcp app class

            for(i=0;i < SAMEPORT_IPNUM;i++)
            {
                if(user_tcp_proto_node[port][i].ip == 0)
                {
                    user_tcp_proto_node[port][i].ip = ipv4_stonl(toks[1]);
                    user_tcp_proto_node[port][i].type_id = type_id;
                    strncpy(user_tcp_proto_node[port][i].name, toks[0], 31);
                    printf("name[%s]type id[%x]ip[%s]\n",toks[0], type_id, toks[1]);
                    break;
                }
            }
            if(i >= SAMEPORT_IPNUM) {
                E("Invaild proto_nodemgt[%s]\n", line);
                goto free_toks;
            }
        } 

        if (!strncmp(str_trim(toks[4]), "yes", 3)) { //user tcp app class
 
            for(i=0;i < SAMEPORT_IPNUM;i++)
            {
                if(user_udp_proto_node[port][i].ip == 0)
                {
                    user_udp_proto_node[port][i].ip = ipv4_stonl(toks[1]);
                    user_udp_proto_node[port][i].type_id = type_id;
                    strncpy(user_udp_proto_node[port][i].name, toks[0], 31);
                    printf("name[%s]type id[%x]ip[%s]\n",toks[0], type_id, toks[1]);
                    break;
                }
            }
            if(i >= SAMEPORT_IPNUM) {
                E("Invaild proto_nodemgt[%s]\n", line);
                goto free_toks;
            }
        } 

free_toks:
        memset(line, 0x00, 512);
        mSplitFree(&toks, num_toks);
    }
    fclose(fp);

    return 0;
}
#endif

#if 1
/*	read from configure file, preload the constant node 
  *	configuration format: 
  *          name | ip_flag   | min_ip |max_ip |ip_step|ip_step_length| port_flag|min port|max port| port_step |port_step_length| study flag
  *    1.	name | ip_range | min xx | max xx|ip_step|  xx               | port_range    |min xx  | max xx  | port_step |       xx              |  0/1
  *    2.	name | ip_range | min_xx |max xx| ip_step|  xx              | port_exact     |min xx  | max xx  | port_step |       xx              |  0/1
  *    3.	name | ip_exact | min xx | max xx|ip_step |  xx              | port_range    |min xx  | max xx  | port_step |       xx              |  0/1
  *    4.	name | ip_exact | min_xx |max xx| ip_step|  xx              | port_exact     |min xx  | max xx  | port_step |       xx              |  0/1
  *  tokens   0           1          2             3          4           5                   6                   7           8               9                  10                 11
  *  Note: step length should be > 0
  */ 
static int load_proto_constant_nodemgt()
{
	char proto_nodemg_file[1024] = {0};
	FILE *fp = NULL;
	char line[512];
	uint32_t type_id = 0;
	uint16_t port = 0;
	char **toks = NULL;
	int num_toks = 0;
	int retv = -1;
	int i = 0;
	char tokens_all_num = 12;
	uint16_t min_port = 0;
	uint16_t max_port = 0;
	uint16_t port_step_length = 0;
	uint8_t study_flag = 0;
	
	uint32_t ip_network_addr_num = 0;
	int ip_addr_last_num = 0;
	int ip_addr_step_length = 0;
	int ip_min_addr_num[4] = {0};
	int ip_max_addr_num[4] = {0};
	char *ip_min_addr_str = NULL;
	char *ip_max_addr_str = NULL;
	

	strncpy(proto_nodemg_file, pv.conf_path, 1024);
	strncat(proto_nodemg_file, "load_constant_node_mgt.conf", 1024 - strlen(pv.conf_path));
	if ((fp = fopen(proto_nodemg_file, "rb")) == NULL) 
	{
		E("-------open %s fail.\n", proto_nodemg_file);
		return -1;
	}
	
	while(fgets(line, 512, fp))  {
		if (*line == '#')
			continue;
		
		toks = mSplit(line, "|", tokens_all_num, &num_toks, 0);

		if(num_toks != tokens_all_num) 
		{
		//	E("invaild proto_nodemgt line\n");
			goto free_toks;
		}

		type_id = get_app_class_id_by_name(str_trim(toks[0]));    //tokens[0] = name 
		if (unlikely(type_id == -1)) {
		    E("app_class_user.sft has no [%s]\n", toks[0]);
		    goto free_toks;
		} 
		
		study_flag = atoi(str_trim(toks[11])) == 0? 0 : 1; //tokens[11] = study flag

		//ip_range
		if (!strncmp(str_trim(toks[1]), "ip_range", 8)) //tokens[1] = ip_flag [ip_range | ip_exact]
		{
			ip_min_addr_str = 	str_trim(toks[2]); //tokens[2] = min_ip
			if(sscanf(ip_min_addr_str, "%d.%d.%d.%d", &ip_min_addr_num[0], &ip_min_addr_num[1], &ip_min_addr_num[2], &ip_min_addr_num[3]) != 4)
			{
				E("Invaild min_ip in proto_nodemgt[%s]\n", line);
				goto free_toks;
			}
			//printf("ip_min_addr_str =  %d.%d.%d.%d\n", ip_min_addr_num[0], ip_min_addr_num[1], ip_min_addr_num[2], ip_min_addr_num[3]);
			
			ip_max_addr_str = str_trim(toks[3]);//tokens[3] = max_ip
			if(sscanf(ip_max_addr_str, "%d.%d.%d.%d", &ip_max_addr_num[0], &ip_max_addr_num[1], &ip_max_addr_num[2], &ip_max_addr_num[3]) != 4)
			{
				E("Invaild max_ip in proto_nodemgt[%s]\n", line);
				goto free_toks;
			}
			//printf("ip_max_addr_str =  %d.%d.%d.%d\n", ip_max_addr_num[0], ip_max_addr_num[1], ip_max_addr_num[2], ip_max_addr_num[3]);

			if ( !(ip_min_addr_num[0] == ip_max_addr_num[0] && ip_min_addr_num[1] == ip_max_addr_num[1] &&
			      ip_min_addr_num[2] == ip_max_addr_num[2] && ip_min_addr_num[3] <= ip_max_addr_num[3]))
			{
				E("Invaild min_ip should <= max_ip in proto_nodemgt[%s]\n", line);
				goto free_toks;
			}
			
			ip_addr_step_length = atoi(str_trim(toks[5])); //tokens[5] = ip_step_length
			if (0 != strncmp(str_trim(toks[4]), "ip_step", 7) || ip_addr_step_length < 0)//tokens[4] = ip_step
			{
				E("Invaild ip_step in proto_nodemgt[%s]\n", line);
				goto free_toks;
			}
			
			study_flag = atoi(str_trim(toks[11])); //tokens[11] = study flag

			for (ip_addr_last_num = ip_min_addr_num[3]; ip_addr_last_num <= ip_max_addr_num[3]; ip_addr_last_num += ip_addr_step_length)
			{
				ip_network_addr_num = (((__u32)ip_min_addr_num[0] & 0xff)) | 
					                             (((__u32)ip_min_addr_num[1] & 0xff) << 8) |
					                             (((__u32)ip_min_addr_num[2] & 0xff) << 16) | 
					                             (((__u32)ip_addr_last_num & 0xff) << 24);


				//port_range
				if (!strncmp(str_trim(toks[6]), "port_range", 10)) //tokens[6] = port_flag [port_range | port_exact]
				{
					min_port =atoi(str_trim(toks[7]));//tokens[7] = min port 
					max_port = atoi(str_trim(toks[8]));//tokens[8] = max port = port 

					if(min_port > max_port)
					{
						E("Invaild min port should be > max port in proto_nodemgt[%s]\n", line);
						goto free_toks;
					}
					
					port_step_length = atoi(str_trim(toks[10]));//tokens[10] = step length >= 0 
					if (0 != strncmp(str_trim(toks[9]), "port_step", 9) || port_step_length < 0) //tokens[9] = "port step"
					{
						E("Invaild port_step in proto_nodemgt[%s]\n", line);
						goto free_toks;
					}
					
					for(port = min_port; port <= max_port; port += port_step_length)
					{	       
						if(0 == study_flag)
						{
							study_cache_try_get(ip_network_addr_num, htons(port), type_id, 1);
						}
						else
						{
							dynamic_cache_try_get(ip_network_addr_num, htons(port), type_id, 1);
						}
					}
				}
				//port_exact
				else if (!strncmp(str_trim(toks[6]), "port_exact", 10)) //tokens[6] = port_flag [port_range | port_exact]
				{
					port = port_stons(str_trim(toks[7]));//tokens[7] = min port = port, ignore tokens[8] = max port
					if(0 == study_flag)
					{
						study_cache_try_get(ip_network_addr_num, port, type_id, 1);
					}
					else
					{
						dynamic_cache_try_get(ip_network_addr_num, port, type_id, 1);
					}
				}
				else
				{
					E("Invaild port flag in proto_nodemgt[%s]\n", line);
					goto free_toks;
				}
			}
			
		}
		//ip_exact
		else if (!strncmp(str_trim(toks[1]), "ip_exact", 8))//tokens[1] = ip_flag [ip_range | ip_exact]
		{
			str_trim(toks[2]);//tokens[2] = min_ip
			ip_network_addr_num = ipv4_stonl(toks[2]);

			//port_range
		       if (!strncmp(str_trim(toks[6]), "port_range", 10)) //tokens[6] = port flag [port_range | port_exact]
			{
				min_port =atoi(str_trim(toks[7]));//tokens[7] = min port 
				max_port = atoi(str_trim(toks[8]));//tokens[8] = max port = port 

				if(min_port > max_port)
				{
					E("Invaild min port should be > max port in proto_nodemgt[%s]\n", line);
					goto free_toks;
				}
				
				port_step_length = atoi(str_trim(toks[10]));//tokens[10] = step length >= 0 
				if (0 != strncmp(str_trim(toks[9]), "port_step", 9) || port_step_length < 0) //tokens[9] = "port step"
				{
					E("Invaild port_step in proto_nodemgt[%s]\n", line);
					goto free_toks;
				}
					
				for(port = min_port; port <= max_port; port += port_step_length)
				{	       
					if(0 == study_flag)
					{
						study_cache_try_get(ip_network_addr_num, htons(port), type_id, 1);
					}
					else
					{
						dynamic_cache_try_get(ip_network_addr_num, htons(port), type_id, 1);
					}
				}
			}
			//port_exact
			else if (!strncmp(str_trim(toks[6]), "port_exact", 10)) //tokens[6] = port flag [port_range | port_exact]
			{
				port = port_stons(str_trim(toks[7]));//tokens[7] = min port = port, ignore tokens[8] = max port
				if(0 == study_flag)
				{
					study_cache_try_get(ip_network_addr_num, port, type_id, 1);
				}
				else
				{
					dynamic_cache_try_get(ip_network_addr_num, port, type_id, 1);
				}
			}	
			else
			{
				E("Invaild port flag in proto_nodemgt[%s]\n", line);
				goto free_toks;
			}
		}
		else
		{
			E("Invaild ip flag in proto_nodemgt[%s]\n", line);
			goto free_toks;
		}
	free_toks:
	    memset(line, 0x00, 512);
	    mSplitFree(&toks, num_toks);
	}
	
	fclose(fp);

	return 0;
}
#endif

static int load_user_proto()
{
    char user_proto_file[64] = {0};
    FILE *fp;
    char line[512];
    int32_t type_id;
    uint16_t port;
    char **toks;
    char *p;
    int num_toks;
    int retv = -1;
    int i;
    
    strncpy(user_proto_file, pv.conf_path, 64);
    strncat(user_proto_file, "proto_useradd.conf", 64 - strlen(pv.conf_path));
    if ((fp = fopen(user_proto_file, "rb")) == NULL) {
        E("-------open proto_useradd.conf fail.\n");
        return -1;
    }
    while(fgets(line, 512, fp))  {
        if (*line == '#')
            continue;
        toks = mSplit(line, "|", 5, &num_toks, 0);
        if(num_toks < 4) {
            E("invaild proto_nodemgt line\n");
            goto free_toks;
        }

        type_id = get_app_class_id_by_name(str_trim(toks[2]));     
        if (unlikely(type_id == -1)) {
            E("app_class_user.sft has no [%s][%d]\n", toks[2], type_id);
            goto free_toks;
        } 

        str_trim(toks[3]);
        if ((p = strchr(toks[3], '_'))) {
            *p = 0x00;
            str_trim(toks[1]);
            p++;
        } else {
            continue;
        }



        
        port = port_stons(str_trim(p));
        if (!strncmp(str_trim(toks[3]), "tcp", 3)) { //user tcp app class
            user_tcp_proto[port].type_id = type_id;
            strncpy(user_tcp_proto[port].name, toks[2], 31);
        } else if (!strncmp(str_trim(toks[3]), "udp", 3)) { //user tcp app class
            user_udp_proto[port].type_id = type_id;
            strncpy(user_udp_proto[port].name, toks[2], 31);
        } 

        printf("user name[%s]type_id[%x]port[%u]\n", toks[2], type_id, port);
free_toks:
        memset(line, 0x00, 512);
        mSplitFree(&toks, num_toks);
    }
    fclose(fp);

    return 0;
}

static inline void __rules_reload(int rules_type)
{

	switch (rules_type) {
		case PROTO_NODEMGT_RULE:
			{

				break;
			}
		case PROTO_USERADD_RULE:
			{

				break;
			}
		case ALL_RULE_TYPE:
			{   
				parse_log_conf();
            
                time_t cur_modify_time = get_last_conf_modify_time();
                struct tm *lt = localtime(&cur_modify_time);
                char nowtime[24];
                memset(nowtime, 0, sizeof(nowtime));
                strftime(nowtime, 24, "%Y-%m-%d %H:%M:%S", lt);
                E("nowtime = %s\n", nowtime);

                if (cur_modify_time <= pre_modify_time) {
                    break;
                }
                pre_modify_time = cur_modify_time;

                cleanup_acsm();
				app_class_clear();
				fixed_length_cache_clear();
				study_cache_static_clear();
				dynamic_cache_static_clear();
				pattern_clear(); 
				rules_clear();

				load_app_class();
				load_place();
				load_pattern();
				load_rules();
				load_user_proto();
				if (unlikely(load_proto_nodemgt() == -1)) {
					E("load_proto_nodemgt fail\n");
				}
				if (unlikely(load_proto_constant_nodemgt() == -1)) {
					E("load_proto_constant_nodemgt fail\n");
				}
				
				init_acsm();
				
				pv.dpi_ctrl = DPI_ENABLE;
				break;
			}
        case INVALID_RULE_TYPE:
            break;
		default:
			break;
	}


}


static void reload_timer_func(struct rte_timer *tim, void *data)
{
	uint64_t hz = rte_get_timer_hz();
	unsigned lcore_id = rte_lcore_id();
	uint64_t cur_time = rte_get_timer_cycles();


    if (pv.reload) 
    {
        pv.dpi_ctrl = DPI_DISABLE;
        __rules_reload(ALL_RULE_TYPE);
        if (test_flags_dpi_enable()) { 
            pv.dpi_ctrl = DPI_ENABLE;
        } else {
            pv.dpi_ctrl = DPI_DISABLE;
        }
        pv.reload = 0;

        init_dump_pcaps();
    }
	
    int ret = rte_timer_reset(&timer, 5*hz, SINGLE, lcore_id,
				reload_timer_func, NULL);
}

int start_reload_timer() 
{
        uint64_t hz = rte_get_timer_hz();
        unsigned lcore_id = rte_lcore_id();
        rte_timer_init(&timer);
		rte_timer_reset_sync(&timer, 5*hz, SINGLE, lcore_id,
				reload_timer_func, NULL);
}
static int reload_rules_type = 0;
int rules_reload(int rules_type) 
{
	pv.reload = 1;
	
	pattern_cnt = TOKEN_MAX;
	G_state_id_inc = 1;
	G_place_token_id = 0;
	
        return 0;

}

static void get_comm_app_id()
{
	pv.http_proto_appid = get_app_class_id_by_name("http_proto");
    app_class_t *app_class = get_app_class_by_name("http_proto");
    if (app_class) {
        app_class->type_id |= 0x3;;
    }
	pv.qq_login_appid = get_app_class_id_by_name("qq_login");
    pv.weibo_appid = get_app_class_id_by_name("sinaweibo");
    pv.tieba_appid = get_app_class_id_by_name("baidutieba");
	pv.ftp_proto_appid = get_app_class_id_by_name("ftp");    
	
	pv.connectionless_tcp_appid = get_app_class_id_by_name("connectionless_tcp"); 
	pv.unknown_80_appid = get_app_class_id_by_name("unknown_80")|0x1;    
	pv.udp_application_mutual  = get_app_class_id_by_name("udp_application_mutual") | 0x1; 
	pv.udp_video_download = get_app_class_id_by_name("udp_video_download")|0x1; 
	pv.tcp_application_mutual  = get_app_class_id_by_name("tcp_application_mutual")|0x1; 
	pv.tcp_video_download = get_app_class_id_by_name("tcp_video_download")|0x1; 
	pv.bt_download = get_app_class_id_by_name("bt_download"); 
	pv.syn_ack = get_app_class_id_by_name("syn_ack"); 
	pv.original = get_app_class_id_by_name("original"); 
}
int parse_dpi_conf()
{
    if (init_dpi_conf() == -1)
        return -1;
    app_class_cache_init();
    pattern_cache_init();
    fixed_length_cache_init();
    shared_user_cache_init();
    init_dynamic_protocol();
    if (unlikely(dpi_state_tbl_init()))
        return -1;
    pv.dpi_ctrl = DPI_DISABLE;
    __rules_reload(ALL_RULE_TYPE); 
    init_dpi_statistics();
    get_comm_app_id();  
    if (test_flags_dpi_enable()) 
        pv.dpi_ctrl = DPI_ENABLE;
    else 
        pv.dpi_ctrl = DPI_DISABLE;

#if 0
    load_app_class(); 
    load_place(); 
    load_pattern();
    init_acsm();
    load_rules();
#endif
    return 0;
}



