/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>, (C)2014-2020
 * License: GPLv2
 * From: https://github.com/netoptimizer/network-testing
 *
 */
#ifndef ASM_X86_H
#define ASM_X86_H

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

#endif
