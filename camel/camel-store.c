/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-store.c : Abstract class for an email store */

/* 
 *
 * Authors:
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
#include "camel-store.h"
#include "camel-folder.h"
#include "camel-exception.h"

static CamelServiceClass *parent_class = NULL;

/* Returns the class for a CamelStore */
#define CS_CLASS(so) CAMEL_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static CamelFolder *get_folder (CamelStore *store, const char *folder_name,
				gboolean create, CamelException *ex);
static void delete_folder (CamelStore *store, const char *folder_name,
			   CamelException *ex);
static void rename_folder (CamelStore *store, const char *old_name,
			   const char *new_name, CamelException *ex);

static char *get_folder_name (CamelStore *store, const char *folder_name,
			      CamelException *ex);
static char *get_root_folder_name (CamelStore *store, CamelException *ex);
static char *get_default_folder_name (CamelStore *store, CamelException *ex);

static CamelFolder *lookup_folder (CamelStore *store, const char *folder_name);
static void cache_folder (CamelStore *store, const char *folder_name,
			  CamelFolder *folder);
static void uncache_folder (CamelStore *store, CamelFolder *folder);

static void
camel_store_class_init (CamelStoreClass *camel_store_class)
{
	parent_class = CAMEL_SERVICE_CLASS (camel_type_get_global_classfuncs (camel_service_get_type ()));

	/* virtual method definition */
	camel_store_class->get_folder = get_folder;
	camel_store_class->delete_folder = delete_folder;
	camel_store_class->rename_folder = rename_folder;
	camel_store_class->get_folder_name = get_folder_name;
	camel_store_class->get_root_folder_name = get_root_folder_name;
	camel_store_class->get_default_folder_name = get_default_folder_name;
	camel_store_class->lookup_folder = lookup_folder;
	camel_store_class->cache_folder = cache_folder;
	camel_store_class->uncache_folder = uncache_folder;
}

static void
camel_store_init (void *o, void *k)
{
	CamelStore *store = o;

	store->folders = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
camel_store_finalize (CamelObject *object)
{
	CamelStore *store = CAMEL_STORE (object);

	if (store->folders) {
		if (g_hash_table_size (store->folders) != 0) {
			g_warning ("Folder cache for store %p contains "
				   "%d folders at destruction.", store,
				   g_hash_table_size (store->folders));
		}
		g_hash_table_destroy (store->folders);
	}
}


CamelType
camel_store_get_type (void)
{
	static CamelType camel_store_type = CAMEL_INVALID_TYPE;

	if (camel_store_type == CAMEL_INVALID_TYPE) {
		camel_store_type = camel_type_register (CAMEL_SERVICE_TYPE, "CamelStore",
							sizeof (CamelStore),
							sizeof (CamelStoreClass),
							(CamelObjectClassInitFunc) camel_store_class_init,
							NULL,
							(CamelObjectInitFunc) camel_store_init,
							(CamelObjectFinalizeFunc) camel_store_finalize );
	}

	return camel_store_type;
}


static CamelFolder *
get_folder (CamelStore *store, const char *folder_name,
	    gboolean create, CamelException *ex)
{
	g_warning ("CamelStore::get_folder not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store)));
	return NULL;
}

static void
delete_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	g_warning ("CamelStore::delete_folder not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store)));
}

static void rename_folder (CamelStore *store, const char *old_name,
			   const char *new_name, CamelException *ex)
{
	g_warning ("CamelStore::rename_folder not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store)));
	camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
			     "rename folder unimplemented for: %s",
			     camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store)));
}


/* CamelStore::get_folder_name should:
 * a) make sure that the provided name is valid
 * b) return it in canonical form, in allocated memory.
 *
 * This is used to make sure that duplicate names for the same folder
 * don't result in duplicate cache entries.
 */
