/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>, (C)2014
 * License: GPLv2
 * From: https://github.com/netoptimizer/network-testing
 *
 * UDP sink program
 *  for testing performance of different receive system calls
 *
 */

#define _GNU_SOURCE /* needed for struct mmsghdr and getopt.h */
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <linux/udp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/uio.h> /* struct iovec */
#include <errno.h>

#include <getopt.h>

//#include "syscalls.h"
#include <linux/unistd.h>       /* for _syscallX macros/related stuff */

#include "global.h"
#include "common.h"
#include "common_socket.h"

static int usage(char *argv[])
{
	printf("-= ERROR: Parameter problems =-\n");
	printf(" Usage: %s [-c count] [-l listen_port] [-4] [-6] [-v]\n\n",
	       argv[0]);
	return EXIT_FAIL_OPTION;
}

static int sink_with_read(int sockfd, int count, int batch) {
	int i, res;
	uint64_t total = 0;
	int buf_sz = 4096;
	char *buffer = malloc_payload_buffer(buf_sz);

	for (i = 0; i < count; i++) {
		res = read(sockfd, buffer, buf_sz);
		if (res < 0)
			goto error;
		total += res;
	}
	if (verbose > 0)
		printf(" - read %lu bytes in %d packets\n", total, i);

	free(buffer);
	return i;

 error: /* ugly construct to make sure the loop is small */
	fprintf(stderr, "ERROR: %s() failed (%d) errno(%d) ",
		__func__, res, errno);
	perror("- read");
	free(buffer);
	close(sockfd);
	exit(EXIT_FAIL_SOCK);
}

static int sink_with_recvfrom(int sockfd, int count, int batch) {
	int i, res;
	uint64_t total = 0;
	int buf_sz = 4096;
	char *buffer = malloc_payload_buffer(buf_sz);

	for (i = 0; i < count; i++) {
		res = recvfrom(sockfd, buffer, buf_sz, 0, NULL, NULL);
		if (res < 0)
			goto error;
		total += res;
	}
	if (verbose > 0)
		printf(" - read %lu bytes in %d packets = %lu bytes payload\n",
		       total, i, total / i);

	free(buffer);
	return i;

 error: /* ugly construct to make sure the loop is small */
	fprintf(stderr, "ERROR: %s() failed (%d) errno(%d) ",
		__func__, res, errno);
	perror("- recvfrom");
	free(buffer);
	close(sockfd);
	exit(EXIT_FAIL_SOCK);
}


static int sink_with_recvmsg(int sockfd, int count, int batch) {
	int i, res;
	uint64_t total = 0;
	int buf_sz = 4096;
	char *buffer = malloc_payload_buffer(buf_sz);
	struct msghdr *msg_hdr;  /* struct for setting up transmit */
	struct iovec  *msg_iov;  /* io-vector: array of pointers to payload data */
	unsigned int iov_array_elems = batch; /* test scattered payload */

	msg_hdr = malloc_msghdr();               /* Alloc msghdr setup structure */
	msg_iov = malloc_iovec(iov_array_elems); /* Alloc I/O vector array */

	/*** Setup packet structure for receiving ***/
	/* The senders info is stored here but we don't care, so use NULL */
	msg_hdr->msg_name    = NULL;
	msg_hdr->msg_namelen = 0;
	/* Setup io-vector pointers for receiving payload data */
	msg_iov[0].iov_base = buffer;
	msg_iov[0].iov_len  = buf_sz;
	/* The io-vector supports scattered payload data, below add a simpel
	 * testcase with dst payload, adjust iov_array_elems > 1 to activate code
	 */
	for (i = 1; i < iov_array_elems; i++) {
		msg_iov[i].iov_base = buffer;
		msg_iov[i].iov_len  = buf_sz;
	}
	/* Binding io-vector to packet setup struct */
	msg_hdr->msg_iov    = msg_iov;
	msg_hdr->msg_iovlen = iov_array_elems;

	/* Having several IOV's does not help much. The return value
	 * of recvmsg is the total packet size.  It can be split out
	 * on several IOVs, only if the buffer size of the first IOV
	 * is too small.
	 */

	/* Receive LOOP */
	for (i = 0; i < count; i++) {
		res = recvmsg(sockfd, msg_hdr, 0);
		if (res < 0)
			goto error;
		total += res;
	}
	if (verbose > 0)
		printf(" - read %lu bytes in %d packets = %lu bytes payload\n",
		       total, i, total / i);

	free(msg_iov);
	free(msg_hdr);
	free(buffer);
	return i;

 error: /* ugly construct to make sure the loop is small */
	fprintf(stderr, "ERROR: %s() failed (%d) errno(%d) ",
		__func__, res, errno);
	perror("- recvmsg");
	free(buffer);
	close(sockfd);
	exit(EXIT_FAIL_SOCK);
}

