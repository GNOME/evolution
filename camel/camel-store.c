/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-store.c : Abstract class for an email store */

/* 
 * Authors:
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *  Dan Winship <danw@ximian.com>
 *
 * Copyright 1999-2003 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "camel-debug.h"

#include "camel-session.h"
#include "camel-store.h"
#include "camel-folder.h"
#include "camel-vtrash-folder.h"
#include "camel-exception.h"
#include "camel-private.h"
#include "camel-i18n.h"

#define d(x)
#define w(x)

static CamelServiceClass *parent_class = NULL;

/* Returns the class for a CamelStore */
#define CS_CLASS(so) ((CamelStoreClass *)((CamelObject *)(so))->klass)

static CamelFolder *get_folder (CamelStore *store, const char *folder_name,
				guint32 flags, CamelException *ex);
static CamelFolder *get_inbox (CamelStore *store, CamelException *ex);

static CamelFolder *get_trash (CamelStore *store, CamelException *ex);
static CamelFolder *get_junk (CamelStore *store, CamelException *ex);

static CamelFolderInfo *create_folder (CamelStore *store,
				       const char *parent_name,
				       const char *folder_name,
				       CamelException *ex);
static void delete_folder (CamelStore *store, const char *folder_name,
			   CamelException *ex);
static void rename_folder (CamelStore *store, const char *old_name,
			   const char *new_name, CamelException *ex);

static void store_sync (CamelStore *store, int expunge, CamelException *ex);
static CamelFolderInfo *get_folder_info (CamelStore *store, const char *top,
					 guint32 flags, CamelException *ex);
static void free_folder_info (CamelStore *store, CamelFolderInfo *tree);

static gboolean folder_subscribed (CamelStore *store, const char *folder_name);
static void subscribe_folder (CamelStore *store, const char *folder_name, CamelException *ex);
static void unsubscribe_folder (CamelStore *store, const char *folder_name, CamelException *ex);

static void noop (CamelStore *store, CamelException *ex);

static void construct (CamelService *service, CamelSession *session,
		       CamelProvider *provider, CamelURL *url,
		       CamelException *ex);

static int store_setv (CamelObject *object, CamelException *ex, CamelArgV *args);
static int store_getv (CamelObject *object, CamelException *ex, CamelArgGetV *args);

static void
camel_store_class_init (CamelStoreClass *camel_store_class)
{
	CamelObjectClass *camel_object_class = CAMEL_OBJECT_CLASS (camel_store_class);
	CamelServiceClass *camel_service_class = CAMEL_SERVICE_CLASS(camel_store_class);
	
	parent_class = CAMEL_SERVICE_CLASS (camel_type_get_global_classfuncs (camel_service_get_type ()));
	
	/* virtual method definition */
	camel_store_class->hash_folder_name = g_str_hash;
	camel_store_class->compare_folder_name = g_str_equal;
	camel_store_class->get_folder = get_folder;
	camel_store_class->get_inbox = get_inbox;
	camel_store_class->get_trash = get_trash;
	camel_store_class->get_junk = get_junk;
	camel_store_class->create_folder = create_folder;
	camel_store_class->delete_folder = delete_folder;
	camel_store_class->rename_folder = rename_folder;
	camel_store_class->sync = store_sync;
	camel_store_class->get_folder_info = get_folder_info;
	camel_store_class->free_folder_info = free_folder_info;
	camel_store_class->folder_subscribed = folder_subscribed;
	camel_store_class->subscribe_folder = subscribe_folder;
	camel_store_class->unsubscribe_folder = unsubscribe_folder;
	camel_store_class->noop = noop;
	
	/* virtual method overload */
	camel_service_class->construct = construct;
	
	camel_object_class->setv = store_setv;
	camel_object_class->getv = store_getv;
	
	camel_object_class_add_event(camel_object_class, "folder_opened", NULL);
	camel_object_class_add_event(camel_object_class, "folder_created", NULL);
	camel_object_class_add_event(camel_object_class, "folder_deleted", NULL);
	camel_object_class_add_event(camel_object_class, "folder_renamed", NULL);
	camel_object_class_add_event(camel_object_class, "folder_subscribed", NULL);
	camel_object_class_add_event(camel_object_class, "folder_unsubscribed", NULL);
}

