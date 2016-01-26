#include <string.h>

#include <rte_malloc.h>

#include "sft_queue.h"
#include "utils.h"
#include "debug.h"

#if 0
/*
 * kfifo_alloc - allocates a new FIFO and its internal buffer
 * @size: the size of the internal buffer to be allocated.
 * @lock: the lock to be used to protect the fifo buffer
 * The size will be rounded-up to a power of 2.
 */
struct sft_queue *sft_queue_alloc(unsigned int size)
{
    unsigned char *buffer;
    struct sft_queue *ret;

    /*
     * round up to the next power of 2, since our 'let the indices
     * wrap' technique works only in this case.
     */
    if (!is_power_of_2(size)) {
        BUG_ON(size > 0x80000000);
        size = roundup_pow_of_two(size);
    }

    buffer = rte_malloc(NULL, size, 0);
    if (unlikely(!buffer))
        return NULL;

    ret = sft_queue_init(buffer, size);

    if (unlikely(!ret))
        rte_free(buffer);

    return ret;
}
#endif

