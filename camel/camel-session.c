/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-session.c : Abstract class for an email session */

/*
 *
 * Author:
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *  Dan Winship <danw@helixcode.com>
 *  Jeffrey Stedfast <fejj@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <config.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gal/util/e-util.h>
#include "camel-session.h"
#include "camel-store.h"
#include "camel-transport.h"
#include "camel-exception.h"
#include "string-utils.h"
#include "camel-url.h"
#include "hash-table-utils.h"

#include "camel-private.h"

static CamelObjectClass *parent_class;

static void
camel_session_init (CamelSession *session)
{
	session->modules = camel_provider_init ();
	session->providers = g_hash_table_new (g_strcase_hash, g_strcase_equal);
	session->priv = g_malloc0(sizeof(*session->priv));
#ifdef ENABLE_THREADS
	session->priv->lock = g_mutex_new();
#endif	
}

static gboolean
camel_session_destroy_provider (gpointer key, gpointer value, gpointer user_data)
{
	CamelProvider *prov = (CamelProvider *)value;

	g_hash_table_destroy (prov->service_cache);

	return TRUE;
}

static void
camel_session_finalise (CamelObject *o)
{
	CamelSession *session = (CamelSession *)o;

	g_free(session->storage_path);
	g_hash_table_foreach_remove (session->providers,
				     camel_session_destroy_provider, NULL);
	g_hash_table_destroy (session->providers);

#ifdef ENABLE_THREADS
	g_mutex_free(session->priv->lock);
#endif	

	g_free(session->priv);
}

static void
camel_session_class_init (CamelSessionClass *camel_session_class)
{
	parent_class = camel_type_get_global_classfuncs (camel_object_get_type ());
}

CamelType
camel_session_get_type (void)
{
	static CamelType camel_session_type = CAMEL_INVALID_TYPE;

	if (camel_session_type == CAMEL_INVALID_TYPE) {
		camel_session_type = camel_type_register (camel_object_get_type (), "CamelSession",
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
 * camel_session_new:
 * @storage_path: Path to a directory Camel can use for persistent storage.
 * (This directory must already exist.)
 * @authenticator: A callback for discussing authentication information
 * @registrar: A callback for registering timeout callbacks
 * @remove: A callback for removing timeout callbacks
 *
 * This creates a new CamelSession object, which represents global state
 * for the Camel library, and contains callbacks that can be used to
 * interact with the main application.
 *
 * Return value: the new CamelSession
 **/
CamelSession *
camel_session_new (const char *storage_path,
		   CamelAuthCallback authenticator,
		   CamelTimeoutRegisterCallback registrar,
		   CamelTimeoutRemoveCallback remover)
{
	CamelSession *session = CAMEL_SESSION (camel_object_new (CAMEL_SESSION_TYPE));

	session->storage_path = g_strdup (storage_path);
	session->authenticator = authenticator;
	session->registrar = registrar;
	session->remover = remover;
	return session;
}

/**
 * camel_session_register_provider:
 * @session: a session object
 * @protocol: the protocol the provider provides for
 * @provider: provider object
 *
 * Registers a protocol to provider mapping for the session.
 *
 * Assumes the session lock has already been obtained,
 * which is the case for automatically loaded provider modules.
 **/
void
camel_session_register_provider (CamelSession *session,
				 CamelProvider *provider)
{
	g_return_if_fail (CAMEL_IS_SESSION (session));
	g_return_if_fail (provider != NULL);

	g_hash_table_insert (session->providers, provider->protocol, provider);
}

static void
ensure_loaded (gpointer key, gpointer value, gpointer user_data)
{
	CamelSession *session = user_data;
	char *name = key;
	char *path = value;

	if (!g_hash_table_lookup (session->providers, name)) {
		CamelException ex;

		camel_exception_init (&ex);
		camel_provider_load (session, path, &ex);
		camel_exception_clear (&ex);
	}
}

static gint
provider_compare (gconstpointer a, gconstpointer b)
{
	const CamelProvider *cpa = (const CamelProvider *)a;
	const CamelProvider *cpb = (const CamelProvider *)b;

	return strcmp (cpa->name, cpb->name);
}

static void
add_to_list (gpointer key, gpointer value, gpointer user_data)
{
	GList **list = user_data;
	CamelProvider *prov = value;

	*list = g_list_insert_sorted (*list, prov, provider_compare);
}

