// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#ifndef XQX_MAP_TMC_H__
#define XQX_MAP_TMC_H__

struct pia_file;

struct xqx_tmc_level
{
	struct pia_file *pia;
	const char *format_string;
	uint32_t empty_color;
};

struct xqx_map_tmc
{
	struct xqx_map common;
	struct xqx_tmc_level *levels;

	unsigned int namebuf_size;
};

void xqx_map_tmc_init(void);

#endif /* XQX_MAP_TMC_H__ */
