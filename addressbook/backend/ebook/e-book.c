/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * The Evolution addressbook client object.
 *
 * Author:
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright 1999, 2000, Ximian, Inc.
 */

#include <config.h>
#include <glib.h>
#include <glib-object.h>
#include <string.h>
#include <bonobo-activation/bonobo-activation.h>

#include "addressbook.h"
#include "e-card-cursor.h"
#include "e-book-listener.h"
#include "e-book.h"
#include "e-book-marshal.h"
#include "e-util/e-component-listener.h"

static GObjectClass *parent_class;

#define CARDSERVER_OAF_ID "OAFIID:GNOME_Evolution_Wombat_ServerFactory"

typedef enum {
	URINotLoaded,
	URILoading,
	URILoaded
} EBookLoadState;

struct _EBookPrivate {
	GList *book_factories;
	GList *iter;

	/* cached capabilites */
	char *cap;
	gboolean cap_queried;

	EBookListener	      *listener;
	EComponentListener    *comp_listener;

	GNOME_Evolution_Addressbook_Book         corba_book;

	EBookLoadState         load_state;

	/*
	 * The operation queue.  New operations are appended to the
	 * end of the queue.  When responses come back from the PAS,
	 * the op structures are popped off the front of the queue.
	 */
	GList                 *pending_ops;

	guint op_tag;

	gchar *uri;

	gulong listener_signal;
	gulong died_signal;
};

enum {
	OPEN_PROGRESS,
	WRITABLE_STATUS,
	LINK_STATUS,
	BACKEND_DIED,
	LAST_SIGNAL
};

static guint e_book_signals [LAST_SIGNAL];

typedef struct {
	guint     tag;
	gboolean  active;
	gpointer  cb;
	gpointer  closure;
	EBookViewListener *listener;
} EBookOp;

/*
 * Local response queue management.
 */

static void
e_book_op_free (EBookOp *op)
{
	if (op->listener) {
		bonobo_object_unref (BONOBO_OBJECT (op->listener));
		op->listener = NULL;
	}
	g_free (op);
}

static guint
e_book_queue_op (EBook    *book,
		 gpointer  cb,
		 gpointer  closure,
		 EBookViewListener *listener)
{
	EBookOp *op;

	op           = g_new0 (EBookOp, 1);
	op->tag      = book->priv->op_tag++;
	op->active   = TRUE;
	op->cb       = cb;
	op->closure  = closure;
	op->listener = listener;

	if (op->listener)
		bonobo_object_ref (BONOBO_OBJECT (op->listener));

	book->priv->pending_ops =
		g_list_append (book->priv->pending_ops, op);

	return op->tag;
}

/*
 * Local response queue management.
 */
static void
e_book_unqueue_op (EBook    *book)
{
	EBookOp *op;
	GList *removed;

	removed = g_list_last (book->priv->pending_ops);

	if (removed) {
		book->priv->pending_ops = g_list_remove_link (book->priv->pending_ops,
							      removed);
		op = removed->data;
		e_book_op_free (op);
		g_list_free_1 (removed);
		book->priv->op_tag--;
	}
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
		g_list_remove_link (book->priv->pending_ops,
				    book->priv->pending_ops);

	g_list_free_1 (popped);

	return op;
}

static gboolean
e_book_cancel_op (EBook *book, guint tag)
{
	GList *iter;
	gboolean cancelled = FALSE;

	for (iter = book->priv->pending_ops; iter != NULL && !cancelled; iter = g_list_next (iter)) {
		EBookOp *op = iter->data;
		if (op->tag == tag) {
			op->active = FALSE;
			cancelled = TRUE;
		}
	}
	
	return cancelled;
}

static void
e_book_do_response_create_card (EBook                 *book,
				EBookListenerResponse *resp)
{
	EBookOp *op;

	op = e_book_pop_op (book);

	if (op == NULL) {
		g_warning ("e_book_do_response_create_card: Cannot find operation "
			   "in local op queue!\n");
		return;
	}

	if (op->cb)
		((EBookIdCallback) op->cb) (book, resp->status, resp->id, op->closure);
	g_free (resp->id);
	e_book_op_free (op);
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

	if (op->cb)
		((EBookCallback) op->cb) (book, resp->status, op->closure);

	e_book_op_free (op);
}

static void
e_book_do_response_get_vcard (EBook                 *book,
			      EBookListenerResponse *resp)
{
	EBookOp *op;
	ECard *card;

	op = e_book_pop_op (book);

	if (op == NULL) {
		g_warning ("e_book_do_response_get_vcard: Cannot find operation "
			   "in local op queue!\n");
		return;
	}
	if (resp->vcard != NULL) {

		card = e_card_new(resp->vcard);

		if (card != NULL) {
			e_card_set_book (card, book);
			if (op->cb) {
				if (op->active)
					((EBookCardCallback) op->cb) (book, resp->status, card, op->closure);
				else
					((EBookCardCallback) op->cb) (book, E_BOOK_STATUS_CANCELLED, NULL, op->closure);
			}

			g_object_unref(card);
		} else {
			((EBookCursorCallback) op->cb) (book, resp->status, NULL, op->closure);
		}
	} else {
		((EBookCardCallback) op->cb) (book, resp->status, NULL, op->closure);
	}

	g_free (resp->vcard);
	e_book_op_free (op);
}

