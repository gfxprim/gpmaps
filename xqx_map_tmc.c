// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>

#include "libpia/libpia.h"
#include "xqx_map.h"
#include "xqx_pixmap.h"
#include "xqx_map_tmc.h"

static uint32_t
format_filename(struct xqx_map_tmc *map, char *namebuf, uint32_t l, uint32_t x, uint32_t y)
{
	int written = snprintf(namebuf, map->namebuf_size, map->levels[l].format_string, l, x, y);

	return (written > 0) && ((unsigned int)written < map->namebuf_size);
}

ssize_t dir_read_whole_item(struct xqx_map_tmc *map, uint32_t l, uint32_t x, uint32_t y, void **buf)
{
	char namebuf[map->namebuf_size];
	struct stat stat;
	int rv;
	int fd;

	rv = format_filename(map, namebuf, l, x, y);
	if (rv < 0)
		return -1;

	rv = access(namebuf, F_OK);
	if (rv < 0)
		return 0;

	fd = open(namebuf, O_RDONLY);
	if (fd < 0)
		return -1;

	rv = fstat(fd, &stat);
	if (rv < 0)
		return -1;

	if (!S_ISREG(stat.st_mode))
		return -1;

	*buf = malloc(stat.st_size);
	if (!*buf)
		return -1;

	rv = read(fd, *buf, stat.st_size);
	if (rv < stat.st_size) {
		free(*buf);
		return -1;
	}

	close(fd);
	return rv;
}

static void read_tmc_tile(struct xqx_map *map, uint32_t l, uint32_t x, uint32_t y)
{
	struct xqx_map_tmc *tmc_map = (void*)map;;
	void *buf = NULL;
	ssize_t bufsize;

	if (tmc_map->levels[l].pia)
		bufsize = pia_read_whole_item(tmc_map->levels[l].pia, x, y, &buf);
	else
		bufsize = dir_read_whole_item(tmc_map, l, x, y, &buf);

	if (bufsize < 0)
		xqx_map_cache_make_error_node(map, l, x, y);
	else if (bufsize == 0)
		xqx_map_cache_make_color_node(map, l, x, y, tmc_map->levels[l].empty_color);
	else {
		xqx_pixmap *pb = xqx_pixmap_decode(map, buf, bufsize);
		if (!pb)
			xqx_map_cache_make_error_node(map, l, x, y);
		else
			xqx_map_cache_make_data_node(map, l, x, y, pb);
	}

	free(buf);
}

/* TMC description parser */

static void advance(const char **buf)
{
	while (((**buf) < 33) && ((**buf) != 0))
		(*buf)++;

	if ((**buf) == 0)
		(*buf) = NULL;
}

static int match_fixed_str(const char **buf, const char *str)
{
	if (!*buf)
		return 0;

	int l = strlen(str);

	if (strncmp(*buf, str, l))
		return 0;

	(*buf) += l;

	advance(buf);

	return 1;
}

static int match_str(const char **buf, const char **val)
{
	if (!*buf)
		return 0;

	const char *tmp = *buf;

	while (*tmp > 32)
		tmp++;

	if (tmp == *buf)
		return 0;

	uint32_t len = tmp - *buf;
	char *tmp2 = malloc(len + 1);
	memcpy(tmp2, *buf, len);
	tmp2[len] = 0;

	*buf = tmp;
	*val = tmp2;

	advance(buf);

	return 1;
}

static int match_int(const char **buf, int *val)
{
	if (!*buf)
		return 0;

	char *rest;
	long int l = strtol(*buf, &rest, 10);

	if ((l <= INT_MIN) || (l >= INT_MAX) || (rest == *buf))
		return 0;

	*val = (int)l;
	(*buf) = rest;
	advance(buf);

	return 1;
}

static int match_uint32_t(const char **buf, uint32_t *val)
{
	if (!*buf)
		return 0;

	char *rest;
	long int l = strtol(*buf, &rest, 10);

	if ((l < 0) || (l >= UINT32_MAX) || (rest == *buf))
		return 0;

	*val = (uint32_t)l;
	(*buf) = rest;
	advance(buf);

	return 1;
}

