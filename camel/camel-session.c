/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-session.c : Abstract class for an email session */

/*
 * Authors:
 *  Dan Winship <danw@ximian.com>
 *  Jeffrey Stedfast <fejj@ximian.com>
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999 - 2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "camel-session.h"
#include "camel-store.h"
#include "camel-transport.h"
#include "camel-exception.h"
#include "camel-file-utils.h"
#include "camel-string-utils.h"
#include "camel-url.h"

#include "camel-private.h"

#define d(x)

#define CS_CLASS(so) ((CamelSessionClass *)((CamelObject *)so)->klass)

static CamelService *get_service (CamelSession *session,
				  const char *url_string,
				  CamelProviderType type,
				  CamelException *ex);
static char *get_storage_path (CamelSession *session,
			       CamelService *service,
			       CamelException *ex);

static void *session_thread_msg_new(CamelSession *session, CamelSessionThreadOps *ops, unsigned int size);
static void session_thread_msg_free(CamelSession *session, CamelSessionThreadMsg *msg);
static int session_thread_queue(CamelSession *session, CamelSessionThreadMsg *msg, int flags);
static void session_thread_wait(CamelSession *session, int id);
static void session_thread_status(CamelSession *session, CamelSessionThreadMsg *msg, const char *text, int pc);

static void
camel_session_init (CamelSession *session)
{
	session->online = TRUE;
	session->priv = g_malloc0(sizeof(*session->priv));
	
	session->priv->lock = g_mutex_new();
	session->priv->thread_lock = g_mutex_new();
	session->priv->thread_id = 1;
	session->priv->thread_active = g_hash_table_new(NULL, NULL);
	session->priv->thread_queue = NULL;
}

static void
camel_session_finalise (CamelObject *o)
{
	CamelSession *session = (CamelSession *)o;
	
	g_hash_table_destroy(session->priv->thread_active);
	if (session->priv->thread_queue)
		e_thread_destroy(session->priv->thread_queue);

	g_free(session->storage_path);
	
	g_mutex_free(session->priv->lock);
	g_mutex_free(session->priv->thread_lock);
	
	g_free(session->priv);
}

static void
camel_session_class_init (CamelSessionClass *camel_session_class)
{
	/* virtual method definition */
	camel_session_class->get_service = get_service;
	camel_session_class->get_storage_path = get_storage_path;
	
	camel_session_class->thread_msg_new = session_thread_msg_new;
	camel_session_class->thread_msg_free = session_thread_msg_free;
	camel_session_class->thread_queue = session_thread_queue;
	camel_session_class->thread_wait = session_thread_wait;
	camel_session_class->thread_status = session_thread_status;
}

CamelType
camel_session_get_type (void)
{
	static CamelType camel_session_type = CAMEL_INVALID_TYPE;

	if (camel_session_type == CAMEL_INVALID_TYPE) {
		camel_session_type = camel_type_register (
			camel_object_get_type (), "CamelSession",
			sizeof (CamelSession),
			sizeof (CamelSessionClass),
			(CamelObjectClassInitFunc) camel_session_class_init,
			NULL,
			(CamelObjectInitFunc) camel_session_init,
			(CamelObjectFinalizeFunc) camel_session_finalise);
	}

	return camel_session_type;
}

/**
 * camel_session_construct:
 * @session: a session object to construct
 * @storage_path: path to a directory the session can use for
 * persistent storage. (This directory must already exist.)
 *
 * Constructs @session.
 **/
void
camel_session_construct (CamelSession *session, const char *storage_path)
{
	session->storage_path = g_strdup (storage_path);
}

static CamelService *
get_service (CamelSession *session, const char *url_string,
	     CamelProviderType type, CamelException *ex)
{
	CamelURL *url;
	CamelProvider *provider;
	CamelService *service;
	CamelException internal_ex;
	
	url = camel_url_new (url_string, ex);
	if (!url)
		return NULL;

	/* We need to look up the provider so we can then lookup
	   the service in the provider's cache */
	provider = camel_provider_get(url->protocol, ex);
	if (provider && !provider->object_types[type]) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      _("No provider available for protocol `%s'"),
				      url->protocol);
		provider = NULL;
	}
	
	if (!provider) {
		camel_url_free (url);
		return NULL;
	}

	/* If the provider doesn't use paths but the URL contains one,
	 * ignore it.
	 */
	if (url->path && !CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_PATH))
		camel_url_set_path (url, NULL);
	
	/* Now look up the service in the provider's cache */
	service = camel_object_bag_reserve(provider->service_cache[type], url);
	if (service == NULL) {
		service = (CamelService *)camel_object_new (provider->object_types[type]);
		camel_exception_init (&internal_ex);
		camel_service_construct (service, session, provider, url, &internal_ex);
		if (camel_exception_is_set (&internal_ex)) {
			camel_exception_xfer (ex, &internal_ex);
			camel_object_unref (service);
			service = NULL;
			camel_object_bag_abort(provider->service_cache[type], url);
		} else {
			camel_object_bag_add(provider->service_cache[type], url, service);
		}
	}

	camel_url_free (url);

	return service;
}

