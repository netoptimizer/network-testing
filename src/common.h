/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>, (C)2014
 * License: GPLv2
 * From: https://github.com/netoptimizer/network-testing
 *
 * Common/shared helper functions
 */
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

extern int verbose;

inline uint64_t rdtsc()
{
	uint32_t low, high;
	asm volatile("rdtsc" : "=a" (low), "=d" (high));
	return low  | (((uint64_t )high ) << 32);
}

unsigned long long gettime(void);

char *malloc_payload_buffer(int msg_sz);

/* Using __builtin_constant_p(x) to ignore cases where the return
 * value is always the same.
 */
# ifndef likely
#  define likely(x)	(__builtin_constant_p(x) ? !!(x) : __builtin_expect((x),1))
# endif
# ifndef unlikely
#  define unlikely(x)	(__builtin_constant_p(x) ? !!(x) : __builtin_expect((x),0))
# endif


static inline uint32_t locked_cmpxchg(uint32_t *dst, uint32_t old, uint32_t new)
{
	volatile uint32_t *ptr = (volatile uint32_t *)dst;
	uint32_t ret;

	asm volatile("lock; cmpxchgl %2, %1"
		     : "=a" (ret), "+m" (*ptr)
		     : "r" (new), "0" (old)
		     : "memory");

	return ret;
}

static inline uint32_t unlocked_cmpxchg(uint32_t *dst, uint32_t old, uint32_t new)
{
	volatile uint32_t *ptr = (volatile uint32_t *)dst;
	uint32_t ret;

	asm volatile("cmpxchgl %2, %1"
		     : "=a" (ret), "+m" (*ptr)
		     : "r" (new), "0" (old)
		     : "memory");

	return ret;
}

/* xchg code based on LTTng */
struct __uatomic_dummy {
	unsigned long v[10];
};
#define __hp(x)	((struct __uatomic_dummy *)(x))

static inline unsigned int implicit_locked_xchg(void *addr, unsigned long val)
{
	unsigned int result;

	/* Note: the "xchg" instruction does not need a "lock" prefix,
	 * because it is implicit lock prefixed
	 */
	asm volatile("xchgl %0, %1"
		     : "=r"(result), "+m"(*__hp(addr))
		     : "0" ((unsigned int)val)
		     : "memory");

	return result;
}


int time_func(int loops,
	      int (*func)(int loops, uint64_t* tsc_begin, uint64_t* tsc_end,
			  uint64_t* time_begin, uint64_t* time_end)
	);

#endif /* COMMON_H */