static void
e_book_do_response_get_cursor (EBook                 *book,
			       EBookListenerResponse *resp)
{
	CORBA_Environment ev;
	EBookOp *op;
	ECardCursor *cursor;

	op = e_book_pop_op (book);

	if (op == NULL) {
		g_warning ("e_book_do_response_get_cursor: Cannot find operation "
			   "in local op queue!\n");
		return;
	}

	cursor = e_card_cursor_new(resp->cursor);

	if (cursor != NULL) {
		if (op->cb) {
			if (op->active)
				((EBookCursorCallback) op->cb) (book, resp->status, cursor, op->closure);
			else
				((EBookCursorCallback) op->cb) (book, E_BOOK_STATUS_CANCELLED, NULL, op->closure);
		}

		/*
		 * Release the remote GNOME_Evolution_Addressbook_Book in the PAS.
		 */
		CORBA_exception_init (&ev);

		bonobo_object_release_unref (resp->cursor, &ev);

		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("e_book_do_response_get_cursor: Exception releasing "
				   "remote GNOME_Evolution_Addressbook_CardCursor interface!\n");
		}

		CORBA_exception_free (&ev);

		g_object_unref(cursor);
	} else {
		((EBookCursorCallback) op->cb) (book, E_BOOK_STATUS_CANCELLED, NULL, op->closure);
	}
	
	e_book_op_free (op);
}

static void
e_book_do_response_get_view (EBook                 *book,
			     EBookListenerResponse *resp)
{
	CORBA_Environment ev;
	EBookOp *op;
	EBookView *book_view;

	op = e_book_pop_op (book);

	if (op == NULL) {
		g_warning ("e_book_do_response_get_view: Cannot find operation "
			   "in local op queue!\n");
		return;
	}
	
	book_view = e_book_view_new (resp->book_view, op->listener);

	if (book_view != NULL) {
		e_book_view_set_book (book_view, book);

		/* Only execute the callback if the operation is still flagged as active (i.e. hasn't
		   been cancelled.  This is mildly wasteful since we unnecessaryily create the
		   book_view, etc... but I'm leery of tinkering with the CORBA magic. */
		if (op->cb) {
			if (op->active)
				((EBookBookViewCallback) op->cb) (book, resp->status, book_view, op->closure);
			else
				((EBookBookViewCallback) op->cb) (book, E_BOOK_STATUS_CANCELLED, NULL, op->closure);
		}
	
		/*
		 * Release the remote GNOME_Evolution_Addressbook_Book in the PAS.
		 */
		CORBA_exception_init (&ev);

		bonobo_object_release_unref  (resp->book_view, &ev);

		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("e_book_do_response_get_view: Exception releasing "
				   "remote GNOME_Evolution_Addressbook_BookView interface!\n");
		}

		CORBA_exception_free (&ev);

		g_object_unref(book_view);
	} else {
		e_book_view_listener_stop (op->listener);
		((EBookBookViewCallback) op->cb) (book, E_BOOK_STATUS_CANCELLED, NULL, op->closure);
	}

	e_book_op_free (op);
}

static void
e_book_do_response_get_changes (EBook                 *book,
				EBookListenerResponse *resp)
{
	CORBA_Environment ev;
	EBookOp *op;
	EBookView *book_view;

	op = e_book_pop_op (book);

	if (op == NULL) {
		g_warning ("e_book_do_response_get_changes: Cannot find operation "
			   "in local op queue!\n");
		return;
	}

	book_view = e_book_view_new (resp->book_view, op->listener);

	if (book_view != NULL) {
		e_book_view_set_book (book_view, book);
	
		if (op->cb) {
			if (op->active)
				((EBookBookViewCallback) op->cb) (book, resp->status, book_view, op->closure);
			else
				((EBookBookViewCallback) op->cb) (book, E_BOOK_STATUS_CANCELLED, NULL, op->closure);
		}

		/*
		 * Release the remote GNOME_Evolution_Addressbook_BookView in the PAS.
		 */
		CORBA_exception_init (&ev);

		bonobo_object_release_unref  (resp->book_view, &ev);

		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("e_book_do_response_get_changes: Exception releasing "
				   "remote GNOME_Evolution_Addressbook_BookView interface!\n");
		}

		CORBA_exception_free (&ev);

		g_object_unref(book_view);
	} else {
		e_book_view_listener_stop (op->listener);
		((EBookBookViewCallback) op->cb) (book, E_BOOK_STATUS_CANCELLED, NULL, op->closure);
	}

	e_book_op_free (op);
}

