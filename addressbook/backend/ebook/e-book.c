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
#include <e-book-listener.h>
#include <e-book.h>

GtkObjectClass *e_book_parent_class;

#define CARDSERVER_GOAD_ID "evolution:card-server"

typedef enum {
	URINotLoaded,
	URILoading,
	URILoaded
} EBookLoadState;

struct _EBookPrivate {
	Evolution_BookFactory  book_factory;
	EBookListener	      *listener;

	Evolution_Book         corba_book;

	EBookLoadState         load_state;

	/*
	 * The operation queue.  New operations are appended to the
	 * end of the queue.  When responses come back from the PAS,
	 * the op structures are popped off the front of the queue.
	 */
	GList                 *pending_ops;
};

enum {
	OPEN_PROGRESS,
	CARD_CHANGED,
	CARD_REMOVED,
	CARD_ADDED,
	LINK_STATUS,
	LAST_SIGNAL
};

static guint e_book_signals [LAST_SIGNAL];

typedef struct {
	gpointer  cb;
	gpointer  closure;
} EBookOp;

/*
 * Local response queue management.
 */
static void
e_book_queue_op (EBook    *book,
		 gpointer  cb,
		 gpointer  closure)
{
	EBookOp *op;

	op          = g_new0 (EBookOp, 1);
	op->cb      = cb;
	op->closure = closure;

	book->priv->pending_ops =
		g_list_append (book->priv->pending_ops, op);
}

static EBookOp *
e_book_pop_op (EBook *book)
{
	GList   *popped;
	EBookOp *op;

	if (book->priv->pending_ops == NULL)
		return NULL;

	op = book->priv->pending_ops->data;

	popped = book->priv->pending_ops;
	book->priv->pending_ops =
		g_list_remove_link (book->priv->pending_ops, (gpointer) op);

	g_list_free_1 (popped);

	return op;
}

static void
e_book_do_response_generic (EBook                 *book,
			    EBookListenerResponse *resp)
{
	EBookOp *op;

	op = e_book_pop_op (book);

	if (op == NULL) {
		g_warning ("e_book_do_response_generic: Cannot find operation "
			   "in local op queue!\n");
	}

	((EBookCallback) op->cb) (book, resp->status, op->closure);

	g_free (op);
}

static void
e_book_do_response_open (EBook                 *book,
			 EBookListenerResponse *resp)
{
	EBookOp *op;

	if (resp->status == E_BOOK_STATUS_SUCCESS) {
		book->priv->corba_book  = resp->book;
		book->priv->load_state  = URILoaded;
	}

	op = e_book_pop_op (book);

	if (op == NULL) {
		g_warning ("e_book_do_response_open: Cannot find operation "
			   "in local op queue!\n");
		return;
	}

	((EBookCallback) op->cb) (book, resp->status, op->closure);
	g_free (op);
}

static void
e_book_do_progress_event (EBook                 *book,
			  EBookListenerResponse *resp)
{
	gtk_signal_emit (GTK_OBJECT (book), e_book_signals [OPEN_PROGRESS],
			 resp->msg, resp->percent);

	g_free (resp->msg);
}

static void
e_book_do_link_event (EBook                 *book,
		      EBookListenerResponse *resp)
{
	gtk_signal_emit (GTK_OBJECT (book), e_book_signals [LINK_STATUS],
			 resp->connected);
}

static void
e_book_do_added_event (EBook                 *book,
		       EBookListenerResponse *resp)
{
	gtk_signal_emit (GTK_OBJECT (book), e_book_signals [CARD_ADDED],
			 resp->id);

	g_free (resp->id); 
}

static void
e_book_do_modified_event (EBook                 *book,
			  EBookListenerResponse *resp)
{
	gtk_signal_emit (GTK_OBJECT (book), e_book_signals [CARD_CHANGED],
			 resp->id);

	g_free (resp->id); 
}

static void
e_book_do_removed_event (EBook                 *book,
			 EBookListenerResponse *resp)
{
	gtk_signal_emit (GTK_OBJECT (book), e_book_signals [CARD_REMOVED],
			 resp->id);

	g_free (resp->id); 
}


/*
 * Reading notices out of the EBookListener's queue.
 */
