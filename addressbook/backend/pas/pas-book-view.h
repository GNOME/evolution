/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * A wrapper object which exports the GNOME_Evolution_Addressbook_Book CORBA interface
 * and which maintains a request queue.
 *
 * Author:
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright 2000, Ximian, Inc.
 */

#ifndef __PAS_BOOK_VIEW_H__
#define __PAS_BOOK_VIEW_H__

#include <bonobo/bonobo-object.h>
#include <pas/addressbook.h>
#include <glib.h>
#include <glib-object.h>

#define PAS_TYPE_BOOK_VIEW        (pas_book_view_get_type ())
#define PAS_BOOK_VIEW(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), PAS_TYPE_BOOK_VIEW, PASBookView))
#define PAS_BOOK_VIEW_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), PAS_TYPE_BOOK_VIEW, PASBookViewClass))
#define PAS_IS_BOOK_VIEW(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), PAS_TYPE_BOOK_VIEW))
#define PAS_IS_BOOK_VIEW_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), PAS_TYPE_BOOK_VIEW))
#define PAS_BOOK_VIEW_GET_CLASS(k) (G_TYPE_INSTANCE_GET_CLASS ((obj), PAS_TYPE_BOOK_VIEW, PASBookView))

typedef struct _PASBookView        PASBookView;
typedef struct _PASBookViewClass   PASBookViewClass;
typedef struct _PASBookViewPrivate PASBookViewPrivate;

struct _PASBookView {
	BonoboObject     parent_object;
	PASBookViewPrivate *priv;
};

struct _PASBookViewClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Addressbook_BookView__epv epv;
};


PASBookView *pas_book_view_new                    (GNOME_Evolution_Addressbook_BookViewListener  listener);

void         pas_book_view_notify_change          (PASBookView                *book_view,
						   const GList                *cards);
void         pas_book_view_notify_change_1        (PASBookView                *book_view,
						   const char                 *card);
void         pas_book_view_notify_remove          (PASBookView                *book_view,
						   const GList                *ids);
void         pas_book_view_notify_remove_1        (PASBookView                *book_view,
						   const char                 *id);
void         pas_book_view_notify_add             (PASBookView                *book_view,
						   const GList                *cards);
void         pas_book_view_notify_add_1           (PASBookView                *book_view,
						   const char                 *card);
void         pas_book_view_notify_complete        (PASBookView                *book_view,
						   GNOME_Evolution_Addressbook_BookViewListener_CallStatus);
void         pas_book_view_notify_status_message  (PASBookView                *book_view,
						   const char                 *message);

GType      pas_book_view_get_type               (void);

#endif /* ! __PAS_BOOK_VIEW_H__ */
