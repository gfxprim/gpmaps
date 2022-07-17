// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * pia - tool for manipulation with PIA archives
 *
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021-2022 Cyril Hrubis <metan@ucw.cz>
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

#include <getopt.h>

#include "libpia.h"

#define PIA_VERSION "0.9"

#define DEFAULT_PATTERN "%04u_%04u.%s"

/*
  - create
  - add
  - extract
  - remove
  - pack pia_in pia_out
  - list

  pia -c file.pia 256 256 jpg -- *.jpg
  pia -a file.pia -- *.jpg
  pia -x file.pia 20 10

  quessable file 0003_0004.jpg
  file and position   f:10:20:aaa.jpg
  array and pattern   r:0:0:10:10:%d_%d.jpg
  all (for extract)   a:%d_%d.jpg
*/

enum {
	MODE_UNDEFINED,
	MODE_CREATE,
	MODE_ADD,
	MODE_EXTRACT,
	MODE_REMOVE,
	MODE_PACK,
	MODE_LIST
};

static int global_mode = MODE_UNDEFINED;
static char *global_pattern;
static int global_pattern_length;
static int arg_verbose;
static int arg_force;
static int arg_tile_width = 512;
static int arg_tile_height = 512;
static uint32_t arg_empty_color = 0xFFFFFFFF;

static inline void check_syserror(int cond, const char *str)
{
	if (cond) {
		perror(str);
		exit(1);
	}
}

#define BUG() internal_error(__FILE__, __LINE__)

static inline void internal_error(const char *file, int line)
{
	fprintf(stderr, "Bug at %s:%d\n", file, line);
	exit(1);
}

static inline void check_error(int cond, const char *str)
{
	if (cond) {
		fprintf(stderr, "ERROR: %s\n", str);
		exit (1);
	}
}

static void ensure_dir(const char *name_in)
{
	uint32_t i;

	if (*name_in == 0)
		return;

	char *name = strdup(name_in);
	for (i = 1; name[i] != 0; i++)
		if (name[i] == '/') {
			name[i] = 0;

			if (access(name, F_OK) < 0) {
				check_syserror(errno != ENOENT,
					       "Error during directory creation");
				int rv = mkdir(name, 0777);
				check_syserror(rv != 0,
					       "Error during directory creation");
			}
			name[i] = '/';
		}
	free(name);
}

static void add_file(struct pia_file *obj, uint32_t x, uint32_t y, const char *filename)
{
	if (access(filename, F_OK) < 0) {
		check_syserror(errno != ENOENT, "error during image opening");
		if (arg_verbose)
			printf("skipping file '%s' - file not found\n",
			       filename);
		return;
	}

	if (arg_force || (!pia_used_p(obj, x, y))) {
		if (arg_verbose)
			printf("adding file '%s' as tile (%u, %u)\n", filename,
			       x, y);
		pia_add_from_file(obj, x, y, filename, 64 * 1024);
	} else {
		printf
		    ("ERROR: skipping file '%s' - tile (%u, %u) already occupied\n",
		     filename, x, y);
		exit(-1);
	}
}

static void extract_file(struct pia_file *obj, uint32_t x, uint32_t y, const char *filename)
{
	if (!pia_used_p(obj, x, y)) {
		if (arg_verbose)
			printf("skipping tile (%u, %u) - tile not found\n", x,
			       y);
		return;
	}

	ensure_dir(filename);
	if (arg_verbose)
		printf("extracting tile (%u, %u) to file '%s'\n", x, y,
		       filename);
	pia_extract_to_file(obj, x, y, filename, 64 * 1024, arg_force);
}

static void remove_file(struct pia_file *obj, uint32_t x, uint32_t y, const char *filename)
{
	(void) filename;

	if (!pia_used_p(obj, x, y)) {
		if (arg_verbose)
			printf("skipping tile (%u, %u) - tile not found\n", x, y);
		return;
	}

	if (arg_verbose)
		printf("removing tile (%u, %u)\n", x, y);
	pia_remove_item(obj, x, y);
}

static void process_file(uint32_t x, uint32_t y,
	     void (*file_cb)(struct pia_file *, uint32_t, uint32_t, const char *),
	     struct pia_file *obj)
{
	uint32_t blen = global_pattern_length + 50;
	char str[blen + 1];
	snprintf(str, blen, global_pattern, x, y, obj->hdr.suffix);
	str[blen] = '0';
	file_cb(obj, x, y, str);
}

