#include <string.h>

#include <rte_malloc.h>

#include "kfifo.h"
#include "utils.h"
#include "debug.h"

static struct rte_mempool *kfifo_cache;
void kfifo_cache_init()
{
   
    kfifo_cache = rte_mempool_create("kfifo_struct_cache", KFIFO_STRUCT_MAX_NUM,
                sizeof(struct kfifo),
                64, 0,
                cache_mp_init, NULL,
                cache_obj_init, NULL,
                rte_socket_id(),  0);
                
    if(unlikely(!kfifo_cache))
    {
        LOG("%s(%d) kfifo memcache create failed.\n", __FUNCTION__, __LINE__);
        exit(1);
    }
}

/**
 * kfifo_init - allocates a new FIFO using a preallocated buffer
 * @buffer: the preallocated buffer to be used.
 * @size: the size of the internal buffer, this have to be a power of 2.
 * @gfp_mask: get_free_pages mask, passed to kmalloc()
 * @lock: the lock to be used to protect the fifo buffer
 *
 * Do NOT pass the kfifo to kfifo_free() after use ! Simply free the
 * struct kfifo with kfree().
 */
struct kfifo *kfifo_init(unsigned char *buffer, unsigned int size, rte_spinlock_t *lock)
{
	struct kfifo *fifo;

    /* size must be a power of 2 */
    if (!is_power_of_2(size)) {
        BUG_ON(!is_power_of_2(size));
        LOG("size=%x\n",size);
        size = roundup_pow_of_two(size);
    }
#if 0
    if(rte_mempool_mc_get(kfifo_cache, (void **)&fifo) <0)
		return NULL;
#else
     if (unlikely(fifo = rte_malloc(NULL, sizeof(struct kfifo), 0)) == NULL) {
        return NULL;
     }
#endif
    fifo->buffer = buffer;
    fifo->size = size;
    fifo->in = fifo->out = 0;
    fifo->lock = lock;
	return fifo;
}
/*
 * kfifo_alloc - allocates a new FIFO and its internal buffer
 * @size: the size of the internal buffer to be allocated.
 * @lock: the lock to be used to protect the fifo buffer
 * The size will be rounded-up to a power of 2.
 */
struct kfifo *kfifo_alloc(unsigned int size, rte_spinlock_t *lock)
{
    unsigned char *buffer;
    struct kfifo *ret;

    /*
     * round up to the next power of 2, since our 'let the indices
     * wrap' technique works only in this case.
     */
    if (!is_power_of_2(size)) {
        BUG_ON(size > 0x80000000);
        LOG("size=%x\n",size);
        size = roundup_pow_of_two(size);
    }

    buffer = rte_malloc(NULL, size, 0);
    if (unlikely(!buffer))
        return NULL;

    ret = kfifo_init(buffer, size, lock);

    if (unlikely(!ret))
        rte_free(buffer);

    return ret;
}
/**
 * kfifo_free - frees the FIFO
 * @fifo: the fifo to be freed.
 */
void kfifo_free(struct kfifo *fifo)
{
    rte_free(fifo->buffer);
    rte_mempool_mp_put(kfifo_cache, fifo);
}

