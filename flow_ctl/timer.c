/*
 *  linux/kernel/timer.c
 *
 *  Kernel internal timers, basic process system calls
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  1997-01-28  Modified by Finn Arne Gangstad to make timers scale better.
 *
 *  1997-09-10  Updated NTP code according to technical memorandum Jan '96
 *              "A Kernel Model for Precision Timekeeping" by Dave Mills
 *  1998-12-24  Fixed a xtime SMP race (we need the xtime_lock rw spinlock to
 *              serialize accesses to xtime/lost_ticks).
 *                              Copyright (C) 1998  Andrea Arcangeli
 *  1999-03-10  Improved NTP compatibility by Ulrich Windl
 *  2002-05-31	Move sys_sysinfo here and make its locking sane, Robert Love
 *  2000-10-05  Implemented scalable SMP per-CPU timer handling.
 *                              Copyright (C) 2000, 2001, 2002  Ingo Molnar
 *              Designed by David S. Miller, Alexey Kuznetsov and Ingo Molnar
 */


#include <rte_spinlock.h>
#include <rte_rwlock.h>
#include <rte_timer.h>
#include <rte_cycles.h>
#include "timer.h"
#include "utils.h"
#include "init.h"
#include "debug.h"

#define CONFIG_BASE_SMALL 0

#define TVN_BITS (CONFIG_BASE_SMALL ? 4 : 6)
#define TVR_BITS (CONFIG_BASE_SMALL ? 6 : 8)
#define TVN_SIZE (1 << TVN_BITS)
#define TVR_SIZE (1 << TVR_BITS)
#define TVN_MASK (TVN_SIZE - 1)
#define TVR_MASK (TVR_SIZE - 1)

struct tvec {
	struct list_head vec[TVN_SIZE];
};

struct tvec_root {
	struct list_head vec[TVR_SIZE];
};

struct tvec_base {
	rte_spinlock_t lock;
	struct timer_list *running_timer;
	unsigned long timer_jiffies;
	unsigned long next_timer;
	struct tvec_root tv1;
	struct tvec tv2;
	struct tvec tv3;
	struct tvec tv4;
	struct tvec tv5;
} __rte_cache_aligned;

static struct tvec_base *tvec_bases[16];


/*
 * Note that all tvec_bases are 2 byte aligned and lower bit of
 * base in timer_list is guaranteed to be zero. Use the LSB for
 * the new flag to indicate whether the timer is deferrable
 */
#define TBASE_DEFERRABLE_FLAG		(0x1)

/* Functions below help us manage 'deferrable' flag */
static inline unsigned int tbase_get_deferrable(struct tvec_base *base)
{
	return ((unsigned int)(unsigned long)base & TBASE_DEFERRABLE_FLAG);
}

static inline struct tvec_base *tbase_get_base(struct tvec_base *base)
{
	return ((struct tvec_base *)((unsigned long)base & ~TBASE_DEFERRABLE_FLAG));
}

static inline void timer_set_deferrable(struct timer_list *timer)
{
	timer->base = ((struct tvec_base *)((unsigned long)(timer->base) |
				       TBASE_DEFERRABLE_FLAG));
}

static inline void
timer_set_base(struct timer_list *timer, struct tvec_base *new_base)
{
	timer->base = (struct tvec_base *)((unsigned long)(new_base) |
				      tbase_get_deferrable(timer->base));
}
static inline void set_running_timer(struct tvec_base *base,
					struct timer_list *timer)
{
	base->running_timer = timer;
}

