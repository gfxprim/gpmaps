// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyrml Hrubis <metan@ucw.cz>
 */

#include "xqx_map_layer.h"

static uint32_t search_array(struct xqx_map_layer *ml, unsigned int level, int lx, unsigned int hx, unsigned int hy)
{
	while (ml->ay < hy) {
		while (ml->ax < hx) {
			if (xqx_map_cache_lookup(ml->cc, ml->map, level, ml->ax, ml->ay) == NULL)
				return 1;
			ml->ax++;
		}
		ml->ax = lx;
		ml->ay++;
	}

	return 0;
}

static uint32_t find_missing_tile(struct xqx_map_layer *ml)
{
	switch (ml->as) {
	case 0:
		ml->ax = ml->tx2;
		ml->ay = ml->ty2;
		ml->as = 1;
	/* fallthrough */
	case 1:
		if (search_array(ml, ml->level, ml->tx2, ml->tx3, ml->ty3))
			return 3;
		ml->ax = ml->tx1;
		ml->ay = ml->ty1;
		ml->as = 2;
	/* fallthrough */
	case 2:
		if (search_array(ml, ml->level, ml->tx1, ml->tx4, ml->ty2))
			return 2;
		ml->ax = ml->tx1;
		ml->ay = ml->ty3;
		ml->as = 3;
	/* fallthrough */
	case 3:
		if (search_array(ml, ml->level, ml->tx1, ml->tx4, ml->ty4))
			return 2;
		ml->ax = ml->tx1;
		ml->ay = ml->ty2;
		ml->as = 4;
	/* fallthrough */
	case 4:
		if (search_array(ml, ml->level, ml->tx1, ml->tx2, ml->ty3))
			return 2;
		ml->ax = ml->tx3;
		ml->ay = ml->ty2;
		ml->as = 5;
	/* fallthrough */
	case 5:
		if (search_array(ml, ml->level, ml->tx3, ml->tx4, ml->ty3))
			return 2;
		ml->ax = ml->t2x1;
		ml->ay = ml->t2y1;
		ml->as = 6;
		if (ml->level == 0)
			return 0;
	/* fallthrough */
	case 6:
		if (search_array(ml, ml->level - 1, ml->t2x1, ml->t2x2, ml->t2y2))
			return 1;
		ml->as = 7;
	/* fallthrough */
	case 7:
		return 0;
	}

	return 0;
}

/* notification from cache about new avamlable tiles */

static void map_layer_cc_notify(void *ml_i, struct xqx_map *map, unsigned int l, unsigned int x, unsigned int y, struct xqx_map_cache_node *tile)
{
	struct xqx_map_layer *ml = ml_i;
	// printf("CC notify L%u X%u Y%u\n", l, x, y);

	(void) map;
	(void) tile;

	if ((ml->level == l) && (ml->tx2 <= x) && (x < ml->tx3) && (ml->ty2 <= y) && (y < ml->ty3)) {
		int sx = (x - ml->tx2) * ml->map->tile_w + ml->pix_off_x;
		int sy = (y - ml->ty2) * ml->map->tile_h + ml->pix_off_y;
		xqx_view_request_redraw(ml->common.view, sx, sy, sx + ml->map->tile_w, sy + ml->map->tile_h);
	}
}


/* query from cache about requested tiles */

static uint32_t map_layer_cc_query(void *ml_i, struct xqx_map **map, uint32_t *l, uint32_t *x, uint32_t *y)
{
	struct xqx_map_layer *ml = ml_i;
	uint32_t mt = find_missing_tile(ml);

	if (mt > 0) {
		// printf("AF0 QUERY %d L%d X%d Y%d\n", mt, ml->level, ml->ax, ml->ay);
		*map = ml->map;
		*x = ml->ax;
		*y = ml->ay;
		*l = (mt == 1) ? (ml->level - 1) : ml->level;
	}

	return mt;
}

/* eval tiles from cache for removing */

static uint32_t map_layer_cc_eval(void *ml_i, struct xqx_map_cache_node *cn)
{
	struct xqx_map_layer *ml = ml_i;

	if ((cn->l == ml->level)
	    && (ml->tx2 <= cn->x) && (cn->x < ml->tx3)
	    && (ml->ty2 <= cn->y) && (cn->y < ml->ty3))
		return 3;

	if ((cn->l == ml->level)
	    && (ml->tx1 <= cn->x) && (cn->x < ml->tx4)
	    && (ml->ty1 <= cn->y) && (cn->y < ml->ty4))
		return 2;

	/*
	if ((cn->l + 1) == ml->level)
		{
			printf("X %d %d %d\n", (2 * ml->tx2), cn->x, (2 * ml->tx3));
			printf("Y %d %d %d\n", (2 * ml->ty2), cn->y, (2 * ml->ty3));
			if ((1)
			&& ((2 * ml->tx2) <= cn->x) && (cn->x < (2 * ml->tx3))
			&& ((2 * ml->ty2) <= cn->y) && (cn->y < (2 * ml->ty3)))
		return (1);
		}
	*/

	return 0;
}

