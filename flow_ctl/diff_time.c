#include    <sys/types.h>
#include    <sys/ipc.h>
#include    <sys/shm.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include    <stdio.h>
#include    <stdint.h>
#include    <error.h>
#include    "mstring.h"
#include    "debug.h"
#include    "log.h"

#define SHM_SIZE    4096
#define SHM_MODE    (SHM_R | SHM_W | IPC_CREAT) /* user read/write */
#define STAT_SHM_KEY 0x99
typedef struct {
    uint64_t diff_tsc[8];
    uint64_t arg_diff_tsc[8];
    uint64_t skb_num[8];
    uint64_t max_arg_diff_tsc[8];
    uint64_t total_tsc[8];
    uint64_t total_num[8];
    uint32_t hash_level_water_mark;
    uint32_t exceed_hash_level_count;
    uint32_t max_hash_level;
    uint64_t fail_skb;
    uint64_t exceed_high_water_mark_count;
    uint64_t nums_sft_fip_entry;
    uint64_t rte_timer_tsc_per_sec[8];
    uint64_t send_pkt_tsc_per_sec[8];
    uint8_t web_mesg_flag;
    uint64_t cache_pkg_num;
    uint64_t failed_recv_pkg[8];
    int web_msgid;
}skbstat_t;

int __ch_isspace(char c)
{
    return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}
static inline void _strim(char *str)
{
    char *p1, *p2, c;
    size_t len;

    if((len = strlen(str)) == 0)
        return;

    /* Determine start position. */
    for(p1 = str; (c = *p1); p1++)
    {
        if(!__ch_isspace(c))
            break;
    }

    /* Determine ending position. */
    for(p2 = str + len; p2 > p1 && (c = *(p2 - 1)); p2--)
    {
        if(!__ch_isspace(c))
            break;
    }

    /* Move string ahead, and put new terminal character. */
    memmove(str, p1, (size_t)(p2 - p1));
    str[(size_t)(p2 - p1)] = '\0';
}
uint64_t high_water_mark = 0;
uint32_t hash_level_water_mark = 0;
int parse_conf()
{
    FILE *fp;
    char line[512];
    char *remote_server = NULL;
    uint16_t remote_port = 0;
    char *p;
    char **toks;
    int num_toks;



    if ((fp = fopen("/opt/utc/conf/water_mark.conf", "rb")) == NULL) {
        E("-------open config file /opt/utc/conf/water_mark.conf fail.\n");
        return -1;
    }
    while(fgets(line, 512, fp))  {
        if (*line == '#')
            continue;
        toks = mSplit(line, "=", 2, &num_toks, 0);
        if(num_toks != 2)
            goto free_toks;
        _strim(toks[0]);
        _strim(toks[1]);
        printf("[%s]--[%s]\n",toks[0], toks[1]);
        if (strncmp(toks[0], "HIGH_WATER_MARK", strlen("HIGH_WATER_MARK")) == 0) {
            
            high_water_mark = (uint64_t)atoi(toks[1]);             
            if (high_water_mark <= 0) {

                mSplitFree(&toks, num_toks);
                fclose(fp);
                return 1;
            }  
        }
        if (strncmp(toks[0], "HASH_LEVEL_WATER", strlen("HASH_LEVEL_WATER")) == 0) {
                hash_level_water_mark = (uint64_t)atoi(toks[1]);
                if (hash_level_water_mark < 0) {
                    mSplitFree(&toks, num_toks);
                    fclose(fp);
                    return 1;

                }
        }   
free_toks:
        memset(line, 0x00, 512);
        mSplitFree(&toks, num_toks);
    }
    fclose(fp);
    return 0;
}


int main(int argc, char **argv)
{
    int     shmid;
    skbstat_t    *skbstat;
    int core_num;
    parse_conf();
    if (high_water_mark == 0 || hash_level_water_mark == 0) {
        printf("Please set high water mark in config file\n");
        exit(1);
    }
    if ( (shmid = shmget(STAT_SHM_KEY, SHM_SIZE, SHM_MODE)) < 0)
        perror("shmget");

    if ( ( skbstat = (skbstat_t *)shmat(shmid, NULL, 0)) == (void *) -1)
        perror("shmat");
    core_num = 8; 
    skbstat->hash_level_water_mark = hash_level_water_mark;
    int output_flag = 0;
    if (argc >1) {
        output_flag = atoi(argv[1]);
    }
    time_t timep;
    char buf[400];
    printf("core_num=%d\n",core_num);
    while (1) {
        int i;
        //system("clear");
        for (i = 0; i < core_num; ) {
            if (skbstat->total_tsc[i] && skbstat->total_num[i]) {
                if (skbstat->max_arg_diff_tsc[i] < (skbstat->total_tsc[i]) /(skbstat->total_num[i])) {
                    skbstat->max_arg_diff_tsc[i] = (skbstat->total_tsc[i])/(skbstat->total_num[i]);
                }
                if (skbstat->total_tsc[i] /(2 * 5) > high_water_mark) {
                   skbstat->exceed_high_water_mark_count ++; 
                }
                if (output_flag) {
                    time (&timep);
                printf("%score_%u avg-time: %6u ns, max-time: %6uns, pkg(s) :%8u  time(s):%10u high-water(%u) %5u, free-skb %10u, htimes:%5u, high-hwater:%5u, num-FIP:%5u,rte-timer(s):%10u,send-pkt(s):%10u,web-msg:%5d, cache-pkg:%5u, fail-recv-pkg:%5u\n", 
                        asctime(gmtime(&timep)), i, skbstat->total_tsc[i]/(2*(skbstat->total_num[i])), skbstat->max_arg_diff_tsc[i]/2,
                         skbstat->total_num[i]/5, skbstat->total_tsc[i]/(5*2), high_water_mark, skbstat->exceed_high_water_mark_count, skbstat->fail_skb,
                         skbstat->max_hash_level, skbstat->exceed_hash_level_count, skbstat->nums_sft_fip_entry,skbstat->rte_timer_tsc_per_sec[i] / 2, skbstat->send_pkt_tsc_per_sec[i]/2, skbstat->web_mesg_flag, skbstat->cache_pkg_num, skbstat->failed_recv_pkg[i]);
                } else {
                sprintf(buf, "core_%u avg-time: %6u ns, max-time: %6uns, pkg(s) :%8u  time(s):%10u high-water(%u) %5u, free-skb %10u, htimes:%5u, high-hwater:%5u, num-FIP:%5u,rte-timer(s):%10u,send-pkt(s):%10u,web-msg:%5d, cache-pkg:%5u, fail-recv-pkg:%5u\n", 
                         i, skbstat->total_tsc[i]/(2*(skbstat->total_num[i])), skbstat->max_arg_diff_tsc[i]/2, 
                        skbstat->total_num[i]/5, skbstat->total_tsc[i]/(5*2), high_water_mark, skbstat->exceed_high_water_mark_count, skbstat->fail_skb, 
                        skbstat->max_hash_level, skbstat->exceed_hash_level_count, skbstat->nums_sft_fip_entry,skbstat->rte_timer_tsc_per_sec[i] / 2, skbstat->send_pkt_tsc_per_sec[i]/2, skbstat->web_mesg_flag, skbstat->cache_pkg_num, skbstat->failed_recv_pkg[i]);
                    write_log(buf);
                }
            }
            i++;
        }

        sleep(3);

    }
    exit(0);
}
