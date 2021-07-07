// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

/*

   Map cache is where map tiles are stored to. Map tiles are requested by a
   cache client e.g. view and loaded asynchronously. A cache client is then
   notified when tiles are available and tiles are rendered on a screen.

 */

#ifndef XQX_CACHE_H__
#define XQX_CACHE_H__

#define MAX_PRIO 3
#define MIN_PRIO 1

#include "xqx_common.h"
#include "xqx_pixmap.h"

enum xqx_map_cache_node_state {
	XQX_CACHE_NODE_ERROR,
	XQX_CACHE_NODE_VALID_DATA,
	XQX_CACHE_NODE_VALID_COLOR
};

struct xqx_map_cache
{
	size_t low_size, high_size;
	uint32_t hash_size;

	struct xqx_map *map_first, *map_last;
	struct xqx_map_cache_client *query_first[MAX_PRIO+1];
	struct xqx_map_cache_client *query_last[MAX_PRIO+1];
};

struct xqx_map_cache_level
{
	struct xqx_map_cache_client *notify_first, *notify_last;
};

struct xqx_map_cache_map
{
	size_t act_size, node_size;
	struct xqx_map_cache_node **hash_table;
	uint32_t hash_size;
	struct xqx_map_cache_level *levels;

	struct xqx_map *next, *prev; /* in cache */
	struct xqx_map_cache_node *node_first, *node_last;
};

struct xqx_map_cache_node
{
	uint32_t state;
	void *data;
	uint32_t l, x, y;
	struct xqx_map_cache_node *next, *prev; /* per image */
	struct xqx_map_cache_node *hash_next;	 /* in hash list */
};

struct xqx_map_cache_client_ops
{
	void (*notify)(void *, struct xqx_map *, uint32_t, uint32_t, uint32_t, struct xqx_map_cache_node *);
	uint32_t (*query)(void *, struct xqx_map **, uint32_t*, uint32_t*, uint32_t*);
	uint32_t (*eval)(void *, struct xqx_map_cache_node *);
};

struct xqx_map_cache_client
{
	struct xqx_map_cache_client_ops *ops;
	void *data;

	struct xqx_map_cache_level *monitored;
	struct xqx_map_cache_client *notify_next, *notify_prev;

	unsigned int prio;
	struct xqx_map_cache_client *query_next, *query_prev;
};

void xqx_map_cache_init(size_t low_size, size_t high_size, uint32_t hash_size);

struct xqx_map_cache_node *xqx_map_cache_node_make(struct xqx_map *map, uint32_t l, uint32_t x, uint32_t y,
                                           enum xqx_map_cache_node_state state, void *data);

static inline struct xqx_map_cache_node *
xqx_map_cache_make_error_node(struct xqx_map *map, uint32_t l, uint32_t x, uint32_t y)
{
	return xqx_map_cache_node_make(map, l, x, y, XQX_CACHE_NODE_ERROR, NULL);
}

static inline struct xqx_map_cache_node *
xqx_map_cache_make_data_node(struct xqx_map *map, uint32_t l, uint32_t x, uint32_t y, xqx_pixmap *pb)
{
	return xqx_map_cache_node_make(map, l, x, y, XQX_CACHE_NODE_VALID_DATA, pb);
}

static inline struct xqx_map_cache_node *
xqx_map_cache_make_color_node(struct xqx_map *map, uint32_t l, uint32_t x, uint32_t y, uint32_t color)
{
	return xqx_map_cache_node_make(map, l, x, y, XQX_CACHE_NODE_VALID_COLOR, (void *) (uintptr_t) color);
}

struct xqx_map_cache_node *xqx_map_cache_lookup(struct xqx_map_cache_client *client, struct xqx_map *map,
                                                uint32_t level, uint32_t x, uint32_t y);

void xqx_map_cache_init_map(struct xqx_map *map);

struct xqx_map_cache_client *xqx_map_cache_make_client(struct xqx_map_cache_client_ops *ops, void *data);

void xqx_map_cache_discard_client(struct xqx_map_cache_client *client);

void xqx_map_cache_request_notification(struct xqx_map_cache_client *client, struct xqx_map *map, uint32_t level);

void xqx_map_cache_request_attention(struct xqx_map_cache_client *client, unsigned int prio);

#endif /* XQX_CACHE_H__ */
