/* -*- c-file-style: "linux" -*-
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

/* Err handle wrapper for setsockopt */
int Setsockopt (int fd, int level, int optname, const void *optval,
		socklen_t optlen)
{
	int res = setsockopt(fd, level, optname, optval, optlen);
	if (res < 0) {
		fprintf(stderr, "ERROR: %s() failed (%d) errno(%d) ",
			__func__, res, errno);
		perror("- setsockopt");
		exit(EXIT_FAIL_SOCK);
	}
	return res;
}
