/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * The Evolution addressbook client object.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 1999, 2000, Helix Code, Inc.
 */

#include <addressbook.h>
#include <libgnorba/gnorba.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>
#include <e-card-cursor.h>
#include <e-book-view-listener.h>
#include <e-book-view.h>

GtkObjectClass *e_book_view_parent_class;

struct _EBookViewPrivate {
	Evolution_BookView     corba_book_view;
	
	EBookViewListener     *listener;

	int                    responses_queued_id;
};

enum {
	CARD_CHANGED,
	CARD_REMOVED,
	CARD_ADDED,
	LAST_SIGNAL
};

static guint e_book_view_signals [LAST_SIGNAL];

static void
e_book_view_do_added_event (EBookView                 *book_view,
			    EBookViewListenerResponse *resp)
{
	gtk_signal_emit (GTK_OBJECT (book_view), e_book_view_signals [CARD_ADDED],
			 resp->cards);

	g_list_foreach (resp->cards, (GFunc) gtk_object_unref, NULL);
	g_list_free (resp->cards);
}

static void
e_book_view_do_modified_event (EBookView                 *book_view,
			       EBookViewListenerResponse *resp)
{
	gtk_signal_emit (GTK_OBJECT (book_view), e_book_view_signals [CARD_CHANGED],
			 resp->cards);

	g_list_foreach (resp->cards, (GFunc) gtk_object_unref, NULL);
	g_list_free (resp->cards);
}

static void
e_book_view_do_removed_event (EBookView                 *book_view,
			      EBookViewListenerResponse *resp)
{
	gtk_signal_emit (GTK_OBJECT (book_view), e_book_view_signals [CARD_REMOVED],
			 resp->id);

	g_free(resp->id);
}


/*
 * Reading notices out of the EBookViewListener's queue.
 */
static void
e_book_view_check_listener_queue (EBookViewListener *listener, EBookView *book_view)
{
	EBookViewListenerResponse *resp;

	resp = e_book_view_listener_pop_response (listener);

	if (resp == NULL)
		return;

	switch (resp->op) {
	case CardAddedEvent:
		e_book_view_do_added_event (book_view, resp);
		break;
	case CardModifiedEvent:
		e_book_view_do_modified_event (book_view, resp);
		break;
	case CardRemovedEvent:
		e_book_view_do_removed_event (book_view, resp);
		break;
	default:
		g_error ("EBookView: Unknown operation %d in listener queue!\n",
			 resp->op);
	}

	g_free (resp);
}

static gboolean
e_book_view_construct (EBookView *book_view, Evolution_BookView corba_book_view, EBookViewListener *listener)
{
	CORBA_Environment  ev;
	g_return_val_if_fail (book_view != NULL,     FALSE);
	g_return_val_if_fail (E_IS_BOOK_VIEW (book_view), FALSE);

	/*
	 * Copy in the corba_book_view.
	 */
	CORBA_exception_init (&ev);

	book_view->priv->corba_book_view = CORBA_Object_duplicate(corba_book_view, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_view_construct: Exception duplicating corba_book_view.\n");
		CORBA_exception_free (&ev);
		return FALSE;
	}

	Evolution_BookView_ref(book_view->priv->corba_book_view, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_view_construct: Exception reffing corba_book_view.\n");
		CORBA_exception_free (&ev);
		CORBA_exception_init (&ev);
	        CORBA_Object_release (book_view->priv->corba_book_view, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("e_book_view_construct: Exception releasing corba_book_view.\n");
		}
		CORBA_exception_free (&ev);
		book_view->priv->corba_book_view = NULL;
		return FALSE;
	}

	CORBA_exception_free (&ev);

	/*
	 * Create our local BookListener interface.
	 */
	book_view->priv->listener = listener;

	gtk_object_ref(GTK_OBJECT(book_view->priv->listener));
	book_view->priv->responses_queued_id = gtk_signal_connect (GTK_OBJECT (book_view->priv->listener), "responses_queued",
								   e_book_view_check_listener_queue, book_view);

	return TRUE;
}

/**
 * e_book_view_new:
 */
EBookView *
e_book_view_new (Evolution_BookView corba_book_view, EBookViewListener *listener)
{
	EBookView *book_view;

	book_view = gtk_type_new (E_BOOK_VIEW_TYPE);

	if (! e_book_view_construct (book_view, corba_book_view, listener)) {
		gtk_object_unref (GTK_OBJECT (book_view));
		return NULL;
	}

	return book_view;
}

static void
e_book_view_init (EBookView *book_view)
{
	book_view->priv                      = g_new0 (EBookViewPrivate, 1);
	book_view->priv->corba_book_view     = CORBA_OBJECT_NIL;
	book_view->priv->listener            = NULL;
	book_view->priv->responses_queued_id = 0;
}

static void
e_book_view_destroy (GtkObject *object)
{
	EBookView             *book_view = E_BOOK_VIEW (object);
	CORBA_Environment  ev;

	if (book_view->priv->corba_book_view) {
		CORBA_exception_init (&ev);

		Evolution_BookView_unref(book_view->priv->corba_book_view, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("EBookView: Exception while unreffing BookView\n");
			
			CORBA_exception_free (&ev);
			CORBA_exception_init (&ev);
		}

		CORBA_Object_release (book_view->priv->corba_book_view, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("EBookView: Exception while releasing BookView\n");
		}

		CORBA_exception_free (&ev);
	}

	if (book_view->priv->listener) {
		if (book_view->priv->responses_queued_id)
			gtk_signal_disconnect(GTK_OBJECT(book_view->priv->listener),
					      book_view->priv->responses_queued_id);
		gtk_object_unref (GTK_OBJECT(book_view->priv->listener));
	}

	g_free (book_view->priv);

	if (GTK_OBJECT_CLASS (e_book_view_parent_class)->destroy)
		GTK_OBJECT_CLASS (e_book_view_parent_class)->destroy (object);
}

static void
e_book_view_class_init (EBookViewClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	e_book_view_parent_class = gtk_type_class (gtk_object_get_type ());

	e_book_view_signals [CARD_CHANGED] =
		gtk_signal_new ("card_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EBookViewClass, card_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	e_book_view_signals [CARD_ADDED] =
		gtk_signal_new ("card_added",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EBookViewClass, card_added),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);

	e_book_view_signals [CARD_REMOVED] =
		gtk_signal_new ("card_removed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EBookViewClass, card_removed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, e_book_view_signals,
				      LAST_SIGNAL);

	object_class->destroy = e_book_view_destroy;
}

/**
 * e_book_view_get_type:
 */
GtkType
e_book_view_get_type (void)
{
	static GtkType type = 0;

	if (! type) {
		GtkTypeInfo info = {
			"EBookView",
			sizeof (EBookView),
			sizeof (EBookViewClass),
			(GtkClassInitFunc)  e_book_view_class_init,
			(GtkObjectInitFunc) e_book_view_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gtk_object_get_type (), &info);
	}

	return type;
}
