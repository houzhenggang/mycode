#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <rte_mbuf.h>
#include <rte_mempool.h>

struct rule_statics_t
{
    int id;
    char name[64];
    rte_atomic64_t rx_bytes;
    rte_atomic64_t cache_bytes;
};

int send_single_packet(struct rte_mbuf *m, uint8_t port);

void set_conf_path(const char *str);
char * get_conf_path(void);

void set_keylib_path(const char *str);
char * get_keylib_path(void);

void set_local_addr(uint32_t local_ip);
uint32_t get_local_addr(void);

void mem_cache_pool_create(void);
struct rte_mempool * get_clone_pool(void);

void set_flags_tc_enable(void);
void clear_flags_tc_enable(void);
int test_flags_tc_enable(void);

void set_flags_l2ct_enable(void);
void clear_flags_l2ct_enable(void);
int test_flags_l2ct_enable(void);

void set_flags_dpi_enable(void);
void clear_flags_dpi_enable(void);
int test_flags_dpi_enable(void);

void set_tc_reload_conf(void);
uint8_t is_tc_reloading(void);

void set_flags_ofo_enable(uint8_t ofo);
uint8_t get_flags_ofo_enable(void);

void set_flags_dynamic_enable(uint8_t dynamic);
uint8_t get_flags_dynamic_enable(void);

void dev_name_init(void);
void add_dev_by_name(char *name);
char * get_dev_name_by_index(int idx);
uint8_t is_dev_load(void);

void init_and_timer_dev_statistics(void);
void add_rx_pkgs_for_dev(int port, int pkgs_count);
void add_tx_pkgs_for_dev(int port, int pkgs_count);
void add_rx_bytes_for_dev(int port, int bytes_count);
void add_tx_bytes_for_dev(int port, int bytes_count);

void set_hash_size(uint64_t size);
uint64_t get_hash_size(void);

void inner_outer_port_init(void);
void add_inner_outer_port(char *inner, char *outer);
uint8_t isinner_by_portid(int portid);
uint8_t get_dport_by_sport(int portid);

//int sess_hash_init_by_lcore(uint8_t lcore_id, struct sess_hash *hash);
struct sess_hash * get_session_hash_by_lcore(uint8_t lcoreid);
void sess_hash_uninit_by_lcore(void);

void pkg_num_inc(void);
void pkg_num_decc(void);
int64_t pkg_num_read(void);

void check_fin_reload_conf(void);

uint8_t get_idle_lcore_id(void);

struct rule_statics_t* init_rule_flow(char *name, int id);
void rule_flow_statics_rx_add(struct rule_statics_t *p, uint64_t rx);
void rule_flow_statics_cache_add(struct rule_statics_t *p, uint64_t cache);

#endif