static void
backend_died_cb (EComponentListener *cl, gpointer user_data)
{
	EBook *book = user_data;

	book->priv->load_state = URINotLoaded;
        g_signal_emit (book, e_book_signals [BACKEND_DIED], 0);
}

static void
e_book_do_response_open (EBook                 *book,
			 EBookListenerResponse *resp)
{
	EBookOp *op;

	if (resp->status == E_BOOK_STATUS_SUCCESS) {
		book->priv->corba_book  = resp->book;
		book->priv->load_state  = URILoaded;

		book->priv->comp_listener = e_component_listener_new (book->priv->corba_book);
                book->priv->died_signal = g_signal_connect (book->priv->comp_listener, "component_died",
							    G_CALLBACK (backend_died_cb), book);
	}

	op = e_book_pop_op (book);

	if (op == NULL) {
		g_warning ("e_book_do_response_open: Cannot find operation "
			   "in local op queue!\n");
		return;
	}

	if (op->cb)
		((EBookCallback) op->cb) (book, resp->status, op->closure);
	e_book_op_free (op);
}

static void
e_book_do_progress_event (EBook                 *book,
			  EBookListenerResponse *resp)
{
	g_signal_emit (book, e_book_signals [OPEN_PROGRESS], 0,
		       resp->msg, resp->percent);

	g_free (resp->msg);
}

static void
e_book_do_link_event (EBook                 *book,
		      EBookListenerResponse *resp)
{
	g_signal_emit (book, e_book_signals [LINK_STATUS], 0,
		       resp->connected);
}

static void
e_book_do_writable_event (EBook                 *book,
			  EBookListenerResponse *resp)
{
	g_signal_emit (book, e_book_signals [WRITABLE_STATUS], 0,
		       resp->writable);
}

static void
e_book_do_response_get_supported_fields (EBook                 *book,
					 EBookListenerResponse *resp)
{
	EBookOp *op;

	op = e_book_pop_op (book);

	if (op == NULL) {
		g_warning ("e_book_do_response_get_supported_fields: Cannot find operation "
			   "in local op queue!\n");
		return;
	}

	if (op->cb) {
		if (op->active)
			((EBookFieldsCallback) op->cb) (book, resp->status, resp->list, op->closure);
		else
			((EBookFieldsCallback) op->cb) (book, E_BOOK_STATUS_CANCELLED, NULL, op->closure);
	}

	g_object_unref(resp->list);

	e_book_op_free (op);
}

static void
e_book_do_response_get_supported_auth_methods (EBook                 *book,
					       EBookListenerResponse *resp)
{
	EBookOp *op;

	op = e_book_pop_op (book);

	if (op == NULL) {
		g_warning ("e_book_do_response_get_supported_auth_methods: Cannot find operation "
			   "in local op queue!\n");
		return;
	}

	if (op->cb) {
		if (op->active)
			((EBookAuthMethodsCallback) op->cb) (book, resp->status, resp->list, op->closure);
		else
			((EBookAuthMethodsCallback) op->cb) (book, E_BOOK_STATUS_CANCELLED, NULL, op->closure);
	}

	g_object_unref(resp->list);

	e_book_op_free (op);
}

/*
 * Reading notices out of the EBookListener's queue.
 */
