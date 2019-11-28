/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>, (C)2014-2016
 * License: GPLv2
 * From: https://github.com/netoptimizer/network-testing
 */
static const char *__doc__=
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
#include <linux/filter.h>

#include <getopt.h>

//#include "syscalls.h"
#include <linux/unistd.h>       /* for _syscallX macros/related stuff */

#include "global.h"
#include "common.h"
#include "common_socket.h"

#ifndef SO_ATTACH_REUSEPORT_CBPF
#define SO_ATTACH_REUSEPORT_CBPF	51
#endif

#define RUN_RECVMSG   0x1
#define RUN_RECVMMSG  0x2
#define RUN_RECVFROM  0x4
#define RUN_READ      0x8
#define RUN_RECV      0x10
#define RUN_ALL (RUN_RECVMSG | RUN_RECVMMSG | RUN_RECVFROM | RUN_READ |RUN_RECV)

struct sink_params {
	struct params_common c;
	int lite;
	int iov_elems;
	int batch;
	int count;
	int repeat;
	int waitforone;
	int dontwait;
	int bad_addr;
	int recv_ttl;
	int recv_pktinfo;
	int sk_timeout;
	int timeout;
	int check;
	struct sockaddr_storage sender_addr;
	int so_reuseport;
	int use_bpf;
	int buf_sz;
	unsigned int run_flag;
	unsigned int run_flag_curr;
	/* TODO: Below stats should move to separate stats struct */
	long long ooo;
	long long bad_magic;
	long long bad_repeat;
};

static const struct option long_options[] = {
	/* keep recv functions grouped together */
	{"read",	no_argument,		NULL, 'T' },
	{"recvfrom",	no_argument,		NULL, 't' },
	{"recvmsg",	no_argument,		NULL, 'u' },
	{"recvmmsg",	no_argument,		NULL, 'U' },
	{"recv",	no_argument,		NULL, 176 },
	/* Other options */
	{"help",	no_argument,		NULL, 'h' },
	{"ipv4",	no_argument,		NULL, '4' },
	{"ipv6",	no_argument,		NULL, '6' },
	{"reuse-port",	no_argument,		NULL, 's' },
	{"use-bpf",	no_argument,		NULL, 0 },
	{"waitforone",	no_argument,		NULL, 'O' },
	{"timeout",	required_argument,	NULL, 'i' },
	{"sk-timeout",	required_argument,	NULL, 'I' },
	{"check-pktgen",optional_argument,	NULL, 0 },
	{"nr-iovec",	required_argument,	NULL, 0 },
	{"check-sender",required_argument,	NULL, 'S' },
	{"lite",	no_argument,		NULL, 'L' },
	{"dontwait",	no_argument,		NULL, 'd' },
	{"use-bad-ptr",	required_argument,	NULL, 'B' },
	{"recv-ttl",	no_argument,		NULL, 0  },
	{"recv-pktinfo",no_argument,		NULL, 0  },
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
	printf("Hint: Following options takes an optional argument:\n"
	       "  verbose=N and check-pktgen=N\n"
	       "Notice must be specified with an equal sign "
	       "(due to strange choice of getopt_long)\n"
		);
	printf("\n");

	return EXIT_FAIL_OPTION;
}

