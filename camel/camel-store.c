/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelStore.c : Abstract class for an email store */

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
#include "camel-store.h"
#include "camel-exception.h"
#include "camel-log.h"

static CamelServiceClass *parent_class = NULL;

/* Returns the class for a CamelStore */
#define CS_CLASS(so) CAMEL_STORE_CLASS (GTK_OBJECT(so)->klass)

static CamelFolder *_get_root_folder(CamelStore *store, CamelException *ex);
static CamelFolder *_get_default_folder(CamelStore *store, CamelException *ex);
static CamelFolder *_get_folder (CamelStore *store, const gchar *folder_name, CamelException *ex);

static void
camel_store_class_init (CamelStoreClass *camel_store_class)
{

	parent_class = gtk_type_class (camel_service_get_type ());
	
	/* virtual method definition */
	camel_store_class->get_folder = _get_folder;
	camel_store_class->get_root_folder = _get_root_folder;
	camel_store_class->get_default_folder = _get_default_folder;
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




static CamelFolder *
_get_folder (CamelStore *store, const gchar *folder_name, CamelException *ex)
{
	return NULL;
}


/** 
 * camel_store_get_folder: return the folder corresponding to a path.
 * @store: store
 * @folder_name: name of the folder to get
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
 *
 * Return value: the folder
 **/
CamelFolder *
camel_store_get_folder (CamelStore *store, const gchar *folder_name, CamelException *ex)
{
	return CS_CLASS(store)->get_folder (store, folder_name, ex);
}


/**
 * camel_store_get_root_folder : return the toplevel folder
 * 
 * Returns the folder which is at the top of the folder
 * hierarchy. This folder is generally different from
 * the default folder. 
 * 
 * @Return value: the toplevel folder.
 **/
static CamelFolder *
_get_root_folder (CamelStore *store, CamelException *ex)
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
_get_default_folder (CamelStore *store, CamelException *ex)
{
    return NULL;
}
