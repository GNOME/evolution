/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * A client-side GObject which exposes the
 * Evolution:BookListener interface.
 *
 * Author:
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright 2000, Ximian, Inc.
 */

#ifndef __E_BOOK_LISTENER_H__
#define __E_BOOK_LISTENER_H__

#include <bonobo/bonobo-object.h>
#include <ebook/addressbook.h>
#include <ebook/e-book-types.h>
#include <e-util/e-list.h>

#define E_TYPE_BOOK_LISTENER        (e_book_listener_get_type ())
#define E_BOOK_LISTENER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_BOOK_LISTENER, EBookListener))
#define E_BOOK_LISTENER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TYPE_BOOK_LISTENER, EBookListenerClass))
#define E_IS_BOOK_LISTENER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_BOOK_LISTENER))
#define E_IS_BOOK_LISTENER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_BOOK_LISTENER))
#define E_BOOK_LISTENER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_BOOK_LISTENER, EBookListenerClass))

G_BEGIN_DECLS

typedef struct _EBookListener EBookListener;
typedef struct _EBookListenerClass EBookListenerClass;
typedef struct _EBookListenerPrivate EBookListenerPrivate;

struct _EBookListener {
	BonoboObject           parent;
	EBookListenerPrivate *priv;
};

struct _EBookListenerClass {
	BonoboObjectClass parent;

	POA_GNOME_Evolution_Addressbook_BookListener__epv epv;
	/*
	 * Signals
	 */
	void (*responses_queued) (void);
};

typedef enum {
	/* Async responses */
	OpenBookResponse,
	CreateCardResponse,
	RemoveCardResponse,
	ModifyCardResponse,
	GetCardResponse,
	GetCursorResponse,
	GetBookViewResponse,
	GetChangesResponse,
	AuthenticationResponse,
	GetSupportedFieldsResponse,
	GetSupportedAuthMethodsResponse,

	/* Async events */
	LinkStatusEvent,
	WritableStatusEvent,
	OpenProgressEvent,
} EBookListenerOperation;

typedef struct {
	EBookListenerOperation  op;

	/* For most Response notifications */
	EBookStatus             status;

	/* For OpenBookResponse */
	GNOME_Evolution_Addressbook_Book          book;

	/* For GetCursorResponse */
	GNOME_Evolution_Addressbook_CardCursor    cursor;

	/* For GetBookViewReponse */
	GNOME_Evolution_Addressbook_BookView      book_view;

	/* For GetSupportedFields/GetSupportedAuthMethods */
	EList                                    *list;

	/* For OpenProgressEvent */
	char                   *msg;
	short                   percent;

	/* For LinkStatusEvent */
	gboolean                connected;

	/* For WritableStatusEvent */
	gboolean                writable;

	/* For Card[Added|Removed|Modified]Event */
	char                   *id;
	char                   *vcard;
} EBookListenerResponse;


EBookListener         *e_book_listener_new            (void);
int                    e_book_listener_check_pending  (EBookListener *listener);
EBookListenerResponse *e_book_listener_pop_response   (EBookListener *listener);
GType                  e_book_listener_get_type       (void);
void                   e_book_listener_stop           (EBookListener *listener);

G_END_DECLS

#endif /* ! __E_BOOK_LISTENER_H__ */
