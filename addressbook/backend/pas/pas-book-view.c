/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * pas-book-view.c
 *
 * Copyright 2000, Ximian, Inc.
 */

#include <config.h>
#include <glib.h>
#include <bonobo/bonobo-main.h>
#include "pas-book-view.h"

static BonoboObjectClass *pas_book_view_parent_class;

struct _PASBookViewPrivate {
	GNOME_Evolution_Addressbook_BookViewListener  listener;
};

/**
 * pas_book_view_notify_change:
 */
void
pas_book_view_notify_change (PASBookView                *book_view,
			     const GList                *cards)
{
	CORBA_Environment ev;
	gint i, length;
	CORBA_sequence_GNOME_Evolution_Addressbook_VCard card_sequence;

	length = g_list_length((GList *) cards);

	card_sequence._buffer = CORBA_sequence_GNOME_Evolution_Addressbook_VCard_allocbuf(length);
	card_sequence._maximum = length;
	card_sequence._length = length;

	for ( i = 0; cards; cards = g_list_next(cards), i++ ) {
		card_sequence._buffer[i] = CORBA_string_dup((char *) cards->data);
	}

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_BookViewListener_notifyCardChanged (
		book_view->priv->listener, &card_sequence, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_view_notify_change: Exception signaling BookViewListener!\n");
	}

	CORBA_exception_free (&ev);

	CORBA_free(card_sequence._buffer);
}

void
pas_book_view_notify_change_1 (PASBookView *book_view,
			       const char  *card)
{
	GList *list = g_list_append(NULL, (char *) card);
	pas_book_view_notify_change(book_view, list);
	g_list_free(list);
}

/**
 * pas_book_view_notify_remove:
 */
void
pas_book_view_notify_remove (PASBookView                *book_view,
			     const char                 *id)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_BookViewListener_notifyCardRemoved (
		book_view->priv->listener, (GNOME_Evolution_Addressbook_CardId) id, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_view_notify_remove: Exception signaling BookViewListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_view_notify_add:
 */
void
pas_book_view_notify_add (PASBookView                *book_view,
			  const GList                *cards)
{
	CORBA_Environment ev;
	gint i, length;
	CORBA_sequence_GNOME_Evolution_Addressbook_VCard card_sequence;

	length = g_list_length((GList *)cards);

	card_sequence._buffer = CORBA_sequence_GNOME_Evolution_Addressbook_VCard_allocbuf(length);
	card_sequence._maximum = length;
	card_sequence._length = length;

	for ( i = 0; cards; cards = g_list_next(cards), i++ ) {
		card_sequence._buffer[i] = CORBA_string_dup((char *) cards->data);
	}

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_BookViewListener_notifyCardAdded (
		book_view->priv->listener, &card_sequence, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_view_notify_add: Exception signaling BookViewListener!\n");
	}

	CORBA_exception_free (&ev);

	CORBA_free(card_sequence._buffer);
}

void
pas_book_view_notify_add_1 (PASBookView *book_view,
			    const char  *card)
{
	GList *list = g_list_append(NULL, (char *) card);
	pas_book_view_notify_add(book_view, list);
	g_list_free(list);
}

void
pas_book_view_notify_complete (PASBookView *book_view,
			       GNOME_Evolution_Addressbook_BookViewListener_CallStatus status)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_BookViewListener_notifySequenceComplete (
		book_view->priv->listener, status, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_view_notify_complete: Exception signaling BookViewListener!\n");
	}

	CORBA_exception_free (&ev);
}

void
pas_book_view_notify_status_message (PASBookView *book_view,
				     const char  *message)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_BookViewListener_notifyStatusMessage (
		book_view->priv->listener, message, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_view_notify_status_message: Exception signaling BookViewListener!\n");
	}

	CORBA_exception_free (&ev);
}

static void
pas_book_view_construct (PASBookView                *book_view,
			 GNOME_Evolution_Addressbook_BookViewListener  listener)
{
	PASBookViewPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (book_view != NULL);
	g_return_if_fail (listener != CORBA_OBJECT_NIL);

	priv = book_view->priv;

	CORBA_exception_init (&ev);

	bonobo_object_dup_ref (listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning("Unable to duplicate & ref listener object in pas-book-view.c\n");
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);

	priv->listener  = listener;
}

/**
 * pas_book_view_new:
 */
PASBookView *
pas_book_view_new (GNOME_Evolution_Addressbook_BookViewListener  listener)
{
	PASBookView *book_view;
	GNOME_Evolution_Addressbook_BookView corba_objref;

	book_view = g_object_new (PAS_TYPE_BOOK_VIEW, NULL);
	
	pas_book_view_construct (book_view, listener);

	return book_view;
}

static void
pas_book_view_dispose (GObject *object)
{
	PASBookView *book_view = PAS_BOOK_VIEW (object);

	if (book_view->priv) {
		bonobo_object_release_unref (book_view->priv->listener, NULL);

		g_free (book_view->priv);
		book_view->priv = NULL;
	}

	if (G_OBJECT_CLASS (pas_book_view_parent_class)->dispose)
		G_OBJECT_CLASS (pas_book_view_parent_class)->dispose (object);	
}

static void
pas_book_view_class_init (PASBookViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	pas_book_view_parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = pas_book_view_dispose;
}

static void
pas_book_view_init (PASBookView *book_view)
{
	book_view->priv           = g_new0 (PASBookViewPrivate, 1);
	book_view->priv->listener = CORBA_OBJECT_NIL;
}

BONOBO_TYPE_FUNC_FULL (
		       PASBookView,
		       GNOME_Evolution_Addressbook_BookView,
		       BONOBO_TYPE_OBJECT,
		       pas_book_view);