static void
e_book_check_listener_queue (EBookListener *listener, EBook *book)
{
	EBookListenerResponse *resp;

	resp = e_book_listener_pop_response (listener);

	if (resp == NULL)
		return;

	switch (resp->op) {
	case CreateCardResponse:
		e_book_do_response_create_card (book, resp);
		break;
	case RemoveCardResponse:
	case ModifyCardResponse:
	case AuthenticationResponse:
		e_book_do_response_generic (book, resp);
		break;
	case GetCardResponse:
		e_book_do_response_get_vcard (book, resp);
		break;
	case GetCursorResponse:
		e_book_do_response_get_cursor (book, resp);
		break;
	case GetBookViewResponse:
		e_book_do_response_get_view(book, resp);
		break;
	case GetChangesResponse:
		e_book_do_response_get_changes(book, resp);
		break;
	case OpenBookResponse:
		e_book_do_response_open (book, resp);
		break;
	case GetSupportedFieldsResponse:
		e_book_do_response_get_supported_fields (book, resp);
		break;
	case GetSupportedAuthMethodsResponse:
		e_book_do_response_get_supported_auth_methods (book, resp);
		break;

	case OpenProgressEvent:
		e_book_do_progress_event (book, resp);
		break;
	case LinkStatusEvent:
		e_book_do_link_event (book, resp);
		break;
	case WritableStatusEvent:
		e_book_do_writable_event (book, resp);
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

typedef struct {
	char                      *uri;
	EBookCallback              open_response;
	gpointer                   closure;
} EBookLoadURIData;

static void e_book_load_uri_from_factory (EBook *book,
					  GNOME_Evolution_Addressbook_BookFactory factory,
					  EBookLoadURIData *load_uri_data);

static void
e_book_load_uri_step (EBook *book, EBookStatus status, EBookLoadURIData *data)
{
	/* iterate to the next possible CardFactory, or fail
	   if it's the last one */
	book->priv->iter = book->priv->iter->next;
	if (book->priv->iter) {
		GNOME_Evolution_Addressbook_BookFactory factory = book->priv->iter->data;
		e_book_load_uri_from_factory (book, factory, data);
	}
	else {
		EBookCallback cb = data->open_response;
		gpointer closure = data->closure;
		
		/* reset the load_state to NotLoaded so people can
                   attempt another load_uri on the book. */
		book->priv->load_state = URINotLoaded;

		g_free (data);

		cb (book, status, closure);
	}
}

static void
e_book_load_uri_open_cb (EBook *book, EBookStatus status, EBookLoadURIData *data)
{
	if (status == E_BOOK_STATUS_SUCCESS) {
		EBookCallback cb = data->open_response;
		gpointer closure = data->closure;

		g_free (data);

		cb (book, status, closure);
	}
	else {
		e_book_load_uri_step (book, status, data);
	}
}

static void
e_book_load_uri_from_factory (EBook *book,
			      GNOME_Evolution_Addressbook_BookFactory factory,
			      EBookLoadURIData *load_uri_data)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	e_book_queue_op (book, e_book_load_uri_open_cb, load_uri_data, NULL);

	GNOME_Evolution_Addressbook_BookFactory_openBook (
		factory, book->priv->uri,
		bonobo_object_corba_objref (BONOBO_OBJECT (book->priv->listener)),
		&ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_load_uri: CORBA exception while opening addressbook!\n");
		e_book_unqueue_op (book);
		CORBA_exception_free (&ev);
		e_book_load_uri_step (book, E_BOOK_STATUS_OTHER_ERROR, load_uri_data);
	}

	CORBA_exception_free (&ev);

}

static gboolean
activate_factories_for_uri (EBook *book, const char *uri)
{
	CORBA_Environment ev;
	Bonobo_ServerInfoList *info_list = NULL;
	int i;
	char *protocol, *query, *colon;
	gboolean retval = FALSE;

	colon = strchr (uri, ':');
	if (!colon) {
		g_warning ("e_book_load_uri: Unable to determine protocol in the URI\n");
		return FALSE;
	}

	protocol = g_strndup (uri, colon-uri);
	query = g_strdup_printf ("repo_ids.has ('IDL:GNOME/Evolution/BookFactory:1.0')"
				 " AND addressbook:supported_protocols.has ('%s')", protocol
				 );

	CORBA_exception_init (&ev);
	
	info_list = bonobo_activation_query (query, NULL, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Eeek!  Cannot perform bonobo-activation query for book factories.");
		CORBA_exception_free (&ev);
		goto shutdown;
	}

	if (info_list->_length == 0) {
		g_warning ("Can't find installed BookFactory that handles protocol '%s'.", protocol);
		CORBA_exception_free (&ev);
		goto shutdown;
	}

	CORBA_exception_free (&ev);

	for (i = 0; i < info_list->_length; i ++) {
		const Bonobo_ServerInfo *info;
		GNOME_Evolution_Addressbook_BookFactory factory;

		info = info_list->_buffer + i;

		factory = bonobo_activation_activate_from_id (info->iid, 0, NULL, NULL);

		if (factory == CORBA_OBJECT_NIL)
			g_warning ("e_book_construct: Could not obtain a handle "
				   "to the Personal Addressbook Server with IID `%s'\n", info->iid);
		else
			book->priv->book_factories = g_list_append (book->priv->book_factories,
								    factory);
	}

	if (!book->priv->book_factories) {
		g_warning ("Couldn't activate any book factories.");
		goto shutdown;
	}

	retval = TRUE;

 shutdown:
	if (info_list)
		CORBA_free (info_list);
	g_free (query);
	g_free (protocol);

	return retval;
}

void
e_book_load_uri (EBook                     *book,
		 const char                *uri,
		 EBookCallback              open_response,
		 gpointer                   closure)
{
	EBookLoadURIData *load_uri_data;
	GNOME_Evolution_Addressbook_BookFactory factory;

	g_return_if_fail (book != NULL);
	g_return_if_fail (E_IS_BOOK (book));
	g_return_if_fail (uri != NULL);
	g_return_if_fail (open_response != NULL);

	if (book->priv->load_state != URINotLoaded) {
		g_warning ("e_book_load_uri: Attempted to load a URI "
			   "on a book which already has a URI loaded!\n");
		open_response (book, E_BOOK_STATUS_OTHER_ERROR, closure); /* XXX need a new status code here */
		return;
	}

	/* try to find a list of factories that can handle the protocol */
	if (!activate_factories_for_uri (book, uri)) {
		open_response (book, E_BOOK_STATUS_PROTOCOL_NOT_SUPPORTED, closure);
		return;
	}
		
	g_free (book->priv->uri);
	book->priv->uri = g_strdup (uri);

	/*
	 * Create our local BookListener interface.
	 */
	book->priv->listener = e_book_listener_new ();
	if (book->priv->listener == NULL) {
		g_warning ("e_book_load_uri: Could not create EBookListener!\n");
		open_response (NULL, E_BOOK_STATUS_OTHER_ERROR, closure); /* XXX need a new status code here */
		return;
	}

	book->priv->listener_signal = g_signal_connect (book->priv->listener, "responses_queued",
							G_CALLBACK (e_book_check_listener_queue), book);

	load_uri_data = g_new (EBookLoadURIData, 1);
	load_uri_data->open_response = open_response;
	load_uri_data->closure = closure;

	/* initialize the iterator, and load from the first one*/
	book->priv->iter = book->priv->book_factories;

	factory = book->priv->iter->data;

	e_book_load_uri_from_factory (book, factory, load_uri_data);

	book->priv->load_state = URILoading;
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
	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_unload_uri: No URI is loaded!\n");
		return;
	}

	/*
	 * Release the remote GNOME_Evolution_Addressbook_Book in the PAS.
	 */
	CORBA_exception_init (&ev);

	bonobo_object_release_unref  (book->priv->corba_book, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_unload_uri: Exception releasing "
			   "remote book interface!\n");
	}

	CORBA_exception_free (&ev);

	e_book_listener_stop (book->priv->listener);
	bonobo_object_unref (BONOBO_OBJECT (book->priv->listener));

	book->priv->listener   = NULL;
	book->priv->load_state = URINotLoaded;
}

