#ifndef __SFT_QUEUE_H
#define __SFT_QUEUE_H
#include <stdio.h>
#include <stdint.h>
#include <sys/queue.h>
#include <errno.h>
#include <rte_common.h>
#include <rte_memory.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include "utils.h"
#include "debug.h"

#define SFT_QUEUE_PAUSE_REP_COUNT 0
#define SFT_QUEUE_STRUCT_MAX_NUM (1<<5)
typedef struct sft_queue {

    /** queue producer status. */
    struct sft_prod {
        uint32_t watermark;      /**< Maximum items before EDQUOT. */
        uint32_t size;           /**< Size of ring. */
        uint32_t mask;           /**< Mask (size-1) of ring. */
        volatile uint32_t head;  /**< Producer head. */
        volatile uint32_t tail;  /**< Producer tail. */
    } prod ;

    /** queue consumer status. */
    struct sft_cons {
        uint32_t size;           /**< Size of the ring. */
        uint32_t mask;           /**< Mask (size-1) of ring. */
        volatile uint32_t head;  /**< Consumer head. */
        volatile uint32_t tail;  /**< Consumer tail. */
    } cons;
    void *buffer;   /* the buffer holding the data */
}sft_queue_t;
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
static inline void __attribute__((always_inline))
sft_queue_init(unsigned char *buffer, unsigned int size, struct sft_queue *queue)
{

    /* size must be a power of 2 */
    BUG_ON(!is_power_of_2(size));

    /* init the ring structure */
    //memset(queue, 0, sizeof(*queue));
    queue->prod.watermark = size;
    queue->prod.size = queue->cons.size = size;
    queue->prod.mask = queue->cons.mask = size-1;
    queue->prod.head = queue->cons.head = 0;
    queue->prod.tail = queue->cons.tail = 0;
    queue->buffer = buffer;
}
#if 0
/**
 * sft_queue_buffer_free - frees the queue buffer
 * @fifo: the fifo to be freed.
 */
static inline void __attribute__((always_inline))
sft_queue_buffer_free(struct sft_queue *queue)
{
    queue->release(queue);
}
#endif

static inline unsigned int __attribute__((always_inline))
sft_queue_put(struct sft_queue *queue, 
            const unsigned char *buffer, unsigned int len)
{
	uint32_t prod_head, prod_next;
	uint32_t cons_tail, free_entries;
	const unsigned max = len;
	int success;
	unsigned i, rep = 0;
	uint32_t mask = queue->prod.mask;

	/* move prod.head atomically */
	do {
		/* Reset n to the initial burst count */
		len = max;

		prod_head = queue->prod.head;
		cons_tail = queue->cons.tail;
		/* The subtraction is done between two unsigned 32bits value
		 * (the result is always modulo 32 bits even if we have
		 * prod_head > cons_tail). So 'free_entries' is always between 0
		 * and size(ring)-1. */
		free_entries = (mask + cons_tail - prod_head);

		/* check that we have enough room in ring */
        if (unlikely(len > free_entries)) {
            /* No free entry available */
            if (unlikely(free_entries == 0)) {
                return 0;
            }

            len = free_entries;
        }

		prod_next = prod_head + len;
		success = rte_atomic32_cmpset(&queue->prod.head, prod_head,
					      prod_next);
	} while (unlikely(success == 0));

	/* write entries in ring */
//	QUEUE_PUT_PTRS();
    memcpy(queue->buffer + (prod_head & mask), buffer, len);
	rte_compiler_barrier();

	/* if we exceed the watermark */
	if (unlikely(((mask + 1) - free_entries + len) > queue->prod.watermark)) {
	    LOG("exceed the watermark\n");
    }

	/*
	 * If there are other enqueues in progress that preceded us,
	 * we need to wait for them to complete
	 */
	while (unlikely(queue->prod.tail != prod_head)) {
		rte_pause();

		/* Set RTE_RING_PAUSE_REP_COUNT to avoid spin too long waiting
		 * for other thread finish. It gives pre-empted thread a chance
		 * to proceed and finish with ring dequeue operation. */
		if (SFT_QUEUE_PAUSE_REP_COUNT &&
		    ++rep == SFT_QUEUE_PAUSE_REP_COUNT) {
			rep = 0;
			sched_yield();
		}
	}
	queue->prod.tail = prod_next;
	return len;
}
static inline int __attribute__((always_inline))
sft_queue_get(struct sft_queue *queue,
        unsigned char *buffer, unsigned int len)
{
    uint32_t cons_head, prod_tail;
    uint32_t cons_next, entries;
    const unsigned max = len;
    int success;
    unsigned i, rep = 0;
    uint32_t mask = queue->prod.mask;

    /* move cons.head atomically */
    do {
        /* Restore n as it may change every loop */
        len = max;

        cons_head = queue->cons.head;
        prod_tail = queue->prod.tail;
        /* The subtraction is done between two unsigned 32bits value
         * (the result is always modulo 32 bits even if we have
         * cons_head > prod_tail). So 'entries' is always between 0
         * and size(ring)-1. */
        entries = (prod_tail - cons_head);

#if 0
        /* Set the actual entries for dequeue */
        if (len > entries) {
                if (unlikely(entries == 0)){
                    return 0;
                }

                len = entries;
        }
#else
        if (unlikely(len > entries)) {
            return 0;
        }

#endif
        cons_next = cons_head + len;
        success = rte_atomic32_cmpset(&queue->cons.head, cons_head,
                cons_next);
    } while (unlikely(success == 0));
    /* copy in table */
        //QUEUE_GET_PTRS();
        memcpy(buffer, queue->buffer + (cons_head & mask), len);
        rte_compiler_barrier();
        /*
         * If there are other dequeues in progress that preceded us,
         * we need to wait for them to complete
         */
        while (unlikely(queue->cons.tail != cons_head)) {
            rte_pause();

            /* Set RTE_RING_PAUSE_REP_COUNT to avoid spin too long waiting
             * for other thread finish. It gives pre-empted thread a chance
             * to proceed and finish with ring dequeue operation. */
            if (SFT_QUEUE_PAUSE_REP_COUNT &&
                    ++rep == SFT_QUEUE_PAUSE_REP_COUNT) {
                rep = 0;
                sched_yield();
            }
        }
        queue->cons.tail = cons_next;
        return  len;
}

static inline int
sft_queue_full(const struct sft_queue *queue)
{
    uint32_t prod_tail = queue->prod.tail;
    uint32_t cons_tail = queue->cons.tail;
    return (((cons_tail - prod_tail - 1) & queue->prod.mask) == 0);
}

static inline int
sft_queue_empty(const struct sft_queue *queue)
{
    uint32_t prod_tail = queue->prod.tail;
    uint32_t cons_tail = queue->cons.tail;
    return !!(cons_tail == prod_tail);
}

    static inline unsigned
sft_queue_count(const struct sft_queue *queue)
{
    uint32_t prod_tail = queue->prod.tail;
    uint32_t cons_tail = queue->cons.tail;
    return ((prod_tail - cons_tail) & queue->prod.mask);
}

/**
 * Return the number of free entries in a ring.
 *
 * @param r
 *   A pointer to the ring structure.
 * @return
 *   The number of free entries in the ring.
 */
    static inline unsigned
sft_queue_free_count(const struct sft_queue *queue)
{
    uint32_t prod_tail = queue->prod.tail;
    uint32_t cons_tail = queue->cons.tail;
    return ((cons_tail - prod_tail - 1) & queue->prod.mask);
}
#endif