static void
e_book_check_listener_queue (EBookListener *listener)
{
	EBook                 *book;
	EBookListenerResponse *resp;
	
	book = e_book_listener_get_book (listener);
	g_assert (book != NULL);

	resp = e_book_listener_pop_response (listener);

	if (resp == NULL)
		return;

	switch (resp->op) {
	case CreateCardResponse:
	case RemoveCardResponse:
	case ModifyCardResponse:
		e_book_do_response_generic (book, resp);
		break;
	case OpenBookResponse:
		e_book_do_response_open (book, resp);
		break;

	case OpenProgressEvent:
		e_book_do_progress_event (book, resp);
		break;
	case LinkStatusEvent:
		e_book_do_link_event (book, resp);
		break;
	case CardAddedEvent:
		e_book_do_added_event (book, resp);
		break;
	case CardModifiedEvent:
		e_book_do_modified_event (book, resp);
		break;
	case CardRemovedEvent:
		e_book_do_removed_event (book, resp);
		break;
	default:
		g_error ("EBook: Unknown operation %d in listener queue!\n",
			 resp->op);
	}

	g_free (resp);
}

/**
 * e_book_load_uri:
 */
gboolean
e_book_load_uri (EBook                     *book,
		 const char                *uri,
		 EBookCallback              open_response,
		 gpointer                   closure)
{
	CORBA_Environment ev;

	g_return_val_if_fail (book != NULL,          FALSE);
	g_return_val_if_fail (E_IS_BOOK (book),      FALSE);
	g_return_val_if_fail (uri != NULL,           FALSE);
	g_return_val_if_fail (open_response != NULL, FALSE);

	if (book->priv->load_state != URINotLoaded) {
		g_warning ("e_book_load_uri: Attempted to load a URI "
			   "on a book which already has a URI loaded!\n");
		return FALSE;
	}

	/*
	 * Load the addressbook into the PAS.
	 */
	CORBA_exception_init (&ev);

	Evolution_BookFactory_open_book (
		book->priv->book_factory, uri,
		bonobo_object_corba_objref (BONOBO_OBJECT (book->priv->listener)),
		&ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_load_uri: CORBA exception while opening addressbook!\n");
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	book->priv->load_state = URILoading;

	e_book_queue_op (book, open_response, closure);

	/* Now we play the waiting game. */

	return TRUE;
}

/**
 * e_book_unload_uri:
 */
void
e_book_unload_uri (EBook *book)
{
	CORBA_Environment ev;

	g_return_if_fail (book != NULL);
	g_return_if_fail (E_IS_BOOK (book));

	/*
	 * FIXME: Make sure this works if the URI is still being
	 * loaded.
	 */
	if (book->priv->load_state == URINotLoaded) {
		g_warning ("e_book_unload_uri: No URI is loaded!\n");
		return;
	}

	/*
	 * Release the remote Evolution_Book in the PAS.
	 */
	CORBA_exception_init (&ev);

	Bonobo_Unknown_unref  (book->priv->corba_book, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_unload_uri: Exception unref'ing "
			   "remote Evolution_Book interface!\n");
		CORBA_exception_free (&ev);
		CORBA_exception_init (&ev);
	}
	
	CORBA_Object_release (book->priv->corba_book, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_unload_uri: Exception releasing "
			   "remote book interface!\n");
	}

	CORBA_exception_free (&ev);

	gtk_object_unref (GTK_OBJECT (book->priv->listener));

	book->priv->listener   = NULL;
	book->priv->load_state = URINotLoaded;
}

static gboolean
e_book_construct (EBook *book)
{
	g_return_val_if_fail (book != NULL,     FALSE);
	g_return_val_if_fail (E_IS_BOOK (book), FALSE);

	/*
	 * Connect to the Personal Addressbook Server.
	 */
	book->priv->book_factory = (Evolution_BookFactory)
		goad_server_activate_with_id (NULL, CARDSERVER_GOAD_ID, 0, NULL);

	if (book->priv->book_factory == CORBA_OBJECT_NIL) {
		g_warning ("e_book_construct: Could not obtain a handle "
			   "to the Personal Addressbook Server!\n");
		return FALSE;
	}

	/*
	 * Create our local BookListener interface.
	 */
	book->priv->listener = e_book_listener_new (book);
	if (book->priv->listener == NULL) {
		g_warning ("e_book_construct: Could not create EBookListener!\n");
		return FALSE;
	}

	gtk_signal_connect (GTK_OBJECT (book->priv->listener), "responses_queued",
			    e_book_check_listener_queue, NULL);

	return TRUE;
}

/**
 * e_book_new:
 */
EBook *
e_book_new (void)
{
	EBook *book;

	book = gtk_type_new (E_BOOK_TYPE);

	if (! e_book_construct (book)) {
		gtk_object_unref (GTK_OBJECT (book));
		return NULL;
	}

	return book;
}

