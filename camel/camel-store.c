/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-store.c : Abstract class for an email store */

/* 
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
#include <string.h>

#include "camel-session.h"

#include "camel-store.h"
#include "camel-folder.h"
#include "camel-vee-store.h"
#include "camel-vee-folder.h"
#include "camel-exception.h"

#include "camel-private.h"

static CamelServiceClass *parent_class = NULL;

/* Returns the class for a CamelStore */
#define CS_CLASS(so) CAMEL_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static CamelFolder *get_folder (CamelStore *store, const char *folder_name,
				guint32 flags, CamelException *ex);
static CamelFolder *get_inbox (CamelStore *store, CamelException *ex);

static void        init_trash (CamelStore *store);
static CamelFolder *get_trash (CamelStore *store, CamelException *ex);

static CamelFolderInfo *create_folder (CamelStore *store,
				       const char *parent_name,
				       const char *folder_name,
				       CamelException *ex);
static void delete_folder (CamelStore *store, const char *folder_name,
			   CamelException *ex);
static void rename_folder (CamelStore *store, const char *old_name,
			   const char *new_name, CamelException *ex);

static void store_sync (CamelStore *store, CamelException *ex);
static CamelFolderInfo *get_folder_info (CamelStore *store, const char *top,
					 gboolean fast, gboolean recursive,
					 gboolean subscribed_only,
					 CamelException *ex);
static void free_folder_info (CamelStore *store, CamelFolderInfo *tree);

static gboolean folder_subscribed (CamelStore *store, const char *folder_name);
static void subscribe_folder (CamelStore *store, const char *folder_name, CamelException *ex);
static void unsubscribe_folder (CamelStore *store, const char *folder_name, CamelException *ex);

static void
camel_store_class_init (CamelStoreClass *camel_store_class)
{
	CamelObjectClass *camel_object_class =
		CAMEL_OBJECT_CLASS (camel_store_class);
	
	parent_class = CAMEL_SERVICE_CLASS (camel_type_get_global_classfuncs (camel_service_get_type ()));
	
	/* virtual method definition */
	camel_store_class->hash_folder_name = g_str_hash;
	camel_store_class->compare_folder_name = g_str_equal;
	camel_store_class->get_folder = get_folder;
	camel_store_class->get_inbox = get_inbox;
	camel_store_class->init_trash = init_trash;
	camel_store_class->get_trash = get_trash;
	camel_store_class->create_folder = create_folder;
	camel_store_class->delete_folder = delete_folder;
	camel_store_class->rename_folder = rename_folder;
	camel_store_class->sync = store_sync;
	camel_store_class->get_folder_info = get_folder_info;
	camel_store_class->free_folder_info = free_folder_info;
	camel_store_class->folder_subscribed = folder_subscribed;
	camel_store_class->subscribe_folder = subscribe_folder;
	camel_store_class->unsubscribe_folder = unsubscribe_folder;
	
	/* virtual method overload */
	camel_object_class_declare_event (camel_object_class,
					  "folder_created", NULL);
	camel_object_class_declare_event (camel_object_class,
					  "folder_deleted", NULL);
}

static void
camel_store_init (void *o)
{
	CamelStore *store = o;
	CamelStoreClass *store_class = (CamelStoreClass *)CAMEL_OBJECT_GET_CLASS (o);

	if (store_class->hash_folder_name) {
		store->folders = g_hash_table_new (store_class->hash_folder_name,
						   store_class->compare_folder_name);
	} else
		store->folders = NULL;
	
	store->flags = 0;
	
	store->priv = g_malloc0 (sizeof (*store->priv));
#ifdef ENABLE_THREADS
	store->priv->folder_lock = g_mutex_new();
	store->priv->cache_lock = g_mutex_new();
#endif
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
	
#ifdef ENABLE_THREADS
	g_mutex_free (store->priv->folder_lock);
	g_mutex_free (store->priv->cache_lock);
#endif
	g_free (store->priv);
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
folder_finalize (CamelObject *folder, gpointer event_data, gpointer user_data)
{
	CamelStore *store = CAMEL_STORE (user_data);

	if (store->folders) {
		CAMEL_STORE_LOCK(store, cache_lock);
		g_hash_table_foreach_remove (store->folders, folder_matches, folder);
		CAMEL_STORE_UNLOCK(store, cache_lock);
	}
}

static CamelFolder *
get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	g_warning ("CamelStore::get_folder not implemented for `%s'",
		   camel_type_to_name(CAMEL_OBJECT_GET_TYPE(store)));
	return NULL;
}

