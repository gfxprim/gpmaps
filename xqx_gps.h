// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#ifndef XQX_GPS_H__
#define XQX_GPS_H__

#include "xqx_list.h"

enum xqx_gps_msg_type {
	/* connected to gpsd */
	XQX_GPS_MSG_CONNECTED,
	/* disconnected from gpsd */
	XQX_GPS_MSG_DISCONNECTED,
	/* connected but no data in last 5 seconds */
	XQX_GPS_MSG_NO_DATA,
	/* gps_fix_t in data pointer */
	XQX_GPS_MSG_FIX,
};

struct xqx_gps_notify {
	void (*gps_msg_cb)(struct xqx_gps_notify *self, enum xqx_gps_msg_type type, void *data);
	void *priv;
	struct list_head list;
};

/*
 * Sets gpsd parameters has to be called before xqx_gps_connect()
 *
 * @addr gpsd server address, defaults to "127.0.0.1"
 * @port gpsd server port, defaults to 2947
 * @reconnect_delay reconnect delay in seconds defaults to 10
 */
void xqx_gps_init(const char *addr, const char *port, unsigned int reconnect_delay);

/*
 * Connects to gpsd server
 */
int xqx_gps_connect(void);

/*
 * Disconnects from gpsd server
 */
void xqx_gps_disconnect(void);

/*
 * Registers/unregisters a notification callback for gps connection state and data packets.
 */
void xqx_gps_register_notify(struct xqx_gps_notify *notify);
void xqx_gps_unregister_notify(struct xqx_gps_notify *notify);

#endif /* XQX_GPS_H__ */
