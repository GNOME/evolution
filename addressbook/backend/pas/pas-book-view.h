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
#include <pas/pas-types.h>
#include <ebook/e-contact.h>

#define PAS_TYPE_BOOK_VIEW        (pas_book_view_get_type ())
#define PAS_BOOK_VIEW(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), PAS_TYPE_BOOK_VIEW, PASBookView))
#define PAS_BOOK_VIEW_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), PAS_TYPE_BOOK_VIEW, PASBookViewClass))
#define PAS_IS_BOOK_VIEW(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), PAS_TYPE_BOOK_VIEW))
#define PAS_IS_BOOK_VIEW_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), PAS_TYPE_BOOK_VIEW))
#define PAS_BOOK_VIEW_GET_CLASS(k) (G_TYPE_INSTANCE_GET_CLASS ((obj), PAS_TYPE_BOOK_VIEW, PASBookView))

typedef struct _PASBookViewPrivate PASBookViewPrivate;

struct _PASBookView {
	BonoboObject     parent_object;
	PASBookViewPrivate *priv;
};

struct _PASBookViewClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Addressbook_BookView__epv epv;
};


PASBookView *pas_book_view_new                    (PASBackend                 *backend,
						   GNOME_Evolution_Addressbook_BookViewListener  listener,
						   const char                 *card_query,
						   PASBackendCardSExp         *card_sexp);

const char*  pas_book_view_get_card_query         (PASBookView                *book_view);
PASBackendCardSExp* pas_book_view_get_card_sexp   (PASBookView                *book_view);
PASBackend*  pas_book_view_get_backend            (PASBookView                *book_view);
GNOME_Evolution_Addressbook_BookViewListener pas_book_view_get_listener (PASBookView  *book_view);

void         pas_book_view_notify_update          (PASBookView                *book_view,
						   EContact                   *contact);
void         pas_book_view_notify_remove          (PASBookView                *book_view,
						   const char                 *id);
void         pas_book_view_notify_complete        (PASBookView                *book_view,
						   GNOME_Evolution_Addressbook_CallStatus);
void         pas_book_view_notify_status_message  (PASBookView                *book_view,
						   const char                 *message);

GType        pas_book_view_get_type               (void);

#endif /* ! __PAS_BOOK_VIEW_H__ */
