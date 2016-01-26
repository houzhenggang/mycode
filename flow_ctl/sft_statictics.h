#ifndef SFT_STATICTICS_H 
#define SFT_STATICTICS_H 
#include <rte_atomic.h>

struct dpi_statistics_struct
{
    rte_atomic64_t up;
    rte_atomic64_t down;
    rte_atomic64_t total_link;
    rte_atomic64_t link;
    char name[32];
};
extern struct dpi_statistics_struct (*DPI_Statistics)[16][256];
int sft_do_statistics_flow_work(struct rte_mbuf *skb); 
int init_dpi_statistics(void);
#endif