static int match_u32_hex(const char **buf, uint32_t *val)
{
	if (!*buf)
		return 0;

	char *rest;
	long long int l = strtoll(*buf, &rest, 16);

	if ((l < 0) || (l > UINT32_MAX) || (rest == *buf))
		return 0;

	*val = (uint32_t) l;
	(*buf) = rest;
	advance(buf);

	return 1;
}

static int match_eol(const char **buf)
{
	return !*buf;
}

static struct xqx_map *map_load_tmc(const char *filename);

static struct xqx_map_ops map_tmc_ops = {
	.suffix = "tmc",
	.suffix_len = 3,
	.map_load_cb = map_load_tmc,
	.read_tile_cb = read_tmc_tile,
};

void xqx_map_tmc_init(void)
{
	xqx_register_map_ops(&map_tmc_ops);
}

static struct xqx_map *map_load_tmc(const char *filename)
{
	FILE *f;
	char *buf = NULL;
	size_t bufsize = 0;
	uint32_t iw, ih, tw, th, levels, jpl, proj=0, empty_color;
	iw = ih = tw = th = levels = 0;
	jpl = UINT32_MAX;
	empty_color = 0xFFFFFFFF;

	f = fopen(filename, "r");
	if (!f)
		return NULL;


	int p1px, p1py, p1cx, p1cy, p1_ok;
	int p2px, p2py, p2cx, p2cy, p2_ok;
	p1_ok = p2_ok = 0;

	const char *suffix = NULL;

	while (getline(&buf, &bufsize, f) >= 0) {
		const char *pos = buf;
		int ok = 1;

		advance(&pos);
		if (match_fixed_str(&pos, "image-width")) {
			ok = match_uint32_t(&pos, &iw) && match_eol(&pos);
		} else if (match_fixed_str(&pos, "image-height")) {
			ok = match_uint32_t(&pos, &ih) && match_eol(&pos);
		} else if (match_fixed_str(&pos, "tile-width")) {
			ok = match_uint32_t(&pos, &tw) && match_eol(&pos);
		} else if (match_fixed_str(&pos, "tile-height")) {
			ok = match_uint32_t(&pos, &th) && match_eol(&pos);
		} else if (match_fixed_str(&pos, "tile-format")) {
			ok = match_str(&pos, &suffix) && match_eol(&pos);
		} else if (match_fixed_str(&pos, "point-1")) {
			ok = match_int(&pos, &p1px) && match_int(&pos, &p1py)
			     && match_int(&pos, &p1cx) && match_int(&pos, &p1cy)
			     && match_eol(&pos) && (p1_ok = 1);
		} else if (match_fixed_str(&pos, "point-2")) {
			ok = match_int(&pos, &p2px) && match_int(&pos, &p2py)
			     && match_int(&pos, &p2cx) && match_int(&pos, &p2cy)
			     && match_eol(&pos) && (p2_ok = 1);
		} else if (match_fixed_str(&pos, "projection")) {
			ok = match_uint32_t(&pos, &proj) && match_eol(&pos);
		} else if (match_fixed_str(&pos, "levels")) {
			ok = match_uint32_t(&pos, &levels) && match_eol(&pos);
		} else if (match_fixed_str(&pos, "empty-color")) {
			ok = match_u32_hex(&pos, &empty_color) && match_eol(&pos);
		} else if (match_fixed_str(&pos, "jpeg-level")) {
			ok = match_uint32_t(&pos, &jpl) && match_eol(&pos);
		} else if (pos != NULL) {
			printf("warning: unsupported option in '%s': '%s'\n", filename, buf);
		}

		if (!ok) {
			printf("error: unparsable line in '%s': '%s'\n", filename, buf);
			fclose(f);
			free(buf);
			return NULL;
		}
	}

	fclose(f);
	free(buf);

	int ok = 1;
	if (iw == 0) {
		printf("error: 'image-width' is missing in '%s'\n", filename); ok = 0;
	}
	if (ih == 0) {
		printf("error: 'image-height' is missing in '%s'\n", filename); ok = 0;
	}
	if (tw == 0) {
		printf("error: 'tile-width' is missing in '%s'\n", filename); ok = 0;
	}
	if (th == 0) {
		printf("error: 'tile-height' is missing in '%s'\n", filename); ok = 0;
	}
	if (suffix == NULL) {
		printf("error: 'tile-format' is missing in '%s'\n", filename); ok = 0;
	}
	if (levels == 0) {
		printf("error: 'levels' is missing in '%s'\n", filename); ok = 0;
	}
	if (p1_ok != p2_ok) {
		printf("error: 'point-1' and 'point-2' are not used together in '%s'\n", filename); ok = 0;
	}
	if (!ok) {
		return NULL;
	}

