// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdlib.h>
#include <stdio.h>

#include <widgets/gp_widgets_task.h>

#include "xqx_map.h"
#include "xqx_map_cache.h"
#include "xqx_dllist.h"

static struct xqx_map_cache *cache;

static inline uint32_t compute_hash(struct xqx_map_cache_map *map, uint32_t l, uint32_t x, uint32_t y)
{
	return ((l * 3 + x * 7 + y * 13) % map->hash_size);
}

static inline struct xqx_map_cache_node **index_hash(struct xqx_map_cache_map *map, uint32_t l, uint32_t x, uint32_t y)
{
	return (map->hash_table + compute_hash(map, l, x, y));
}

static inline void notify_cache_client(struct xqx_map_cache_client *client, struct xqx_map *map, uint32_t l, uint32_t x, uint32_t y, struct xqx_map_cache_node *cn)
{
	client->ops->notify(client->data, map, l, x, y, cn);
}

static inline uint32_t query_cache_client(struct xqx_map_cache_client *client, struct xqx_map **map, uint32_t *l, uint32_t *x, uint32_t *y)
{
	return client->ops->query(client->data, map, l, x, y);
}

static inline uint32_t eval_in_cache_client(struct xqx_map_cache_client *client, struct xqx_map_cache_node *cn)
{
	return client->ops->eval(client->data, cn);
}

void xqx_map_cache_init(size_t low_size, size_t high_size, uint32_t hash_size)
{
	cache = calloc(1, sizeof(struct xqx_map_cache));
	cache->low_size = low_size;
	cache->high_size = high_size;
	cache->hash_size = hash_size;
}

static void register_cleanup(void);

struct xqx_map_cache_node *xqx_map_cache_node_make(struct xqx_map *map, uint32_t l, uint32_t x, uint32_t y, enum xqx_map_cache_node_state state, void *data)
{
	struct xqx_map_cache_map *ci = &(map->cache);
	struct xqx_map_cache_node *cn = calloc(1, sizeof(struct xqx_map_cache_node));
	struct xqx_map_cache_node **pos;

	cn->state = state;
	cn->data = data;
	cn->l = l;
	cn->x = x;
	cn->y = y;

	DLL_APPEND(ci, node_first, node_last, cn, prev, next);
	pos = index_hash(ci, l, x, y);
	cn->hash_next = *pos;
	*pos = cn;

	if (state == XQX_CACHE_NODE_VALID_DATA)
		map->cache.act_size += map->cache.node_size;

	if (map->cache.act_size > cache->high_size)
		register_cleanup();

	// printf("ADD L%u X%u Y%u S%u\n", l, x, y, state);
	struct xqx_map_cache_client *cc;
	for (cc = ci->levels[l].notify_first; cc != NULL; cc = cc->notify_next)
		notify_cache_client(cc, map, l, x, y, cn);

	return cn;
}

static void destroy_cache_node(struct xqx_map *map, struct xqx_map_cache_node *cn)
{
	DLL_REMOVE(map, cache.node_first, cache.node_last, cn, prev, next);

	struct xqx_map_cache_node **pos;

	for (pos = index_hash(&(map->cache), cn->l, cn->x, cn->y); *pos && (*pos != cn); pos = &((*pos)->hash_next));

//	ASSERT (*pos != NULL);
	*pos = cn->hash_next;

	/* ugly hack */
	if (cn->state == XQX_CACHE_NODE_VALID_DATA) {
		map->cache.act_size -= map->cache.node_size;
		xqx_pixmap_free(cn->data);
	}

	free(cn);
}

struct xqx_map_cache_node *xqx_map_cache_lookup(struct xqx_map_cache_client *client, struct xqx_map *map,
                                                uint32_t level, uint32_t x, uint32_t y)
{
	struct xqx_map_cache_node *n;

	(void) client;

	for (n = *index_hash(&(map->cache), level, x, y); n; n = n->hash_next) {
		if ((n->l == level) && (n->x == x) && (n->y == y)) {
			// printf("L OK\n");
			/* FIXME update LRU */
			return n;
		}
	}

	return NULL;
}

void xqx_map_cache_init_map(struct xqx_map *map)
{
	map->cache.act_size = 0;
	map->cache.node_size = map->tile_w * map->tile_h * 4;
	map->cache.hash_size = cache->hash_size;
	map->cache.hash_table = calloc(map->cache.hash_size, sizeof(struct xqx_map_cache_node *));
	map->cache.levels = calloc(map->num_levels, sizeof(struct xqx_map_cache_level));

	DLL_APPEND(cache, map_first, map_last, map, cache.prev, cache.next);
}

/* client calls */

static inline void unlink_cache_client(struct xqx_map_cache_client *cc)
{
	DLL_REMOVE(cache, query_first[cc->prio], query_last[cc->prio], cc, query_prev, query_next);
}

static inline void link_cache_client(struct xqx_map_cache_client *cc, int i)
{
	DLL_APPEND(cache, query_first[i], query_last[i], cc, query_prev, query_next);
}

struct xqx_map_cache_client *xqx_map_cache_make_client(struct xqx_map_cache_client_ops *ops, void *data)
{
	struct xqx_map_cache_client *cc = calloc(1, sizeof(struct xqx_map_cache_client));

