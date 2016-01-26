#include <string.h>
#include <rte_timer.h>
#include <rte_cycles.h>

#include "global.h"
#include "utils.h"
#include "tc_private.h"
#include "stat.h"
#include "export.h"
#include "tc_queue.h"

#define KSIFT_FLAGS_TC_ENABLE		0
#define KSIFT_FLAGS_L2CT_ENABLE		1
#define KSIFT_FLAGS_DPI_ENABLE		2
#define KSIFT_FLAGS_RELOAD_CONF		3

#define MAX_DEV_NAME 16
#define MAX_DEV_COUNT 32
#define DEV_STATIS_INTERVAL 2

#define DEV_STAT_FILE "/dev/shm/dev_statistics"

struct dev_statistics
{
    rte_atomic64_t rx_pkgs;
    rte_atomic64_t tx_pkgs;
    rte_atomic64_t rx_bytes;
    rte_atomic64_t tx_bytes;
};

struct lcore_hash
{
    struct sess_hash *hash;
};

static uint32_t sft_local_ip = 0;
static struct rte_mempool *    clone_pool;
static char conf_path[1024];
static char keylib_path[1024];

static char dev_name_list[MAX_DEV_COUNT][MAX_DEV_NAME];
static int dev_count = 0;

static struct dev_statistics dev_st[MAX_DEV_COUNT];
static struct rte_timer dev_st_timer;

static unsigned long   sft_flags = 0;
static uint8_t    reload_flags[64];
static struct rte_timer reload_timer;
static uint8_t is_reloading = 0;
static uint64_t reload_timestamp = 0;

static uint8_t enable_ofo = 0;
static uint8_t enable_dynamic = 0;

static uint64_t all_hash_size = 1024;

static uint8_t inner_outer_st[MAX_DEV_COUNT];
static uint8_t inner_outer_flag[MAX_DEV_COUNT];

static struct lcore_hash *g_sess_hash = NULL;

struct rte_mempool *fip_fifo_mem_cache = NULL;
struct rte_mempool *fip_data_mem_cache = NULL;
struct rte_mempool *datapipe_fifo_mem_cache = NULL;
struct rte_mempool *datapipe_data_mem_cache = NULL;
struct rte_mempool *flow_ip_mem_cache = NULL;

rte_atomic64_t g_used_pkg_num = RTE_ATOMIC64_INIT(0);

#define RULE_STATICS_MAX 4096
static char rule_flow_statics_buff[RULE_STATICS_MAX * 2];
static struct rule_statics_t rule_flows[RULE_STATICS_MAX];
static int rule_flows_num = 0;

#define	CLONE_MBUF_SIZE	(sizeof(struct rte_mbuf))
#define	NB_CLONE_MBUF	(1024* 2)

enum 
{
    RELOAD_CONF_START   = 1,
    RELOAD_CONF_DOING   = 2,
    RELOAD_CONF_END     = 3,
};

inline void set_conf_path(const char *str)
{
    if(!str)
    {
        return;
    }
    
    memset(conf_path, 0, sizeof(conf_path));
    stpcpy(conf_path, str);
}

inline char * get_conf_path(void)
{
    if(conf_path[0] == '\0')
    {
        return NULL;
    }
    
    return conf_path;
}

inline void set_keylib_path(const char *str)
{
    if(!str)
    {
        return;
    }
    
    memset(keylib_path, 0, sizeof(keylib_path));
    stpcpy(keylib_path, str);
}

inline char * get_keylib_path(void)
{
    if(keylib_path[0] == '\0')
    {
        return NULL;
    }
    
    return keylib_path;
}

void set_local_addr(uint32_t local_ip)
{
    sft_local_ip = local_ip;
}

uint32_t get_local_addr(void)
{
    return sft_local_ip;
}

