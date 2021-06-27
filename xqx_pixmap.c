// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#include <loaders/gp_loaders.h>

#include "xqx_pixmap.h"

xqx_pixmap *xqx_pixmap_decode(struct xqx_map *map, void *buf, size_t bufsize)
{
	gp_io *io;
	gp_pixmap *ret;

	(void) map;

	io = gp_io_mem(buf, bufsize, NULL);
	if (!io)
		return NULL;

	ret = gp_read_image(io, NULL);

	gp_io_close(io);

	return ret;
}

void xqx_pixmap_free(xqx_pixmap *pixmap)
{
	gp_pixmap_free(pixmap);
}
