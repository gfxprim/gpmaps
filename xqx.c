// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#include "xqx.h"
#include "xqx_map_cache.h"
#include "xqx_map_tmc.h"

void xqx_init(void)
{
	xqx_map_cache_init(32 * 1 << 20, 128 * 1 << 20, 1023);
	xqx_map_tmc_init();
	xqx_gps_connect();
}
