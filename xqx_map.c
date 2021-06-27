// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdlib.h>
#include <string.h>
#include "xqx_map.h"

static struct xqx_map_ops *maps_ops;

struct xqx_map *xqx_map_load(const char *filename)
{
	struct xqx_map_ops *ops;
	int nl = strlen(filename);

	for (ops = maps_ops; ops; ops = ops->next) {
		if ((nl > ops->suffix_len) && !strcmp(filename + (nl - ops->suffix_len), ops->suffix))
			return (ops->map_load_cb(filename));
	}

	return NULL;
}

void xqx_map_set_projection(struct xqx_map *map, unsigned int epsg)
{
	map->epsg = epsg;
}

void xqx_register_map_ops(struct xqx_map_ops *ops)
{
	ops->next = ops;
	maps_ops = ops;
}
