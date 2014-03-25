/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>, (C)2014
 * License: GPLv2
 * From: https://github.com/netoptimizer/network-testing
 *
 * Common/shared helper functions
 *
 */
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "global.h"

/* Time code based on:
 *  https://github.com/dterei/Scraps/tree/master/c/time
 *
 * Results
 *  time (sec) => 4ns
 *  ftime (ms) => 39ns
 *  gettimeofday (us) => 30ns
 *  clock_gettime (ns) => 26ns (CLOCK_REALTIME)
 *  clock_gettime (ns) => 8ns (CLOCK_REALTIME_COARSE)
 *  clock_gettime (ns) => 26ns (CLOCK_MONOTONIC)
 *  clock_gettime (ns) => 9ns (CLOCK_MONOTONIC_COARSE)
 *  clock_gettime (ns) => 170ns (CLOCK_PROCESS_CPUTIME_ID)
 *  clock_gettime (ns) => 154ns (CLOCK_THREAD_CPUTIME_ID)
 *  cached_clock (sec) => 0ns
 */

/* gettime returns the current time of day in nanoseconds. */
uint64_t gettime(void)
{
	struct timespec t;
	int res;

	res = clock_gettime(CLOCK_MONOTONIC, &t);
	if (res < 0) {
		fprintf(stderr, "error with gettimeofday! (%i)\n", res);
		exit(EXIT_FAIL_TIME);
	}

	return (uint64_t) t.tv_sec * NANOSEC_PER_SEC + t.tv_nsec;
}