static void
camel_store_init (void *o)
{
	CamelStore *store = o;
	CamelStoreClass *store_class = (CamelStoreClass *)CAMEL_OBJECT_GET_CLASS (o);

	if (store_class->hash_folder_name) {
		store->folders = camel_object_bag_new(store_class->hash_folder_name,
						      store_class->compare_folder_name,
						      (CamelCopyFunc)g_strdup, g_free);
	} else
		store->folders = NULL;
	
	/* set vtrash and vjunk on by default */
	store->flags = CAMEL_STORE_VTRASH | CAMEL_STORE_VJUNK;

	store->priv = g_malloc0 (sizeof (*store->priv));
	store->priv->folder_lock = e_mutex_new (E_MUTEX_REC);
}

static void
camel_store_finalize (CamelObject *object)
{
	CamelStore *store = CAMEL_STORE (object);

	if (store->folders)
		camel_object_bag_destroy(store->folders);
	
	e_mutex_destroy (store->priv->folder_lock);
	
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

static int
store_setv (CamelObject *object, CamelException *ex, CamelArgV *args)
{
	/* CamelStore doesn't currently have anything to set */
	return CAMEL_OBJECT_CLASS (parent_class)->setv (object, ex, args);
}

static int
store_getv (CamelObject *object, CamelException *ex, CamelArgGetV *args)
{
	/* CamelStore doesn't currently have anything to get */
	return CAMEL_OBJECT_CLASS (parent_class)->getv (object, ex, args);
}

static void
construct (CamelService *service, CamelSession *session,
	   CamelProvider *provider, CamelURL *url,
	   CamelException *ex)
{
	CamelStore *store = CAMEL_STORE(service);

	parent_class->construct(service, session, provider, url, ex);
	if (camel_exception_is_set (ex))
		return;

	if (camel_url_get_param(url, "filter"))
		store->flags |= CAMEL_STORE_FILTER_INBOX;
}

static CamelFolder *
get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	w(g_warning ("CamelStore::get_folder not implemented for `%s'",
		     camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store))));
	
	camel_exception_setv (ex, CAMEL_EXCEPTION_STORE_INVALID,
			      _("Cannot get folder: Invalid operation on this store"));
	
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
	
	g_return_val_if_fail (folder_name != NULL, NULL);
	
	/* O_EXCL doesn't make sense if we aren't requesting to also create the folder if it doesn't exist */
	if (!(flags & CAMEL_STORE_FOLDER_CREATE))
		flags &= ~CAMEL_STORE_FOLDER_EXCL;
	
	if (store->folders) {
		/* Try cache first. */
		folder = camel_object_bag_reserve(store->folders, folder_name);
		if (folder && (flags & CAMEL_STORE_FOLDER_EXCL)) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Cannot create folder `%s': folder exists"),
					      folder_name);
			
			camel_object_unref (folder);
			return NULL;
		}
	}
	
	if (!folder) {
		if ((store->flags & CAMEL_STORE_VTRASH) && strcmp(folder_name, CAMEL_VTRASH_NAME) == 0) {
			folder = CS_CLASS(store)->get_trash(store, ex);
		} else if ((store->flags & CAMEL_STORE_VJUNK) && strcmp(folder_name, CAMEL_VJUNK_NAME) == 0) {
			folder = CS_CLASS(store)->get_junk(store, ex);
		} else {
			folder = CS_CLASS (store)->get_folder(store, folder_name, flags, ex);
			if (folder) {
				CamelVeeFolder *vfolder;
				
				if ((store->flags & CAMEL_STORE_VTRASH)
				    && (vfolder = camel_object_bag_get(store->folders, CAMEL_VTRASH_NAME))) {
					camel_vee_folder_add_folder(vfolder, folder);
					camel_object_unref(vfolder);
				}

				if ((store->flags & CAMEL_STORE_VJUNK)
				    && (vfolder = camel_object_bag_get(store->folders, CAMEL_VJUNK_NAME))) {
					camel_vee_folder_add_folder(vfolder, folder);
					camel_object_unref(vfolder);
				}
			}
		}

		if (store->folders) {
			if (folder)
				camel_object_bag_add(store->folders, folder_name, folder);
			else
				camel_object_bag_abort(store->folders, folder_name);
		}

		if (folder)
			camel_object_trigger_event(store, "folder_opened", folder);
	}

	return folder;
}

static CamelFolderInfo *
create_folder (CamelStore *store, const char *parent_name,
	       const char *folder_name, CamelException *ex)
{
	w(g_warning ("CamelStore::create_folder not implemented for `%s'",
		     camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store))));
	
	camel_exception_setv (ex, CAMEL_EXCEPTION_STORE_INVALID,
			      _("Cannot create folder: Invalid operation on this store"));
	
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

	if ((parent_name == NULL || parent_name[0] == 0)
	    && (((store->flags & CAMEL_STORE_VTRASH) && strcmp(folder_name, CAMEL_VTRASH_NAME) == 0)
		|| ((store->flags & CAMEL_STORE_VJUNK) && strcmp(folder_name, CAMEL_VJUNK_NAME) == 0))) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_INVALID,
				     _("Cannot create folder: %s: folder exists"), folder_name);
		return NULL;
	}

	CAMEL_STORE_LOCK(store, folder_lock);
	fi = CS_CLASS (store)->create_folder (store, parent_name, folder_name, ex);
	CAMEL_STORE_UNLOCK(store, folder_lock);

	return fi;
}

