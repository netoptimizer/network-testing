/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>, (C)2014
 * License: GPLv2
 * From: https://github.com/netoptimizer/network-testing
 *
 * Burn CPU time
 *  - To workaround kernel issue where softirq doesn't get sched accounted
 *
 * Example usage:
 *  nice -20 taskset -c 2 ./burn_cpu
 */
#define _GNU_SOURCE /* needed for getopt.h */
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

#include "global.h"

static const struct option long_options[] = {
	{"help",	no_argument,		NULL, 'h' },
	{0, 0, NULL,  0 }
};

int burn_cpu()
{
	for (;;) {
	}
}

/* TODO: add function that also touch memory. E.g. do a memset(ptr,
 * 'X' , size) in the loop to not only burn cpu cycles, but force some
 * part of CPU cache (or all of it) to be stressed .
 */

static int usage(char *argv[])
{
	int i;

	printf("-= ERROR: Parameter problems =-\n");
	printf(" Usage: %s (options-see-below)\n", argv[0]);
	printf(" Listing options:\n");
	for (i = 0; long_options[i].name != 0; i++) {
		printf(" --%s", long_options[i].name);
		if (long_options[i].flag != NULL)
			printf("\t\t flag (internal value:%d)",
			       *long_options[i].flag);
		else
			printf("\t\t short-option: -%c",
			       long_options[i].val);
		printf("\n");
	}
	printf("\n");

	return EXIT_FAIL_OPTION;
}


int main(int argc, char *argv[])
{
	int longindex = 0;
	int c;

	/* Parse commands line args */
	while ((c = getopt_long(argc, argv, "h",
				long_options, &longindex)) != -1) {
		if (c == 'h' || c == '?') return usage(argv);
	}

	burn_cpu();
}
