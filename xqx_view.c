// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#include "xqx_view.h"
#include "xqx_dllist.h"
#include "xqx_map_layer.h"
#include "xqx_grid_layer.h"
#include "xqx_gps_layer.h"

static inline void do_notify_layer(struct xqx_view_layer *lr, struct xqx_view *vw, enum xqx_view_layer_change change)
{
	if (lr->notify_cb)
		lr->notify_cb(lr, vw, change);
}

static inline void do_render_layer(struct xqx_view_layer *lr, struct xqx_view *vw, gp_pixmap *pixmap, struct xqx_rectangle *rect)
{
	lr->render_cb(lr, vw, pixmap, rect);
}

static void do_notify_layers(struct xqx_view *vw, enum xqx_view_layer_change change)
{
	struct xqx_view_layer *lr;

	for (lr = vw->view_first; lr != NULL; lr = lr->next)
		do_notify_layer(lr, vw, change);
}

static inline void notify_layer(struct xqx_view_layer *lr, struct xqx_view *vw, enum xqx_view_layer_change change)
{
	if (vw->valid)
		do_notify_layer(lr, vw, change);
}

static inline void notify_layers(struct xqx_view *vw, enum xqx_view_layer_change change)
{
	if (vw->valid)
		do_notify_layers(vw, change);
}

static inline void invalidate_view(struct xqx_view *vw)
{
	gp_widget_redraw(vw->pixmap);
}

void xqx_view_pixels_to_coords(struct xqx_view *vw, int px, int py, struct xqx_coordinate *c)
{
	int64_t x = px;
	int64_t y = py;

	x -= vw->w / 2;
	x *= vw->scale_cx;
	x *= vw->scale_main;
	x /= vw->scale_px;
	x += vw->center.x;

	y -= vw->h / 2;
	y *= vw->scale_cy;
	y *= vw->scale_main;
	y /= vw->scale_py;
	y += vw->center.y;

	/* FIXME check overflow */
	c->x = (int32_t) x;
	c->y = (int32_t) y;
}

static void update_step(struct xqx_view *vw)
{
	int64_t sx = 256;
	sx *= vw->scale_cx;
	sx *= vw->scale_main;
	sx /= vw->scale_px;

	int64_t sy = 256;
	sy *= vw->scale_cy;
	sy *= vw->scale_main;
	sy /= vw->scale_py;

	vw->step_x = sx;
	vw->step_y = sy;
}

static void update_first(struct xqx_view *vw, struct xqx_view_layer *lr)
{
	if (vw->view_last == lr) {
		struct xqx_map_layer *il = ((struct xqx_map_layer *)lr);

		if (!vw->used) {
			int64_t tmpx = il->map->map_w / 2;
			tmpx -= il->map->geo_pox;
			tmpx *= il->map->geo_csx;
			tmpx /= il->map->geo_psx;
			tmpx += il->map->geo_cox;
			vw->center.x = tmpx;

			int64_t tmpy = il->map->map_h / 2;
			tmpy -= il->map->geo_poy;
			tmpy *= il->map->geo_csy;
			tmpy /= il->map->geo_psy;
			tmpy += il->map->geo_coy;
			vw->center.y = tmpy;

			vw->scale_cx = il->map->geo_csx;
			vw->scale_cy = il->map->geo_csy;
			vw->scale_px = il->map->geo_psx;
			vw->scale_py = il->map->geo_psy;

			vw->scale_main = 1;
			if (il->map->num_levels > 2)
				vw->scale_main = 1 << (il->map->num_levels - 2);

			int64_t def = vw->scale_main;
			def *= vw->scale_cx;
			def /= vw->scale_px;
			vw->scale_def = abs((int) def);

			vw->used = 1;
		} else {
			int64_t oscale, nscale;
			oscale = vw->scale_def;

			vw->scale_cx = il->map->geo_csx;
			vw->scale_cy = il->map->geo_csy;
			vw->scale_px = il->map->geo_psx;
			vw->scale_py = il->map->geo_psy;

			int l;
			for (l = 0; l < il->map->num_levels; l++) {
				vw->scale_main = 1 << l;
				nscale = vw->scale_main;
				nscale *= vw->scale_cx;
				nscale /= vw->scale_px;

				if (oscale <= nscale)
					break;
			}

			if ((l > 0) && (l < il->map->num_levels) &&
			   ((oscale * 64 / nscale) < (nscale * 32 / oscale)))
				vw->scale_main /= 2;
		}

		update_step(vw);
	}
}

