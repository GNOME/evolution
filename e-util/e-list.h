/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#ifndef __E_LIST_H__
#define __E_LIST_H__

typedef struct _EList EList;
typedef struct _EListClass EListClass;

#include <stdio.h>
#include <time.h>
#include <glib.h>
#include <glib-object.h>
#include <e-util/e-list-iterator.h>

#define E_TYPE_LIST            (e_list_get_type ())
#define E_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_LIST, EList))
#define E_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_LIST, EListClass))
#define E_IS_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_LIST))
#define E_IS_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_LIST))
#define E_LIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_LIST, EListClass))

typedef void *(*EListCopyFunc) (const void *data, void *closure);
typedef void (*EListFreeFunc) (void *data, void *closure);

struct _EList {
	GObject      object;
	GList         *list;
	GList         *iterators;
	EListCopyFunc  copy;
	EListFreeFunc  free;
	void          *closure;
};

struct _EListClass {
	GObjectClass parent_class;
};

EList     *e_list_new                  (EListCopyFunc  copy, 
					EListFreeFunc  free,
					void          *closure);
void       e_list_construct            (EList         *list,
					EListCopyFunc  copy, 
					EListFreeFunc  free,
					void          *closure);
EList     *e_list_duplicate            (EList *list);
EIterator *e_list_get_iterator         (EList         *list);
void       e_list_append               (EList         *list,
					const void    *data);
void       e_list_remove               (EList         *list,
					const void    *data);
int        e_list_length               (EList         *list);

/* For iterators to call. */
void       e_list_remove_link          (EList         *list, 
					GList         *link);
void       e_list_remove_iterator      (EList         *list,
					EIterator     *iterator);
void       e_list_invalidate_iterators (EList         *list,
					EIterator     *skip);

/* Standard Glib function */
GType      e_list_get_type             (void);

#endif /* ! __E_LIST_H__ */
