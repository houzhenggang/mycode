#ifndef __UTILS_H
#define __UTILS_H
#include <arpa/inet.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <linux/types.h>

#include <rte_mempool.h>
#include <rte_ether.h>

#include "com.h"

#define swap_u32(a, b)  do { __u32 t = a; a = b; b = t; } while(0)
#define swap___u16(a, b)  do { __u32 t = a; a = b; b = t; } while(0)

#define barrier() asm volatile("": : :"memory")
#define smp_mb()    barrier()
#define smp_rmb()   barrier()
#define smp_wmb()   barrier()

#define min(a, b) ((a) < (b) ? (a) : (b))
#define countof(array) (sizeof (array) / sizeof ((array)[0]))
/* Compile read-write barrier */
#define barrier() asm volatile("": : :"memory")

/* Pause instruction to prevent excess processor bus usage */
#define cpu_relax() asm volatile("pause\n": : :"memory")
/*
 *  * Check at compile time that something is of a particular type.
 *   * Always evaluates to 1 so you may use it easily in comparisons.
 *    */
#define typecheck(type,x) \
({  type __dummy; \
    typeof(x) __dummy2; \
    (void)(&__dummy == &__dummy2); \
    1; \
})

/*
 *  * Check at compile time that 'function' is a certain type, or is a pointer
 *   * to that type (needs to use typedef for the function type.)
 *    */
#define typecheck_fn(type,function) \
({  typeof(type) __tmp = function; \
    (void)__tmp; \
})

#define time_after(a,b)     \
    (typecheck(unsigned long, a) && \
     typecheck(unsigned long, b) && \
     ((long)(b) - (long)(a) < 0))
#define time_before(a,b)    time_after(b,a)

#define time_after_eq(a,b)  \
    (typecheck(unsigned long, a) && \
     typecheck(unsigned long, b) && \
     ((long)(a) - (long)(b) >= 0))
#define time_before_eq(a,b) time_after_eq(b,a)

/*
 *  * Calculate whether a is in the range of [b, c].
 *   */
#define time_in_range(a,b,c) \
    (time_after_eq(a,b) && \
     time_before_eq(a,c))

/*
 *  * Calculate whether a is in the range of [b, c).
 *   */
#define time_in_range_open(a,b,c) \
    (time_after_eq(a,b) && \
     time_before(a,c))

/* Same as above, but does so with platform independent 64bit types.
 *  * These must be used when utilizing jiffies_64 (i.e. return value of
 *   * get_jiffies_64() */
#define time_after64(a,b)   \
    (typecheck(__u64, a) && \
     typecheck(__u64, b) && \
     ((__s64)(b) - (__s64)(a) < 0))
#define time_before64(a,b)  time_after64(b,a)

#define time_after_eq64(a,b)    \
    (typecheck(__u64, a) && \
     typecheck(__u64, b) && \
     ((__s64)(a) - (__s64)(b) >= 0))
#define time_before_eq64(a,b)   time_after_eq64(b,a)
/*
 *  * These four macros compare jiffies and 'a' for convenience.
 *   */

/* time_is_before_jiffies(a) return true if a is before jiffies */
#define time_is_before_jiffies(a) time_after(jiffies, a)

/* time_is_after_jiffies(a) return true if a is after jiffies */
#define time_is_after_jiffies(a) time_before(jiffies, a)

/* time_is_before_eq_jiffies(a) return true if a is before or equal to jiffies*/
#define time_is_before_eq_jiffies(a) time_after_eq(jiffies, a)

/* time_is_after_eq_jiffies(a) return true if a is after or equal to jiffies*/
#define time_is_after_eq_jiffies(a) time_before_eq(jiffies, a)

/*
 *  * Have the 32 bit jiffies value wrap 5 minutes after boot
 *   * so jiffies wrap bugs show up earlier.
 *    */
#define INITIAL_JIFFIES ((unsigned long)(unsigned int) (-300*HZ))

