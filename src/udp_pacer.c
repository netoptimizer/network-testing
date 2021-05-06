/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>
 * License: GPLv2
 */
static const char *__doc__=
 " This tool is a UDP pacer that clock-out packets at fixed interval.\n";

#define _GNU_SOURCE /* needed for struct mmsghdr and getopt.h */
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <linux/udp.h>
#include <arpa/inet.h>

#include "global.h"
#include "common.h"
#include "common_socket.h"

static const struct option long_options[] = {
	{"help",	no_argument,		NULL, 'h' },
	{"verbose",	optional_argument,	NULL, 'v' },
	{"batch",	required_argument,	NULL, 'b' },
	{"count",	required_argument,	NULL, 'c' },
	{"port",	required_argument,	NULL, 'p' },
	{0, 0, NULL,  0 }
};

struct cfg_params {
	int batch;
	int count;
	int msg_sz;
	// int pmtu; /* Path MTU Discovery setting, affect DF bit */

	/* Below socket setup */
	int sockfd;
	int addr_family;    /* redundant: in dest_addr after setup_sockaddr */
	uint16_t dest_port; /* redundant: in dest_addr after setup_sockaddr */
	struct sockaddr_storage dest_addr; /* Support for both IPv4 and IPv6 */
};

static int usage(char *argv[])
{
	int i;

	printf("\nDOCUMENTATION:\n%s\n\n", __doc__);
	printf(" Usage: %s (options-see-below) IPADDR\n", argv[0]);
	printf(" Listing options:\n");
	for (i = 0; long_options[i].name != 0; i++) {
		printf(" --%-12s", long_options[i].name);
		if (long_options[i].flag != NULL)
			printf(" flag (internal value:%d)",
			       *long_options[i].flag);
		else
			printf(" short-option: -%c",
			       long_options[i].val);
		printf("\n");
	}
	return EXIT_FAIL_OPTION;
}

static int socket_send(int sockfd, struct cfg_params *p)
{
	char *msg_buf;
	int cnt, res = 0;
	int flags = 0;
	uint64_t total = 0;

	/* Allocate payload buffer */
	msg_buf = malloc_payload_buffer(p->msg_sz);

	/* Send a batch of the same packet  */
	for (cnt = 0; cnt < p->batch; cnt++) {
		res = send(sockfd, msg_buf, p->msg_sz, flags);
		if (res < 0) {
			fprintf(stderr, "Managed to send %d packets\n", cnt);
			perror("- send");
			goto out;
		}
		total += res;
	}
	res = cnt;
out:
	free(msg_buf);
	return res;
}

void setup_socket(struct cfg_params *p, char *dest_ip_string)
{
	/* Setup dest_addr - will exit prog on invalid input */
	setup_sockaddr(p->addr_family, &p->dest_addr,
		       dest_ip_string, p->dest_port);

	/* Socket setup stuff */
	p->sockfd = Socket(p->addr_family, SOCK_DGRAM, IPPROTO_UDP);

	// TODO: Do we need some setsockopt() ?

	/* Connect to recv ICMP error messages, and to avoid the
	 * kernel performing connect/unconnect cycles
	 */
	Connect(p->sockfd,
		(struct sockaddr *)&p->dest_addr,
		sockaddr_len(&p->dest_addr));

}

static void init_params(struct cfg_params *p)
{
	memset(p, 0, sizeof(struct cfg_params));
	p->count  = 30; // DEFAULT_COUNT
	p->batch = 32;
	p->msg_sz = 18; /* 18 +14(eth)+8(UDP)+20(IP)+4(Eth-CRC) = 64 bytes */
	p->addr_family = AF_INET; /* Default address family */
	p->dest_port = 6666;
}

int main(int argc, char *argv[])
{
	int c, longindex = 0;
	struct cfg_params p;
	char *dest_ip_str;

	init_params(&p); /* Default settings */

	/* Parse commands line args */
	while ((c = getopt_long(argc, argv, "h6c:p:m:v:b:",
				long_options, &longindex)) != -1) {
		if (c == 'c') p.count       = atoi(optarg);
		if (c == 'p') p.dest_port   = atoi(optarg);
		if (c == 'm') p.msg_sz      = atoi(optarg);
		if (c == 'b') p.batch       = atoi(optarg);
		if (c == '6') p.addr_family = AF_INET6;
		if (c == 'v') verbose     = optarg ? atoi(optarg) : 1;
		if (c == 'h' || c == '?') return usage(argv);
	}
	if (optind >= argc) {
		fprintf(stderr,
			"Expected dest IP-address argument after options\n");
		return usage(argv);
	}
	dest_ip_str = argv[optind];
	if (verbose > 0)
		printf("Destination IP:%s port:%d\n", dest_ip_str, p.dest_port);

	/* Setup socket - will exit prog on invalid input */
	setup_socket(&p, dest_ip_str);

	socket_send(p.sockfd, &p);

	return EXIT_OK;
}
