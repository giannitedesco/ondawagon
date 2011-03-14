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
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

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
	free(d->d_label);
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
		return 0;
	}

	for(ret = 1, i = 0; i < conf->bNumInterfaces; i++) {
		if ( libusb_detach_kernel_driver(h, i) && errno != ENODATA )
			ret = 0;
	}

	libusb_free_config_descriptor(conf);
	return ret;
}

int dongle_needs_ready(dongle_t d)
{
	return d->d_state == DONGLE_STATE_ZEROCD;
}

int dongle__make_live(struct _dongle *d)
{
	struct libusb_config_descriptor *conf;
	libusb_device *dev;
	unsigned int i;
	int ret;

	if ( d->d_state >= DONGLE_STATE_LIVE )
		return 1;

	dev = libusb_get_device(d->d_handle);
	if ( libusb_get_active_config_descriptor(dev, &conf) ) {
		return 0;
	}

	/* Claim all vendor specific interfaces */
	for(ret = 1, i = 0; i < conf->bNumInterfaces; i++) {
		if ( conf->interface[i].altsetting[0].bInterfaceClass != 0xff )
			continue;
		if ( conf->interface[i].altsetting[0].bInterfaceSubClass != 0xff)
			continue;
		if ( conf->interface[i].altsetting[0].bInterfaceProtocol != 0xff )
			continue;
		libusb_detach_kernel_driver(d->d_handle, i);
		if ( libusb_claim_interface(d->d_handle, i) ) {
			fprintf(stderr, "%s: libusb_claim_interface: %s\n",
				odw_cmd, system_err());
			libusb_free_config_descriptor(conf);
			return 0;
		}
		printf("%s: %s: claimed interface %u\n",
			odw_cmd, d->d_serial, i);
	}

	d->d_state = DONGLE_STATE_LIVE;
	libusb_free_config_descriptor(conf);
	return 1;
}

int dongle_ready(dongle_t d)
{
	static const uint8_t buf[0x1f] = {
		0x55, 0x53, 0x42, 0x43, 0x68, 0xcd, 0xb7, 0xff,
		0x24, 0x00, 0x00, 0x00, 0x80, 0x00, 0x06, 0x85,
		0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	int ret;

	if ( d->d_state != DONGLE_STATE_ZEROCD )
		return 1;

	printf("%s; Mode-switching %s\n", odw_cmd, d->d_serial);
	if ( !kill_kernel_driver(d->d_handle) ) {
		fprintf(stderr, "%s: kill_kernel_driver: %s\n",
			odw_cmd, system_err());
		return 0;
	}

	if ( libusb_set_configuration(d->d_handle, 1) ) {
		fprintf(stderr, "%s: libusb_set_configuration: %s\n",
			odw_cmd, system_err());
		return 0;
	}
	if ( libusb_claim_interface(d->d_handle, 0) ) {
		fprintf(stderr, "%s: libusb_claim_interface: %s\n",
			odw_cmd, system_err());
		return 0;
	}

	if ( libusb_bulk_transfer(d->d_handle, 1,
				(uint8_t *)buf, sizeof(buf),
				&ret, 1000) || ret != sizeof(buf) ) {
		fprintf(stderr, "%s: libusb_bulk_transfer: %s\n",
			odw_cmd, system_err());
		return 0;
	}

	if ( libusb_reset_device(d->d_handle) ) {
		fprintf(stderr, "%s: libusb_reset_device: %s\n",
			odw_cmd, system_err());
		return 0;
	}

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
		d->d_state = DONGLE_STATE_ZEROCD;
	}else{
		d->d_state = DONGLE_STATE_READY;
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

int dongle_atcmd(dongle_t d, const char *cmd)
{
	uint8_t buf[256];
	size_t cmd_len;
	int ret;

	if ( !dongle__make_live(d) )
		return 0;

	cmd_len = strlen(cmd);

	if ( libusb_bulk_transfer(d->d_handle, 2,
					(uint8_t *)cmd, cmd_len, &ret,
					1000) || (size_t)ret != cmd_len ) {
		fprintf(stderr, "%s: libusb_bulk_transfer: %s\n",
			odw_cmd, system_err());
		return 0;
	}

	if ( libusb_bulk_transfer(d->d_handle, LIBUSB_ENDPOINT_IN | 2,
					buf, sizeof(buf),
					&ret, 1000) || ret <= 0 ) {
		fprintf(stderr, "%s: libusb_bulk_transfer: %s\n",
			odw_cmd, system_err());
		return 0;
	}

	printf("<<< %.*s\n", ret, buf);
	return 1;
}
