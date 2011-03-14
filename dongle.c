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

char *dongle_serial(dongle_t d)
{
	assert(NULL != d->d_serial);
	return d->d_serial;
}

char *dongle_label(dongle_t d)
{
	return d->d_label;
}

void dongle_close(dongle_t d)
{
	libusb_close(d->d_handle);
	free(d->d_serial);
	list_del(&d->d_list);
	free(d);
}

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

static char *get_string(struct _dongle *d, unsigned int idx)
{
	uint8_t buf[256];
	int ret;

	ret = libusb_get_string_descriptor(d->d_handle, idx,
					0x0409,
					buf, sizeof(buf));
	if ( ret <= 2 )
		return NULL;
	if ( buf[0] != ret )
		return NULL;
	if ( buf[1] != LIBUSB_DT_STRING )
		return NULL;

	return unidecode(buf + 2, ret - 2);
}

static int kill_kernel_driver(libusb_device_handle *h)
{
	struct libusb_config_descriptor *conf;
	libusb_device *dev;
	unsigned int i;
	int ret;

	dev = libusb_get_device(h);
	if ( libusb_get_active_config_descriptor(dev, &conf) ) {
		libusb_unref_device(dev);
		return 0;
	}
	libusb_unref_device(dev);

	for(ret = 1, i = 0; i < conf->bNumInterfaces; i++) {
		if ( !libusb_detach_kernel_driver(h, i) )
			ret = 0;
	}

	return ret;
}

int dongle_ready(dongle_t d)
{
	if ( d->d_state >= DONGLE_STATE_READY )
		return 1;
	
	assert(d->d_state == DONGLE_STATE_ZEROCD);
	if ( !kill_kernel_driver(d->d_handle) )
		return 0;

	if ( libusb_reset_device(d->d_handle) )
		return 0;

	/* Now what? */
	return 1;
}

struct _dongle *dongle__open(libusb_device *dev, unsigned int flags,
				uint16_t serial, uint16_t label)
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

	if ( flags & DEVLIST_ZEROCD ) {
		//if ( !kill_kernel_driver(dev, d->d_handle) )
		//	goto err_close;
		d->d_state = DONGLE_STATE_ZEROCD;
	}else{
		d->d_state = DONGLE_STATE_LIVE;
	}

#if 0
	do {
		unsigned int i;
		for(i = 0; i < 256; i++) {
			printf("%u: %s\n", i, get_string(d, i));
		}
	}while(0);
#endif

	d->d_serial = get_string(d, serial);
	if ( NULL == d->d_serial ) {
		fprintf(stderr, "%s: get_serial: %s\n",
			odw_cmd, system_err());
		goto err_close;
	}

	d->d_label = get_string(d, label);
	if ( NULL == d->d_label) {
		fprintf(stderr, "%s: get_label: %s\n",
			odw_cmd, system_err());
		goto err_close;
	}

	return d;
err_close:
	libusb_close(d->d_handle);
err_free:
	free(d);
err:
	return NULL;
}

