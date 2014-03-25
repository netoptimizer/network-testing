/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>, (C)2014
 * License: GPLv2
 * From: https://github.com/netoptimizer/network-testing
 *
 * UDP flood program
 *  for testing performance of different send system calls
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

#include <getopt.h>

//#include "syscalls.h"
#include <linux/unistd.h>       /* for _syscallX macros/related stuff */

#include "global.h"
#include "common.h"
#include "common_socket.h"

static int verbose = 1;

static int usage(char *argv[])
{
	printf("-= ERROR: Parameter problems =-\n", argv[0]);
	printf(" Usage: %s [-c count] [-p port] [-m payloadsize] [-4] [-6] [-v] IPADDR\n\n",
	       argv[0]);
	return EXIT_FAIL_OPTION;
}

/* Allocate payload buffer */
static char *malloc_payload_buffer(int msg_sz)
{
	char * msg_buf = malloc(msg_sz);

	if (!msg_buf) {
		fprintf(stderr, "ERROR: %s() failed in malloc() (caller: 0x%x)",
			__func__, __builtin_return_address(0));
		exit(EXIT_FAIL_MEM);
	}
	memset(msg_buf, 0, msg_sz);
	if (verbose)
		fprintf(stderr, " - malloc(msg_buf) = %d bytes\n", msg_sz);
	return msg_buf;
}

/* Allocate struct msghdr setup structure for sendmsg/recvmsg */
static struct msghdr *malloc_msghdr()
{
	struct msghdr *msg_hdr;
	unsigned int msg_hdr_sz = sizeof(*msg_hdr);

	msg_hdr = malloc(msg_hdr_sz);
	if (!msg_hdr) {
		fprintf(stderr, "ERROR: %s() failed in malloc() (caller: 0x%x)",
			__func__, __builtin_return_address(0));
		exit(EXIT_FAIL_MEM);
	}
	memset(msg_hdr, 0, msg_hdr_sz);
	if (verbose)
		fprintf(stderr, " - malloc(msg_hdr) = %d bytes\n", msg_hdr_sz);
	return msg_hdr;
}

/* Allocate vector array of struct mmsghdr pointers for sendmmsg/recvmmsg
 *  Notice: double "m" im mmsghdr
 */
static struct mmsghdr *malloc_mmsghdr(unsigned int array_elems)
{
	struct mmsghdr *mmsg_hdr_vec;
	unsigned int memsz;

	//memsz = sizeof(*mmsg_hdr_vec) * array_elems;
	memsz = sizeof(struct mmsghdr) * array_elems;
	mmsg_hdr_vec = malloc(memsz);
	if (!mmsg_hdr_vec) {
		fprintf(stderr, "ERROR: %s() failed in malloc() (caller: 0x%x)",
			__func__, __builtin_return_address(0));
		exit(EXIT_FAIL_MEM);
	}
	memset(mmsg_hdr_vec, 0, memsz);
	if (verbose)
		fprintf(stderr, " - malloc(mmsghdr[%d]) = %d bytes\n",
			array_elems, memsz);
	return mmsg_hdr_vec;
}

/* Allocate I/O vector array of struct iovec.
 * (The structure supports scattered payloads)
 */
static struct iovec *malloc_iovec(unsigned int iov_array_elems)
{
	struct iovec  *msg_iov;      /* io-vector: array of pointers to payload data */
	unsigned int  msg_iov_memsz; /* array memory size */

	msg_iov_memsz = sizeof(*msg_iov) * iov_array_elems;
	msg_iov = malloc(msg_iov_memsz);
	if (!msg_iov) {
		fprintf(stderr, "ERROR: %s() failed in malloc() (caller: 0x%x)",
			__func__, __builtin_return_address(0));
		exit(EXIT_FAIL_MEM);
	}
	memset(msg_iov, 0, msg_iov_memsz);
	if (verbose)
		fprintf(stderr, " - malloc(msg_iov[%d]) = %d bytes\n",
			iov_array_elems, msg_iov_memsz);
	return msg_iov;
}



static int flood_with_sendto(int sockfd, struct sockaddr_storage *dest_addr,
			     int count, int msg_sz)
{
	char *msg_buf;
	int cnt, res = 0;
	socklen_t addrlen = sockaddr_len(dest_addr);

	/* Allocate payload buffer */
	msg_buf = malloc_payload_buffer(msg_sz);

	/* Flood loop */
	for (cnt = 0; cnt < count; cnt++) {
		res = sendto(sockfd, msg_buf, msg_sz, 0,
			     (struct sockaddr *) dest_addr, addrlen);
		if (res < 0) {
			fprintf(stderr, "Managed to send %d packets\n", cnt);
			perror("- sendto");
			goto out;
		}
	}
	res = cnt;

out:
	free(msg_buf);
	return res;
}


