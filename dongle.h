#ifndef _DONGLE_H
#define _DONGLE_H

#include "list.h"

#define DEVLIST_ZEROCD	(1 << 0)

struct _dongle {
	libusb_device_handle 	*d_handle;
#define DONGLE_STATE_ZEROCD	0
#define DONGLE_STATE_READY	1
#define DONGLE_STATE_LIVE	2
	unsigned int	 	d_state;
	struct list_head	d_list;
	char 			*d_serial;
	char			*d_label;
};

struct _dongle *dongle__open(libusb_device *dev, unsigned int flags,
				uint16_t serial, uint16_t label);

#endif /* _DONGLE_H */
