/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-session.c : Abstract class for an email session */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org> .
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
#include "string-utils.h"
#include "url-util.h"
#include "camel-provider.h"
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
camel_session_new ()
{
	return gtk_type_new (CAMEL_SESSION_TYPE);
}

/**
 * camel_session_set_provider: set the default provider for a protocol
 * @session: session object for wich the provider will the default
 * @provider: provider object
 * 
 * Set the default implementation for a protocol. The protocol
 * is determined by provider->protocol field (See CamelProtocol).
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
 * 
 * 
 * 
 * Return value: the newly instantiated store
 **/
CamelStore *
camel_session_get_store_from_provider (CamelSession *session, CamelProvider *provider)
{
	CamelStore *store;

	g_assert(session);
	g_assert(provider);

	store = CAMEL_STORE (gtk_object_new (provider->object_type, NULL));
#warning set the url to a useful value.
	camel_store_init(store, session, NULL);
	return store;
}




/**
 * camel_session_get_store_for_protocol: get the store associated to a protocol
 * @session: CamelSession object
 * @protocol: protocol name 
 * 
 * Return a CamelStore object associated to a given
 * store protocol. If a provider has been set for this
 * protocol in the session @session using 
 * camel_session_set_provider (), then a store 
 * obtained from this provider is return.
 * Otherwise, if one or more  provider corresponding 
 * to this protocol has been registered (See 
 * camel_provider_register_as_module), the last registered
 * one is used. 
 * 
 * Return value: store associated to this protocol or NULL if no provider was found. 
 **/
CamelStore *
camel_session_get_store_for_protocol (CamelSession *session, const gchar *protocol)
{
	const CamelProvider *provider = NULL;
	CamelStore *new_store;

	/* look if there is a provider assiciated to this
	   protocol in this session */
	provider = CAMEL_PROVIDER (g_hash_table_lookup (session->store_provider_list, protocol));
	if (!provider)
		/* no provider was found in this session, look 
		   if there is a registered provider for this 
		   protocol */
		provider = camel_provider_get_for_protocol (protocol, PROVIDER_STORE);
	
	if (!provider) return NULL;
	
	new_store = (CamelStore *)gtk_type_new (provider->object_type);
	return new_store;
}




/**
 * camel_session_get_store: get a store object for an URL
 * @session: session object
 * @url_string: url 
 * 
 * return a store corresponding to an URL. 
 * 
 * Return value: the store, or NULL if no provider correponds to the protocol
 **/
CamelStore *
camel_session_get_store (CamelSession *session, const gchar *url_string)
{
	Gurl *url = NULL;
	CamelStore *new_store = NULL;

	url = g_url_new (url_string);
	g_return_val_if_fail (url, NULL);
	
	if (url->protocol) {
		new_store = camel_session_get_store_for_protocol (session, url->protocol);
		if (new_store)
			camel_store_init (new_store, session, url_string);
	}
	g_url_free (url);
	
	return new_store;

}
