// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

/*

   A layer that draws a grid.

 */

#ifndef XQX_GRID_H__
#define XQX_GRID_H__

#include "xqx_view.h"

struct xqx_grid
{
	struct xqx_view_layer common;
	int dist, step;
};

struct xqx_grid *xqx_make_grid(void);

static inline void xqx_discard_grid(struct xqx_grid * gr)
{
	free(gr);
}

#endif /* XQX_GRID_H__*/