void xqx_view_remove_layer(struct xqx_view *vw, struct xqx_view_layer *lr)
{
	lr->view = NULL;
	DLL_REMOVE(vw, view_first, view_last, lr, prev, next);
	notify_layer(lr, vw, XQX_VLC_FINISH);
}

void xqx_view_prepend_layer(struct xqx_view *vw, struct xqx_view_layer *lr)
{
	lr->view = vw;
	DLL_PREPEND(vw, view_first, view_last, lr, prev, next);
	update_first(vw, lr);
	notify_layer(lr, vw, XQX_VLC_INIT);
}

void xqx_view_append_layer(struct xqx_view *vw, struct xqx_view_layer *lr)
{
	lr->view = vw;
	DLL_APPEND(vw, view_first, view_last, lr, prev, next);
	update_first(vw, lr);
	notify_layer(lr, vw, XQX_VLC_INIT);
}

void xqx_view_set_center(struct xqx_view *vw, int nx, int ny)
{
	vw->center.x = nx;
	vw->center.y = ny;

	notify_layers(vw, XQX_VLC_MOVE);
	invalidate_view(vw);
}

void xqx_view_set_scale(struct xqx_view *vw, int s)
{
	struct xqx_map_layer *il = ((struct xqx_map_layer *)(vw->view_last));
	int smax = 1 << (il->map->num_levels - 1);
	if (s < 1) s = 1;
	if (s > smax) s = smax;

	if (s == vw->scale_main)
		return;

	vw->scale_main = s;
	int64_t def = vw->scale_main;
	def *= vw->scale_cx;
	def /= vw->scale_px;
	vw->scale_def = abs((int) def);

	update_step(vw);
	notify_layers(vw, XQX_VLC_SCALE);
	invalidate_view(vw);
}

/* FIXME this function should probably go away */
void xqx_view_choose_map(struct xqx_view *vw, int s)
{
	if ((s >= vw->maps_num) || (vw->maps[s] == NULL))
		return;

	struct xqx_view_layer *vl = vw->view_last;

	if (vl != NULL) {
		xqx_view_remove_layer(vw, vl);
		xqx_discard_map_layer((struct xqx_map_layer *) vl);
	}

	vw->active_map = vw->maps[s];
	struct xqx_map_layer *il = xqx_make_map_layer(vw->active_map);
	xqx_view_append_layer(vw, (struct xqx_view_layer *) il);
	invalidate_view(vw);
}

void xqx_view_enable_grid(struct xqx_view *vw)
{
	vw->grid = xqx_make_grid();
	xqx_view_prepend_layer(vw, (struct xqx_view_layer *) (vw->grid));
	invalidate_view(vw);
}

void xqx_view_disable_grid(struct xqx_view *vw)
{
	xqx_view_remove_layer(vw, (struct xqx_view_layer *) (vw->grid));
	xqx_discard_grid(vw->grid);
	vw->grid = NULL;
	invalidate_view(vw);
}

void xqx_view_toggle_grid(struct xqx_view *vw)
{
	if (vw->grid)
		xqx_view_disable_grid(vw);
	else
		xqx_view_enable_grid(vw);
}

void xqx_view_enable_gps(struct xqx_view *vw)
{
	vw->gps = xqx_make_gps_layer();
	xqx_view_prepend_layer(vw, (struct xqx_view_layer *) (vw->gps));
	invalidate_view(vw);
}

void xqx_view_disable_gps(struct xqx_view *vw)
{
	xqx_view_remove_layer(vw, (struct xqx_view_layer *) (vw->gps));
	xqx_discard_gps_layer(vw->gps);
	vw->gps = NULL;
	invalidate_view(vw);
}

void xqx_view_toggle_gps_lock(struct xqx_view *vw)
{
	if (!vw->gps)
		return;

	vw->gps->locked = !vw->gps->locked;

	if (vw->gps->locked)
		xqx_view_set_center(vw, vw->gps->px, vw->gps->py);
}

static inline void xqx_view_move_user(struct xqx_view *vw, int dx, int dy)
{
	if (vw->gps)
		vw->gps->locked = 0;

	xqx_view_move(vw, dx, dy);
}

void xqx_view_request_redraw(struct xqx_view *vw, int lx, int ly, int hx, int hy)
{
	lx = CLAMP(lx, 0, ((int) vw->w));
	ly = CLAMP(ly, 0, ((int) vw->h));
	hx = CLAMP(hx, lx, ((int) vw->w));
	hy = CLAMP(hy, ly, ((int) vw->h));

	gp_widget_pixmap_redraw(vw->pixmap, lx, ly, hx, hy);
	printf("INV %d %d %d %d\n", lx, ly, hx, hy);
}

