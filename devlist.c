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
#define FLAG_ZEROCD	(1 << 0)
	unsigned int flags;
}devlist[] = {
	/* ordered by vendor then product */
	{0x19d2, 0x1007, FLAG_ZEROCD},
	{0x19d2, 0x1008, 0},
	{0x19d2, 0x0103, FLAG_ZEROCD},
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

#if 0
static int kill_kernel_driver(libusb_device *dev, libusb_device_handle *h)
{
	struct libusb_config_descriptor *conf;
	unsigned int i;
	int ret;

	if ( libusb_get_active_config_descriptor(dev, &conf) )
		return 0;

	for(ret = 1, i = 0; i < conf->bNumInterfaces; i++) {
		if ( !libusb_detach_kernel_driver(h, i) )
			ret = 0;
	}

	return ret;
}
#endif

static char *unidecode(const uint8_t *in, size_t len)
{
	const uint8_t *end;
	char *out, *ptr;

	end = in + len;
	len /= 2;

	out = malloc(len + 1);
	if ( NULL == out )
		return NULL;

	for(ptr = out; in < end; ptr++, in += 2) {
		/* ouch */
		(*ptr) = (in[1] == 0 && isprint(in[0])) ? in[0] : '?';
	}

	(*ptr) = '\0';
	return out;
}

static char *get_serial(struct _dongle *d)
{
	struct {
		uint8_t str;
		uint8_t type;
		uint8_t lang_lo;
		uint8_t lang_hi;
	}__attribute__((packed)) desc;
	uint8_t buf[256];
	int ret;

	ret = libusb_get_string_descriptor(d->d_handle, 0, 0,
					(uint8_t *)&desc, sizeof(desc));
	if ( ret != sizeof(desc) )
		return NULL;
	if ( desc.type != LIBUSB_DT_STRING )
		return NULL;

	ret = libusb_get_string_descriptor(d->d_handle, desc.str,
					(desc.lang_hi << 8) | desc.lang_lo,
					buf, sizeof(buf));
	if ( ret <= 2 )
		return NULL;
	if ( buf[0] != ret )
		return NULL;
	if ( buf[1] != LIBUSB_DT_STRING )
		return NULL;

	return unidecode(buf + 2, ret - 2);
}

void dongle_close(dongle_t d)
{
	libusb_close(d->d_handle);
	free(d->d_serial);
	free(d);
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

	if ( flags & FLAG_ZEROCD ) {
		//if ( !kill_kernel_driver(dev, d->d_handle) )
		//	goto err_close;
		d->d_state = DONGLE_STATE_ZEROCD;
	}else{
		d->d_state = DONGLE_STATE_LIVE;
	}

	d->d_serial = get_serial(d);
	if ( NULL == d->d_serial )
		goto err_close;

	return d;
err_close:
	libusb_close(d->d_handle);
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

	d = open_dongle(dev, flags);
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

char *dongle_serial(dongle_t d)
{
	assert(NULL != d->d_serial);
	return d->d_serial;
}

static int do_all_devices(dongle_t **dongles, size_t *nmemb)
{
	libusb_device **devlist;
	struct _dongle *d;
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
	list_for_each_entry(d, &list, d_list) {
		(*dongles)[n++] = d;
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
