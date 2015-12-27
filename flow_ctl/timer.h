#ifndef _LINUX_TIMER_H
#define _LINUX_TIMER_H
#include <stdint.h>
#include "list.h"

struct tvec_base;

struct timer_list {
	struct list_head entry;
	unsigned long expires;

	void (*function)(unsigned long);
	unsigned long data;

	struct tvec_base *base;
};
struct lock_class_key { };

#define __TIMER_LOCKDEP_MAP_INITIALIZER(_kn)

#define TIMER_INITIALIZER(_function, _expires, _data) {		\
		.entry = { .prev = TIMER_ENTRY_STATIC },	\
		.function = (_function),			\
		.expires = (_expires),				\
		.data = (_data),				\
		.base = &boot_tvec_bases,			\
		__TIMER_LOCKDEP_MAP_INITIALIZER(		\
			__FILE__ ":" __stringify(__LINE__))	\
	}

#define DEFINE_TIMER(_name, _function, _expires, _data)		\
	struct timer_list _name =				\
		TIMER_INITIALIZER(_function, _expires, _data)

void init_timer_key(struct timer_list *timer,
		    const char *name);
void init_timer_deferrable_key(struct timer_list *timer,
			       const char *name
			       );
#define init_timer(timer)\
	init_timer_key((timer), NULL)
#define init_timer_deferrable(timer)\
	init_timer_deferrable_key((timer), NULL)
#define init_timer_on_stack(timer)\
	init_timer_on_stack_key((timer), NULL)
#define setup_timer(timer, fn, data)\
	setup_timer_key((timer), NULL,(fn), (data))
#define setup_timer_on_stack(timer, fn, data)\
	setup_timer_on_stack_key((timer), NULL, (fn), (data))
static inline void init_timer_on_stack_key(struct timer_list *timer,
					   const char *name)
{
	init_timer_key(timer, name);
}

static inline void setup_timer_key(struct timer_list * timer,
				const char *name,
				void (*function)(unsigned long),
				unsigned long data)
{
	timer->function = function;
	timer->data = data;
	init_timer_key(timer, name);
}

static inline void setup_timer_on_stack_key(struct timer_list *timer,
					const char *name,
					void (*function)(unsigned long),
					unsigned long data)
{
	timer->function = function;
	timer->data = data;
	init_timer_on_stack_key(timer, name);
}

/**
 * timer_pending - is a timer pending?
 * @timer: the timer in question
 *
 * timer_pending will tell whether a given timer is currently pending,
 * or not. Callers must ensure serialization wrt. other operations done
 * to this timer, eg. interrupt contexts, or other CPUs on SMP.
 *
 * return value: 1 if the timer is pending, 0 if not.
 */
static inline int timer_pending(const struct timer_list * timer)
{
	return timer->entry.next != NULL;
}

extern void add_timer_on(struct timer_list *timer, int cpu);
extern int del_timer(struct timer_list * timer);
extern int mod_timer(struct timer_list *timer, unsigned long expires);
extern int mod_timer_pending(struct timer_list *timer, unsigned long expires);
extern int mod_timer_pinned(struct timer_list *timer, unsigned long expires);

#define TIMER_NOT_PINNED	0
#define TIMER_PINNED		1
/*
 * The jiffies value which is added to now, when there is no timer
 * in the timer wheel:
 */
#define NEXT_TIMER_MAX_DELTA	((1UL << 30) - 1)
extern void add_timer(struct timer_list *timer);

extern int try_to_del_timer_sync(struct timer_list *timer);
extern int del_timer_sync(struct timer_list *timer);
#define del_singleshot_timer_sync(t) del_timer_sync(t)

extern void init_timers(void);
#endif