/** 
 * camel_store_get_folder: Return the folder corresponding to a path.
 * @store: a CamelStore
 * @folder_name: name of the folder to get
 * @flags: folder flags (create, save body index, etc)
 * @ex: a CamelException
 * 
 * Return value: the folder corresponding to the path @folder_name.
 **/
CamelFolder *
camel_store_get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	CamelFolder *folder = NULL;

	CAMEL_STORE_LOCK(store, folder_lock);

	if (store->folders) {
		/* Try cache first. */
		CAMEL_STORE_LOCK(store, cache_lock);
		folder = g_hash_table_lookup (store->folders, folder_name);
		if (folder)
			camel_object_ref (CAMEL_OBJECT (folder));
		CAMEL_STORE_UNLOCK(store, cache_lock);
	}

	if (!folder) {
		folder = CS_CLASS (store)->get_folder (store, folder_name, flags, ex);
		if (folder && store->folders) {
			CAMEL_STORE_LOCK(store, cache_lock);
			
			g_hash_table_insert (store->folders, g_strdup (folder_name), folder);
			
			/* Add the folder to the vTrash folder if this store implements it */
			if (store->vtrash)
				camel_vee_folder_add_folder (CAMEL_VEE_FOLDER (store->vtrash), folder);
			
			camel_object_hook_event (CAMEL_OBJECT (folder), "finalize", folder_finalize, store);
			CAMEL_STORE_UNLOCK(store, cache_lock);
		}
	}

	CAMEL_STORE_UNLOCK(store, folder_lock);
	return folder;
}


