
// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#include <gps.h>

#include "xqx_view.h"
#include "xqx_gps_layer.h"
#include "xqx_projection.h"

static void gps_layer_render(void *gl_i, struct xqx_view *vw, gp_pixmap *pixmap, struct xqx_rectangle *rect)
{
	struct xqx_gps_layer *gl = gl_i;
	int64_t x, y;
	int64_t ex, ey;

	(void) rect;

	if (gl->state == 0)
		return;

	x = gl->px;
	x -= vw->center.x;
	x *= vw->scale_px;
	x /= vw->scale_cx;
	x /= vw->scale_main;
	x += vw->w / 2;

	ex = gl->epx * 16;
	ex *= vw->scale_px;
	ex /= vw->scale_cx;
	ex /= vw->scale_main;
	ex = ABS(ex);

	y = gl->py;
	y -= vw->center.y;
	y *= vw->scale_py;
	y /= vw->scale_cy;
	y /= vw->scale_main;
	y += vw->h / 2;

	ey = gl->epy * 16;
	ey *= vw->scale_py;
	ey /= vw->scale_cy;
	ey /= vw->scale_main;
	ey = ABS(ey);

	/* Scale the circle size by the reported error */
	gp_size r = MAX(4, MAX(ex+1, ey+1));

	gp_pixel red = gp_rgb_to_pixmap_pixel(0xff, 0x00, 0x00, pixmap);

	gp_fill_ring(pixmap, x, y, r, r-2, red);
}

static void gps_msg_cb(struct xqx_gps_notify *self, enum xqx_gps_msg_type type, void *data)
{
	struct xqx_gps_layer *gl = CONTAINER_OF(self, struct xqx_gps_layer, gps_notify);
	struct xqx_view *vw = gl->common.view;
	struct gps_fix_t *fix = data;

	if (type != XQX_GPS_MSG_FIX)
		return;

	if (gl == NULL || !vw->active_map->epsg)
		return;

	gl->state = fix->mode;

	if (fix->mode < MODE_2D)
		return;

	xqx_wgs84_to_coords(vw->active_map->epsg, fix->latitude, fix->longitude, fix->altitude,
			    &gl->px, &gl->py, &gl->pz);

	gl->epx = fix->epx;
	gl->epy = fix->epy;

	if (gl->locked)
		xqx_view_set_center(vw, gl->px, gl->py);
}

struct xqx_gps_layer *xqx_make_gps_layer(void)
{
	struct xqx_gps_layer *gl = calloc(1, sizeof(struct xqx_gps_layer));

	if (!gl)
		return NULL;

	gl->common.render_cb = gps_layer_render;
	gl->locked = 1;

	gl->gps_notify.gps_msg_cb = gps_msg_cb;

	xqx_gps_register_notify(&gl->gps_notify);

	return gl;
}

void xqx_discard_gps_layer(struct xqx_gps_layer *gl)
{
	xqx_gps_unregister_notify(&gl->gps_notify);
	free(gl);
}
