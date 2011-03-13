/*
 * This file is part of ondawagon
 * Copyright (c) 2011 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <libusb-1.0/libusb.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "ondawagon.h"
#include "dongle.h"

static const struct devlist {
	uint16_t vendor;
	uint16_t product;
#define FLAG_ZEROCD	(1 << 0)
	unsigned int flags;
}devlist[] = {
	/* ordered by vendor then product */
	{0x19d2, 0x1007, FLAG_ZEROCD},
	{0x19d2, 0x1008, 0},
};

static libusb_context *ctx;

static void do_exit(void)
{
	libusb_exit(ctx);
	ctx = NULL;
}

static int do_init(void)
{
	if ( NULL == ctx && libusb_init(&ctx) )
		return 0;
	atexit(do_exit);
	return 1;
}

/* binary search the known-device table */
static int find_device(uint16_t vendor, uint16_t product,
				unsigned int *flags)
{
	const struct devlist *d = devlist;
	unsigned int n = sizeof(devlist) / sizeof(*devlist);
	uint32_t needle, haystack;

	needle = (vendor << 16) | product;

	while ( n ) {
		unsigned int i;
		int cmp;

		i = n / 2U;

		haystack = (d[i].vendor << 16) | d[i].product;

		cmp = haystack - needle;
		if ( cmp < 0 ) {
			n = i;
		}else if ( cmp > 0 ) {
			d = d + (i + 1U);
			n = n - (i + 1U);
		}else{
			*flags = d[i].flags;
			return 1;
		}
	}

	return 0;
}

static struct _dongle *open_dongle(libusb_device *dev, unsigned int flags)
{
	struct _dongle *d;

	d = calloc(1, sizeof(*d));
	if ( NULL == d ) {
		fprintf(stderr, "%s: calloc: %s\n",
			odw_cmd, system_err());
		goto err;
	}

	if ( libusb_open(dev, &d->d_handle) ) {
		fprintf(stderr, "%s: libusb_open: %s\n",
			odw_cmd, system_err());
		goto err_free;
	}

	return d;
err_free:
	free(d);
err:
	return NULL;
}

static int do_device(libusb_device *dev, struct list_head *list)
{
	struct libusb_device_descriptor desc;
	unsigned int flags;
	struct _dongle *d;

	if ( libusb_get_device_descriptor(dev, &desc) )
		return 0;

	if ( !find_device(desc.idVendor, desc.idProduct, &flags) )
		return 1; /* we don't care about this device */

	printf("%03d.%03d = %04x:%04x (flags 0x%x)\n",
		libusb_get_bus_number(dev),
		libusb_get_device_address(dev),
		desc.idVendor, desc.idProduct, flags);
	
	d = open_dongle(dev, flags);
	if ( NULL == d )
		return 0;
	
	list_add_tail(&d->d_list, list);
	return 1;
}

static int do_all_devices(dongle_t *dongles, size_t *nmemb)
{
	libusb_device **devlist;
	ssize_t numdev, i;
	int ret = 0;
	LIST_HEAD(list);

	if ( !do_init() )
		goto out;

	numdev = libusb_get_device_list(ctx, &devlist);
	if ( numdev <= 0 )
		goto out;

	for(ret = 1, i = 0; i < numdev; i++) {
		if ( !do_device(devlist[i], &list) )
			ret = 0;
	}

	libusb_free_device_list(devlist, 1);
out:
	return ret;
}

int dongle_list_all(dongle_t *dev, size_t *nmemb)
{
	return do_all_devices(dev, nmemb);
}

dongle_t dongle_open(const char *serial)
{
	return NULL;
}