void mem_cache_pool_create(void)
{
    clone_pool = rte_mempool_create("clone_pool", NB_CLONE_MBUF,
                                    CLONE_MBUF_SIZE, 32, 0, NULL, NULL, rte_pktmbuf_init, NULL,
                                    rte_socket_id(), 0);
    if(!clone_pool)
    {
        printf("%s(%d) ofo clone mempool create failed.\n", __FUNCTION__, __LINE__);
    }

    if (unlikely(tc_queue_create_bulk(rte_socket_id()) == -1)) {
        LOG("no enough mem for tc queue\n");
        exit(1);
    }
    
    if(!flow_ip_mem_cache)
    {
        flow_ip_mem_cache = rte_mempool_create("flow_ip_mem_cache", (KSFT_FIP_HASHSIZE << 1) * rte_lcore_count(),
                                    sizeof(struct sft_flow_ip_entry),
                                    64, 0,
                                    cache_mp_init, NULL,
                                    cache_obj_init, NULL,
                                    rte_socket_id(),  0);
        if(!flow_ip_mem_cache)
        {
            printf("%s(%d) flow ip  memcache create failed.\n", __FUNCTION__, __LINE__);
        }
    }
}

struct rte_mempool * get_clone_pool(void)
{
    return clone_pool;
}

inline void set_flags_tc_enable(void)
{
	set_bit(KSIFT_FLAGS_TC_ENABLE, &sft_flags);
	return;
}
inline void clear_flags_tc_enable(void)
{
	clear_bit(KSIFT_FLAGS_TC_ENABLE, &sft_flags);
	return;
}
inline int test_flags_tc_enable(void)
{
	return test_bit(KSIFT_FLAGS_TC_ENABLE, &sft_flags);
}

inline void set_flags_l2ct_enable(void)
{
	set_bit(KSIFT_FLAGS_L2CT_ENABLE, &sft_flags);
	return;
}
inline void clear_flags_l2ct_enable(void)
{
	clear_bit(KSIFT_FLAGS_L2CT_ENABLE, &sft_flags);
	return;
}
inline int test_flags_l2ct_enable(void)
{
	return test_bit(KSIFT_FLAGS_L2CT_ENABLE, &sft_flags);
}

inline void set_flags_dpi_enable(void)
{
	set_bit(KSIFT_FLAGS_DPI_ENABLE, &sft_flags);
	return;
}
inline void clear_flags_dpi_enable(void)
{
	clear_bit(KSIFT_FLAGS_DPI_ENABLE, &sft_flags);
	return;
}
inline int test_flags_dpi_enable(void)
{
	return test_bit(KSIFT_FLAGS_DPI_ENABLE, &sft_flags);
}

inline void set_flags_ofo_enable(uint8_t ofo)
{
    enable_ofo = ofo;
}

inline uint8_t get_flags_ofo_enable(void)
{
    return enable_ofo;
}

inline void set_flags_dynamic_enable(uint8_t dynamic)
{
    enable_dynamic = dynamic;
}

inline uint8_t get_flags_dynamic_enable(void)
{
    return enable_dynamic;
}

void check_fin_reload_conf(void)
{
    if(!is_reloading)
        return;
    
    is_reloading = 0;
    set_flags_tc_enable();
}

uint8_t is_tc_reloading(void)
{
    return is_reloading;
}

void set_tc_reload_conf(void)
{
    if(test_flags_tc_enable())
    {
        clear_flags_tc_enable();
    }
    else
    {
        return;
    }

    is_reloading = 1;
    
    //uint64_t _1s = rte_get_timer_hz();

    //rte_timer_init(&reload_timer);
    //rte_timer_reset(&reload_timer, _1s * 2, SINGLE, rte_lcore_id(), reload_conf_fin_cb,  NULL);
}

void dev_name_init(void)
{
    memset(dev_name_list, 0, MAX_DEV_COUNT * MAX_DEV_NAME);
}

void add_dev_by_name(char *name)
{
    if(dev_count >= MAX_DEV_COUNT)
    {
        return;
    }
    
    int len = strlen(name) >= MAX_DEV_NAME ? MAX_DEV_NAME : strlen(name);
    memcpy(dev_name_list[dev_count], name, len);
    dev_name_list[dev_count][MAX_DEV_NAME - 1] = '\0';
    dev_count++;
}

char * get_dev_name_by_index(int idx)
{
    if(idx >= dev_count)
    {
        return NULL;
    }
    
    return dev_name_list[idx];
}