static void
process_rectangle(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
		  void (*file_cb)(struct pia_file *, uint32_t, uint32_t, const char *),
		  struct pia_file *obj)
{
	uint32_t ox, oy;

	for (oy = 0; oy < h; oy++) {
		for (ox = 0; ox < w; ox++)
			process_file(x + ox, y + oy, file_cb, obj);
	}
}

// Suppose s != NULL, zero terminated
static int update_pattern(const char *s)
{
	if ((s[0] != ':') || (s[1] == 0))
		return (0);

	// FIXME check pattern
	free(global_pattern);
	global_pattern = strdup(s + 1);
	global_pattern_length = strlen(global_pattern);
	return (1);
}

static void
parse_item(const char *s,
	   void (*file_cb)(struct pia_file *, uint32_t, uint32_t, const char *),
	   struct pia_file *obj)
{
	uint32_t x, y, w, h;
	const char *p;
	int rv, off;

	switch (s[0]) {
	case '0' ... '9':
		rv = sscanf(s, "%u%*[^0-9]%u", &x, &y);
		if (rv == 2)
			file_cb(obj, x, y, s);
		else
			fprintf(stderr,
				"ERROR: parsing of item '%s' failed, skipping\n",
				s);
		break;

	case '.':
	case '/':
		rv = sscanf(s, "%*[^0-9]%u%*[^0-9]%u", &x, &y);
		if (rv == 2)
			file_cb(obj, x, y, s);
		else
			fprintf(stderr,
				"ERROR: parsing of item '%s' failed, skipping\n",
				s);
		break;

	case 'f':
		rv = sscanf(s, "f:%u:%u%n", &x, &y, &off);
		p = s + off;
		if ((rv == 2) && (p[0] == 0))
			process_file(x, y, file_cb, obj);
		else if ((rv == 2) && (p[0] == ':') && (p[1] != 0))
			file_cb(obj, x, y, p + 1);
		else
			fprintf(stderr,
				"ERROR: parsing of item '%s' failed, skipping\n",
				s);
		break;

	case 'r':
		rv = sscanf(s, "r:%u:%u:%u:%u%n", &x, &y, &w, &h, &off);
		p = s + off;
		if ((rv == 4) && ((p[0] == 0) || update_pattern(p)))
			process_rectangle(x, y, w, h, file_cb, obj);
		else
			fprintf(stderr,
				"ERROR: parsing of item '%s' failed, skipping\n",
				s);
		break;

	case 'a':
		if ((s[1] == 0) || update_pattern(s + 1))
			process_rectangle(0, 0, obj->hdr.table_width,
					  obj->hdr.table_height, file_cb, obj);
		else
			fprintf(stderr,
				"ERROR: parsing of item '%s' failed, skipping\n",
				s);
		break;

	case 'p':
		if (!update_pattern(s + 1))
			fprintf(stderr,
				"ERROR: parsing of item '%s' failed, skipping\n",
				s);
		break;

	default:
		fprintf(stderr,
			"ERROR: parsing of item '%s' failed (unknown type), skipping\n",
			s);
		break;
	}
}

static int
main_create(const char *file, uint32_t tbl_w, uint32_t tbl_h, const char *suffix,
	    int argc, char **argv)
{
	struct pia_file *obj;
	int i;

	obj = make_pia(file, tbl_w, tbl_h, arg_tile_width, arg_tile_height, suffix, arg_empty_color);

	for (i = 0; i < argc; i++)
		parse_item(argv[i], add_file, obj);

	pia_close(obj);

	return 0;
}

static int main_add(const char *file, int argc, char **argv)
{
	struct pia_file *obj = open_pia(file, 1);
	int i;

	for (i = 0; i < argc; i++)
		parse_item(argv[i], add_file, obj);

	pia_close(obj);
	return (0);
}

static int main_extract(const char *file, int argc, char **argv)
{
	struct pia_file *obj = open_pia(file, 0);
	int i;

	for (i = 0; i < argc; i++)
		parse_item(argv[i], extract_file, obj);

	pia_close(obj);
	return 0;
}

static int main_remove(const char *file, int argc, char **argv)
{
	struct pia_file *obj = open_pia(file, 1);
	int i;

	for (i = 0; i < argc; i++)
		parse_item(argv[i], remove_file, obj);

	pia_close(obj);
	return 0;
}

