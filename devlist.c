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
#include <ctype.h>
#include <assert.h>

#include "ondawagon.h"
#include "dongle.h"

static const struct devlist {
	uint16_t vendor;
	uint16_t product;
	uint16_t serial;
	uint16_t label;
	unsigned int flags;
}devlist[] = {
	/* ordered by vendor then product */
	{0x19d2, 0x0103, 4, 2, DEVLIST_ZEROCD},
	{0x19d2, 0x1007, 3, 1, DEVLIST_ZEROCD},
	{0x19d2, 0x1008, 3, 1, 0},
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
				unsigned int *flags,
				uint16_t *serial,
				uint16_t *label)
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

		cmp = needle - haystack;
		if ( cmp < 0 ) {
			n = i;
		}else if ( cmp > 0 ) {
			d = d + (i + 1U);
			n = n - (i + 1U);
		}else{
			*flags = d[i].flags;
			*serial = d[i].serial;
			*label = d[i].label;
			return 1;
		}
	}

	return 0;
}

static int do_device(libusb_device *dev, struct list_head *list)
{
	struct libusb_device_descriptor desc;
	uint16_t serial, label;
	unsigned int flags;
	struct _dongle *d;

	if ( libusb_get_device_descriptor(dev, &desc) )
		return 0;

	if ( !find_device(desc.idVendor, desc.idProduct,
				&flags, &serial, &label) )
		return 1; /* we don't care about this device */

	d = dongle__open(dev, flags, serial, label);
	if ( NULL == d )
		return 0;

#if 0
	printf("%03d.%03d = %04x:%04x -> %s\n",
		libusb_get_bus_number(dev),
		libusb_get_device_address(dev),
		desc.idVendor, desc.idProduct, d->d_serial);
#endif

	list_add_tail(&d->d_list, list);
	return 1;
}

static int do_all_devices(dongle_t **dongles, size_t *nmemb)
{
	libusb_device **devlist;
	struct _dongle *d, *tmp;
	ssize_t numdev, i;
	LIST_HEAD(list);
	int ret = 0;
	size_t n;

	*dongles = NULL;
	*nmemb = 0;

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

	n = 0;
	list_for_each_entry(d, &list, d_list) {
		n++;
	}

	*dongles = calloc(n, sizeof(*dongles));
	if ( NULL == *dongles ) {
		ret = 0;
		goto out;
	}

	n = 0;
	list_for_each_entry_safe(d, tmp, &list, d_list) {
		(*dongles)[n++] = d;
		list_del(&d->d_list);
	}

	*nmemb = n;
out:
	return ret;
}

int dongle_list_all(dongle_t **dev, size_t *nmemb)
{
	return do_all_devices(dev, nmemb);
}

dongle_t dongle_open(const char *serial)
{
	return NULL;
}
