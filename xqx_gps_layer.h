// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

/*

   GPS layer draws current position on the screen and also centers map around
   it, if enabled.

 */

#ifndef XQX_GPS_LAYER_H__
#define XQX_GPS_LAYER_H__

#include "xqx_gps.h"

struct xqx_gps_layer
{
	struct xqx_view_layer common;
	struct xqx_gps_notify gps_notify;
	int32_t px, py, pz;
	double epx, epy;
	int locked;
	int state;
};

struct xqx_gps_layer *xqx_make_gps_layer(void);

void xqx_discard_gps_layer(struct xqx_gps_layer *gps);

#endif /* XQX_GPS_LAYER_H__ */
