/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Chris Lahey <clahey@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#ifndef __E_CARD_LIST_H__
#define __E_CARD_LIST_H__

#include <time.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <addressbook/backend/ebook/e-card-iterator.h>

#define E_TYPE_CARD_LIST            (e_card_list_get_type ())
#define E_CARD_LIST(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_CARD_LIST, ECardList))
#define E_CARD_LIST_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_CARD_LIST, ECardListClass))
#define E_IS_CARD_LIST(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_CARD_LIST))
#define E_IS_CARD_LIST_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_CARD_LIST))

typedef void *(*ECardListCopyFunc) (const void *data, void *closure);
typedef void (*ECardListFreeFunc) (void *data, void *closure);

typedef struct _ECardList ECardList;
typedef struct _ECardListClass ECardListClass;

struct _ECardList {
	GtkObject          object;
	GList             *list;
	GList             *iterators;
	ECardListCopyFunc  copy;
	ECardListFreeFunc  free;
	void              *closure;
};

struct _ECardListClass {
	GtkObjectClass parent_class;
};

ECardList     *e_card_list_new                  (ECardListCopyFunc  copy, 
						 ECardListFreeFunc  free,
						 void              *closure);
ECardIterator *e_card_list_get_iterator         (ECardList         *list);
void           e_card_list_append               (ECardList         *list,
						 const void        *data);
int            e_card_list_length               (ECardList         *list);

/* For iterators to call. */
void           e_card_list_invalidate_iterators (ECardList         *list, 
						 ECardIterator     *skip);
void           e_card_list_remove_iterator      (ECardList         *list,
						 ECardIterator     *iterator);

/* Standard Gtk function */
GtkType        e_card_list_get_type             (void);

#endif /* ! __E_CARD_LIST_H__ */
