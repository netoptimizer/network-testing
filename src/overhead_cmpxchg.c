/* Program to measure call overhead of cmpxchg
 *
 * Must be compiled with -O2 to make sure inlining happens
 *  gcc -O2 FILE.c -lrt
 *
 * Pin to a CPU if TSC is unsable across CPUs
 *  taskset -c 1 ./FILE
 */
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> /* exit(3) */

#include "global.h"
#include "common.h"

#define LOOPS 100000000 * 10

int loop_cmpxchg(int loops, uint64_t* tsc_begin, uint64_t* tsc_end,
		 uint64_t* time_begin, uint64_t* time_end)
{
	int i;
	uint32_t data;
	uint32_t res;

	*time_begin = gettime();
	*tsc_begin  = rdtsc();
	for (i = 0; i < loops; i++) {
		res = unlocked_cmpxchg(&data, 0, 0);
	}
	*tsc_end  = rdtsc();
	*time_end = gettime();
	/* Using res to make GCC not give a "set but not used" warning */
	data = res;
	return i;
}

int loop_cmpxchg_locked(int loops, uint64_t* tsc_begin, uint64_t* tsc_end,
			uint64_t* time_begin, uint64_t* time_end)
{
	int i;
	uint32_t data;
	uint32_t res;

	*time_begin = gettime();
	*tsc_begin  = rdtsc();
	for (i = 0; i < loops; i++) {
		res = locked_cmpxchg(&data, 0, 0);
	}
	*tsc_end  = rdtsc();
	*time_end = gettime();
	/* Using res to make GCC not give a "set but not used" warning */
	data = res;
	return i;
}

int loop_xchg(int loops, uint64_t* tsc_begin, uint64_t* tsc_end,
		 uint64_t* time_begin, uint64_t* time_end)
{
	int i;
	uint32_t data;
	uint32_t res;

	*time_begin = gettime();
	*tsc_begin  = rdtsc();
	for (i = 0; i < loops; i++) {
		res = implicit_locked_xchg(&data, 0);
	}
	*tsc_end  = rdtsc();
	*time_end = gettime();
	/* Using res to make GCC not give a "set but not used" warning */
	data = res;
	return i;
}


int main()
{
	printf("Measuring unlocked cmpxchg:\n");
	time_func(LOOPS, loop_cmpxchg);

	printf("Measuring locked cmpxchg:\n");
	time_func(LOOPS, loop_cmpxchg_locked);

	printf("Measuring implicit locked xchg:\n");
	time_func(LOOPS, loop_xchg);

	return 0;
}