static CamelFolderInfo *
create_folder (CamelStore *store, const char *parent_name,
	       const char *folder_name, CamelException *ex)
{
	g_warning ("CamelStore::create_folder not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store)));
	return NULL;
}

/** 
 * camel_store_create_folder:
 * @store: a CamelStore
 * @parent_name: name of the new folder's parent, or %NULL
 * @folder_name: name of the folder to create
 * @ex: a CamelException
 * 
 * Creates a new folder as a child of an existing folder.
 * @parent_name can be %NULL to create a new top-level folder.
 *
 * Return value: info about the created folder, which the caller must
 * free with camel_store_free_folder_info().
 **/
CamelFolderInfo *
camel_store_create_folder (CamelStore *store, const char *parent_name,
			   const char *folder_name, CamelException *ex)
{
	CamelFolderInfo *fi;

	CAMEL_STORE_LOCK(store, folder_lock);
	fi = CS_CLASS (store)->create_folder (store, parent_name, folder_name, ex);
	CAMEL_STORE_UNLOCK(store, folder_lock);

	return fi;
}


static void
delete_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	g_warning ("CamelStore::delete_folder not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store)));
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
camel_store_delete_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	CAMEL_STORE_LOCK(store, folder_lock);
	CS_CLASS (store)->delete_folder (store, folder_name, ex);
	CAMEL_STORE_UNLOCK(store, folder_lock);
}


static void
rename_folder (CamelStore *store, const char *old_name,
	       const char *new_name, CamelException *ex)
{
	g_warning ("CamelStore::rename_folder not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store)));
}

/**
 * camel_store_rename_folder:
 * @store: a CamelStore
 * @old_name: the current name of the folder
 * @new_name: the new name of the folder
 * @ex: a CamelException
 * 
 * Rename a named folder to a new name.
 **/
void
camel_store_rename_folder (CamelStore *store, const char *old_name, const char *new_name, CamelException *ex)
{
	CAMEL_STORE_LOCK(store, folder_lock);
	CS_CLASS (store)->rename_folder (store, old_name, new_name, ex);
	CAMEL_STORE_UNLOCK(store, folder_lock);
}


static CamelFolder *
get_inbox (CamelStore *store, CamelException *ex)
{
	/* Default: assume the inbox's name is "inbox"
	 * and open with default flags.
	 */
	return CS_CLASS (store)->get_folder (store, "inbox", 0, ex);
}

/** 
 * camel_store_get_inbox:
 * @store: a CamelStore
 * @ex: a CamelException
 *
 * Return value: the folder in the store into which new mail is
 * delivered, or %NULL if no such folder exists.
 **/
CamelFolder *
camel_store_get_inbox (CamelStore *store, CamelException *ex)
{
	CamelFolder *folder;

	CAMEL_STORE_LOCK(store, folder_lock);
	folder = CS_CLASS (store)->get_inbox (store, ex);
	CAMEL_STORE_UNLOCK(store, folder_lock);

	return folder;
}


static void
trash_add_folder (gpointer key, gpointer value, gpointer data)
{
	CamelFolder *folder = CAMEL_FOLDER (value);
	CamelStore *store = CAMEL_STORE (data);
	
	camel_vee_folder_add_folder (CAMEL_VEE_FOLDER (store->vtrash), folder);
}

static void
trash_finalize (CamelObject *trash, gpointer event_data, gpointer user_data)
{
	CamelStore *store = CAMEL_STORE (user_data);
	
	store->vtrash = NULL;
}

static void
init_trash (CamelStore *store)
{
	char *name;
	
	name = g_strdup_printf ("%s?(match-all (system-flag \"Deleted\"))", "vTrash");
	
	store->vtrash = camel_vee_folder_new (store, name, CAMEL_STORE_FOLDER_CREATE |
					      CAMEL_STORE_VEE_FOLDER_AUTO, NULL);
	
	g_free (name);
	
	if (store->vtrash) {
		/* attach to the finalise event of the vtrash */
		camel_object_hook_event (CAMEL_OBJECT (store->vtrash), "finalize",
					 trash_finalize, store);
		
		/* add all the pre-opened folders to the vtrash */
		if (store->folders)
			g_hash_table_foreach (store->folders, trash_add_folder, store);
	}
}


static CamelFolder *
get_trash (CamelStore *store, CamelException *ex)
{
	if (store->vtrash) {
		camel_object_ref (CAMEL_OBJECT (store->vtrash));
		return store->vtrash;
	} else {
		CS_CLASS (store)->init_trash (store);
		if (store->vtrash) {
			/* We don't ref here because we don't want the
                           store to own a ref on the trash folder */
			/*camel_object_ref (CAMEL_OBJECT (store->vtrash));*/
			return store->vtrash;
		} else {
			g_warning ("This store does not support vTrash.");
			return NULL;
		}
	}
}

/** 
 * camel_store_get_trash:
 * @store: a CamelStore
 * @ex: a CamelException
 *
 * Return value: the folder in the store into which trash is
 * delivered, or %NULL if no such folder exists.
 **/
CamelFolder *
camel_store_get_trash (CamelStore *store, CamelException *ex)
{
	CamelFolder *folder;
	
	CAMEL_STORE_LOCK(store, folder_lock);
	folder = CS_CLASS (store)->get_trash (store, ex);
	CAMEL_STORE_UNLOCK(store, folder_lock);
	
	return folder;
}


static void
sync_folder (gpointer key, gpointer folder, gpointer ex)
{
	if (!camel_exception_is_set (ex))
		camel_folder_sync (folder, FALSE, ex);
}

static void
store_sync (CamelStore *store, CamelException *ex)
{
	if (store->folders) {
		CAMEL_STORE_LOCK(store, cache_lock);
		g_hash_table_foreach (store->folders, sync_folder, ex);
		CAMEL_STORE_UNLOCK(store, cache_lock);
	}
}

/**
 * camel_store_sync:
 * @store: a CamelStore
 * @ex: a CamelException
 *
 * Syncs any changes that have been made to the store object and its
 * folders with the real store.
 **/
void
camel_store_sync (CamelStore *store, CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_STORE (store));

	CS_CLASS (store)->sync (store, ex);
}


static CamelFolderInfo *
get_folder_info (CamelStore *store, const char *top,
		 gboolean fast, gboolean recursive,
		 gboolean subscribed_only,
		 CamelException *ex)
{
	g_warning ("CamelStore::get_folder_info not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store)));
	return NULL;
}

/**
 * camel_store_get_folder_info:
 * @store: a CamelStore
 * @top: the name of the folder to start from
 * @fast: whether or not to do a "fast" scan.
 * @recursive: whether to include information for subfolders
 * @ex: a CamelException
 *
 * This fetches information about the folder structure of @store,
 * starting with @top, and returns a tree of CamelFolderInfo
 * structures. If @fast is %TRUE, the message_count or
 * unread_message_count fields of some or all of the structures may be
 * set to -1, if the store cannot determine that information quickly.
 * If @recursive is %TRUE, the returned tree will include all levels of
 * hierarchy below @top. If it is %FALSE, it will only include the
 * immediate subfolders of @top.
 *
 * Return value: a CamelFolderInfo tree, which must be freed with
 * camel_store_free_folder_info.
 **/
CamelFolderInfo *
camel_store_get_folder_info (CamelStore *store, const char *top,
			     gboolean fast, gboolean recursive,
			     gboolean subscribed_only,
			     CamelException *ex)
{
	CamelFolderInfo *ret;

	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);

	CAMEL_STORE_LOCK(store, folder_lock);

	ret = CS_CLASS (store)->get_folder_info (store, top, fast, recursive, subscribed_only, ex);

	CAMEL_STORE_UNLOCK(store, folder_lock);

	return ret;
}


static void
free_folder_info (CamelStore *store, CamelFolderInfo *fi)
{
	g_warning ("CamelStore::free_folder_info not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store)));
}

/**
 * camel_store_free_folder_info:
 * @store: a CamelStore
 * @tree: the tree returned by camel_store_get_folder_info()
 *
 * Frees the data returned by camel_store_get_folder_info().
 **/
void
camel_store_free_folder_info (CamelStore *store, CamelFolderInfo *fi)
{
	g_return_if_fail (CAMEL_IS_STORE (store));

	CS_CLASS (store)->free_folder_info (store, fi);
}

/**
 * camel_store_free_folder_info_full:
 * @store: a CamelStore
 * @tree: the tree returned by camel_store_get_folder_info()
 *
 * An implementation for CamelStore::free_folder_info. Frees all
 * of the data.
 **/
void
camel_store_free_folder_info_full (CamelStore *store, CamelFolderInfo *fi)
{
	camel_folder_info_free (fi);
}

/**
 * camel_store_free_folder_info_nop:
 * @store: a CamelStore
 * @tree: the tree returned by camel_store_get_folder_info()
 *
 * An implementation for CamelStore::free_folder_info. Does nothing.
 **/
void
camel_store_free_folder_info_nop (CamelStore *store, CamelFolderInfo *fi)
{
	;
}


/**
 * camel_folder_info_free:
 * @fi: the CamelFolderInfo
 *
 * Frees @fi.
 **/
void
camel_folder_info_free (CamelFolderInfo *fi)
{
	if (fi) {
		camel_folder_info_free (fi->sibling);
		camel_folder_info_free (fi->child);
		g_free (fi->name);
		g_free (fi->full_name);
		g_free (fi->url);
		g_free (fi);
	}
}


/**
 * camel_folder_info_build:
 * @folders: an array of CamelFolderInfo
 * @namespace: an ignorable prefix on the folder names
 * @separator: the hieararchy separator character
 * @short_names: %TRUE if the (short) name of a folder is the part after
 * the last @separator in the full name. %FALSE if it is the full name.
 *
 * This takes an array of folders and attaches them together according
 * to the hierarchy described by their full_names and @separator. If
 * @namespace is non-%NULL, then it will be ignored as a full_name
 * prefix, for purposes of comparison. If necessary,
 * camel_folder_info_build will create additional CamelFolderInfo with
 * %NULL urls to fill in gaps in the tree. The value of @short_names
 * is used in constructing the names of these intermediate folders.
 *
 * Return value: the top level of the tree of linked folder info.
 **/
CamelFolderInfo *
camel_folder_info_build (GPtrArray *folders, const char *namespace,
			 char separator, gboolean short_names)
{
	CamelFolderInfo *fi, *pfi, *top = NULL;
	GHashTable *hash;
	char *name, *p, *pname;
	int i, nlen;

	if (!namespace)
		namespace = "";
	nlen = strlen (namespace);

	/* Hash the folders. */
	hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (i = 0; i < folders->len; i++) {
		fi = folders->pdata[i];
		if (!strncmp (namespace, fi->full_name, nlen))
			name = fi->full_name + nlen;
		else
			name = fi->full_name;
		if (*name == separator)
			name++;
		g_hash_table_insert (hash, name, fi);
	}

	/* Now find parents. */
	for (i = 0; i < folders->len; i++) {
		fi = folders->pdata[i];
		if (!strncmp (namespace, fi->full_name, nlen))
			name = fi->full_name + nlen;
		else
			name = fi->full_name;
		if (*name == separator)
			name++;
		p = strrchr (name, separator);
		if (p) {
			pname = g_strndup (name, p - name);
			pfi = g_hash_table_lookup (hash, pname);
			if (pfi) {
				g_free (pname);
			} else {
				pfi = g_new0 (CamelFolderInfo, 1);
				pfi->full_name = pname;
				if (short_names) {
					pfi->name = strrchr (pname, separator);
					if (pfi->name)
						pfi->name = g_strdup (pfi->name + 1);
					else
						pfi->name = g_strdup (pname);
				} else
					pfi->name = g_strdup (pname);
				g_hash_table_insert (hash, pname, pfi);
				g_ptr_array_add (folders, pfi);
			}
			fi->sibling = pfi->child;
			fi->parent = pfi;
			pfi->child = fi;
		} else if (!top)
			top = fi;
	}
	g_hash_table_destroy (hash);

	/* Link together the top-level folders */
	for (i = 0; i < folders->len; i++) {
		fi = folders->pdata[i];
		if (fi->parent || fi == top)
			continue;
		if (top)
			fi->sibling = top;
		top = fi;
	}

	return top;			
}

gboolean
camel_store_supports_subscriptions (CamelStore *store)
{
	return (store->flags & CAMEL_STORE_SUBSCRIPTIONS);
}


static gboolean
folder_subscribed (CamelStore *store, const char *folder_name)
{
	g_warning ("CamelStore::folder_subscribed not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store)));
	return FALSE;
}

/**
 * camel_store_folder_subscribed: Tell whether or not a folder has been subscribed to.
 * @store: a CamelStore
 * @folder_name: the folder on which we're querying subscribed status.
 * Return value: TRUE if folder is subscribed, FALSE if not.
 **/
gboolean
camel_store_folder_subscribed (CamelStore *store,
			       const char *folder_name)
{
	gboolean ret;

	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);
	g_return_val_if_fail (store->flags & CAMEL_STORE_SUBSCRIPTIONS, FALSE);

	CAMEL_STORE_LOCK(store, folder_lock);

	ret = CS_CLASS (store)->folder_subscribed (store, folder_name);

	CAMEL_STORE_UNLOCK(store, folder_lock);

	return ret;
}

static void
subscribe_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	g_warning ("CamelStore::subscribe_folder not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store)));
}

/**
 * camel_store_subscribe_folder: marks a folder as subscribed.
 * @store: a CamelStore
 * @folder_name: the folder to subscribe to.
 **/
void
camel_store_subscribe_folder (CamelStore *store,
			      const char *folder_name,
			      CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (store->flags & CAMEL_STORE_SUBSCRIPTIONS);

	CAMEL_STORE_LOCK(store, folder_lock);

	CS_CLASS (store)->subscribe_folder (store, folder_name, ex);

	CAMEL_STORE_UNLOCK(store, folder_lock);
}

static void
unsubscribe_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	g_warning ("CamelStore::unsubscribe_folder not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store)));
}


/**
 * camel_store_unsubscribe_folder: marks a folder as unsubscribed.
 * @store: a CamelStore
 * @folder_name: the folder to unsubscribe from.
 **/
void
camel_store_unsubscribe_folder (CamelStore *store,
				const char *folder_name,
				CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (store->flags & CAMEL_STORE_SUBSCRIPTIONS);

	CAMEL_STORE_LOCK(store, folder_lock);

	CS_CLASS (store)->unsubscribe_folder (store, folder_name, ex);

	CAMEL_STORE_UNLOCK(store, folder_lock);
}
