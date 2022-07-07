// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <utils/gp_json.h>
#include "xqx_waypoints.h"

struct xqx_path *xqx_path_new(const char *name)
{
	struct xqx_path *path = malloc(sizeof(struct xqx_path));

	if (!path)
		return NULL;

	if (name)
		path->name = strdup(name);
	else
		path->name = NULL;

	list_init(&path->waypoints);
	path->waypoints_cnt = 0;

	return path;
}

struct xqx_waypoint *xqx_waypoint_new(double lat, double lon, double alt, const char *name)
{
	struct xqx_waypoint *waypoint = malloc(sizeof(struct xqx_waypoint));

	if (!waypoint)
		return NULL;

	waypoint->lat = lat;
	waypoint->lon = lon;
	waypoint->alt = alt;

	if (name)
		waypoint->name = strdup(name);
	else
		waypoint->name = NULL;

	return waypoint;
}

void xqx_path_waypoint_append(struct xqx_path *path, struct xqx_waypoint *waypoint)
{
	list_append(&path->waypoints, &waypoint->list);
	path->waypoints_cnt++;
}

void xqx_path_free(struct xqx_path *path)
{
	struct xqx_waypoint *waypoint;

	LIST_FOREACH_SAFE(&path->waypoints, i) {
		waypoint = LIST_ENTRY(i, struct xqx_waypoint, list);
		free(waypoint);
	}

	free(path->name);
	free(path);
}

void xqx_path_print(struct xqx_path *path)
{
	struct xqx_waypoint *waypoint;

	printf("Path '%s' points %u\n", path->name ? path->name : "(unnamed)", path->waypoints_cnt);

	LIST_FOREACH(&path->waypoints, i) {
		waypoint = LIST_ENTRY(i, struct xqx_waypoint, list);
		printf("\t[%2.15lf, %2.15lf, %5.2lf] '%s'\n",
		       waypoint->lat, waypoint->lon, waypoint->alt,
		       waypoint->name ? waypoint->name : "(unnamed)");
	}
}

static struct xqx_path *path_from_coordinates(gp_json_buf *json, gp_json_val *val)
{
	struct xqx_path *path;

	if (gp_json_next_type(json) != GP_JSON_ARR) {
		gp_json_err(json, "Expected array");
		return NULL;
	}

	path = xqx_path_new(NULL);

	GP_JSON_ARR_FOREACH(json, val) {
		struct xqx_waypoint *waypoint;
		double coords[2];
		int coord_cnt = 0;

		if (val->type != GP_JSON_ARR) {
			gp_json_err(json, "Expected array of coordinate arrays");
			goto err0;
		}

		GP_JSON_ARR_FOREACH(json, val) {
			if (val->type != GP_JSON_FLOAT &&
			    val->type != GP_JSON_INT) {
				goto err1;
			}

			if (coord_cnt > 2)
				goto err1;

			coords[coord_cnt++] = val->val_float;
		}

		if (coord_cnt != 2)
			goto err1;

		waypoint = xqx_waypoint_new(coords[0], coords[1], NAN, NULL);

		if (!waypoint)
			goto err0;

		xqx_path_waypoint_append(path, waypoint);
	}

	return path;
err1:
	gp_json_err(json, "Expected [lon, lat]");
err0:
	xqx_path_free(path);
	return NULL;
}

static struct xqx_path *path_from_geometry(gp_json_buf *json, gp_json_val *val)
{
	struct xqx_path *ret = NULL;

	if (gp_json_next_type(json) != GP_JSON_OBJ) {
		gp_json_err(json, "Wrong geometry type");
		return NULL;
	}

	GP_JSON_OBJ_FOREACH(json, val) {
		if (!strcmp(val->id, "coordinates")) {
			ret = path_from_coordinates(json, val);
		} else if (!strcmp(val->id, "type")) {
			if (val->type != GP_JSON_STR ||
			    strcmp(val->val_str, "LineString"))
				gp_json_warn(json, "Wrong type expected 'LineString'");
		} else {
			gp_json_warn(json, "Unknown key");
			//TODO: Skip
		}
	}

	return ret;
}

static struct xqx_path *path_from_geojson(struct gp_json_buf *json)
{
	char buf[64];
	gp_json_val val = {.buf = buf, .buf_size = sizeof(buf)};
	struct xqx_path *ret = NULL;
	char *name = NULL;

	GP_JSON_OBJ_FOREACH(json, &val) {
		if (!strcmp(val.id, "type")) {
			if (val.type != GP_JSON_STR) {
				gp_json_err(json, "Wrong type value type");
				return NULL;
			}
		} else if (!strcmp(val.id, "geometry")) {
			ret = path_from_geometry(json, &val);
		} else if (!strcmp(val.id, "properties")) {
			GP_JSON_OBJ_FOREACH(json, &val) {
				if (!strcmp(val.id, "name") &&
				    val.type == GP_JSON_STR) {
					name = strdup(val.val_str);
				}
			}
		} else {
			gp_json_warn(json, "Unknown key");
			//TODO: Skip
		}
	}

	if (ret)
		ret->name = name;
	else
		free(name);

	return ret;
}

struct xqx_path *xqx_path_geojson(const char *pathname)
{
	struct gp_json_buf *json;
	struct xqx_path *path = NULL;

	json = gp_json_load(pathname);
	if (!json)
		return NULL;

	if (gp_json_next_type(json) != GP_JSON_OBJ) {
		gp_json_warn(json, "Expected object!");
		return NULL;
	}

	path = path_from_geojson(json);

	if (gp_json_is_err(json))
		gp_json_err_print(json);
	else if (!gp_json_empty(json))
		gp_json_warn(json, "Garbage after JSON");

	gp_json_free(json);

	return path;
}
