#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/if_packet.h>
#include <errno.h>

//#include <stdio.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <sys/socket.h>
//#include <sys/mman.h>
//#include <linux/filter.h>
//#include <ctype.h>
//#include <fcntl.h>
//#include <unistd.h>
//#include <bits/wordsize.h>
//#include <net/ethernet.h>
//#include <netinet/ip.h>
//#include <arpa/inet.h>
//#include <stdint.h>
//#include <string.h>
//#include <assert.h>
//#include <net/if.h>
//#include <inttypes.h>
//#include <poll.h>
//#include <unistd.h>

#ifndef PACKET_QDISC_BYPASS
#define PACKET_QDISC_BYPASS 20
#endif

/* Avail in kernel >= 3.14
 * in commit d346a3fae3 (packet: introduce PACKET_QDISC_BYPASS socket option)
 */
void set_sock_qdisc_bypass(int fd, int verbose)
{
	int ret, val = 1;

	ret = setsockopt(fd, SOL_PACKET, PACKET_QDISC_BYPASS, &val, sizeof(val));
	if (ret < 0) {
		printf("[DEBUG] %s(): err:%d errno:%d\n", __func__, ret, errno);
		if (errno == ENOPROTOOPT) {
			if (verbose)
				printf("No kernel support for PACKET_QDISC_BYPASS"
				       " (kernel < 3.14?)\n");
		} else {
			perror("Cannot set PACKET_QDISC_BYPASS");
		}
	} else
		if (verbose) printf("Enabled kernel qdisc bypass\n");
}

int pf_tx_socket(int ver)
{
        int ret, val = 1;

	/* Don't use proto htons(ETH_P_ALL) as we only want to transmit */
	int sock = socket(PF_PACKET, SOCK_RAW, 0);
        if (sock == -1) {
                perror("Creation of RAW PF_SOCKET failed!\n");
                exit(1);
        }

        ret = setsockopt(sock, SOL_PACKET, PACKET_VERSION, &ver, sizeof(ver));
        if (ret == -1) {
                perror("setsockopt");
                exit(1);
        }

        return sock;
}

int main(int argc, char **argv)
{
	printf("Lame RAW/PF_PACKET socket TX test program\n");

	int sock = pf_tx_socket(0);

	set_sock_qdisc_bypass(sock, 1);

	return 0;
}
