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

#define LOOPS 100000000

int loop_cmpxchg_A(int loops, uint64_t* tsc_begin, uint64_t* tsc_end,
		   uint64_t* time_begin, uint64_t* time_end)
{
	int i;
	uint32_t data;
	uint32_t res;

	*time_begin = gettime();
	*tsc_begin  = rdtsc();
	for (i = 0; i < loops; i++) {
		res = cmpxchg(&data, 0, 0);
	}
	*tsc_end = rdtsc();
	*time_end = gettime();
	/* Using res to make GCC not give a "set but not used" warning */
	data = res;
	return i;
}

int time_func(int loops,
	      int (*func)(int loops, uint64_t* tsc_begin, uint64_t* tsc_end,
			  uint64_t* time_begin, uint64_t* time_end)
	)
{
	uint64_t tsc_begin, tsc_end, tsc_interval;
	uint64_t time_begin, time_end, time_interval;
	double calls_per_sec, ns_per_call, timesec;
	uint64_t tsc_cycles;
	int loops_cnt;

	/*** Loop function being timed ***/
	loops_cnt = func(loops, &tsc_begin, &tsc_end, &time_begin, &time_end);

	tsc_interval  = tsc_end - tsc_begin;
	time_interval = time_end - time_begin;

	if ((loops =! loops_cnt))
		printf(" WARNING: Loop count(%d) not equal to loops(%d)\n",
		       loops_cnt, loops);

	/* Stats */
	calls_per_sec = loops_cnt / ((double)time_interval / NANOSEC_PER_SEC);
	tsc_cycles    = tsc_interval / loops_cnt;
	ns_per_call   = ((double)time_interval / loops_cnt);
	timesec       = ((double)time_interval / NANOSEC_PER_SEC);

	printf(" Per call: %lu cycles(tsc) %.2f ns\n"
	       "  - %.2f calls per sec (measurement periode time:%.2f sec)\n"
	       "  - (loop count:%d tsc_interval:%lu)\n",
	       tsc_cycles, ns_per_call, calls_per_sec, timesec,
	       loops_cnt, tsc_interval);

	return 0;
}

int main()
{
	printf("Measuring cmpxchg:\n");
	time_func(LOOPS, loop_cmpxchg_A);

	return 0;
}
