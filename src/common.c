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
#include <string.h> /* memset */

#include "global.h"

int verbose = 1;

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

/* Allocate payload buffer */
char *malloc_payload_buffer(int msg_sz)
{
	char * msg_buf = malloc(msg_sz);

	if (!msg_buf) {
		fprintf(stderr, "ERROR: %s() failed in malloc() (caller: 0x%p)\n",
			__func__, __builtin_return_address(0));
		exit(EXIT_FAIL_MEM);
	}
	memset(msg_buf, 0, msg_sz);
	if (verbose)
		fprintf(stderr, " - malloc(msg_buf) = %d bytes\n", msg_sz);
	return msg_buf;
}
