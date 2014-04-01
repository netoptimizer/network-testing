/* Quick program to measure the system call overhead
 *
 * gcc -O2 -o syscall_overhead syscall_overhead.c -lrt
 *
 * Pin to a CPU if TSC is unsable across CPUs
 *  taskset -c 1 ./syscall_overhead
 *
 * Add wallclock measurement, look at
 *  http://stackoverflow.com/questions/6498972/faster-equivalent-of-gettimeofday
 *  https://github.com/dterei/Scraps/tree/master/c/time
 */
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> /* exit(3) */

#include <time.h>
#include <sys/time.h>

#define LOOPS    100000000
#define SYSCALLS LOOPS
#define NANOSEC_PER_SEC 1000000000 /* 10^9 */

inline uint64_t rdtsc()
{
	uint32_t low, high;
	asm volatile("rdtsc" : "=a" (low), "=d" (high));
	return low  | (((uint64_t )high ) << 32);
}

/* gettime returns the current time of day in nanoseconds. */
uint64_t gettime(void)
{
	struct timespec t;
	int res;

	res = clock_gettime(CLOCK_MONOTONIC, &t);
	if (res < 0) {
		fprintf(stderr, "error with gettimeofday! (%i)\n", res);
		exit(2);
	}
	return (uint64_t) t.tv_sec * NANOSEC_PER_SEC + t.tv_nsec;
}

int main()
{
	int i;
	uint64_t tsc_begin, tsc_end, tsc_interval;
	uint64_t time_begin, time_end, time_interval;
	double calls_per_sec, ns_per_call, timesec;
	int tsc_cycles;

	time_begin = gettime();
	tsc_begin  = rdtsc();
	for (i = 0; i < LOOPS; i++) {
		getuid();
	}
	tsc_end  = rdtsc();
	time_end = gettime();
	tsc_interval  = tsc_end - tsc_begin;
	time_interval = time_end - time_begin;

	/* Stats */
	calls_per_sec = SYSCALLS / ((double)time_interval / NANOSEC_PER_SEC);
	tsc_cycles   = tsc_interval / SYSCALLS;
	ns_per_call  = ((double)time_interval / SYSCALLS);
	timesec      = ((double)time_interval / NANOSEC_PER_SEC);

	printf("Per syscall: %llu cycles(tsc) %.2f ns\n"
	       "  - %.2f calls per sec (measurement periode time:%.2f sec)\n"
	       "  - (loop count:%d tsc_interval:%llu)\n",
	       tsc_cycles, ns_per_call, calls_per_sec, timesec,
	       SYSCALLS, tsc_interval);

	printf("TSC cycles: tsc_end(%llu)-tsc_begin(%llu) = %llu\n",
	       tsc_begin, tsc_end, tsc_interval);
	printf("TSC cycles per getuid() system call: %llu\n",
	       tsc_cycles);

	return 0;
}