static int main_pack(const char *src_file, const char *dst_file)
{
	printf("unsupported");
	return 1;
/*
  struct pia_file *src = open_pia(src_file, 0);
  struct pia_file *dst = make_pia(dst_file, src->hdr.table_width, src->hdr.table_height,
				  src->hdr.tile_width, src->hdr.tile_height,
				  src->hdr.suffix, src->hdr.empty_color);

  uint32_t i = 0;
  uint32_t w = src->hdr.table_width;
  uint32_t m = w * src->hdr.table_height;

  for (i = 0; i < m; i++)
    if (src->table[i].offset)
      pia_add_from_fd(dst, i % w, i / w, src->fd, src->table[i].offset
		      + sizeof (struct pia_item_header), src->table[i].size, 64*1024);

  pia_close(dst);
  pia_close(src);
  return (0);
*/
}

static int main_list(const char *file)
{
	struct pia_file *obj;
	uint32_t i = 0;

	obj = open_pia(file, 0);
	if (!obj)
		return 1;

	uint32_t w = obj->hdr.table_width;
	uint32_t m = w * obj->hdr.table_height;

	printf("PIA file '%s'\n\n"
	       "suffix:\t\t%s\n"
	       "table-width:\t%u\n"
	       "table-height:\t%u\n"
	       "tile-width:\t%u\n"
	       "tile-height:\t%u\n"
	       "empty-color:\t0x%x\n\n",
	       file, obj->hdr.suffix, obj->hdr.table_width,
	       obj->hdr.table_height, obj->hdr.tile_width, obj->hdr.tile_height,
	       obj->hdr.empty_color);

	printf("X\tY\toffset\tsize\tend\n");

	for (i = 0; i < m; i++) {
		if (obj->table[i].offset) {
			struct pia_item *pi = pia_open_item(obj, i % w, i / w);
			printf("%u\t%u\t%llu\t%llu\t%llu\n",
			       i % w, i / w,
			       (long long unsigned) pi->offset,
			       (long long unsigned) pi->size,
			       (long long unsigned) pi->offset + pi->size);
			pia_item_close(pi);
		}
	}

	pia_close(obj);
	return 0;
}

static void print_help(const char *c)
{
	printf("PIA archiving tool, version %s\n\n"
	       "Usage:\n"
	       "    %s --create  archive table-width table-height suffix ...\n"
	       "    %s --add     archive ...\n"
	       "    %s --extract archive ...\n"
	       "    %s --remove  archive ...\n"
	       "    %s --pack    archive-in archive-out\n"
	       "    %s --list    archive\n"
	       "    %s --help\n"
	       "    %s --version\n"
	       "  where ... is a list of options and specifications\n"
	       "  it is possible to use shortcuts (-c -a -x -r -p -l -h -V)\n\n"
	       "Options:\n"
	       "    --common-data-header bytes (number or 'auto')   Can be used only with --create with some files specified.\n"
	       "    --verbose | -v\n"
	       "    --force | -f\n"
	       "    --tile-width number\n"
	       "    --tile-height number\n"
	       "    --empty-color color (in hexadecimal notation)\n\n"
	       "Specifications:\n"
	       "    F            - position quessed from filename F (beginning with [0-9./])\n"
	       "    f:X:Y        - position (X, Y), filename by default pattern\n"
	       "    f:X:Y:F      - position (X, Y), filename F\n"
	       "    r:X:Y:W:H    - rectangle of positions (X, Y, W, H), filenames by default pattern\n"
	       "    r:X:Y:W:H:P  - rectangle of positions (X, Y, W, H), filenames by pattern P\n"
	       "    a            - all positions, filenames by default pattern\n"
	       "    a:P          - all positions, filenames by pattern P\n"
	       "  default pattern is %s\n",
	       PIA_VERSION, c, c, c, c, c, c, c, c, DEFAULT_PATTERN);
	exit(0);
}

static void print_version(void)
{
	printf("PIA archiving tool, version %s\n"
	       "Copyright (C) 2008 Ondrej Zajicek\n"
               "Copyright (C) 2021-2022 Cyril Hrubis <metan@ucw.cz>\n"
	       "This is free software.  You may redistribute copies of it under the terms of\n"
	       "the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
	       "There is NO WARRANTY, to the extent permitted by law.\n",
	       PIA_VERSION);
	exit(0);
}