static void internal_add_timer(struct tvec_base *base, struct timer_list *timer)
{
	unsigned long expires = timer->expires;
	unsigned long idx = expires - base->timer_jiffies;
	struct list_head *vec;

	if (idx < TVR_SIZE) {
		int i = expires & TVR_MASK;
		vec = base->tv1.vec + i;
	} else if (idx < 1 << (TVR_BITS + TVN_BITS)) {
		int i = (expires >> TVR_BITS) & TVN_MASK;
		vec = base->tv2.vec + i;
	} else if (idx < 1 << (TVR_BITS + 2 * TVN_BITS)) {
		int i = (expires >> (TVR_BITS + TVN_BITS)) & TVN_MASK;
		vec = base->tv3.vec + i;
	} else if (idx < 1 << (TVR_BITS + 3 * TVN_BITS)) {
		int i = (expires >> (TVR_BITS + 2 * TVN_BITS)) & TVN_MASK;
		vec = base->tv4.vec + i;
	} else if ((signed long) idx < 0) {
		/*
		 * Can happen if you add a timer with expires == jiffies,
		 * or you set a timer to go off in the past
		 */
		vec = base->tv1.vec + (base->timer_jiffies & TVR_MASK);
	} else {
		int i;
		/* If the timeout is larger than 0xffffffff on 64-bit
		 * architectures then we use the maximum timeout:
		 */
		if (idx > 0xffffffffUL) {
			idx = 0xffffffffUL;
			expires = idx + base->timer_jiffies;
		}
		i = (expires >> (TVR_BITS + 3 * TVN_BITS)) & TVN_MASK;
		vec = base->tv5.vec + i;
	}
	/*
	 * Timers are FIFO:
	 */
	list_add_tail(&timer->entry, vec);
}


static void __init_timer(struct timer_list *timer,
			 const char *name)
{
	timer->entry.next = NULL;
	uint8_t cpu = rte_lcore_id();
	timer->base = tvec_bases[cpu];
}

/**
 * init_timer_key - initialize a timer
 * @timer: the timer to be initialized
 * @name: name of the timer
 * @key: lockdep class key of the fake lock used for tracking timer
 *       sync lock dependencies
 *
 * init_timer_key() must be done to a timer prior calling *any* of the
 * other timer functions.
 */
void init_timer_key(struct timer_list *timer,
		    const char *name)
{
	__init_timer(timer, name);
}

void init_timer_deferrable_key(struct timer_list *timer,
			       const char *name)
{
	init_timer_key(timer, name);
	timer_set_deferrable(timer);
}

static inline void detach_timer(struct timer_list *timer,
				int clear_pending)
{
	struct list_head *entry = &timer->entry;


	__list_del(entry->prev, entry->next);
	if (clear_pending)
		entry->next = NULL;
	entry->prev = LIST_POISON2;
}

/*
 * We are using hashed locking: holding per_cpu(tvec_bases).lock
 * means that all timers which are tied to this base via timer->base are
 * locked, and the base itself is locked too.
 *
 * So __run_timers/migrate_timers can safely modify all timers which could
 * be found on ->tvX lists.
 *
 * When the timer's base is locked, and the timer removed from list, it is
 * possible to set timer->base = NULL and drop the lock: the timer remains
 * locked.
 */
static struct tvec_base *lock_timer_base(struct timer_list *timer)
{
	struct tvec_base *base;

	for (;;) {
		struct tvec_base *prelock_base = timer->base;
		base = tbase_get_base(prelock_base);
		if (likely(base != NULL)) {
			//rte_spinlock_lock(&base->lock);
			if (likely(prelock_base == timer->base))
				return base;
			/* The timer has migrated to another CPU */
			//rte_spinlock_unlock(&base->lock);
		}
		cpu_relax();
	}
}

