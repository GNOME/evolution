/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 * 
 * Authors: Peter Williams <peterw@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2000,2001 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef G_LOG_DOMAIN
#undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN "folder tree"

#include <pthread.h>

#include <bonobo/bonobo-exception.h>
#include <camel/camel-store.h>
#include <camel/camel-folder.h>

#include "mail-mt.h"
#include "mail-folder-cache.h"
#include "mail-ops.h"

#define d(x)

/* note that many things are effectively serialised by having them run in
   the main loop thread which they need to do because of corba/gtk calls */
#define LOCK(x) pthread_mutex_lock(&x)
#define UNLOCK(x) pthread_mutex_unlock(&x)

static pthread_mutex_t info_lock = PTHREAD_MUTEX_INITIALIZER;

struct _folder_info {
	struct _store_info *store_info;	/* 'parent' link */

	char *path;		/* shell path */
	char *name;		/* shell display name? */
	char *full_name;	/* full name of folder/folderinfo */
	CamelFolder *folder;	/* if known */
};

struct _store_info {
	GHashTable *folders;	/* by full_name */

	/* only 1 should be set */
	EvolutionStorage *storage;
	GNOME_Evolution_Storage corba_storage;
};

static GHashTable *stores;

static void
update_1folder(struct _folder_info *mfi, CamelFolderInfo *info)
{
	struct _store_info *si;
	CamelFolder *folder;
	int unread;
	CORBA_Environment ev;

	si  = mfi->store_info;

	LOCK(info_lock);
	folder = mfi->folder;
	if (folder)
		unread = camel_folder_get_unread_message_count(folder);
	else if (info)
		unread = info->unread_message_count;
	else
		unread = -1;
	UNLOCK(info_lock);

	if (si->storage == NULL) {
		d(printf("Updating existing (local) folder: %s\n", mfi->path));
		CORBA_exception_init(&ev);
		GNOME_Evolution_Storage_updateFolder(si->corba_storage, mfi->path, mfi->name, unread, &ev);
		CORBA_exception_free(&ev);
	} else {
		d(printf("Updating existing folder: %s\n", mfi->path));
		evolution_storage_update_folder(si->storage, mfi->path, mfi->name, unread);
	}
}

static void
setup_folder(const char *path, CamelFolderInfo *fi, struct _store_info *si)
{
	struct _folder_info *mfi;
	char *type;
	int unread = fi->unread_message_count;

	LOCK(info_lock);
	mfi = g_hash_table_lookup(si->folders, fi->full_name);
	if (mfi) {
		UNLOCK(info_lock);
		update_1folder(mfi, fi);
	} else {
		/* always 'add it', but only 'add it' to non-local stores */
		d(printf("Adding new folder: %s (%s)\n", path, fi->url));
		mfi = g_malloc0(sizeof(*mfi));
		mfi->path = g_strdup(path);
		mfi->name = g_strdup(fi->name);
		mfi->full_name = g_strdup(fi->full_name);
		mfi->store_info = si;
		g_hash_table_insert(si->folders, mfi->full_name, mfi);
		UNLOCK(info_lock);

		if (si->storage != NULL) {
			type = (strncmp(fi->url, "vtrash:", 7)==0)?"vtrash":"mail";
			evolution_storage_new_folder(si->storage, path, mfi->name, type,
						     fi->url, mfi->name, unread);
		}
	}
}

static void
real_folder_changed(CamelFolder *folder, void *event_data, void *data)
{
	struct _folder_info *mfi = data;

	update_1folder(mfi, NULL);
}

static void
folder_changed(CamelObject *o, gpointer event_data, gpointer user_data)
{
	struct _folder_info *mfi = user_data;

	if (mfi->folder != CAMEL_FOLDER(o))
		return;

	d(printf("Fodler changed!\n"));
	mail_msg_wait(mail_proxy_event((CamelObjectEventHookFunc)real_folder_changed, o, NULL, mfi));
}

static void
folder_finalised(CamelObject *o, gpointer event_data, gpointer user_data)
{
	struct _folder_info *mfi = user_data;

	d(printf("Folder finalised!\n"));
	mfi->folder = NULL;
}

static void
real_note_folder(CamelFolder *folder, char *path, void *data)
{
	CamelStore *store = folder->parent_store;
	struct _store_info *si;
	struct _folder_info *mfi;

	LOCK(info_lock);
	si = g_hash_table_lookup(stores, store);
	UNLOCK(info_lock);
	if (si == NULL) {
		g_free(path);
		g_warning("Adding a folder `%s' to a store which hasn't been added yet?\n", folder->full_name);
		camel_object_unref((CamelObject *)folder);
		return;
	}

	if (path == NULL)
		path = g_strdup_printf("/%s", folder->full_name);

	LOCK(info_lock);
	mfi = g_hash_table_lookup(si->folders, folder->full_name);
	UNLOCK(info_lock);

	if (mfi == NULL) {
		g_warning("Adding a folder `%s' that I dont know about yet?\n path='%s'", folder->full_name, path);
		g_free(path);
		camel_object_unref((CamelObject *)folder);
		return;
	}
	g_free(path);

	/* dont do anything if we already have this */
	if (mfi->folder == folder)
		return;

	mfi->folder = folder;
	update_1folder(mfi, NULL);

	camel_object_hook_event((CamelObject *)folder, "folder_changed", folder_changed, mfi);
	camel_object_hook_event((CamelObject *)folder, "message_changed", folder_changed, mfi);
	camel_object_hook_event((CamelObject *)folder, "finalized", folder_finalised, mfi);

	camel_object_unref((CamelObject *)folder);
}

