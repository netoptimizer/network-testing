/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>, (C)2014
 * License: GPLv2
 * From: https://github.com/netoptimizer/network-testing
 *
 * Common socket related helper functions
 */
#ifndef COMMON_SOCKET_H
#define COMMON_SOCKET_H

/* Wrapper functions with error handling like "Stevens" */
int Socket(int addr_family, int type, int protocol);
int Setsockopt (int fd, int level, int optname, const void *optval, socklen_t optlen);

#endif /* COMMON_SOCKET_H */
