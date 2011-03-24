#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <fcntl.h>
#include <unistd.h>

#include "ondawagon.h"
#include "tapif.h"

#define DEV_NET_TUN	"/dev/net/tun"

struct _tapif {
	int fd;
	char ifname[IFNAMSIZ];
	size_t mtu;
};

static int fd_nonblock(int fd)
{
	int fl;

	fl = fcntl(fd, F_GETFL);
	if ( fl < 0 )
		return 0;

	fl |= O_NONBLOCK;

	fl = fcntl(fd, F_SETFL, fl);
	if ( fl < 0 )
		return 0;

	return 1;
}

tapif_t tapif_open(const char *ifname)
{
	struct _tapif *t;
	struct ifreq ifr;

	t = calloc(1, sizeof(*t));
	if ( NULL == t )
		goto err;

	t->fd = open(DEV_NET_TUN, O_RDWR);
	if ( t->fd < 0 )
		goto err_free;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	if ( ifname && *ifname ) {
		snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
	}else{
		snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", "tap%d");
	}

	if ( ioctl(t->fd, TUNSETIFF, &ifr) ) {
		fprintf(stderr, "%s: TUNSETIFF: %s\n", odw_cmd, os_err());
		goto err_close;
	}

	if ( !fd_nonblock(t->fd) )
		goto err_close;

	snprintf(t->ifname, sizeof(t->ifname), "%s", ifr.ifr_name);
	printf("%s: %s\n", __func__, t->ifname);
	return t;
err_close:
	close(t->fd);
err_free:
	free(t);
err:
	return NULL;
}

void tapif_close(tapif_t t)
{
	if ( t ) {
		close(t->fd);
		free(t);
	}
}