static void __check_pkt(struct iovec *iov, int nr, int len, struct sink_params *p)
{
	static struct pktgen_hdr last = { .pgh_magic = 0 };
	int offset = 0, i = 0, hdr = 0, l = len;
	struct pktgen_hdr _pgh, *pgh = NULL, current = { .pgh_magic = 0 };
	int cur_len;

	cur_len = len < iov[i].iov_len ? len : iov[i].iov_len;
	len -= cur_len;
	for (;;) {
		int first_chunk = cur_len - offset;

		/* check for end of buffer */
		if (first_chunk + len < sizeof(_pgh))
			break;

		/* access the header, possibly across different iov buckets */
		if (first_chunk < sizeof(_pgh)) {
			int second_chunk;

			memcpy(&_pgh, iov[i].iov_base + offset, first_chunk);
			second_chunk = sizeof(_pgh) - first_chunk;
			if (second_chunk > 0) {
				if (i + 1 >= nr)
					break;

				memcpy(((char *)&_pgh) + first_chunk,
				       iov[i + 1].iov_base, second_chunk);
			}

			pgh = &_pgh;
		} else {
			pgh = (struct pktgen_hdr*)(iov[i].iov_base + offset);
		}

		if (hdr == 0) {
			/* first header check seqnum and magic */
			if (ntohl(pgh->pgh_magic) != PKTGEN_MAGIC)
				++p->bad_magic;

			if (last.pgh_magic && ((pgh->tv_sec < last.tv_sec) ||
			           (pgh->tv_sec == last.tv_sec &&
			            pgh->tv_usec < last.tv_usec) ||
			           (pgh->tv_sec == last.tv_sec &&
			            pgh->tv_usec == last.tv_usec &&
			            pgh->seq_num < last.seq_num &&
			            last.seq_num < 3*1000*1000*1000u)))
				++p->ooo;

			/* check-pktgen level 3 */
			if ((p->check > 2) && last.pgh_magic)
				if (pgh->seq_num != last.seq_num + 1)
					++p->ooo;

			last = *pgh;
			/* Header is expected to be repeated filling
			 * whole packet, verify at "check-pktgen" level 2
			 */
			if (p->check < 2)
				break;

			current = *pgh;
		} else if (memcmp(&current, pgh, sizeof(current))) {
			p->bad_repeat++;
		}

		hdr++;

		/* move to next chunk */
		offset += sizeof(*pgh);
		if (offset >= iov[i].iov_len) {
			offset -= iov[i].iov_len;
			if (++i >= nr)
				break;
			cur_len = len < iov[i].iov_len ? len : iov[i].iov_len;
			len -= cur_len;
		}
	}

	if ((p->check > 2) && (p->ooo || p->bad_repeat || p->bad_magic)) {
		printf("%s with packet len %d iov nr %d\n", p->ooo ? "OoO" :
			(p->bad_repeat ? "bad repeated hdr" : "bad magic"),
			l, nr);
		exit(EXIT_FAIL_RECV);
	}

	if (current.pgh_magic)
		last = current;
}
static inline
void check_pkt(struct iovec *iov, int nr, int len, struct sink_params *p)
{
	if (likely(!p->check))
		return;

	__check_pkt(iov, nr, len, p);
}

void print_check_result(struct sink_params *p)
{
	if ((p->check)
	    /* Notice: pktgen check only implemented for some functions */
	    && (p->run_flag_curr & (RUN_RECVMMSG | RUN_RECVMSG)))
	{
		printf(" - Failed pktgen checks OoO %lld wrong magic %lld"
		       " bad repeat %lld\n",
		       p->ooo, p->bad_magic, p->bad_repeat);
	}
}

static int sink_with_read(int sockfd, struct sink_params *p,
			  struct time_bench_record *r) {
	int i, res;
	uint64_t total = 0;
	char *buffer = malloc_payload_buffer(p->buf_sz);

	for (i = 0; i < p->count; i++) {
		res = read(sockfd, buffer, p->buf_sz);
		if (res < 0) {
			if (errno == EAGAIN) {
				r->try_again++;
				continue;
			}
			goto error;
		}
		total += res;
	}
	r->bytes = total;
	if (verbose > 0)
		printf(" - read %lu bytes in %d packets\n", total, i);

	free(buffer);
	return (i - r->try_again);

 error: /* ugly construct to make sure the loop is small */
	fprintf(stderr, "ERROR: %s() failed (%d) errno(%d) ",
		__func__, res, errno);
	perror("- read");
	free(buffer);
	close(sockfd);
	exit(EXIT_FAIL_SOCK);
}

