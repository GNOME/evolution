/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelStore.c : Abstract class for an email store */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr> .
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
#include "camel-store.h"

static GtkObjectClass *parent_class=NULL;

/* Returns the class for a CamelStore */
#define CS_CLASS(so) CAMEL_STORE_CLASS (GTK_OBJECT(so)->klass)

static void _set_separator(CamelStore *store, gchar sep);
static CamelFolder *_get_root_folder(CamelStore *store);
static CamelFolder *_get_default_folder(CamelStore *store);
static void _init(CamelStore *store, CamelSession *session, gchar *url_name);



static void
camel_store_class_init (CamelStoreClass *camel_store_class)
{
	parent_class = gtk_type_class (camel_service_get_type ());
	
	/* virtual method definition */
	camel_store_class->init = _init;
	camel_store_class->set_separator = _set_separator;
	camel_store_class->get_separator = camel_store_get_separator;
	camel_store_class->get_folder = camel_store_get_folder;
	camel_store_class->get_root_folder = _get_root_folder;
	camel_store_class->get_default_folder = _get_default_folder;
	/* virtual method overload */
}







GtkType
camel_store_get_type (void)
{
	static GtkType camel_store_type = 0;
	
	if (!camel_store_type)	{
		GtkTypeInfo camel_store_info =	
		{
			"CamelStore",
			sizeof (CamelStore),
			sizeof (CamelStoreClass),
			(GtkClassInitFunc) camel_store_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_store_type = gtk_type_unique (CAMEL_SERVICE_TYPE, &camel_store_info);
	}
	
	return camel_store_type;
}





/**
 * camel_store_init: call store's init method
 * @store: the store to initialize
 * @session: session which instantiates the store
 * @url_name: URL defining the store
 *
 * This routine is called by the session object from which this 
 * store is created. It must not be called directly.
 * 
 **/
void 
camel_store_init(CamelStore *store, CamelSession *session, gchar *url_name)
{
	g_assert(store);
	CS_CLASS(store)->init (store, session, url_name);
}


/**
 * init: method called by a session object to 
 * initialize a store object
 * @store: the store to initialize
 * @session: session which instantiates the store
 * @url_name: URL defining the store
 * 
 * This routine is called by the session object from which this 
 * store is created. Be careful, @url_name is used as a private field
 * of the store object. 
 * 
 **/
static void 
_init (CamelStore *store, CamelSession *session, gchar *url_name)
{
	
	
	g_assert(session);
	g_assert(url_name);

	store->session = session;
	store->url_name = url_name;
}



/** 
 * camel_store_set_separator: set the character which separates this folder 
 * path from the folders names in a lower level of hierarchy.
 *
 * @store:
 * @sep:
 *
 **/
static void
_set_separator (CamelStore *store, gchar sep)
{
    store->separator = sep;
}



/** 
 * camel_store_get_separator: return the character which separates this folder 
 * path from the folders names in a lower level of hierarchy.
 *
 * @store: store
 *
 **/
gchar
camel_store_get_separator (CamelStore *store)
{
	g_assert(store);
	return store->separator;
}




/** 
 * camel_store_get_folder: return the folder corresponding to a path.
 * 
 * Returns the folder corresponding to the path "name". 
 * If the path begins with the separator caracter, it 
 * is relative to the root folder. Otherwise, it is
 * relative to the default folder.
 * The folder does not necessarily exist on the store.
 * To make sure it already exists, use its "exists" method.
 * If it does not exist, you can create it with its 
 * "create" method.
 *
 * @store: store
 * @folder_name: name of the folder to get
 *
 * Return value: the folder
 **/
CamelFolder *
camel_store_get_folder (CamelStore *store, gchar *folder_name)
{

#warning fill this part in.
	return NULL;
}


/**
 * camel_store_get_root_folder : return the toplevel folder
 * 
 * Returns the folder which is at the top of the folder
 * hierarchy. This folder is generally different from
 * the default folder. 
 * 
 * @Return value: the tolevel folder.
 **/
static CamelFolder *
_get_root_folder (CamelStore *store)
{
    return NULL;
}

/** 
 * camel_store_get_default_folder : return the store default folder
 *
 * The default folder is the folder which is presented 
 * to the user in the default configuration. The default
 * is often the root folder.
 *
 *  @Return value: the default folder.
 **/
static CamelFolder *
_get_default_folder (CamelStore *store)
{
    return NULL;
}



