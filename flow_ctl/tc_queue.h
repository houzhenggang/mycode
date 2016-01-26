#ifndef __TC_QUEUE_H
#define __TC_QUEUE_H
#include <stdint.h>
#include "sft_queue.h"

#define TC_QUEUE_MAX_NUM (1<<10)
#define TC_QUEUE_BUFFER_SIZE (1<<12)

int tc_queue_create_bulk(uint32_t socket_id);
struct sft_queue *tc_queue_init();
void tc_queue_free(struct sft_queue *queue);
int tc_enqueue(struct sft_queue *queue, void *buffer, uint32_t len);
int tc_dequeue(struct sft_queue *queue, void *buffer, uint32_t len);
#endif