/* deletes folder/removes it from the folder cache, if it's there */
static void
cs_delete_cached_folder(CamelStore *store, const char *folder_name)
{
	CamelFolder *folder;

	if (store->folders
	    && (folder = camel_object_bag_get(store->folders, folder_name))) {
		CamelVeeFolder *vfolder;
			
		if ((store->flags & CAMEL_STORE_VTRASH)
		    && (vfolder = camel_object_bag_get(store->folders, CAMEL_VTRASH_NAME))) {
			camel_vee_folder_remove_folder(vfolder, folder);
			camel_object_unref(vfolder);
		}

		if ((store->flags & CAMEL_STORE_VJUNK)
		    && (vfolder = camel_object_bag_get(store->folders, CAMEL_VJUNK_NAME))) {
			camel_vee_folder_remove_folder(vfolder, folder);
			camel_object_unref(vfolder);
		}

		camel_folder_delete(folder);

		camel_object_bag_remove(store->folders, folder);
		camel_object_unref(folder);
	}
}

static void
delete_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	w(g_warning ("CamelStore::delete_folder not implemented for `%s'",
		     camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store))));
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
	CamelException local;

	/* TODO: should probably be a parameter/bit on the storeinfo */
	if (((store->flags & CAMEL_STORE_VTRASH) && strcmp(folder_name, CAMEL_VTRASH_NAME) == 0)
	    || ((store->flags & CAMEL_STORE_VJUNK) && strcmp(folder_name, CAMEL_VJUNK_NAME) == 0)) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Cannot delete folder: %s: Invalid operation"), folder_name);
		return;
	}

	camel_exception_init(&local);

	CAMEL_STORE_LOCK(store, folder_lock);

	CS_CLASS(store)->delete_folder(store, folder_name, &local);

	if (!camel_exception_is_set(&local))
		cs_delete_cached_folder(store, folder_name);
	else
		camel_exception_xfer(ex, &local);
	
	CAMEL_STORE_UNLOCK(store, folder_lock);
}