static uint8_t get_dev_index_by_name(char *name)
{
    int i = 0;
    for(; i < dev_count; i++)
    {
        if(strcmp(dev_name_list[i], name) == 0)
        {
            return i;
        }
    }
    
    return (uint8_t)-1;
}

uint8_t is_dev_load(void)
{
    return dev_count ? 1 : 0;
}

static void init_dev_statistics(void)
{
    int i = 0;
    for(; i < MAX_DEV_COUNT; i++)
    {
        rte_atomic64_init(&dev_st[i].rx_pkgs);
        rte_atomic64_init(&dev_st[i].tx_pkgs);
        rte_atomic64_init(&dev_st[i].rx_bytes);
        rte_atomic64_init(&dev_st[i].tx_bytes);
    }
}

void add_rx_pkgs_for_dev(int port, int pkgs_count)
{
    if(port >= dev_count)
    {
        return;
    }
    
    rte_atomic64_add(&dev_st[port].rx_pkgs, pkgs_count);
}

void add_tx_pkgs_for_dev(int port, int pkgs_count)
{
    if(port >= dev_count)
    {
        return;
    }
    
    rte_atomic64_add(&dev_st[port].tx_pkgs, pkgs_count);
}

void add_rx_bytes_for_dev(int port, int bytes_count)
{
    if(port >= dev_count)
    {
        return;
    }
    
    rte_atomic64_add(&dev_st[port].rx_bytes, bytes_count);
}

void add_tx_bytes_for_dev(int port, int bytes_count)
{
    if(port >= dev_count)
    {
        return;
    }
    
    rte_atomic64_add(&dev_st[port].tx_bytes, bytes_count);
}

static void dev_statistics_to_file(struct rte_timer *tim, void *data)
{
    char buf[1024];
    int i = 0;

    FILE *fs = fopen(DEV_STAT_FILE, "w+");
    if(!fs)
    {
        printf("%s(%d), can not open configue file, file:%s\n",  __FUNCTION__, __LINE__, DEV_STAT_FILE);
        return;
    }
    
    for(; i < dev_count; i++)
    {
        sprintf(buf, "%s|RX_packets:%lu|TX_packets:%lu|RX_bytes:%lu|TX_bytes:%lu\n", 
                get_dev_name_by_index(i), rte_atomic64_read(&dev_st[i].rx_pkgs), 
                rte_atomic64_read(&dev_st[i].tx_pkgs), rte_atomic64_read(&dev_st[i].rx_bytes),
                rte_atomic64_read(&dev_st[i].tx_bytes));
        fputs(buf, fs);
    }

    fclose(fs);
    return;
}

void init_and_timer_dev_statistics(void)
{
    rte_timer_init(&dev_st_timer);
    init_dev_statistics();
    
    uint64_t _1s = rte_get_timer_hz();
    
    dev_statistics_to_file(NULL, NULL);
    rte_timer_reset(&dev_st_timer, _1s * DEV_STATIS_INTERVAL, 
                    PERIODICAL, rte_lcore_id(), dev_statistics_to_file, NULL);
}

void set_hash_size(uint64_t size)
{
    all_hash_size = size;
}

uint64_t get_hash_size(void)
{
    return all_hash_size;
}

void inner_outer_port_init(void)
{
    memset(inner_outer_st, 0xFF, sizeof(inner_outer_st));
    memset(inner_outer_flag, 0xFF, sizeof(inner_outer_flag));
}

void add_inner_outer_port(char *inner, char *outer)
{
    uint8_t in = get_dev_index_by_name(inner);
    uint8_t out = get_dev_index_by_name(outer);
    
    inner_outer_st[in] = out;
    inner_outer_st[out] = in;
    
    if(in != (uint8_t)-1)
        inner_outer_flag[in] = 0;
    
    if(out != (uint8_t)-1)
        inner_outer_flag[out] = 1;
}

uint8_t isinner_by_portid(int portid)
{
    if(portid >= dev_count)
    {
        return (uint8_t)-1;
    }
    
    return !inner_outer_flag[portid];
}

