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