static uint32_t get_nearest_level(struct xqx_map *map, int scale_main)
{
	int ns = 1;
	int l = 0;
	while ((ns <= scale_main) && (l < map->num_levels)) {
		ns *= 2;
		l++;
	}

	return ((l > 0) ? (l - 1) : l);
}


/* notification from view about view geometry change */

static void map_layer_notify(void *ml_i, struct xqx_view *vw, uint32_t change)
{
	struct xqx_map_layer *ml = ml_i;
	struct xqx_coordinate *c = &(vw->center);

	if (change == XQX_VLC_FINISH)
		return;

	if ((change == XQX_VLC_INIT) || (change == XQX_VLC_SCALE)) {
		ml->level = get_nearest_level(ml->map, vw->scale_main);
		xqx_map_cache_request_notification(ml->cc, ml->map, ml->level);
	}

	// FIXME remove next check
	if (vw->scale_main != (1 << ml->level))
		printf("WARNING: Inconsistent scale and level\n");

	int tw = ml->map->tile_w;
	int th = ml->map->tile_h;
	int txc = ml->map->num_tiles_x[ml->level];
	int tyc = ml->map->num_tiles_y[ml->level];

	/* coordinates in current pixel coordinate space (CPCS)
		 are coordinates in pixels of current zoom,
		 relative to the start of a image */

	/* c. are coordinates of center of view in CPCS */
	/* FIXME analyze needed precision */

	int64_t tmpx = c->x;
	tmpx -= ml->map->geo_cox;
	tmpx *= ml->map->geo_psx;
	tmpx /= ml->map->geo_csx;
	tmpx += ml->map->geo_pox;
	int cx = tmpx / (1 << ml->level);

	int64_t tmpy = c->y;
	tmpy -= ml->map->geo_coy;
	tmpy *= ml->map->geo_psy;
	tmpy /= ml->map->geo_csy;
	tmpy += ml->map->geo_poy;
	int cy = tmpy / (1 << ml->level);

	/* l. and h. are coordinates of ul,lr corners of view in CPCS */
	int lx = cx - (vw->w / 2);
	int ly = cy - (vw->h / 2);
	int hx = lx + vw->w;
	int hy = ly + vw->h;

	/* tl. and th. are indices of 'interesting' tiles - tiles
		 containing corners of view */
	int tlx = lx / tw;
	int tly = ly / th;
	int thx = (hx - 1 + tw) / tw;
	int thy = (hy - 1 + th) / th;

	/* we clamp indices to interval avamlable tiles */
	tlx = CLAMP(tlx, 0, txc);
	tly = CLAMP(tly, 0, tyc);
	thx = CLAMP(thx, 0, txc);
	thy = CLAMP(thy, 0, tyc);

	/* ml->pix_off_. are offsets of a start of 'interesting' tile rectangle
		 in pixels of current zoom, relative to the start of a view */
	ml->pix_off_x = tlx * tw - lx;
	ml->pix_off_y = tly * th - ly;

	/* save 'interesting' tiles coords */
	ml->tx2 = tlx;
	ml->tx3 = thx;
	ml->ty2 = tly;
	ml->ty3 = thy;

	int dx = (thx - tlx + 1) / 2;
	int dy = (thy - tly + 1) / 2;

	/* extended interesting tiles */
	ml->tx1 = MAX(0, tlx - dx);
	ml->ty1 = MAX(0, tly - dy);
	ml->tx4 = MIN(thx + dx, txc);
	ml->ty4 = MIN(thy + dy, tyc);

	/* rectangle for prefetch of lower level */
	if (ml->level > 0) {
		int t2xc = ml->map->num_tiles_x[ml->level - 1];
		int t2yc = ml->map->num_tiles_y[ml->level - 1];
		ml->t2x1 = MIN(t2xc, (int) ml->tx2 * 2);
		ml->t2y1 = MIN(t2yc, (int) ml->ty2 * 2);
		ml->t2x2 = MIN(t2xc, (int) ml->tx3 * 2);
		ml->t2y2 = MIN(t2yc, (int) ml->ty3 * 2);
	}
	//	printf("CONF X: %d %d %d %d\n", ml->tx1, ml->tx2, ml->tx3, ml->tx4);
	//	printf("CONF Y: %d %d %d %d\n", ml->ty1, ml->ty2, ml->ty3, ml->ty4);
	//	printf("OFF: %d %d\n", ml->pix_off_x, ml->pix_off_y);

	ml->as = 0;
	int mt = find_missing_tile(ml);

	xqx_map_cache_request_attention(ml->cc, mt);
}