const char *
e_book_get_uri (EBook *book)
{
	g_return_val_if_fail (book && E_IS_BOOK (book), NULL);

	return book->priv->uri;
}

char *
e_book_get_static_capabilities (EBook *book)
{
	if (!book->priv->cap_queried) {
		CORBA_Environment ev;
		char *temp;

		CORBA_exception_init (&ev);

		if (book->priv->load_state != URILoaded) {
			g_warning ("e_book_unload_uri: No URI is loaded!\n");
			return g_strdup("");
		}

		temp = GNOME_Evolution_Addressbook_Book_getStaticCapabilities(book->priv->corba_book, &ev);

		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("e_book_get_static_capabilities: Exception "
				   "during get_static_capabilities!\n");
			CORBA_exception_free (&ev);
			return g_strdup("");
		}

		book->priv->cap = g_strdup(temp);
		book->priv->cap_queried = TRUE;

		CORBA_free(temp);

		CORBA_exception_free (&ev);
	}

	return g_strdup (book->priv->cap);
}

gboolean
e_book_check_static_capability  (EBook *book, const char *cap)
{
	gboolean rv = FALSE;
	char *caps = e_book_get_static_capabilities (book);
	if (!caps)
		return FALSE;

	/* XXX this is an inexact test but it works for our use */
	if (strstr (caps, cap))
		rv = TRUE;

	g_free (caps);

	return rv;
}

guint
e_book_get_supported_fields (EBook              *book,
			     EBookFieldsCallback cb,
			     gpointer            closure)
{
	CORBA_Environment ev;
	guint tag;

	CORBA_exception_init (&ev);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_unload_uri: No URI is loaded!\n");
		return 0;
	}

	tag = e_book_queue_op (book, cb, closure, NULL);

	GNOME_Evolution_Addressbook_Book_getSupportedFields(book->priv->corba_book, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_get_supported_fields: Exception "
			   "during get_supported_fields!\n");
		CORBA_exception_free (&ev);
		e_book_unqueue_op (book);
		return 0;
	}

	CORBA_exception_free (&ev);

	return tag;
}

guint
e_book_get_supported_auth_methods (EBook                   *book,
				   EBookAuthMethodsCallback cb,
				   gpointer                 closure)
{
	CORBA_Environment ev;
	guint tag;

	CORBA_exception_init (&ev);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_unload_uri: No URI is loaded!\n");
		return 0;
	}

	tag = e_book_queue_op (book, cb, closure, NULL);

	GNOME_Evolution_Addressbook_Book_getSupportedAuthMethods(book->priv->corba_book, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_get_supported_auth_methods: Exception "
			   "during get_supported_auth_methods!\n");
		CORBA_exception_free (&ev);
		e_book_unqueue_op (book);
		return 0;
	}

	CORBA_exception_free (&ev);

	return tag;
}

