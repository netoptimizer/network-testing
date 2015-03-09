/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>, (C)2015
 * License: GPLv2
 * From: https://github.com/netoptimizer/network-testing
 *
 * TCP sink program
 *  - for testing effect of SO_REUSEPORT
 *
 * Program will simply TCP listen() on a port, and accept() any
 * connection, possibly (not-default) write something into the
 * connection, and the close() it quickly.
 *
 */

#define _GNU_SOURCE /* needed for getopt.h */
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <linux/tcp.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#include <getopt.h>

#include <linux/unistd.h>
#include <time.h>

#include "global.h"
#include "common.h"
#include "common_socket.h"

static struct option long_options[] = {
	{"ipv4",	no_argument,		NULL, '4' },
	{"ipv6",	no_argument,		NULL, '6' },
	{"listen-port",	required_argument,	NULL, 'l' },
	{"verbose",	optional_argument,	NULL, 'v' },
	{"reuseport",	no_argument,		NULL, 's' },
	{"write-back", 	no_argument,		NULL, 'w' },
	{0, 0, NULL,  0 }
};

static int usage(char *argv[])
{
	printf("-= ERROR: Parameter problems =-\n");
	printf(" Usage: %s [-c count] [-l listen_port] [-4] [-6] [-v] [-s] [-w]\n\n",
	       argv[0]);
	return EXIT_FAIL_OPTION;
}

int main(int argc, char *argv[])
{
	int listenfd, connfd;
	int option_index = 0;
	int c, i;
	int count  = 1000000;
	int so_reuseport = 0;
	int write_something = 0;
	pid_t pid = getpid();

	/* Default settings */
	int addr_family = AF_INET; /* Default address family */
	uint16_t listen_port = 6666;

	char send_buf[1024];
	time_t ticks;

	/* Support for both IPv4 and IPv6.
	 *  sockaddr_storage: Can contain both sockaddr_in and sockaddr_in6
	 */
	struct sockaddr_storage listen_addr;

	memset(&listen_addr, 0, sizeof(listen_addr));
	memset(send_buf, 0, sizeof(send_buf));

	/* Parse commands line args */
	while ((c = getopt_long(argc, argv, "c:l:64swv:",
			long_options, &option_index)) != -1) {
		if (c == 'c') count       = atoi(optarg);
		if (c == 'l') listen_port = atoi(optarg);
		if (c == '4') addr_family = AF_INET;
		if (c == '6') addr_family = AF_INET6;
		if (c == 's') so_reuseport= 1;
		if (c == 'w') write_something = 1;
		if (c == 'v') (optarg) ? verbose = atoi(optarg) : (verbose = 1);
		if (c == '?') return usage(argv);
	}

	if (verbose > 0)
		printf("IP%s TCP listen port %d PID:[%d]\n",
		       (addr_family == AF_INET6) ? "v6":"v4",
		       listen_port, pid);

	/* Socket setup stuff */
	listenfd = Socket(addr_family, SOCK_STREAM, IPPROTO_IP);

	/* Enable use of SO_REUSEPORT for multi-process testing  */
	if (so_reuseport) {
		if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT,
				&so_reuseport, sizeof(so_reuseport))) < 0) {
			printf("ERROR: No support for SO_REUSEPORT\n");
			perror("- setsockopt(SO_REUSEPORT)");
			exit(EXIT_FAIL_SOCKOPT);
		} else if (verbose) {
			printf(" - Enabled SO_REUSEPORT\n");
		}
	}

	/* Setup listen_addr depending on IPv4 or IPv6 address */
	//setup_sockaddr(addr_family, &listen_addr, "0.0.0.0", listen_port);
	if (addr_family == AF_INET) {
		struct sockaddr_in *addr4 = (struct sockaddr_in *)&listen_addr;
		addr4->sin_family      = addr_family;
		addr4->sin_port        = htons(listen_port);
		addr4->sin_addr.s_addr = htonl(INADDR_ANY);
	} else if (addr_family == AF_INET6) {
		struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&listen_addr;
		addr6->sin6_family= addr_family;
		addr6->sin6_port  = htons(listen_port);
	}

	Bind(listenfd, &listen_addr);

	/* Notice "backlog" limited by: /proc/sys/net/core/somaxconn */
	listen(listenfd, 1024);

	for (i=1; i <= count; i++) {

		/* In the call to accept(), the server is put to sleep
		 * and when for an incoming client request, the three
		 * way TCP handshake is complete, the function
		 * accept() wakes up and returns the socket descriptor
		 * representing the client socket.
		 *
		 * Thus, for fake SYN-floods this will not be woken-up.
		 */
		connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);

		if (write_something) {
			/* Send/write something back into the TCP stream */
			ticks = time(NULL);
			snprintf(send_buf, sizeof(send_buf),
				 "PID:[%5d] cnt:%d %.24s\r\n",
				 pid, i, ctime(&ticks));
			write(connfd, send_buf, strlen(send_buf));
		}

		close(connfd);
		if (verbose)
			printf("PID:[%5d] Connection count: %d\n", pid, i);

	}

	close(listenfd);
	return 0;
}