/* supply path if different from folder->full_name? */
void mail_note_folder(CamelFolder *folder, const char *path)
{
	char *real = NULL;

	if (stores == NULL) {
		g_warning("Adding a folder `%s' to a store which hasn't been added yet?\n", folder->full_name);
		return;
	}

	camel_object_ref((CamelObject *)folder);
	if (path)
		real = g_strdup_printf("/%s", path);
	mail_proxy_event((CamelObjectEventHookFunc)real_note_folder, (CamelObject *)folder, real, NULL);
}

static void
real_folder_created(CamelStore *store, void *event_data, CamelFolderInfo *fi)
{
	struct _store_info *si;
	char *path;

	d(printf("real_folder_created: %s (%s)\n", fi->full_name, fi->url));

	LOCK(info_lock);
	si = g_hash_table_lookup(stores, store);
	UNLOCK(info_lock);
	if (si) {
		path = g_strdup_printf("/%s", fi->full_name);
		setup_folder(path, fi, si);
		g_free(path);
	} else {
		/* leaks, so what */
		g_warning("real_folder_created: can't find store: %s\n",
			  camel_url_to_string(((CamelService *)store)->url, 0));
	}
}

static void
store_folder_created(CamelObject *o, void *event_data, void *data)
{
	CamelFolderInfo *info = event_data;

	d(printf("folder added: %s\n", info->full_name));
	d(printf("uri = '%s'\n", info->url));

	/* dont need to copy info since we're waiting for it to complete */
	mail_msg_wait(mail_proxy_event((CamelObjectEventHookFunc)real_folder_created, o, NULL, info));
}

static void
store_folder_deleted(CamelObject *o, void *event_data, void *data)
{
	CamelStore *store = (CamelStore *)o;
	CamelFolderInfo *info = event_data;

	store = store;
	info = info;

	/* should really remove it? */
	d(printf("folder deleted: %s\n", info->full_name));
}

static void
free_folder_info(char *path, struct _folder_info *info, void *data)
{
	g_free(info->path);
	g_free(info->name);
	g_free(info->full_name);
}

static void
store_finalised(CamelObject *o, void *event_data, void *data)
{
	CamelStore *store = (CamelStore *)o;
	struct _store_info *si;

	d(printf("store finalised!!\n"));
	LOCK(info_lock);
	si = g_hash_table_lookup(stores, store);
	if (si) {
		g_hash_table_remove(stores, store);
		g_hash_table_foreach(si->folders, free_folder_info, NULL);
		g_hash_table_destroy(si->folders);
		g_free(si);
	}
	UNLOCK(info_lock);
}

static void
create_folders(const char *prefix, CamelFolderInfo *fi, struct _store_info *si)
{
	char *path;

	path = g_strdup_printf ("%s/%s", prefix, fi->name);
	setup_folder(path, fi, si);
	
	if (fi->child)
		create_folders(path, fi->child, si);
	
	g_free(path);

	if (fi->sibling)
		create_folders(prefix, fi->sibling, si);
}

static void
update_folders(CamelStore *store, CamelFolderInfo *info, void *data)
{
	struct _store_info *si = data;

	if (info) {
		if (si->storage)
			gtk_object_set_data (GTK_OBJECT (si->storage), "connected", GINT_TO_POINTER (TRUE));
		create_folders("", info, si);
	}
}

static void
setup_store(CamelStore *store, EvolutionStorage *storage, GNOME_Evolution_Storage corba_storage)
{
	struct _store_info *si;

	LOCK(info_lock);

	if (stores == NULL)
		stores = g_hash_table_new(NULL, NULL);

	si = g_hash_table_lookup(stores, store);
	if (si == NULL) {

		d(printf("Noting a new store: %p: %s\n", store, camel_url_to_string(((CamelService *)store)->url, 0)));

		si = g_malloc0(sizeof(*si));
		si->folders = g_hash_table_new(g_str_hash, g_str_equal);
		si->storage = storage;
		si->corba_storage = corba_storage;
		g_hash_table_insert(stores, store, si);

		camel_object_hook_event((CamelObject *)store, "folder_created", store_folder_created, NULL);
		camel_object_hook_event((CamelObject *)store, "folder_deleted", store_folder_deleted, NULL);
		camel_object_hook_event((CamelObject *)store, "finalized", store_finalised, NULL);
	}

	UNLOCK(info_lock);

	mail_msg_wait(mail_get_folderinfo(store, update_folders, si));
}


void mail_note_store(CamelStore *store)
{
	EvolutionStorage *storage;
	/* i'm not including all the sht in mail.h just for this !*/
	EvolutionStorage *mail_lookup_storage (CamelStore *store);

	g_assert(CAMEL_IS_STORE(store));
	g_assert(pthread_self() == mail_gui_thread);

	storage = mail_lookup_storage(store);
	if (storage == NULL) {
		g_warning("Trying to monitor a store for which there is no storage: %s",
			  camel_url_to_string(((CamelService *)store)->url, 0));
		return;
	}

	setup_store(store, storage, CORBA_OBJECT_NIL);
}

void mail_note_local_store(CamelStore *store, GNOME_Evolution_Storage corba_storage)
{
	g_assert(CAMEL_IS_STORE(store));
	g_assert(pthread_self() == mail_gui_thread);

	setup_store(store, NULL, corba_storage);
}
