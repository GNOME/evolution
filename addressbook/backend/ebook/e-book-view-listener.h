/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * A client-side GObject which exposes the
 * Evolution:BookViewListener interface.
 *
 * Author:
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright 2000, Ximian, Inc.
 */

#ifndef __E_BOOK_VIEW_LISTENER_H__
#define __E_BOOK_VIEW_LISTENER_H__

#include <bonobo/bonobo-object.h>
#include <ebook/e-book-types.h>
#include <ebook/addressbook.h>

#define E_TYPE_BOOK_VIEW_LISTENER           (e_book_view_listener_get_type ())
#define E_BOOK_VIEW_LISTENER(o)             (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_BOOK_VIEW_LISTENER, EBookViewListener))
#define E_BOOK_VIEW_LISTENER_CLASS(k)       (G_TYPE_CHECK_CLASS_CAST((k), E_TYPE_BOOK_VIEW_LISTENER, EBookViewListenerClass))
#define E_IS_BOOK_VIEW_LISTENER(o)          (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_BOOK_VIEW_LISTENER))
#define E_IS_BOOK_VIEW_LISTENER_CLASS(k)    (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_BOOK_VIEW_LISTENER))
#define E_BOOK_VIEW_LISTENER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_BOOK_VIEW_LISTENER, EBookViewListenerClass))

G_BEGIN_DECLS

typedef struct _EBookViewListener EBookViewListener;
typedef struct _EBookViewListenerClass EBookViewListenerClass;
typedef struct _EBookViewListenerPrivate EBookViewListenerPrivate;

typedef struct _EBookViewListenerResponse EBookViewListenerResponse;

struct _EBookViewListener {
	BonoboObject           parent;
	EBookViewListenerPrivate *priv;
};

struct _EBookViewListenerClass {
	BonoboObjectClass parent;

	POA_GNOME_Evolution_Addressbook_BookViewListener__epv epv;

	/*
	 * Signals
	 */
	void (*response) (EBookViewListener *listener, EBookViewListenerResponse *response);


	/* Padding for future expansion */
	void (*_ebook_reserved0) (void);
	void (*_ebook_reserved1) (void);
	void (*_ebook_reserved2) (void);
	void (*_ebook_reserved3) (void);
	void (*_ebook_reserved4) (void);
};

typedef enum {
	/* Async events */
	ContactsAddedEvent,
	ContactsRemovedEvent,
	ContactsModifiedEvent,
	SequenceCompleteEvent,
	StatusMessageEvent,
} EBookViewListenerOperation;

struct _EBookViewListenerResponse {
	EBookViewListenerOperation  op;

	/* For SequenceComplete */
	EBookViewStatus             status;

	/* For ContactsRemovedEvent */
	GList                  *ids;

	/* For Contact[sAdded|Modified]Event */
	GList                  *contacts; /* Of type EContact. */

	/* For StatusMessageEvent */
	char                   *message;
	
};

EBookViewListener         *e_book_view_listener_new            (void);
GType                      e_book_view_listener_get_type       (void);
void                       e_book_view_listener_stop           (EBookViewListener *listener);

G_END_DECLS

#endif /* ! __E_BOOK_VIEW_LISTENER_H__ */