#define __be32 uint32_t
#define bool int

#define BITS_PER_LONG __WORDSIZE

#define RTE_NIPQUAD(addr) \
    ((unsigned char *)&addr)[0], \
((unsigned char *)&addr)[1], \
((unsigned char *)&addr)[2], \
((unsigned char *)&addr)[3]
#define NIPQUAD_FMT "%u.%u.%u.%u"

//#define rte_zmalloc(a, b, c) calloc(1, b)
//#define rte_realloc(a, b, c) realloc(a, b)
//#define rte_free free


struct token_struct
{
    rte_atomic32_t token;
};
 
static inline int test_bit(int nr, const volatile void *addr)
{

    return (1UL&(((const int *)addr)[nr>>5]>>(nr&31))) != 0;
}

static inline void set_bit(int nr, volatile unsigned long *addr)
{
	addr[nr / BITS_PER_LONG] |= 1UL << (nr % BITS_PER_LONG);
}

static inline void clear_bit(int nr, volatile unsigned long *addr)
{
	addr[nr / BITS_PER_LONG] &= ~(1UL << (nr % BITS_PER_LONG));
}

static inline bool ipv4_is_multicast(__be32 addr)
{
	return (addr & htonl(0xf0000000)) == htonl(0xe0000000);
}

static inline bool ipv4_is_local_multicast(__be32 addr)
{
    return (addr & htonl(0xffffff00)) == htonl(0xe0000000);
}

static inline bool ipv4_is_lbcast(__be32 addr)
{
    /* limited broadcast */
    return addr == htonl(0xffffffff);
}

static inline bool ipv4_is_zeronet(__be32 addr)
{
    return (addr & htonl(0xff000000)) == htonl(0x00000000);
}

static inline int fls(int x)
{
    int r;
    __asm__("bsrl %1,%0\n\t"
        "cmovzl %2,%0"
        : "=&r" (r) : "rm" (x), "rm" (-1));
    return r + 1;
}
static inline int fls64(uint64_t x)
{
    uint32_t h = x >> 32;
    if (h)
        return fls(h) + 32;
    return fls(x);
}
static inline unsigned fls_long(unsigned long l)
{
    if (sizeof(l) == 4)
        return fls(l);
    return fls64(l);
}

static inline unsigned long roundup_pow_of_two(unsigned long x)
{
    return 1UL << (fls_long(x - 1));
}

static inline
bool is_power_of_2(unsigned long n)
{
    return (n != 0 && ((n & (n - 1)) == 0));
}

static inline void cache_obj_init(struct rte_mempool *mp, __attribute__((unused)) void *arg,
        void *obj, unsigned i)
{
    uint32_t *objnum = obj;
    memset(obj, 0, mp->elt_size);
    *objnum = i;
}

static inline void cache_mp_init(struct rte_mempool * mp, __attribute__((unused)) void * arg)
{
    printf("mempool name is %s\n", mp->name);
    /* nothing to be implemented here*/
    return ;
}

/*
 * memmem(): A strstr() work-alike for non-text buffers
 */
static inline void *memmem(const void *s1, const void *s2, size_t len1, size_t len2)
{
	char *bf = (char *)s1, *pt = (char *)s2;
	size_t i, j;

	if (len2 > len1)
		return (void *)NULL;

	for (i = 0; i <= (len1 - len2); ++i)
	{
		for (j = 0; j < len2; ++j)
			if (pt[j] != bf[i + j]) break;
		if (j == len2) return (bf + i);
	}
	return NULL;
}
#define IPQUADS(addr) \
        ((unsigned char *)&addr)[0], \
        ((unsigned char *)&addr)[1], \
        ((unsigned char *)&addr)[2], \
        ((unsigned char *)&addr)[3]
