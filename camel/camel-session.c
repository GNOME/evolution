/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-session.c : Abstract class for an email session */

/*
 * Authors:
 *  Dan Winship <danw@ximian.com>
 *  Jeffrey Stedfast <fejj@ximian.com>
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999 - 2001 Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "camel-session.h"
#include "camel-store.h"
#include "camel-transport.h"
#include "camel-exception.h"
#include "string-utils.h"
#include "camel-url.h"
#include "hash-table-utils.h"
#include "camel-vee-store.h"

#include "camel-private.h"

#define CS_CLASS(so) CAMEL_SESSION_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static void register_provider (CamelSession *session, CamelProvider *provider);
static GList *list_providers (CamelSession *session, gboolean load);
static CamelProvider *get_provider (CamelSession *session,
				    const char *url_string,
				    CamelException *ex);

static CamelService *get_service (CamelSession *session,
				  const char *url_string,
				  CamelProviderType type,
				  CamelException *ex);
static char *get_storage_path (CamelSession *session,
			       CamelService *service,
			       CamelException *ex);


/* The vfolder provider is always avilable */
static CamelProvider vee_provider = {
	"vfolder",
	N_("Virtual folder email provider"),

	N_("For reading mail as a query of another set of folders"),

	"vfolder",

	0, /* flags */

	0, /* url_flags */

	/* ... */
};

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
	/* virtual method definition */
	camel_session_class->register_provider = register_provider;
	camel_session_class->list_providers = list_providers;
	camel_session_class->get_provider = get_provider;
	camel_session_class->get_service = get_service;
	camel_session_class->get_storage_path = get_storage_path;

	vee_provider.object_types[CAMEL_PROVIDER_STORE] = camel_vee_store_get_type ();
	vee_provider.service_cache = g_hash_table_new (camel_url_hash, camel_url_equal);
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
	camel_session_register_provider(session, &vee_provider);
}


static void 
register_provider (CamelSession *session, CamelProvider *provider)
{
	g_hash_table_insert (session->providers, provider->protocol, provider);
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

	CS_CLASS (session)->register_provider (session, provider);
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

static GList *
list_providers (CamelSession *session, gboolean load)
{
	GList *list = NULL;

	if (load)
		g_hash_table_foreach (session->modules, ensure_loaded, session);

	g_hash_table_foreach (session->providers, add_to_list, &list);
	return list;
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

	CAMEL_SESSION_LOCK (session, lock);
	list = CS_CLASS (session)->list_providers (session, load);
	CAMEL_SESSION_UNLOCK (session, lock);

	return list;
}


static CamelProvider *
get_provider (CamelSession *session, const char *url_string, CamelException *ex)
{
	CamelProvider *provider;
	char *protocol;

	protocol = g_strndup (url_string, strcspn (url_string, ":"));

	provider = g_hash_table_lookup (session->providers, protocol);
	if (!provider) {
		/* See if there's one we can load. */
		char *path;

		path = g_hash_table_lookup (session->modules, protocol);
		if (path) {
			camel_provider_load (session, path, ex);
			if (camel_exception_is_set (ex)) {
				g_free (protocol);
				return NULL;
			}
		}
		provider = g_hash_table_lookup (session->providers, protocol);
	}

	if (!provider) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      _("No provider available for protocol `%s'"),
				      protocol);
	}
	g_free (protocol);

	return provider;
}

/**
 * camel_session_get_provider:
 * @session: the session
 * @url_string: the URL for the service whose provider you want
 * @ex: a CamelException
 *
 * This returns the CamelProvider that would be used to handle
 * @url_string, loading it in from disk if necessary.
 *
 * Return value: the provider, or %NULL, in which case @ex will be set.
 **/
