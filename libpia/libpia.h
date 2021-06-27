// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#ifndef LIBPIA_H__
#define LIBPIA_H__

#include <stdint.h>

#define PIA_HDR_MAGIC 0x59A14C76
#define PIA_ITEM_MAGIC 0x97F21E5B
#define PIA_EB_CDH_MAGIC 0x72646548

#define PIA_TABLE_SIZE_MAX (2<<24)

struct pia_node
{
	uint64_t offset;
	uint64_t size;
};

struct pia_item_header
{
	uint64_t magic;
	uint32_t x;
	uint32_t y;
	uint64_t size;
};

struct pia_header
{
	uint64_t magic;
	uint32_t table_width;
	uint32_t table_height;
	uint32_t tile_width;
	uint32_t tile_height;
	uint32_t empty_color;
	uint32_t reserved;
	char suffix[8];
};

struct pia_extended_block_header
{
	uint32_t type;
	uint32_t shortlen;
};

struct pia_file
{
	struct pia_header hdr;
	struct pia_node *table;
	int fd;
	int rw;
	int children;
	int table_dirty;

	// appending in progress
	int app_run;
	uint32_t app_x;
	uint32_t app_y;
	uint64_t app_size;
	uint64_t app_offset;
};

struct pia_item
{
	struct pia_file *pia;
	uint64_t size;
	off64_t offset;
	off64_t position;
};

struct pia_file *open_pia(const char *filename, int rw);

struct pia_file *
make_pia(const char *filename, unsigned int tbl_w, unsigned int tbl_h,
	 unsigned int tile_w, unsigned int tile_h,
	 const char *suffix, uint32_t empty_color);

int pia_close(struct pia_file *obj);

uint64_t pia_get_item_offset(struct pia_file *obj, uint32_t x, uint32_t y);

uint64_t pia_get_item_size(struct pia_file *obj, uint32_t x, uint32_t y);

static inline int pia_used_p(struct pia_file *obj, uint32_t x, uint32_t y)
{
	return (pia_get_item_offset(obj, x, y) != 0);
}

struct pia_item *pia_open_item(struct pia_file *obj, uint32_t x, uint32_t y);

ssize_t pia_item_read(struct pia_item *obj, void *buf, size_t count);

off64_t pia_item_seek(struct pia_item *obj, off64_t off, int whence);

off64_t pia_item_tell(struct pia_item *obj);

void pia_item_close(struct pia_item *obj);

void pia_append_item(struct pia_file *obj, uint32_t x, uint32_t y);

void pia_append_data(struct pia_file *obj, void *buf, size_t count);

void pia_append_finish(struct pia_file *obj);

void pia_remove_item(struct pia_file *obj, uint32_t x, uint32_t y);

ssize_t pia_read_whole_item(struct pia_file *obj, uint32_t x, uint32_t y, void **buf);

void pia_add_from_file(struct pia_file *obj, uint32_t x, uint32_t y, const char *filename, int bufsize);

void pia_extract_to_file(struct pia_file *obj, uint32_t x, uint32_t y, const char *filename, int bufsize, int force);

#endif /* LIBPIA_H__ */

