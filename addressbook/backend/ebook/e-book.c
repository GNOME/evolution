/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>

#include <pthread.h>

#include <string.h>

#include "e-book.h"
#include "e-vcard.h"

#include <bonobo-activation/bonobo-activation.h>

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-main.h>

#include <libgnome/gnome-i18n.h>

#include "e-book-marshal.h"
#include "e-book-listener.h"
#include "addressbook.h"
#include "e-util/e-component-listener.h"
#include "e-util/e-msgport.h"

static GObjectClass *parent_class;

#define CARDSERVER_OAF_ID "OAFIID:GNOME_Evolution_Wombat_ServerFactory"

#define e_return_error_if_fail(expr,error_code)	G_STMT_START{		\
     if G_LIKELY(expr) { } else						\
       {								\
	 g_log (G_LOG_DOMAIN,						\
		G_LOG_LEVEL_CRITICAL,					\
		"file %s: line %d (%s): assertion `%s' failed",		\
		__FILE__,						\
		__LINE__,						\
		__PRETTY_FUNCTION__,					\
		#expr);							\
	 g_set_error (error, E_BOOK_ERROR, (error_code),                \
		"file %s: line %d (%s): assertion `%s' failed",		\
		__FILE__,						\
		__LINE__,						\
		__PRETTY_FUNCTION__,					\
		#expr);							\
	 return FALSE;							\
       };				}G_STMT_END

/* XXX we need a better error message here */
#define E_BOOK_CHECK_STATUS(status,error) G_STMT_START{			\
	if ((status) == E_BOOK_ERROR_OK) {				\
		return TRUE;						\
	}								\
	else {								\
		g_set_error ((error), E_BOOK_ERROR, (status), "EBookStatus returned %d", (status));	\
		return FALSE;						\
	}				}G_STMT_END

enum {
	OPEN_PROGRESS,
	WRITABLE_STATUS,
	BACKEND_DIED,
	LAST_SIGNAL
};

static guint e_book_signals [LAST_SIGNAL];

typedef struct {
	EMutex *mutex;
	pthread_cond_t cond;
	EBookStatus status;

	char *id;
	GList *list;
	EContact *contact;

	EBookView *view;
	EBookViewListener *listener;
} EBookOp;

typedef enum {
	E_BOOK_URI_NOT_LOADED,
	E_BOOK_URI_LOADING,
	E_BOOK_URI_LOADED
} EBookLoadState;

struct _EBookPrivate {
	GList *book_factories;
	GList *iter;

	/* cached capabilites */
	char *cap;
	gboolean cap_queried;

	/* cached writable status */
	gboolean writable;

	EBookListener         *listener;
	EComponentListener    *comp_listener;

	GNOME_Evolution_Addressbook_Book         corba_book;

	EBookLoadState         load_state;


	EBookOp *current_op;

	EMutex *mutex;

	gchar *uri;

	gulong listener_signal;
	gulong died_signal;
};


/* Error quark */
GQuark
e_book_error_quark (void)
{
  static GQuark q = 0;
  if (q == 0)
    q = g_quark_from_static_string ("e-book-error-quark");

  return q;
}



/* EBookOp calls */

static EBookOp*
e_book_new_op (EBook *book)
{
	EBookOp *op = g_new0 (EBookOp, 1);

	op->mutex = e_mutex_new (E_MUTEX_SIMPLE);
	pthread_cond_init (&op->cond, 0);

	book->priv->current_op = op;

	return op;
}

static EBookOp*
e_book_get_op (EBook *book)
{
	if (!book->priv->current_op) {
		g_warning ("unexpected response");
		return NULL;
	}
		
	return book->priv->current_op;
}

static void
e_book_op_free (EBookOp *op)
{
	/* XXX more stuff here */
	pthread_cond_destroy (&op->cond);
	e_mutex_destroy (op->mutex);
	g_free (op);
}

static void
e_book_op_remove (EBook *book,
		  EBookOp *op)
{
	if (book->priv->current_op != op)
		g_warning ("cannot remove op, it's not current");

	book->priv->current_op = NULL;
}

static void
e_book_clear_op (EBook *book,
		 EBookOp *op)
{
	e_book_op_remove (book, op);
	e_mutex_unlock (op->mutex);
	e_book_op_free (op);
}



/**
 * e_book_add_card:
 * @book: an #EBook
 * @contact: an #EContact
 *
 * adds @contact to @book.
 *
 * Return value: a #EBookStatus value.
 **/
gboolean
e_book_add_contact (EBook           *book,
		    EContact        *contact,
		    GError         **error)
{
	EBookOp *our_op;
	EBookStatus status;
	CORBA_Environment ev;
	char *vcard_str;

	printf ("e_book_add_contact\n");

	e_return_error_if_fail (book && E_IS_BOOK (book), E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (contact && E_IS_CONTACT (contact), E_BOOK_ERROR_INVALID_ARG);

	e_mutex_lock (book->priv->mutex);

	if (book->priv->load_state != E_BOOK_URI_LOADED) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_URI_NOT_LOADED,
			     _("e_book_add_contact called on book before e_book_load_uri"));
		return FALSE;
	}

	if (book->priv->current_op != NULL) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_BUSY,
			     _("book busy"));
		return FALSE;
	}

	vcard_str = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);

	our_op = e_book_new_op (book);

	e_mutex_lock (our_op->mutex);

	e_mutex_unlock (book->priv->mutex);

	CORBA_exception_init (&ev);

	/* will eventually end up calling e_book_response_add_contact */
	GNOME_Evolution_Addressbook_Book_addContact (book->priv->corba_book,
						     (const GNOME_Evolution_Addressbook_VCard) vcard_str, &ev);

	g_free (vcard_str);

	if (ev._major != CORBA_NO_EXCEPTION) {

		e_book_clear_op (book, our_op);

		CORBA_exception_free (&ev);

		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_CORBA_EXCEPTION,
			     _("Corba exception making Book::addContact call"));
		return FALSE;
	}

	CORBA_exception_free (&ev);

	/* wait for something to happen (both cancellation and a
	   successful response will notity us via our cv */
	e_mutex_cond_wait (&our_op->cond, our_op->mutex);

	status = our_op->status;
	e_contact_set (contact, E_CONTACT_UID, our_op->id);
	g_free (our_op->id);

	e_book_clear_op (book, our_op);

	E_BOOK_CHECK_STATUS (status, error);
}

