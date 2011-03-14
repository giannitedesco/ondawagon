#ifndef _DONGLE_H
#define _DONGLE_H

#include "list.h"

#define DEVLIST_ZEROCD	(1 << 0)

struct _dongle {
	libusb_device_handle 	*d_handle;
#define DONGLE_STATE_ZEROCD	0
#define DONGLE_STATE_LIVE	1
	unsigned int	 	d_state;
	struct list_head	d_list;
	char 			*d_serial;
};

struct _dongle *dongle__open(libusb_device *dev, unsigned int flags);

#endif /* _DONGLE_H */