static inline int
__mod_timer(struct timer_list *timer, unsigned long expires,
						bool pending_only, int pinned)
{
	struct tvec_base *base, *new_base;
	unsigned long flags;
	int ret = 0 , cpu;

	BUG_ON(!timer->function);

	base = lock_timer_base(timer);

	if (timer_pending(timer)) {
		detach_timer(timer, 0);
		if (timer->expires == base->next_timer &&
		    !tbase_get_deferrable(timer->base))
			base->next_timer = base->timer_jiffies;
		ret = 1;
	} else {
		if (pending_only)
			goto out_unlock;
	}

	cpu = rte_lcore_id();
	new_base = tvec_bases[cpu];

	if (base != new_base) {
		/*
		 * We are trying to schedule the timer on the local CPU.
		 * However we can't change timer's base while it is running,
		 * otherwise del_timer_sync() can't detect that the timer's
		 * handler yet has not finished. This also guarantees that
		 * the timer is serialized wrt itself.
		 */
		if (likely(base->running_timer != timer)) {
			/* See the comment in lock_timer_base() */
			timer_set_base(timer, NULL);
			//rte_spinlock_unlock(&base->lock);
			base = new_base;
			//rte_spinlock_lock(&base->lock);
			timer_set_base(timer, base);
		}
	}

	timer->expires = expires;
	if (time_before(timer->expires, base->next_timer) &&
	    !tbase_get_deferrable(timer->base))
		base->next_timer = timer->expires;
	internal_add_timer(base, timer);

out_unlock:
	//rte_spinlock_unlock(&base->lock);

	return ret;
}

/**
 * mod_timer_pending - modify a pending timer's timeout
 * @timer: the pending timer to be modified
 * @expires: new timeout in jiffies
 *
 * mod_timer_pending() is the same for pending timers as mod_timer(),
 * but will not re-activate and modify already deleted timers.
 *
 * It is useful for unserialized use of timers.
 */
int mod_timer_pending(struct timer_list *timer, unsigned long expires)
{
	return __mod_timer(timer, expires, true, TIMER_NOT_PINNED);
}

/**
 * mod_timer - modify a timer's timeout
 * @timer: the timer to be modified
 * @expires: new timeout in jiffies
 *
 * mod_timer() is a more efficient way to update the expire field of an
 * active timer (if the timer is inactive it will be activated)
 *
 * mod_timer(timer, expires) is equivalent to:
 *
 *     del_timer(timer); timer->expires = expires; add_timer(timer);
 *
 * Note that if there are multiple unserialized concurrent users of the
 * same timer, then mod_timer() is the only safe way to modify the timeout,
 * since add_timer() cannot modify an already running timer.
 *
 * The function returns whether it has modified a pending timer or not.
 * (ie. mod_timer() of an inactive timer returns 0, mod_timer() of an
 * active timer returns 1.)
 */
int mod_timer(struct timer_list *timer, unsigned long expires)
{
	/*
	 * This is a common optimization triggered by the
	 * networking code - if the timer is re-modified
	 * to be the same thing then just return:
	 */
	if (timer_pending(timer) && timer->expires == expires)
		return 1;

	return __mod_timer(timer, expires, false, TIMER_NOT_PINNED);
}

/**
 * mod_timer_pinned - modify a timer's timeout
 * @timer: the timer to be modified
 * @expires: new timeout in jiffies
 *
 * mod_timer_pinned() is a way to update the expire field of an
 * active timer (if the timer is inactive it will be activated)
 * and not allow the timer to be migrated to a different CPU.
 *
 * mod_timer_pinned(timer, expires) is equivalent to:
 *
 *     del_timer(timer); timer->expires = expires; add_timer(timer);
 */
int mod_timer_pinned(struct timer_list *timer, unsigned long expires)
{
	if (timer->expires == expires && timer_pending(timer))
		return 1;

	return __mod_timer(timer, expires, false, TIMER_PINNED);
}

/**
 * add_timer - start a timer
 * @timer: the timer to be added
 *
 * The kernel will do a ->function(->data) callback from the
 * timer interrupt at the ->expires point in the future. The
 * current time is 'jiffies'.
 *
 * The timer's ->expires, ->function (and if the handler uses it, ->data)
 * fields must be set prior calling this function.
 *
 * Timers with an ->expires field in the past will be executed in the next
 * timer tick.
 */
