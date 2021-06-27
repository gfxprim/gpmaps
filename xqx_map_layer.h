// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

/*
 * This implements a map layer which is the layer that draws an actuall map.
 */

#ifndef XQX_MAP_LAYER_H__
#define XQX_MAP_LAYER_H__

#include <stdint.h>

#include "xqx_view.h"

struct xqx_map_layer
{
  struct xqx_view_layer common;

  struct xqx_map *map;
  struct xqx_map_cache_client *cc;

  int pix_off_x, pix_off_y;
  uint32_t level;

  uint32_t tx1, tx2, tx3, tx4, ty1, ty2, ty3, ty4;
  uint32_t t2x1, t2x2, t2y1, t2y2;
  uint32_t ax, ay, as;
};

struct xqx_map_layer *xqx_make_map_layer(struct xqx_map *map);

void xqx_discard_map_layer(struct xqx_map_layer *il);

#endif /* XQX_MAP_LAYER_H__ */
