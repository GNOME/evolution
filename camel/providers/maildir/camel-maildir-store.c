/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-maildir-store.c : class for an maildir store */

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

#include "camel-maildir-store.h"
#include "camel-maildir-folder.h"
#include "url-util.h"

static CamelStoreClass *parent_class=NULL;

/* Returns the class for a CamelMaildirStore */
#define CMAILDIRS_CLASS(so) CAMEL_MAILDIR_STORE_CLASS (GTK_OBJECT(so)->klass)
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (GTK_OBJECT(so)->klass)
#define CMAILDIRF_CLASS(so) CAMEL_MAILDIR_FOLDER_CLASS (GTK_OBJECT(so)->klass)

static void _init (CamelStore *store, CamelSession *session,
		   const gchar *url_name);
static CamelFolder *_get_folder (CamelStore *store, const gchar *folder_name);

static void
camel_maildir_store_class_init (
	CamelMaildirStoreClass *camel_maildir_store_class)
{
	CamelStoreClass *camel_store_class =
		CAMEL_STORE_CLASS (camel_maildir_store_class);

	parent_class = gtk_type_class (camel_store_get_type ());
	
	/* virtual method definition */
	/* virtual method overload */
	camel_store_class->init = _init;
	camel_store_class->get_folder = _get_folder;
}

static void
camel_maildir_store_init (gpointer object, gpointer klass)
{
	CamelMaildirStore *maildir_store = CAMEL_MAILDIR_STORE (object);
	CamelStore *store = CAMEL_STORE (object);
	
	store->separator = G_DIR_SEPARATOR;
}

GtkType
camel_maildir_store_get_type (void)
{
	static GtkType camel_maildir_store_type = 0;
	
	if (!camel_maildir_store_type)	{
		GtkTypeInfo camel_maildir_store_info =	
		{
			"CamelMaildirStore",
			sizeof (CamelMaildirStore),
			sizeof (CamelMaildirStoreClass),
			(GtkClassInitFunc) camel_maildir_store_class_init,
			(GtkObjectInitFunc) camel_maildir_store_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_maildir_store_type =
			gtk_type_unique (CAMEL_STORE_TYPE,
					 &camel_maildir_store_info);
	}
	
	return camel_maildir_store_type;
}

static void 
_init (CamelStore *store, CamelSession *session, const gchar *url_name)
{
	CamelMaildirStore *maildir_store = CAMEL_MAILDIR_STORE (store);
	Gurl *store_url;
	
	g_assert (url_name);

	/* call parent implementation */
	parent_class->init (store, session, url_name);
	
	/* find the path in the URL*/
	store_url = g_url_new (url_name);

	g_return_if_fail (store_url);
	g_return_if_fail (store_url->path); 
	
	maildir_store->toplevel_dir = g_strdup (store_url->path); 

	g_url_free (store_url);
}

static CamelFolder *
_get_folder (CamelStore *store, const gchar *folder_name)
{
	CamelMaildirStore *maildir_store = CAMEL_MAILDIR_STORE (store);
	CamelMaildirFolder *new_maildir_folder;
	CamelFolder *new_folder;
	gchar *maildir = g_strconcat (maildir_store->toplevel_dir, G_DIR_SEPARATOR_S, folder_name, NULL);

	new_maildir_folder = gtk_type_new (CAMEL_MAILDIR_FOLDER_TYPE);
	new_folder = CAMEL_FOLDER (new_maildir_folder);

	CF_CLASS (new_folder)->init_with_store (new_folder, store);
	CF_CLASS (new_folder)->set_name (new_folder, maildir);
	
	return new_folder;
}
