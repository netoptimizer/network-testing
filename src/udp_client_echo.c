/* -*- c-file-style: "linux" -*- */

/* TODO:
 * IPv6 UDP client that expects an echo reply of its own packet
 *
 * TODO:
 *  - Set socket options to "encourage" fragmentation
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

#define PORT 4040 /* Default port, change with option "-p" */
static volatile int verbose = 1;

int main(int argc, char *argv[])
{
	int sockfd;
	int size = 3000;
	int opt;
	int addr_family = AF_INET6; /* Default address family */
	uint16_t dest_port = PORT;
	int res;
	char *dest_ip;

	/* Adding support for both IPv4 and IPv6 */
	struct sockaddr_storage dest_addr; /* Can contain both sockaddr_in and sockaddr_in6 */
	struct sockaddr_in  *dest_addr_v4; /* Pointer for IPv4 type casting */
	struct sockaddr_in6 *dest_addr_v6; /* Pointer for IPv6 type casting */
	memset(&dest_addr, 0, sizeof(dest_addr));

	while ((opt = getopt(argc, argv, "s:64v:p:")) != -1) {
		if (opt == 's') size = atoi(optarg);
		if (opt == '4') addr_family = AF_INET;
		if (opt == '6') addr_family = AF_INET6;
		if (opt == 'v') verbose = atoi(optarg);
		if (opt == 'p') dest_port = atoi(optarg);
	}
	if (optind >= argc) {
		fprintf(stderr, "Expected dest IP-address (IPv6 or IPv4) argument after options\n");
		exit(2);
	}
	dest_ip = argv[optind];
	if (verbose > 0)
		printf("Destination IP:%s port:%d\n", dest_ip, dest_port);

	/* Setup dest_addr depending on IPv4 or IPv6 address */
	if (addr_family == AF_INET6) {
		dest_addr_v6 = (struct sockaddr_in6*) &dest_addr;
		dest_addr_v6->sin6_family= addr_family;
		dest_addr_v6->sin6_port  = htons(dest_port);
		res = inet_pton(AF_INET6, dest_ip, &dest_addr_v6->sin6_addr);
	} else if (addr_family == AF_INET) {
		dest_addr_v4 = (struct sockaddr_in*) &dest_addr;
		dest_addr_v4->sin_family = addr_family;
		dest_addr_v4->sin_port   = htons(dest_port);
		res = inet_pton(AF_INET, dest_ip, &(dest_addr_v4->sin_addr));
	} else {
		fprintf(stderr, "ERROR: Unsupported addr_family\n");
		exit(3);
	}
	if (res <= 0) {
		if (res == 0)
			fprintf(stderr, "ERROR: IP \"%s\"not in presentation format\n", dest_ip);
		else
			perror("inet_pton");
		exit(4);
	}


}
