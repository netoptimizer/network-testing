/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>, (C)2014
 * License: GPLv2
 * From: https://github.com/netoptimizer/network-testing
 *
 * UDP flood program
 *  for testing performance of different send system calls
 *
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/udp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>

#define _GNU_SOURCE
#include <getopt.h>

#include "global.h"
#include "common.h"
#include "common_socket.h"

static int verbose = 1;
#define NANOSEC_PER_SEC 1000000000 /* 10^9 */

static int flood_with_sendto(int sockfd, struct sockaddr_storage *dest_addr,
			     int count, int msg_sz)
{
	char *msg_buf;
	int cnt, res;
	socklen_t addrlen = sockaddr_len(dest_addr);

	/* Allocate message buffer */
	msg_buf = malloc(msg_sz);
	if (!msg_buf) {
		fprintf(stderr, "ERROR: %s() failed in malloc()", __func__);
		exit(EXIT_FAIL_MEM);
	}
	memset(msg_buf, 0, msg_sz);

	/* Flood loop */
	for (cnt = 0; cnt < count; cnt++) {
		res = sendto(sockfd, msg_buf, msg_sz, 0,
			     (struct sockaddr *) dest_addr, addrlen);
		if (res < 0) {
			perror("- sendto");
			goto out;
		}
	}
	res = cnt;

out:
	free(msg_buf);
	return res;
}


int main(int argc, char *argv[])
{
	int sockfd, c;
	uint64_t tsc_begin, tsc_end, tsc_interval;
	int cnt_send;
	double pps;
	int nanosecs;

	/* Default settings */
	int addr_family = AF_INET; /* Default address family */
	int count = 1000000;
	int msg_sz = 32; /* 32 + 8(UDP) + 20(IP) + 4(Eth-CRC) = 64 bytes */
	uint16_t dest_port = 6666;
	char *dest_ip;

	/* Support for both IPv4 and IPv6 */
	struct sockaddr_storage dest_addr; /* Can contain both sockaddr_in and sockaddr_in6 */
	memset(&dest_addr, 0, sizeof(dest_addr));

	/* Parse commands line args */
	while ((c = getopt(argc, argv, "c:l:64v:")) != -1) {
		if (c == 'c') count       = atoi(optarg);
		if (c == 'p') dest_port   = atoi(optarg);
		if (c == 'm') msg_sz      = atoi(optarg);
		if (c == '4') addr_family = AF_INET;
		if (c == '6') addr_family = AF_INET6;
		if (c == 'v') verbose     = atoi(optarg);
	}
	if (optind >= argc) {
		fprintf(stderr, "Expected dest IP-address (IPv6 or IPv4) argument after options\n");
		exit(EXIT_FAIL_OPTION);
	}
	dest_ip = argv[optind];
	if (verbose > 0)
		printf("Destination IP:%s port:%d\n", dest_ip, dest_port);

	/* Socket setup stuff */
//	sockfd = Socket(addr_family, SOCK_DGRAM, IPPROTO_IP);
	sockfd = Socket(addr_family, SOCK_DGRAM, 0);

	/* Setup dest_addr depending on IPv4 or IPv6 address */
	setup_sockaddr(addr_family, &dest_addr, dest_ip, dest_port);

	/* Connect to recv ICMP error messages, and to avoid the
	 * kernel performing connect/unconnect cycles
	 */
	Connect(sockfd, (struct sockaddr *)&dest_addr, sockaddr_len(&dest_addr));

	tsc_begin = rdtsc();
	cnt_send = flood_with_sendto(sockfd, &dest_addr, count, msg_sz);
	tsc_end = rdtsc();
	tsc_interval = tsc_end - tsc_begin;

	if (cnt_send < 0) {
		fprintf(stderr, "ERROR: failed to send packets\n");
		close(sockfd);
		exit(EXIT_FAIL_SEND);
	}

	/* Stats */
	pps      = cnt_send / ((double)tsc_interval / NANOSEC_PER_SEC);
	nanosecs = tsc_interval / cnt_send;
	printf("TSC cycles(%d) per packet: %llu nanosec (pkts send:%d) %.2f pps\n",
	       tsc_interval, nanosecs, cnt_send, pps);


	close(sockfd);
}