/*
 For understanding 'recvmmsg' / mmsghdr data structures
 ======================================================

 int recvmmsg(int sockfd, struct mmsghdr *msgvec, unsigned int vlen,
              unsigned int flags, struct timespec *timeout);

	struct mmsghdr {
		struct msghdr msg_hdr;  // Message header
		unsigned int  msg_len;  // Number of received bytes
	};
*/

static int sink_with_recvMmsg(int sockfd, int count, int batch) {
	int cnt, i, res, pkt;
	uint64_t total = 0, packets;
	int buf_sz = 4096;
	char *buffer = malloc_payload_buffer(buf_sz);
	struct iovec  *msg_iov;  /* io-vector: array of pointers to payload data */
	unsigned int iov_array_elems = 1; /* test scattered payload */

	/* struct *mmsghdr -  pointer to an array of mmsghdr structures.
	 *   *** Notice: double "m" in mmsghdr ***
	 * Allows the caller to transmit multiple messages on a socket
	 * using a single system call
	 */
	struct mmsghdr *mmsg_hdr;

	mmsg_hdr = malloc_mmsghdr(batch);         /* Alloc mmsghdr array */
	msg_iov  = malloc_iovec(iov_array_elems); /* Alloc I/O vector array */

	count = count / batch;

	/*** Setup packet structure for receiving
	 ***/
	/* Setup io-vector pointers for receiving payload data */
	msg_iov[0].iov_base = buffer;
	msg_iov[0].iov_len  = buf_sz;
	/* The io-vector supports scattered payload data, below add a simpel
	 * testcase with dst payload, adjust iov_array_elems > 1 to activate code
	 */
	for (i = 1; i < iov_array_elems; i++) {
		msg_iov[i].iov_base = buffer;
		msg_iov[i].iov_len  = buf_sz;
	}

	for (pkt = 0; pkt < batch; pkt++) {
		/* The senders info is stored here but we don't care, so use NULL */
		mmsg_hdr[pkt].msg_hdr.msg_name    = NULL;
		mmsg_hdr[pkt].msg_hdr.msg_namelen = 0;
		/* Binding io-vector to packet setup struct */
		mmsg_hdr[pkt].msg_hdr.msg_iov    = msg_iov;
		mmsg_hdr[pkt].msg_hdr.msg_iovlen = iov_array_elems;
	}

	/* Receive LOOP */
	for (cnt = 0; cnt < count; cnt++) {
		res = recvmmsg(sockfd, mmsg_hdr, batch, 0, NULL);
//		res = syscall(__NR_recvmmsg, sockfd, mmsg_hdr, batch, 0, NULL);
		if (res < 0)
			goto error;
		for (pkt = 0; pkt < batch; pkt++)
			total += mmsg_hdr[pkt].msg_len;
	}
	packets = cnt * batch;
	if (verbose > 0)
		printf(" - read %lu bytes in %lu packets= %lu bytes payload (loop %d)\n",
		       total, packets, total / packets, cnt);

	free(msg_iov);
	free(mmsg_hdr);
	free(buffer);
	return packets;

 error: /* ugly construct to make sure the loop is small */
	fprintf(stderr, "ERROR: %s() failed (%d) errno(%d) ",
		__func__, res, errno);
	perror("- recvmsg");
	free(msg_iov);
	free(mmsg_hdr);
	free(buffer);
	close(sockfd);
	exit(EXIT_FAIL_SOCK);
}



