/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mbox-store.c : class for an mbox store */

/* 
 *
 * Copyright (C) 2000 Helix Code, Inc. <bertrand@helixcode.com>
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
#include "camel-log.h"

#include "camel-mbox-store.h"
#include "camel-mbox-folder.h"
#include "camel-exception.h"
#include "url-util.h"

/* Returns the class for a CamelMboxStore */
#define CMBOXS_CLASS(so) CAMEL_MBOX_STORE_CLASS (GTK_OBJECT(so)->klass)
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (GTK_OBJECT(so)->klass)
#define CMBOXF_CLASS(so) CAMEL_MBOX_FOLDER_CLASS (GTK_OBJECT(so)->klass)

static CamelFolder *_get_folder (CamelStore *store, const gchar *folder_name, CamelException *ex);


static void
camel_mbox_store_class_init (CamelMboxStoreClass *camel_mbox_store_class)
{
	CamelStoreClass *camel_store_class = CAMEL_STORE_CLASS (camel_mbox_store_class);
	
	/* virtual method overload */
	camel_store_class->get_folder = _get_folder;
}



static void
camel_mbox_store_init (gpointer object, gpointer klass)
{
	CamelService *service = CAMEL_SERVICE (object);

	service->url_flags = CAMEL_SERVICE_URL_NEED_PATH;
}




GtkType
camel_mbox_store_get_type (void)
{
	static GtkType camel_mbox_store_type = 0;
	
	if (!camel_mbox_store_type)	{
		GtkTypeInfo camel_mbox_store_info =	
		{
			"CamelMboxStore",
			sizeof (CamelMboxStore),
			sizeof (CamelMboxStoreClass),
			(GtkClassInitFunc) camel_mbox_store_class_init,
			(GtkObjectInitFunc) camel_mbox_store_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_mbox_store_type = gtk_type_unique (CAMEL_STORE_TYPE, &camel_mbox_store_info);
	}
	
	return camel_mbox_store_type;
}


const gchar *
camel_mbox_store_get_toplevel_dir (CamelMboxStore *store)
{
	Gurl *url = CAMEL_SERVICE (store)->url;

	g_assert(url != NULL);
	return url->path;
}



static CamelFolder *
_get_folder (CamelStore *store, const gchar *folder_name, CamelException *ex)
{
	CamelMboxFolder *new_mbox_folder;
	CamelFolder *new_folder;

	/* check if folder has already been created */
	/* call the standard routine for that when  */
	/* it is done ... */

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMboxStore::get_folder\n");
	new_mbox_folder =  gtk_type_new (CAMEL_MBOX_FOLDER_TYPE);
	new_folder = CAMEL_FOLDER (new_mbox_folder);
	
	/* XXX We shouldn't be passing NULL here, but it's equivalent to
	 * what was there before, and there's no
	 * CamelMboxFolder::get_subfolder yet anyway...
	 */
	CF_CLASS (new_folder)->init (new_folder, store, NULL,
				     folder_name, '/', ex);
	
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMboxStore::get_folder\n");

	return new_folder;
}