static gboolean
e_book_construct (EBook *book)
{
	g_return_val_if_fail (book != NULL,     FALSE);
	g_return_val_if_fail (E_IS_BOOK (book), FALSE);

	book->priv->book_factories = NULL;

	return TRUE;
}

/**
 * e_book_new:
 */
EBook *
e_book_new (void)
{
	EBook *book;

	book = g_object_new (E_TYPE_BOOK, NULL);

	if (! e_book_construct (book)) {
		g_object_unref (book);
		return NULL;
	}

	return book;
}

/* User authentication. */

void
e_book_authenticate_user (EBook         *book,
			  const char    *user,
			  const char    *passwd,
			  const char    *auth_method,
			  EBookCallback cb,
			  gpointer      closure)
{
	CORBA_Environment  ev;

	g_return_if_fail (book != NULL);
	g_return_if_fail (E_IS_BOOK (book));

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_authenticate_user: No URI loaded!\n");
		return;
	}

	CORBA_exception_init (&ev);

	e_book_queue_op (book, cb, closure, NULL);

	GNOME_Evolution_Addressbook_Book_authenticateUser (book->priv->corba_book,
							   user,
							   passwd,
							   auth_method,
							   &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_authenticate_user: Exception authenticating user with the PAS!\n");
		CORBA_exception_free (&ev);
		e_book_unqueue_op (book);
		return;
	}

	CORBA_exception_free (&ev);
}

/* Fetching cards */

/**
 * e_book_get_card:
 */
guint
e_book_get_card (EBook             *book,
		 const char        *id,
		 EBookCardCallback  cb,
		 gpointer           closure)
{
	CORBA_Environment ev;
	guint tag;

	g_return_val_if_fail (book != NULL,     0);
	g_return_val_if_fail (E_IS_BOOK (book), 0);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_get_card: No URI loaded!\n");
		return 0;
	}

	CORBA_exception_init (&ev);

	tag = e_book_queue_op (book, cb, closure, NULL);

	GNOME_Evolution_Addressbook_Book_getVCard (book->priv->corba_book, (const GNOME_Evolution_Addressbook_VCard) id, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_get_card: Exception "
			   "getting card!\n");
		CORBA_exception_free (&ev);
		e_book_unqueue_op (book);
		return 0;
	}

	CORBA_exception_free (&ev);

	return tag;
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
	GList *list = NULL;
	gboolean rv;

	list = g_list_prepend (list, (char*)id);
	
	rv = e_book_remove_cards (book, list, cb, closure);

	g_list_free (list);

	return rv;
}

gboolean
e_book_remove_cards (EBook         *book,
		     GList         *ids,
		     EBookCallback cb,
		     gpointer      closure)
{
	GNOME_Evolution_Addressbook_CardIdList idlist;
	CORBA_Environment ev;
	GList *l;
	int num_ids, i;

	g_return_val_if_fail (book != NULL,     FALSE);
	g_return_val_if_fail (E_IS_BOOK (book), FALSE);
	g_return_val_if_fail (ids != NULL,     FALSE);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_remove_card_by_id: No URI loaded!\n");
		return FALSE;
	}

	CORBA_exception_init (&ev);

	e_book_queue_op (book, cb, closure, NULL);

	num_ids = g_list_length (ids);
	idlist._buffer = CORBA_sequence_GNOME_Evolution_Addressbook_CardId_allocbuf (num_ids);
	idlist._maximum = num_ids;
	idlist._length = num_ids;

	for (l = ids, i = 0; l; l=l->next, i ++) {
		idlist._buffer[i] = CORBA_string_dup (l->data);
	}

	GNOME_Evolution_Addressbook_Book_removeCards (book->priv->corba_book, &idlist, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_remove_card_by_id: CORBA exception "
			   "talking to PAS!\n");
		CORBA_exception_free (&ev);
		e_book_unqueue_op (book);
		return FALSE;
	}
	
	CORBA_exception_free (&ev);

	CORBA_free(idlist._buffer);

	return TRUE;
}


/* Adding cards. */

/**
 * e_book_add_card:
 */
gboolean
e_book_add_card (EBook           *book,
		 ECard           *card,
		 EBookIdCallback  cb,
		 gpointer         closure)

{
	char     *vcard;
	gboolean  retval;

	g_return_val_if_fail (book != NULL,     FALSE);
	g_return_val_if_fail (E_IS_BOOK (book), FALSE);
	g_return_val_if_fail (card != NULL,     FALSE);
	g_return_val_if_fail (E_IS_CARD (card), FALSE);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_add_card: No URI loaded!\n");
		return FALSE;
	}

	vcard = e_card_get_vcard_assume_utf8 (card);

	if (vcard == NULL) {
		g_warning ("e_book_add_card: Cannot convert card to VCard string!\n");
		return FALSE;
	}

	retval = e_book_add_vcard (book, vcard, cb, closure);

	g_free (vcard);

	e_card_set_book (card, book);

	return retval;
}

