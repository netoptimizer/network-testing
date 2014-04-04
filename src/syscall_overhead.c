/* Program to measure the system call overhead
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

#include "global.h"
#include "common.h"

#define LOOPS    100000000

int loop_syscall_getuid(
	int loops, uint64_t* tsc_begin, uint64_t* tsc_end,
	uint64_t* time_begin, uint64_t* time_end)
{
	int i;

	*time_begin = gettime();
	*tsc_begin  = rdtsc();
	for (i = 0; i < loops; i++) {
		getuid();
	}
	*tsc_end  = rdtsc();
	*time_end = gettime();
	return i;
}

int main()
{
	printf("Measuring syscall getuid:\n");
	time_func(LOOPS, loop_syscall_getuid);

	return 0;
}
