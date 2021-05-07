/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>
 * License: GPLv2
 */
static const char *__doc__=
 " This tool is a UDP pacer that clock-out packets at fixed interval.\n";

#define _GNU_SOURCE /* needed for struct mmsghdr and getopt.h */
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <linux/udp.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

#include "global.h"
#include "common.h"
#include "common_socket.h"

static const struct option long_options[] = {
	{"help",	no_argument,		NULL, 'h' },
	{"verbose",	optional_argument,	NULL, 'v' },
	{"batch",	required_argument,	NULL, 'b' },
	{"count",	required_argument,	NULL, 'c' },
	{"port",	required_argument,	NULL, 'p' },
	{0, 0, NULL,  0 }
};

/* Global variables */
static int shutdown_global = 0;

/* Default interval in usec */
#define DEFAULT_INTERVAL 1000000

#define USEC_PER_SEC		1000000
#define NSEC_PER_SEC		1000000000

struct cfg_params {
	int batch;
	int count;
	int msg_sz;
	// int pmtu; /* Path MTU Discovery setting, affect DF bit */

	// int clock;
	unsigned long interval;

	/* Below socket setup */
	int sockfd;
	int addr_family;    /* redundant: in dest_addr after setup_sockaddr */
	uint16_t dest_port; /* redundant: in dest_addr after setup_sockaddr */
	struct sockaddr_storage dest_addr; /* Support for both IPv4 and IPv6 */
};

/* Struct to transfer parameters to the thread */
struct thread_param {
	struct thread_stat *stats;

	int sockfd;
	int batch;
	int msg_sz;

	int clock;
	unsigned long interval;

	unsigned long max_cycles;
};

/* Struct for statistics */
struct thread_stat {
	pthread_t thread;
	int thread_started;

	unsigned long cycles;
	//unsigned long cyclesread;
	long min;
	long max;
	long act;
	double avg;
};

static int usage(char *argv[])
{
	int i;

	printf("\nDOCUMENTATION:\n%s\n\n", __doc__);
	printf(" Usage: %s (options-see-below) IPADDR\n", argv[0]);
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
	return EXIT_FAIL_OPTION;
}

static inline void tsnorm(struct timespec *ts)
{
	while (ts->tv_nsec >= NSEC_PER_SEC) {
		ts->tv_nsec -= NSEC_PER_SEC;
		ts->tv_sec++;
	}
}

#define DEBUG 1
static void sighand(int sig)
{
	struct timespec wait;
	int clock_type = CLOCK_MONOTONIC;

	shutdown_global = 1;

	clock_gettime(clock_type, &wait);
	if (DEBUG)
		printf("%s() Goodbye at %ld.%ld sec\n", __func__,
		       wait.tv_sec, wait.tv_nsec);
}


static inline int64_t calcdiff(struct timespec t1, struct timespec t2)
{
	int64_t diff;
	diff = USEC_PER_SEC * (long long)((int) t1.tv_sec - (int) t2.tv_sec);
	diff += ((int) t1.tv_nsec - (int) t2.tv_nsec) / 1000;
	return diff;
}

static int socket_send(int sockfd, int msg_sz, int batch)
{
	uint64_t total = 0;
	int cnt, res = 0;
	int flags = 0;
	char *msg_buf;

	/* Allocate payload buffer */
	msg_buf = malloc_payload_buffer(msg_sz);

	/* Add test contents easy viewable via nc */
	memset(msg_buf, 'A', msg_sz);
	msg_buf[0]='\n';

	/* Send a batch of the same packet  */
	for (cnt = 0; cnt < batch; cnt++) {
		res = send(sockfd, msg_buf, msg_sz, flags);
		if (res < 0) {
			fprintf(stderr, "Managed to send %d packets\n", cnt);
			perror("- send");
			goto out;
		}
		total += res;
	}
	res = cnt;
out:
	free(msg_buf);
	return res;
}

void *timer_thread(void *param)
{
	struct thread_param *par = param;
	struct thread_stat *stat = par->stats;

	int timermode = TIMER_ABSTIME;
	int clock = par->clock;

	struct timespec now, next, interval;

	interval.tv_sec = par->interval / USEC_PER_SEC;
	interval.tv_nsec = (par->interval % USEC_PER_SEC) * 1000;

	clock_gettime(clock, &now);

	next = now;
	next.tv_sec  += interval.tv_sec;
	next.tv_nsec += interval.tv_nsec;
	tsnorm(&next);

	stat->thread_started++;

	while (!shutdown_global) {
		uint64_t diff;
		int err;

		/* Wait for next period */
		err = clock_nanosleep(clock, timermode, &next, NULL);
		/* Took case MODE_CLOCK_NANOSLEEP from cyclictest */
		if (err) {
			if (err != EINTR)
				fprintf(stderr, "clock_nanosleep failed."
					" err:%d errno:%d\n", err, errno);
			goto out;
		}

		/* Expecting to wakeup at "next" get systime "now" to check */
		err = clock_gettime(clock, &now);
		if (err) {
			if (err != EINTR)
				fprintf(stderr, "clock_getttime() failed."
					" err:%d errno:%d\n", err, errno);
			goto out;
		}

		/* Detect inaccuracy diff */
		diff = calcdiff(now, next);
		if (diff < stat->min)
			stat->min = diff;
		if (diff > stat->max)
			stat->max = diff;
		stat->avg += (double) diff;
		stat->act = diff;

		stat->cycles++;

		socket_send(par->sockfd, par->msg_sz, par->batch);

		printf("Diff at cycle:%lu min:%ld max:%ld\n",
		       stat->cycles, stat->min, stat->max);

		next.tv_sec  += interval.tv_sec;
		next.tv_nsec += interval.tv_nsec;
		tsnorm(&next);

		if (par->max_cycles && par->max_cycles == stat->cycles)
			break;

	}
	printf("Thread ended stats: cycles:%lu min:%ld max:%ld\n",
	       stat->cycles, stat->min, stat->max);

out:
	shutdown_global = 1;
	stat->thread_started = -1;
	return NULL;
}