/*
 For understanding 'sendmsg' data structures
 ===========================================
 Structure describing messages sent by  `sendmsg' and received by `recvmsg'.
 ---------
	struct msghdr
	{
		// -=- The sockaddr_in part -=-
		void *msg_name;		// Address to send to/receive from.
		socklen_t msg_namelen;	// Length of address data

		// -=- Pointers to payload part -=-
		struct iovec *msg_iov;	// Vector of data to send/receive into
		size_t msg_iovlen;	// Number of elements in the vector

		//
		void *msg_control;	// Ancillary data (eg BSD filedesc passing)
		size_t msg_controllen;	// Ancillary data buffer length.
					// !! The type should be socklen_t but the
					// definition of the kernel is incompatible
					// with this.
		int msg_flags;		// Flags on received message.
	};

 Structure for scatter/gather I/O"
 ---------
	struct iovec {
		void *iov_base;	// Pointer to data.
		size_t iov_len;	// Length of data.
	};
*/

static int flood_with_sendmsg(int sockfd, struct sockaddr_storage *dest_addr,
			      int count, int msg_sz)
{
	char          *msg_buf;  /* payload data */
	struct msghdr *msg_hdr;  /* struct for setting up transmit */
	struct iovec  *msg_iov;  /* io-vector: array of pointers to payload data */
	unsigned int  msg_hdr_sz;
	unsigned int  msg_iov_sz;
	unsigned int  iov_array_elems = 1; /*adjust to test scattered payload */
	int i;

	int cnt, res;
	socklen_t addrlen = sockaddr_len(dest_addr);

	msg_buf = malloc_payload_buffer(msg_sz); /* Alloc payload buffer */
	msg_hdr = malloc_msghdr();               /* Alloc msghdr setup structure */
	msg_iov = malloc_iovec(iov_array_elems); /* Alloc I/O vector array */

	/*** Setup packet structure for transmitting ***/

	/* The destination addr */
	msg_hdr->msg_name    = dest_addr;
	msg_hdr->msg_namelen = addrlen;

	/* Setup io-vector pointers to payload data */
	msg_iov[0].iov_base = msg_buf;
	msg_iov[0].iov_len  = msg_sz;
	/* The io-vector supports scattered payload data, below add a simpel
	 * testcase with same payload, adjust iov_array_elems > 1 to activate code
	 */
	for (i = 1; i < iov_array_elems; i++) {
		msg_iov[i].iov_base = msg_buf;
		msg_iov[i].iov_len  = msg_sz;
	}
	/* Binding io-vector to packet setup struct */
	msg_hdr->msg_iov    = msg_iov;
	msg_hdr->msg_iovlen = iov_array_elems;

	/* Flood loop */
	for (cnt = 0; cnt < count; cnt++) {
		res = sendmsg(sockfd, msg_hdr, 0);
		if (res < 0) {
			goto error;
		}
	}
	res = cnt;
	goto out;
error:
	/* Error case */
	fprintf(stderr, "Managed to send %d packets\n", cnt);
	perror("- sendmsg");
out:
	free(msg_iov);
	free(msg_hdr);
	free(msg_buf);
	return res;
}

/*
 For understanding 'sendmmsg' / mmsghdr data structures
 ======================================================

 int sendmmsg(int sockfd, struct mmsghdr *msgvec, unsigned int vlen,
              unsigned int flags);

	struct mmsghdr {
		struct msghdr msg_hdr;  // Message header
		unsigned int  msg_len;  // Number of bytes transmitted
	};
*/

/* Notice: double "m" in sendmmsg
 * - sending multible packet in one syscall
 */
static int flood_with_sendmmsg(int sockfd, struct sockaddr_storage *dest_addr,
			       int count, int msg_sz)
{
	char          *msg_buf;  /* payload data */
	struct iovec  *msg_iov;  /* io-vector: array of pointers to payload data */
	unsigned int  msg_hdr_sz;
	unsigned int  msg_iov_sz;
	unsigned int  iov_array_elems = 1; /*adjust to test scattered payload */
	unsigned int  burst = 32;
	int i;

	count = count / burst;

	/* struct *mmsghdr -  pointer to an array of mmsghdr structures.
	 *   *** Notice: double "m" in mmsghdr ***
	 * Allows the caller to transmit multiple messages on a socket
	 * using a single system call
	 */
	struct mmsghdr *mmsg_hdr;

	int cnt, res, pkt;
	socklen_t addrlen = sockaddr_len(dest_addr);

	msg_buf  = malloc_payload_buffer(msg_sz); /* Alloc payload buffer */
	mmsg_hdr = malloc_mmsghdr(burst);         /* Alloc mmsghdr array */
	msg_iov  = malloc_iovec(iov_array_elems); /* Alloc I/O vector array */

	/*** Setup packet structure for transmitting ***/

	/* Setup io-vector pointers to payload data */
	msg_iov[0].iov_base = msg_buf;
	msg_iov[0].iov_len  = msg_sz;
	/* The io-vector supports scattered payload data, below add a simpel
	 * testcase with same payload, adjust iov_array_elems > 1 to activate code
	 */
	for (i = 1; i < iov_array_elems; i++) {
		msg_iov[i].iov_base = msg_buf;
		msg_iov[i].iov_len  = msg_sz;
	}

	for (pkt = 0; pkt < burst; pkt++) {
		/* The destination addr */
		mmsg_hdr[pkt].msg_hdr.msg_name    = dest_addr;
		mmsg_hdr[pkt].msg_hdr.msg_namelen = addrlen;
		/* Binding io-vector to packet setup struct */
		mmsg_hdr[pkt].msg_hdr.msg_iov    = msg_iov;
		mmsg_hdr[pkt].msg_hdr.msg_iovlen = iov_array_elems;
	}

	/* Flood loop */
	for (cnt = 0; cnt < count; cnt++) {
//		res = sendmmsg(sockfd, mmsg_hdr, burst, 0);
		res = syscall(__NR_sendmmsg, sockfd, mmsg_hdr, burst, 0);

		if (res < 0) {
			goto error;
		}
	}
	res = cnt * burst;
	goto out;
error:
	/* Error case */
	fprintf(stderr, "Managed to send %d packets\n", cnt);
	perror("- sendMmsg");
out:
	free(msg_iov);
	free(mmsg_hdr);
	free(msg_buf);
	return res;
}