/**
 * camel_session_get_service:
 * @session: the CamelSession
 * @url_string: a Camel URL describing the service to get
 * @type: the provider type (%CAMEL_PROVIDER_STORE or
 * %CAMEL_PROVIDER_TRANSPORT) to get, since some URLs may be able
 * to specify either type.
 * @ex: a CamelException
 *
 * This resolves a CamelURL into a CamelService, including loading the
 * provider library for that service if it has not already been loaded.
 *
 * Services are cached, and asking for "the same" @url_string multiple
 * times will return the same CamelService (with its reference count
 * incremented by one each time). What constitutes "the same" URL
 * depends in part on the provider.
 *
 * Return value: the requested CamelService, or %NULL
 **/
CamelService *
camel_session_get_service (CamelSession *session, const char *url_string,
			   CamelProviderType type, CamelException *ex)
{
	CamelService *service;

	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);
	g_return_val_if_fail (url_string != NULL, NULL);

	CAMEL_SESSION_LOCK (session, lock);
	service = CS_CLASS (session)->get_service (session, url_string, type, ex);
	CAMEL_SESSION_UNLOCK (session, lock);

	return service;
}

/**
 * camel_session_get_service_connected:
 * @session: the CamelSession
 * @url_string: a Camel URL describing the service to get
 * @type: the provider type
 * @ex: a CamelException
 *
 * This works like camel_session_get_service(), but also ensures that
 * the returned service will have been successfully connected (via
 * camel_service_connect().)
 *
 * Return value: the requested CamelService, or %NULL
 **/
CamelService *
camel_session_get_service_connected (CamelSession *session,
				     const char *url_string,
				     CamelProviderType type,
				     CamelException *ex)
{
	CamelService *svc;

	svc = camel_session_get_service (session, url_string, type, ex);
	if (svc == NULL)
		return NULL;

	if (svc->status != CAMEL_SERVICE_CONNECTED) {
		if (camel_service_connect (svc, ex) == FALSE) {
			camel_object_unref (svc);
			return NULL;
		}
	}

	return svc;
}


static char *
get_storage_path (CamelSession *session, CamelService *service, CamelException *ex)
{
	char *path, *p;

	p = camel_service_get_path (service);
	path = g_strdup_printf ("%s/%s", session->storage_path, p);
	g_free (p);

	if (access (path, F_OK) == 0)
		return path;

	if (camel_mkdir (path, S_IRWXU) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create directory %s:\n%s"),
				      path, g_strerror (errno));
		g_free (path);
		return NULL;
	}

	return path;
}

/**
 * camel_session_get_storage_path:
 * @session: session object
 * @service: a CamelService
 * @ex: a CamelException
 *
 * This returns the path to a directory which the service can use for
 * its own purposes. Data stored there will remain between Evolution
 * sessions. No code outside of that service should ever touch the
 * files in this directory. If the directory does not exist, it will
 * be created.
 *
 * Return value: the path (which the caller must free), or %NULL if
 * an error occurs.
 **/
char *
camel_session_get_storage_path (CamelSession *session, CamelService *service,
				CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), NULL);

	return CS_CLASS (session)->get_storage_path (session, service, ex);
}


/**
 * camel_session_get_password:
 * @session: session object
 * @service: the service this query is being made by
 * @domain: domain of password request.  May be null to use the default.
 * @prompt: prompt to provide to user
 * @item: an identifier, unique within this service, for the information
 * @flags: CAMEL_SESSION_PASSWORD_REPROMPT, the prompt should force a reprompt
 * CAMEL_SESSION_PASSWORD_SECRET, whether the password is secret
 * CAMEL_SESSION_PASSWORD_STATIC, the password is remembered externally
 * @ex: a CamelException
 *
 * This function is used by a CamelService to ask the application and
 * the user for a password or other authentication data.
 *
 * @service and @item together uniquely identify the piece of data the
 * caller is concerned with.
 *
 * @prompt is a question to ask the user (if the application doesn't
 * already have the answer cached). If CAMEL_SESSION_PASSWORD_SECRET
 * is set, the user's input will not be echoed back.
 *
 * If CAMEL_SESSION_PASSWORD_STATIC is set, it means the password returned
 * will be stored statically by the caller automatically, for the current
 * session.
 * 
 * The authenticator
 * should set @ex to %CAMEL_EXCEPTION_USER_CANCEL if the user did not
 * provide the information. The caller must g_free() the information
 * returned when it is done with it.
 *
 * Return value: the authentication information or %NULL.
 **/
