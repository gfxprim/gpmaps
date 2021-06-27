// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdio.h>
#define ACCEPT_USE_OF_DEPRECATED_PROJ_API_H 1
#include <proj_api.h>
#include "xqx_projection.h"

static projPJ proj_wgs84;
static projPJ proj_epsg;
static unsigned int cur_epsg;

int xqx_wgs84_to_coords(unsigned int epsg, double lat, double lon, double alt,
                        int32_t *x, int32_t *y, int32_t *z)
{
	if (!proj_wgs84) {
		proj_wgs84 = pj_init_plus("+proj=latlong +datum=WGS84");

		if (!proj_wgs84)
			return 1;
	}

	if (cur_epsg != epsg) {
		char buf[40];

		sprintf(buf, "+init=epsg:%d", epsg);
		proj_epsg = pj_init_plus(buf);

		if (!proj_epsg)
			return 1;

		cur_epsg = epsg;
	}

	double dx = lon * DEG_TO_RAD;
	double dy = lat * DEG_TO_RAD;
	double dz = alt;

	pj_transform(proj_wgs84, proj_epsg, 1, 1, &dx, &dy, &dz);

	*x = dx * 16;
	*y = dy * 16;
	*z = dz * 16;

	return 0;
}