static int sink_with_recvfrom(int sockfd, struct sink_params *p,
			      struct time_bench_record *r) {
	int i, res;
	uint64_t total = 0;
	char *buffer = malloc_payload_buffer(p->buf_sz);
	int flags = p->dontwait ? MSG_DONTWAIT : 0;

	for (i = 0; i < p->count; i++) {
		res = recvfrom(sockfd, buffer, p->buf_sz, flags, NULL, NULL);
		if (res < 0) {
			if (errno == EAGAIN) {
				r->try_again++;
				continue;
			}
			goto error;
		}
		total += res;
	}
	r->bytes = total;
	if (verbose > 0)
		printf(" - read %lu bytes in %d packets = %lu bytes payload\n",
		       total, i, total / i);

	free(buffer);
	return (i - r->try_again);

 error: /* ugly construct to make sure the loop is small */
	fprintf(stderr, "ERROR: %s() failed (%d) errno(%d) ",
		__func__, res, errno);
	perror("- recvfrom");
	free(buffer);
	close(sockfd);
	exit(EXIT_FAIL_SOCK);
}

static int sink_with_recv(int sockfd, struct sink_params *p,
			  struct time_bench_record *r) {
	int i, res;
	uint64_t total = 0;
	char *buffer = malloc_payload_buffer(p->buf_sz);
	int flags = p->dontwait ? MSG_DONTWAIT : 0;

	for (i = 0; i < p->count; i++) {
		res = recv(sockfd, buffer, p->buf_sz, flags);
		if (res < 0) {
			if (errno == EAGAIN) {
				r->try_again++;
				continue;
			}
			goto error;
		}
		total += res;
	}
	r->bytes = total;
	if (verbose > 0)
		printf(" - read %lu bytes in %d packets = %lu bytes payload\n",
		       total, i, total / i);

	free(buffer);
	return (i - r->try_again);

 error: /* ugly construct to make sure the loop is small */
	fprintf(stderr, "ERROR: %s() failed (%d) errno(%d) ",
		__func__, res, errno);
	perror("- recv");
	free(buffer);
	close(sockfd);
	exit(EXIT_FAIL_SOCK);
}

static void setup_msg_name(struct msghdr *msg_hdr,
			   struct sockaddr_storage *addr, int family)
{
	if (!family) {
		/* we don't care about the senders info */
		msg_hdr->msg_name    = NULL;
		msg_hdr->msg_namelen = 0;
		return;
	}

	msg_hdr->msg_name = addr;
	msg_hdr->msg_namelen = sizeof(*addr);
}

static void check_msg_name(struct msghdr *msg_hdr,
			   struct sockaddr_storage *sender_addr)
{
	char snd_str[128], in_str[128];
	void *snd, *in;
	int len, alen;

	if (!sender_addr->ss_family)
		return;

	if (sender_addr->ss_family == AF_INET) {
		in = &((struct sockaddr_in *)msg_hdr->msg_name)->sin_addr;
		snd = &((struct sockaddr_in *)sender_addr)->sin_addr;
		len = sizeof(struct in_addr);
		alen = sizeof(struct sockaddr_in);
	} else {
		in = &((struct sockaddr_in6 *)msg_hdr->msg_name)->sin6_addr;
		snd = &((struct sockaddr_in6 *)sender_addr)->sin6_addr;
		len = sizeof(struct in6_addr);
		alen = sizeof(struct sockaddr_in6);
	}

	if (alen != msg_hdr->msg_namelen) {
		printf("sender address len %d does not match expected one %d\n",
		       msg_hdr->msg_namelen, alen);
		exit(EXIT_FAIL_SOCK);
	} else if (memcmp(snd, in, len)) {
		printf("sender address %s does not match expected one %s\n",
		       inet_ntop(sender_addr->ss_family, in, in_str, 128),
		       inet_ntop(sender_addr->ss_family, snd, snd_str, 128));
		exit(EXIT_FAIL_SOCK);
	}
}