static void time_function(int sockfd, struct sockaddr_storage *dest_addr,
			  int count, int msg_sz,
	int (*func)(int sockfd, struct sockaddr_storage *dest_addr,
		    int count, int msg_sz))
{
	uint64_t tsc_begin,  tsc_end,  tsc_interval;
	uint64_t time_begin, time_end, time_interval;
	int cnt_send;
	double pps, ns_per_pkt, timesec;
	int tsc_cycles;

	time_begin = gettime();
	tsc_begin  = rdtsc();
	cnt_send = func(sockfd, dest_addr, count, msg_sz);
	//cnt_send = flood_with_sendmsg(sockfd, dest_addr, count, msg_sz);
	//cnt_send = flood_with_sendtp(sockfd, dest_addr, count, msg_sz);
	tsc_end  = rdtsc();
	time_end = gettime();
	tsc_interval  = tsc_end  - tsc_begin;
	time_interval = time_end - time_begin;

	if (cnt_send < 0) {
		fprintf(stderr, "ERROR: failed to send packets\n");
		close(sockfd);
		exit(EXIT_FAIL_SEND);
	}

	/* Stats */
	pps        = cnt_send / ((double)time_interval / NANOSEC_PER_SEC);
	tsc_cycles = tsc_interval / cnt_send;
	ns_per_pkt = ((double)time_interval / cnt_send);
	timesec    = ((double)time_interval / NANOSEC_PER_SEC);
	printf(" - Per packet: %llu cycles(tsc) %.2f ns, %.2f pps (time:%.2f sec)\n"
	       "   (packet count:%d tsc_interval:%llu)\n",
	       tsc_cycles, ns_per_pkt, pps, timesec,
	       cnt_send, tsc_interval);
}

int main(int argc, char *argv[])
{
	int sockfd, c;
	uint64_t tsc_begin, tsc_end, tsc_interval;
	int cnt_send;
	double pps;
	int nanosecs;

	/* Default settings */
	int addr_family = AF_INET; /* Default address family */
	int count = 1000000;
	int msg_sz = 32; /* 32 + 8(UDP) + 20(IP) + 4(Eth-CRC) = 64 bytes */
	uint16_t dest_port = 6666;
	char *dest_ip;

	/* Support for both IPv4 and IPv6 */
	struct sockaddr_storage dest_addr; /* Can contain both sockaddr_in and sockaddr_in6 */
	memset(&dest_addr, 0, sizeof(dest_addr));

	/* Parse commands line args */
	while ((c = getopt(argc, argv, "c:l:64v:")) != -1) {
		if (c == 'c') count       = atoi(optarg);
		if (c == 'p') dest_port   = atoi(optarg);
		if (c == 'm') msg_sz      = atoi(optarg);
		if (c == '4') addr_family = AF_INET;
		if (c == '6') addr_family = AF_INET6;
		if (c == 'v') verbose     = atoi(optarg);
		if (c == '?') return usage(argv);
	}
	if (optind >= argc) {
		fprintf(stderr, "Expected dest IP-address (IPv6 or IPv4) argument after options\n");
		return usage(argv);
	}
	dest_ip = argv[optind];
	if (verbose > 0)
		printf("Destination IP:%s port:%d\n", dest_ip, dest_port);

	/* Socket setup stuff */
	sockfd = Socket(addr_family, SOCK_DGRAM, IPPROTO_IP);

	/* Setup dest_addr depending on IPv4 or IPv6 address */
	setup_sockaddr(addr_family, &dest_addr, dest_ip, dest_port);

	/* Connect to recv ICMP error messages, and to avoid the
	 * kernel performing connect/unconnect cycles
	 */
	Connect(sockfd, (struct sockaddr *)&dest_addr, sockaddr_len(&dest_addr));

	printf("\nPerformance of: sendto()\n");
	time_function(sockfd, &dest_addr, count, msg_sz, flood_with_sendto);

	printf("\nPerformance of: sendmsg()\n");
	time_function(sockfd, &dest_addr, count, msg_sz, flood_with_sendmsg);

	printf("\nPerformance of: sendMmsg()\n");
	time_function(sockfd, &dest_addr, count, msg_sz, flood_with_sendmmsg);

	close(sockfd);
}