/* Fetching cards */

/**
 * e_book_get_card:
 */
ECard *
e_book_get_card (EBook       *book,
		 const char  *id)
{
	char  *vcard;
	ECard *card;

	g_return_val_if_fail (book != NULL,     NULL);
	g_return_val_if_fail (E_IS_BOOK (book), NULL);

	if (! book->priv->load_state != URILoaded) {
		g_warning ("e_book_get_card: No URI loaded!\n");
		return NULL;
	}

	vcard = e_book_get_vcard (book, id);

	if (vcard == NULL) {
		g_warning ("e_book_get_card: Got bogus VCard from PAS!\n");
		return NULL;
	}

	card = e_card_new (vcard);
	g_strdup (vcard);

	return card;
}

/**
 * e_book_get_vcard:
 */
char *
e_book_get_vcard (EBook       *book,
		  const char  *id)
{
	CORBA_Environment  ev;
	char              *retval;
	char              *vcard;

	g_return_val_if_fail (book != NULL,     NULL);
	g_return_val_if_fail (E_IS_BOOK (book), NULL);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_get_vcard: No URI loaded!\n");
		return NULL;
	}

	CORBA_exception_init (&ev);

	vcard = Evolution_Book_get_vcard (book->priv->corba_book,
					  (Evolution_CardId) id,
					  &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_get_vcard: Exception getting VCard from PAS!\n");
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	if (vcard == NULL || strlen (vcard) == 0) {
		g_warning ("e_book_get_vcard: Got NULL VCard from PAS!\n");
		return NULL;
	}

	retval = g_strdup (vcard);
	CORBA_free (vcard);

	return retval;
}

/* Deleting cards. */

/**
 * e_book_remove_card:
 */
gboolean
e_book_remove_card (EBook         *book,
		    ECard         *card,
		    EBookCallback  cb,
		    gpointer       closure)
{
	const char *id;

	g_return_val_if_fail (book != NULL,     FALSE);
	g_return_val_if_fail (E_IS_BOOK (book), FALSE);
	g_return_val_if_fail (card != NULL,     FALSE);
	g_return_val_if_fail (E_IS_CARD (card), FALSE);
	g_return_val_if_fail (cb != NULL,       FALSE);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_remove_card: No URI loaded!\n");
		return FALSE;
	}

	id = e_card_get_id (card);
	g_assert (id != NULL);

	return e_book_remove_card_by_id (book, id, cb, closure);
}

/**
 * e_book_remove_card_by_id:
 */
gboolean
e_book_remove_card_by_id (EBook         *book,
			  const char    *id,
			  EBookCallback  cb,
			  gpointer       closure)

{
	CORBA_Environment ev;

	g_return_val_if_fail (book != NULL,     FALSE);
	g_return_val_if_fail (E_IS_BOOK (book), FALSE);
	g_return_val_if_fail (id != NULL,       FALSE);
	g_return_val_if_fail (cb != NULL,       FALSE);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_remove_card_by_id: No URI loaded!\n");
		return FALSE;
	}

	CORBA_exception_init (&ev);

	Evolution_Book_remove_card (
		book->priv->corba_book, (const Evolution_CardId) id, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_remove_card_by_id: CORBA exception "
			   "talking to PAS!\n");
		CORBA_exception_free (&ev);
		return FALSE;
	}
	
	CORBA_exception_free (&ev);

	e_book_queue_op (book, cb, closure);

	return TRUE;
}

/* Adding cards. */

/**
 * e_book_add_card:
 */
gboolean
e_book_add_card (EBook         *book,
		 ECard         *card,
		 EBookCallback  cb,
		 gpointer       closure)

{
	char     *vcard;
	gboolean  retval;

	g_return_val_if_fail (book != NULL,     FALSE);
	g_return_val_if_fail (E_IS_BOOK (book), FALSE);
	g_return_val_if_fail (card != NULL,     FALSE);
	g_return_val_if_fail (E_IS_CARD (card), FALSE);
	g_return_val_if_fail (cb   != NULL,     FALSE);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_add_card: No URI loaded!\n");
		return FALSE;
	}

	vcard = e_card_get_vcard (card);

	if (vcard == NULL) {
		g_warning ("e_book_add_card: Cannot convert card to VCard string!\n");
		return FALSE;
	}

	retval = e_book_add_vcard (book, vcard, cb, closure);

	g_free (vcard);

	return retval;
}

/**
 * e_book_add_vcard:
 */