static void map_layer_render(void *ml_i, struct xqx_view *vw, gp_pixmap *dst, struct xqx_rectangle *rect)
{
	struct xqx_map_layer *ml = ml_i;

	(void) vw;

	int tw = ml->map->tile_w;
	int th = ml->map->tile_h;

	/* - first we translate absolute (in window) pixel coordinates to
				 relative (to first tile in window begin) coordinates
		 - then we scale from pixels to tiles
		 - then we translate from relative (to first tile in window) tiles to
				 absolute tiles
		 - we should draw tiles satisfying lx <= x < hx, ly <= y < hy
	*/

	int lx = (rect->lx - ml->pix_off_x) / tw + ml->tx2;
	int ly = (rect->ly - ml->pix_off_y) / th + ml->ty2;
	int hx = (rect->hx - ml->pix_off_x - 1 + tw) / tw + ml->tx2;
	int hy = (rect->hy - ml->pix_off_y - 1 + th) / th + ml->ty2;

	/* now crop tile coords to visible tiles range (tiles between t_2 and t_3) */

	lx = (lx < (int)ml->tx2) ? (int)ml->tx2 : lx;
	ly = (ly < (int)ml->ty2) ? (int)ml->ty2 : ly;
	hx = (hx > (int)ml->tx3) ? (int)ml->tx3 : hx;
	hy = (hy < (int)ml->ty3) ? (int)ml->ty3 : hy;

	// printf("RENDER (%d %d) - (%d %d)\n", lx, ly, hx, hy);
	int i, j;
	for (i = lx; i < hx; i++) {
		for (j = ly; j < hy; j++) {
			int ax = (i - ml->tx2) * tw + ml->pix_off_x;
			int ay = (j - ml->ty2) * th + ml->pix_off_y;
			int aw = tw;
			int ah = th;

			/* fixup for a width/height of tile at last column/row */
			if ((i + 1) == ml->map->num_tiles_x[ml->level])
				aw = (ml->map->map_w >> ml->level) - i * tw;

			if ((j + 1) == ml->map->num_tiles_y[ml->level])
				ah = (ml->map->map_h >> ml->level) - j * th;

			struct xqx_map_cache_node *cn = xqx_map_cache_lookup(ml->cc, ml->map, ml->level, i, j);

			if (cn == NULL) {
				//printf("NODATA (%d %d) at (%d %d)\n", i, j, ax, ay);
				/* no data */
				/* FIXME add scaling of larger */
			} else if (cn->state == XQX_CACHE_NODE_VALID_DATA) {
				//printf("DRAW (%d %d) at (%d %d)\n", i, j, ax, ay);
				gp_pixmap *pb = cn->data;
				gp_blit_xywh_clipped(pb, 0, 0, aw, ah, dst, ax, ay);
			} else if (cn->state == XQX_CACHE_NODE_VALID_COLOR) {
				//printf("COLOR (%d %d) at (%d %d)\n", i, j, ax, ay);
				uint32_t rgb = (uintptr_t) cn->data;

				gp_pixel color = gp_rgb_to_pixmap_pixel(rgb&0x00ff0000>>16,
				                                        rgb&0x0000ff00>>8,
				                                        rgb&0x000000ff, dst);

				gp_fill_rect_xywh(dst, ax, ay, aw, ah, color);
			}
		}
	}
}

static struct xqx_map_cache_client_ops map_layer_ops = {
	map_layer_cc_notify, map_layer_cc_query, map_layer_cc_eval
};

struct xqx_map_layer *xqx_make_map_layer(struct xqx_map *map)
{
	struct xqx_map_layer *ml = calloc(1, sizeof(struct xqx_map_layer));

	if (!ml)
		return NULL;

	ml->common.notify_cb = map_layer_notify;
	ml->common.render_cb = map_layer_render;
	ml->map = map;
	ml->cc = xqx_map_cache_make_client(&map_layer_ops, ml);

	if (!ml->cc) {
		free(ml);
		return NULL;
	}

	return ml;
}

/* image_layer should be removed from view before discarding */
void xqx_discard_map_layer(struct xqx_map_layer *ml)
{
	xqx_map_cache_discard_client(ml->cc);
	free(ml);
}