#define IPQUAD_FMT "%u.%u.%u.%u"
static inline unsigned __isspace(char c)
{
	return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

static inline unsigned __isblank(char c)
{
	return (c == ' ' || c == '\t');
}
static inline char *ipv4_hltos(__u32 u, char *s)
{
	static char hs[20];
	if(!s) s = hs;
	sprintf(s, "%d.%d.%d.%d",
		(int)(u >> 24) & 0xff,
		(int)(u >> 16) & 0xff,
		(int)(u >> 8) & 0xff,
		(int)u & 0xff );
	return s;
}

static inline char *ipv4_nltos(__u32 u, char *s)
{
	static char ns[20];
	if(!s) s = ns;
	sprintf(s, "%d.%d.%d.%d",
		(int)u & 0xff, 
		(int)(u >> 8) & 0xff,
		(int)(u >> 16) & 0xff,
		(int)(u >> 24) & 0xff);
	return s;
}

static inline __u32 ipv4_stohl(const char *s)
{
	int u[4];
	if(sscanf(s, "%d.%d.%d.%d", &u[0], &u[1], &u[2], &u[3]) == 4)
	{
		return  (((__u32)u[0] & 0xff) << 24) |
				(((__u32)u[1] & 0xff) << 16) |
				(((__u32)u[2] & 0xff) << 8) |
				(((__u32)u[3] & 0xff));
	}
	else
		return 0xffffffff;
}
static inline __u32 ipv4_stonl(const char *s)
{
	int u[4];
	if(sscanf(s, "%d.%d.%d.%d", &u[0], &u[1], &u[2], &u[3]) == 4)
	{
		return  (((__u32)u[0] & 0xff)) |
				(((__u32)u[1] & 0xff) << 8) |
				(((__u32)u[2] & 0xff) << 16) |
				(((__u32)u[3] & 0xff) << 24);
	}
	else
		return 0xffffffff;
}
static inline __u16 port_stons(const char *s)
{
    uint32_t u;
	if(sscanf(s, "%u", &u) == 1)
	{
		return (uint16_t)htons(u);
	}
	else
		return 0xffff;
}
static inline __u16 port_stohs(const char *s)
{
	int u[2];
	if(sscanf(s, "%d%d", &u[0], &u[1]) == 2)
	{
		return  (((__u16)u[1] & 0xff)) |
				(((__u16)u[0] & 0xff) << 8);
	}
	else
		return 0xffff;
}

static inline bool is_u8(const char *s)
{
	__u8 u;
	if(sscanf(s, "%c", &u) == 1)
		return true;
	else
		return false;
}

static inline bool is_u32(const char *s)
{
	__u32 u;
	if(sscanf(s, "%u", &u) == 1)
		return true;
	else
		return false;
}

static inline bool is___u16(const char *s)
{
	__u16 u;
	if(sscanf(s, "%u", &u) == 1)
		return true;
	else
		return false;
}

static inline bool is_ipv4_addr(const char *s)
{
	int u[4];
	if(sscanf(s, "%d.%d.%d.%d", &u[0], &u[1], &u[2], &u[3]) == 4)
		return true;
	else
		return false;
}
#if 0
static inline void init_list_entry(struct list_head *entry)
{
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}

static inline int list_entry_orphan(struct list_head *entry)
{
	return entry->next == LIST_POISON1;
}
#endif
static int is_trim(char c,const char *sp)
{
    int i;
    int sp_len = strlen(sp);
    if (isspace(c)||c == '\n'|| c == '\r')
        return 1;
    for (i = 0; i < sp_len; i ++) {
        if (c == sp[i])
            return 1;
    }
    return 0;
}

static inline char *trim_specific(char *s, const char *sp)
{
    int i = 0,cp_point = 0;
    char * c = s + strlen(s) - 1;
    char * cpst;

    while (is_trim(*c, sp) && c >= s)
    {
        *c = '\0';
        --c;
    }

    c = s;
    while(*c != '\0')
    {
        if(is_trim(*c, sp))
        {
            i++;
        }
        else
        {
            break;
        }
        c++;
    }

    if(i != 0 )
    {
        cpst = s + i;
        while(*cpst != '\0')
        {
            *(s + cp_point) = *cpst;
            cp_point++;
            cpst++;
        }
        *(s + cp_point) = '\0';
    }
    return s;
}

static inline bool ch_isspace(char c)
{
    return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}
static inline void trim_spaces(char *str)
{
    char *p1, *p2, c;
    size_t len;

    if((len = strlen(str)) == 0)
        return;

    /* Determine start position. */
    for(p1 = str; (c = *p1); p1++)
    {
        if(!ch_isspace(c))
            break;
    }

    /* Determine ending position. */
    for(p2 = str + len; p2 > p1 && (c = *(p2 - 1)); p2--)
    {
        if(!ch_isspace(c))
            break;
    }

    /* Move string ahead, and put new terminal character. */
    memmove(str, p1, (size_t)(p2 - p1));
    str[(size_t)(p2 - p1)] = '\0';
}
static inline bool __ch_isspace(char c)
{
    return c == ' ' || c == '\r' || c == '\n' || c == '\t'||c == '['||
            c == ']'|| c == '{' || c == '}';
}
static inline void _strim(char *str)
{
    char *p1, *p2, c;
    size_t len;

    if((len = strlen(str)) == 0)
        return;

    /* Determine start position. */
    for(p1 = str; (c = *p1); p1++)
    {
        if(!__ch_isspace(c))
            break;
    }

    /* Determine ending position. */
    for(p2 = str + len; p2 > p1 && (c = *(p2 - 1)); p2--)
    {
        if(!__ch_isspace(c))
            break;
    }

    /* Move string ahead, and put new terminal character. */
    memmove(str, p1, (size_t)(p2 - p1));
    str[(size_t)(p2 - p1)] = '\0';
}
static inline bool __ch_spec(char c)
{
    return c == '\r' || c == '\n' || c == '\t'||c == '['||
            c == ']'|| c == '{' || c == '}';
}

static inline void strim_except_space(char *str)
{
    char *p1, *p2, c;
    size_t len;

    if((len = strlen(str)) == 0)
        return;

    /* Determine start position. */
    for(p1 = str; (c = *p1); p1++)
    {
        if(!__ch_spec(c))
            break;
    }

    /* Determine ending position. */
    for(p2 = str + len; p2 > p1 && (c = *(p2 - 1)); p2--)
    {
        if(!__ch_spec(c))
            break;
    }

    /* Move string ahead, and put new terminal character. */
    memmove(str, p1, (size_t)(p2 - p1));
    str[(size_t)(p2 - p1)] = '\0';
}


static inline int nocase_cmp(int key1, int key2)
{
        if (((key1 >= 97 && key1 <= 122) || (key1 >= 65 && key1 <= 90)) \
        &&((key2 >= 97 && key2 <= 122) || (key2 >= 65 && key2 <= 90))) {
                switch (key1 - key2){
                        case 0:
                        case 32:
                        case -32:
                                return 0;
                        default:
                                return 1;

                }

        }
        return (key1 != key2);


}

static inline void nocase_next_index(const char *str, int len, int next[])
{

        int i = 0, j = -1;

        next[i] = -1;

        while ( i < len) {
                if ( j == -1 || !(nocase_cmp(str[i] ,str[j]))) {
                        j ++;
                        i ++;
                        if (nocase_cmp(str[i], str[j])) {
                                next[i] = j;
                        } else {
                                next[i] = next[j];
                        }
                } else {

                        j = next[j];
                }

        }
}
static inline int substr_in_mainstr_nocase(const char *mstr,int mlen, const char *pstr, char meta)
{
    int i = 0, j = 0;
    int plen = strlen(pstr);

    int next[plen];
    nocase_next_index(pstr, plen, next);
    if (meta == 0) {
        while (i < mlen && j < plen) {
                if (!nocase_cmp(mstr[i], pstr[j])) {
                        i ++;
                        j ++;
                } else {
                        if (next[j] != -1)
                                j = next[j];
                        else {
                                j = 0;
                                i ++;
                        }
                }

        }

    } else {
        while (i < mlen && j < plen) {
                if (!nocase_cmp(mstr[i], meta))
                    break;
                if (!nocase_cmp(mstr[i], pstr[j])) {
                        i ++;
                        j ++;
                } else {
                        if (next[j] != -1)
                                j = next[j];
                        else {
                                j = 0;
                                i ++;
                        }
                }

        }


    }
    if (j >= plen)
        //return i - plen;
        return i;
    return -1;
}

static inline int cmp(uint8_t key1, uint8_t key2)
{
    return (key1 != key2);
}

static inline void next_index(const char *str, int len, int next[])
{

        int i = 0, j = -1;

        next[i] = -1;

        while ( i < len) {
                if ( j == -1 || !(cmp(str[i] ,str[j]))) {
                        j ++;
                        i ++;
                        if (cmp(str[i], str[j])) {
                                next[i] = j;
                        } else {
                                next[i] = next[j];
                        }
                } else {

                        j = next[j];
                }

        }
}
static inline int substr_in_mainstr(const char *mstr, int mlen, const char *pstr, char meta)
{
    int i = 0, j = 0;
    int plen = strlen(pstr);

    int next[plen];
    next_index(pstr, plen, next);
    if (meta == 0) {
        while (i < mlen && j < plen) {
                if (!cmp(mstr[i], pstr[j])) {
                        i ++;
                        j ++;
                } else {
                        if (next[j] != -1)
                                j = next[j];
                        else {
                                j = 0;
                                i ++;
                        }
                }

        }

    } else {
        while (i < mlen && j < plen) {
                if (mstr[i] == meta)
                    break;
                if (!cmp(mstr[i], pstr[j])) {
                        i ++;
                        j ++;
                } else {
                        if (next[j] != -1)
                                j = next[j];
                        else {
                                j = 0;
                                i ++;
                        }
                }

        }


    }
        if (j >= plen)
                return i;
        return -1;
}
static inline int subhex_in_mainhex(const char *mstr, int mlen, const char *pstr, const int plen, char meta)
{
    int i = 0, j = 0;

    int next[plen];
    next_index(pstr, plen, next);
    if (meta == 0) {
        while (i < mlen && j < plen) {
                if (!cmp(mstr[i], pstr[j])) {
                        i ++;
                        j ++;
                } else {
                        if (next[j] != -1)
                                j = next[j];
                        else {
                                j = 0;
                                i ++;
                        }
                }

        }

    } else {
        while (i < mlen && j < plen) {
//                if (mstr[i] == meta)
//                    break;
                if (!cmp(mstr[i], pstr[j])) {
                        i ++;
                        j ++;
                } else {
                        if (next[j] != -1)
                                j = next[j];
                        else {
                                j = 0;
                                i ++;
                        }
                }

        }


    }
        if (j >= plen)
                return i;
        return -1;
}


static inline char *str_trim(char *s)
{
        int i = 0,cp_point = 0;
        char * c = s + strlen(s) - 1;
        char * cpst;

        while ((isspace(*c) ||*c == '\n'|| *c == '\r'|| *c == '\t' ) && c >= s)
        {
                *c = '\0';
                --c;
        }

        c = s;
        while(*c != '\0')
        {
                if(isspace(*c)||*c == '\n'|| *c == '\r'||*c == '\t')
                {
                        i++;
                }
                else
                {
                        break;
                }
                c++;
        }

        if(i != 0 )
        {
                cpst = s + i;
                while(*cpst != '\0')
                {
                        *(s + cp_point) = *cpst;
                        cp_point++;
                        cpst++;
                }
                *(s + cp_point) = '\0';
        }
    return s;
}
static inline int strcmp_nocase(char * src, char * mark)
{
    int i;
    int ret = 0;
    char s;
    char m;

    if (src == NULL || mark == NULL)
    {
        return -1;
    }

    for (i = 0;;i++)
    {
        s = tolower(*(src + i));
        m = tolower(*(mark + i));
        if (s > m)
        {
            ret = i + 1;
            break;
        }
        else if (s < m)
        {
            ret = 0 - i - 1;
            break;
        }

        if (s == 0 || m == 0)
        {
            break;
        }
    }
    return ret;
}
static inline int strncmp_nocase(char * src, char * mark, int len)
{
    int i;
    int ret = 0;
    char s;
    char m;

    if (src == NULL || mark == NULL)
    {
        return -1;
    }

    for (i = 0; i < len; i++)
    {
        s = tolower(*(src + i));
        m = tolower(*(mark + i));
        if (s > m)
        {
            ret = i + 1;
            break;
        }
        else if (s < m)
        {
            ret = 0 - i - 1;
            break;
        }

        if (s == 0 || m == 0)
        {
            break;
        }
    }
    return ret;
}
static inline int sft_memmem(const unsigned char *src, int slen, const char *mark, int mark_len)
{
    int i = 0;
    int j = 0;
    int sign = 0;

    if (mark_len > slen || src == NULL || mark == NULL || slen <= 0)
    {
        return -1;
    }

    for (i = 0; i <= slen - mark_len; i++)
    {
        for (j = 0; j < mark_len; j++)
        {
            if (*(src + i + j) != mark[j])
            {
                sign = 1;
                break;
            }
            else
            {
                sign = 0;
            }
        }
        if (!sign)
        {
            return i;
        }
    }
    return -1;
}
static inline int str2ip(char *address)
{
    struct in_addr ad;
    memset(&ad, 0x00, sizeof(ad));
    inet_aton(address, &ad);
    return(ntohl(ad.s_addr));
}
static inline size_t
get_vlan_offset(struct ether_hdr *eth_hdr, uint16_t *proto)
{
    size_t vlan_offset = 0;


#if 1
    if (0x81 == *proto) {
        struct vlan_hdr *vlan_hdr = (struct vlan_hdr *)(eth_hdr + 1);
        vlan_offset += sizeof(struct vlan_hdr);
        *proto = vlan_hdr->eth_proto;

        while (0x81 == *proto) {
            vlan_hdr = vlan_hdr + 1;
            *proto = vlan_hdr->eth_proto;
            vlan_offset += sizeof(struct vlan_hdr);
        }
    }
#else
    if (rte_cpu_to_be_16(ETHER_TYPE_VLAN) == *proto) {
        struct vlan_hdr *vlan_hdr = (struct vlan_hdr *)(eth_hdr + 1);

        vlan_offset = sizeof(struct vlan_hdr);
        *proto = vlan_hdr->eth_proto;

        if (rte_cpu_to_be_16(ETHER_TYPE_VLAN) == *proto) {
            vlan_hdr = vlan_hdr + 1;
            *proto = vlan_hdr->eth_proto;
            vlan_offset += sizeof(struct vlan_hdr);
        }
    }
#endif
    return vlan_offset;
}

static void urldecode(char *p, int len)  
{  
    register i=0;  
    while(*(p+i)) 
    {  
        if ((*p=*(p+i)) == '%')  
        {  
            *p=*(p+i+1) >= 'A' ? ((*(p+i+1) & 0XDF) - 'A') + 10 : (*(p+i+1) - '0');  
            *p=(*p) * 16;  
            *p+=*(p+i+2) >= 'A' ? ((*(p+i+2) & 0XDF) - 'A') + 10 : (*(p+i+2) - '0');  
            i+=2;  
        }  
        else if (*(p+i)=='+')  
        {  
            *p=' ';  
        }  
        p++;  
    }  
    *p='\0';  
}  

#define smp_processor_id() rte_lcore_id()
#endif /* __UTILS_H */