static void
rename_folder (CamelStore *store, const char *old_name, const char *new_name, CamelException *ex)
{
	w(g_warning ("CamelStore::rename_folder not implemented for `%s'",
		     camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store))));
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
camel_store_rename_folder (CamelStore *store, const char *old_namein, const char *new_name, CamelException *ex)
{
	CamelFolder *folder;
	int i, oldlen, namelen;
	GPtrArray *folders;
	char *old_name;

	d(printf("store rename folder %s '%s' '%s'\n", ((CamelService *)store)->url->protocol, old_name, new_name));

	if (strcmp(old_namein, new_name) == 0)
		return;

	if (((store->flags & CAMEL_STORE_VTRASH) && strcmp(old_namein, CAMEL_VTRASH_NAME) == 0)
	    || ((store->flags & CAMEL_STORE_VJUNK) && strcmp(old_namein, CAMEL_VJUNK_NAME) == 0)) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Cannot rename folder: %s: Invalid operation"), old_namein);
		return;
	}

	/* need to save this, since old_namein might be folder->full_name, which could go away */
	old_name = g_strdup(old_namein);
	oldlen = strlen(old_name);

	CAMEL_STORE_LOCK(store, folder_lock);

	/* If the folder is open (or any subfolders of the open folder)
	   We need to rename them atomically with renaming the actual folder path */
	if (store->folders) {
		folders = camel_object_bag_list(store->folders);
		for (i=0;i<folders->len;i++) {
			folder = folders->pdata[i];
			namelen = strlen(folder->full_name);
			if ((namelen == oldlen &&
			     strcmp(folder->full_name, old_name) == 0)
			    || ((namelen > oldlen)
				&& strncmp(folder->full_name, old_name, oldlen) == 0
				&& folder->full_name[oldlen] == '/')) {
				d(printf("Found subfolder of '%s' == '%s'\n", old_name, folder->full_name));
				CAMEL_FOLDER_LOCK(folder, lock);
			} else {
				g_ptr_array_remove_index_fast(folders, i);
				i--;
				camel_object_unref(folder);
			}
		}
	}

	/* Now try the real rename (will emit renamed event) */
	CS_CLASS (store)->rename_folder (store, old_name, new_name, ex);

	/* If it worked, update all open folders/unlock them */
	if (!camel_exception_is_set(ex)) {
		guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE;
		CamelRenameInfo reninfo;

		for (i=0;i<folders->len;i++) {
			char *new;

			folder = folders->pdata[i];

			new = g_strdup_printf("%s%s", new_name, folder->full_name+strlen(old_name));
			camel_object_bag_rekey(store->folders, folder, new);
			camel_folder_rename(folder, new);
			g_free(new);

			CAMEL_FOLDER_UNLOCK(folder, lock);
			camel_object_unref(folder);
		}

		/* Emit renamed signal */
		if (store->flags & CAMEL_STORE_SUBSCRIPTIONS)
			flags |= CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;
		
		reninfo.old_base = (char *)old_name;
		reninfo.new = ((CamelStoreClass *)((CamelObject *)store)->klass)->get_folder_info(store, new_name, flags, ex);
		if (reninfo.new != NULL) {
			camel_object_trigger_event (store, "folder_renamed", &reninfo);
			((CamelStoreClass *)((CamelObject *)store)->klass)->free_folder_info(store, reninfo.new);
		}
	} else {
		/* Failed, just unlock our folders for re-use */
		for (i=0;i<folders->len;i++) {
			folder = folders->pdata[i];
			CAMEL_FOLDER_UNLOCK(folder, lock);
			camel_object_unref(folder);
		}
	}

	CAMEL_STORE_UNLOCK(store, folder_lock);

	g_ptr_array_free(folders, TRUE);
	g_free(old_name);
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

static CamelFolder *
get_special(CamelStore *store, enum _camel_vtrash_folder_t type)
{
	CamelFolder *folder;
	GPtrArray *folders;
	int i;

	folder = camel_vtrash_folder_new(store, type);
	folders = camel_object_bag_list(store->folders);
	for (i=0;i<folders->len;i++) {
		if (!CAMEL_IS_VTRASH_FOLDER(folders->pdata[i]))
			camel_vee_folder_add_folder((CamelVeeFolder *)folder, (CamelFolder *)folders->pdata[i]);
		camel_object_unref(folders->pdata[i]);
	}
	g_ptr_array_free(folders, TRUE);

	return folder;
}