static void
e_book_response_add_contact (EBook       *book,
			     EBookStatus  status,
			     char        *id)
{
	EBookOp *op;

	printf ("e_book_response_add_contact\n");

	op = e_book_get_op (book);

	if (op == NULL) {
	  g_warning ("e_book_response_add_contact: Cannot find operation ");
	  return;
	}

	e_mutex_lock (op->mutex);

	op->status = status;
	op->id = g_strdup (id);

	pthread_cond_signal (&op->cond);

	e_mutex_unlock (op->mutex);
}



/**
 * e_book_commit_contact:
 * @book: an #EBook
 * @contact: an #EContact
 *
 * applies the changes made to @contact to the stored version in
 * @book.
 *
 * Return value: a #EBookStatus value.
 **/
gboolean
e_book_commit_contact (EBook           *book,
		       EContact        *contact,
		       GError         **error)
{
	EBookOp *our_op;
	EBookStatus status;
	CORBA_Environment ev;
	char *vcard_str;

	e_return_error_if_fail (book && E_IS_BOOK (book), E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (contact && E_IS_CONTACT (contact), E_BOOK_ERROR_INVALID_ARG);

	e_mutex_lock (book->priv->mutex);

	if (book->priv->load_state != E_BOOK_URI_LOADED) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_URI_NOT_LOADED,
			     _("e_book_commit_contact called on book before e_book_load_uri"));
		return FALSE;
	}

	if (book->priv->current_op != NULL) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_BUSY,
			     _("book busy"));
		return FALSE;
	}

	vcard_str = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);

	our_op = e_book_new_op (book);

	e_mutex_lock (our_op->mutex);

	e_mutex_unlock (book->priv->mutex);

	CORBA_exception_init (&ev);

	/* will eventually end up calling _e_book_response_generic */
	GNOME_Evolution_Addressbook_Book_modifyContact (book->priv->corba_book,
							(const GNOME_Evolution_Addressbook_VCard) vcard_str, &ev);

	g_free (vcard_str);

	if (ev._major != CORBA_NO_EXCEPTION) {

		e_book_clear_op (book, our_op);

		CORBA_exception_free (&ev);

		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_CORBA_EXCEPTION,
			     _("Corba exception making Book::modifyContact call"));
		return FALSE;
	}

	CORBA_exception_free (&ev);

	/* wait for something to happen (both cancellation and a
	   successful response will notity us via our cv */
	e_mutex_cond_wait (&our_op->cond, our_op->mutex);

	status = our_op->status;
	e_contact_set (contact, E_CONTACT_UID, our_op->id);
	g_free (our_op->id);

	/* remove the op from the book's hash of operations */
	e_book_clear_op (book, our_op);

	E_BOOK_CHECK_STATUS (status, error);
}


/**
 * e_book_get_supported_fields:
 * @book: an #EBook
 * @fields: a #GList
 *
 * queries @book for the list of fields it supports.  mostly for use
 * by the contact editor so it knows what fields to sensitize.
 *
 * Return value: a #EBookStatus value.
 **/
gboolean
e_book_get_supported_fields  (EBook            *book,
			      GList           **fields,
			      GError          **error)
{
	EBookOp *our_op;
	EBookStatus status;
	CORBA_Environment ev;

	e_return_error_if_fail (book && E_IS_BOOK (book), E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (fields,                   E_BOOK_ERROR_INVALID_ARG);

	e_mutex_lock (book->priv->mutex);

	if (book->priv->load_state != E_BOOK_URI_LOADED) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_URI_NOT_LOADED,
			     _("e_book_get_supported_fields on book before e_book_load_uri"));
		return FALSE;
	}

	if (book->priv->current_op != NULL) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_BUSY,
			     _("book busy"));
	}

	our_op = e_book_new_op (book);

	e_mutex_lock (our_op->mutex);

	e_mutex_unlock (book->priv->mutex);

	CORBA_exception_init (&ev);

	/* will eventually end up calling
	   _e_book_response_get_supported_fields */
	GNOME_Evolution_Addressbook_Book_getSupportedFields(book->priv->corba_book, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {

		e_book_clear_op (book, our_op);

		CORBA_exception_free (&ev);

		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_CORBA_EXCEPTION,
			     _("Corba exception making Book::getSupportedFields call"));
		return FALSE;
	}


	CORBA_exception_free (&ev);

	/* wait for something to happen (both cancellation and a
	   successful response will notity us via our cv */
	e_mutex_cond_wait (&our_op->cond, our_op->mutex);

	status = our_op->status;
	*fields = our_op->list;

	e_book_clear_op (book, our_op);

	E_BOOK_CHECK_STATUS (status, error);
}

static void
e_book_response_get_supported_fields (EBook       *book,
				      EBookStatus  status,
				      GList       *fields)
{
	EBookOp *op;

	op = e_book_get_op (book);

	if (op == NULL) {
	  g_warning ("e_book_response_get_supported_fields: Cannot find operation ");
	  return;
	}

	e_mutex_lock (op->mutex);

	op->status = status;
	op->list = fields;

	pthread_cond_signal (&op->cond);

	e_mutex_unlock (op->mutex);
}


/**
 * e_book_get_supported_auth_methods:
 * @book: an #EBook
 * @auth_methods: a #GList
 *
 * queries @book for the list of authentication methods it supports.
 *
 * Return value: a #EBookStatus value.
 **/
