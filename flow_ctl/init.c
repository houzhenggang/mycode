#include <string.h>
#include <syslog.h>
#include <rte_cycles.h>
#include"init.h"
#include "rules.h"
#include "global.h"
#include "debug.h"
#include "utils.h"
#include "h_cache.h"
#include "mstring.h"
#include "timer.h"
//#include "dpi_log.h"

#define KEY_LIB_PATH "/opt/utc/keylib/"
#define CONF_PATH "/opt/utc/conf/"
#define SFT_DPI_PATH "/dev/shm/sft_dpi"
PV pv;

const static char *file_type[MAX_DPI_FILE_TYPE] = {
    "app_class", "place", "pattern", "rule"
};

void init_resouce_for_each_core(uint32_t lcore_id)
{
//   init_dpi_log_meme(lcore_id); 

}
int parse_log_conf()
{
    char syslog_path[128] = {0};
    FILE *fp;
    char line[1024];
    char *remote_server = NULL;
    uint16_t remote_port = 0;
    char *p;
    char **toks;
    int num_toks;


    strncpy(syslog_path, pv.conf_path, 128);
    strcat(syslog_path, "/sft_syslog.conf");

    if ((fp = fopen(syslog_path, "rb")) == NULL) {
        E("-------open log config file [%s] fail.\n", pv.conf_path);
        return -1;
    }
    while(fgets(line, 1024, fp))  {
        if (*line == '#')
            continue;
        toks = mSplit(line, "=", 2, &num_toks, 0);
        if(unlikely(num_toks != 2))
            goto free_toks;
        _strim(toks[0]);
        trim_specific(toks[1], "\"");
        printf("[%s]--[%s]\n",toks[0], toks[1]);
        if (strncmp(toks[0], "enableSYSLOG", strlen("enableSYSLOG")) == 0) {
            if (strncmp(toks[1], "1", 1) == 0) {
                pv.urlog = 1;
                pv.qqlog = 1;
                D("syslog is enabled!\n");
            } else {

                pv.urlog = 0;
                pv.qqlog = 0;
                D("syslog is disabled!\n");
            }
        } else if (strncmp(toks[0], "remote_server", strlen("remote_server")) == 0 && is_ipv4_addr(toks[1])) {
            remote_server = strdup(toks[1]);
        } else if (strncmp(toks[0], "remote_port", strlen("remote_port")) == 0 && atoi(toks[1]) > 0) {
            remote_port = (uint16_t) atoi(toks[1]);
        }

free_toks:
        memset(line, 0x00, 1024);
        mSplitFree(&toks, num_toks);
    }
    fclose(fp);

        printf("peer addr is %s:%u\n",remote_server, remote_port);
    if (pv.urlog && pv.qqlog  && remote_server && remote_port) {
        if (pv.syslog_sd) 
            close(pv.syslog_sd);
        pv.syslog_sd = socket(PF_INET, SOCK_DGRAM, 0);
        if (pv.syslog_sd<0) {
            perror("socket()");
            exit(1);
        }

        pv.syslog_peer_addr.sin_family = AF_INET;
        pv.syslog_peer_addr.sin_port = htons(remote_port);
        inet_pton(AF_INET, remote_server, &pv.syslog_peer_addr.sin_addr);
        D("connect remote syslog server\n");
    }

    return 0;
}
int parse_ipc_conf()
{
    char ipc_path[128] = {0};
    FILE *fp;
    char line[1024];
    char *p;
    char **toks;
    int num_toks;


    strncpy(ipc_path, pv.conf_path, 128);
    strcat(ipc_path, "/sft_ipc.conf");

    if ((fp = fopen(ipc_path, "rb")) == NULL) {
        E("-------open log config file [%s] fail.\n", pv.conf_path);
        return -1;
    }
    while(fgets(line, 1024, fp))  {
        if (*line == '#')
            continue;
        toks = mSplit(line, "=", 2, &num_toks, 0);
        if(unlikely(num_toks != 2))
            goto free_toks;
        _strim(toks[0]);
        trim_specific(toks[1], "\"");
        printf("[%s]--[%s]\n",toks[0], toks[1]);
        if (strncmp(toks[0], "remote_server", strlen("remote_server")) == 0 && is_ipv4_addr(toks[1])) {
            pv.peer_ip = strdup(toks[1]);
        } else if (strncmp(toks[0], "remote_port", strlen("remote_port")) == 0 && atoi(toks[1]) > 0) {
            pv.peer_port = (uint16_t) atoi(toks[1]);
        }

free_toks:
        memset(line, 0x00, 1024);
        mSplitFree(&toks, num_toks);
    }
    fclose(fp);

    printf("peer addr is %s:%u\n",pv.peer_ip, pv.peer_port);
    return 0;
}
static void
my_obj_init(struct rte_mempool *mp, __attribute__((unused)) void *arg,
        void *obj, unsigned i)
{
    uint32_t *objnum = obj;
    memset(obj, 0, mp->elt_size);
    *objnum = i;
}
static void my_mp_init(struct rte_mempool * mp, __attribute__((unused)) void * arg)
{
    printf("mempool name is %s\n", mp->name);
    /* nothing to be implemented here*/
    return ;
}


