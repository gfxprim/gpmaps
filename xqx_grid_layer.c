// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#include "xqx_grid_layer.h"

#define ORDER(a, b, tmp) do { if (b < a) { tmp = a ; a = b;  b = tmp; } } while (0)

#define VALS 3
static const int step_table[VALS] = {2, 5, 10};
static int step_exp = 10;

static void grid_notify(void *gr_i, struct xqx_view *vw, unsigned int change)
{
	struct xqx_grid *gr = gr_i;

	if ((change == XQX_VLC_INIT) || (change == XQX_VLC_SCALE)) {
		int64_t dx = gr->dist;
		dx *= vw->scale_cx;
		dx *= vw->scale_main;
		dx /= vw->scale_px;
		dx = abs((int) dx);

		int i;
		int step_base = 16;

		for (i = 16; i < dx; i *= step_exp)
			step_base = i;

		for (i = 0; (i < VALS) && (step_base * step_table[i] < dx); i++);

		gr->step = step_base * step_table[i];
	}
}

static void draw_coord(gp_pixmap *pixmap, int x, int y, int align, int coord)
{
	char buf[16];

	buf[15] = 0;

	snprintf(buf, 15, "%d", coord/16000);

	gp_text(pixmap, NULL, x+1, y+1, align, 0xffffff, 0, buf);
	gp_text(pixmap, NULL, x, y, align, 0, 0, buf);
}

static void dashed_vline(gp_pixmap *pixmap, gp_coord x, gp_coord y0, gp_coord y1, gp_pixel color)
{
	gp_size len = y1 - y0, i;

	for (i = 0; i <= len/10; i++) {
		if (i%2)
			gp_vline(pixmap, x, y0 + 10*(i-1), y0 + 10*i, color);
	}
}

static void dashed_hline(gp_pixmap *pixmap, gp_coord x0, gp_coord x1, gp_coord y, gp_pixel color)
{
	gp_size len = x1 - x0, i;

	for (i = 0; i <= len/10; i++) {
		if (i%2)
			gp_hline(pixmap, x0 + 10*(i-1), x0 + 10*i, y, color);
	}
}

static void grid_render(void *gr_i, struct xqx_view *vw, gp_pixmap *pixmap, struct xqx_rectangle *rect)
{
	struct xqx_grid *gr = gr_i;
	struct xqx_coordinate lc, hc;

	unsigned int i;

	xqx_view_pixels_to_coords(vw, rect->lx, rect->ly, &lc);
	xqx_view_pixels_to_coords(vw, rect->hx, rect->hy, &hc);

	lc.x /= gr->step;
	lc.y /= gr->step;
	hc.x /= gr->step;
	hc.y /= gr->step;

	ORDER(lc.x, hc.x, i);
	ORDER(lc.y, hc.y, i);

	lc.x--;
	lc.y--;
	hc.x++;
	hc.y++;

	gp_pixel color = gp_rgb_to_pixmap_pixel(0x22, 0x22, 0xaa, pixmap);

	for (i = lc.x; i <= hc.x; i++) {
		int64_t tmp = i;
		tmp *= gr->step;
		tmp -= vw->center.x;
		tmp *= vw->scale_px;
		tmp /= vw->scale_cx;
		tmp /= vw->scale_main;
		tmp += vw->w / 2;

		if (i % 5)
			dashed_vline(pixmap, tmp, rect->ly, rect->hy, color);
		else
			gp_vline(pixmap, tmp, rect->ly, rect->hy, color);
	}

	for (i = lc.y; i <= hc.y; i++) {
		int64_t tmp = i;
		tmp *= gr->step;
		tmp -= vw->center.y;
		tmp *= vw->scale_py;
		tmp /= vw->scale_cy;
		tmp /= vw->scale_main;
		tmp += vw->h / 2;

		if (i % 5)
			dashed_hline(pixmap, rect->lx, rect->hx, tmp, color);
		else
			gp_hline(pixmap, rect->lx, rect->hx, tmp, color);
	}

	for (i = lc.x; i <= hc.x; i++) {
		int coord = i * gr->step;
		if (coord % 16000 == 0) {
			int64_t tmp = coord;
			tmp -= vw->center.x;
			tmp *= vw->scale_px;
			tmp /= vw->scale_cx;
			tmp /= vw->scale_main;
			tmp += vw->w / 2;
			draw_coord(pixmap, (int) tmp, 1, GP_ALIGN_CENTER | GP_VALIGN_BELOW, coord);
		}
	}

	for (i = lc.y; i <= hc.y; i++) {
		int coord = i * gr->step;
		if (coord % 16000 == 0) {
			int64_t tmp = coord;
			tmp -= vw->center.y;
			tmp *= vw->scale_py;
			tmp /= vw->scale_cy;
			tmp /= vw->scale_main;
			tmp += vw->h / 2;

			draw_coord(pixmap, 1, (int) tmp, GP_ALIGN_RIGHT | GP_VALIGN_CENTER, coord);
		}
	}
}

struct xqx_grid *xqx_make_grid(void)
{
	struct xqx_grid *gr;

	gr = calloc(1, sizeof(struct xqx_grid));
	if (!gr)
		return NULL;

	gr->common.notify_cb = grid_notify;
	gr->common.render_cb = grid_render;
	gr->dist = 60;

	return gr;
}