gboolean
e_book_get_supported_auth_methods (EBook            *book,
				   GList           **auth_methods,
				   GError          **error)
{
	EBookOp *our_op;
	EBookStatus status;
	CORBA_Environment ev;

	e_return_error_if_fail (book && E_IS_BOOK (book), E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (auth_methods,             E_BOOK_ERROR_INVALID_ARG);

	e_mutex_lock (book->priv->mutex);

	if (book->priv->load_state != E_BOOK_URI_LOADED) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_URI_NOT_LOADED,
			     _("e_book_get_supported_auth_methods on book before e_book_load_uri"));
		return FALSE;
	}

	if (book->priv->current_op != NULL) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_BUSY,
			     _("book busy"));
		return FALSE;
	}

	our_op = e_book_new_op (book);

	e_mutex_lock (our_op->mutex);

	e_mutex_unlock (book->priv->mutex);

	CORBA_exception_init (&ev);

	/* will eventually end up calling
	   e_book_response_get_supported_fields */
	GNOME_Evolution_Addressbook_Book_getSupportedAuthMethods(book->priv->corba_book, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {

		e_book_clear_op (book, our_op);

		CORBA_exception_free (&ev);

		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_CORBA_EXCEPTION,
			     _("Corba exception making Book::getSupportedAuthMethods call"));
		return FALSE;
	}


	CORBA_exception_free (&ev);

	/* wait for something to happen (both cancellation and a
	   successful response will notity us via our cv */
	e_mutex_cond_wait (&our_op->cond, our_op->mutex);

	status = our_op->status;
	*auth_methods = our_op->list;

	e_book_clear_op (book, our_op);

	E_BOOK_CHECK_STATUS (status, error);
}

static void
e_book_response_get_supported_auth_methods (EBook                 *book,
					    EBookStatus            status,
					    GList                 *auth_methods)
{
	EBookOp *op;

	op = e_book_get_op (book);

	if (op == NULL) {
	  g_warning ("e_book_response_get_supported_auth_methods: Cannot find operation ");
	  return;
	}

	e_mutex_lock (op->mutex);

	op->status = status;
	op->list = auth_methods;

	pthread_cond_signal (&op->cond);

	e_mutex_unlock (op->mutex);
}



/**
 * e_book_authenticate_user:
 * @book: an #EBook
 * @user: a string
 * @passwd: a string
 * @auth_method: a string
 *
 * authenticates @user with @passwd, using the auth method
 * @auth_method.  @auth_method must be one of the authentication
 * methods returned using e_book_get_supported_auth_methods.
 *
 * Return value: a #EBookStatus value.
 **/
gboolean
e_book_authenticate_user (EBook         *book,
			  const char    *user,
			  const char    *passwd,
			  const char    *auth_method,
			  GError       **error)
{
	EBookOp *our_op;
	EBookStatus status;
	CORBA_Environment ev;

	e_return_error_if_fail (book && E_IS_BOOK (book), E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (user,                     E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (passwd,                   E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (auth_method,              E_BOOK_ERROR_INVALID_ARG);

	e_mutex_lock (book->priv->mutex);

	if (book->priv->load_state != E_BOOK_URI_LOADED) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_URI_NOT_LOADED,
			     _("e_book_authenticate_user on book before e_book_load_uri"));
		return FALSE;
	}

	if (book->priv->current_op != NULL) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_BUSY,
			     _("book busy"));
		return FALSE;
	}

	our_op = e_book_new_op (book);

	e_mutex_lock (our_op->mutex);

	e_mutex_unlock (book->priv->mutex);

	CORBA_exception_init (&ev);

	/* will eventually end up calling
	   e_book_response_generic */
	GNOME_Evolution_Addressbook_Book_authenticateUser (book->priv->corba_book,
							   user, passwd,
							   auth_method,
							   &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {

		e_book_clear_op (book, our_op);

		CORBA_exception_free (&ev);

		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_CORBA_EXCEPTION,
			     _("Corba exception making Book::authenticateUser call"));
		return FALSE;
	}

	CORBA_exception_free (&ev);

	/* wait for something to happen (both cancellation and a
	   successful response will notity us via our cv */
	e_mutex_cond_wait (&our_op->cond, our_op->mutex);

	status = our_op->status;

	e_book_clear_op (book, our_op);

	E_BOOK_CHECK_STATUS (status, error);
}


/**
 * e_book_get_contact:
 * @book: an #EBook
 * @id: a string
 * @contact: an #EContact
 *
 * Fills in @contact with the contents of the vcard in @book
 * corresponding to @id.
 *
 * Return value: a #EBookStatus value.
 **/
gboolean
e_book_get_contact (EBook       *book,
		    const char  *id,
		    EContact   **contact,
		    GError     **error)
{
	EBookOp *our_op;
	EBookStatus status;
	CORBA_Environment ev;

	e_return_error_if_fail (book && E_IS_BOOK (book), E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (id,                       E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (contact,                     E_BOOK_ERROR_INVALID_ARG);

	e_mutex_lock (book->priv->mutex);

	if (book->priv->load_state != E_BOOK_URI_LOADED) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_URI_NOT_LOADED,
			     _("e_book_get_contact on book before e_book_load_uri"));
		return FALSE;
	}

	if (book->priv->current_op != NULL) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_BUSY,
			     _("book busy"));
		return FALSE;
	}

	our_op = e_book_new_op (book);

	e_mutex_lock (our_op->mutex);

	e_mutex_unlock (book->priv->mutex);

	CORBA_exception_init (&ev);

	/* will eventually end up calling e_book_response_generic */
	GNOME_Evolution_Addressbook_Book_getContact (book->priv->corba_book,
						     (const GNOME_Evolution_Addressbook_VCard) id, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {

		e_book_clear_op (book, our_op);

		CORBA_exception_free (&ev);

		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_CORBA_EXCEPTION,
			     _("Corba exception making Book::getContact call"));
		return FALSE;
	}

	CORBA_exception_free (&ev);

	/* wait for something to happen (both cancellation and a
	   successful response will notity us via our cv */
	e_mutex_cond_wait (&our_op->cond, our_op->mutex);

	status = our_op->status;
	*contact = our_op->contact;

	e_book_clear_op (book, our_op);

	E_BOOK_CHECK_STATUS (status, error);
}

