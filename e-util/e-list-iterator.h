/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#ifndef __E_LIST_ITERATOR_H__
#define __E_LIST_ITERATOR_H__

typedef struct _EListIterator EListIterator;
typedef struct _EListIteratorClass EListIteratorClass;

#include <stdio.h>
#include <time.h>
#include <glib.h>
#include <glib-object.h>

#include <e-util/e-iterator.h>
#include <e-util/e-list.h>

#define E_TYPE_LIST_ITERATOR            (e_list_iterator_get_type ())
#define E_LIST_ITERATOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_LIST_ITERATOR, EListIterator))
#define E_LIST_ITERATOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_LIST_ITERATOR, EListIteratorClass))
#define E_IS_LIST_ITERATOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_LIST_ITERATOR))
#define E_IS_LIST_ITERATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_LIST_ITERATOR))

struct _EListIterator {
	EIterator      parent;

	EList         *list;
	GList         *iterator;
};

struct _EListIteratorClass {
	EIteratorClass parent_class;
};

EIterator *e_list_iterator_new      (EList *list);

/* Standard Glib function */
GType      e_list_iterator_get_type (void);

#endif /* ! __E_LIST_ITERATOR_H__ */