/**
 * camel_session_list_providers:
 * @session: the session
 * @load: whether or not to load in providers that are not already loaded
 *
 * This returns a list of available providers in this session. If @load
 * is %TRUE, it will first load in all available providers that haven't
 * yet been loaded.
 *
 * Return value: a GList of providers, which the caller must free.
 **/
GList *
camel_session_list_providers (CamelSession *session, gboolean load)
{
	GList *list;

	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);

	CAMEL_SESSION_LOCK(session, lock);

	if (load)
		g_hash_table_foreach (session->modules, ensure_loaded, session);

	list = NULL;
	g_hash_table_foreach (session->providers, add_to_list, &list);

	CAMEL_SESSION_UNLOCK(session, lock);

	return list;
}

static void
service_cache_remove (CamelService *service, gpointer event_data, gpointer user_data)
{
	CamelProvider *provider;
	CamelSession *session = CAMEL_SESSION (user_data);

	g_return_if_fail (CAMEL_IS_SESSION (session));
	g_return_if_fail (service != NULL);
	g_return_if_fail (service->url != NULL);
	
	CAMEL_SESSION_LOCK(session, lock);

	provider = g_hash_table_lookup (session->providers, service->url->protocol);
	g_hash_table_remove (provider->service_cache, service->url);

	CAMEL_SESSION_UNLOCK(session, lock);
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
	CamelURL *url;
	CamelProvider *provider;
	CamelService *service;

	url = camel_url_new (url_string, ex);
	if (!url)
		return NULL;

	/* We need to look up the provider so we can then lookup
	   the service in the provider's cache */
	CAMEL_SESSION_LOCK(session, lock);
	provider = g_hash_table_lookup (session->providers, url->protocol);
	if (!provider) {
		/* See if there's one we can load. */
		char *path;

		path = g_hash_table_lookup (session->modules, url->protocol);
		if (path) {
			camel_provider_load (session, path, ex);
			if (camel_exception_get_id (ex) !=
			    CAMEL_EXCEPTION_NONE) {
				camel_url_free (url);
				CAMEL_SESSION_UNLOCK(session, lock);
				return NULL;
			}
		}
		provider = g_hash_table_lookup (session->providers, url->protocol);
	}

	if (!provider || !provider->object_types[type]) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      _("No provider available for protocol `%s'"),
				      url->protocol);
		camel_url_free (url);
		CAMEL_SESSION_UNLOCK(session, lock);
		return NULL;
	}
	
	/* Now look up the service in the provider's cache */
	service = g_hash_table_lookup (provider->service_cache, url);
	if (service != NULL) {
		camel_url_free (url);
		camel_object_ref (CAMEL_OBJECT (service));
		CAMEL_SESSION_UNLOCK(session, lock);
		return service;
	}

	service = camel_service_new (provider->object_types[type], session, provider, url, ex);
	if (service) {
		g_hash_table_insert (provider->service_cache, url, service);
		camel_object_hook_event (CAMEL_OBJECT (service), "finalize", (CamelObjectEventHookFunc) service_cache_remove, session);
	}
	CAMEL_SESSION_UNLOCK(session, lock);

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

	if (svc->connected == FALSE) {
		if (camel_service_connect (svc, ex) == FALSE) {
			camel_object_unref (CAMEL_OBJECT (svc));
			return NULL;
		}
	}

	return svc;
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
	char *path, *p;

	p = camel_service_get_path (service);
	path = g_strdup_printf ("%s/%s", session->storage_path, p);
	g_free (p);

	if (access (path, F_OK) == 0)
		return path;

	if (e_mkdir_hier (path, S_IRWXU) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create directory %s:\n%s"),
				      path, g_strerror (errno));
		g_free (path);
		return NULL;
	}

	return path;
}

/**
 * camel_session_query_authenticator: query the session authenticator
 * @session: session object
 * @mode: %CAMEL_AUTHENTICATOR_ASK or %CAMEL_AUTHENTICATOR_TELL
 * @data: prompt to query user with, or data to cache
 * @secret: whether or not the data is secret (eg, a password)
 * @service: the service this query is being made by
 * @item: an identifier, unique within this service, for the information
 * @ex: a CamelException
 *
 * This function is used by a CamelService to discuss authentication
 * information with the application.
 *
 * @service and @item together uniquely identify the piece of data the
 * caller is concerned with.
 *
 * If @mode is %CAMEL_AUTHENTICATOR_ASK, then @data is a question to
 * ask the user (if the application doesn't already have the answer
 * cached). If @secret is set, the user's input should not be echoed
 * back. The authenticator should set @ex to
 * %CAMEL_EXCEPTION_USER_CANCEL if the user did not provide the
 * information. The caller must g_free() the information returned when
 * it is done with it.
 *
 * If @mode is %CAMEL_AUTHENTICATOR_TELL, then @data is information
 * that the application should cache, or %NULL if it should stop
 * caching anything about that datum (eg, because the data is a
 * password that turned out to be incorrect).
 *
 * Return value: the authentication information or %NULL.
 **/
