// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

/*

  View describes what user sees on the screen. View consists of several layers,
  the bottom layer is the map and then there are other layers that may be
  painted on the top, e.g. grid.

 */

#ifndef XQX_VIEW_H__
#define XQX_VIEW_H__

#include <widgets/gp_widgets.h>
#include <stdint.h>
#include "xqx.h"

enum xqx_view_layer_change {
	XQX_VLC_INIT,
	XQX_VLC_FINISH,
	XQX_VLC_MOVE,
	XQX_VLC_RESIZE,
	XQX_VLC_SCALE
};

struct xqx_coordinate
{
	/* UTM coordinate in 1/16 of meter */
	uint32_t x, y;
};

struct xqx_rectangle
{
	int lx, ly, hx, hy;
};

struct xqx_view
{
	int valid, used;
	struct xqx_coordinate center; /* center of view */
	int scale_px, scale_py, scale_cx, scale_cy, scale_main, scale_def;

	uint32_t w, h; /* width and height of window in pixels */
	int step_x, step_y;

	struct xqx_map **maps;
	struct xqx_map *active_map;
	int maps_num;

	gp_widget *pixmap;

	struct xqx_grid *grid;
	struct xqx_gps_layer *gps;
	struct xqx_view_layer *view_first;
	struct xqx_view_layer *view_last;
};

/*
 * Generic layer functions.
 *
 * The notify_cb callback is called before certain operations such as move or
 * scale in order to prepare for the operation.
 *
 * The render_cb callback is the function that actually draws into a pixmap.
 */
struct xqx_view_layer
{
	void (*notify_cb)(void *, struct xqx_view *, uint32_t);
	void (*render_cb)(void *, struct xqx_view *, gp_pixmap *pixmap, struct xqx_rectangle *);
	struct xqx_view_layer *prev, *next;
	struct xqx_view *view;
};

struct xqx_view *xqx_make_view(gp_widget *pixmap);

void xqx_view_pixels_to_coords(struct xqx_view *vw, int px, int py, struct xqx_coordinate *c);

void xqx_view_remove_layer(struct xqx_view *vw, struct xqx_view_layer *lr);
void xqx_view_prepend_layer(struct xqx_view *vw, struct xqx_view_layer *lr);
void xqx_view_append_layer(struct xqx_view *vw, struct xqx_view_layer *lr);

void xqx_view_enable_gps(struct xqx_view *vw);
void xqx_view_disable_gps(struct xqx_view *vw);

void xqx_view_set_center(struct xqx_view *vw, int nx, int ny);
void xqx_view_set_scale(struct xqx_view *vw, int s);
void xqx_view_choose_map(struct xqx_view *vw, int s);

/*
 * This is called when particular rectangle on a screen needs to be repainted.
 *
 * This function does not do any drawing, it just passes the request to the
 * widget library and the actual repainting is done in the widget repaint even
 * handler.
 */
void xqx_view_request_redraw(struct xqx_view *vw, int lx, int ly, int hx, int hy);

static inline void xqx_view_move(struct xqx_view *vw, int dx, int dy)
{
	xqx_view_set_center(vw, vw->center.x + dx, vw->center.y + dy);
}

static inline void xqx_view_zoom_in(struct xqx_view *vw, int coef)
{
	int64_t ns = vw->scale_main;
	ns = (ns * 1024) / coef;

	/* FIXME check for overflow ? */
	xqx_view_set_scale(vw, ns);
}

static inline void xqx_view_zoom_out(struct xqx_view *vw, int coef)
{
	int64_t ns = vw->scale_main;
	ns = (ns * coef) / 1024;

	/* FIXME check for overflow ? */
	xqx_view_set_scale(vw, ns);
}

#endif /* XQX_VIEW_H__ */
