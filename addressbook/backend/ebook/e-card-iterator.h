/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Chris Lahey <clahey@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#ifndef __E_CARD_ITERATOR_H__
#define __E_CARD_ITERATOR_H__

#include <time.h>
#include <gtk/gtk.h>
#include <stdio.h>

#define E_TYPE_CARD_ITERATOR            (e_card_iterator_get_type ())
#define E_CARD_ITERATOR(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_CARD_ITERATOR, ECardIterator))
#define E_CARD_ITERATOR_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_CARD_ITERATOR, ECardIteratorClass))
#define E_IS_CARD_ITERATOR(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_CARD_ITERATOR))
#define E_IS_CARD_ITERATOR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_CARD_ITERATOR))

typedef struct _ECardIterator ECardIterator;
typedef struct _ECardIteratorClass ECardIteratorClass;

struct _ECardIterator {
	GtkObject object;
};

struct _ECardIteratorClass {
	GtkObjectClass parent_class;

	/* Signals */
	void         (*invalidate) (ECardIterator *iterator);
	
	/* Virtual functions */
	const void * (*get)        (ECardIterator *iterator);
	void         (*reset)      (ECardIterator *iterator);
	gboolean     (*next)       (ECardIterator *iterator);
	gboolean     (*prev)       (ECardIterator *iterator);
	void         (*delete)     (ECardIterator *iterator);
	void         (*set)        (ECardIterator *iterator,
				    const void    *object);
	gboolean     (*is_valid)   (ECardIterator *iterator);
};

const void    *e_card_iterator_get        (ECardIterator *iterator);
void           e_card_iterator_reset      (ECardIterator *iterator);
gboolean       e_card_iterator_next       (ECardIterator *iterator);
gboolean       e_card_iterator_prev       (ECardIterator *iterator);
void           e_card_iterator_delete     (ECardIterator *iterator);
void           e_card_iterator_set        (ECardIterator *iterator, 
				           const void    *object);
gboolean       e_card_iterator_is_valid   (ECardIterator *iterator);

void           e_card_iterator_invalidate (ECardIterator *iterator);

/* Standard Gtk function */
GtkType        e_card_iterator_get_type   (void);

#endif /* ! __E_CARD_ITERATOR_H__ */
