#ifndef _LOCK_H
#define _LOCK_H
#define ATOMICLOCK

#define PTHREAD
/* Spin lock using xchg. Added backoff wait to avoid concurrent lock/unlock
 * operation.
 * Original code copied from http://locklessinc.com/articles/locks/
 */

/* Compile read-write barrier */
#define barrier() asm volatile("": : :"memory")

/* Pause instruction to prevent excess processor bus usage */
#define cpu_relax() asm volatile("pause\n": : :"memory")

typedef struct {
    volatile int counter;
} atomic_t;
#define ATOMIC_INIT(i)  { (i) }
#define atomic_read(v) ((v)->counter)
#define atomic_set(v,i) (((v)->counter) = (i))
#define atomic_set_bit(v,i) (((v)->counter) |= 1 <<(i))
#define atomic_clear_bit(v,i) (((v)->counter) &= ~(1 <<(i)))
#define atomic_test(v,i) (((v)->counter) == (i))
static inline int atomic_add_return( int i, atomic_t *v )
{
    return (int)__sync_add_and_fetch(&v->counter, i);
}

static inline void atomic_add( int i, atomic_t *v )
{
    (void)__sync_add_and_fetch(&v->counter, i);
}
static inline void atomic_sub( int i, atomic_t *v )
{
    (void)__sync_sub_and_fetch(&v->counter, i);
}
static inline int atomic_sub_and_test( int i, atomic_t *v )
{
        return !(__sync_sub_and_fetch(&v->counter, i));
}
static inline void atomic_inc(atomic_t *v )
{
       (void)__sync_fetch_and_add(&v->counter, 1);
}
static inline void atomic_dec( atomic_t *v )
{
       (void)__sync_fetch_and_sub(&v->counter, 1);
}

static inline int atomic_dec_and_test( atomic_t *v )
{
       return !(__sync_sub_and_fetch(&v->counter, 1));
}
static inline int atomic_inc_and_test( atomic_t *v )
{
      return !(__sync_add_and_fetch(&v->counter, 1));
}
static inline int atomic_add_negative( int i, atomic_t *v )
{
       return (__sync_add_and_fetch(&v->counter, i) < 0);
}
static inline int atomic_cas( atomic_t *v, int oldval, int newval )
{
        return __sync_bool_compare_and_swap(&v->counter, oldval, newval);
}

/* Compile read-write barrier */
#define barrier() asm volatile("": : :"memory")

/* Pause instruction to prevent excess processor bus usage */
#define cpu_relax() asm volatile("pause\n": : :"memory")

/* Atomic exchange (of various sizes) */
static inline void *xchg_64(void *ptr, void *x)
{
    __asm__ __volatile__("xchgq %0,%1"
                :"=r" ((unsigned long long) x)
                :"m" (*(volatile long long *)ptr), "0" ((unsigned long long) x)
                :"memory");

    return x;
}

static inline unsigned xchg_32(void *ptr, unsigned x)
{
    __asm__ __volatile__("xchgl %0,%1"
                :"=r" ((unsigned) x)
                :"m" (*(volatile unsigned *)ptr), "0" (x)
                :"memory");

    return x;
}

static inline unsigned short xchg_16(void *ptr, unsigned short x)
{
    __asm__ __volatile__("xchgw %0,%1"
                :"=r" ((unsigned short) x)
                :"m" (*(volatile unsigned short *)ptr), "0" (x)
                :"memory");

    return x;
}


static inline unsigned short xchg_8(void *ptr, unsigned char x)
{
    __asm__ __volatile__("xchgb %0,%1"
                :"=r" (x)
                :"m" (*(volatile unsigned char *)ptr), "0" (x)
                :"memory");

    return x;
}

#ifdef ATOMICLOCK
typedef atomic_t spinlock;
static inline void spin_lock_init(spinlock* lock,long* flag)
{
    atomic_set(lock,0);
}
static inline void spin_lock(spinlock *lock)
{
    while (!atomic_cas(lock, 0, 1));    
}
static inline void spin_unlock(spinlock *lock)
{
    atomic_set(lock, 0);    
}

static inline int spin_trylock(spinlock *lock)
{
    return atomic_cas(lock, 0, 1);   
}
#elif defined (XCHGBACKOFF)
#define BUSY 1
typedef volatile unsigned char spinlock;

#define SPINLOCK_INITIALIZER 0

static inline void spin_lock(spinlock *lock)
{
    int wait = 1;
    while (1) {
        if (!xchg_8(lock, BUSY)) return;
        
        int i; 
        // wait here is important to performance.
        for (i = 0; i < wait; i++) {
            cpu_relax();
        }
        while (*lock) {
            wait *= 2; // exponential backoff if can't get lock
            for (i = 0; i < wait; i++) {
                cpu_relax();
            }
        }
    }
}
#include <unistd.h>
static inline void spin_unlock(spinlock *lock)
{
    barrier();
    *lock = 0;
}

static inline int spin_trylock(spinlock *lock)
{
    return xchg_8(lock, BUSY);
}

#elif defined(PTHREAD)
#define SPINLOCK_ATTR static __inline __attribute__((always_inline, no_instrument_function))

#define spinlock pthread_mutex_t

SPINLOCK_ATTR void spin_lock(spinlock *lock)
{
    pthread_mutex_lock(lock);
}

SPINLOCK_ATTR void spin_unlock(spinlock *lock)
{
    pthread_mutex_unlock(lock);
}
static inline void spin_lock_init(spinlock* lock,long* flag)
{
    pthread_mutex_init(lock,NULL);
}

#define SPINLOCK_INITIALIZER { 0 }
#elif defined(SYN_FETCH)
typedef struct
{
 volatile long  flag_;
 volatile long* spin_;
}spin_lock_t;
typedef spin_lock_t spinlock;
void spin_lock_init(spin_lock_t* lock,long* flag);
void spin_lock(spin_lock_t* lock);
int spin_trylock(spin_lock_t* lock);
void spin_unlock(spin_lock_t* lock);
int spin_is_lock(spin_lock_t* lock);

#endif

#endif /* _SPINLOCK_XCHG_BACKOFF_H */