	struct xqx_map_tmc *map = calloc(1, sizeof(struct xqx_map_tmc));

	map->levels = calloc(levels, sizeof(struct xqx_tmc_level));
	map->common.num_tiles_x = calloc(levels, sizeof(int));
	map->common.num_tiles_y = calloc(levels, sizeof(int));

	map->common.ops = &map_tmc_ops;
	map->common.map_w = iw;
	map->common.map_h = ih;
	map->common.tile_w = tw;
	map->common.tile_h = th;
	map->common.num_levels = levels;

	if (p1_ok == 0)	{
		/* No georeferencing, suppose pixel-bases coordinates */
		map->common.geo_psx = map->common.geo_psy = 1;
		map->common.geo_csx = map->common.geo_csy = 16;
	} else {
		/* Georeferenced image, wow */

		/* FIXME analyze needed integer precision */
		p1cx *= 16;
		p1cy *= 16;
		p2cx *= 16;
		p2cy *= 16;

		map->common.geo_pox = p1px;
		map->common.geo_poy = p1py;
		map->common.geo_cox = p1cx;
		map->common.geo_coy = p1cy;
		map->common.geo_psx = p2px - p1px;
		map->common.geo_psy = p2py - p1py;
		map->common.geo_csx = p2cx - p1cx;
		map->common.geo_csy = p2cy - p1cy;
	}

	xqx_map_set_projection((struct xqx_map*)map, proj);

	char *tmp = strdup(filename);
	char *dn = dirname(tmp);
	char *patt = "/%02d/%04d/%04d.";

	uint32_t nbs = strlen(dn) + strlen(patt) + strlen(suffix) + 1 + 4; // + 4 for jpeg hack
	map->namebuf_size = nbs;

	char *s1 = malloc(nbs);
	char *s2 = malloc(nbs);
	snprintf(s1, nbs, "%s%s%s", dn, patt, suffix);
	snprintf(s2, nbs, "%s%sjpeg", dn, patt);
	/* FIXME ^^^ check for % in dn, patt and suffix */

	uint32_t l;
	uint32_t after = 0;
	char namebuf[nbs];
	int s1c = 0;
	int s2c = 0;

	/* in next cycle we compute number of tiles in each level (using iw, ih),
		 check for (and open) PIA files, set format_string to s1 or s2, check
		 for only one file variant and count usage of s1, s2 */

	iw = (iw + tw - 1) / tw;
	ih = (ih + th - 1) / th;

	for (l = 0; l < levels; l++) {
		snprintf(namebuf, nbs, "%s/%02d.pia", dn, l);
		if (access(namebuf, F_OK) == 0) {
			printf("Found PIA file '%s'\n", namebuf);
			map->levels[l].pia = open_pia(namebuf, 0);
			map->levels[l].empty_color = map->levels[l].pia->hdr.empty_color;
		} else {
			map->levels[l].format_string = (l < jpl) ? s1 : s2;
			map->levels[l].empty_color = empty_color;

			if ((iw == 1) && (ih == 1)) {
				snprintf(namebuf, nbs, "%s/%02d.%s", dn, l, (l < jpl) ? suffix : "jpeg");
				if (access(namebuf, F_OK) == 0) {
					map->levels[l].format_string = strdup(namebuf);
					after = 1;
				} else if (after) {
					/* really strange case */
					map->levels[l].format_string = strdup(map->levels[l].format_string);
				} else {
					((l < jpl) ? s1c++ : s2c++);
				}
			} else {
				((l < jpl) ? s1c++ : s2c++);
			}
		}

		map->common.num_tiles_x[l] = iw;
		map->common.num_tiles_y[l] = ih;

		iw = (iw + 1) / 2;
		ih = (ih + 1) / 2;
	}

	free(tmp);
	if (s1c == 0)
		free(s1);
	if (s2c == 0)
		free(s2);

	xqx_map_cache_init_map((struct xqx_map *)map);
	return (struct xqx_map *)map;
}

