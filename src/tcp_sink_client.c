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
#include <errno.h>

#include "global.h"
#include "common.h"
#include "common_socket.h"

/* Whether to close connections immediately.  Not setting this can be
 * used for stressing TCP to keep state longer.
 */
static int close_conn = 1;

static struct option long_options[] = {
	{"ipv4",	no_argument,		NULL, '4' },
	{"ipv6",	no_argument,		NULL, '6' },
	{"port",	required_argument,	NULL, 'p' },
	{"sport",	required_argument,	NULL, 's' },
	{"source-port",	required_argument,	NULL, 's' },
	{"verbose",	optional_argument,	NULL, 'v' },
	{"quiet",	no_argument,		&verbose, 0 },
	{"no-close",	no_argument,		&close_conn, 0 },
	{0, 0, NULL,  0 }
};

static int usage(char *argv[])
{
	printf("-= ERROR: Parameter problems =-\n");
	printf(" Usage: %s [-c count] [-p port] [-4] [-6] [-v] IP-addr\n\n",
	       argv[0]);
	return EXIT_FAIL_OPTION;
}

/* Force using a specific source port number for connection */
static void bind_source_port(int addr_family, int sockfd, uint16_t src_port)
{
	struct sockaddr_storage sock_addr;
	int val = 1;

	memset(&sock_addr, 0, sizeof(sock_addr));

	if (addr_family == AF_INET) {
		struct sockaddr_in *addr4 = (struct sockaddr_in *)&sock_addr;
		addr4->sin_family      = addr_family;
		addr4->sin_port        = htons(src_port);
		addr4->sin_addr.s_addr = htonl(INADDR_ANY);
	} else if (addr_family == AF_INET6) {
		struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&sock_addr;
		addr6->sin6_family = addr_family;
		addr6->sin6_port   = htons(src_port);
	}

	/* If several conn, allow to re-bind to same addr */
	Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
	//Setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));

	Bind(sockfd, &sock_addr);
}

int connect_retries(int sockfd, struct sockaddr_storage *dest_addr,
		    int max_retries)
{
	int res, retries = 0;

retry:
	res = connect(sockfd, (struct sockaddr *)dest_addr,
			      sockaddr_len(dest_addr));
	if (res >= 0)
		return res;

	if (++retries > max_retries) {
		fprintf(stderr, "ERROR: exceed conn attempts (%d) errno(%d) ",
			retries, errno);
		perror("- connect");
		close(sockfd);
		exit(EXIT_FAIL_SOCK);
	}

	/* Error handling, knowing res == -1 */
	switch (errno) {
	case EADDRNOTAVAIL: /* 99 */
		/* Can happen due to fast re-bind re-connect cycle */
		fprintf(stderr, "RETRY(%d): re-connect() errno(%d) ",
			retries, errno);
		perror("- connect");
		goto retry;
		break;
	case ECONNRESET: /* 104 */
		/* Usually happens due to SO_REUSEPORT listen errors,
		 * or conn reset during 3WHS.
		 */
		fprintf(stderr, "ERROR: Likely SO_REUSEPORT failed errno(%d) ",
			errno);
		perror("- connect");
		close(sockfd);
		exit(EXIT_FAIL_REUSEPORT);
		break;
	default:
		fprintf(stderr, "ERROR: connect() failed errno(%d) ", errno);
		perror("- connect");
		close(sockfd);
		exit(EXIT_FAIL_SOCK);
	}

	return res;
}

int main(int argc, char *argv[])
{
	int sockfd;
	int i, c, longindex = 0;
	char *dest_ip;

	/* Default settings */
	int addr_family = AF_INET; /* Default address family */
	uint16_t dest_port = 6666;
	uint16_t src_port = 0; /* Allow to "force" source port */
	int count = 100;

	/* Support for both IPv4 and IPv6.
	 *  sockaddr_storage: Can contain both sockaddr_in and sockaddr_in6
	 */
	struct sockaddr_storage dest_addr;
	memset(&dest_addr, 0, sizeof(dest_addr));

	/* Parse commands line args */
	while ((c = getopt_long(argc, argv, "c:p:s:64v:",
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
		if (c == 's') src_port    = atoi(optarg);
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
		sockfd = Socket(addr_family, SOCK_STREAM, IPPROTO_TCP);

		if (src_port > 0)
			bind_source_port(addr_family, sockfd, src_port);

		connect_retries(sockfd, &dest_addr, 2);

		if (close_conn)
			Close(sockfd);
	}

	return 0;
}