/**
 * e_book_add_vcard:
 */
gboolean
e_book_add_vcard (EBook           *book,
		  const char      *vcard,
		  EBookIdCallback  cb,
		  gpointer         closure)
{
	CORBA_Environment ev;

	g_return_val_if_fail (book  != NULL,    FALSE);
	g_return_val_if_fail (E_IS_BOOK (book), FALSE);
	g_return_val_if_fail (vcard != NULL,    FALSE);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_add_vcard: No URI loaded!\n");
		return FALSE;
	}

	CORBA_exception_init (&ev);

	e_book_queue_op (book, (EBookCallback) cb, closure, NULL);

	GNOME_Evolution_Addressbook_Book_addCard (
		book->priv->corba_book, (const GNOME_Evolution_Addressbook_VCard) vcard, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_add_vcard: Exception adding card to PAS!\n");
		CORBA_exception_free (&ev);
		e_book_unqueue_op (book);
		return FALSE;
	}

	CORBA_exception_free (&ev);

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

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_commit_card: No URI loaded!\n");
		return FALSE;
	}

	vcard = e_card_get_vcard_assume_utf8 (card);

	if (vcard == NULL) {
		g_warning ("e_book_commit_card: Error "
			   "getting VCard for card!\n");
		return FALSE;
	}

	retval = e_book_commit_vcard (book, vcard, cb, closure);

	g_free (vcard);

	e_card_set_book (card, book);

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

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_commit_vcard: No URI loaded!\n");
		return FALSE;
	}

	CORBA_exception_init (&ev);

	e_book_queue_op (book, cb, closure, NULL);

	GNOME_Evolution_Addressbook_Book_modifyCard (
		book->priv->corba_book, (const GNOME_Evolution_Addressbook_VCard) vcard, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_commit_vcard: Exception "
			   "modifying card in PAS!\n");
		CORBA_exception_free (&ev);
		e_book_unqueue_op (book);
		return FALSE;
	}

	CORBA_exception_free (&ev);

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

	GNOME_Evolution_Addressbook_Book_checkConnection (book->priv->corba_book, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_check_connection: Exception "
			   "querying the PAS!\n");
		CORBA_exception_free (&ev);
		return FALSE;
	}
	
	CORBA_exception_free (&ev);

	return TRUE;
}

guint
e_book_get_cursor       (EBook               *book,
			 gchar               *query,
			 EBookCursorCallback  cb,
			 gpointer             closure)
{
	CORBA_Environment ev;
	guint tag;
  
	g_return_val_if_fail (book != NULL,     0);
	g_return_val_if_fail (E_IS_BOOK (book), 0);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_check_connection: No URI loaded!\n");
		return 0;
	}
	
	CORBA_exception_init (&ev);

	tag = e_book_queue_op (book, cb, closure, NULL);
	
	GNOME_Evolution_Addressbook_Book_getCursor (book->priv->corba_book, query, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_get_all_cards: Exception "
			   "querying list of cards!\n");
		CORBA_exception_free (&ev);
		e_book_unqueue_op (book);
		return 0;
	}
	
	CORBA_exception_free (&ev);

	return tag;
}

guint
e_book_get_book_view       (EBook                 *book,
			    const gchar           *query,
			    EBookBookViewCallback  cb,
			    gpointer               closure)
{
	CORBA_Environment ev;
	EBookViewListener *listener;
	guint tag;
  
	g_return_val_if_fail (book != NULL,     0);
	g_return_val_if_fail (E_IS_BOOK (book), 0);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_get_book_view: No URI loaded!\n");
		return 0;
	}

	listener = e_book_view_listener_new();
	
	CORBA_exception_init (&ev);

	tag = e_book_queue_op (book, cb, closure, listener);
	
	GNOME_Evolution_Addressbook_Book_getBookView (book->priv->corba_book, bonobo_object_corba_objref(BONOBO_OBJECT(listener)), query, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_get_book_view: Exception "
			   "getting book_view!\n");
		CORBA_exception_free (&ev);
		e_book_unqueue_op (book);
		return 0;
	}
	
	CORBA_exception_free (&ev);

	return tag;
}

guint
e_book_get_completion_view      (EBook                 *book,
				 const gchar           *query,
				 EBookBookViewCallback  cb,
				 gpointer               closure)
{
	CORBA_Environment ev;
	EBookViewListener *listener;
	guint tag;
  
	g_return_val_if_fail (book != NULL,     0);
	g_return_val_if_fail (E_IS_BOOK (book), 0);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_get_completion_view: No URI loaded!\n");
		return 0;
	}

	listener = e_book_view_listener_new();
	
	CORBA_exception_init (&ev);

	tag = e_book_queue_op (book, cb, closure, listener);
	
	GNOME_Evolution_Addressbook_Book_getCompletionView (book->priv->corba_book,
							    bonobo_object_corba_objref(BONOBO_OBJECT(listener)),
							    query, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_get_completion_view: Exception "
			   "getting completion_view!\n");
		CORBA_exception_free (&ev);
		e_book_unqueue_op (book);
		return 0;
	}
	
	CORBA_exception_free (&ev);

	return tag;
}

