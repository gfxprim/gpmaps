// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#ifndef XQX_PROJECTION_H__
#define XQX_PROJECTION_H__

#include <stdint.h>

/*
 * @epsg: A projection id
 * @lat, lon, alt: WGS84 coordinates
 * @x, y, z: Coordinages in given projection in meters in 28.4 fixed point
 */
int xqx_wgs84_to_coords(unsigned int epsg, double lat, double lon, double alt,
                        int32_t *x, int32_t *y, int32_t *z);

#endif /* XQX_PROJECTION_H__ */
