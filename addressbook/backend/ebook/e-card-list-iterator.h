/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Chris Lahey <clahey@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#ifndef __E_CARD_LIST_ITERATOR_H__
#define __E_CARD_LIST_ITERATOR_H__

#include <time.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <ebook/e-card-iterator.h>
#include <ebook/e-card-list.h>

#define E_TYPE_CARD_LIST_ITERATOR            (e_card_list_iterator_get_type ())
#define E_CARD_LIST_ITERATOR(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_CARD_LIST_ITERATOR, ECardListIterator))
#define E_CARD_LIST_ITERATOR_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_CARD_LIST_ITERATOR, ECardListIteratorClass))
#define E_IS_CARD_LIST_ITERATOR(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_CARD_LIST_ITERATOR))
#define E_IS_CARD_LIST_ITERATOR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_CARD_LIST_ITERATOR))

typedef struct _ECardListIterator ECardListIterator;
typedef struct _ECardListIteratorClass ECardListIteratorClass;

struct _ECardListIterator {
	ECardIterator      parent;

	ECardList         *list;
	GList             *iterator;
};

struct _ECardListIteratorClass {
	ECardIteratorClass parent_class;
};

ECardIterator *e_card_list_iterator_new (ECardList *list);

/* Standard Gtk function */
GtkType        e_card_list_iterator_get_type   (void);

#endif /* ! __E_CARD_LIST_ITERATOR_H__ */