char *
camel_session_get_password (CamelSession *session, CamelService *service,
			    const char *domain, const char *prompt, const char *item,
			    guint32 flags,
			    CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);
	g_return_val_if_fail (prompt != NULL, NULL);
	g_return_val_if_fail (item != NULL, NULL);
	
	return CS_CLASS (session)->get_password (session, service, domain, prompt, item, flags, ex);
}


/**
 * camel_session_forget_password:
 * @session: session object
 * @service: the service rejecting the password
 * @item: an identifier, unique within this service, for the information
 * @ex: a CamelException
 *
 * This function is used by a CamelService to tell the application
 * that the authentication information it provided via
 * camel_session_get_password was rejected by the service. If the
 * application was caching this information, it should stop,
 * and if the service asks for it again, it should ask the user.
 *
 * @service and @item identify the rejected authentication information,
 * as with camel_session_get_password.
 **/
void
camel_session_forget_password (CamelSession *session, CamelService *service,
			       const char *domain, const char *item, CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_SESSION (session));
	g_return_if_fail (item != NULL);

	CS_CLASS (session)->forget_password (session, service, domain, item, ex);
}


/**
 * camel_session_alert_user:
 * @session: session object
 * @type: the type of alert (info, warning, or error)
 * @prompt: the message for the user
 * @cancel: whether or not to provide a "Cancel" option in addition to
 * an "OK" option.
 *
 * Presents the given @prompt to the user, in the style indicated by
 * @type. If @cancel is %TRUE, the user will be able to accept or
 * cancel. Otherwise, the message is purely informational.
 *
 * Return value: %TRUE if the user accepts, %FALSE if they cancel.
 */
gboolean
camel_session_alert_user (CamelSession *session, CamelSessionAlertType type,
			  const char *prompt, gboolean cancel)
{
	g_return_val_if_fail (CAMEL_IS_SESSION (session), FALSE);
	g_return_val_if_fail (prompt != NULL, FALSE);

	return CS_CLASS (session)->alert_user (session, type, prompt, cancel);
}


/**
 * camel_session_is_online:
 * @session: the session.
 *
 * Return value: whether or not @session is online.
 **/
gboolean
camel_session_is_online (CamelSession *session)
{
	return session->online;
}


/**
 * camel_session_set_online:
 * @session: the session
 * @online: whether or not the session should be online
 *
 * Sets the online status of @session to @online.
 **/
void
camel_session_set_online (CamelSession *session, gboolean online)
{
	session->online = online;
}


/**
 * camel_session_get_filter_driver:
 * @session: the session
 * @type: the type of filter (eg, "incoming")
 * @ex: a CamelException
 *
 * Return value: a filter driver, loaded with applicable rules
 **/
CamelFilterDriver *
camel_session_get_filter_driver (CamelSession *session,
				 const char *type,
				 CamelException *ex)
{
	return CS_CLASS (session)->get_filter_driver (session, type, ex);
}

static void
cs_thread_status(CamelOperation *op, const char *what, int pc, void *data)
{
	CamelSessionThreadMsg *m = data;

	CS_CLASS(m->session)->thread_status(m->session, m, what, pc);
}

static void *session_thread_msg_new(CamelSession *session, CamelSessionThreadOps *ops, unsigned int size)
{
	CamelSessionThreadMsg *m;

	g_assert(size >= sizeof(*m));

	m = g_malloc0(size);
	m->ops = ops;
	m->session = session;
	camel_object_ref(session);
	m->op = camel_operation_new(cs_thread_status, m);
	camel_exception_init(&m->ex);
	CAMEL_SESSION_LOCK(session, thread_lock);
	m->id = session->priv->thread_id++;
	g_hash_table_insert(session->priv->thread_active, GINT_TO_POINTER(m->id), m);
	CAMEL_SESSION_UNLOCK(session, thread_lock);

	return m;
}

static void session_thread_msg_free(CamelSession *session, CamelSessionThreadMsg *msg)
{
	g_assert(msg->ops != NULL);

	d(printf("free message %p session %p\n", msg, session));

	CAMEL_SESSION_LOCK(session, thread_lock);
	g_hash_table_remove(session->priv->thread_active, GINT_TO_POINTER(msg->id));
	CAMEL_SESSION_UNLOCK(session, thread_lock);

	d(printf("free msg, ops->free = %p\n", msg->ops->free));
	
	if (msg->ops->free)
		msg->ops->free(session, msg);
	if (msg->op)
		camel_operation_unref(msg->op);
	camel_exception_clear(&msg->ex);
	camel_object_unref(msg->session);
	g_free(msg);
}

