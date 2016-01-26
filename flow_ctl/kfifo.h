#ifndef _MY_KFIFO_H
#define _MY_KFIFO_H
#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_spinlock.h>
#include "utils.h"

#define KFIFO_STRUCT_MAX_NUM (1<<11)

typedef struct kfifo {
    //pthread_mutex_t lock;
    rte_spinlock_t *lock;
	void *buffer;	/* the buffer holding the data */
	unsigned int size;	/* the size of the allocated buffer */
	volatile unsigned int in;	/* data is added at offset (in % size) */
	volatile unsigned int out;	/* data is extracted from off. (out % size) */
}kfifo_t;
/*kfifo  operation collections. */

void kfifo_cache_init();
struct kfifo *kfifo_init(unsigned char *buffer, unsigned int size, rte_spinlock_t *lock); 
struct kfifo *kfifo_alloc(unsigned int size, 
                 rte_spinlock_t *lock);
void kfifo_free(struct kfifo *fifo);

static inline unsigned int __kfifo_put(struct kfifo *fifo,
            const unsigned char *buffer, unsigned int len)
{
    uint32_t l;
    len = min(len, fifo->size - fifo->in + fifo->out);

    /*
     * Ensure that we sample the fifo->out index -before- we
     * start putting bytes into the kfifo.
     */

    smp_mb();

    /* first put the data starting from fifo->in to buffer end */
    l = min(len, fifo->size - (fifo->in & (fifo->size - 1)));
    memcpy(fifo->buffer + (fifo->in & (fifo->size - 1)), buffer, l);

    /* then put the rest (if any) at the beginning of the buffer */
    memcpy(fifo->buffer,   buffer + l, len - l);

    /*
     * Ensure that we add the bytes to the kfifo -before-
     * we update the fifo->in index.
     */

    smp_wmb();

    fifo->in += len;
    //pkg_num_inc();
    return len;
}
static inline unsigned int __kfifo_get(struct kfifo *fifo,
             unsigned char *buffer, unsigned int len)
{
    uint32_t l;
    
    len = min(len, fifo->in - fifo->out);

    /*
     * Ensure that we sample the fifo->in index -before- we
     * start removing bytes from the kfifo.
     */

    smp_rmb();

    /* first get the data from fifo->out until the end of the buffer */
    l = min(len, fifo->size - (fifo->out & (fifo->size - 1)));
    memcpy(buffer, fifo->buffer + (fifo->out & (fifo->size - 1)), l);

    /* then get the rest (if any) from the beginning of the buffer */
    memcpy(buffer + l, fifo->buffer, len - l);

    /*
     * Ensure that we remove the bytes from the kfifo -before-
     * we update the fifo->out index.
     */
    smp_mb();

    fifo->out += len;

    return len;
}


/**
 * __kfifo_reset - removes the entire FIFO contents, no locking version
 * @fifo: the fifo to be emptied.
 */
static inline void __kfifo_reset(struct kfifo *fifo)
{
	fifo->in = fifo->out = 0;
}

/**
 * kfifo_reset - removes the entire FIFO contents
 * @fifo: the fifo to be emptied.
 */
static inline void kfifo_reset(struct kfifo *fifo)
{
/*
*add locl here
*/
    rte_spinlock_lock(fifo->lock);
	__kfifo_reset(fifo);
    rte_spinlock_unlock(fifo->lock);
}

/**
 * kfifo_put - puts some data into the FIFO
 * @fifo: the fifo to be used.
 * @buffer: the data to be added.
 * @len: the length of the data to be added.
 *
 * This function copies at most @len bytes from the @buffer into
 * the FIFO depending on the free space, and returns the number of
 * bytes copied.
 */
static inline unsigned int kfifo_put(struct kfifo *fifo,
                const unsigned char *buffer, unsigned int len)
{
    unsigned int ret;

    rte_spinlock_lock(fifo->lock);

    ret = __kfifo_put(fifo, buffer, len);

    rte_spinlock_unlock(fifo->lock);

    return ret;
}

/**
 * kfifo_get - gets some data from the FIFO
 * @fifo: the fifo to be used.
 * @buffer: where the data must be copied.
 * @len: the size of the destination buffer.
 *
 * This function copies at most @len bytes from the FIFO into the
 * @buffer and returns the number of copied bytes.
 */
static inline unsigned int kfifo_get(struct kfifo *fifo,
                     unsigned char *buffer, unsigned int len)
{
    unsigned int ret;

    rte_spinlock_lock(fifo->lock);

    ret = __kfifo_get(fifo, buffer, len);

    /*
     * optimization: if the FIFO is empty, set the indices to 0
     * so we don't wrap the next time
     */
    if (fifo->in == fifo->out)
        fifo->in = fifo->out = 0;

    rte_spinlock_unlock(fifo->lock);

    return ret;
}
/**
 * __kfifo_len - returns the number of bytes available in the FIFO, no locking version
 * @fifo: the fifo to be used.
 */
static inline unsigned int __kfifo_len(struct kfifo *fifo)
{
	return fifo->in - fifo->out;
}

/**
 * kfifo_len - returns the number of bytes available in the FIFO
 * @fifo: the fifo to be used.
 */
static inline unsigned int kfifo_len(struct kfifo *fifo)
{
	unsigned int ret;

    rte_spinlock_lock(fifo->lock);

	ret = __kfifo_len(fifo);

    rte_spinlock_unlock(fifo->lock);
	return ret;
}

#endif
