/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * A client-side GtkObject which exposes the
 * Evolution:BookViewListener interface.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#ifndef __E_BOOK_VIEW_LISTENER_H__
#define __E_BOOK_VIEW_LISTENER_H__

#include <libgnome/gnome-defs.h>
#include <bonobo/bonobo-object.h>
#include <addressbook/backend/ebook/addressbook.h>

BEGIN_GNOME_DECLS

typedef struct _EBookViewListener EBookViewListener;
typedef struct _EBookViewListenerClass EBookViewListenerClass;
typedef struct _EBookViewListenerPrivate EBookViewListenerPrivate;

struct _EBookViewListener {
	BonoboObject           parent;
	EBookViewListenerPrivate *priv;
};

struct _EBookViewListenerClass {
	BonoboObjectClass parent;

	/*
	 * Signals
	 */
	void (*responses_queued) (void);
};

typedef enum {
	/* Async events */
	CardAddedEvent,
	CardRemovedEvent,
	CardModifiedEvent,
	SequenceCompleteEvent,
} EBookViewListenerOperation;

typedef struct {
	EBookViewListenerOperation  op;

	/* For CardRemovedEvent */
	char                   *id;

	/* For Card[Added|Modified]Event */
	GList                  *cards; /* Of type ECard. */
	
} EBookViewListenerResponse;

EBookViewListener         *e_book_view_listener_new            (void);
int                        e_book_view_listener_check_pending  (EBookViewListener *listener);
EBookViewListenerResponse *e_book_view_listener_pop_response   (EBookViewListener *listener);
GtkType                    e_book_view_listener_get_type       (void);

POA_Evolution_BookViewListener__epv *e_book_view_listener_get_epv (void);

#define E_BOOK_VIEW_LISTENER_TYPE        (e_book_view_listener_get_type ())
#define E_BOOK_VIEW_LISTENER(o)          (GTK_CHECK_CAST ((o), E_BOOK_VIEW_LISTENER_TYPE, EBookViewListener))
#define E_BOOK_VIEW_LISTENER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_BOOK_VIEW_LISTENER_TYPE, EBookViewListenerClass))
#define E_IS_BOOK_VIEW_LISTENER(o)       (GTK_CHECK_TYPE ((o), E_BOOK_VIEW_LISTENER_TYPE))
#define E_IS_BOOK_VIEW_LISTENER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_BOOK_VIEW_LISTENER_TYPE))

END_GNOME_DECLS

#endif /* ! __E_BOOK_VIEW_LISTENER_H__ */