	if (!cc)
		return NULL;

	cc->ops = ops;
	cc->data = data;

	link_cache_client(cc, 0);

	return(cc);
}

static void cleanup_notification(struct xqx_map_cache_client *cc)
{
	if (cc->monitored) {
		DLL_REMOVE(cc->monitored, notify_first, notify_last, cc, notify_prev, notify_next);
		cc->monitored = NULL;
	}
}

void xqx_map_cache_discard_client(struct xqx_map_cache_client *cc)
{
	cleanup_notification(cc);
	unlink_cache_client(cc);
	free(cc);
}

void xqx_map_cache_request_notification(struct xqx_map_cache_client *cc, struct xqx_map *map, uint32_t level)
{
	cleanup_notification(cc);
	cc->monitored = &(map->cache.levels[level]);
	DLL_APPEND(cc->monitored, notify_first, notify_last, cc, notify_prev, notify_next);
}

static void update_client_attention(struct xqx_map_cache_client *cc, unsigned int prio)
{
	if (cc->prio != prio) {
		unlink_cache_client(cc);
		link_cache_client(cc, prio);
		cc->prio = prio;
	}
}

static void register_event_source(void);

void xqx_map_cache_request_attention(struct xqx_map_cache_client *cc, uint32_t prio)
{
	update_client_attention(cc, prio);
	register_event_source();
}

static int query_cache_clients(uint32_t least_prio, struct xqx_map **map, uint32_t *l, uint32_t *x, uint32_t *y)
{
	struct xqx_map_cache_client *cc;
	uint32_t i;

	for (i = MAX_PRIO; i >= least_prio; i--) {
		while (cache->query_first[i]) {
			cc = cache->query_first[i];
			uint32_t np = query_cache_client(cc, map, l, x, y);

			if (np == i)
				return i;

			update_client_attention(cc, np);
			if (np > i)
				printf("WARNING: priority raise during query\n");
		}
	}

	return 0;
}


static int cache_iteration(uint32_t least_prio)
{
	struct xqx_map *map;
	uint32_t l, x, y;

	int rv = query_cache_clients(least_prio, &map, &l, &x, &y);
	if (rv)
		xqx_map_read_tile(map, l, x, y);

	return (rv);
}

static void local_cache_cleanup(struct xqx_map *map)
{
	struct xqx_map_cache_node *cn, *acn;
	struct xqx_map_cache_client *cc;

	uint32_t prio, node_prio, tmp;

	int tbl[4] = {0, 0, 0, 0};
	int cln = 0;

	for (prio = 0; prio < MAX_PRIO; prio++) {
		if (prio == 1)
			printf("TBL %d %d %d %d\n", tbl[0], tbl[1], tbl[2], tbl[3]);
		cn = map->cache.node_first;

		while (cn) {
			if(map->cache.act_size <= cache->low_size) {
				printf("Cleanup finished successfully during phase %d (%d cleaned)\n", prio, cln);
				return;
			}

			acn = cn;
			cn = cn->next;
			node_prio = 0;

			for (cc = map->cache.levels[acn->l].notify_first; cc; cc = cc->notify_next) {
				tmp = eval_in_cache_client(cc, acn);
				node_prio = MAX(node_prio, tmp);
			}

			if (prio == 0)
				tbl[node_prio]++;

			if (node_prio <= prio) {
				destroy_cache_node(map, acn);
				cln++;
			}
		}
	}

	printf("Cleanup finished unsuccessfully (%d cleaned)\n", cln);
	printf("TBL %d %d %d %d\n", tbl[0], tbl[1], tbl[2], tbl[3]);
}

static void cache_cleanup(void)
{
	struct xqx_map *map;
	for (map = cache->map_first; map != NULL; map=map->cache.next)
		local_cache_cleanup(map);
}

static int cache_high_iteration(gp_task *self)
{
	(void) self;

	return cache_iteration(MAX_PRIO);
}

static int cache_low_iteration(gp_task *self)
{
	(void) self;

	return cache_iteration(MIN_PRIO);
}

static inline int top_priority(void)
{
	int i;
	for (i = MAX_PRIO; i > 0; i--) {
		if (cache->query_first[i] != NULL)
			return i;
	}

	return 0;
}

static void register_event_source(void)
{
	int prio = top_priority();

	static gp_task high_prio = {
		.id = "xqx cache high prio",
		.callback = cache_high_iteration,
		.prio = 1,
	};

	static gp_task low_prio = {
		.id = "xqx cache low prio",
		.callback = cache_low_iteration,
		.prio = 2,
	};

	if (prio == MAX_PRIO) {
		printf("CACHE MAX!\n");
		gp_widgets_task_ins(&high_prio);
	}

	if (prio > 0) {
		printf("CACHE LOW!\n");
		gp_widgets_task_ins(&low_prio);
	}
}

static int cache_cleanup_iteration(gp_task *self)
{
	(void) self;

	cache_cleanup();
	return 0;
}

static void register_cleanup(void)
{
	static gp_task cleanup = {
		.id = "xqx cache cleanup",
		.callback = cache_cleanup_iteration,
		.prio = 3,
	};

	gp_widgets_task_ins(&cleanup);
}
