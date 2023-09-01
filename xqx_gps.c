// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#include <gps.h>
#include <widgets/gp_widgets.h>
#include "xqx_gps.h"

static struct gps_data_t gpsdata;

static LIST_HEAD(root);

static struct list_head *notify_root = &root;

static const char *gps_addr = "127.0.0.1";
static const char *gps_port = "2947";
static unsigned int gps_reconnect_delay_ms = 10000;

static void gps_notify(struct xqx_gps_notify *nt, enum xqx_gps_msg_type type, void *data)
{
	nt->gps_msg_cb(nt, type, data);
}

static void gps_notify_broadcast(enum xqx_gps_msg_type type, void *data)
{
	struct xqx_gps_notify *nt;

	LIST_FOREACH(notify_root, i) {
		nt = LIST_ENTRY(i, struct xqx_gps_notify, list);
		gps_notify(nt, type, data);
	}
}

static uint32_t gps_notify_no_data(gp_timer *self)
{
	(void)self;

	gps_notify_broadcast(XQX_GPS_MSG_NO_DATA, NULL);

	return GP_TIMER_STOP;
}

static uint32_t gpsd_reconnect(gp_timer *self)
{
	(void)self;

	if (xqx_gps_connect())
		return GP_TIMER_STOP;
	else
		return gps_reconnect_delay_ms;
}

static struct gp_timer gps_read_timeout = {
	.callback = gps_notify_no_data,
	.id = "gpsd read timeout"
};

static struct gp_timer gps_reconnect = {
	.callback = gpsd_reconnect,
	.id = "gpsd reconnect",
};

static void gps_schedulle_reconnect(void)
{
	if (!gps_reconnect_delay_ms)
		return;

	gps_reconnect.expires = gps_reconnect_delay_ms;
	gp_widgets_timer_ins(&gps_reconnect);
}

static void gps_cancel_reconnect(void)
{
	if (!gps_reconnect_delay_ms)
		return;

	gp_widgets_timer_rem(&gps_reconnect);
}

static int read_gps(struct gp_fd *self)
{
	int ret;

	(void) self;

	gp_widgets_timer_rem(&gps_read_timeout);

#if GPSD_API_MAJOR_VERSION >= 10
        ret = gps_read(&gpsdata, NULL, 0);
#else
        ret = gps_read(&gpsdata);
#endif

	if (ret < 0) {
		gps_close(&gpsdata);
		gpsdata.gps_fd = 0;

		gps_notify_broadcast(XQX_GPS_MSG_DISCONNECTED, NULL);

		gps_schedulle_reconnect();

		return 1;
	}

	gps_notify_broadcast(XQX_GPS_MSG_FIX, &gpsdata.fix);

	gps_read_timeout.expires = 5000;
	gp_widgets_timer_ins(&gps_read_timeout);

	return 0;
}

static int gps_is_connected(void)
{
	return gpsdata.gps_fd != 0;
}

static gp_fd gps_fd = {
	.events = GP_POLLIN,
	.event = read_gps,
};

static void gps_connect(void)
{
	if (gps_open(gps_addr, gps_port, &gpsdata)) {
		gpsdata.gps_fd = 0;
		return;
	}

	gps_stream(&gpsdata, WATCH_ENABLE, NULL);

	gps_fd.fd = gpsdata.gps_fd;

	gp_widget_poll_add(&gps_fd);

	gp_widgets_timer_ins(&gps_read_timeout);
}

void gps_disconnect(void)
{
	gp_widget_poll_rem(&gps_fd);
	gps_close(&gpsdata);
	gpsdata.gps_fd = 0;
}

void xqx_gps_init(const char *addr, const char *port, unsigned int reconnect_delay)
{
	if (gps_is_connected())
		return;

	gps_addr = addr;
	gps_port = port;
	gps_reconnect_delay_ms = reconnect_delay * 1000;
}

void xqx_gps_unregister_notify(struct xqx_gps_notify *notify)
{
	list_remove(&notify->list);
}

void xqx_gps_register_notify(struct xqx_gps_notify *notify)
{
	list_append(notify_root, &notify->list);

	if (gps_is_connected())
		gps_notify(notify, XQX_GPS_MSG_CONNECTED, NULL);
	else
		gps_notify(notify, XQX_GPS_MSG_DISCONNECTED, NULL);
}

int xqx_gps_connect(void)
{
	if (gps_is_connected())
		return 1;

	gps_connect();

	if (!gps_is_connected()) {
		gps_schedulle_reconnect();
		return 0;
	}

	gps_notify_broadcast(XQX_GPS_MSG_CONNECTED, NULL);

	return 1;
}

void xqx_gps_disconnect(void)
{
	gps_disconnect();
	gps_cancel_reconnect();
	gps_notify_broadcast(XQX_GPS_MSG_DISCONNECTED, NULL);
}