guint
e_book_get_changes         (EBook                 *book,
			    gchar                 *changeid,
			    EBookBookViewCallback  cb,
			    gpointer               closure)
{
	CORBA_Environment ev;
	EBookViewListener *listener;
	guint tag;
  
	g_return_val_if_fail (book != NULL,     0);
	g_return_val_if_fail (E_IS_BOOK (book), 0);

	if (book->priv->load_state != URILoaded) {
		g_warning ("e_book_get_changes: No URI loaded!\n");
		return FALSE;
	}

	listener = e_book_view_listener_new();
	
	CORBA_exception_init (&ev);

	tag = e_book_queue_op (book, cb, closure, listener);
	
	GNOME_Evolution_Addressbook_Book_getChanges (book->priv->corba_book, bonobo_object_corba_objref(BONOBO_OBJECT(listener)), changeid, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_book_changes: Exception "
			   "getting changes!\n");
		CORBA_exception_free (&ev);
		e_book_unqueue_op (book);
		return 0;
	}
	
	CORBA_exception_free (&ev);

	return tag;
}

/**
 * e_book_cancel
 */

void
e_book_cancel (EBook *book, guint tag)
{
	g_return_if_fail (book != NULL);
	g_return_if_fail (E_IS_BOOK (book));
	g_return_if_fail (tag != 0);

	/* In an attempt to be useful, we take a bit of extra care in reporting
	   errors.  This might come in handy someday. */
	if (tag >= book->priv->op_tag)
		g_warning ("Attempt to cancel unassigned operation (%u)", tag);
	else if (! e_book_cancel_op (book, tag))
		g_warning ("Attempt to cancel unknown operation (%u)", tag); 
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

	name = GNOME_Evolution_Addressbook_Book_getName (book->priv->corba_book, &ev);

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
	book->priv->op_tag     = 1;
	book->priv->uri        = NULL;
}

static void
e_book_dispose (GObject *object)
{
	EBook             *book = E_BOOK (object);

	if (book->priv) {
		CORBA_Environment  ev;
		GList *l;

		if (book->priv->comp_listener) {
			g_signal_handler_disconnect (book->priv->comp_listener, book->priv->died_signal);
			g_object_unref (book->priv->comp_listener);
			book->priv->comp_listener = NULL;
		}

		if (book->priv->load_state == URILoaded)
			e_book_unload_uri (book);

		CORBA_exception_init (&ev);

		for (l = book->priv->book_factories; l; l = l->next) {
			CORBA_Object_release ((CORBA_Object)l->data, &ev);
			if (ev._major != CORBA_NO_EXCEPTION) {
				g_warning ("EBook: Exception while releasing BookFactory\n");

				CORBA_exception_free (&ev);
				CORBA_exception_init (&ev);
			}
		}
		
		CORBA_exception_free (&ev);

		if (book->priv->listener) {
			g_signal_handler_disconnect (book->priv->listener, book->priv->listener_signal);
			bonobo_object_unref (book->priv->listener);
			book->priv->listener = NULL;
		}
		
		g_free (book->priv->cap);

		g_free (book->priv->uri);

		g_free (book->priv);
		book->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
e_book_class_init (EBookClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (G_TYPE_OBJECT);

	e_book_signals [LINK_STATUS] =
		g_signal_new ("link_status",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EBookClass, link_status),
			      NULL, NULL,
			      e_book_marshal_NONE__BOOL,
			      G_TYPE_NONE, 1,
			      G_TYPE_BOOLEAN);

	e_book_signals [WRITABLE_STATUS] =
		g_signal_new ("writable_status",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EBookClass, writable_status),
			      NULL, NULL,
			      e_book_marshal_NONE__BOOL,
			      G_TYPE_NONE, 1,
			      G_TYPE_BOOLEAN);

	e_book_signals [BACKEND_DIED] =
		g_signal_new ("backend_died",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EBookClass, backend_died),
			      NULL, NULL,
			      e_book_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	object_class->dispose = e_book_dispose;
}

/**
 * e_book_get_type:
 */
GType
e_book_get_type (void)
{
	static GType type = 0;

	if (! type) {
		GTypeInfo info = {
			sizeof (EBookClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc)  e_book_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EBook),
			0,    /* n_preallocs */
			(GInstanceInitFunc) e_book_init
		};

		type = g_type_register_static (G_TYPE_OBJECT, "EBook", &info, 0);
	}

	return type;
}