uint8_t get_dport_by_sport(int portid)
{
    if(portid >= dev_count)
    {
        return (uint8_t)-1;
    }
    
    return inner_outer_st[portid];
}

int sess_hash_init_by_lcore(uint8_t lcore_id, struct sess_hash *hash)
{
    if(!g_sess_hash)
    {
        g_sess_hash = (struct lcore_hash *)malloc(rte_lcore_count() * sizeof(struct lcore_hash));
        if(!g_sess_hash)
        {
            return -1;
        }
    }
    
    if(lcore_id >= rte_lcore_count())
    {
        return -1;
    }
    
    g_sess_hash[lcore_id].hash = hash;
    return 0;
}

struct sess_hash * get_session_hash_by_lcore(uint8_t lcore_id)
{
    if(!g_sess_hash)
    {
        return NULL;
    }

    return g_sess_hash[lcore_id].hash;
}

void sess_hash_uninit_by_lcore(void)
{
}

void pkg_num_inc(void)
{
    rte_atomic64_inc(&g_used_pkg_num);
}

void pkg_num_dec(void)
{
    rte_atomic64_dec(&g_used_pkg_num);
}

int64_t pkg_num_read(void)
{
    return rte_atomic64_read(&g_used_pkg_num);
}

inline uint8_t get_idle_lcore_id(void)
{
    uint64_t idle_id = 0;
#if 0    
    uint64_t min_idle = (uint64_t)-1;
    int count = rte_lcore_count();
    idle_id = count - 1;
    for(count--; count >= 0; count--)
    {
        if(skb_stat)
        {
            if(min_idle > (skb_stat->total_tsc[count] / 5 + skb_stat->rte_timer_tsc_per_sec[count] + skb_stat->send_pkt_tsc_per_sec[count]))
            {
                min_idle = skb_stat->total_tsc[count] / 5 + skb_stat->rte_timer_tsc_per_sec[count] + skb_stat->send_pkt_tsc_per_sec[count];
                idle_id = count;
            }
        }
        else
        {
            break;
        }
    }
#else
    static uint8_t lcore = 0;

    if(lcore >= rte_lcore_count())
    {
        lcore = 0;
    }
    else
    {
        lcore++;
    }
    idle_id = lcore;
#endif    
    
    return idle_id;
    
}

struct rule_statics_t* init_rule_flow(char *name, int id)
{
    int i = 0;
    int empty_index = -1;
    for(; i < RULE_STATICS_MAX; i++)
    {
        if(rule_flows[i].id == id && !strcmp(rule_flows[i].name, name))
        {
            return &rule_flows[i]; 
        }
        else if(empty_index == -1 && !strcmp(rule_flows[i].name, ""))
        {
            empty_index = i;
        }
    }
    
    memset(&rule_flows[empty_index], 0, sizeof(struct rule_statics_t));
    rule_flows[empty_index].id = id;
    memcpy(rule_flows[empty_index].name, name, strlen(name));
    rule_flows_num++;
    return &rule_flows[empty_index];
}

void rule_flow_statics_rx_add(struct rule_statics_t *p, uint64_t rx)
{
    if(!p)
        return;
    
    rte_atomic64_add(&p->rx_bytes, rx);
}

void rule_flow_statics_cache_add(struct rule_statics_t *p, uint64_t cache)
{
    if(!p)
        return;
    
    rte_atomic64_add(&p->cache_bytes, cache);
}

void output_rule_flow_statics()
{
    int i = 0, j = 0, n = 0;
    for(; i < RULE_STATICS_MAX && j < rule_flows_num; i++)
    {
        if(rule_flows[i].id != 0)
        {
            n += snprintf(rule_flow_statics_buff+n, 256, "%s-%d %lu/%lu\n",rule_flows[i].name, rule_flows[i].id, 
                      rte_atomic64_read(&rule_flows[i].rx_bytes), rte_atomic64_read(&rule_flows[i].cache_bytes));
            j++;
        } 
    }
    
    export_file(STATICTICS_RULE_FLOW, rule_flow_statics_buff, 3);
}