static CamelFolder *
get_trash(CamelStore *store, CamelException *ex)
{
	return get_special(store, CAMEL_VTRASH_FOLDER_TRASH);
}

static CamelFolder *
get_junk(CamelStore *store, CamelException *ex)
{
	return get_special(store, CAMEL_VTRASH_FOLDER_JUNK);
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
	if ((store->flags & CAMEL_STORE_VTRASH) == 0)
		return CS_CLASS(store)->get_trash(store, ex);
	else
		return camel_store_get_folder(store, CAMEL_VTRASH_NAME, 0, ex);
}

/** 
 * camel_store_get_junk:
 * @store: a CamelStore
 * @ex: a CamelException
 *
 * Return value: the folder in the store into which junk is
 * delivered, or %NULL if no such folder exists.
 **/
CamelFolder *
camel_store_get_junk (CamelStore *store, CamelException *ex)
{
	if ((store->flags & CAMEL_STORE_VJUNK) == 0)
		return CS_CLASS(store)->get_junk(store, ex);
	else
		return camel_store_get_folder(store, CAMEL_VJUNK_NAME, 0, ex);
}

static void
store_sync (CamelStore *store, int expunge, CamelException *ex)
{
	if (store->folders) {
		GPtrArray *folders;
		CamelFolder *folder;
		CamelException x;
		int i;

		/* we don't sync any vFolders, that is used to update certain vfolder queries mainly,
		   and we're really only interested in storing/expunging the physical mails */
		camel_exception_init(&x);
		folders = camel_object_bag_list(store->folders);
		for (i=0;i<folders->len;i++) {
			folder = folders->pdata[i];
			if (!CAMEL_IS_VEE_FOLDER(folder)
			    && !camel_exception_is_set(&x))
				camel_folder_sync(folder, expunge, &x);
			camel_object_unref(folder);
		}
		camel_exception_xfer(ex, &x);
		g_ptr_array_free(folders, TRUE);
	}
}

/**
 * camel_store_sync:
 * @store: a CamelStore
 * @expunge: do we expunge deleted messages too?
 * @ex: a CamelException
 *
 * Syncs any changes that have been made to the store object and its
 * folders with the real store.
 **/
void
camel_store_sync(CamelStore *store, int expunge, CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_STORE (store));

	CS_CLASS(store)->sync(store, expunge, ex);
}

static CamelFolderInfo *
get_folder_info (CamelStore *store, const char *top, guint32 flags, CamelException *ex)
{
	w(g_warning ("CamelStore::get_folder_info not implemented for `%s'",
		     camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store))));
	
	return NULL;
}

static void
add_special_info (CamelStore *store, CamelFolderInfo *info, const char *name, const char *translated, gboolean unread_count)
{
	CamelFolderInfo *fi, *vinfo, *parent;
	char *uri, *path;
	CamelURL *url;
	
	g_return_if_fail (info != NULL);
	
	parent = NULL;
	for (fi = info; fi; fi = fi->next) {
		if (!strcmp (fi->name, name))
			break;
		parent = fi;
	}
	
	/* create our vTrash/vJunk URL */
	url = camel_url_new (info->uri, NULL);
	if (((CamelService *) store)->provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH) {
		camel_url_set_fragment (url, name);
	} else {
		path = g_strdup_printf ("/%s", name);
		camel_url_set_path (url, path);
		g_free (path);
	}
	
	uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
	camel_url_free (url);
	
	if (fi) {
		/* We're going to replace the physical Trash/Junk folder with our vTrash/vJunk folder */
		vinfo = fi;
		g_free (vinfo->full_name);
		g_free (vinfo->name);
		g_free (vinfo->uri);
	} else {
		/* There wasn't a Trash/Junk folder so create a new folder entry */
		vinfo = g_new0 (CamelFolderInfo, 1);
		
		g_assert(parent != NULL);
		
		vinfo->flags |= CAMEL_FOLDER_NOINFERIORS | CAMEL_FOLDER_SUBSCRIBED;
		
		/* link it into the right spot */
		vinfo->next = parent->next;
		parent->next = vinfo;
	}
	
	/* Fill in the new fields */
	vinfo->flags |= CAMEL_FOLDER_VIRTUAL|CAMEL_FOLDER_SYSTEM|CAMEL_FOLDER_VTRASH;
	vinfo->full_name = g_strdup (name);
	vinfo->name = g_strdup (translated);
	vinfo->uri = uri;
	if (!unread_count)
		vinfo->unread = -1;
}