static void session_thread_destroy(EThread *thread, CamelSessionThreadMsg *msg, CamelSession *session)
{
	d(printf("destroy message %p session %p\n", msg, session));
	camel_session_thread_msg_free(session, msg);
}

static void session_thread_received(EThread *thread, CamelSessionThreadMsg *msg, CamelSession *session)
{
	d(printf("receive message %p session %p\n", msg, session));
	if (msg->ops->receive) {
		CamelOperation *oldop;

		oldop = camel_operation_register(msg->op);
		msg->ops->receive(session, msg);
		camel_operation_register(oldop);
	}
}

static int session_thread_queue(CamelSession *session, CamelSessionThreadMsg *msg, int flags)
{
	int id;

	CAMEL_SESSION_LOCK(session, thread_lock);
	if (session->priv->thread_queue == NULL) {
		session->priv->thread_queue = e_thread_new(E_THREAD_QUEUE);
		e_thread_set_msg_destroy(session->priv->thread_queue, (EThreadFunc)session_thread_destroy, session);
		e_thread_set_msg_received(session->priv->thread_queue, (EThreadFunc)session_thread_received, session);
	}
	CAMEL_SESSION_UNLOCK(session, thread_lock);

	id = msg->id;
	e_thread_put(session->priv->thread_queue, &msg->msg);

	return id;
}

static void session_thread_wait(CamelSession *session, int id)
{
	int wait;

	/* we just busy wait, only other alternative is to setup a reply port? */
	do {
		CAMEL_SESSION_LOCK(session, thread_lock);
		wait = g_hash_table_lookup(session->priv->thread_active, GINT_TO_POINTER(id)) != NULL;
		CAMEL_SESSION_UNLOCK(session, thread_lock);
		if (wait) {
			usleep(20000);
		}
	} while (wait);
}

static void session_thread_status(CamelSession *session, CamelSessionThreadMsg *msg, const char *text, int pc)
{
}

/**
 * camel_session_thread_msg_new:
 * @session: 
 * @ops: 
 * @size: 
 * 
 * Create a new thread message, using ops as the receive/reply/free
 * ops, of @size bytes.
 *
 * @ops points to the operations used to recieve/process and finally
 * free the message.
 **/
void *camel_session_thread_msg_new(CamelSession *session, CamelSessionThreadOps *ops, unsigned int size)
{
	g_assert(CAMEL_IS_SESSION(session));
	g_assert(ops != NULL);
	g_assert(size >= sizeof(CamelSessionThreadMsg));
		 
	return CS_CLASS (session)->thread_msg_new(session, ops, size);
}

/**
 * camel_session_thread_msg_free:
 * @session: 
 * @msg: 
 * 
 * Free a @msg.  Note that the message must have been allocated using
 * msg_new, and must nto have been submitted to any queue function.
 **/
void camel_session_thread_msg_free(CamelSession *session, CamelSessionThreadMsg *msg)
{
	g_assert(CAMEL_IS_SESSION(session));
	g_assert(msg != NULL);
	g_assert(msg->ops != NULL);

	CS_CLASS (session)->thread_msg_free(session, msg);
}

/**
 * camel_session_thread_queue:
 * @session: 
 * @msg: 
 * @flags: queue type flags, currently 0.
 * 
 * Queue a thread message in another thread for processing.
 * The operation should be (but needn't) run in a queued manner
 * with other operations queued in this manner.
 * 
 * Return value: The id of the operation queued.
 **/
int camel_session_thread_queue(CamelSession *session, CamelSessionThreadMsg *msg, int flags)
{
	g_assert(CAMEL_IS_SESSION(session));
	g_assert(msg != NULL);

	return CS_CLASS (session)->thread_queue(session, msg, flags);
}

/**
 * camel_session_thread_wait:
 * @session: 
 * @id: 
 * 
 * Wait on an operation to complete (by id).
 **/
void camel_session_thread_wait(CamelSession *session, int id)
{
	g_assert(CAMEL_IS_SESSION(session));
	
	if (id == -1)
		return;

	CS_CLASS (session)->thread_wait(session, id);
}

/**
 * camel_session_check_junk:
 * @session: 
 * 
 * Do we have to check incoming messages to be junk?
 **/
gboolean
camel_session_check_junk (CamelSession *session)
{
	g_assert(CAMEL_IS_SESSION(session));

	return session->check_junk;
}

/**
 * camel_session_set_check_junk:
 * @session: 
 * @check_junk: 
 * 
 * Set check_junk flag, if set, incoming mail will be checked for being junk.
 **/
void
camel_session_set_check_junk (CamelSession *session, gboolean check_junk)
{
	g_assert(CAMEL_IS_SESSION(session));

	session->check_junk = check_junk;
}
