/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>, (C)2014-2016
 * License: GPLv2
 * From: https://github.com/netoptimizer/network-testing
 */
static const char *__doc__=
 " This tool is a UDP flood that measures the outgoing packet rate.\n"
 " Default cycles through tests with different send system calls.\n"
 " What function-call to invoke can also be specified as a command\n"
 " line option (see below).\n"
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
#include <time.h>

#include <getopt.h>

//#include "syscalls.h"
#include <linux/unistd.h>       /* for _syscallX macros/related stuff */

#include "global.h"
#include "common.h"
#include "common_socket.h"

#define RUN_SENDMSG   0x1
#define RUN_SENDMMSG  0x2
#define RUN_SENDTO    0x4
#define RUN_WRITE     0x8
#define RUN_SEND      0x10
#define RUN_ALL       (RUN_SENDMSG | RUN_SENDMMSG | RUN_SENDTO | RUN_WRITE | RUN_SEND)

struct flood_params {
	struct params_common c;
	int lite;
	int batch;
	int count;
	int msg_sz;
	int pmtu; /* Path MTU Discovery setting, affect DF bit */
	int pktgen_hdr;

	/* Support for both IPv4 and IPv6 */
	struct sockaddr_storage dest_addr;
};

static const struct option long_options[] = {
	{"help",	no_argument,		NULL, 'h' },
	{"ipv4",	no_argument,		NULL, '4' },
	{"ipv6",	no_argument,		NULL, '6' },
	{"lite",	no_argument,		NULL, 'L' },
	{"pktgen-header", no_argument,		NULL, 'P' },
	/* keep these grouped together */
	{"sendmsg",	no_argument,		NULL, 'u' },
	{"sendmmsg",	no_argument,		NULL, 'U' },
	{"sendto",	no_argument,		NULL, 't' },
	{"write",	no_argument,		NULL, 'T' },
	{"send",	no_argument,		NULL, 'S' },
	{"batch",	required_argument,	NULL, 'b' },
	{"count",	required_argument,	NULL, 'c' },
	{"port",	required_argument,	NULL, 'p' },
	{"payload",	required_argument,	NULL, 'm' },
	{"pmtu",	required_argument,	NULL, 'd' },// IP_MTU_DISCOVER
	{"verbose",	optional_argument,	NULL, 'v' },
	{0, 0, NULL,  0 }
};

/* From: kernel/include/uapi/linux/ip.h */
#if 0
/* IP_MTU_DISCOVER values */
#define IP_PMTUDISC_DONT		0	/* Never send DF frames */
#define IP_PMTUDISC_WANT		1	/* Use per route hints	*/
#define IP_PMTUDISC_DO			2	/* Always DF		*/
#define IP_PMTUDISC_PROBE		3       /* Ignore dst pmtu      */
/* Always use interface mtu (ignores dst pmtu) but don't set DF flag.
 * Also incoming ICMP frag_needed notifications will be ignored on
 * this socket to prevent accepting spoofed ones.
 */
#define IP_PMTUDISC_INTERFACE		4
/* weaker version of IP_PMTUDISC_INTERFACE, which allos packets to get
 * fragmented if they exeed the interface mtu
 */
#define IP_PMTUDISC_OMIT		5
#endif
#define IP_PMTUDISC_MAX	(IP_PMTUDISC_OMIT + 1)

static const char *ip_mtu_discover_names[IP_PMTUDISC_MAX] = {
	[IP_PMTUDISC_DONT]	= "IP_PMTUDISC_DONT",
	[IP_PMTUDISC_WANT]	= "IP_PMTUDISC_WANT",
	[IP_PMTUDISC_DO]	= "IP_PMTUDISC_DO",
	[IP_PMTUDISC_PROBE]	= "IP_PMTUDISC_PROBE",
	[IP_PMTUDISC_INTERFACE]	= "IP_PMTUDISC_INTERFACE",
	[IP_PMTUDISC_OMIT]	= "IP_PMTUDISC_OMIT",
};
static const char *pmtu_to_string(int pmtu)
{
	if (pmtu < IP_PMTUDISC_MAX)
		return ip_mtu_discover_names[pmtu];
	return NULL;
}

#define DEFAULT_COUNT 1000000