char *
camel_session_query_authenticator (CamelSession *session,
				   CamelAuthCallbackMode mode,
				   char *prompt, gboolean secret,
				   CamelService *service, char *item,
				   CamelException *ex)
{
	return session->authenticator (mode, prompt, secret,
				       service, item, ex);
}

/**
 * camel_session_register_timeout: Register a timeout to be called
 * periodically.
 *
 * @session: the CamelSession
 * @interval: the number of milliseconds interval between calls
 * @callback: the function to call
 * @user_data: extra data to be passed to the callback
 *
 * This function will use the registrar callback provided upon
 * camel_session_new to register the timeout. The callback will
 * be called every @interval milliseconds until it returns @FALSE.
 * It will be passed one argument, @user_data.
 *
 * Returns a nonzero handle that can be used with 
 * camel_session_remove_timeout on success, and 0 on failure to 
 * register the timeout.
 **/
guint
camel_session_register_timeout (CamelSession *session,
				guint32 interval,
				CamelTimeoutCallback callback,
				gpointer user_data)
{
	g_return_val_if_fail (CAMEL_IS_SESSION (session), FALSE);

	return session->registrar (interval, callback, user_data);
}

/**
 * camel_session_remove_timeout: Remove a previously registered
 * timeout.
 *
 * @session: the CamelSession
 * @handle: a value returned from camel_session_register_timeout
 *
 * This function will use the remover callback provided upon
 * camel_session_new to remove the timeout.
 *
 * Returns TRUE on success and FALSE on failure.
 **/
gboolean
camel_session_remove_timeout (CamelSession *session, guint handle)
{
	return session->remover (handle);
}

/* ********************************************************************** */

struct _CamelCancel {
	pthread_t id;		/* id of running thread */
	guint32 flags;		/* cancelled ? */
	int blocked;		/* cancellation blocked depth */
	int refcount;
#ifdef ENABLE_THREADS
	EMsgPort *cancel_port;
	int cancel_fd;
	pthread_mutex_t lock;
#endif
};

#define CAMEL_CANCEL_CANCELLED (1<<0)

#ifdef ENABLE_THREADS
#define CAMEL_CANCEL_LOCK(cc) pthread_mutex_lock(&cc->lock)
#define CAMEL_CANCEL_UNLOCK(cc) pthread_mutex_lock(&cc->lock)
#define CAMEL_ACTIVE_LOCK() pthread_mutex_lock(&cancel_active_lock)
#define CAMEL_ACTIVE_UNLOCK() pthread_mutex_lock(&cancel_active_lock)
static pthread_mutex_t cancel_active_lock = PTHREAD_MUTEX_INITIALIZER;
#else
#define CAMEL_CANCEL_LOCK(cc)
#define CAMEL_CANCEL_UNLOCK(cc)
#define CAMEL_ACTIVE_LOCK()
#define CAMEL_ACTIVE_UNLOCK()
#endif

static GHashTable *cancel_active;

typedef struct _CamelCancelMsg {
	EMsg msg;
} CamelCancelMsg ;

/* creates a new cancel handle */
CamelCancel *camel_cancel_new(void)
{
	CamelCancel *cc;

	cc = g_malloc0(sizeof(*cc));

	cc->flags = 0;
	cc->blocked = 0;
	cc->refcount = 1;
#ifdef ENABLE_THREADS
	cc->id = ~0;
	cc->cancel_port = e_msgport_new();
	cc->cancel_fd = e_msgport_fd(cc->cancel_port);
	pthread_mutex_init(&cc->lock, NULL);
#endif

	return cc;
}

void camel_cancel_reset(CamelCancel *cc)
{
#ifdef ENABLE_THREADS
	CamelCancelMsg *msg;

	while ((msg = (CamelCancelMsg *)e_msgport_get(cc->cancel_port)))
		g_free(msg);
#endif

	cc->flags = 0;
	cc->blocked = 0;
}

