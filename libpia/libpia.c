// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include "libpia.h"

#define DBG(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define ABORT(fmt, ...) do { fprintf(stderr, fmt "\n", ##__VA_ARGS__); abort(); } while (0)

struct pia_file *open_pia(const char *filename, int rw)
{
	ssize_t rc;
	struct pia_file *obj;
	int err = EINVAL;

	obj = malloc(sizeof(struct pia_file));
	if (!obj) {
		err = ENOMEM;
		goto err0;
	}

	obj->rw = rw;
	obj->fd = open64(filename, (rw ? O_RDWR : O_RDONLY));
	if (obj->fd < 0) {
		err = errno;
		goto err1;
	}

	rc = read(obj->fd, &(obj->hdr), sizeof(struct pia_header));
	if (rc < (ssize_t)sizeof(struct pia_header)) {
		DBG("Failed to read PIA header");
		goto err2;
	}

	uint64_t ts = obj->hdr.table_width * obj->hdr.table_height;

	if (ts > PIA_TABLE_SIZE_MAX) {
		DBG("invalid PIA header - too large table");
		goto err2;
	}

	if (obj->hdr.magic != PIA_HDR_MAGIC) {
		DBG("Invalid PIA header - bad magic number");
		goto err2;
	}

	if (obj->hdr.reserved) {
		DBG("invalid PIA header - different version?");
		goto err2;
	}

	obj->table = malloc(sizeof(struct pia_node) * ts);
	if (!obj->table) {
		err = ENOMEM;
		goto err2;
	}

	rc = read(obj->fd, obj->table, sizeof (struct pia_node) * ts);
	if (rc < (ssize_t)(sizeof(struct pia_node) * ts)) {
		DBG("PIA table read failed");
		goto err3;
	}

	obj->children = 0;
	obj->table_dirty = 0;

	return obj;
err3:
	free(obj->table);
err2:
	close(obj->fd);
err1:
	free(obj);
err0:
	errno = err;
	return NULL;
}