CamelProvider *
camel_session_get_provider (CamelSession *session, const char *url_string,
			    CamelException *ex)
{
	CamelProvider *provider;

	CAMEL_SESSION_LOCK (session, lock);
	provider = CS_CLASS (session)->get_provider (session, url_string, ex);
	CAMEL_SESSION_UNLOCK (session, lock);

	return provider;
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


static CamelService *
get_service (CamelSession *session, const char *url_string,
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
	provider = CS_CLASS (session)->get_provider (session, url->protocol, ex);
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
	
	/* Now look up the service in the provider's cache */
	service = g_hash_table_lookup (provider->service_cache, url);
	if (service != NULL) {
		camel_url_free (url);
		camel_object_ref (CAMEL_OBJECT (service));
		return service;
	}

	service = (CamelService *)camel_object_new (provider->object_types[type]);
	camel_service_construct (service, session, provider, url, ex);
	if (camel_exception_is_set (ex)) {
		camel_object_unref (CAMEL_OBJECT (service));
		service = NULL;
	} else {
		g_hash_table_insert (provider->service_cache, url, service);
		camel_object_hook_event (CAMEL_OBJECT (service), "finalize", (CamelObjectEventHookFunc) service_cache_remove, session);
	}

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

	if (svc->connected == FALSE) {
		if (camel_service_connect (svc, ex) == FALSE) {
			camel_object_unref (CAMEL_OBJECT (svc));
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

	if (camel_mkdir_hier (path, S_IRWXU) == -1) {
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
 * @prompt: prompt to provide to user
 * @secret: whether or not the data is secret (eg, a password, as opposed
 * to a smartcard response)
 * @service: the service this query is being made by
 * @item: an identifier, unique within this service, for the information
 * @ex: a CamelException
 *
 * This function is used by a CamelService to ask the application and
 * the user for a password or other authentication data.
 *
 * @service and @item together uniquely identify the piece of data the
 * caller is concerned with.
 *
 * @prompt is a question to ask the user (if the application doesn't
 * already have the answer cached). If @secret is set, the user's
 * input will not be echoed back. The authenticator should set @ex
 * to %CAMEL_EXCEPTION_USER_CANCEL if the user did not provide the
 * information. The caller must g_free() the information returned when
 * it is done with it.
 *
 * Return value: the authentication information or %NULL.
 **/
char *
camel_session_get_password (CamelSession *session, const char *prompt,
			    gboolean secret, CamelService *service,
			    const char *item, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), NULL);
	g_return_val_if_fail (prompt != NULL, NULL);
	g_return_val_if_fail (item != NULL, NULL);

	return CS_CLASS (session)->get_password (session, prompt, secret, service, item, ex);
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
			       const char *item, CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_SESSION (session));
	g_return_if_fail (CAMEL_IS_SERVICE (service));
	g_return_if_fail (item != NULL);

	CS_CLASS (session)->forget_password (session, service, item, ex);
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
 * camel_session_register_timeout:
 * @session: the CamelSession
 * @interval: the number of milliseconds interval between calls
 * @callback: the function to call
 * @user_data: extra data to be passed to the callback
 *
 * Registers the given timeout. @callback will be called every
 * @interval milliseconds with one argument, @user_data, until it
 * returns %FALSE.
 *
 * Return value: On success, a non-zero handle that can be used with
 * camel_session_remove_timeout(). On failure, 0.
 **/
guint
camel_session_register_timeout (CamelSession *session,
				guint32 interval,
				CamelTimeoutCallback callback,
				gpointer user_data)
{
	g_return_val_if_fail (CAMEL_IS_SESSION (session), 0);

	return CS_CLASS (session)->register_timeout (session, interval, callback, user_data);
}

/**
 * camel_session_remove_timeout:
 * @session: the CamelSession
 * @handle: a value returned from camel_session_register_timeout()
 *
 * Removes the indicated timeout.
 *
 * Return value: %TRUE on success, %FALSE on failure.
 **/
gboolean
camel_session_remove_timeout (CamelSession *session, guint handle)
{
	g_return_val_if_fail (CAMEL_IS_SESSION (session), FALSE);

	return CS_CLASS (session)->remove_timeout (session, handle);
}