static void
e_book_response_get_contact (EBook       *book,
			     EBookStatus  status,
			     EContact    *contact)
{
	EBookOp *op;

	printf ("e_book_response_get_contact\n");

	op = e_book_get_op (book);

	if (op == NULL) {
	  g_warning ("e_book_response_get_contact: Cannot find operation ");
	  return;
	}

	e_mutex_lock (op->mutex);

	op->status = status;
	op->contact = contact;

	pthread_cond_signal (&op->cond);

	e_mutex_unlock (op->mutex);
}


/**
 * e_book_remove_contact:
 * @book: an #EBook
 * @id: a string
 *
 * Removes the contact with id @id from @book.
 *
 * Return value: a #EBookStatus value.
 **/
gboolean
e_book_remove_contact (EBook       *book,
		       const char  *id,
		       GError     **error)
{
	GList *list;
	gboolean rv;

	e_return_error_if_fail (book && E_IS_BOOK (book), E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (id,                       E_BOOK_ERROR_INVALID_ARG);

	e_mutex_lock (book->priv->mutex);

	if (book->priv->load_state != E_BOOK_URI_LOADED) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_URI_NOT_LOADED,
			     _("e_book_remove_contact on book before e_book_load_uri"));
		return FALSE;
	}

	if (book->priv->current_op != NULL) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_BUSY,
			     _("book busy"));
		return FALSE;
	}

	e_mutex_lock (book->priv->mutex);

	list = g_list_append (NULL, (char*)id);

	rv = e_book_remove_contacts (book, list, error);

	return rv;
}

/**
 * e_book_remove_contacts:
 * @book: an #EBook
 * @ids: an #GList of const char *id's
 *
 * Removes the contacts with ids from the list @ids from @book.  This is
 * always more efficient than calling e_book_remove_contact_by_id if you
 * have more than one id to remove, as some backends can implement it
 * as a batch request.
 *
 * Return value: a #EBookStatus value.
 **/
gboolean
e_book_remove_contacts (EBook    *book,
			GList    *ids,
			GError  **error)
{
	GNOME_Evolution_Addressbook_ContactIdList idlist;
	CORBA_Environment ev;
	GList *iter;
	int num_ids, i;
	EBookOp *our_op;
	EBookStatus status;

	e_return_error_if_fail (book && E_IS_BOOK (book), E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (ids,                      E_BOOK_ERROR_INVALID_ARG);

	e_mutex_lock (book->priv->mutex);

	if (book->priv->load_state != E_BOOK_URI_LOADED) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_URI_NOT_LOADED,
			     _("e_book_remove_contacts on book before e_book_load_uri"));
		return FALSE;
	}

	if (book->priv->current_op != NULL) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_BUSY,
			     _("book busy"));
		return FALSE;
	}

	our_op = e_book_new_op (book);

	e_mutex_lock (our_op->mutex);

	e_mutex_unlock (book->priv->mutex);

	CORBA_exception_init (&ev);

	num_ids = g_list_length (ids);
	idlist._buffer = CORBA_sequence_GNOME_Evolution_Addressbook_ContactId_allocbuf (num_ids);
	idlist._maximum = num_ids;
	idlist._length = num_ids;

	for (iter = ids, i = 0; iter; iter = iter->next)
		idlist._buffer[i++] = CORBA_string_dup (iter->data);

	/* will eventually end up calling e_book_response_generic */
	GNOME_Evolution_Addressbook_Book_removeContacts (book->priv->corba_book, &idlist, &ev);

	CORBA_free(idlist._buffer);

	if (ev._major != CORBA_NO_EXCEPTION) {

		e_book_clear_op (book, our_op);

		CORBA_exception_free (&ev);

		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_CORBA_EXCEPTION,
			     _("Corba exception making Book::removeContacts call"));
		return FALSE;
	}
	
	CORBA_exception_free (&ev);

	/* wait for something to happen (both cancellation and a
	   successful response will notity us via our cv */
	e_mutex_cond_wait (&our_op->cond, our_op->mutex);

	status = our_op->status;

	e_book_clear_op (book, our_op);

	E_BOOK_CHECK_STATUS (status, error);
}


/**
 * e_book_get_book_view:
 * @book: an #EBook
 * @query: an #EBookQuery
 * @requested_fields a #GList containing the names of fields to return, or NULL for all
 * @max_results the maximum number of contacts to show (or 0 for all)
 *
 * need docs here..
 *
 * Return value: a #EBookStatus value.
 **/