void camel_cancel_ref(CamelCancel *cc)
{
	CAMEL_CANCEL_LOCK(cc);
	cc->refcount++;
	CAMEL_CANCEL_UNLOCK(cc);
}

void camel_cancel_unref(CamelCancel *cc)
{
#ifdef ENABLE_THREADS
	CamelCancelMsg *msg;

	if (cc->refcount == 1) {
		while ((msg = (CamelCancelMsg *)e_msgport_get(cc->cancel_port)))
			g_free(msg);

		e_msgport_destroy(cc->cancel_port);
#endif
		g_free(cc);
	} else {
		CAMEL_CANCEL_LOCK(cc);
		cc->refcount--;
		CAMEL_CANCEL_UNLOCK(cc);
	}
}

/* block cancellation */
void camel_cancel_block(CamelCancel *cc)
{
	CAMEL_CANCEL_LOCK(cc);

	cc->blocked++;

	CAMEL_CANCEL_UNLOCK(cc);
}

/* unblock cancellation */
void camel_cancel_unblock(CamelCancel *cc)
{
	CAMEL_CANCEL_LOCK(cc);

	cc->blocked--;

	CAMEL_CANCEL_UNLOCK(cc);
}

/* cancels an operation */
void camel_cancel_cancel(CamelCancel *cc)
{
	CamelCancelMsg *msg;

	if ((cc->flags & CAMEL_CANCEL_CANCELLED) == 0) {
		CAMEL_CANCEL_LOCK(cc);
		msg = g_malloc0(sizeof(*msg));
		e_msgport_put(cc->cancel_port, (EMsg *)msg);
		cc->flags |= CAMEL_CANCEL_CANCELLED;
		CAMEL_CANCEL_UNLOCK(cc);
	}
}

/* register a thread for cancellation */
void camel_cancel_register(CamelCancel *cc)
{
	pthread_t id = pthread_self();

	CAMEL_ACTIVE_LOCK();

	if (cancel_active == NULL)
		cancel_active = g_hash_table_new(NULL, NULL);

	if (cc == NULL) {
		cc = g_hash_table_lookup(cancel_active, (void *)id);
		if (cc == NULL) {
			cc = camel_cancel_new();
		}
	}

	cc->id = id;
	g_hash_table_insert(cancel_active, (void *)id, cc);
	camel_cancel_ref(cc);

	CAMEL_ACTIVE_UNLOCK();
}

/* remove a thread from being able to be cancelled */
void camel_cancel_unregister(CamelCancel *cc)
{
	CAMEL_ACTIVE_LOCK();

	if (cancel_active == NULL)
		cancel_active = g_hash_table_new(NULL, NULL);

	if (cc == NULL) {
		cc = g_hash_table_lookup(cancel_active, (void *)cc->id);
		if (cc == NULL) {
			g_warning("Trying to unregister a thread that was never registered for cancellation");
		}
	}

	if (cc)
		g_hash_table_remove(cancel_active, (void *)cc->id);

	CAMEL_ACTIVE_UNLOCK();

	if (cc)
		camel_cancel_unref(cc);
}

/* test for cancellation */
gboolean camel_cancel_check(CamelCancel *cc)
{
	CamelCancelMsg *msg;

	if (cc == NULL) {
		if (cancel_active) {
			CAMEL_ACTIVE_LOCK();
			cc = g_hash_table_lookup(cancel_active, (void *)pthread_self());
			CAMEL_ACTIVE_UNLOCK();
		}
		if (cc == NULL)
			return FALSE;
	}

	if (cc->blocked > 0)
		return FALSE;

	if (cc->flags & CAMEL_CANCEL_CANCELLED)
		return TRUE;

	msg = (CamelCancelMsg *)e_msgport_get(cc->cancel_port);
	if (msg) {
		CAMEL_CANCEL_LOCK(cc);
		cc->flags |= CAMEL_CANCEL_CANCELLED;
		CAMEL_CANCEL_UNLOCK(cc);
		return TRUE;
	}
	return FALSE;
}

/* get the fd for cancellation waiting */
int camel_cancel_fd(CamelCancel *cc)
{
	if (cc == NULL) {
		if (cancel_active) {
			CAMEL_ACTIVE_LOCK();
			cc = g_hash_table_lookup(cancel_active, (void *)pthread_self());
			CAMEL_ACTIVE_UNLOCK();
		}
		if (cc == NULL)
			return -1;
	}
	if (cc->blocked)
		return -1;

	return cc->cancel_fd;
}

