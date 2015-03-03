/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>, (C)2014
 * License: GPLv2
 * From: https://github.com/netoptimizer/network-testing
 *
 * IPv6 UDP client that expects an echo reply of its own packet
 *  - Set socket options to "encourage" fragmentation
 *
 * TODO:
 *  - Impl. recv message, but with a timeout option
 *  - Can we recv ICMP err messages?
 *  - Can we detect if GSO has been enabled? (this can be a problem with KVM)
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/udp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "global.h"
#include "common_socket.h"

#define PORT 4040 /* Default port, change with option "-p" */
static volatile int verbose = 2;

int send_packet(int sockfd, const struct sockaddr_storage *dest_addr,
		char *buf_send, uint16_t pkt_size)
{
	socklen_t len_addr = sockaddr_len(dest_addr);
	int len_send;
	/* -- Send packet -- */
	len_send = sendto(sockfd, buf_send, pkt_size, 0, (const struct sockaddr*) dest_addr, len_addr);
	if (len_send < 0) {
		fprintf(stderr, "ERROR: %s() sendto failed (%d) ", __func__, len_send);
		perror("- sendto");
		exit(5);
	}
	if (verbose > 1) {
		printf("Send UDP packet of length:%d\n", len_send);
	}
	return len_send;
}

int recv_packet(int sockfd, const struct sockaddr_storage *dest_addr,
		char *buf_recv, uint16_t pkt_size)
{
	/* TODO: use dest_addr for validating against from_addr */
	// socklen_t len_addr = sockaddr_len(dest_addr);
	// struct sockaddr *from_addr;

	int len_recv;

	/* -- Receive packet -- */
	if (verbose > 1) {
		printf("Waiting for recvfrom()\n");
	}
	len_recv = recvfrom(sockfd, buf_recv, pkt_size, 0, NULL, NULL);
	if (len_recv < 0) {
		perror("recvfrom");
		exit(5);
	}
	if (verbose > 1) {
		printf("Received UDP packet of length:%d\n", len_recv);
	}
	return len_recv;
}

void validate_packet(int len_send, int len_recv, char* buf_send, char* buf_recv)
{
	/* Verify message */
	if (len_recv != len_send) {
		fprintf(stderr, "ERROR - Packet validation failed: size check\n");
		exit(1);
	}
	printf("OK: valid size\n");
}

int main(int argc, char *argv[])
{
	int sockfd;
	int pkt_size = 3000;
	int opt;
	int addr_family = AF_INET6; /* Default address family */
	uint16_t dest_port = PORT;
	char *dest_ip;
	int len_send, len_recv;
	char buf_send[65535], buf_recv[65535];

	/* Adding support for both IPv4 and IPv6 */
	struct sockaddr_storage dest_addr; /* Can contain both sockaddr_in and sockaddr_in6 */
	memset(&dest_addr, 0, sizeof(dest_addr));

	while ((opt = getopt(argc, argv, "s:64v:p:")) != -1) {
		if (opt == 's') pkt_size = atoi(optarg);
		if (opt == '4') addr_family = AF_INET;
		if (opt == '6') addr_family = AF_INET6;
		if (opt == 'v') verbose = atoi(optarg);
		if (opt == 'p') dest_port = atoi(optarg);
	}
	if (optind >= argc) {
		fprintf(stderr, "Expected dest IP-address (IPv6 or IPv4) argument after options\n");
		exit(EXIT_FAIL_OPTION);
	}
	dest_ip = argv[optind];
	if (verbose > 0)
		printf("Destination IP%s: %s port:%d\n",
		       (addr_family == AF_INET6) ? "v6": "", dest_ip, dest_port);

	sockfd = Socket(addr_family, SOCK_DGRAM, 0);

	/* Socket options, see man-pages ip(7) and ipv6(7) */
	//int set_pmtu_disc = IP_PMTUDISC_DO; /* do PMTU = Don't Fragment */
	int set_pmtu_disc = IP_PMTUDISC_DONT; /* Allow fragments, dont do PMTU */
	Setsockopt(sockfd, IPPROTO_IP,   IP_MTU_DISCOVER,   &set_pmtu_disc, sizeof(int));
	if (addr_family == AF_INET6)
		Setsockopt(sockfd, IPPROTO_IPV6, IPV6_MTU_DISCOVER,
			   &set_pmtu_disc, sizeof(int));

	/* Setup dest_addr depending on IPv4 or IPv6 address */
	setup_sockaddr(addr_family, &dest_addr, dest_ip, dest_port);

	/* Connect to recv ICMP error messages */
	Connect(sockfd, (struct sockaddr *)&dest_addr, sockaddr_len(&dest_addr));

	len_send = send_packet(sockfd, &dest_addr, buf_send, pkt_size);
	len_recv = recv_packet(sockfd, &dest_addr, buf_recv, len_send);
	validate_packet(len_send, len_recv, buf_send, buf_recv);
	return 0;
}
