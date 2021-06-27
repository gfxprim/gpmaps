// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#ifndef XQX_LIST_H__
#define XQX_LIST_H__

#include "xqx_common.h"

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

#define LIST_HEAD(name) struct list_head name = {&name, &name}

#define LIST_FOREACH(list_root, iter) \
	for (struct list_head *iter = (list_root)->next; iter != (list_root); iter = iter->next)

#define LIST_FOREACH_SAFE(list_root, iter) \
	for (struct list_head *iter = (list_root)->next, *tmp__ = iter->next; iter != (list_root); iter = tmp__, tmp__ = iter->next)

static inline void list_init(struct list_head *root)
{
	root->prev = root;
	root->next = root;
}

static inline void list_add(struct list_head *prev, struct list_head *next, struct list_head *memb)
{
	next->prev = memb;
	memb->next = next;
	memb->prev = prev;
	prev->next = memb;
}

static inline void list_append(struct list_head *root, struct list_head *memb)
{
	list_add(root->prev, root, memb);
}

static inline void list_remove(struct list_head *memb)
{
	memb->prev->next = memb->next;
	memb->next->prev = memb->prev;

	memb->prev = NULL;
	memb->next = NULL;
}

#define LIST_ENTRY(ptr, type, member) CONTAINER_OF(ptr, type, member)

#endif /* XQX_LIST_H__ */
