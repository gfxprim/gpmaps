// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#ifndef XQX_WAYPOINTS_H__
#define XQX_WAYPOINTS_H__

#include "xqx_list.h"

struct xqx_waypoint {
	/*
	 * Coordinates in WGS84
	 *
	 * alt may not be defined and set to nan
	 */
	double lat, lon, alt;

	/* Optional name may be NULL */
	char *name;

	struct list_head list;
};

struct xqx_path {
	/* Optional name may be NULL */
	char *name;

	unsigned int waypoints_cnt;
	struct list_head waypoints;
};

struct xqx_path *xqx_path_new(const char *name);

void xqx_path_free(struct xqx_path *path);

struct xqx_waypoint *xqx_waypoint_new(double lat, double lon, double alt, const char *name);

void xqx_path_waypoint_append(struct xqx_path *path, struct xqx_waypoint *waypoint);

struct xqx_path *xqx_path_geojson(const char *pathname);

void xqx_path_print(struct xqx_path *path);

#endif /* XQX_WAYPOINTS_H__ */
