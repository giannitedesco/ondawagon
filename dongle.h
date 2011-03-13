#ifndef _DONGLE_H
#define _DONGLE_H

#include "list.h"

struct _dongle {
	libusb_device_handle 	*d_handle;
#define DONGLE_STATE_ZEROCD	0
#define DONGLE_STATE_LIVE	1
	unsigned int	 	d_state;
	struct list_head	d_list;
	char 			*d_serial;
};

#endif /* _DONGLE_H */
