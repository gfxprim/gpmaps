// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#ifndef XQX_COMMON_H__
#define XQX_COMMON_H__

#include <stddef.h>

struct xqx_map;

#define MAX(a, b) ({ \
	typeof(a) a__ = (a); \
	typeof(b) b__ = (b); \
	a__ > b__ ? a__ : b__; \
})

#define MIN(a, b) ({ \
	typeof(a) a__ = (a); \
	typeof(b) b__ = (b); \
	a__ < b__ ? a__ : b__; \
})

#define ABS(a) ({ \
	typeof(a) a__ = (a); \
	a__ < 0 ? -a__ : a__; \
})

#define CLAMP(x, min, max) (((x) > (max)) ? (max) : (((x) < (min)) ? (min) : (x)))

#define CONTAINER_OF(ptr, structure, member) \
        ((structure *)((char *)(ptr) - offsetof(structure, member)))

#endif /* XQX_COMMON_H__ */
