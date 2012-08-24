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

void setup_sockaddr(int addr_family, struct sockaddr_storage *addr,
		   char *ip_str, uint16_t port)
{
	struct sockaddr_in  *addr_v4; /* Pointer for IPv4 type casting */
	struct sockaddr_in6 *addr_v6; /* Pointer for IPv6 type casting */
	int res;

	/* Setup sockaddr depending on IPv4 or IPv6 address */
	if (addr_family == AF_INET6) {
		addr_v6 = (struct sockaddr_in6*) addr;
		addr_v6->sin6_family= addr_family;
		addr_v6->sin6_port  = htons(port);
		res = inet_pton(AF_INET6, ip_str, &addr_v6->sin6_addr);
	} else if (addr_family == AF_INET) {
		addr_v4 = (struct sockaddr_in*) addr;
		addr_v4->sin_family = addr_family;
		addr_v4->sin_port   = htons(port);
		res = inet_pton(AF_INET, ip_str, &(addr_v4->sin_addr));
	} else {
		fprintf(stderr, "ERROR: Unsupported addr_family\n");
		exit(3);
	}
	if (res <= 0) {
		if (res == 0)
			fprintf(stderr, "ERROR: IP \"%s\"not in presentation format\n", ip_str);
		else
			perror("inet_pton");
		exit(4);
	}
}

socklen_t sockaddr_len(const struct sockaddr_storage *sockaddr)
{
	socklen_t len_addr = 0;
	switch (sockaddr->ss_family) {
	case AF_INET:
		len_addr = sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		len_addr = sizeof(struct sockaddr_in6);
		break;
	default:
		fprintf(stderr, "ERROR: %s(): Cannot determine lenght of addr_family(%d)",
                        __func__, sockaddr->ss_family);
		exit(4);
	}
	return len_addr;
}

int send_packet(int sockfd, const struct sockaddr_storage *dest_addr,
		    uint16_t pkt_size)
{
	socklen_t len_addr = sockaddr_len(dest_addr);
	char buf[65535];
	int len;
	len = sendto(sockfd, buf, pkt_size, 0, (const struct sockaddr*) dest_addr, len_addr);
	if (len < 0) {
		fprintf(stderr, "ERROR: %s() sendto failed (%d)\n", __func__, len);
		perror("sendto");
		exit(5);
	}
	if (verbose > 0) {
		printf("%s(): Send UDP packet of length:%d\n", __func__, len);
	}
}

int main(int argc, char *argv[])
{
	int sockfd;
	int pkt_size = 3000;
	int opt;
	int addr_family = AF_INET6; /* Default address family */
	uint16_t dest_port = PORT;
	char *dest_ip;

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
		exit(2);
	}
	dest_ip = argv[optind];
	if (verbose > 0)
		printf("Destination IP:%s port:%d\n", dest_ip, dest_port);

	/* Setup dest_addr depending on IPv4 or IPv6 address */
	setup_sockaddr(addr_family, &dest_addr, dest_ip, dest_port);

	sockfd = socket(addr_family, SOCK_DGRAM, 0);

	send_packet(sockfd, &dest_addr, pkt_size);
}