static void time_function(int sockfd, int count, int repeat, int batch,
			  int (*func)(int sockfd, int count, int batch))
{
	uint64_t tsc_begin,  tsc_end,  tsc_interval, tsc_cycles;
	uint64_t time_begin, time_end, time_interval;
	int cnt_recv, j;
	double pps, ns_per_pkt, timesec;
	#define TMPMAX 4096
	char buffer[TMPMAX];
	int res;

	//WAIT on first packet of flood
	printf(" - Waiting on first packet (of expected flood)\n");
	res = read(sockfd, buffer, TMPMAX);
	if (res < 0) {
		fprintf(stderr, "ERROR: %s() failed (%d) errno(%d) ",
			__func__, res, errno);
		perror("- read");
		close(sockfd);
		exit(EXIT_FAIL_SOCK);
	}
	printf("  * Got first packet (starting timing)\n");

	for (j = 0; j < repeat; j++) {
		printf(" Test run: %d (expecting to receive %d pkts)\n",
		       j, count);

		time_begin = gettime();
		tsc_begin  = rdtsc();
		cnt_recv = func(sockfd, count, batch);
		tsc_end  = rdtsc();
		time_end = gettime();
		tsc_interval  = tsc_end  - tsc_begin;
		time_interval = time_end - time_begin;

		if (cnt_recv < 0) {
			fprintf(stderr, "ERROR: failed to send packets\n");
			close(sockfd);
			exit(EXIT_FAIL_RECV);
		}

		/* Stats */
		pps        = cnt_recv / ((double)time_interval / NANOSEC_PER_SEC);
		tsc_cycles = tsc_interval / cnt_recv;
		ns_per_pkt = ((double)time_interval / cnt_recv);
		timesec    = ((double)time_interval / NANOSEC_PER_SEC);
		printf(" - Per packet: %lu cycles(tsc) %.2f ns, %.2f pps (time:%.2f sec)\n"
		       "   (packet count:%d tsc_interval:%lu)\n",
		       tsc_cycles, ns_per_pkt, pps, timesec,
		       cnt_recv, tsc_interval);
	}
}

int main(int argc, char *argv[])
{
	int sockfd, c;
	int count  = 1000000;
	int repeat = 2;

	/* Default settings */
	int addr_family = AF_INET; /* Default address family */
	uint16_t listen_port = 6666;

	/* Support for both IPv4 and IPv6 */
	struct sockaddr_storage listen_addr; /* Can contain both sockaddr_in and sockaddr_in6 */

	/* Parse commands line args */
	while ((c = getopt(argc, argv, "c:r:l:64v:")) != -1) {
		if (c == 'c') count       = atoi(optarg);
		if (c == 'r') repeat      = atoi(optarg);
		if (c == 'l') listen_port = atoi(optarg);
		if (c == '4') addr_family = AF_INET;
		if (c == '6') addr_family = AF_INET6;
		if (c == 'v') verbose     = atoi(optarg);
		if (c == '?') return usage(argv);
	}

	if (verbose > 0)
		printf("Listen port %d\n", listen_port);

	/* Socket setup stuff */
	sockfd = Socket(addr_family, SOCK_DGRAM, IPPROTO_IP);

	/* Setup listen_addr depending on IPv4 or IPv6 address */
	//setup_sockaddr(addr_family, &listen_addr, dest_ip, dest_port);
	memset(&listen_addr, 0, sizeof(listen_addr));
	if (addr_family == AF_INET) {
		struct sockaddr_in *addr4 = (struct sockaddr_in *)&listen_addr;
		addr4->sin_family = addr_family;
		addr4->sin_port   = htons(listen_port);
	} else if (addr_family == AF_INET6) {
		struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&listen_addr;
		addr6->sin6_family= addr_family;
		addr6->sin6_port  = htons(listen_port);
	}

	Bind(sockfd, &listen_addr);

	printf("\nPerformance of: recvMmsg() batch:32\n");
	time_function(sockfd, count, repeat, 32, sink_with_recvMmsg);

	printf("\nPerformance of: recvmsg()\n");
	time_function(sockfd, count, repeat, 1, sink_with_recvmsg);

	printf("\nPerformance of: read()\n");
	time_function(sockfd, count, repeat, 0, sink_with_read);

	printf("\nPerformance of: recvfrom()\n");
	time_function(sockfd, count, repeat, 0, sink_with_recvfrom);

	close(sockfd);
}