static int usage(char *argv[])
{
	int i;

	printf("\nDOCUMENTATION:\n%s\n", __doc__);
	printf(" Default transmit %d packets per test, adjust via --count\n",
	       DEFAULT_COUNT);
	printf("\n");
	printf(" Usage: %s (options-see-below) IPADDR\n",
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
	printf("     -u -U -t -T -S: run any combination of"
		       " sendmsg/sendmmsg/sendto/write/send\n");
	printf("\n");
	printf("Option --pmtu <N>  for Path MTU discover socket option"
	       " IP_MTU_DISCOVER\n"
	       " This affects the DF(Don't-Fragment) bit setting.\n"
	       " Following values are selectable:\n");
	for (i = 0; i < IP_PMTUDISC_MAX; i++)
		printf("  %d = %s\n", i, pmtu_to_string(i));
	printf(" Documentation see under IP_MTU_DISCOVER in 'man 7 ip'\n");
	printf("\n");

	return EXIT_FAIL_OPTION;
}

static void fill_buf(const struct flood_params *p, char *buf, int len)
{
	static uint32_t sequence = 0;
	struct pktgen_hdr hdr;
	struct timespec ts;
	int l;

	if (!p->pktgen_hdr)
		return;

	clock_gettime(CLOCK_REALTIME_COARSE, &ts);
	hdr.tv_sec = ts.tv_sec;
	hdr.tv_usec = ts.tv_nsec * 1000;
	hdr.pgh_magic = htonl(PKTGEN_MAGIC);
	hdr.seq_num = sequence++;
	for (l = 0; l < len; l += sizeof(hdr)) {
		int cur_len = (len - l < sizeof(hdr)) ? len - l : sizeof(hdr);

		memcpy(buf + l, &hdr, cur_len);
	}
}

static int flood_with_sendto(int sockfd, struct flood_params *p,
			     struct time_bench_record *r)
{
	char *msg_buf;
	int cnt, res = 0;
	socklen_t addrlen = sockaddr_len(&p->dest_addr);
	uint64_t total = 0;

	/* Allocate payload buffer */
	msg_buf = malloc_payload_buffer(p->msg_sz);

	/* Flood loop */
	for (cnt = 0; cnt < p->count; cnt++) {
		fill_buf(p, msg_buf, p->msg_sz);
		res = sendto(sockfd, msg_buf, p->msg_sz, 0,
			     (struct sockaddr *) &p->dest_addr, addrlen);
		if (res < 0) {
			fprintf(stderr, "Managed to send %d packets\n", cnt);
			perror("- sendto");
			goto out;
		}
		total += res;
	}
	r->bytes = total;
	res = cnt;

out:
	free(msg_buf);
	return res;
}

static int flood_with_send(int sockfd, struct flood_params *p,
			   struct time_bench_record *r)
{
	char *msg_buf;
	int cnt, res = 0;
	int flags = 0;
	uint64_t total = 0;

	/* Allocate payload buffer */
	msg_buf = malloc_payload_buffer(p->msg_sz);

	/* Flood loop */
	for (cnt = 0; cnt < p->count; cnt++) {
		res = send(sockfd, msg_buf, p->msg_sz, flags);
		if (res < 0) {
			fprintf(stderr, "Managed to send %d packets\n", cnt);
			perror("- send");
			goto out;
		}
		total += res;
	}
	r->bytes = total;
	res = cnt;

out:
	free(msg_buf);
	return res;
}

static int flood_with_write(int sockfd, struct flood_params *p,
			    struct time_bench_record *r)
{
	char *msg_buf;
	int cnt, res = 0;
	uint64_t total = 0;

	/* Allocate payload buffer */
	msg_buf = malloc_payload_buffer(p->msg_sz);

	/* Flood loop */
	for (cnt = 0; cnt < p->count; cnt++) {
		fill_buf(p, msg_buf, p->msg_sz);
		res = write(sockfd, msg_buf, p->msg_sz);
		if (res < 0) {
			fprintf(stderr, "Managed to send %d packets\n", cnt);
			perror("- write");
			goto out;
		}
		total += res;
	}
	r->bytes = total;
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

static int flood_with_sendmsg(int sockfd, struct flood_params *p,
			      struct time_bench_record *r)
{
	char          *msg_buf;  /* payload data */
	struct msghdr *msg_hdr;  /* struct for setting up transmit */
	struct iovec  *msg_iov;  /* io-vector: array of pointers to payload data */
	unsigned int  iov_array_elems = 1; /*adjust to test scattered payload */
	uint64_t total = 0;
	int i;

	int cnt, res;
	socklen_t addrlen = sockaddr_len(&p->dest_addr);

	msg_buf = malloc_payload_buffer(p->msg_sz); /* Alloc payload buffer */
	msg_hdr = malloc_msghdr();               /* Alloc msghdr setup structure */
	msg_iov = malloc_iovec(iov_array_elems); /* Alloc I/O vector array */

	/*** Setup packet structure for transmitting ***/

	/* The destination addr */
	msg_hdr->msg_name    = &p->dest_addr;
	msg_hdr->msg_namelen = addrlen;

	/* Setup io-vector pointers to payload data */
	msg_iov[0].iov_base = msg_buf;
	msg_iov[0].iov_len  = p->msg_sz / iov_array_elems;
	/* The io-vector supports scattered payload data, below add a simpel
	 * testcase with same payload, adjust iov_array_elems > 1 to activate code
	 */
	for (i = 1; i < iov_array_elems; i++) {
		msg_iov[i].iov_base = msg_buf + (p->msg_sz / iov_array_elems) * i;
		msg_iov[i].iov_len  = p->msg_sz / iov_array_elems;
	}
	/* Binding io-vector to packet setup struct */
	msg_hdr->msg_iov    = msg_iov;
	msg_hdr->msg_iovlen = iov_array_elems;

	/* Flood loop */
	for (cnt = 0; cnt < p->count; cnt++) {
		fill_buf(p, msg_buf, p->msg_sz);
		res = sendmsg(sockfd, msg_hdr, 0);
		if (res < 0) {
			goto error;
		}
		total += res;
	}
	r->bytes = total;
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
static int flood_with_sendMmsg(int sockfd, struct flood_params *p,
			       struct time_bench_record *r)
{
	int total_size = p->batch * p->msg_sz; /* total amount to be allocated */
	char          *msg_buf;  /* payload data */
	struct iovec  *msg_iov;  /* io-vector: array of pointers to payload data */
	unsigned int  iov_array_elems = 1; /*adjust to test scattered payload */
	int i, batches, last;
	uint64_t total = 0;

	batches = p->count / p->batch;
	last = p->count - batches * p->batch;

	/* struct *mmsghdr -  pointer to an array of mmsghdr structures.
	 *   *** Notice: double "m" in mmsghdr ***
	 * Allows the caller to transmit multiple messages on a socket
	 * using a single system call
	 */
	struct mmsghdr *mmsg_hdr;

	int cnt, res, pkt;
	socklen_t addrlen = sockaddr_len(&p->dest_addr);

	if (verbose > 0)
		fprintf(stderr, " - batching %d packets in sendmmsg\n", p->batch);

	msg_buf  = malloc_payload_buffer(total_size); /* Alloc payload buffer */
	mmsg_hdr = malloc_mmsghdr(p->batch);         /* Alloc mmsghdr array */
	msg_iov  = malloc_iovec(iov_array_elems * p->batch); /* Alloc I/O vector array */

	/*** Setup packet structure for transmitting ***/
	for (pkt = 0; pkt < p->batch; pkt++) {
		char *base = msg_buf + pkt * p->msg_sz;
		int incr = p->msg_sz / iov_array_elems;
		int iov_idx = pkt * iov_array_elems;

		/* Setup io-vector pointers to payload data */
		for (i = 0; i < iov_array_elems; i++) {
			msg_iov[iov_idx + i].iov_base = base + i * incr;
			msg_iov[iov_idx + i].iov_len  = incr;
		}

		/* The destination addr */
		mmsg_hdr[pkt].msg_hdr.msg_name    = &p->dest_addr;
		mmsg_hdr[pkt].msg_hdr.msg_namelen = addrlen;
		/* Binding io-vector to packet setup struct */
		mmsg_hdr[pkt].msg_hdr.msg_iov    = &msg_iov[iov_idx];
		mmsg_hdr[pkt].msg_hdr.msg_iovlen = iov_array_elems;
	}

	/* Flood loop */
	for (cnt = 0; cnt < batches; cnt++) {
		if (p->pktgen_hdr)
			for (pkt = 0; pkt < p->batch; pkt++)
				fill_buf(p, msg_buf + pkt * p->msg_sz, p->msg_sz);
//		res = sendmmsg(sockfd, mmsg_hdr, batch, 0);
		res = syscall(__NR_sendmmsg, sockfd, mmsg_hdr, p->batch, 0);

		if (res < 0)
			goto error;
		total += res * p->msg_sz;
	}
	r->bytes = total;

	if (last) {
		if (p->pktgen_hdr)
			for (pkt = 0; pkt < p->batch; pkt++)
				fill_buf(p, msg_buf + pkt * p->msg_sz, p->msg_sz);
		res = syscall(__NR_sendmmsg, sockfd, mmsg_hdr, last, 0);
		if (res < 0)
			goto error;
	}

	res = p->count;
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


static void time_function(int sockfd, struct flood_params *p,
			  int (*func)(int sockfd, struct flood_params *p,
				      struct time_bench_record *r))
{
	struct time_bench_record rec = {0};
	int cnt_send;

	time_bench_start(&rec);
	cnt_send = func(sockfd, p, &rec);
	time_bench_stop(&rec);

	if (cnt_send < 0) {
		fprintf(stderr, "ERROR: failed to send packets\n");
		close(sockfd);
		exit(EXIT_FAIL_SEND);
	}
	rec.packets = cnt_send;
	time_bench_calc_stats(&rec);
	time_bench_print_stats(&rec, &p->c);
}

static void init_params(struct flood_params *params)
{
	memset(params, 0, sizeof(struct flood_params));
	params->count  = DEFAULT_COUNT;
	params->batch = 32;
	params->msg_sz = 18; /* 18 +14(eth)+8(UDP)+20(IP)+4(Eth-CRC) = 64 bytes */
	params->pmtu = -1;
}

int main(int argc, char *argv[])
{
	int sockfd, c;

	/* Default settings */
	int addr_family = AF_INET; /* Default address family */
	struct flood_params p;
	uint16_t dest_port = 6666;
	char *dest_ip;
	int run_flag = 0;
	int longindex = 0;

	init_params(&p);

	/* Parse commands line args */
	while ((c = getopt_long(argc, argv, "hc:p:m:64PLv:tTuUb:",
				long_options, &longindex)) != -1) {
		if (c == 'c') p.count     = atoi(optarg);
		if (c == 'p') dest_port   = atoi(optarg);
		if (c == 'm') p.msg_sz    = atoi(optarg);
		if (c == 'b') p.batch     = atoi(optarg);
		if (c == 'P') p.pktgen_hdr= 1;
		if (c == '4') addr_family = AF_INET;
		if (c == '6') addr_family = AF_INET6;
		if (c == 'd') p.pmtu      = atoi(optarg);
		if (c == 'L') p.lite      = 1;
		if (c == 'v') verbose     = optarg ? atoi(optarg) : 1;
		if (c == 'u') run_flag   |= RUN_SENDMSG;
		if (c == 'U') run_flag   |= RUN_SENDMMSG;
		if (c == 't') run_flag   |= RUN_SENDTO;
		if (c == 'T') run_flag   |= RUN_WRITE;
		if (c == 'S') run_flag   |= RUN_SEND;
		if (c == 'h' || c == '?') return usage(argv);
	}
	if (optind >= argc) {
		fprintf(stderr, "Expected dest IP-address (IPv6 or IPv4) argument after options\n");
		return usage(argv);
	}
	dest_ip = argv[optind];
	if (verbose > 0)
		printf("Destination IP:%s port:%d\n", dest_ip, dest_port);

	if (run_flag == 0)
		run_flag = RUN_ALL;

	/* Socket setup stuff */
	sockfd = Socket(addr_family, SOCK_DGRAM, p.lite ? IPPROTO_UDPLITE :
			IPPROTO_UDP);

	if (p.pmtu != -1) {
		if (verbose > 0)
			printf("setsockopt IP_MTU_DISCOVER: %s(%d)\n",
			       pmtu_to_string(p.pmtu), p.pmtu);
		setsockopt(sockfd, SOL_IP, IP_MTU_DISCOVER,
			   &p.pmtu, sizeof(p.pmtu));
	}

	/* Setup dest_addr depending on IPv4 or IPv6 address */
	setup_sockaddr(addr_family, &p.dest_addr, dest_ip, dest_port);

	/* Connect to recv ICMP error messages, and to avoid the
	 * kernel performing connect/unconnect cycles
	 */
	Connect(sockfd, (struct sockaddr *)&p.dest_addr, sockaddr_len(&p.dest_addr));
	p.c.connect = 1;

	if (!verbose)
		printf("             \tns/pkt\tpps\t\tcycles\tpayload\n");
	if (run_flag & RUN_SEND) {
		print_header("send", 0);
		time_function(sockfd, &p, flood_with_send);
	}
	if (run_flag & RUN_SENDTO) {
		print_header("sendto", 0);
		time_function(sockfd, &p, flood_with_sendto);
	}

	if (run_flag & RUN_SENDMSG) {
		print_header("sendmsg", 0);
		time_function(sockfd, &p, flood_with_sendmsg);
	}

	if (run_flag & RUN_SENDMMSG) {
		print_header("sendMmsg", p.batch);
		time_function(sockfd, &p, flood_with_sendMmsg);
	}

	if (run_flag & RUN_WRITE) {
		print_header("write", 0);
		time_function(sockfd, &p, flood_with_write);
	}

	close(sockfd);
	return 0;
}
