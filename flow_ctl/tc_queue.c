#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <rte_cycles.h>
#include <rte_ring.h>
#include <rte_mempool.h>

#include "tc_queue.h"
#include "sft_queue.h"

struct tc_queue_table
{
    struct rte_mempool *tc_queue_cache;
} tc_queue_tbl;

/**
 * tc_queue_create - allocates a new queue using a cache buffer
 * @name:   queue cache name and queue size cache name
 * @queue_cout: the size of the  queue, this have to be a power of 2.
 * @queue_size: the size of the internal buffer, this have to be a power of 2.
 * @socket_id cpu core id
 */

int tc_queue_create_bulk(uint32_t socket_id)
{
    tc_queue_tbl.tc_queue_cache = rte_mempool_create("sft_queue_struct_cache", TC_QUEUE_MAX_NUM,
            sizeof(struct sft_queue) + TC_QUEUE_BUFFER_SIZE,
            64, 0,
            cache_mp_init, NULL,
            cache_obj_init, NULL,
            /*rte_socket_id()*/socket_id,  0);

    if(unlikely(!tc_queue_tbl.tc_queue_cache))
    {
        LOG("%s(%d) kfifo memcache create failed.\n", __FUNCTION__, __LINE__);
        return -1;
    }
    printf("tc_queue_create_bulk\n");
    return 0;
}

struct sft_queue *tc_queue_init() 
{
    void *queue_buf;

    if(rte_mempool_mc_get(tc_queue_tbl.tc_queue_cache, (void **)&queue_buf) <0)
		return NULL;
    
    sft_queue_init((unsigned char *)((struct sft_queue *)queue_buf + 1), TC_QUEUE_BUFFER_SIZE, (struct sft_queue *) queue_buf);
    return (struct sft_queue *) queue_buf;
}

void tc_queue_free(struct sft_queue *queue) 
{
    rte_mempool_mp_put(tc_queue_tbl.tc_queue_cache, queue);
}

int tc_enqueue(struct sft_queue *queue, void *buffer, uint32_t len)
{
    return sft_queue_put(queue, buffer, len);
}

int tc_dequeue(struct sft_queue *queue, void *buffer, uint32_t len)
{
    return sft_queue_get(queue, buffer, len); 
}