#define HASH_QUEUE_SIZE    (1 << 12)
static void init_pv()
{
    int i;
    
    bzero(&pv, sizeof(PV));
    pv.daemon_flag = 1; 
    pv.hz = rte_get_timer_hz();
    if (test_flags_dpi_enable()) 
        pv.dpi_ctrl = DPI_ENABLE; 

    
    init_timers();
    for (i = 0; i < MAX_DPI_FILE_TYPE; i ++) {
        pv.dpi_conf[i].conf_type_name = strdup(file_type[i]);
        INIT_LIST_HEAD(&pv.dpi_conf[i].conf_head);
    }
#if 0
    if ((pv.hash_cache = rte_mempool_create("hash_men_cache", HASH_QUEUE_SIZE,
                    sizeof(void *),
                    32, 0,
                    my_mp_init, NULL,
                    my_obj_init, NULL,
                    SOCKET_ID_ANY,  0)) == NULL)
    {
        E("no enough mem for hash_men_cache\n");
        exit(1);
    }
#endif
#if 0
    for (i = 0; i < rte_lcore_count(); i++) {
        
        pv.hash_do_queue[i] = kfifo_alloc(HASH_QUEUE_SIZE, NULL);
        if (pv.hash_do_queue[i] == NULL) {
            E("no enough mem for hash_men_cache\n");
            exit(1);
        }
        rte_timer_init(&pv.queue_timer[i]);
        rte_timer_reset(&pv.queue_timer[i], 1*pv.hz, PERIODICAL, i,
                queue_timer_func, NULL);
    }
#endif

#if 0
    INIT_LIST_HEAD(&pv.dip_conf.app_class_file_head);
    INIT_LIST_HEAD(&pv.dip_conf.place_file_head);
    INIT_LIST_HEAD(&pv.dip_conf.pattern_file_head);
    INIT_LIST_HEAD(&pv.dip_conf.rule_file_head);
#endif
    if(get_keylib_path())
    {
        pv.key_lib_path = strdup(get_keylib_path());
    }
    else
    {
        pv.key_lib_path = strdup(KEY_LIB_PATH);
    }
    if(get_conf_path())
    {
        pv.conf_path = strdup(get_conf_path());
    }
    else
    {
        pv.conf_path = strdup(CONF_PATH);
    }
    
    pv.sft_dpi_path = strdup(SFT_DPI_PATH);

    pv.need_web_flag = (1<< rte_lcore_count()) -1;
}
int init_sys()
{
    init_pv();
    parse_log_conf();
    parse_dpi_conf();
    parse_ipc_conf();
}

#if 1
int init_connect_mode_conf()
{
    char conf_filename[128] = {0};
    FILE *fp;
    char line[1024];
    char *remote_server = NULL;
    uint16_t remote_port = 0;
    char *p;
    char **toks;
    int num_toks;
    int connect_mode = 0;


    strncpy(conf_filename, "/opt/utc/conf/connect_mode.conf", 128);

    if ((fp = fopen(conf_filename, "rb")) == NULL) {
        E("-------open log config file [%s] fail.\n", conf_filename);
        return -1;
    }
    while(fgets(line, 1024, fp))  
    {
        if (*line == '#')
            continue;
        
        toks = mSplit(line, "=", 2, &num_toks, 0);
        if(unlikely(num_toks != 2))
            goto free_toks;
        
        _strim(toks[0]);
        trim_specific(toks[1], "\"");
        printf("[%s]--[%s]\n",toks[0], toks[1]);
        
        if (strncmp(toks[0], "connect_mode", strlen("connect_mode")) == 0) 
        {
            if (strncmp(toks[1], "0", 1) == 0) 
            {
                printf("The current connect mode is Access Mode!\n");
                connect_mode = 0;
            }
            else  if (strncmp(toks[1], "1", 1) == 0) 
            {
                printf("The current connect mode is Trunk Mode!\n");
                connect_mode = 1;
            }
            else
            {
                printf("Failed to configure connect_mode value!\n");
                connect_mode = -1;
            }
        } 

free_toks:
        memset(line, 0x00, 1024);
        mSplitFree(&toks, num_toks);
    }
    fclose(fp);

    return connect_mode;
}
#endif
