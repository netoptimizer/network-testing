/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>, (C)2014-2016
 * License: GPLv2
 * From: https://github.com/netoptimizer/network-testing
 */
static char *__doc__=
 " This tool is a UDP sink that measures the incoming packet rate,\n"
 " it expects a continuous flow of UDP packets (up to --count per test).\n"
 " Default it cycles through different ways/function-calls to\n"
 " receive packets.  What function-call to invoke can also be\n"
 " specified as a command line option (see below)\n"
 ;

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
#include <stdbool.h>

#include <getopt.h>

//#include "syscalls.h"
#include <linux/unistd.h>       /* for _syscallX macros/related stuff */

#include "global.h"
#include "common.h"
#include "common_socket.h"

#define RUN_RECVMSG   0x1
#define RUN_RECVMMSG  0x2
#define RUN_RECVFROM  0x4
#define RUN_READ      0x8
#define RUN_ALL       (RUN_RECVMSG | RUN_RECVMMSG | RUN_RECVFROM | RUN_READ)

static const struct option long_options[] = {
	/* keep recv functions grouped together */
	{"read",	no_argument,		NULL, 'T' },
	{"recvfrom",	no_argument,		NULL, 't' },
	{"recvmsg",	no_argument,		NULL, 'u' },
	{"recvmmsg",	no_argument,		NULL, 'U' },
	/* Other options */
	{"help",	no_argument,		NULL, 'h' },
	{"ipv4",	no_argument,		NULL, '4' },
	{"ipv6",	no_argument,		NULL, '6' },
	{"reuse-port",	no_argument,		NULL, 's' },
	{"batch",	required_argument,	NULL, 'b' },
	{"count",	required_argument,	NULL, 'c' },
	{"port",	required_argument,	NULL, 'l' },
	{"payload",	required_argument,	NULL, 'm' },
	{"repeat",	required_argument,	NULL, 'r' },
	{"verbose",	optional_argument,	NULL, 'v' },
	{"connect",	optional_argument,	NULL, 'C' },
	{0, 0, NULL,  0 }
};

#define DEFAULT_COUNT 1000000

static int usage(char *argv[])
{
	int i;
	printf("\nDOCUMENTATION:\n%s\n", __doc__);
	printf(" Default receives %d packets per test, adjust via --count\n",
	       DEFAULT_COUNT);
	printf("\n");
	printf(" Usage: %s (options-see-below)\n",
	       argv[0]);
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
	printf("\n Multiple tests can be selected:\n");
	printf("     default: all tests\n");
	printf("     -u -U -t -T: run any combination of"
			" recvmsg/recvmmsg/recvfrom/read\n");
	printf("\n");

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
			  bool do_connect,
			  int (*func)(int sockfd, int count, int batch))
{
	uint64_t tsc_begin,  tsc_end,  tsc_interval, tsc_cycles;
	uint64_t time_begin, time_end, time_interval;
	char from_ip[INET6_ADDRSTRLEN] = {0}; /* Assume max IPv6 */
	int str_max = sizeof(from_ip);
	int cnt_recv, j;
	double pps, ns_per_pkt, timesec;
	#define TMPMAX 4096
	char buffer[TMPMAX];
	int res;

	/* Support for both IPv4 and IPv6.
	 * "storage" can contain both sockaddr_in and sockaddr_in6
	 */
	struct sockaddr_storage src_store;
	struct sockaddr *src = (struct sockaddr *)&src_store;
	socklen_t addrlen = sizeof(src_store); /* updated by recvfrom */
	struct sockaddr_in  *ipv4 = NULL;
	struct sockaddr_in6 *ipv6 = NULL;
	__be16 src_port = 0;
	void *addr_ptr = NULL;
	int flags = 0;

	/* WAIT on first packet of flood */
	if (verbose)
		printf(" - Waiting on first packet (of expected flood)\n");

	/* Using recvfrom to get remote src info for connect() */
	res = recvfrom(sockfd, buffer, TMPMAX, flags, src, &addrlen);
	if (res < 0) {
		perror("- read");
		goto socket_error;
	}
	switch (src->sa_family) {
	case AF_INET:
		ipv4 = (struct sockaddr_in *)src;
		addr_ptr = (void *)&ipv4->sin_addr;
		src_port = ipv4->sin_port;
		break;
	case AF_INET6:
		ipv6 = (struct sockaddr_in6 *)src;
		addr_ptr = (void *)&ipv6->sin6_addr;
		src_port = ipv6->sin6_port;
		break;
	default:
		fprintf(stderr, "ERROR: %s() "
			"unsupported sa_family(%d) from socket errno(%d)\n",
			__func__, src->sa_family, errno);
		close(sockfd);
		exit(EXIT_FAIL_RECV);
	}
	if (!inet_ntop(src->sa_family, addr_ptr, from_ip, str_max)) {
		perror("- inet_ntop");
		goto socket_error;
	}
	if (verbose && !do_connect)
		printf("  * Got first packet from IP:port %s:%d\n",
		       from_ip, ntohs(src_port));

	if (do_connect) {
		if (verbose)
			printf("  * Connect UDP sock to src IP:port %s:%d\n",
			       from_ip, ntohs(src_port));
		Connect(sockfd, src, addrlen);
	}

	for (j = 0; j < repeat; j++) {
		if (verbose) {
			printf(" Test run: %d (expecting to receive %d pkts)\n",
			       j, count);
		} else {
			printf("run: %d %d\t", j, count);
		}

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
		print_result(tsc_cycles, ns_per_pkt, pps, timesec,
			     cnt_recv, tsc_interval);
	}
	return;

socket_error:
	fprintf(stderr, "ERROR: %s() failed (%d) errno(%d) ",
		__func__, res, errno);
	close(sockfd);
	exit(EXIT_FAIL_SOCK);
}