void add_timer(struct timer_list *timer)
{
	BUG_ON(timer_pending(timer));
	mod_timer(timer, timer->expires);
}

/**
 * add_timer_on - start a timer on a particular CPU
 * @timer: the timer to be added
 * @cpu: the CPU to start it on
 *
 * This is not very scalable on SMP. Double adds are not possible.
 */
void add_timer_on(struct timer_list *timer, int cpu)
{
	struct tvec_base *base = tvec_bases[cpu];

	BUG_ON(timer_pending(timer) || !timer->function);
	//rte_spinlock_lock(&base->lock);
	timer_set_base(timer, base);
	if (time_before(timer->expires, base->next_timer) &&
	    !tbase_get_deferrable(timer->base))
		base->next_timer = timer->expires;
	internal_add_timer(base, timer);
	/*
	 * Check whether the other CPU is idle and needs to be
	 * triggered to reevaluate the timer wheel when nohz is
	 * active. We are protected against the other CPU fiddling
	 * with the timer by holding the timer base lock. This also
	 * makes sure that a CPU on the way to idle can not evaluate
	 * the timer wheel.
	 */
	//rte_spinlock_unlock(&base->lock);
}

/**
 * del_timer - deactive a timer.
 * @timer: the timer to be deactivated
 *
 * del_timer() deactivates a timer - this works on both active and inactive
 * timers.
 *
 * The function returns whether it has deactivated a pending timer or not.
 * (ie. del_timer() of an inactive timer returns 0, del_timer() of an
 * active timer returns 1.)
 */
int del_timer(struct timer_list *timer)
{
	struct tvec_base *base;
	unsigned long flags;
	int ret = 0;

	if (timer_pending(timer)) {
		base = lock_timer_base(timer);
		if (timer_pending(timer)) {
			detach_timer(timer, 1);
			if (timer->expires == base->next_timer &&
			    !tbase_get_deferrable(timer->base))
				base->next_timer = base->timer_jiffies;
			ret = 1;
		}
		//rte_spinlock_unlock(&base->lock);
	}

	return ret;
}

/**
 * try_to_del_timer_sync - Try to deactivate a timer
 * @timer: timer do del
 *
 * This function tries to deactivate a timer. Upon successful (ret >= 0)
 * exit the timer is not queued and the handler is not running on any CPU.
 *
 * It must not be called from interrupt contexts.
 */
int try_to_del_timer_sync(struct timer_list *timer)
{
	struct tvec_base *base;
	int ret = -1;

	base = lock_timer_base(timer);

	if (base->running_timer == timer)
		goto out;

	ret = 0;
	if (timer_pending(timer)) {
		detach_timer(timer, 1);
		if (timer->expires == base->next_timer &&
		    !tbase_get_deferrable(timer->base))
			base->next_timer = base->timer_jiffies;
		ret = 1;
	}
out:
	//rte_spinlock_unlock(&base->lock);

	return ret;
}

/**
 * del_timer_sync - deactivate a timer and wait for the handler to finish.
 * @timer: the timer to be deactivated
 *
 * This function only differs from del_timer() on SMP: besides deactivating
 * the timer it also makes sure the handler has finished executing on other
 * CPUs.
 *
 * Synchronization rules: Callers must prevent restarting of the timer,
 * otherwise this function is meaningless. It must not be called from
 * interrupt contexts. The caller must not hold locks which would prevent
 * completion of the timer's handler. The timer's handler must not call
 * add_timer_on(). Upon exit the timer is not queued and the handler is
 * not running on any CPU.
 *
 * The function returns whether it has deactivated a pending timer or not.
 */
int del_timer_sync(struct timer_list *timer)
{

	for (;;) {
		int ret = try_to_del_timer_sync(timer);
		if (ret >= 0)
			return ret;
		cpu_relax();
	}
}