int main(int argc, char **argv)
{
	int rv;
	static struct option options[] = {
		{ "create", no_argument, 0, 'c' },
		{ "add", no_argument, 0, 'a' },
		{ "extract", no_argument, 0, 'x' },
		{ "remove", no_argument, 0, 'r' },
		{ "pack", no_argument, 0, 'p' },
		{ "list", no_argument, 0, 'l' },
		{ "help", no_argument, 0, 'h' },
		{ "version", no_argument, 0, 'V' },
		{ "verbose", no_argument, 0, 'v' },
		{ "force", no_argument, 0, 'f' },
		{ "tile-width", required_argument, 0, 1 },
		{ "tile-height", required_argument, 0, 2 },
		{ "empty-color", required_argument, 0, 3 },
		{ 0, 0, 0, 0 }
	};

	global_pattern = strdup(DEFAULT_PATTERN);
	global_pattern_length = strlen(global_pattern);

	while (1) {
		int index = 0;
		int c = getopt_long(argc, argv, "caxrplhVvf", options, &index);

		if (c == -1)
			break;

		switch (c) {
		case 'c':
			check_error(global_mode != MODE_UNDEFINED,
				    "more than one command specified");
			global_mode = MODE_CREATE;
		break;
		case 'a':
			check_error(global_mode != MODE_UNDEFINED,
				    "more than one command specified");
			global_mode = MODE_ADD;
		break;
		case 'x':
			check_error(global_mode != MODE_UNDEFINED,
				    "more than one command specified");
			global_mode = MODE_EXTRACT;
		break;
		case 'r':
			check_error(global_mode != MODE_UNDEFINED,
				    "more than one command specified");
			global_mode = MODE_REMOVE;
		break;
		case 'p':
			check_error(global_mode != MODE_UNDEFINED,
				    "more than one command specified");
			global_mode = MODE_PACK;
		break;
		case 'l':
			check_error(global_mode != MODE_UNDEFINED,
				    "more than one command specified");
			global_mode = MODE_LIST;
		break;
		case 'h':
			print_help(argv[0]);
			exit(0);
		case 'V':
			print_version();
			exit(0);
		case 'v':
			arg_verbose = 1;
		break;
		case 'f':
			arg_force = 1;
		break;
		case 1:
			rv = sscanf(optarg, "%u", &arg_tile_width);
			check_error(rv != 1,
				    "invalid argument after --tile-width");
		break;
		case 2:
			rv = sscanf(optarg, "%u", &arg_tile_height);
			check_error(rv != 1,
				    "invalid argument after --tile-height");
		break;
		case 3:
			rv = sscanf(optarg, "%x", &arg_empty_color);
			check_error(rv != 1,
				    "invalid argument after --empty-color");
		break;
		case -1:
		case '?':
			exit(-1);

		default:
			BUG();
		}
	}

	int rest = argc - optind;
	uint32_t w, h;

	switch (global_mode) {
	case MODE_UNDEFINED:
		check_error(1,
			    "no command specified, use --help command to list commands");
		break;

	case MODE_CREATE:
		check_error(rest < 4,
			    "not enough arguments of 'create' command");
		rv = sscanf(argv[optind + 1], "%u", &w);
		check_error(rv =
			    !1,
			    "Invalid 'table-width' argument of 'create' command");
		rv = sscanf(argv[optind + 2], "%u", &h);
		check_error(rv =
			    !1,
			    "Invalid 'table-height' argument of 'create' command");
		return main_create(argv[optind], w, h, argv[optind + 3],
				   argc - optind - 4, argv + optind + 4);

	case MODE_ADD:
		check_error(rest < 1, "not enough arguments of 'add' command");
		return main_add(argv[optind], argc - optind - 1,
				argv + optind + 1);

	case MODE_EXTRACT:
		check_error(rest < 1,
			    "not enough arguments of 'extract' command");
		return main_extract(argv[optind], argc - optind - 1,
				    argv + optind + 1);

	case MODE_REMOVE:
		check_error(rest < 1,
			    "not enough arguments of 'remove' command");
		return main_remove(argv[optind], argc - optind - 1,
				   argv + optind + 1);

	case MODE_PACK:
		check_error(rest < 2, "not enough arguments of 'pack' command");
		check_error(rest > 2, "too many arguments of 'pack' command");
		return main_pack(argv[optind], argv[optind + 1]);

	case MODE_LIST:
		check_error(rest < 1, "not enough arguments to 'list' command");
		check_error(rest > 1, "too many arguments to 'list' command");
		return main_list(argv[optind]);

	default:
		BUG();
	}

	return 0;
}