gboolean
e_book_get_book_view (EBook       *book,
		      EBookQuery  *query,
		      GList       *requested_fields,
		      int          max_results,
		      EBookView  **book_view,
		      GError     **error)
{
	GNOME_Evolution_Addressbook_stringlist stringlist;
	CORBA_Environment ev;
	EBookOp *our_op;
	EBookStatus status;
	int num_fields, i;
	GList *iter;
	char *query_string;

	e_return_error_if_fail (book && E_IS_BOOK (book),       E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (query,                          E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (book_view,                      E_BOOK_ERROR_INVALID_ARG);

	e_mutex_lock (book->priv->mutex);

	if (book->priv->load_state != E_BOOK_URI_LOADED) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_URI_NOT_LOADED,
			     _("e_book_get_book_view on book before e_book_load_uri"));
		return FALSE;
	}

	if (book->priv->current_op != NULL) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_BUSY,
			     _("book busy"));
		return FALSE;
	}

	our_op = e_book_new_op (book);

	e_mutex_lock (our_op->mutex);

	e_mutex_unlock (book->priv->mutex);

	CORBA_exception_init (&ev);

	our_op->listener = e_book_view_listener_new();

	num_fields = g_list_length (requested_fields);

	stringlist._buffer = CORBA_sequence_CORBA_string_allocbuf (num_fields);
	stringlist._maximum = num_fields;
	stringlist._length = num_fields;

	for (i = 0, iter = requested_fields; iter; iter = iter->next, i ++) {
		stringlist._buffer[i] = CORBA_string_dup ((char*)iter->data);
	}

	query_string = e_book_query_to_string (query);

	/* will eventually end up calling e_book_response_get_book_view */
	GNOME_Evolution_Addressbook_Book_getBookView (book->priv->corba_book,
						      bonobo_object_corba_objref(BONOBO_OBJECT(our_op->listener)),
						      query_string,
						      &stringlist, max_results, &ev);

	CORBA_free(stringlist._buffer);
	g_free (query_string);

	if (ev._major != CORBA_NO_EXCEPTION) {

		e_book_clear_op (book, our_op);

		CORBA_exception_free (&ev);

		g_warning ("corba exception._major = %d\n", ev._major);

		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_CORBA_EXCEPTION,
			     _("Corba exception making Book::getBookView call"));
		return FALSE;
	}
	
	CORBA_exception_free (&ev);

	/* wait for something to happen (both cancellation and a
	   successful response will notity us via our cv */
	e_mutex_cond_wait (&our_op->cond, our_op->mutex);

	status = our_op->status;
	*book_view = our_op->view;

	e_book_clear_op (book, our_op);

	E_BOOK_CHECK_STATUS (status, error);
}

static void
e_book_response_get_book_view (EBook       *book,
			       EBookStatus  status,
			       GNOME_Evolution_Addressbook_BookView corba_book_view)
{

	EBookOp *op;

	printf ("e_book_response_get_book_view\n");

	op = e_book_get_op (book);

	if (op == NULL) {
	  g_warning ("e_book_response_get_book_view: Cannot find operation ");
	  return;
	}

	e_mutex_lock (op->mutex);

	op->status = status;
	op->view = e_book_view_new (corba_book_view, op->listener);

	pthread_cond_signal (&op->cond);

	e_mutex_unlock (op->mutex);
}



/**
 * e_book_get_contacts:
 * @book: an #EBook
 * @query: an #EBookQuery
 *
 * need docs here..
 *
 * Return value: a #EBookStatus value.
 **/
gboolean
e_book_get_contacts (EBook       *book,
		     EBookQuery  *query,
		     GList      **contacts,
		     GError     **error)
{
	CORBA_Environment ev;
	EBookOp *our_op;
	EBookStatus status;
	char *query_string;

	e_return_error_if_fail (book && E_IS_BOOK (book),       E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (query,                          E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (contacts,                       E_BOOK_ERROR_INVALID_ARG);

	e_mutex_lock (book->priv->mutex);

	if (book->priv->load_state != E_BOOK_URI_LOADED) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_URI_NOT_LOADED,
			     _("e_book_get_contacts on book before e_book_load_uri"));
		return FALSE;
	}

	if (book->priv->current_op != NULL) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_BUSY,
			     _("book busy"));
		return FALSE;
	}

	our_op = e_book_new_op (book);

	e_mutex_lock (our_op->mutex);

	e_mutex_unlock (book->priv->mutex);

	CORBA_exception_init (&ev);

	query_string = e_book_query_to_string (query);

	/* will eventually end up calling e_book_response_get_contacts */
	GNOME_Evolution_Addressbook_Book_getContactList (book->priv->corba_book, query_string, &ev);

	g_free (query_string);

	if (ev._major != CORBA_NO_EXCEPTION) {

		e_book_clear_op (book, our_op);

		CORBA_exception_free (&ev);

		g_warning ("corba exception._major = %d\n", ev._major);

		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_CORBA_EXCEPTION,
			     _("Corba exception making Book::getContactList call"));
		return FALSE;
	}
	
	CORBA_exception_free (&ev);

	/* wait for something to happen (both cancellation and a
	   successful response will notity us via our cv */
	e_mutex_cond_wait (&our_op->cond, our_op->mutex);

	status = our_op->status;
	*contacts = our_op->list;

	e_book_clear_op (book, our_op);

	E_BOOK_CHECK_STATUS (status, error);
}

static void
e_book_response_get_contacts (EBook       *book,
			      EBookStatus  status,
			      GList       *contact_list)
{

	EBookOp *op;

	op = e_book_get_op (book);

	if (op == NULL) {
	  g_warning ("e_book_response_get_contacts: Cannot find operation ");
	  return;
	}

	e_mutex_lock (op->mutex);

	op->status = status;
	op->list = contact_list;

	pthread_cond_signal (&op->cond);

	e_mutex_unlock (op->mutex);
}


gboolean
e_book_get_changes (EBook       *book,
		    char        *changeid,
		    GList      **changes,
		    GError     **error)
{
	CORBA_Environment ev;
	EBookOp *our_op;
	EBookStatus status;

	e_return_error_if_fail (book && E_IS_BOOK (book),       E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (changeid,                       E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (changes,                        E_BOOK_ERROR_INVALID_ARG);

	e_mutex_lock (book->priv->mutex);

	if (book->priv->load_state != E_BOOK_URI_LOADED) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_URI_NOT_LOADED,
			     _("e_book_get_changes on book before e_book_load_uri"));
		return FALSE;
	}

	if (book->priv->current_op != NULL) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_BUSY,
			     _("book busy"));
		return FALSE;
	}

	our_op = e_book_new_op (book);

	e_mutex_lock (our_op->mutex);

	e_mutex_unlock (book->priv->mutex);

	CORBA_exception_init (&ev);

	/* will eventually end up calling e_book_response_get_changes */
	GNOME_Evolution_Addressbook_Book_getChanges (book->priv->corba_book, changeid, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {

		e_book_clear_op (book, our_op);

		CORBA_exception_free (&ev);

		g_warning ("corba exception._major = %d\n", ev._major);

		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_CORBA_EXCEPTION,
			     _("Corba exception making Book::getChanges call"));
		return FALSE;
	}
	
	CORBA_exception_free (&ev);

	/* wait for something to happen (both cancellation and a
	   successful response will notity us via our cv */
	e_mutex_cond_wait (&our_op->cond, our_op->mutex);

	status = our_op->status;
	*changes = our_op->list;

	e_book_clear_op (book, our_op);

	E_BOOK_CHECK_STATUS (status, error);
}