static void
dump_fi(CamelFolderInfo *fi, int depth)
{
	char *s;

	s = g_alloca(depth+1);
	memset(s, ' ', depth);
	s[depth] = 0;

	while (fi) {
		printf("%suri: %s\n", s, fi->uri);
		printf("%sfull_name: %s\n", s, fi->full_name);
		printf("%sflags: %08x\n", s, fi->flags);
		dump_fi(fi->child, depth+2);
		fi = fi->next;
	}
}

/**
 * camel_store_get_folder_info:
 * @store: a CamelStore
 * @top: the name of the folder to start from
 * @flags: various CAMEL_STORE_FOLDER_INFO_* flags to control behavior
 * @ex: a CamelException
 *
 * This fetches information about the folder structure of @store,
 * starting with @top, and returns a tree of CamelFolderInfo
 * structures. If @flags includes %CAMEL_STORE_FOLDER_INFO_SUBSCRIBED,
 * only subscribed folders will be listed. (This flag can only be used
 * for stores that support subscriptions.) If @flags includes
 * %CAMEL_STORE_FOLDER_INFO_RECURSIVE, the returned tree will include
 * all levels of hierarchy below @top. If not, it will only include
 * the immediate subfolders of @top. If @flags includes
 * %CAMEL_STORE_FOLDER_INFO_FAST, the unread_message_count fields of
 * some or all of the structures may be set to -1, if the store cannot
 * determine that information quickly. If @flags includes
 * %CAMEL_STORE_FOLDER_INFO_NO_VIRTUAL, don't include special virtual
 * folders (such as vTrash or vJunk).
 * 
 * Return value: a CamelFolderInfo tree, which must be freed with
 * camel_store_free_folder_info.
 **/
CamelFolderInfo *
camel_store_get_folder_info(CamelStore *store, const char *top, guint32 flags, CamelException *ex)
{
	CamelFolderInfo *info;
	
	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);
	g_return_val_if_fail ((store->flags & CAMEL_STORE_SUBSCRIPTIONS) ||
			      !(flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED),
			      NULL);

	info = CS_CLASS (store)->get_folder_info (store, top, flags, ex);
	
	if (info && (top == NULL || *top == '\0') && (flags & CAMEL_STORE_FOLDER_INFO_NO_VIRTUAL) == 0) {
		if (info->uri && (store->flags & CAMEL_STORE_VTRASH))
			add_special_info (store, info, CAMEL_VTRASH_NAME, _("Trash"), FALSE);
		if (info->uri && (store->flags & CAMEL_STORE_VJUNK))
			add_special_info (store, info, CAMEL_VJUNK_NAME, _("Junk"), TRUE);
	}

	if (camel_debug_start("store:folder_info")) {
		char *url = camel_url_to_string(((CamelService *)store)->url, CAMEL_URL_HIDE_ALL);
		printf("Get folder info(%p:%s, '%s') =\n", store, url, top?top:"<null>");
		g_free(url);
		dump_fi(info, 2);
		camel_debug_end();
	}
	
	return info;
}

static void
free_folder_info (CamelStore *store, CamelFolderInfo *fi)
{
	w(g_warning ("CamelStore::free_folder_info not implemented for `%s'",
		     camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store))));
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
		camel_folder_info_free (fi->next);
		camel_folder_info_free (fi->child);
		g_free (fi->name);
		g_free (fi->full_name);
		g_free (fi->uri);
		g_free (fi);
	}
}

