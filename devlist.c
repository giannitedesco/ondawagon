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

static void do_device(libusb_device *dev)
{
	struct libusb_device_descriptor d;
	unsigned int flags;

	if ( libusb_get_device_descriptor(dev, &d) )
		return;

	if ( !find_device(d.idVendor, d.idProduct, &flags) )
		return;

	printf("%03d.%03d = %04x:%04x (flags 0x%x)\n",
		libusb_get_bus_number(dev),
		libusb_get_device_address(dev),
		d.idVendor, d.idProduct, flags);
}

static int do_all_devices(void)
{
	libusb_device **devlist;
	ssize_t numdev, i;
	int ret = 0;

	if ( !do_init() )
		goto out;

	numdev = libusb_get_device_list(ctx, &devlist);
	if ( numdev <= 0 )
		goto out;

	for(i = 0; i < numdev; i++) {
		do_device(devlist[i]);
	}

	ret = 1;

	libusb_free_device_list(devlist, 1);
out:
	return ret;
}

int dongle_list_all(dongle_t *dev, size_t *nmemb)
{
	return do_all_devices();
}

dongle_t dongle_open(const char *serial)
{
	return NULL;
}