static void
e_book_response_get_changes (EBook       *book,
			     EBookStatus  status,
			     GList       *change_list)
{

	EBookOp *op;

	op = e_book_get_op (book);

	if (op == NULL) {
	  g_warning ("e_book_response_get_contacts: Cannot find operation ");
	  return;
	}

	e_mutex_lock (op->mutex);

	op->status = status;
	op->list = change_list;

	pthread_cond_signal (&op->cond);

	e_mutex_unlock (op->mutex);
}

void
e_book_free_change_list (GList *change_list)
{
	GList *l;
	for (l = change_list; l; l = l->next) {
		EBookChange *change = l->data;

		g_free (change->vcard);
		g_free (change);
	}

	g_list_free (change_list);
}



static void
e_book_response_generic (EBook       *book,
			 EBookStatus  status)
{
	EBookOp *op;

	op = e_book_get_op (book);

	if (op == NULL) {
	  g_warning ("e_book_response_generic: Cannot find operation ");
	  return;
	}

	e_mutex_lock (op->mutex);

	op->status = status;

	pthread_cond_signal (&op->cond);

	e_mutex_unlock (op->mutex);
}

/**
 * e_book_cancel:
 * @book: an #EBook
 *
 * Used to cancel an already running operation on @book.  This
 * function makes a synchronous CORBA to the backend telling it to
 * cancel the operation.  If the operation wasn't cancellable (either
 * transiently or permanently) or had already comopleted on the wombat
 * side, this function will return E_BOOK_STATUS_COULD_NOT_CANCEL, and
 * the operation will continue uncancelled.  If the operation could be
 * cancelled, this function will return E_BOOK_ERROR_OK, and the
 * blocked e_book function corresponding to current operation will
 * return with a status of E_BOOK_STATUS_CANCELLED.
 *
 * Return value: a #EBookStatus value.
 **/
gboolean
e_book_cancel (EBook   *book,
	       GError **error)
{
	EBookOp *op;
	EBookStatus status;
	gboolean rv;
	CORBA_Environment ev;

	e_mutex_lock (book->priv->mutex);

	if (book->priv->current_op == NULL) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_COULD_NOT_CANCEL,
			     _("e_book_cancel: there is no current operation"));
		return FALSE;
	}

	op = book->priv->current_op;

	e_mutex_lock (op->mutex);

	e_mutex_unlock (book->priv->mutex);

	status = GNOME_Evolution_Addressbook_Book_cancelOperation(book->priv->corba_book, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {

		e_mutex_unlock (op->mutex);

		CORBA_exception_free (&ev);

		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_CORBA_EXCEPTION,
			     _("Corba exception making Book::cancelOperation call"));
		return FALSE;
	}

	CORBA_exception_free (&ev);

	if (status == E_BOOK_ERROR_OK) {
		op->status = E_BOOK_ERROR_CANCELLED;

		pthread_cond_signal (&op->cond);

		rv = TRUE;
	}
	else {
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_COULD_NOT_CANCEL,
			     _("e_book_cancel: couldn't cancel"));
		rv = FALSE;
	}

	e_mutex_unlock (op->mutex);

	return rv;
}

static void
e_book_response_open (EBook       *book,
		      EBookStatus  status)
{
	EBookOp *op;

	op = e_book_get_op (book);

	if (op == NULL) {
	  g_warning ("e_book_response_open: Cannot find operation ");
	  return;
	}

	e_mutex_lock (op->mutex);

	op->status = status;

	pthread_cond_signal (&op->cond);

	e_mutex_unlock (op->mutex);
}



gboolean
e_book_remove (EBook   *book,
	       GError **error)
{
	CORBA_Environment ev;
	EBookOp *our_op;
	EBookStatus status;

	e_return_error_if_fail (book && E_IS_BOOK (book), E_BOOK_ERROR_INVALID_ARG);

	e_mutex_lock (book->priv->mutex);

	if (book->priv->load_state != E_BOOK_URI_LOADED) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_URI_NOT_LOADED,
			     _("e_book_remove on book before e_book_load_uri"));
		return FALSE;
	}

	if (book->priv->current_op != NULL) {
		e_mutex_unlock (book->priv->mutex);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_BUSY,
			     _("book busy"));
		return FALSE;
	}

	our_op = e_book_new_op (book);

	e_mutex_lock (our_op->mutex);

	e_mutex_unlock (book->priv->mutex);

	CORBA_exception_init (&ev);

	/* will eventually end up calling e_book_response_remove */
	GNOME_Evolution_Addressbook_Book_remove (book->priv->corba_book, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {

		e_book_clear_op (book, our_op);

		CORBA_exception_free (&ev);

		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_CORBA_EXCEPTION,
			     _("Corba exception making Book::remove call"));
		return FALSE;
	}

	CORBA_exception_free (&ev);

	/* wait for something to happen (both cancellation and a
	   successful response will notity us via our cv */
	e_mutex_cond_wait (&our_op->cond, our_op->mutex);

	status = our_op->status;

	e_book_clear_op (book, our_op);

	E_BOOK_CHECK_STATUS (status, error);
}

