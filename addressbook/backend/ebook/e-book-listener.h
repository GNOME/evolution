/*
 * A client-side GtkObject which exposes the
 * Evolution:BookListener interface.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#ifndef __E_BOOK_LISTENER_H__
#define __E_BOOK_LISTENER_H__

#include <libgnome/gnome-defs.h>
#include <bonobo/gnome-object.h>
#include <e-book.h>
#include <addressbook.h>

BEGIN_GNOME_DECLS

typedef struct _EBookListenerPrivate EBookListenerPrivate;

typedef struct {
	GnomeObject           parent;
	EBookListenerPrivate *priv;
} EBookListener;

typedef struct {
	GnomeObjectClass parent;

	/*
	 * Signals
	 */
	void (*responses_queued) (void);
} EBookListenerClass;

typedef enum {
	/* Async responses */
	OpenBookResponse,
	CreateCardResponse,
	RemoveCardResponse,
	ModifyCardResponse,

	/* Async events */
	CardAddedEvent,
	CardRemovedEvent,
	CardModifiedEvent,
	LinkStatusEvent,
	OpenProgressEvent,
} EBookListenerOperation;

typedef struct {
	EBookListenerOperation  op;

	/* For most Response notifications */
	EBookStatus             status;

	/* For OpenBookResponse */
	Evolution_Book          book;

	/* For OpenProgressEvent */
	char                   *msg;
	short                   percent;

	/* For LinkStatusEvent */
	gboolean                connected;

	/* For Card[Added|Removed|Modified]Event */
	char                   *id;
} EBookListenerResponse;

EBookListener         *e_book_listener_new            (EBook         *book);
EBook                 *e_book_listener_get_book       (EBookListener *listener);
int                    e_book_listener_check_pending  (EBookListener *listener);
EBookListenerResponse *e_book_listener_pop_response   (EBookListener *listener);
GtkType                e_book_listener_get_type       (void);

POA_Evolution_BookListener__epv *e_book_listener_get_epv (void);

#define E_BOOK_LISTENER_TYPE        (e_book_listener_get_type ())
#define E_BOOK_LISTENER(o)          (GTK_CHECK_CAST ((o), E_BOOK_LISTENER_TYPE, EBookListener))
#define E_BOOK_LISTENER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_BOOK_LISTENER_TYPE, EBookListenerClass))
#define E_IS_BOOK_LISTENER(o)       (GTK_CHECK_TYPE ((o), E_BOOK_LISTENER_TYPE))
#define E_IS_BOOK_LISTENER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_BOOK_LISTENER_TYPE))

END_GNOME_DECLS

#endif /* ! __E_BOOK_LISTENER_H__ */
