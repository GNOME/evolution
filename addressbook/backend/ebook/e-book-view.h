/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * The Evolution addressbook client object.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 1999, 2000, Helix Code, Inc.
 */

#ifndef __E_BOOK_VIEW_H__
#define __E_BOOK_VIEW_H__

#include <libgnome/gnome-defs.h>

#include <addressbook/backend/ebook/e-card.h>
#include <addressbook/backend/ebook/e-book-view-listener.h>

BEGIN_GNOME_DECLS

typedef struct _EBookView        EBookView;
typedef struct _EBookViewClass   EBookViewClass;
typedef struct _EBookViewPrivate EBookViewPrivate;

struct _EBookView {
	GtkObject     parent;
	EBookViewPrivate *priv;
};

struct _EBookViewClass {
	GtkObjectClass parent;

	/*
	 * Signals.
	 */
	void (* card_changed)      (EBookView *book_view, const GList *cards);
	void (* card_removed)      (EBookView *book_view, const char *id);
	void (* card_added)        (EBookView *book_view, const GList *cards);
	void (* sequence_complete) (EBookView *book_view);
};

/* Creating a new addressbook. */
EBookView         *e_book_view_new                    (Evolution_BookView corba_book_view, EBookViewListener *listener);

GtkType            e_book_view_get_type               (void);
void               e_book_view_get_book_view_listener (EBookView *book_view);

#define E_BOOK_VIEW_TYPE        (e_book_view_get_type ())
#define E_BOOK_VIEW(o)          (GTK_CHECK_CAST ((o), E_BOOK_VIEW_TYPE, EBookView))
#define E_BOOK_VIEW_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_BOOK_VIEW_TYPE, EBookViewClass))
#define E_IS_BOOK_VIEW(o)       (GTK_CHECK_TYPE ((o), E_BOOK_VIEW_TYPE))
#define E_IS_BOOK_VIEW_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_BOOK_VIEW_TYPE))

END_GNOME_DECLS

#endif /* ! __E_BOOK_VIEW_H__ */
