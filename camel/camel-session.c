/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-session.c : Abstract class for an email session */

/*
 *
 * Author:
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *  Dan Winship <danw@helixcode.com>
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
#include <stdio.h>
#include "camel-session.h"
#include "camel-store.h"
#include "camel-transport.h"
#include "camel-exception.h"
#include "string-utils.h"
#include "camel-url.h"
#include "hash-table-utils.h"

static CamelObjectClass *parent_class;

static void
camel_session_init (CamelSession *session)
{
	session->modules = camel_provider_init ();
	session->providers =
		g_hash_table_new (g_strcase_hash, g_strcase_equal);
	session->service_cache =
		g_hash_table_new (camel_url_hash, camel_url_equal);
}

static void
camel_session_finalise(GtkObject *o)
{
	CamelSession *session = (CamelSession *)o;

	g_hash_table_destroy(session->service_cache);
	g_hash_table_destroy(session->providers);

	GTK_OBJECT_CLASS (parent_class)->finalize (o);
}

static void
camel_session_class_init (CamelServiceClass *camel_service_class)
{
	GtkObjectClass *object_class = (GtkObjectClass *)camel_service_class;

	parent_class = gtk_type_class (camel_object_get_type ());
	object_class->finalize = camel_session_finalise;
}

GtkType
camel_session_get_type (void)
{
	static GtkType camel_session_type = 0;

	if (!camel_session_type) {
		GtkTypeInfo camel_session_info =
		{
			"CamelSession",
			sizeof (CamelSession),
			sizeof (CamelSessionClass),
			(GtkClassInitFunc) camel_session_class_init,
			(GtkObjectInitFunc) camel_session_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_session_type = gtk_type_unique (camel_object_get_type (),
						      &camel_session_info);
	}

	return camel_session_type;
}


CamelSession *
camel_session_new (CamelAuthCallback authenticator)
{
	CamelSession *session = gtk_type_new (CAMEL_SESSION_TYPE);

	session->authenticator = authenticator;
	return session;
}

/**
 * camel_session_register_provider:
 * @session: a session object
 * @protocol: the protocol the provider provides for
 * @provider: provider object
 *
 * Registers a protocol to provider mapping for the session.
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

	if (load) {
		g_hash_table_foreach (session->modules, ensure_loaded,
				      session);
	}

	list = NULL;
	g_hash_table_foreach (session->providers, add_to_list, &list);
	return list;
}

static void
service_cache_remove(CamelService *service, CamelSession *session)
{
	g_hash_table_remove(session->service_cache, service->url);
}

CamelService *
camel_session_get_service (CamelSession *session, const char *url_string,
			   CamelProviderType type, CamelException *ex)
{
	CamelURL *url;
	const CamelProvider *provider;
	CamelService *service;

	url = camel_url_new (url_string, ex);
	if (!url)
		return NULL;

	/* lookup in cache first */
	printf("looking up service in cache: %s\n", url_string);
	service = g_hash_table_lookup(session->service_cache, url);
	if (service != NULL) {
		printf("found!!\n");
		camel_url_free(url);
		gtk_object_ref((GtkObject *)service);
		return service;
	}
	printf("not found, creating service\n");

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
				return NULL;
			}
		}
		provider = g_hash_table_lookup (session->providers,
						url->protocol);
	}

	if (!provider || !provider->object_types[type]) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      "No %s available for protocol `%s'",
				      camel_provider_type_name[type],
				      url->protocol);
		camel_url_free (url);
		return NULL;
	}

	service = camel_service_new (provider->object_types[type], session, url, ex);
	if (service) {
		g_hash_table_insert(session->service_cache, url, service);
		gtk_signal_connect((GtkObject *)service, "destroy", service_cache_remove, session);
	}
	return service;
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