static char *
get_folder_name (CamelStore *store, const char *folder_name,
		 CamelException *ex)
{
	g_warning ("CamelStore::get_folder_name not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store)));
	return NULL;
}

static char *
get_root_folder_name (CamelStore *store, CamelException *ex)
{
	return g_strdup ("/");
}

static char *
get_default_folder_name (CamelStore *store, CamelException *ex)
{
	return CS_CLASS (store)->get_root_folder_name (store, ex);
}

static CamelFolder *
lookup_folder (CamelStore *store, const char *folder_name)
{
	if (store->folders) {
		CamelFolder *folder = g_hash_table_lookup (store->folders, folder_name);
		if (folder)
			camel_object_ref(CAMEL_OBJECT(folder));
		return folder;
	}
	return NULL;
}

static void folder_finalize (CamelObject *folder, gpointer event_data, gpointer user_data)
{
	CS_CLASS (user_data)->uncache_folder (CAMEL_STORE(user_data), CAMEL_FOLDER(folder));
}

static void
cache_folder (CamelStore *store, const char *folder_name, CamelFolder *folder)
{
	if (!store->folders)
		return;

	if (g_hash_table_lookup (store->folders, folder_name)) {
		g_warning ("Caching folder %s that already exists.",
			   folder_name);
	}
	g_hash_table_insert (store->folders, g_strdup (folder_name), folder);

	camel_object_hook_event (CAMEL_OBJECT (folder), "finalize", folder_finalize, store);

	/*
	 * gt_k so as not to get caught by my little gt_k cleanliness detector.
	 *
	 * gt_k_signal_connect_object (CAMEL_OBJECT (folder), "destroy",
	 *			   GT_K_SIGNAL_FUNC (CS_CLASS (store)->uncache_folder),
	 *			   CAMEL_OBJECT (store));
	 */
}

static gboolean
folder_matches (gpointer key, gpointer value, gpointer user_data)
{
	if (value == user_data) {
		g_free (key);
		return TRUE;
	} else
		return FALSE;
}

static void
uncache_folder (CamelStore *store, CamelFolder *folder)
{
	g_hash_table_foreach_remove (store->folders, folder_matches, folder);
}


static CamelFolder *
get_folder_internal (CamelStore *store, const char *folder_name,
		     gboolean create, CamelException *ex)
{
	CamelFolder *folder = NULL;

	/* Try cache first. */
	folder = CS_CLASS (store)->lookup_folder (store, folder_name);

	if (!folder) {
		folder = CS_CLASS (store)->get_folder (store, folder_name,
						       create, ex);
		if (!folder)
			return NULL;

		CS_CLASS (store)->cache_folder (store, folder_name, folder);
	}

	return folder;
}



/** 
 * camel_store_get_folder: Return the folder corresponding to a path.
 * @store: a CamelStore
 * @folder_name: name of the folder to get
 * @create: whether or not to create the folder if it doesn't already exist
 * @ex: a CamelException
 * 
 * Returns the folder corresponding to the path @folder_name. If the
 * path begins with the separator character, it is relative to the
 * root folder. Otherwise, it is relative to the default folder. If
 * @create is %TRUE and the named folder does not already exist, it will
 * be created.
 *
 * Return value: the folder
 **/
CamelFolder *
camel_store_get_folder (CamelStore *store, const char *folder_name,
			gboolean create, CamelException *ex)
{
	char *name;
	CamelFolder *folder = NULL;

	if (!camel_service_is_connected (CAMEL_SERVICE (store)))
		camel_service_connect (CAMEL_SERVICE (store), ex);

	name = CS_CLASS (store)->get_folder_name (store, folder_name, ex);
	if (name) {
		folder = get_folder_internal (store, name, create, ex);
		g_free (name);
	}
	return folder;
}


/** 
 * camel_store_delete_folder: Delete the folder corresponding to a path.
 * @store: a CamelStore
 * @folder_name: name of the folder to delete
 * @ex: a CamelException
 * 
 * Deletes the named folder. The folder must be empty.
 **/
void
camel_store_delete_folder (CamelStore *store, const char *folder_name,
			   CamelException *ex)
{
	char *name;

	name = CS_CLASS (store)->get_folder_name (store, folder_name, ex);
	if (name) {
		CS_CLASS (store)->delete_folder (store, name, ex);
		g_free (name);
	}
}

/**
 * camel_store_rename_folder:
 * @store: 
 * @old_name: 
 * @new_name: 
 * @ex: 
 * 
 * Rename a named folder to a new name.
 **/
void             camel_store_rename_folder      (CamelStore *store,
						 const char *old_name,
						 const char *new_name,
						 CamelException *ex)
{
	char *old, *new;

	old = CS_CLASS (store)->get_folder_name(store, old_name, ex);
	if (old) {
		new = CS_CLASS (store)->get_folder_name(store, new_name, ex);
		if (new) {
			CS_CLASS (store)->rename_folder(store, old, new, ex);
			g_free(new);
		}
		g_free(old);
	}
}


/**
 * camel_store_get_root_folder: return the top-level folder
 * 
 * Returns the folder which is at the top of the folder hierarchy.
 * This folder may or may not be the same as the default folder.
 * 
 * Return value: the top-level folder.
 **/
CamelFolder *
camel_store_get_root_folder (CamelStore *store, CamelException *ex)
{
	char *name;
	CamelFolder *folder = NULL;

	name = CS_CLASS (store)->get_root_folder_name (store, ex);
	if (name) {
		folder = get_folder_internal (store, name, TRUE, ex);
		g_free (name);
	}
	return folder;
}

/** 
 * camel_store_get_default_folder: return the store default folder
 *
 * The default folder is the folder which is presented to the user in
 * the default configuration. This defaults to the root folder if
 * the store doesn't override it.
 *
 * Return value: the default folder.
 **/
CamelFolder *
camel_store_get_default_folder (CamelStore *store, CamelException *ex)
{
	char *name;
	CamelFolder *folder = NULL;

	name = CS_CLASS (store)->get_default_folder_name (store, ex);
	if (name) {
		folder = get_folder_internal (store, name, TRUE, ex);
		g_free (name);
	}
	return folder;
}