static struct thread_param *setup_pthread(struct cfg_params *cfg)
{
	pthread_attr_t attr;
	int status;

	struct thread_param *par;
	struct thread_stat *stat;

	par  = calloc(1, sizeof(*par));
	stat = calloc(1, sizeof(*stat));
	if (!par || !stat) {
                fprintf(stderr, "%s(): Mem alloc error\n", __func__);
                exit(EXIT_FAIL_MEM);
        }

	status = pthread_attr_init(&attr);
	if (status != 0) {
		printf("error from pthread_attr_init: %s\n", strerror(status));
		exit(EXIT_FAIL_PTHREAD);
	}

	par->interval   = cfg->interval;
	par->max_cycles = cfg->count;
	par->clock      = CLOCK_MONOTONIC;

	/* Info for sending packets */
	par->sockfd = cfg->sockfd;
	par->batch  = cfg->batch;
	par->msg_sz = cfg->msg_sz;

	par->stats = stat;
	stat->min = 1000000;
	stat->max = 0;
	stat->avg = 0.0;

	stat->thread_started = 1;
	status = pthread_create(&stat->thread, &attr, timer_thread, par);
	if (status) {
		printf("Failed to create thread: %s\n", strerror(status));
		exit(EXIT_FAIL_PTHREAD);
	}

	return par;
}

void setup_socket(struct cfg_params *p, char *dest_ip_string)
{
	/* Setup dest_addr - will exit prog on invalid input */
	setup_sockaddr(p->addr_family, &p->dest_addr,
		       dest_ip_string, p->dest_port);

	/* Socket setup stuff */
	p->sockfd = Socket(p->addr_family, SOCK_DGRAM, IPPROTO_UDP);

	// TODO: Do we need some setsockopt() ?

	/* Connect to recv ICMP error messages, and to avoid the
	 * kernel performing connect/unconnect cycles
	 */
	Connect(p->sockfd,
		(struct sockaddr *)&p->dest_addr,
		sockaddr_len(&p->dest_addr));

}

static void init_params(struct cfg_params *p)
{
	memset(p, 0, sizeof(struct cfg_params));
	p->count  = 5; // DEFAULT_COUNT
	p->batch = 10;
	// p->msg_sz = 18; /* 18 +14(eth)+8(UDP)+20(IP)+4(Eth-CRC) = 64 bytes */
	p->msg_sz = 1472; /* +14(eth)+8(UDP)+20(IP) = 1514 bytes */
	p->addr_family = AF_INET; /* Default address family */
	p->dest_port = 6666;
	p->interval = DEFAULT_INTERVAL;
}

int main(int argc, char *argv[])
{
	struct thread_param *thread;
	int c, longindex = 0;
	struct cfg_params p;
	char *dest_ip_str;

	init_params(&p); /* Default settings */

	/* Parse commands line args */
	while ((c = getopt_long(argc, argv, "h6c:p:m:v:b:",
				long_options, &longindex)) != -1) {
		if (c == 'c') p.count       = atoi(optarg);
		if (c == 'p') p.dest_port   = atoi(optarg);
		if (c == 'm') p.msg_sz      = atoi(optarg);
		if (c == 'b') p.batch       = atoi(optarg);
		if (c == '6') p.addr_family = AF_INET6;
		if (c == 'v') verbose     = optarg ? atoi(optarg) : 1;
		if (c == 'h' || c == '?') return usage(argv);
	}
	if (optind >= argc) {
		fprintf(stderr,
			"Expected dest IP-address argument after options\n");
		return usage(argv);
	}
	dest_ip_str = argv[optind];
	if (verbose > 0)
		printf("Destination IP:%s port:%d\n", dest_ip_str, p.dest_port);

	/* Setup socket - will exit prog on invalid input */
	setup_socket(&p, dest_ip_str);

	socket_send(p.sockfd, p.msg_sz, p.batch);

	signal(SIGINT, sighand);
	signal(SIGTERM, sighand);
	signal(SIGUSR1, sighand);

	thread = setup_pthread(&p);

	while (!shutdown_global) {
		sleep(0.5);

		//if (p.count && thread->stats->cycles >= p.count)
		//	break;
	}
	printf("Main shutdown\n");

	/* Shutdown pthread before calling free */
	if (thread->stats->thread_started > 0)
		pthread_kill(thread->stats->thread, SIGTERM);
	if (thread->stats->thread_started)
		pthread_join(thread->stats->thread, NULL);

	free(thread->stats);
	free(thread);

	printf("Main exit\n");
	return EXIT_OK;
}