struct pia_file *make_pia(const char *filename, unsigned int tbl_w, unsigned int tbl_h,
	 unsigned int tile_w, unsigned int tile_h, const char *suffix, uint32_t empty_color)
{
	struct pia_file *obj;
	ssize_t rc;
	uint64_t ts = tbl_w * tbl_h;
	int err = EINVAL;

	if (ts > PIA_TABLE_SIZE_MAX) {
		DBG("invalid make_pia() request - too large table");
		goto err0;
	}

	obj = malloc(sizeof(struct pia_file));
	if (!obj) {
		err = ENOMEM;
		goto err0;
	}

	obj->hdr.magic = PIA_HDR_MAGIC;
	obj->hdr.table_width = tbl_w;
	obj->hdr.table_height = tbl_h;
	obj->hdr.tile_width = tile_w;
	obj->hdr.tile_height = tile_h;
	obj->hdr.empty_color = empty_color;
	obj->hdr.reserved = 0;
	strncpy(obj->hdr.suffix, suffix, sizeof(obj->hdr.suffix)-1);
	obj->hdr.suffix[sizeof(obj->hdr.suffix)-1] = 0;

	obj->table = calloc(ts, sizeof(struct pia_node));
	if (!obj->table) {
		err = ENOMEM;
		goto err1;
	}

	obj->rw = 1;
	obj->fd = open64(filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (obj->fd < 0) {
		err = errno;
		DBG("Failed to open file");
		goto err2;
	}

	rc = write(obj->fd, &(obj->hdr), sizeof(struct pia_header));
	if (rc < (ssize_t)sizeof(struct pia_header)) {
		err = errno;
		DBG("PIA header write failed");
		goto err3;
	}

	rc = write(obj->fd, obj->table, sizeof(struct pia_node) * ts);
	if (rc < (ssize_t)(sizeof(struct pia_node) * ts)) {
		err = errno;
		DBG("PIA table write failed");
		goto err3;
	}

	obj->children = 0;
	obj->table_dirty = 0;

	return obj;
err3:
	close(obj->fd);
err2:
	free(obj->table);
err1:
	free(obj);
err0:
	errno = err;
	return NULL;
}

int pia_close(struct pia_file *obj)
{
	if (obj->table_dirty) {
		fsync(obj->fd);
		ssize_t ts = obj->hdr.table_width * obj->hdr.table_height * sizeof(struct pia_node);
		ssize_t rc = pwrite64(obj->fd, obj->table, ts, sizeof(struct pia_header));
		if (rc < ts) {
			DBG("PIA table write failed");
			return 1;
		}
	}

	if (obj->children) {
		DBG("invalid pia_close() request - opened PIA items");
		return 1;
	}

	if (obj->app_run) {
		DBG("invalid pia_close() request - append in progress");
		return 1;
	}

	free(obj->table);
	free(obj);

	return close(obj->fd);
}

static uint64_t get_item_index(struct pia_file *obj, uint32_t x, uint32_t y)
{
	if (x >= obj->hdr.table_width) {
		DBG("invalid request - x out of range");
		return (uint64_t)-1;
	}

	if (y >= obj->hdr.table_height) {
		DBG("invalid request - y out of range");
		return (uint64_t)-1;
	}

	return  x + (y * obj->hdr.table_width);
}

uint64_t pia_get_item_offset(struct pia_file *obj, uint32_t x, uint32_t y)
{
	uint64_t index = get_item_index(obj, x, y);

	if (index == (uint64_t)-1)
		return (uint64_t)-1;

	return obj->table[index].offset;
}

uint64_t pia_get_item_size(struct pia_file *obj, uint32_t x, uint32_t y)
{
	uint64_t index = get_item_index(obj, x, y);

	if (index == (uint64_t)-1)
		return (uint64_t)-1;

	return obj->table[index].size;
}

struct pia_item *pia_open_item(struct pia_file *obj, uint32_t x, uint32_t y)
{
	struct pia_item *item;
	struct pia_item_header head;
	ssize_t rc;
	uint64_t index, offset;
	int err = EINVAL;

	index = get_item_index(obj, x, y);
	if (index == (uint64_t)-1)
		goto err0;

	offset = obj->table[index].offset;
	if (offset == 0) {
		DBG("empty position in PIA table");
		err = 0;
		goto err0;
	}

	item = malloc(sizeof(struct pia_item));
	if (!item) {
		err = ENOMEM;
		goto err0;
	}

	item->pia = obj;
	item->offset = obj->table[index].offset;
	item->size = obj->table[index].size;

	rc = pread64(obj->fd, &head, sizeof(struct pia_item_header), item->offset);
	if (rc != sizeof(struct pia_item_header)) {
		DBG("PIA item header read failed");
		goto err1;
	}

	if (head.magic != PIA_ITEM_MAGIC) {
		DBG("Invalid item header magic");
		goto err1;
	}

	if (head.size != item->size)
		DBG("Invalid item header size");

	if (head.x != x || head.y != y)
		DBG("Invalid item header tile position");

	item->offset += sizeof(struct pia_item_header);
	item->position = 0;

	obj->children++;

	return item;
err1:
	free(item);
err0:
	errno = err;
	return NULL;
}

ssize_t pia_item_read(struct pia_item *item, void *buf, size_t count)
{
	ssize_t rc;

	// FIXME - bug - position might be advanced
	if (item->position + count > item->size)
		count = item->size - item->position;

	rc = pread64(item->pia->fd, buf, count, item->offset + item->position);
	if (rc > 0)
		item->position += rc;

	return rc;
}

off64_t pia_item_seek(struct pia_item *item, off64_t off, int whence)
{
	off64_t new_pos;

	if (whence == SEEK_SET)
		new_pos = off;
	else if (whence == SEEK_CUR)
		new_pos = item->position + off;
	else
		return (off64_t)-1;

	if ((new_pos < 0) || (new_pos > (off64_t)item->size))
		return (off64_t)-1;

	item->position = new_pos;

	return item->position;
}

off64_t pia_item_tell(struct pia_item *item)
{
	return item->position;
}

void pia_item_close(struct pia_item *item)
{
	item->pia->children--;
	free(item);
}

void pia_append_item(struct pia_file *obj, uint32_t x, uint32_t y)
{
	struct pia_item_header head = {0,0,0,0};

	if (x >= obj->hdr.table_width)
		ABORT("invalid request - x out of range");

	if (y >= obj->hdr.table_height)
		ABORT("invalid request - y out of range");

	if (!obj->rw)
		ABORT("invalid request - file opened read-only");

	if (obj->app_run)
		ABORT("invalid request - append already in progress");

	obj->app_run = 1;
	obj->app_x = x;
	obj->app_y = y;

	off64_t rc1 = lseek64(obj->fd, 0, SEEK_END);
	if (rc1 != 0)
		ABORT("PIA item header write failed");

	ssize_t rc2 = write(obj->fd, &head, sizeof(struct pia_item_header));
	if (rc2 != sizeof(struct pia_item_header))
		ABORT("PIA item header write failed");

	obj->app_size = 0;
	obj->app_offset = rc1;
}

void pia_append_data(struct pia_file *obj, void *buf, size_t count)
{
	if (!obj->app_run)
		ABORT("invalid request - no append in progress");

	off64_t rc1 = lseek64(obj->fd, 0, SEEK_END);
	if (rc1 != 0)
		ABORT("PIA item header write failed");

	ssize_t rc2 = write(obj->fd, buf, count);
	if (rc2 != (ssize_t)count)
		ABORT("PIA item data write failed");

	obj->app_size += count;
}

void pia_append_finish(struct pia_file *obj)
{
	if (!obj->app_run)
		ABORT("invalid request - no append in progress");

	struct pia_item_header head = {PIA_ITEM_MAGIC, obj->app_x, obj->app_y, obj->app_size};

	ssize_t rc = pwrite64(obj->fd, &head, sizeof (struct pia_item_header), obj->app_offset);
	if (rc != (ssize_t)sizeof(struct pia_item_header))
		ABORT("PIA item header write failed");

	uint64_t index = obj->app_x + (obj->app_y * obj->hdr.table_width);

	obj->table[index].offset = obj->app_offset;
	obj->table[index].size = obj->app_size;
	obj->table_dirty = 1;
	obj->app_run = 0;
}

void pia_remove_item(struct pia_file *obj, uint32_t x, uint32_t y)
{
	uint64_t index = get_item_index(obj, x, y);

	if (index == (uint64_t)-1)
		return;

	obj->table[index].offset = 0;
	obj->table[index].size = 0;
	obj->table_dirty = 1;
}

ssize_t pia_read_whole_item(struct pia_file *obj, uint32_t x, uint32_t y, void **buf)
{
	struct pia_item *item = pia_open_item(obj, x, y);

	if (!item) {
		*buf = NULL;
		return 0;
	}

	size_t size = pia_get_item_size(obj, x, y);

	(*buf) = malloc(size);
	if (!*buf) {
		errno = ENOMEM;
		return 0;
	}

	ssize_t rv = pia_item_read(item, *buf, size);
	if (rv < (ssize_t)size) {
		DBG("PIA item data read failed");
		free(*buf);
		return 0;
	}

	pia_item_close(item);
	return size;
}

void pia_add_from_file(struct pia_file *obj, uint32_t x, uint32_t y, const char *filename, int bufsize)
{
	char buf[bufsize];
	ssize_t rv;

	int src = open64(filename, O_RDONLY);
	if (src < 0)
		ABORT("source file open failed");

	pia_append_item(obj, x, y);

	while (1) {
		rv = read(src, buf, bufsize);
		if (rv <= 0)
			break;
		pia_append_data(obj, buf, rv);
	}
	if (rv < 0)
		ABORT("source file read failed");

	pia_append_finish(obj);
	close(src);
}

void pia_extract_to_file(struct pia_file *obj, uint32_t x, uint32_t y, const char *filename, int bufsize, int force)
{
	char buf[bufsize];
	ssize_t rv;
	struct pia_item *item = pia_open_item(obj, x, y);

	if (!item)
		ABORT("invalid request - PIA item open failed");

	int dst = open64(filename, O_RDWR | O_CREAT | (force ? O_TRUNC : O_EXCL), 0666);

	if (dst < 0)
		ABORT("destination file open failed");

	while (1) {
		rv = pia_item_read(item, buf, bufsize);
		if (rv <= 0)
			break;
		rv = write(dst, buf, rv);
		if (rv < 0)
			ABORT("destination file write failed");
	}
	if (rv < 0)
		ABORT("source file read failed");

	pia_item_close(item);
	close(dst);
}