static int
folder_info_cmp (const void *ap, const void *bp)
{
	const CamelFolderInfo *a = ((CamelFolderInfo **)ap)[0];
	const CamelFolderInfo *b = ((CamelFolderInfo **)bp)[0];
	
	return strcmp (a->full_name, b->full_name);
}

static void
free_name(void *key, void *data, void *user)
{
	g_free(key);
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
 * NOTE: This is deprected, do not use this.
 * FIXME: remove this/move it to imap, which is the only user of it now.
 *
 * Return value: the top level of the tree of linked folder info.
 **/
CamelFolderInfo *
camel_folder_info_build (GPtrArray *folders, const char *namespace,
			 char separator, gboolean short_names)
{
	CamelFolderInfo *fi, *pfi, *top = NULL, *tail = NULL;
	GHashTable *hash;
	char *name, *p, *pname;
	int i, nlen;
	
	if (!namespace)
		namespace = "";
	nlen = strlen (namespace);

	qsort (folders->pdata, folders->len, sizeof (folders->pdata[0]), folder_info_cmp);
	
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

		g_hash_table_insert (hash, g_strdup(name), fi);
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
				/* we are missing a folder in the heirarchy so
				   create a fake folder node */
				CamelURL *url;
				char *sep;

				pfi = g_new0 (CamelFolderInfo, 1);
				if (short_names) {
					pfi->name = strrchr (pname, separator);
					if (pfi->name)
						pfi->name = g_strdup (pfi->name + 1);
					else
						pfi->name = g_strdup (pname);
				} else
					pfi->name = g_strdup (pname);

				/* FIXME: url's with fragments should have the fragment truncated, not path */
				url = camel_url_new (fi->uri, NULL);
				sep = strrchr (url->path, separator);
				if (sep)
					*sep = '\0';
				else
					d(g_warning ("huh, no \"%c\" in \"%s\"?", separator, fi->url));
				
				pfi->full_name = g_strdup(url->path+1);

				/* since this is a "fake" folder node, it is not selectable */
				camel_url_set_param (url, "noselect", "yes");
				pfi->uri = camel_url_to_string (url, 0);
				camel_url_free (url);

				g_hash_table_insert (hash, pname, pfi);
				g_ptr_array_add (folders, pfi);
			}
			tail = (CamelFolderInfo *)&pfi->child;
			while (tail->next)
				tail = tail->next;
			tail->next = fi;
			fi->parent = pfi;
		} else if (!top)
			top = fi;
	}
	g_hash_table_foreach(hash, free_name, NULL);
	g_hash_table_destroy (hash);

	/* Link together the top-level folders */
	tail = top;
	for (i = 0; i < folders->len; i++) {
		fi = folders->pdata[i];
		if (fi->parent || fi == top)
			continue;
		if (tail == NULL) {
			tail = fi;
			top = fi;
		} else {
			tail->next = fi;
			tail = fi;
		}
	}
	
	return top;
}

static CamelFolderInfo *folder_info_clone_rec(CamelFolderInfo *fi, CamelFolderInfo *parent)
{
	CamelFolderInfo *info;

	info = g_malloc(sizeof(*info));
	info->parent = parent;
	info->uri = g_strdup(fi->uri);
	info->name = g_strdup(fi->name);
	info->full_name = g_strdup(fi->full_name);
	info->unread = fi->unread;
	info->flags = fi->flags;
	
	if (fi->next)
		info->next = folder_info_clone_rec(fi->next, parent);
	else
		info->next = NULL;

	if (fi->child)
		info->child = folder_info_clone_rec(fi->child, info);
	else
		info->child = NULL;

	return info;
}

CamelFolderInfo *
camel_folder_info_clone(CamelFolderInfo *fi)
{
	if (fi == NULL)
		return NULL;

	return folder_info_clone_rec(fi, NULL);
}

gboolean
camel_store_supports_subscriptions (CamelStore *store)
{
	return (store->flags & CAMEL_STORE_SUBSCRIPTIONS);
}

static gboolean
folder_subscribed(CamelStore *store, const char *folder_name)
{
	w(g_warning ("CamelStore::folder_subscribed not implemented for `%s'",
		     camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store))));
	
	return FALSE;
}

