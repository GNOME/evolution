/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * A wrapper object which exports the Evolution_Book CORBA interface
 * and which maintains a request queue.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#ifndef __PAS_BOOK_VIEW_H__
#define __PAS_BOOK_VIEW_H__

#include <bonobo/bonobo-object.h>
#include <libgnome/gnome-defs.h>
#include <pas/addressbook.h>

typedef struct _PASBookView        PASBookView;
typedef struct _PASBookViewClass   PASBookViewClass;
typedef struct _PASBookViewPrivate PASBookViewPrivate;

struct _PASBookView {
	BonoboObject     parent_object;
	PASBookViewPrivate *priv;
};

struct _PASBookViewClass {
	BonoboObjectClass parent_class;
};

PASBookView                *pas_book_view_new                (Evolution_BookViewListener             listener);

void                        pas_book_view_notify_change      (PASBookView                           *book_view,
							      const GList                           *cards);
void                        pas_book_view_notify_change_1    (PASBookView                           *book_view,
							      const char                            *card);
void                        pas_book_view_notify_remove      (PASBookView                           *book_view,
							      const char                            *id);
void                        pas_book_view_notify_add         (PASBookView                           *book_view,
							      const GList                           *cards);
void                        pas_book_view_notify_add_1       (PASBookView                           *book_view,
							      const char                            *card);
void                        pas_book_view_notify_complete    (PASBookView                           *book_view);

GtkType                     pas_book_view_get_type           (void);

#define PAS_BOOK_VIEW_TYPE        (pas_book_view_get_type ())
#define PAS_BOOK_VIEW(o)          (GTK_CHECK_CAST ((o), PAS_BOOK_VIEW_TYPE, PASBookView))
#define PAS_BOOK_VIEW_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), PAS_BOOK_VIEW_FACTORY_TYPE, PASBookViewClass))
#define PAS_IS_BOOK_VIEW(o)       (GTK_CHECK_TYPE ((o), PAS_BOOK_VIEW_TYPE))
#define PAS_IS_BOOK_VIEW_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), PAS_BOOK_VIEW_TYPE))

#endif /* ! __PAS_BOOK_VIEW_H__ */
