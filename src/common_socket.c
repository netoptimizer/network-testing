/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>, (C)2014
 * License: GPLv2
 * From: https://github.com/netoptimizer/network-testing
 *
 * Common socket related helper functions
 *
 */
#include <sys/types.h>  /* POSIX.1-2001 does not require the inclusion */
#include <sys/socket.h> /* setsockopt(3) etc */
#include <netinet/in.h> /* sockaddr_in{,6} */
#include <stdio.h>      /* perror(3) and fprintf(3) */
#include <stdlib.h>     /* exit(3) */
#include <errno.h>

#include "global.h"
#include "common_socket.h"

/* Wrapper functions with error handling, for basic socket function, that
 * checks the error codes, and terminate the program with an error
 * msg.  This reduces code size and still do proper error checking.
 *
 * Using an uppercase letter, just like "Stevens" does.
 */

int Socket(int addr_family, int type, int protocol) {
	int n;

	if ((n = socket(addr_family, type, protocol)) < 0) {
		perror("- socket");
		exit(EXIT_FAIL_SOCK);
	}
	return n;
}

int Connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	int res = connect(sockfd, addr, addrlen);

	if (res < 0) {
		fprintf(stderr, "ERROR: %s() failed (%d) errno(%d) ",
			__func__, res, errno);
		perror("- connect");
		close(sockfd);
		exit(EXIT_FAIL_SOCK);
	}
	return res;
}

int Setsockopt (int fd, int level, int optname, const void *optval,
		socklen_t optlen)
{
	int res = setsockopt(fd, level, optname, optval, optlen);

	if (res < 0) {
		fprintf(stderr, "ERROR: %s() failed (%d) errno(%d) ",
			__func__, res, errno);
		perror("- setsockopt");
		exit(EXIT_FAIL_SOCKOPT);
	}
	return res;
}

/* Helpers */

/* Setup a sockaddr_in{,6} depending on IPv4 or IPv6 address */
void setup_sockaddr(int addr_family, struct sockaddr_storage *addr,
		    char *ip_string, uint16_t port)
{
	struct sockaddr_in  *addr_v4; /* Pointer for IPv4 type casting */
	struct sockaddr_in6 *addr_v6; /* Pointer for IPv6 type casting */
	int res;

	/* Setup sockaddr depending on IPv4 or IPv6 address */
	if (addr_family == AF_INET6) {
		addr_v6 = (struct sockaddr_in6*) addr;
		addr_v6->sin6_family= addr_family;
		addr_v6->sin6_port  = htons(port);
		res = inet_pton(AF_INET6, ip_string, &addr_v6->sin6_addr);
	} else if (addr_family == AF_INET) {
		addr_v4 = (struct sockaddr_in*) addr;
		addr_v4->sin_family = addr_family;
		addr_v4->sin_port   = htons(port);
		res = inet_pton(AF_INET, ip_string, &(addr_v4->sin_addr));
	} else {
		fprintf(stderr, "ERROR: Unsupported addr_family\n");
		exit(EXIT_FAIL_OPTION);
	}
	if (res <= 0) {
		if (res == 0)
			fprintf(stderr,	"ERROR: IP \"%s\" not in presentation format\n", ip_string);
		else
			perror("inet_pton");
		exit(EXIT_FAIL_IP);
	}
}

/* Generic IPv{4,6} sockaddr len/ sizeof */
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
		exit(EXIT_FAIL_SOCK);
	}
	return len_addr;
}