static int cascade(struct tvec_base *base, struct tvec *tv, int index)
{
	/* cascade all the timers from tv up one level */
	struct timer_list *timer, *tmp;
	struct list_head tv_list;

	list_replace_init(tv->vec + index, &tv_list);

	/*
	 * We are removing _all_ timers from the list, so we
	 * don't have to detach them individually.
	 */
	list_for_each_entry_safe(timer, tmp, &tv_list, entry) {
		BUG_ON(tbase_get_base(timer->base) != base);
		internal_add_timer(base, timer);
	}

	return index;
}

#define INDEX(N) ((base->timer_jiffies >> (TVR_BITS + (N) * TVN_BITS)) & TVN_MASK)

/**
 * __run_timers - run all expired timers (if any) on this CPU.
 * @base: the timer vector to be processed.
 *
 * This function cascades all vectors and executes all expired timer
 * vectors.
 */
static inline void __run_timers(struct tvec_base *base)
{
	struct timer_list *timer;

	//rte_spinlock_lock(&base->lock);
	while (time_after_eq(rte_rdtsc(), base->timer_jiffies)) {
		struct list_head work_list;
		struct list_head *head = &work_list;
		int index = base->timer_jiffies & TVR_MASK;

		/*
		 * Cascade timers:
		 */
		if (!index &&
			(!cascade(base, &base->tv2, INDEX(0))) &&
				(!cascade(base, &base->tv3, INDEX(1))) &&
					!cascade(base, &base->tv4, INDEX(2)))
			cascade(base, &base->tv5, INDEX(3));
		++base->timer_jiffies;
		list_replace_init(base->tv1.vec + index, &work_list);
		while (!list_empty(head)) {
			void (*fn)(unsigned long);
			unsigned long data;

			timer = list_first_entry(head, struct timer_list,entry);
			fn = timer->function;
			data = timer->data;

			set_running_timer(base, timer);
			detach_timer(timer, 1);

			//rte_spinlock_unlock(&base->lock);
			{
				fn(data);
			}
			//rte_spinlock_lock(&base->lock);
		}
	}
	set_running_timer(base, NULL);
	//rte_spinlock_unlock(&base->lock);
}

static void
run_local_timers(__attribute__((unused)) struct rte_timer *tim,
      __attribute__((unused)) void *arg)
{
	uint8_t cpu = rte_lcore_id();
    struct tvec_base *base = tvec_bases[cpu];


    if (time_after_eq(rte_rdtsc(), base->timer_jiffies))
        __run_timers(base);
}

static int  init_timers_for_each_cpu()
{
	int i, j;
	struct tvec_base *base;
	uint8_t nb_lcores = rte_lcore_count();

	for (i = 0; i < nb_lcores; i ++) {
		base = rte_zmalloc(NULL, sizeof(*base), 0);	
		if (!base)
			return -1;

		/* Make sure that tvec_base is 2 byte aligned */
		if (tbase_get_deferrable(base)) {
			rte_free(base);
			return -1;
		}
		tvec_bases[i] = base;
		//rte_spinlock_init(&base->lock);

		for (j = 0; j < TVN_SIZE; j++) {
			INIT_LIST_HEAD(base->tv5.vec + j);
			INIT_LIST_HEAD(base->tv4.vec + j);
			INIT_LIST_HEAD(base->tv3.vec + j);
			INIT_LIST_HEAD(base->tv2.vec + j);
		}
		for (j = 0; j < TVR_SIZE; j++)
			INIT_LIST_HEAD(base->tv1.vec + j);

		base->timer_jiffies = rte_rdtsc();
		base->next_timer = base->timer_jiffies;

        struct rte_timer sft_timer;
        rte_timer_init(&sft_timer);

        /* load timer0, every second, on master lcore, reloaded automatically */
        rte_timer_reset(&sft_timer, pv.hz, PERIODICAL, i, run_local_timers, NULL);

	}

	return 0;
}


void init_timers(void)
{
	init_timers_for_each_cpu();
}