static void
e_book_response_remove (EBook       *book,
			EBookStatus  status)
{
	EBookOp *op;

	printf ("e_book_response_remove\n");

	op = e_book_get_op (book);

	if (op == NULL) {
	  g_warning ("e_book_response_remove: Cannot find operation ");
	  return;
	}

	e_mutex_lock (op->mutex);

	op->status = status;

	pthread_cond_signal (&op->cond);

	e_mutex_unlock (op->mutex);
}



static void
e_book_handle_response (EBookListener *listener, EBookListenerResponse *resp, EBook *book)
{
	switch (resp->op) {
	case CreateContactResponse:
		e_book_response_add_contact (book, resp->status, resp->id);
		break;
	case RemoveContactResponse:
	case ModifyContactResponse:
	case AuthenticationResponse:
		e_book_response_generic (book, resp->status);
		break;
	case GetContactResponse: {
		EContact *contact = e_contact_new_from_vcard (resp->vcard);
		e_book_response_get_contact (book, resp->status, contact);
		break;
	}
	case GetContactListResponse:
		e_book_response_get_contacts (book, resp->status, resp->list);
		break;
	case GetBookViewResponse:
		e_book_response_get_book_view(book, resp->status, resp->book_view);
		break;
	case GetChangesResponse:
		e_book_response_get_changes(book, resp->status, resp->list);
		break;
	case OpenBookResponse:
		e_book_response_open (book, resp->status);
		break;
	case RemoveBookResponse:
		e_book_response_remove (book, resp->status);
		break;
	case GetSupportedFieldsResponse:
		e_book_response_get_supported_fields (book, resp->status, resp->list);
		break;
	case GetSupportedAuthMethodsResponse:
		e_book_response_get_supported_auth_methods (book, resp->status, resp->list);
		break;
	case WritableStatusEvent:
		book->priv->writable = resp->writable;
		g_signal_emit (book, e_book_signals [WRITABLE_STATUS], 0, resp->writable);
		break;
	default:
		g_error ("EBook: Unknown response code %d!\n",
			 resp->op);
	}
}



gboolean
e_book_unload_uri (EBook   *book,
		   GError **error)
{
	CORBA_Environment ev;

	e_return_error_if_fail (book && E_IS_BOOK (book), E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (book->priv->load_state != E_BOOK_URI_NOT_LOADED, E_BOOK_ERROR_URI_NOT_LOADED);

	/* Release the remote GNOME_Evolution_Addressbook_Book in the PAS. */
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
	book->priv->load_state = E_BOOK_URI_NOT_LOADED;
	g_free (book->priv->cap);
	book->priv->cap = NULL;
	book->priv->writable = FALSE;

	return TRUE;
}



/**
 * e_book_load_uri:
 */

static void
backend_died_cb (EComponentListener *cl, gpointer user_data)
{
	EBook *book = user_data;
                                                                                                                              
	book->priv->load_state = E_BOOK_URI_NOT_LOADED;
        g_signal_emit (book, e_book_signals [BACKEND_DIED], 0);
}

static GList *
activate_factories_for_uri (EBook *book, const char *uri)
{
	CORBA_Environment ev;
	Bonobo_ServerInfoList *info_list = NULL;
	int i;
	char *protocol, *query, *colon;
	GList *factories = NULL;

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
		goto done;
		return NULL;
	}

	if (info_list->_length == 0) {
		g_warning ("Can't find installed BookFactory that handles protocol '%s'.", protocol);
		CORBA_exception_free (&ev);
		goto done;
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
			factories = g_list_append (factories, factory);
	}

 done:
	if (info_list)
		CORBA_free (info_list);
	g_free (query);
	g_free (protocol);

	return factories;
}

gboolean
e_book_load_uri (EBook        *book,
		 const char   *uri,
		 gboolean      only_if_exists,
		 GError      **error)
{
	GList *factories;
	GList *l;
	gboolean rv = FALSE;
	GNOME_Evolution_Addressbook_Book corba_book = CORBA_OBJECT_NIL;

	e_return_error_if_fail (book && E_IS_BOOK (book), E_BOOK_ERROR_INVALID_ARG);
	e_return_error_if_fail (uri,                      E_BOOK_ERROR_INVALID_ARG);

	/* XXX this needs to happen while holding the book's lock i would think... */
	e_return_error_if_fail (book->priv->load_state == E_BOOK_URI_NOT_LOADED, E_BOOK_ERROR_URI_ALREADY_LOADED);

	/* try to find a list of factories that can handle the protocol */
	if (! (factories = activate_factories_for_uri (book, uri))) {
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_PROTOCOL_NOT_SUPPORTED,
			     _("e_book_load_uri: no factories available for uri `%s'"), uri);
		return FALSE;
	}


	book->priv->load_state = E_BOOK_URI_LOADING;

	/*
	 * Create our local BookListener interface.
	 */
	book->priv->listener = e_book_listener_new ();
	if (book->priv->listener == NULL) {
		g_warning ("e_book_load_uri: Could not create EBookListener!\n");
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_OTHER_ERROR,
			     _("e_book_load_uri: Could not create EBookListener"));
		return FALSE;
	}
	book->priv->listener_signal = g_signal_connect (book->priv->listener, "response",
							G_CALLBACK (e_book_handle_response), book);

	g_free (book->priv->uri);
	book->priv->uri = g_strdup (uri);

	for (l = factories; l; l = l->next) {
		GNOME_Evolution_Addressbook_BookFactory factory = l->data;
		EBookOp *our_op;
		CORBA_Environment ev;
		EBookStatus status;

		our_op = e_book_new_op (book);

		e_mutex_lock (our_op->mutex);

		CORBA_exception_init (&ev);

		corba_book = GNOME_Evolution_Addressbook_BookFactory_getBook (factory, book->priv->uri,
								      bonobo_object_corba_objref (BONOBO_OBJECT (book->priv->listener)),
								      &ev);

		if (ev._major != CORBA_NO_EXCEPTION) {

			e_book_clear_op (book, our_op);

			CORBA_exception_free (&ev);
			continue;
		}

		GNOME_Evolution_Addressbook_Book_open (corba_book,
						       only_if_exists,
						       &ev);

		if (ev._major != CORBA_NO_EXCEPTION) {
			/* kill the listener so the book will die */
			g_signal_handler_disconnect (book->priv->listener, book->priv->listener_signal);
			bonobo_object_unref (book->priv->listener);
			book->priv->listener = NULL;

			e_book_clear_op (book, our_op);

			CORBA_exception_free (&ev);
			continue;
		}

		CORBA_exception_free (&ev);

		/* wait for something to happen (both cancellation and a
		   successful response will notity us via our cv */
		e_mutex_cond_wait (&our_op->cond, our_op->mutex);

		status = our_op->status;

		/* remove the op from the book's hash of operations */
		e_book_clear_op (book, our_op);

		if (status == E_BOOK_ERROR_CANCELLED
		    || status == E_BOOK_ERROR_OK) {
			rv = TRUE;
			break;
		}
	}

	/* free up the factories */
	for (l = factories; l; l = l->next)
		CORBA_Object_release ((CORBA_Object)l->data, NULL);

	if (rv == TRUE) {
		book->priv->corba_book = corba_book;
		book->priv->load_state = E_BOOK_URI_LOADED;
		book->priv->comp_listener = e_component_listener_new (book->priv->corba_book);
		book->priv->died_signal = g_signal_connect (book->priv->comp_listener, "component_died",
							    G_CALLBACK (backend_died_cb), book);
		return TRUE;
	}
	else {
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_PROTOCOL_NOT_SUPPORTED,
			     _("e_book_load_uri: no factories available for uri `%s'"), uri);
		return FALSE;
	}
		
	return rv;
}

