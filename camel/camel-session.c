/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-session.c : Abstract class for an email session */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
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
#include "camel-session.h"
#include "camel-store.h"
#include "camel-transport.h"
#include "camel-exception.h"
#include "string-utils.h"
#include "camel-url.h"
#include "hash-table-utils.h"



static GtkObjectClass *parent_class=NULL;



/* Returns the class for a CamelSession */
#define CSS_CLASS(so) CAMEL_SESSION_CLASS (GTK_OBJECT(so)->klass)


static void
camel_session_class_init (CamelSessionClass *camel_session_class)
{
	parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method definition */
	/* virtual method overload */
}





static void
camel_session_init (CamelSession *session)
{
	session->store_provider_list = g_hash_table_new (g_strcase_hash, g_strcase_equal);
	session->transport_provider_list = g_hash_table_new (g_strcase_hash, g_strcase_equal);
}



GtkType
camel_session_get_type (void)
{
	static GtkType camel_session_type = 0;
	
	if (!camel_session_type)	{
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
		
		camel_session_type = gtk_type_unique (gtk_object_get_type (), &camel_session_info);
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
 * camel_session_set_provider: set the default provider for a protocol
 * @session: session object for wich the provider will the default
 * @provider: provider object
 * 
 * Set the default implementation for a protocol. The protocol
 * is determined by provider->protocol field (See CamelProvider).
 * It overrides the default provider for this protocol.
 * 
 **/
void 
camel_session_set_provider (CamelSession *session, CamelProvider *provider)
{
	GHashTable *table;

	g_assert(session);
	g_assert(provider);
	
	if (provider->provider_type == PROVIDER_STORE)
		table = session->store_provider_list;
	else
		table = session->transport_provider_list;
	
	g_hash_table_insert (table, (gpointer)(provider->protocol), (gpointer)(provider));
	
}





/**
 * camel_session_get_store_from_provider: create a folder instance for a given provider
 * @session: session object the folder will be initialized with
 * @provider: provider folder to instantiate
 * @ex: a CamelException
 * 
 * 
 * Return value: the newly instantiated store
 **/
CamelStore *
camel_session_get_store_from_provider (CamelSession *session,
				       CamelProvider *provider,
				       CamelException *ex)
{
	g_assert(session);
	g_assert(provider);

	return CAMEL_STORE (camel_service_new (provider->object_type,
					       session, NULL, ex));
}



/**
 * get_store_for_protocol_with_url: private helper routine
 * @session: CamelSession object
 * @protocol: protocol name
 * @url: a URL, or NULL
 * @ex: a CamelException
 *
 * Used by camel_session_get_store_for_protocol and
 * camel_session_get_store.
 *
 * Return value: initialized store associated with this protocol, or NULL if no provider was found.
 **/
static CamelStore *
get_store_for_protocol_with_url (CamelSession *session, const char *protocol,
				 CamelURL *url, CamelException *ex)
{
	const CamelProvider *provider = NULL;

	/* See if there is a provider assiciated with this
	 * protocol in this session.
	 */
	provider = CAMEL_PROVIDER (g_hash_table_lookup (session->store_provider_list, protocol));
	if (!provider) {
		/* No provider was found in this session. See
		 * if there is a registered provider for this 
		 * protocol.
		 */
		provider = camel_provider_get_for_protocol (protocol, PROVIDER_STORE);
	}
	if (!provider)
		return NULL;

	return CAMEL_STORE (camel_service_new (provider->object_type,
					       session, url, ex));
}


/**
 * camel_session_get_store_for_protocol: get the store associated to a protocol
 * @session: CamelSession object
 * @protocol: protocol name 
 * @ex: a CamelException
 * 
 * Return a CamelStore object associated with a given
 * store protocol. If a provider has been set for this
 * protocol in the session @session using 
 * camel_session_set_provider (), then a store 
 * obtained from this provider is returned.
 * Otherwise, if one or more providers corresponding 
 * to this protocol have been registered (See 
 * camel_provider_register_as_module), the last registered
 * one is used. 
 * 
 * Return value: store associated with this protocol, or NULL if no provider was found. 
 **/
CamelStore *
camel_session_get_store_for_protocol (CamelSession *session,
				      const char *protocol,
				      CamelException *ex)
{
	return get_store_for_protocol_with_url (session, protocol, NULL, ex);
}



/**
 * camel_session_get_store: get a store object for an URL
 * @session: session object
 * @url_string: url
 * @ex: a CamelException
 * 
 * return a store corresponding to an URL. 
 * 
 * Return value: the store, or NULL if no provider correponds to the protocol
 **/
CamelStore *
camel_session_get_store (CamelSession *session, const char *url_string,
			 CamelException *ex)
{
	CamelURL *url;
	CamelStore *store;

	url = camel_url_new (url_string, ex);
	if (!url)
		return NULL;
	
	store = get_store_for_protocol_with_url (session, url->protocol,
						 url, ex);
	if (store == NULL)
		camel_url_free (url);
	return store;
}



/**
 * camel_session_query_authenticator: query the session authenticator
 * @session: session object
 * @prompt: prompt to use if authenticator can query the user
 * @secret: whether or not the data is secret (eg, a password)
 * @service: the service this query is being made by
 * @item: an identifier, unique within this service, for the information
 * @ex: a CamelException
 *
 * This function is used by a CamelService to request authentication
 * information it needs to complete a connection. If the authenticator
 * stores any authentication information in configuration files, it
 * should use @service and @item as keys to find the right piece of
 * information. If it doesn't store authentication information in config
 * files, it should use the given @prompt to ask the user for the
 * information. If @secret is set, the user's input should not be
 * echoed back. The authenticator should set @ex to
 * CAMEL_EXCEPTION_USER_CANCEL if the user did not provide the
 * information. The caller must g_free() the information when it is
 * done with it.
 *
 * Return value: the authentication information or NULL.
 **/
char *
camel_session_query_authenticator (CamelSession *session, char *prompt,
				   gboolean secret,
				   CamelService *service, char *item,
				   CamelException *ex)
{
	return session->authenticator (prompt, secret, service, item, ex);
}



/**
 * camel_session_get_transport_for_protocol: get the transport for a protocol
 * @session: the session
 * @protocol: protocol name
 * @ex: a CamelException
 *
 * Return a CamelTransport object associated with a given transport
 * protocol. If a provider has been set for this protocol in the
 * session @session using camel_session_set_provider (), then a transport
 * obtained from this provider is returned. Otherwise, if one or more
 * providers corresponding to this protocol have been registered (See
 * camel_provider_register_as_module), the last registered one is
 * used.
 * 
 * Return value: transport associated with this protocol, or NULL if no provider was found. 
 **/
CamelTransport *
camel_session_get_transport_for_protocol (CamelSession *session,
					  const char *protocol,
					  CamelException *ex)
{
	const CamelProvider *provider = NULL;

	/* See if there is a provider assiciated with this
	 * protocol in this session.
	 */
	provider = CAMEL_PROVIDER (g_hash_table_lookup (session->transport_provider_list, protocol));
	if (!provider) {
		/* No provider was found in this session. See
		 * if there is a registered provider for this 
		 * protocol.
		 */
		provider = camel_provider_get_for_protocol (protocol, PROVIDER_TRANSPORT);
	}
	if (!provider)
		return NULL;

	return CAMEL_TRANSPORT (gtk_object_new (provider->object_type, NULL));
}
