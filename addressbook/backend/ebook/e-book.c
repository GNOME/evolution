/*
 * The Evolution addressbook client object.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 1999, 2000, Helix Code, Inc.
 */

#include <addressbook.h>
#include <e-card-cursor.h>
#include <e-book-listener.h>
#include <e-book.h>

#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>

GnomeObjectClass *e_book_parent_class;

#define CARDSERVER_GOAD_ID "FIXME"

struct _EBookPrivate {
	Evolution_BookFactory  book_factory;
	EBookListener	      *listener;

	gboolean               operation_pending;

	EBookCallback          open_response;
	gpointer               closure;
};

enum {
	CARD_CHANGED,
	CARD_REMOVED,
	CARD_ADDED,
	LINK_STATUS,
	LAST_SIGNAL
};

static guint e_book_signals [LAST_SIGNAL];

static EBook *
e_book_construct (EBook                     *book,
		  const char                *uri,
		  EBookOpenProgressCallback  progress_cb,
		  EBookCallback              open_response,
		  gpointer                   closure)
{
	CORBA_Environment ev;

	g_return_val_if_fail (book != NULL, NULL);
	g_assert             (uri != NULL);

	/*
	 * Connect to the Personal Addressbook Server.
	 */
	book->priv->book_factory = (Evolution_BookFactory)
		goad_server_activate_with_id (NULL, CARDSERVER_GOAD_ID, 0, NULL);

	if (book->priv->book_factory == CORBA_OBJECT_NIL) {
		g_warning ("e_book_construct: Could not obtain a handle "
			   "to the Personal Addressbook Server!\n");
		return NULL;
	}

	/*
	 * Create our local BookListener interface.
	 */
	book->priv->listener = e_book_listener_new (book);
	if (book->priv->listener == NULL) {
		g_warning ("e_book_construct: Could not create EBookListener!\n");
		return NULL;
	}

	/*
	 * Setup the callback for getting book-opening progress
	 * notifications.
	 */
	book->priv->listener->open_progress = progress_cb;
	book->priv->listener->closure       = closure;
	book->priv->open_response           = open_response;
	book->priv->closure                 = closure;
	book->priv->operation_pending       = TRUE;

	/*
	 * Load the addressbook into the PAS.
	 */
	CORBA_exception_init (&ev);

	Evolution_BookFactory_open_book (
		book->priv->book_factory, uri,
		gnome_object_corba_objref (GNOME_OBJECT (book->priv->listener)),
		&ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_construct: CORBA exception while opening addressbook!\n");
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);
}

/**
 * e_book_new:
 */
EBook *
e_book_new (const char                *uri,
	    EBookOpenProgressCallback  progress_cb,
	    EBookCallback              open_response,
	    gpointer                   closure)
{
	EBook *book;
	EBook *retval;

	g_return_val_if_fail (uri != NULL, NULL);

	book = gtk_type_new (E_BOOK_TYPE);

	retval = e_book_construct (book, uri, progress_cb,
				   open_response, closure);

	if (retval == NULL) {
		g_warning ("e_book_new: Could not construct EBook!\n");
		gtk_object_unref (GTK_OBJECT (book));

		return NULL;
	}

	return retval;
}

static void
e_book_init (EBook *book)
{
	book->priv = g_new0 (EBookPrivate, 1);
}

static void
e_book_destroy (GtkObject *object)
{
	EBook             *book = E_BOOK (object);
	CORBA_Environment  ev;

	gtk_object_unref (GTK_OBJECT (book->priv->listener));

	CORBA_exception_init (&ev);

	CORBA_Object_release (book->priv->book_factory, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("EBook: Exception while releasing BookFactory\n");

		CORBA_exception_free (&ev);
		CORBA_exception_init (&ev);
	}

	g_free (book->priv);

	GTK_OBJECT_CLASS (e_book_parent_class)->destroy (object);
}

static void
e_book_class_init (EBookClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	e_book_parent_class = gtk_type_class (gtk_object_get_type ());

	e_book_signals [CARD_CHANGED] =
		gtk_signal_new ("card_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EBookClass, card_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	e_book_signals [CARD_ADDED] =
		gtk_signal_new ("card_added",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EBookClass, card_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	e_book_signals [CARD_REMOVED] =
		gtk_signal_new ("card_removed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EBookClass, card_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	e_book_signals [LINK_STATUS] =
		gtk_signal_new ("link_status",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EBookClass, link_status),
				gtk_marshal_NONE__BOOL,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_BOOL);

	gtk_object_class_add_signals (object_class, e_book_signals,
				      LAST_SIGNAL);

	object_class->destroy = e_book_destroy;
}

/* Fetching cards */
ECard *
e_book_get_card (EBook *book,
		 char  *id)
{
	g_return_val_if_fail (book != NULL,     NULL);
	g_return_val_if_fail (E_IS_BOOK (book), NULL);

	if (book->priv->operation_pending) {
		g_warning ("EBook: Operation attempted on busy EBook!\n");
		return;
	}
}

char *
e_book_get_vcard (EBook *book,
		  char  *id)
{
}

ECardCursor *
e_book_query (EBook *book,
	      char  *query)
{
}

/* Deleting cards. */
void
e_book_remove_card (EBook         *book,
		    ECard         *card,
		    EBookCallback  cb,
		    gpointer       closure)
{
}

void
e_book_remove_card_by_id (EBook         *book,
			  char          *id,
			  EBookCallback  cb,
			  gpointer       closure)

{
}

/* Adding cards. */
void
e_book_add_card (EBook         *book,
		 ECard         *card,
		 EBookCallback  cb,
		 gpointer       closure)
{
}

void
e_book_add_vcard (EBook         *book,
		  char          *vcard,
		  char          *id,
		  EBookCallback  cb,
		  gpointer       closure)
{
}


/* Modifying cards. */
void
e_book_commit_card (EBook         *book,
		    ECard         *card,
		    EBookCallback  cb,
		    gpointer       closure)
{
}

void
e_book_commit_vcard (EBook         *book,
		     char          *vcard,
		     EBookCallback  cb,
		     gpointer       closure)
{
}


/**
 * e_book_get_type:
 */
GtkType
e_book_get_type (void)
{
	static GtkType type = 0;

	if (! type) {
		GtkTypeInfo info = {
			"EBook",
			sizeof (EBook),
			sizeof (EBookClass),
			(GtkClassInitFunc)  e_book_class_init,
			(GtkObjectInitFunc) e_book_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gtk_object_get_type (), &info);
	}

	return type;
}