/**
 * camel_store_folder_subscribed: Tell whether or not a folder has been subscribed to.
 * @store: a CamelStore
 * @folder_name: the folder on which we're querying subscribed status.
 * Return value: TRUE if folder is subscribed, FALSE if not.
 **/
gboolean
camel_store_folder_subscribed(CamelStore *store, const char *folder_name)
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
subscribe_folder(CamelStore *store, const char *folder_name, CamelException *ex)
{
	w(g_warning ("CamelStore::subscribe_folder not implemented for `%s'",
		     camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store))));
}

/**
 * camel_store_subscribe_folder: marks a folder as subscribed.
 * @store: a CamelStore
 * @folder_name: the folder to subscribe to.
 **/
void
camel_store_subscribe_folder(CamelStore *store, const char *folder_name, CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (store->flags & CAMEL_STORE_SUBSCRIPTIONS);

	CAMEL_STORE_LOCK(store, folder_lock);

	CS_CLASS (store)->subscribe_folder (store, folder_name, ex);

	CAMEL_STORE_UNLOCK(store, folder_lock);
}

static void
unsubscribe_folder(CamelStore *store, const char *folder_name, CamelException *ex)
{
	w(g_warning ("CamelStore::unsubscribe_folder not implemented for `%s'",
		     camel_type_to_name (CAMEL_OBJECT_GET_TYPE (store))));
}

/**
 * camel_store_unsubscribe_folder: marks a folder as unsubscribed.
 * @store: a CamelStore
 * @folder_name: the folder to unsubscribe from.
 **/
void
camel_store_unsubscribe_folder(CamelStore *store, const char *folder_name, CamelException *ex)
{
	CamelException local;

	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (store->flags & CAMEL_STORE_SUBSCRIPTIONS);

	camel_exception_init(&local);

	CAMEL_STORE_LOCK(store, folder_lock);

	CS_CLASS (store)->unsubscribe_folder (store, folder_name, ex);

	if (!camel_exception_is_set(&local))
		cs_delete_cached_folder(store, folder_name);
	else
		camel_exception_xfer(ex, &local);

	CAMEL_STORE_UNLOCK(store, folder_lock);
}

static void
noop (CamelStore *store, CamelException *ex)
{
	/* no-op */
	;
}

/**
 * camel_store_noop:
 * @store: CamelStore
 * @ex: exception
 *
 * Pings @store so that its connection doesn't timeout.
 **/
void
camel_store_noop (CamelStore *store, CamelException *ex)
{
	CS_CLASS (store)->noop (store, ex);
}


/**
 * camel_store_folder_uri_equal:
 * @store: CamelStore
 * @uri0: a uri
 * @uri1: another uri
 *
 * Compares 2 folder uris to check that they are equal.
 *
 * Returns %TRUE if they are equal or %FALSE otherwise.
 **/
int
camel_store_folder_uri_equal (CamelStore *store, const char *uri0, const char *uri1)
{
	CamelProvider *provider;
	CamelURL *url0, *url1;
	int equal;
	
	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);
	g_return_val_if_fail (uri0 && uri1, FALSE);
	
	provider = ((CamelService *) store)->provider;
	
	if (!(url0 = camel_url_new (uri0, NULL)))
		return FALSE;
	
	if (!(url1 = camel_url_new (uri1, NULL))) {
		camel_url_free (url0);
		return FALSE;
	}
	
	if ((equal = provider->url_equal (url0, url1))) {
		const char *name0, *name1;
		
		if (provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH) {
			name0 = url0->fragment;
			name1 = url1->fragment;
		} else {
			name0 = url0->path[0] == '/' ? url0->path + 1 : url0->path;
			name1 = url1->path[0] == '/' ? url1->path + 1 : url1->path;
		}
		
		equal = CS_CLASS (store)->compare_folder_name (name0, name1);
	}
	
	camel_url_free (url0);
	camel_url_free (url1);
	
	return equal;
}
