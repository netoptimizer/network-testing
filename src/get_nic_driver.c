/* SPDX-License-Identifier: GPL-2.0+ */

/* C-code example of getting NIC driver name used by Linux net_device */
static const char *__doc__ =
 "Extract NIC driver name (example in C-code)\n";

#include <sys/ioctl.h>
#include <net/if.h> /* man netdevice(7) */
#include <linux/ethtool.h>
#include <linux/sockios.h>
#ifndef SIOCETHTOOL
#define SIOCETHTOOL	0x8946		/* Ethtool interface		*/
#endif

#include <stdio.h>
#include <string.h> /* strlen */
#include <errno.h>
#include <getopt.h>
#include <unistd.h> /* close */

struct ioctl_context {
	int fd;			/* socket suitable for ethtool ioctl */
	struct ifreq ifr;	/* ifreq suitable for ethtool ioctl */
};

/* Exit return codes */
#define EXIT_OK			0 /* EXIT_SUCCESS */
#define EXIT_FAIL		1 /* EXIT_FAILURE */
#define EXIT_FAIL_OPTION        2

static const struct option long_options[] = {
        {"help",        no_argument,            NULL, 'h' },
        {"dev",         required_argument,      NULL, 'd' },
        {0, 0, NULL,  0 }
};

static void usage(char *argv[])
{
        int i;

        printf("\nDOCUMENTATION:\n%s\n", __doc__);
        printf(" Usage: %s (options-see-below)\n", argv[0]);
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
        printf("\n");
}

static int ioctl_init(struct ioctl_context *ctx, const char *devname)
{
	if (strlen(devname) >= IFNAMSIZ) {
		fprintf(stderr, "Device name longer than %u characters\n",
			IFNAMSIZ - 1);
		return 1;
	}

	/* Setup our control structures. */
	memset(&ctx->ifr, 0, sizeof(ctx->ifr));
	strncpy(ctx->ifr.ifr_name, devname, IFNAMSIZ);

	/* Open control socket. */
	ctx->fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (ctx->fd < 0) {
		perror("Cannot get control socket");
		return 70;
	}

	return 0;
}

int ioctl_send(struct ioctl_context *ctx, void *cmd)
{
	ctx->ifr.ifr_data = cmd;
	return ioctl(ctx->fd, SIOCETHTOOL, &ctx->ifr);
}

int get_driver_info(const char* devname, struct ethtool_drvinfo *drvinfo)
{
	struct ioctl_context ctx;
	int err;

	if ((err = ioctl_init(&ctx, devname))) {
		perror("Cannot init ioctl fd\n");
		return err;
	}

	drvinfo->cmd = ETHTOOL_GDRVINFO;
	err = ioctl_send(&ctx, drvinfo);
	if (err < 0) {
		perror("Cannot get driver information");
		return 71;
	}

	close(ctx.fd);
	return 0;
}

int main(int argc, char **argv)
{
	int opt, longindex = 0;

	int ifindex = -1;
	char ifname_buf[IF_NAMESIZE];
	char *ifname;

	/* The driver info is stored in this struct */
	struct ethtool_drvinfo drvinfo = {};

	/* Parse commands line args */
        while ((opt = getopt_long(argc, argv, "d:",
                                  long_options, &longindex)) != -1) {
                switch (opt) {
                case 'd':
                        if (strlen(optarg) >= IF_NAMESIZE) {
                                fprintf(stderr, "ERR: --dev name too long\n");
                                goto error;
                        }
                        ifname = (char *)&ifname_buf;
                        strncpy(ifname, optarg, IF_NAMESIZE);
                        ifindex = if_nametoindex(ifname);
                        if (ifindex == 0) {
                                fprintf(stderr,
                                        "ERR: --dev name unknown err(%d):%s\n",
                                        errno, strerror(errno));
                                goto error;
                        }
                        break;
		case 'h':
                error:
                default:
                        usage(argv);
                        return EXIT_FAIL_OPTION;
                }
        }

	/* Required option */
        if (ifindex == -1) {
                fprintf(stderr, "ERR: required option --dev missing\n");
                usage(argv);
                return EXIT_FAIL_OPTION;
        }

	get_driver_info(ifname, &drvinfo);

	printf("net_device: %s (ifindex: %d) use driver: %s\n",
	       ifname_buf, ifindex, drvinfo.driver);

	return EXIT_OK;
}