int main(int argc, char *argv[])
{
	int sockfd, c;
	int count  = DEFAULT_COUNT;
	int repeat = 1;
	int so_reuseport = 0;

	/* Default settings */
	int addr_family = AF_INET; /* Default address family */
	uint16_t listen_port = 6666;
	bool do_connect = 0;
	int longindex = 0;
	int run_flag = 0;
	int batch = 32;

	/* Support for both IPv4 and IPv6 */
	struct sockaddr_storage listen_addr; /* Can contain both sockaddr_in and sockaddr_in6 */

	/* Parse commands line args */
	while ((c = getopt_long(argc, argv, "hc:r:l:64sCv:tTuUb:",
				long_options, &longindex)) != -1) {
		if (c == 'c') count       = atoi(optarg);
		if (c == 'r') repeat      = atoi(optarg);
		if (c == 'b') batch       = atoi(optarg);
		if (c == 'l') listen_port = atoi(optarg);
		if (c == '4') addr_family = AF_INET;
		if (c == '6') addr_family = AF_INET6;
		if (c == 's') so_reuseport= 1;
		if (c == 'C') do_connect  = 1;
		if (c == 'v') verbose     = optarg ? atoi(optarg) : 1;
		if (c == 'u') run_flag   |= RUN_RECVMSG;
		if (c == 'U') run_flag   |= RUN_RECVMMSG;
		if (c == 't') run_flag   |= RUN_RECVFROM;
		if (c == 'T') run_flag   |= RUN_READ;
		if (c == 'h' || c == '?') return usage(argv);
	}

	if (verbose > 0)
		printf("Listen port %d\n", listen_port);

	if (run_flag == 0)
		run_flag = RUN_ALL;

	/* Socket setup stuff */
//	sockfd = Socket(addr_family, SOCK_DGRAM, IPPROTO_IP);
	sockfd = Socket(addr_family, SOCK_DGRAM, IPPROTO_UDP);

	/* Enable use of SO_REUSEPORT for multi-process testing  */
	if (so_reuseport) {
		if ((setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT,
				&so_reuseport, sizeof(so_reuseport))) < 0) {
			    printf("ERROR: No support for SO_REUSEPORT\n");
			    perror("- setsockopt(SO_REUSEPORT)");
			    exit(EXIT_FAIL_SOCKOPT);
		}
	}

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

	if (run_flag & RUN_RECVMMSG) {
		print_header("recvMmsg", batch);
		time_function(sockfd, count, repeat, batch, do_connect,
			      sink_with_recvMmsg);
	}

	if (run_flag & RUN_RECVMSG) {
		print_header("recvmsg", 0);
		time_function(sockfd, count, repeat, 1, do_connect,
			      sink_with_recvmsg);
	}

	if (run_flag & RUN_READ) {
		print_header("read", 0);
		time_function(sockfd, count, repeat, 0, do_connect,
			      sink_with_read);
	}

	if (run_flag & RUN_RECVFROM) {
		print_header("recvfrom", 0);
		time_function(sockfd, count, repeat, 0, do_connect,
			      sink_with_recvfrom);
	}

	close(sockfd);
	return 0;
}
