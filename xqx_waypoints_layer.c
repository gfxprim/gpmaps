// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#include "xqx_view.h"
#include "xqx_waypoints_layer.h"
#include "xqx_projection.h"

static void waypoints_layer_render(void *wl_i, struct xqx_view *vw, gp_pixmap *pixmap, struct xqx_rectangle *rect)
{
	struct xqx_waypoints_layer *wl = wl_i;
	struct xqx_waypoint *waypoint;
	int first = 1;
	int32_t tx, ty, tz, px, py;

	(void) vw;
	(void) rect;

	LIST_FOREACH(&wl->path->waypoints, i) {
		waypoint = LIST_ENTRY(i, struct xqx_waypoint, list);

		xqx_wgs84_to_coords(vw->active_map->epsg,
		                    waypoint->lat, waypoint->lon, waypoint->alt,
				    &tx, &ty, &tz);

		int64_t x = tx;
		x -= vw->center.x;
		x *= vw->scale_px;
		x /= vw->scale_cx;
		x /= vw->scale_main;
		x += vw->w / 2;

		int64_t y = ty;
		y -= vw->center.y;
		y *= vw->scale_py;
		y /= vw->scale_cy;
		y /= vw->scale_main;
		y += vw->h / 2;

		gp_fill_circle(pixmap, x, y, 3, 0xff);

		if (first) {
			first = 0;
			px = x;
			py = y;
			continue;
		}

		gp_line(pixmap, x, y, px, py, 0x000000);

		px = x;
		py = y;
	}
}

struct xqx_waypoints_layer *xqx_make_waypoints_layer(struct xqx_path *path)
{
	struct xqx_waypoints_layer *wl = calloc(1, sizeof(struct xqx_waypoints_layer));

	if (!wl)
		return NULL;

	wl->common.render_cb = waypoints_layer_render;
	wl->path = path;

	return wl;
}

void xqx_discard_waypoints_layer(struct xqx_waypoints_layer *wl)
{
	free(wl);
}
