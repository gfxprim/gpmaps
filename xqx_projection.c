// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021-2022 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdio.h>
#include <proj.h>
#include <math.h>
#include "xqx_projection.h"

static PJ *cur_proj;
static unsigned int cur_epsg;

int xqx_wgs84_to_coords(unsigned int epsg, double lat, double lon, double alt,
                        int32_t *x, int32_t *y, int32_t *z)
{
	if (cur_epsg != epsg) {
		char buf[40];

		proj_context_use_proj4_init_rules(PJ_DEFAULT_CTX, 1);

		sprintf(buf, "+init=epsg:%d", epsg);

		if (cur_proj)
			proj_destroy(cur_proj);

		cur_proj = proj_create_crs_to_crs(PJ_DEFAULT_CTX,
		                                  "+proj=latlong +datum=WGS84",
						  buf,
						  NULL);
		if (!cur_proj)
			return 1;

		cur_epsg = epsg;
	}

	PJ_COORD c_in = {
		.lpzt = {
			.lam = lon,
			.phi = lat,
			.z = alt,
			.t = HUGE_VAL,
		},
	};

	PJ_COORD c_out = proj_trans(cur_proj, PJ_FWD, c_in);

	*x = c_out.xyz.x * 16;
	*y = c_out.xyz.y * 16;
	*z = c_out.xyz.z * 16;

	return 0;
}