gboolean
e_book_add_vcard (EBook         *book,
		  const char    *vcard,
		  EBookCallback  cb,
		  gpointer       closure)
{
	CORBA_Environment ev;

	g_return_val_if_fail (book  != NULL,    FALSE);
	g_return_val_if_fail (E_IS_BOOK (book), FALSE);
	g_return_val_if_fail (vcard != NULL,    FALSE);
	g_return_val_if_fail (cb    != NULL,    FALSE);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_add_vcard: No URI loaded!\n");
		return FALSE;
	}

	CORBA_exception_init (&ev);

	Evolution_Book_create_card (
		book->priv->corba_book, vcard, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_add_vcard: Exception adding card to PAS!\n");
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	e_book_queue_op (book, cb, closure);

	return TRUE;
}

/* Modifying cards. */

/**
 * e_book_commit_card:
 */
gboolean
e_book_commit_card (EBook         *book,
		    ECard         *card,
		    EBookCallback  cb,
		    gpointer       closure)
{
	char     *vcard;
	gboolean  retval;
	
	g_return_val_if_fail (book  != NULL,    FALSE);
	g_return_val_if_fail (E_IS_BOOK (book), FALSE);
	g_return_val_if_fail (card != NULL,     FALSE);
	g_return_val_if_fail (E_IS_CARD (card), FALSE);
	g_return_val_if_fail (cb    != NULL,    FALSE);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_commit_card: No URI loaded!\n");
		return FALSE;
	}

	vcard = e_card_get_vcard (card);

	if (vcard == NULL) {
		g_warning ("e_book_commit_card: Error "
			   "getting VCard for card!\n");
		return FALSE;
	}

	retval = e_book_commit_vcard (book, vcard, cb, closure);

	g_free (vcard);

	return retval;
}

/**
 * e_book_commit_vcard:
 */
gboolean
e_book_commit_vcard (EBook         *book,
		     const char    *vcard,
		     EBookCallback  cb,
		     gpointer       closure)
{
	CORBA_Environment ev;

	g_return_val_if_fail (book  != NULL,    FALSE);
	g_return_val_if_fail (E_IS_BOOK (book), FALSE);
	g_return_val_if_fail (vcard != NULL,    FALSE);
	g_return_val_if_fail (cb    != NULL,    FALSE);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_commit_vcard: No URI loaded!\n");
		return FALSE;
	}

	CORBA_exception_init (&ev);

	Evolution_Book_modify_card (
		book->priv->corba_book, vcard, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_commit_vcard: Exception "
			   "modifying card in PAS!\n");
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	e_book_queue_op (book, cb, closure);

	return TRUE;
}

/**
 * e_book_check_connection:
 */
gboolean
e_book_check_connection (EBook *book)
{
	CORBA_Environment ev;

	g_return_val_if_fail (book != NULL,     FALSE);
	g_return_val_if_fail (E_IS_BOOK (book), FALSE);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_check_connection: No URI loaded!\n");
		return FALSE;
	}

	CORBA_exception_init (&ev);

	Evolution_Book_check_connection (book->priv->corba_book, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_check_connection: Exception "
			   "querying the PAS!\n");
		CORBA_exception_free (&ev);
		return FALSE;
	}
	
	CORBA_exception_free (&ev);

	return TRUE;
}

/**
 * e_book_get_name:
 */
char *
e_book_get_name (EBook *book)
{
	CORBA_Environment  ev;
	char              *retval;
	char              *name;

	g_return_val_if_fail (book != NULL,     NULL);
	g_return_val_if_fail (E_IS_BOOK (book), NULL);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_get_name: No URI loaded!\n");
		return NULL;
	}

	CORBA_exception_init (&ev);

	name = Evolution_Book_get_name (book->priv->corba_book, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_get_name: Exception getting name from PAS!\n");
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	if (name == NULL) {
		g_warning ("e_book_get_name: Got NULL name from PAS!\n");
		return NULL;
	}

	retval = g_strdup (name);
	CORBA_free (name);

	return retval;
}

static void
e_book_init (EBook *book)
{
	book->priv             = g_new0 (EBookPrivate, 1);
	book->priv->load_state = URINotLoaded;
}

static void
e_book_destroy (GtkObject *object)
{
	EBook             *book = E_BOOK (object);
	CORBA_Environment  ev;

	if (book->priv->load_state != URINotLoaded)
		e_book_unload_uri (book);

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
				GTK_SIGNAL_OFFSET (EBookClass, card_added),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	e_book_signals [CARD_REMOVED] =
		gtk_signal_new ("card_removed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EBookClass, card_removed),
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
