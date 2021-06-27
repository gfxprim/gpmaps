// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

/*
 * The xqx_map is an abstract interface for a map loaders.
 *
 * Map loader implements a low level code to read a map file(s) and a callback
 * to load tile(s) which are put into the map cache and rendered in the view.
 */

#ifndef XQX_MAP_H__
#define XQX_MAP_H__

#include <stdint.h>
#include "xqx_map_cache.h"

#define MAX_PROJ4_LEN 1024

struct xqx_map_ops
{
	const char *suffix;
	int suffix_len;

	struct xqx_map* (*map_load_cb)(const char *pathname);

	void (*read_tile_cb)(struct xqx_map *, uint32_t, uint32_t, uint32_t);

	struct xqx_map_ops *next;
};

struct xqx_map
{
	struct xqx_map_ops *ops;
	int map_w, map_h, tile_w, tile_h;
	int num_levels;
	int *num_tiles_x, *num_tiles_y;

	/* georeference informations */
	int geo_pox, geo_poy, geo_psx, geo_psy;
	int geo_cox, geo_coy, geo_csx, geo_csy;

	/* epsg projection id */
	unsigned int epsg;

	struct xqx_map_cache_map cache;
};

static inline void xqx_map_read_tile(struct xqx_map *map, uint32_t level, uint32_t x, uint32_t y)
{
	map->ops->read_tile_cb(map, level, x, y);
}

/*
 * Sets map projection.
 */
void xqx_map_set_projection(struct xqx_map *map, unsigned int epsg);

/*
 * Loads a map from a file.
 */
struct xqx_map *xqx_map_load(const char *pathname);

/*
 * Registers a new map loader.
 */
void xqx_register_map_ops(struct xqx_map_ops *ops);

#endif /* XQX_MAP_H__ */
