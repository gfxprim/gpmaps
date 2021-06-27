// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

/*


 */

#ifndef XQX_WAYPOINTS_LAYER_H__
#define XQX_WAYPOINTS_LAYER_H__

#include "xqx_waypoints.h"

struct xqx_waypoints_layer
{
	struct xqx_view_layer common;
	struct xqx_path *path;
};

struct xqx_waypoints_layer *xqx_make_waypoints_layer(struct xqx_path *path);

void xqx_discard_waypoints_layer(struct xqx_waypoints_layer *wl);

#endif /* XQX_WAYPOINTS_LAYER_H__ */
