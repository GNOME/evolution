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
POA_GNOME_Evolution_Addressbook_BookView__vepv pas_book_view_vepv;

struct _PASBookViewPrivate {
	PASBookViewServant *servant;
	GNOME_Evolution_Addressbook_BookView corba_objref;

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

void
pas_book_view_construct (PASBookView                *book_view,
			 GNOME_Evolution_Addressbook_BookView corba_objref,
			 GNOME_Evolution_Addressbook_BookViewListener  listener)
{
	PASBookViewPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (book_view != NULL);
	g_return_if_fail (corba_objref != CORBA_OBJECT_NIL);
	g_return_if_fail (listener != CORBA_OBJECT_NIL);

	priv = book_view->priv;

	g_return_if_fail (priv->corba_objref == CORBA_OBJECT_NIL);

	priv->corba_objref = corba_objref;
	
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

static PASBookViewServant *
create_servant (PASBookView *factory)
{
	PASBookViewServant *servant;
	POA_GNOME_Evolution_Addressbook_BookView *corba_servant;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	servant = g_new0 (PASBookViewServant, 1);
	corba_servant = (POA_GNOME_Evolution_Addressbook_BookView *) servant;

	corba_servant->vepv = &pas_book_view_vepv;
	POA_GNOME_Evolution_Addressbook_BookView__init ((PortableServer_Servant) corba_servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	servant->object = factory;

	CORBA_exception_free (&ev);

	return servant;
}

static GNOME_Evolution_Addressbook_BookView
activate_servant (PASBookView *factory,
		  POA_GNOME_Evolution_Addressbook_BookView *servant)
{
	GNOME_Evolution_Addressbook_BookView corba_object;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	CORBA_free (PortableServer_POA_activate_object (bonobo_poa (), servant, &ev));

	corba_object = PortableServer_POA_servant_to_reference (bonobo_poa(), servant, &ev);

	if (ev._major == CORBA_NO_EXCEPTION && ! CORBA_Object_is_nil (corba_object, &ev)) {
		CORBA_exception_free (&ev);
		return corba_object;
	}

	CORBA_exception_free (&ev);

	return CORBA_OBJECT_NIL;
}

/**
 * pas_book_view_new:
 */
PASBookView *
pas_book_view_new (GNOME_Evolution_Addressbook_BookViewListener  listener)
{
	PASBookView *book_view;
	PASBookViewPrivate *priv;
	GNOME_Evolution_Addressbook_BookView corba_objref;

	book_view = g_object_new (PAS_TYPE_BOOK_VIEW, NULL);
	priv = book_view->priv;

	priv->servant = create_servant (book_view);
	corba_objref = activate_servant (book_view, (POA_GNOME_Evolution_Addressbook_BookView*)priv->servant);
	
	pas_book_view_construct (book_view, corba_objref, listener);

	return book_view;
}

static void
pas_book_view_dispose (GObject *object)
{
	PASBookView *book_view = PAS_BOOK_VIEW (object);
	CORBA_Environment   ev;

	CORBA_exception_init (&ev);
	bonobo_object_release_unref (book_view->priv->listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);

		return;
	}
	CORBA_exception_free (&ev);

	g_free (book_view->priv);

	G_OBJECT_CLASS (pas_book_view_parent_class)->dispose (object);	
}

static void
corba_class_init (PASBookViewClass *klass)
{
	POA_GNOME_Evolution_Addressbook_BookView__vepv *vepv;
	POA_GNOME_Evolution_Addressbook_BookView__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;


	epv = &klass->epv;

	vepv = &pas_book_view_vepv;
	vepv->_base_epv                                   = base_epv;
	vepv->GNOME_Evolution_Addressbook_BookView_epv = epv;
}

static void
pas_book_view_class_init (PASBookViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	pas_book_view_parent_class = g_object_new (BONOBO_TYPE_OBJECT, NULL);

	object_class->dispose = pas_book_view_dispose;

	corba_class_init (klass);
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