#define CMSG_DLEN(cmsg) ((cmsg)->cmsg_len - sizeof(struct cmsghdr))
static void check_cmsg(struct msghdr *msg_hdr, struct sink_params *p,
		       int max_len)
{
	struct in_pktinfo *found_pktinfo = NULL;
	struct cmsghdr *get_cmsg;
	int found_ttl = 0;

	if (!p->recv_ttl && !p->recv_pktinfo) {
		if (msg_hdr->msg_controllen) {
			printf("found unrequested cmsg data, len %zd\n",
			       msg_hdr->msg_controllen);
			exit(EXIT_FAIL_SOCK);
		}
		return;
	}

	if (msg_hdr->msg_controllen > max_len) {
		printf("bad msg len %zd max %d\n", msg_hdr->msg_controllen,
		       max_len);
		exit(EXIT_FAIL_SOCK);
	}

	for (get_cmsg = CMSG_FIRSTHDR(msg_hdr); get_cmsg;
	     get_cmsg = CMSG_NXTHDR(msg_hdr, get_cmsg)) {
		if (get_cmsg->cmsg_level == IPPROTO_IP &&
		    get_cmsg->cmsg_type == IP_PKTINFO &&
		    CMSG_DLEN(get_cmsg) == sizeof(struct in_pktinfo)) {
			found_pktinfo = (struct in_pktinfo *)CMSG_DATA(get_cmsg);
		} else if (get_cmsg->cmsg_level == IPPROTO_IP &&
			   get_cmsg->cmsg_type == IP_TTL  &&
			   CMSG_DLEN(get_cmsg) == sizeof(int)) {
			int *ttl_ptr = ((int *)CMSG_DATA(get_cmsg));
			found_ttl = *ttl_ptr;
		}
	}

	if (p->recv_ttl ^ !!found_ttl) {
		printf("ttl cmsg missmatch, requested %d found %d\n",
		       p->recv_ttl, found_ttl);
		exit(EXIT_FAIL_SOCK);
	}
	if (p->recv_pktinfo ^ !!found_pktinfo) {
		printf("pktinfo cmsg missmatch, requested %d found %p:%d:%x:%x\n",
		       p->recv_pktinfo, found_pktinfo,
		       found_pktinfo ? found_pktinfo->ipi_ifindex : 0,
		       found_pktinfo ? found_pktinfo->ipi_spec_dst.s_addr : 0,
		       found_pktinfo ? found_pktinfo->ipi_addr.s_addr: 0);
		exit(EXIT_FAIL_SOCK);
	}

	if (!verbose)
		return;

	if (found_pktinfo)
		printf("pktinfo: %d:%x:%x\n", found_pktinfo->ipi_ifindex,
		       found_pktinfo->ipi_spec_dst.s_addr,
		       found_pktinfo->ipi_addr.s_addr);
	if (found_ttl)
		printf("ttl: %d\n", found_ttl);
}

