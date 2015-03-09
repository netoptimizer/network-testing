/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>, (C)2015
 * License: GPLv2
 * From: https://github.com/netoptimizer/network-testing
 *
 * TCP client program for tcp_sink.c
 */

#define _GNU_SOURCE /* needed for getopt.h */
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <linux/tcp.h>
#include <getopt.h>
#include <unistd.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "global.h"
#include "common.h"
#include "common_socket.h"

static struct option long_options[] = {
	{"ipv4",	no_argument,		NULL, '4' },
	{"ipv6",	no_argument,		NULL, '6' },
	{"port",	required_argument,	NULL, 'p' },
	{"verbose",	optional_argument,	NULL, 'v' },
	{"quiet",	no_argument,		&verbose, 0 },
	{0, 0, NULL,  0 }
};

static int usage(char *argv[])
{
	printf("-= ERROR: Parameter problems =-\n");
	printf(" Usage: %s [-c count] [-p port] [-4] [-6] [-v] IP-addr\n\n",
	       argv[0]);
	return EXIT_FAIL_OPTION;
}

int main(int argc, char *argv[])
{
	int sockfd;
	int i, c, longindex = 0;
	char *dest_ip;

	/* Default settings */
	int addr_family = AF_INET; /* Default address family */
	uint16_t dest_port = 6666;
	int count = 100;

	/* Support for both IPv4 and IPv6.
	 *  sockaddr_storage: Can contain both sockaddr_in and sockaddr_in6
	 */
	struct sockaddr_storage dest_addr;
	memset(&dest_addr, 0, sizeof(dest_addr));

	/* Parse commands line args */
	while ((c = getopt_long(argc, argv, "c:p:64v:",
			long_options, &longindex)) != -1) {
		if (c == 0) { /* optional handling "flag" options */
			if (verbose) {
				printf("Flag option %s",
				       long_options[longindex].name);
				if (optarg) printf(" with arg %s", optarg);
				printf("\n");
			}
		}
		if (c == 'c') count       = atoi(optarg);
		if (c == 'p') dest_port   = atoi(optarg);
		if (c == '4') addr_family = AF_INET;
		if (c == '6') addr_family = AF_INET6;
		if (c == 'v') (optarg) ? verbose = atoi(optarg) : (verbose = 1);
		if (c == '?') return usage(argv);
	}
	if (optind >= argc) {
		fprintf(stderr, "Expected dest IP-address (IPv6 or IPv4)"
			" argument after options\n");
		return usage(argv);
	}
	dest_ip = argv[optind];
	if (verbose > 0)
		printf("Destination IP%s:%s port:%d\n",
		       (addr_family == AF_INET6) ? "v6":"v4",
		       dest_ip, dest_port);

	/*** Socket setup ***/
	setup_sockaddr(addr_family, &dest_addr, dest_ip , dest_port);

	for (i = 0; i < count; i++) {
		if (verbose)
			printf("count:%d\n", i);
		sockfd = Socket(addr_family, SOCK_STREAM, IPPROTO_IP);

		Connect(sockfd, (struct sockaddr *)&dest_addr,
			sockaddr_len(&dest_addr));

		close(sockfd);
	}

	return 0;
}
