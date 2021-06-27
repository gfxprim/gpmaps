// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2006-2008 Ondrej 'SanTiago' Zajicek
 * Copyright (c) 2021 Cyril Hrubis <metan@ucw.cz>
 */

#ifndef XQX_DLLIST_H__
#define XQX_DLLIST_H__

#include <stddef.h>

#define DLL_REMOVE(HEAD,FIRST,LAST,NODE,PREV,NEXT)	\
do {							\
	if (!(NODE)->NEXT)				\
		(HEAD)->LAST = (NODE)->PREV;		\
	else						\
		(NODE)->NEXT->PREV = (NODE)->PREV;	\
							\
	if (!(NODE)->PREV)				\
		(HEAD)->FIRST = (NODE)->NEXT;		\
	else						\
		(NODE)->PREV->NEXT = (NODE)->NEXT;	\
							\
	(NODE)->PREV = NULL;				\
	(NODE)->NEXT = NULL;				\
							\
} while(0)

#define DLL_PREPEND(HEAD,FIRST,LAST,NODE,PREV,NEXT)	\
do {							\
	(NODE)->PREV = NULL;				\
	(NODE)->NEXT = (HEAD)->FIRST;			\
							\
	if (!(HEAD)->FIRST) {				\
		(HEAD)->FIRST = NODE;			\
		(HEAD)->LAST = NODE;			\
	} else {					\
		(HEAD)->FIRST->PREV = NODE;		\
		(HEAD)->FIRST = NODE;			\
	}						\
} while(0)

#define DLL_APPEND(HEAD,FIRST,LAST,NODE,PREV,NEXT)	\
do {							\
	(NODE)->PREV = (HEAD)->LAST;			\
	(NODE)->NEXT = NULL;				\
							\
	if (!(HEAD)->FIRST) {				\
		(HEAD)->FIRST = NODE;			\
		(HEAD)->LAST = NODE;			\
	} else {					\
		(HEAD)->LAST->NEXT = NODE;		\
		(HEAD)->LAST = NODE;			\
	}						\
} while(0)

#define DLL_INSERT_AFTER(WHERE,NODE,PREV,NEXT)		\
do {							\
	(NODE)->PREV = WHERE;				\
	(NODE)->NEXT = WHERE->NEXT;			\
	(WHERE)->NEXT = NODE;				\
	(NODE)->NEXT->PREV = NODE;			\
} while(0)

#endif /* XQX_DLLIST_H__ */
