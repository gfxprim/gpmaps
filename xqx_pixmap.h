// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

/*
 * The xqx_pixmap is a abstraction for an image. The low level code for map
 * loaders nor the cache does not need to know how in-memory pixmap looks like
 * all it needs is a function that loads (png or jpeg) image from an in-memory
 * buffer and returns a pointer to be stored in the cache.
 */

#ifndef XQX_PIXMAP_H__
#define XQX_PIXMAP_H__

#include "xqx_common.h"

typedef struct gp_pixmap xqx_pixmap;

/*
 * Loads an png or jpeg encoded image from in-memory buffer.
 *
 * Returns pointer to a in-memory pixmap or NULL on failure.
 */
xqx_pixmap *xqx_pixmap_decode(struct xqx_map *map, void *buf, size_t bufsize);

/*
 * Frees an in-memory pixmap.
 */
void xqx_pixmap_free(xqx_pixmap *pixmap);

#endif /* XQX_PIXMAP_H__ */