static void view_resize(struct xqx_view *vw)
{
	int old_valid = vw->valid;
	gp_widget *pixmap = vw->pixmap;

	vw->valid = 1;
	vw->w = gp_widget_pixmap_w(pixmap);
	vw->h = gp_widget_pixmap_h(pixmap);

	update_step(vw);
	do_notify_layers(vw, old_valid ? XQX_VLC_RESIZE : XQX_VLC_INIT);
}

/* Widget pixmap handler to repaint a screen */

static void view_redraw(gp_widget_event *ev)
{
	struct xqx_view *vw = ev->self->priv;
	gp_pixmap *pixmap = ev->self->pixmap->pixmap;

	if (!vw->valid)
		return;

	struct xqx_rectangle area = {ev->bbox->x, ev->bbox->y,
	                             ev->bbox->x + ev->bbox->w,
	                             ev->bbox->y + ev->bbox->h};
	struct xqx_view_layer *lr;

	printf("REDRAW BBOX " GP_BBOX_FMT "\n", GP_BBOX_PARS(*ev->bbox));

	for (lr = vw->view_last; lr; lr = lr->prev)
		do_render_layer(lr, vw, pixmap, &area);
}

/* widget pixmap keypress handler */

static int view_input(gp_widget_event *ev)
{
	struct xqx_view *vw = ev->self->priv;

	switch (ev->input_ev->type) {
	case GP_EV_REL:
		switch (ev->input_ev->code) {
		case GP_EV_REL_WHEEL:
			if (ev->input_ev->val < 0)
				xqx_view_zoom_out(vw, (2 << 10));
			else
				xqx_view_zoom_in(vw, (2 << 10));
			return 1;
		break;
		}
	break;
	case GP_EV_KEY:
		if (!ev->input_ev->code)
			return 0;

		switch (ev->input_ev->val) {
		case GP_KEY_F1:
			xqx_view_choose_map(vw, 0);
		break;
		case GP_KEY_F2:
			xqx_view_choose_map(vw, 1);
		break;
		case GP_KEY_F3:
			xqx_view_choose_map(vw, 2);
		break;
		case GP_KEY_F4:
			xqx_view_choose_map(vw, 3);
		break;
		case GP_KEY_LEFT:
			xqx_view_move_user(vw, vw->step_x, 0);
		break;
		case GP_KEY_RIGHT:
			xqx_view_move_user(vw, -vw->step_x, 0);
		break;
		case GP_KEY_UP:
			xqx_view_move_user(vw, 0, vw->step_y);
		break;
		case GP_KEY_DOWN:
			xqx_view_move_user(vw, 0, -vw->step_y);
		break;
		case GP_KEY_KP_PLUS:
			xqx_view_zoom_in(vw, (2 << 10));
		break;
		case GP_KEY_KP_MINUS:
			xqx_view_zoom_out(vw, (2 << 10));
		break;
		case GP_KEY_G:
			xqx_view_toggle_grid(vw);
		break;
		case GP_KEY_L:
			xqx_view_toggle_gps_lock(vw);
		break;
		}
	break;
	}

	return 0;
}

static int view_pixmap_on_event(gp_widget_event *ev)
{
	static int resize = 0;

	switch (ev->type) {
	case GP_WIDGET_EVENT_INPUT:
		view_input(ev);
	break;
	case GP_WIDGET_EVENT_RESIZE:
		view_resize(ev->self->priv);
	break;
	case GP_WIDGET_EVENT_REDRAW:
		//HACK
		if (!resize) {
			view_resize(ev->self->priv);
			resize = 1;
		}
		view_redraw(ev);
		return 1;
	break;
	}

	return 0;
}

struct xqx_view *xqx_make_view(gp_widget *pixmap)
{
	struct xqx_view *vw = calloc(1, sizeof (struct xqx_view));

	if (!vw)
		return NULL;

	/* Prevent divison by zero */
	vw->scale_px = vw->scale_py = 1;
	vw->pixmap = pixmap;

	//view_resize(vw);

	gp_widget_on_event_set(pixmap, view_pixmap_on_event, vw);
	gp_widget_event_unmask(pixmap, GP_WIDGET_EVENT_REDRAW);
	gp_widget_event_unmask(pixmap, GP_WIDGET_EVENT_RESIZE);
	gp_widget_event_unmask(pixmap, GP_WIDGET_EVENT_INPUT);

	return vw;
}
