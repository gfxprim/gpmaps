// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#ifndef XQX_H__
#define XQX_H__

//TODO: abstract interface the GPS fix data?
#include <gps.h>

#include "xqx_map.h"
#include "xqx_view.h"
#include "xqx_gps.h"
#include "xqx_waypoints.h"

void xqx_init(void);

#endif /* XQX_H__ */