static int sink_with_recvmsg(int sockfd, struct sink_params *p,
			     struct time_bench_record *r) {
	int i, res;
	uint64_t total = 0;
	char *buffer = malloc_payload_buffer(p->buf_sz);
	struct msghdr *msg_hdr;  /* struct for setting up transmit */
	struct iovec  *msg_iov;  /* io-vector: array of pointers to payload data */
	int flags = p->dontwait ? MSG_DONTWAIT : 0;
	struct sockaddr_storage sender;
	char cbuf[512];

	msg_hdr = malloc_msghdr();               /* Alloc msghdr setup structure */
	msg_iov = malloc_iovec(p->iov_elems); /* Alloc I/O vector array */

	/*** Setup packet structure for receiving ***/
	setup_msg_name(msg_hdr, &sender, p->sender_addr.ss_family);
	/* Setup io-vector pointers for receiving payload data */
	msg_iov[0].iov_base = buffer;
	msg_iov[0].iov_len  = p->buf_sz / p->iov_elems;
	/* The io-vector supports scattered payload data, below add a simpel
	 * testcase with dst payload, adjust iov_array_elems > 1 to activate code
	 */
	for (i = 1; i < p->iov_elems; i++) {
		msg_iov[i].iov_base = buffer + i * msg_iov[0].iov_len;
		msg_iov[i].iov_len  = msg_iov[0].iov_len;
	}
	/* Binding io-vector to packet setup struct */
	msg_hdr->msg_iov    = msg_iov;
	msg_hdr->msg_iovlen = p->iov_elems;

	msg_hdr->msg_control = (p->recv_ttl || p->recv_pktinfo) ? cbuf: NULL;
	msg_hdr->msg_controllen = (p->recv_ttl || p->recv_pktinfo) ?
					sizeof(cbuf): 0;

	/* Having several IOV's does not help much. The return value
	 * of recvmsg is the total packet size.  It can be split out
	 * on several IOVs, only if the buffer size of the first IOV
	 * is too small.
	 */

	/* Receive LOOP */
	for (i = 0; i < p->count; i++) {
		res = recvmsg(sockfd, msg_hdr, flags);
		if (res < 0) {
			if (errno == EAGAIN) {
				r->try_again++;
				continue;
			}
			goto error;
		}

		check_pkt(msg_iov, p->iov_elems, res, p);
		check_msg_name(msg_hdr, &p->sender_addr);
		check_cmsg(msg_hdr, p, sizeof(cbuf));

		total += res;
	}
	r->bytes = total;
	if (verbose > 0)
		printf(" - read %lu bytes in %d packets = %lu bytes payload\n",
		       total, i, total / i);
	free(msg_iov);
	free(msg_hdr);
	free(buffer);
	return (i - r->try_again);

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

static int sink_with_recvMmsg(int sockfd, struct sink_params *p,
			      struct time_bench_record *r) {
	int cnt, i, res, pkt, batches = 0;
	uint64_t total = 0, packets;
	char *buffer = malloc_payload_buffer(p->buf_sz);
	struct iovec  *msg_iov;  /* io-vector: array of pointers to payload data */
	struct timespec __ts, ___ts = { .tv_sec = p->timeout, .tv_nsec = 0};
	struct timespec *ts = NULL;
	int flags = p->dontwait ? MSG_DONTWAIT : 0;
	struct sockaddr_storage sender[p->batch];
	char cbuf[p->batch][512];

	/* struct *mmsghdr -  pointer to an array of mmsghdr structures.
	 *   *** Notice: double "m" in mmsghdr ***
	 * Allows the caller to transmit multiple messages on a socket
	 * using a single system call
	 */
	struct mmsghdr *mmsg_hdr;

	mmsg_hdr = malloc_mmsghdr(p->batch);         /* Alloc mmsghdr array */
	msg_iov  = malloc_iovec(p->iov_elems*p->batch); /* Alloc I/O vector array */

	/*** Setup packet structure for receiving
	 ***/
	for (pkt = 0; pkt < p->batch; pkt++) {
		int size = p->buf_sz / p->iov_elems;
		char *buf;

		if (p->bad_addr && (pkt == (p->bad_addr - 1)))
			buf = NULL;
		else
			buf = malloc(p->buf_sz);
		/* Setup io-vector pointers for receiving payload data */
		for (i = 0; i < p->iov_elems; i++) {
			msg_iov[pkt*p->iov_elems+i].iov_base = buf + size*i;
			msg_iov[pkt*p->iov_elems+i].iov_len  = size;
		}

		setup_msg_name(&mmsg_hdr[pkt].msg_hdr, &sender[pkt],
			       p->sender_addr.ss_family);
		/* Binding io-vector to packet setup struct */
		mmsg_hdr[pkt].msg_hdr.msg_iov    = &msg_iov[pkt*p->iov_elems];
		mmsg_hdr[pkt].msg_hdr.msg_iovlen = p->iov_elems;
		mmsg_hdr[pkt].msg_hdr.msg_control = (p->recv_ttl || p->recv_pktinfo) ?
						cbuf[pkt]: NULL;
		mmsg_hdr[pkt].msg_hdr.msg_controllen = (p->recv_ttl || p->recv_pktinfo) ?
							sizeof(cbuf[pkt]): 0;
	}

	if (p->timeout >= 0)
		ts = &__ts;

	flags |= p->waitforone ? MSG_WAITFORONE: 0;

	/* Receive LOOP */
	for (cnt = 0; cnt < p->count; ) {
		__ts = ___ts;
		res = recvmmsg(sockfd, mmsg_hdr, p->batch, flags, ts);
		if (res < 0) {
			if (errno == EAGAIN) {
				r->try_again++;
				continue;
			}
			goto error;
		}
		batches++;
		for (pkt = 0; pkt < res; pkt++) {
			total += mmsg_hdr[pkt].msg_len;
			check_pkt(mmsg_hdr[pkt].msg_hdr.msg_iov,
				  mmsg_hdr[pkt].msg_hdr.msg_iovlen,
				  mmsg_hdr[pkt].msg_len, p);
			check_msg_name(&mmsg_hdr[pkt].msg_hdr, &p->sender_addr);
			check_cmsg(&mmsg_hdr[pkt].msg_hdr, p, sizeof(cbuf[pkt]));
		}
		cnt += res;
	}
	packets = cnt - r->try_again;
	r->bytes = total;
	if (verbose > 0) {
		printf(" - read %lu bytes in %lu packets= %lu bytes "
			       "payload", total, packets,
			       packets ? total / packets: 0);
		if (p->waitforone)
			printf("= %ld avg batch len",
			       batches ? packets / batches : 0);
		printf(" (loop %d)\n", batches);
	}

	for (pkt=0; pkt < p->batch; ++pkt)
		free(msg_iov[pkt*p->iov_elems].iov_base);

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

static void init_stats(struct sink_params *params, unsigned int testrun)
{
	/* Params also contain some stats the need reset between runs.
	 * TODO: Move these into stats section of struct
	 */
	params->ooo		= 0;
	params->bad_magic	= 0;
	params->bad_repeat	= 0;
	params->run_flag_curr	= testrun;
}

static void time_function(int sockfd, struct sink_params *p, const char *name,
			  int (*func)(int sockfd, struct sink_params *p,
				      struct time_bench_record *r))
{
	char from_ip[INET6_ADDRSTRLEN] = {0}; /* Assume max IPv6 */
	struct time_bench_record rec = {0};
	int str_max = sizeof(from_ip);
	int cnt_recv, j;
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
	if (verbose && !p->c.connect)
		printf("  * Got first packet from IP:port %s:%d\n",
		       from_ip, ntohs(src_port));

	if (p->c.connect) {
		if (verbose)
			printf("  * Connect UDP sock to src IP:port %s:%d\n",
			       from_ip, ntohs(src_port));
		Connect(sockfd, src, addrlen);

	if (verbose)
		printf("  * Got first packet (starting timing)\n");
	}

	for (j = 0; j < p->repeat; j++) {
		if (verbose) {
			printf(" Test run: %d (expecting to receive %d pkts)\n",
			       j, p->count);
		} else {
			int b = (p->run_flag_curr & RUN_RECVMMSG) ? p->batch:0;
			print_header(name, b);
			printf("run: %2d\t", j);
		}

		time_bench_record_setting(&rec);
		time_bench_start(&rec);
		cnt_recv = func(sockfd, p, &rec);
		time_bench_stop(&rec);

		if (cnt_recv < 0) {
			fprintf(stderr, "ERROR: failed to recv packets\n");
			close(sockfd);
			exit(EXIT_FAIL_RECV);
		}
		rec.packets = cnt_recv;
		time_bench_calc_stats(&rec);
		time_bench_print_stats(&rec, &p->c);
		print_check_result(p);
		init_stats(p, p->run_flag_curr);
	}

	return;

socket_error:
	fprintf(stderr, "ERROR: %s() failed (%d) errno(%d) ",
		__func__, res, errno);
	close(sockfd);
	exit(EXIT_FAIL_SOCK);
}

static int enable_bpf(int sockfd)
{
	struct sock_filter code[] = {
		/* A = raw_smp_processor_id() */
		{ BPF_LD  | BPF_W | BPF_ABS, 0, 0, SKF_AD_OFF + SKF_AD_CPU },
		/* return A */
		{ BPF_RET | BPF_A, 0, 0, 0 },
	};
	struct sock_fprog p = {
		.len = 2,
		.filter = code,
	};

	/* the kernel will call the specified filter to distribute the
	 * packets among the SO_REUSEPORT sockets group.
	 * Only the first socket in the group can set such filter.
	 * The filter implemented here distributes the ingress packets
	 * to the socket with the id equal to the CPU id processing
	 * the packet inside the kernel.
	 * With RSS in place and 1 to 1 mapping between ingress NIC
	 * RX queues and NIC's irqs, this maps 1 to 1 between ingress NIC RX
	 * queues and REUSEPORT sockets.
	 */
	if (setsockopt(sockfd, SOL_SOCKET, SO_ATTACH_REUSEPORT_CBPF, &p,
		       sizeof(p)))
		return -1;

	return 0;
}

static void init_params(struct sink_params *params)
{
	memset(params, 0, sizeof(struct sink_params));
	params->timeout = -1;
	params->sk_timeout = -1;
	params->count  = 1000000;
	params->repeat = 1;
	params->batch = 32;
	params->iov_elems = 1;
	params->buf_sz = 4096;
	params->run_flag = 0;
}

int main(int argc, char *argv[])
{
	struct sockaddr_storage listen_addr; /* Can contain both sockaddr_in and sockaddr_in6 */
	uint16_t listen_port = 6666;
	int addr_family = AF_INET; /* Default address family */
	struct sink_params p;
	int longindex = 0;
	int sockfd, c;
	int on = 1;

	init_params(&p);

	/* Parse commands line args */
	while ((c = getopt_long(argc, argv, "hc:r:l:64Oi:I:LdsCS:B:v:tTuUb:",
				long_options, &longindex)) != -1) {
		if (c == 0) {
			/* handle options without short version */
			if (!strcmp(long_options[longindex].name,
				    "check-pktgen"))
				p.check = optarg ? atoi(optarg) : 1;
			if (!strcmp(long_options[longindex].name,
				    "nr-iovec"))
				p.iov_elems = atoi(optarg);
			if (!strcmp(long_options[longindex].name,
				    "recv-pktinfo"))
				p.recv_pktinfo = 1;
			if (!strcmp(long_options[longindex].name,
				    "use-bpf"))
				p.use_bpf = true;
			if (!strcmp(long_options[longindex].name, "recv-ttl"))
				p.recv_ttl = 1;
		}
		if (c == 'c') p.count     = atoi(optarg);
		if (c == 'r') p.repeat    = atoi(optarg);
		if (c == 'b') p.batch     = atoi(optarg);
		if (c == 'l') listen_port = atoi(optarg);
		if (c == '4') addr_family = AF_INET;
		if (c == '6') addr_family = AF_INET6;
		if (c == 'O') p.waitforone  = 1;
		if (c == 'i') p.timeout     = atoi(optarg);
		if (c == 'I') p.sk_timeout  = atoi(optarg);
		if (c == 'L') p.lite      = 1;
		if (c == 'd') p.dontwait  = 1;
		if (c == 'B') p.bad_addr  = atoi(optarg);
		if (c == 'C') p.c.connect  = 1;
		if (c == 's') p.so_reuseport = 1;
		if (c == 'S') setup_sockaddr(addr_family, &p.sender_addr,
					     optarg, 0);
		if (c == 'v') verbose     = optarg ? atoi(optarg) : 1;
		if (c == 'u') p.run_flag   |= RUN_RECVMSG;
		if (c == 'U') p.run_flag   |= RUN_RECVMMSG;
		if (c == 't') p.run_flag   |= RUN_RECVFROM;
		if (c == 'T') p.run_flag   |= RUN_READ;
		if (c == 176) p.run_flag   |= RUN_RECV;
		if (c == 'h' || c == '?') return usage(argv);
	}

	if (verbose > 0)
		printf("Listen port %d\n", listen_port);

	if (p.run_flag == 0)
		p.run_flag = RUN_ALL;

	/* Socket setup stuff */
	sockfd = Socket(addr_family, SOCK_DGRAM, p.lite ? IPPROTO_UDPLITE :
			IPPROTO_UDP);

	/* Enable use of SO_REUSEPORT for multi-process testing  */
	if (p.so_reuseport) {
		if ((setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT,
				&p.so_reuseport, sizeof(p.so_reuseport))) < 0) {
			    printf("ERROR: No support for SO_REUSEPORT\n");
			    perror("- setsockopt(SO_REUSEPORT)");
			    exit(EXIT_FAIL_SOCKOPT);
		}
	}

	/* Enable BPF filtering to distribute the ingress packets among the
	 * SO_REUSEPORT sockets
	 */
	if (p.use_bpf && enable_bpf(sockfd)) {
		printf("ERROR: No support for SO_ATTACH_REUSEPORT_CBPF\n");
		perror("- setsockopt(SO_ATTACH_REUSEPORT_CBPF)");
		exit(EXIT_FAIL_SOCKOPT);
	}

	/* enable the requested ancillary messages */
	if (p.recv_pktinfo) {
		if (setsockopt(sockfd, SOL_IP, IP_PKTINFO, &on, sizeof(on)) < 0) {
			printf("ERROR: No support for IP_PKTINFO\n");
			perror("- setsockopt(IP_PKTINFO)");
			exit(EXIT_FAIL_SOCKOPT);
		}
	}

	if (p.recv_ttl) {
		if (setsockopt(sockfd, SOL_IP, IP_RECVTTL, &on, sizeof(on)) < 0) {
			printf("ERROR: No support for IP_RECVTTL\n");
			perror("- setsockopt(IP_RECVTTL)");
			exit(EXIT_FAIL_SOCKOPT);
		}
	}

	/* Setup listen_addr depending on IPv4 or IPv6 address */
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

	if (p.sk_timeout >= 0) {
		struct timeval tv = { p.sk_timeout, 0 };

		if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv,
			       sizeof(tv)) < 0) {
			perror("- setsockopt(SO_RCVTIMEO)");
			exit(EXIT_FAIL_SOCKOPT);
		}
	}

	if (!verbose)
		printf("%-10s\t%-8s %-8s\tns/pkt\tpps\t\tcycles\tpayload\n",
		       "", "run", "count");


	if (p.run_flag       & RUN_RECVMMSG) {
		init_stats(&p, RUN_RECVMMSG);
		time_function(sockfd, &p, "recvMmsg", sink_with_recvMmsg);
	}

	if (p.run_flag       & RUN_RECVMSG) {
		init_stats(&p, RUN_RECVMSG);
		time_function(sockfd, &p, "recvmsg", sink_with_recvmsg);
	}

	if (p.run_flag       & RUN_READ) {
		init_stats(&p, RUN_READ);
		time_function(sockfd, &p, "read", sink_with_read);
	}

	if (p.run_flag       & RUN_RECVFROM) {
		init_stats(&p, RUN_RECVFROM);
		time_function(sockfd, &p, "recvfrom", sink_with_recvfrom);
	}

	if (p.run_flag       & RUN_RECV) {
		init_stats(&p, RUN_RECV);
		time_function(sockfd, &p, "recv", sink_with_recv);
	}

	close(sockfd);
	return 0;
}