gboolean
e_book_load_local_addressbook (EBook   *book,
			       GError **error)
{
	char *filename;
	char *uri;
	gboolean rv;

	filename = g_build_filename (g_get_home_dir(),
				     "evolution/local/Contacts",
				     NULL);
	uri = g_strdup_printf ("file://%s", filename);

	g_free (filename);
	
	rv = e_book_load_uri (book, uri, TRUE, error);
	
	g_free (uri);

	return rv;
}

const char *
e_book_get_uri (EBook *book)
{
	return book->priv->uri;
}

const char *
e_book_get_static_capabilities (EBook   *book,
				GError **error)
{
	if (!book->priv->cap_queried) {
		CORBA_Environment ev;
		char *temp;

		CORBA_exception_init (&ev);

		if (book->priv->load_state != E_BOOK_URI_LOADED) {
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

	return book->priv->cap;
}

gboolean
e_book_check_static_capability (EBook *book,
				const char  *cap)
{
	const char *caps = e_book_get_static_capabilities (book, NULL);

	/* XXX this is an inexact test but it works for our use */
	if (caps && strstr (caps, cap))
		return TRUE;

	return FALSE;
}

gboolean
e_book_is_writable (EBook *book)
{
	return book->priv->writable;
}




gboolean
e_book_get_self (EContact **contact, EBook **book, GError **error)
{
	GError *e = NULL;

	if (!e_book_get_default_addressbook (book, &e)) {
		g_propagate_error (error, e);
		return FALSE;
	}

#if notyet
	EBook *b;
	char *self_uri, *self_uid;

	/* XXX get the setting for the self book and self uid from gconf */

	b = e_book_new();
	if (! e_book_load_uri (b, self_uri, TRUE, error)) {
		g_object_unref (b);
		return FALSE;
	}

	if (! e_book_get_contact (b, self_uid,
				  contact, error)) {
		g_object_unref (b);
		return FALSE;
	}

	if (book)
		*book = b;
	else
		g_object_unref (b);
	return TRUE;
#endif
}

gboolean
e_book_set_self (EBook *book, const char *id, GError **error)
{
}



gboolean
e_book_get_default_addressbook (EBook **book, GError **error)
{
	/* XXX for now just load the local ~/evolution/local/Contacts */
	char *path, *uri;
	gboolean rv;

	*book = e_book_new ();

	path = g_build_filename (g_get_home_dir (),
				 "evolution/local/Contacts",
				 NULL);
	uri = g_strdup_printf ("file://%s", path);
	g_free (path);

	rv = e_book_load_uri (*book, uri, FALSE, error);

	g_free (uri);

	if (!rv) {
		g_object_unref (*book);
		*book = NULL;
	}

	return rv;
#if notyet
	EConfigListener *listener = e_config_listener_new ();
	ESourceList *sources = ...;
	ESource *default_source;

	default_source = e_source_list_peek_source_by_uid (sources,
							   "default_");
#endif
}

#if notyet
ESourceList*
e_book_get_addressbooks (GError **error)
{
}
#endif


static void*
startup_mainloop (void *arg)
{
	bonobo_main();
	return NULL;
}

/* one-time start up for libebook */
static void
e_book_activate()
{
	static GStaticMutex e_book_lock = G_STATIC_MUTEX_INIT;
	static gboolean activated = FALSE;

	g_static_mutex_lock (&e_book_lock);
	if (!activated) {
		pthread_t ebook_mainloop_thread;
		activated = TRUE;
		pthread_create(&ebook_mainloop_thread, NULL, startup_mainloop, NULL);
	}
	g_static_mutex_unlock (&e_book_lock);
}



EBook*
e_book_new (void)
{
	e_book_activate ();
	return g_object_new (E_TYPE_BOOK, NULL);
}


static void
e_book_init (EBook *book)
{
	book->priv             = g_new0 (EBookPrivate, 1);
	book->priv->load_state = E_BOOK_URI_NOT_LOADED;
	book->priv->uri        = NULL;
	book->priv->mutex      = e_mutex_new (E_MUTEX_REC);
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

		if (book->priv->load_state == E_BOOK_URI_LOADED)
			e_book_unload_uri (book, NULL);

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
