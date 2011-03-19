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

const char *dongle_serial(dongle_t d)
{
	assert(NULL != d->d_serial);
	return d->d_serial;
}

const char *dongle_manufacturer(dongle_t d)
{
	return d->d_mnfr;
}

const char *dongle_product(dongle_t d)
{
	return d->d_product;
}

void dongle_close(dongle_t d)
{
	libusb_close(d->d_handle);
	free(d->d_product);
	free(d->d_serial);
	free(d->d_mnfr);
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

static void _hex_dumpf(FILE *f, const uint8_t *tmp, size_t len, size_t llen)
{
	size_t i, j;
	size_t line;

	if ( NULL == f || 0 == len )
		return;

	for(j = 0; j < len; j += line, tmp += line) {
		if ( j + llen > len ) {
			line = len - j;
		}else{
			line = llen;
		}

		fprintf(f, " | %05x : ", j);

		for(i = 0; i < line; i++) {
			if ( isprint(tmp[i]) ) {
				fprintf(f, "%c", tmp[i]);
			}else{
				fprintf(f, ".");
			}
		}

		for(; i < llen; i++)
			fprintf(f, " ");

		for(i = 0; i < line; i++)
			fprintf(f, " %02x", tmp[i]);

		fprintf(f, "\n");
	}
	fprintf(f, "\n");
}

static void hex_dump(const uint8_t *ptr, size_t len, size_t llen)
{
	_hex_dumpf(stdout, ptr, len, llen);
}

static int do_init_cycle(struct _dongle *d, const uint8_t *ptr, size_t len)
{
	uint8_t buf[4096];
	int ret, rc;

	printf("--- Init Cycle ---\n");

	ret = libusb_control_transfer(d->d_handle, 0x21, 0x0, 0, 4,
					(uint8_t *)ptr, len, 1000);
	if ( ret < 0 ) {
		fprintf(stderr, "%s: libusb_control_transfer_x: %s\n",
			odw_cmd, system_err());
		return 0;
	}

	do {
		usleep(1000000);
		rc = libusb_bulk_transfer(d->d_handle,
						LIBUSB_ENDPOINT_IN | 6,
						buf, 8,
						&ret, 10000);
		if ( rc < 0 ) {
			fprintf(stderr, "%s: libusb_bulk_transfer: %s\n",
				odw_cmd, system_err());
			return 0;
		}
		printf("Got %d bytes\n", ret);
		hex_dump(buf, ret, 16);
	}while(ret != 8);

	ret = libusb_control_transfer(d->d_handle, 0xa1, 0x1, 0, 4,
					buf, sizeof(buf), 1000);
	if ( ret < 0 ) {
		fprintf(stderr, "%s: libusb_control_transfer_x: %s\n",
			odw_cmd, system_err());
		return 0;
	}
	hex_dump(buf, ret, 16);

	return 1;
}

static int init_stuff(struct _dongle *d)
{
	uint8_t msg_1[2] = { 0, 0 };
	uint8_t msg_2[16] = {1, 0xf, 0, 0, 0, 0, 0, 1,
				0x21, 0, 4, 0, 1, 1, 0, 0xff};
	uint8_t msg_3[16] = {1, 0xf, 0, 0, 0, 0, 0, 2,
				0x22, 0, 4, 0, 1, 1, 0, 0x2};
	uint8_t msg_4[13] = {1, 0xc, 0, 0, 2, 1, 0, 1,
				0, 0x21, 0, 0, 0}; /* qualcomm inc */
	uint8_t msg_5[13] = {1, 0xc, 0, 0, 2, 1, 0, 2,
				0, 0x24, 0, 0, 0};
	uint8_t msg_6[13] = {1, 0xc, 0, 0, 2, 1, 0, 3,
				0, 0x25, 0, 0, 0}; /* returns IMEI */
	uint8_t msg_7[17] = {1, 0x10, 0, 0, 0, 0, 0, 3,
				0x23, 0, 5, 0, 1, 2, 0, 2, 1};
	uint8_t msg_8[16] = {1, 0xf, 0, 0, 0, 0, 0, 4,
				0x22, 0, 4, 0, 1, 1, 0, 1};
	uint8_t msg_9[16] = {1, 0xf, 0, 0, 0, 0, 0, 5,
				0x20, 0, 4, 0, 1, 1, 0, 0};
	uint8_t msg_f[7] = {0, 0xc2, 1, 0, 0, 0, 8};
	uint8_t buf[246];
	int ret, rc, i;

	ret = libusb_control_transfer(d->d_handle, 0x21, 0x02, 1, 4,
					msg_1, sizeof(msg_1), 1000);
	if ( ret < 0 ) {
		fprintf(stderr, "%s: libusb_control_transfer_1: %s\n",
			odw_cmd, system_err());
		//return 0;
	}

	if ( !do_init_cycle(d, msg_2, sizeof(msg_2)) )
		return 0;
	if ( !do_init_cycle(d, msg_3, sizeof(msg_3)) )
		return 0;
	if ( !do_init_cycle(d, msg_4, sizeof(msg_4)) )
		return 0;
	if ( !do_init_cycle(d, msg_5, sizeof(msg_5)) )
		return 0;
	if ( !do_init_cycle(d, msg_6, sizeof(msg_6)) )
		return 0;
	if ( !do_init_cycle(d, msg_7, sizeof(msg_7)) )
		return 0;
	if ( !do_init_cycle(d, msg_8, sizeof(msg_8)) )
		return 0;
	if ( !do_init_cycle(d, msg_9, sizeof(msg_9)) )
		return 0;

	printf("--- SHould return zero ---\n");
	ret = libusb_control_transfer(d->d_handle, 0xa1, 0xfe, 0, 5,
					buf, 1, 1000);
	if ( ret < 0 ) {
		fprintf(stderr, "%s: libusb_control_transfer_final: %s\n",
			odw_cmd, system_err());
		//return 0;
	}
	hex_dump(buf, ret, 16);

	printf("--- Stuff ---\n");
	for(i = 0; i < 3; i++) {
		ret = libusb_control_transfer(d->d_handle, 0xa1, 0x21, 0, 3,
						buf, 7, 1000);
		if ( ret < 0 ) {
			fprintf(stderr, "%s: libusb_control_"
				"transfer_weird: %s\n",
				odw_cmd, system_err());
			//return 0;
		}
		hex_dump(buf, ret, 16);
	}

	ret = libusb_control_transfer(d->d_handle, 0x21, 0x20, 0, 3,
					msg_f, sizeof(msg_f), 1000);
	if ( ret < 0 ) {
		fprintf(stderr, "%s: libusb_control_transfer_1: %s\n",
			odw_cmd, system_err());
		//return 0;
	}

	ret = libusb_control_transfer(d->d_handle, 0xa1, 0x21, 0, 3,
					buf, 7, 1000);
	if ( ret < 0 ) {
		fprintf(stderr, "%s: libusb_control_"
			"transfer_weird: %s\n",
			odw_cmd, system_err());
		//return 0;
	}
	hex_dump(buf, ret, 16);

	ret = libusb_control_transfer(d->d_handle, 0x21, 0x20, 0, 3,
					msg_f, sizeof(msg_f), 1000);
	if ( ret < 0 ) {
		fprintf(stderr, "%s: libusb_control_transfer_1: %s\n",
			odw_cmd, system_err());
		//return 0;
	}

	printf("--- Switching to AT mode? ---\n");
	rc = libusb_bulk_transfer(d->d_handle, 4,
					(uint8_t *)"AT%MODE?\n", 9, &ret,
					1000);
	if ( rc < 0 || (size_t)ret != 9 ) {
		fprintf(stderr, "%s: libusb_bulk_transfer: %s\n",
			odw_cmd, system_err());
		//return 0;
	}

	rc = libusb_bulk_transfer(d->d_handle, LIBUSB_ENDPOINT_IN | 5,
					buf, sizeof(buf),
					&ret, 1000);
	if ( rc < 0 ) {
		fprintf(stderr, "%s: libusb_bulk_transfer: %s\n",
			odw_cmd, system_err());
		//return 0;
	}
	hex_dump(buf, ret, 16);

	do {
		usleep(1000000);
		rc = libusb_bulk_transfer(d->d_handle, LIBUSB_ENDPOINT_IN | 5,
						buf, sizeof(buf),
						&ret, 1000);
		if ( rc < 0 ) {
			fprintf(stderr, "%s: libusb_bulk_transfer: %s\n",
				odw_cmd, system_err());
			//return 0;
			continue;
		}
		hex_dump(buf, ret, 16);
	}while(ret != 9);
	return 1;
}

int dongle__make_live(struct _dongle *d)
{
	struct libusb_config_descriptor *conf;
	libusb_device *dev;
	unsigned int i;
	int ret;

	if ( d->d_state == DONGLE_STATE_ZEROCD ) {
		fprintf(stderr, "%s: Device needs to be made ready\n", odw_cmd);
		return 0;
	}

	if ( d->d_state >= DONGLE_STATE_LIVE )
		return 1;

	dev = libusb_get_device(d->d_handle);
	if ( libusb_get_active_config_descriptor(dev, &conf) ) {
		return 0;
	}

	/* Claim all vendor specific interfaces */
	for(ret = 1, i = 0; i < conf->bNumInterfaces; i++) {
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

	init_stuff(d);
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
	int ret, rc;

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

	rc = libusb_bulk_transfer(d->d_handle, 1,
				(uint8_t *)buf, sizeof(buf),
				&ret, 1000);
	if ( rc < 0 || (size_t)ret != sizeof(buf) ) {
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

struct _dongle *dongle__open(libusb_device *dev, unsigned int flags)
{
	struct libusb_device_descriptor desc;
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

	if ( libusb_get_device_descriptor(dev, &desc) )
		goto err_close;

	d->d_serial = get_string(d, desc.iSerialNumber);
	if ( NULL == d->d_serial ) {
		fprintf(stderr, "%s: get_serial: %s\n",
			odw_cmd, system_err());
		goto err_close;
	}

	d->d_product = get_string(d, desc.iProduct);
	if ( NULL == d->d_product ) {
		fprintf(stderr, "%s: get_label: %s\n",
			odw_cmd, system_err());
		goto err_strings;
	}

	d->d_mnfr = get_string(d, desc.iManufacturer);
	if ( NULL == d->d_mnfr ) {
		fprintf(stderr, "%s: get_label: %s\n",
			odw_cmd, system_err());
		goto err_strings;
	}

	return d;
err_strings:
	free(d->d_serial);
	free(d->d_mnfr);
	free(d->d_product);
err_close:
	libusb_close(d->d_handle);
err_free:
	free(d);
err:
	return NULL;
}

int dongle_atcmd(dongle_t d, const char *cmd)
{
	uint8_t buf[4096];
	size_t cmd_len;
	int ret, rc;

	if ( !dongle__make_live(d) )
		return 0;

	cmd_len = strlen(cmd);

	//printf("ATCMD\n");
	//hex_dump((const uint8_t *)cmd, cmd_len, 16);
	rc = libusb_bulk_transfer(d->d_handle, 2,
					(uint8_t *)cmd, cmd_len, &ret,
					1000);
	if ( rc < 0 || (size_t)ret != cmd_len ) {
		fprintf(stderr, "%s: libusb_bulk_transfer: %s\n",
			odw_cmd, system_err());
		return 0;
	}

	rc = libusb_bulk_transfer(d->d_handle, LIBUSB_ENDPOINT_IN | 2,
					buf, sizeof(buf),
					&ret, 1000);
	if ( rc < 0 ) {
		fprintf(stderr, "%s: libusb_bulk_transfer: %s\n",
			odw_cmd, system_err());
		return 0;
	}

	printf("<<< %.*s\n", ret, buf);
	//hex_dump(buf, ret, 16);

	return 1;
}
