/* -*- c-file-style: "linux" -*-
 *
 * Simple UDP sender program
 *
 * Copyright: Eric Dumazet <eric.dumazet@gmail.com>, (C)2016
 * License: GPLv2
 *
 * Quick and dirty codey. Wrote when tracking the UDP v6 checksum bug
 * (4f2e4ad56a65f3b7d64c258e373cb71e8d2499f4 net: mangle zero checksum
 * in skb_checksum_help()), because netperf sends the same message
 * over and over...
 *
 *  Use -d 2   to remove the ip_idents_reserve() overhead.
 */
#define _GNU_SOURCE

#include <errno.h>
#include <error.h>
#include <linux/errqueue.h>
#include <netinet/in.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

char buffer[1400];

int main(int argc, char** argv) {
	int fd, i;
	struct sockaddr_in6 addr;
	char *host = NULL;
	int family = AF_INET6;
	int discover = -1;

	while ((i = getopt(argc, argv, "4H:d:")) != -1) {
		switch (i) {
		case 'H': host = optarg; break;
		case '4': family = AF_INET; break;
		case 'd': discover = atoi(optarg); break;
		}
	}
	if (!host) {
		fprintf(stderr, "Please specify destination host\n");
		fprintf(stderr, "(Notice default uses IPv6)\n");
		fprintf(stderr, " Usage: %s -H host <-d pmtu> <-4>\n",
				argv[0]);
		return 2;
	}
	fd = socket(family, SOCK_DGRAM, 0);
	if (fd < 0)
		error(1, errno, "failed to create socket");
	if (discover != -1)
		setsockopt(fd, SOL_IP, IP_MTU_DISCOVER,
               &discover, sizeof(discover));

	memset(&addr, 0, sizeof(addr));
	if (family == AF_INET6) {
		addr.sin6_family = AF_INET6;
		addr.sin6_port = htons(9);
		inet_pton(family, host, (void *)&addr.sin6_addr.s6_addr);
	} else {
		struct sockaddr_in *in = (struct sockaddr_in *)&addr;
		in->sin_family = family;
		in->sin_port = htons(9);
		inet_pton(family, host, &in->sin_addr);
	}
	connect(fd, (struct sockaddr *)&addr,
		(family == AF_INET6) ? sizeof(addr) :
		sizeof(struct sockaddr_in));
	memset(buffer, 1, 1400);
	for (i = 0; i < 655360000; i++) {
		memcpy(buffer, &i, sizeof(i));
		send(fd, buffer, 100 + rand() % 200, 0);
	}
	return 0;
}
