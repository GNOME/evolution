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
#include <camel/camel-vtrash-folder.h>

#include "mail-mt.h"
#include "mail-folder-cache.h"
#include "mail-ops.h"
#include "mail-vfolder.h"

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
	char *uri;		/* uri of folder */

	CamelFolder *folder;	/* if known */
};

struct _store_info {
	GHashTable *folders;	/* by full_name */
	GHashTable *folders_uri; /* by uri */

	CamelStore *store;	/* the store for these folders */

	/* only 1 should be set */
	EvolutionStorage *storage;
	GNOME_Evolution_Storage corba_storage;
};

static GHashTable *stores;

/* This is how unread counts work (and don't work):
 *
 * camel_folder_unread_message_count() only gives a correct answer if
 * the store is paying attention to the folder. (Some stores always
 * pay attention to all folders, but IMAP can only pay attention to
 * one folder at a time.) But it doesn't have any way to know when
 * it's lying, so it's only safe to call it when you know for sure
 * that the store is paying attention to the folder, such as when it's
 * just been created, or you get a folder_changed or message_changed
 * signal on it.
 *
 * camel_store_get_folder_info() always gives correct answers for the
 * folders it checks, but it can also return -1 for a folder, meaning
 * it didn't check, and so you should stick with your previous answer.
 *
 * update_1folder is called from three places: with info != NULL when
 * the folder is created (or get_folder_info), with info == NULL when
 * a folder changed event is emitted.
 *
 * So if info is NULL, camel_folder_unread_message_count is correct,
 * and if it's not NULL and its unread_message_count isn't -1, then
 * it's correct.  */

static void
update_1folder(struct _folder_info *mfi, CamelFolderInfo *info)
{
	struct _store_info *si;
	CamelFolder *folder;
	int unread = -1;
	CORBA_Environment ev;
	extern CamelFolder *outbox_folder, *sent_folder;

	si  = mfi->store_info;

	LOCK(info_lock);
	folder = mfi->folder;
	if (folder) {
		if (CAMEL_IS_VTRASH_FOLDER (folder) || folder == outbox_folder || folder == sent_folder) {
			unread = camel_folder_get_message_count(folder);
		} else {
			if (info)
				unread = info->unread_message_count;
			else
				unread = camel_folder_get_unread_message_count (folder);
		}
	} else if (info)
		unread = info->unread_message_count;
	UNLOCK(info_lock);
	if (unread == -1)
		return;

	if (si->storage == NULL) {
		d(printf("Updating existing (local) folder: %s (%d unread) folder=%p\n", mfi->path, unread, folder));
		CORBA_exception_init(&ev);
		GNOME_Evolution_Storage_updateFolder(si->corba_storage, mfi->path, mfi->name, unread, &ev);
		CORBA_exception_free(&ev);
	} else {
		d(printf("Updating existing folder: %s (%d unread)\n", mfi->path, unread));
		evolution_storage_update_folder(si->storage, mfi->path, mfi->name, unread);
	}
}

static void
setup_folder(CamelFolderInfo *fi, struct _store_info *si)
{
	struct _folder_info *mfi;
	char *type;
	CamelStore *store;

	LOCK(info_lock);
	mfi = g_hash_table_lookup(si->folders, fi->full_name);
	if (mfi) {
		UNLOCK(info_lock);
		update_1folder(mfi, fi);
	} else {
		/* always 'add it', but only 'add it' to non-local stores */
		d(printf("Adding new folder: %s (%s) %d unread\n", fi->path, fi->url, fi->unread_message_count));
		mfi = g_malloc0(sizeof(*mfi));
		mfi->path = g_strdup(fi->path);
		mfi->name = g_strdup(fi->name);
		mfi->full_name = g_strdup(fi->full_name);
		mfi->uri = g_strdup(fi->url);
		mfi->store_info = si;
		g_hash_table_insert(si->folders, mfi->full_name, mfi);
		g_hash_table_insert(si->folders_uri, mfi->uri, mfi);
		store = si->store;
		camel_object_ref((CamelObject *)store);
		UNLOCK(info_lock);

		if (si->storage != NULL) {
			int unread = (fi->unread_message_count==-1)?0:fi->unread_message_count;

			type = (strncmp(fi->url, "vtrash:", 7)==0)?"vtrash":"mail";
			evolution_storage_new_folder(si->storage, mfi->path, mfi->name, type,
						     fi->url, mfi->name, unread);
		}

		if (strstr(fi->url, ";noselect") == NULL)
			mail_vfolder_add_uri(store, fi->url);

		camel_object_unref((CamelObject *)store);
	}
}

static void
real_folder_changed(CamelFolder *folder, void *event_data, void *data)
{
	struct _folder_info *mfi = data;

	update_1folder(mfi, NULL);
	camel_object_unref((CamelObject *)folder);
}

static void
folder_changed(CamelObject *o, gpointer event_data, gpointer user_data)
{
	struct _folder_info *mfi = user_data;

	if (mfi->folder != CAMEL_FOLDER(o))
		return;

	d(printf("Fodler changed!\n"));
	/* hopefully our mfi isn't lost while this is executing ... */
	camel_object_ref((CamelObject *)o);
	mail_proxy_event((CamelObjectEventHookFunc)real_folder_changed, o, NULL, mfi);
}

static void
folder_finalised(CamelObject *o, gpointer event_data, gpointer user_data)
{
	struct _folder_info *mfi = user_data;

	(printf("Folder finalised '%s'!\n", ((CamelFolder *)o)->full_name));
	mfi->folder = NULL;
}

static void
real_note_folder(CamelFolder *folder, void *event_data, void *data)
{
	CamelStore *store = folder->parent_store;
	struct _store_info *si;
	struct _folder_info *mfi;

	LOCK(info_lock);
	si = g_hash_table_lookup(stores, store);
	UNLOCK(info_lock);
	if (si == NULL) {
		g_warning("Adding a folder `%s' to a store %p which hasn't been added yet?\n", folder->full_name, store);
		camel_object_unref((CamelObject *)folder);
		return;
	}

	LOCK(info_lock);
	mfi = g_hash_table_lookup(si->folders, folder->full_name);
	UNLOCK(info_lock);

	if (mfi == NULL) {
		g_warning("Adding a folder `%s' that I dont know about yet?", folder->full_name);
		camel_object_unref((CamelObject *)folder);
		return;
	}

	/* dont do anything if we already have this */
	if (mfi->folder == folder)
		return;

	mfi->folder = folder;
	update_1folder(mfi, NULL);

	camel_object_hook_event((CamelObject *)folder, "folder_changed", folder_changed, mfi);
	camel_object_hook_event((CamelObject *)folder, "message_changed", folder_changed, mfi);
	camel_object_hook_event((CamelObject *)folder, "finalize", folder_finalised, mfi);

	camel_object_unref((CamelObject *)folder);
}

void mail_note_folder(CamelFolder *folder)
{
	if (stores == NULL) {
		g_warning("Adding a folder `%s' to a store which hasn't been added yet?\n", folder->full_name);
		return;
	}

	camel_object_ref((CamelObject *)folder);
	mail_proxy_event((CamelObjectEventHookFunc)real_note_folder, (CamelObject *)folder, NULL, NULL);
}

static void
real_folder_created(CamelStore *store, void *event_data, CamelFolderInfo *fi)
{
	struct _store_info *si;

	d(printf("real_folder_created: %s (%s)\n", fi->full_name, fi->url));

	LOCK(info_lock);
	si = g_hash_table_lookup(stores, store);
	UNLOCK(info_lock);
	if (si)
		setup_folder(fi, si);
	else
		/* leaks, so what */
		g_warning("real_folder_created: can't find store: %s\n",
			  camel_url_to_string(((CamelService *)store)->url, 0));
}

static void
store_folder_created(CamelObject *o, void *event_data, void *data)
{
	/* we only want created events to do more work if we dont support subscriptions */
	if (!camel_store_supports_subscriptions(CAMEL_STORE(o)))
		mail_msg_wait(mail_proxy_event((CamelObjectEventHookFunc)real_folder_created, o, NULL, event_data));
}

static void
store_folder_subscribed(CamelObject *o, void *event_data, void *data)
{
	mail_msg_wait(mail_proxy_event((CamelObjectEventHookFunc)real_folder_created, o, NULL, event_data));
}

static void
real_folder_deleted(CamelStore *store, void *event_data, CamelFolderInfo *fi)
{
	d(printf("real_folder_deleted: %s (%s)\n", fi->full_name, fi->url));
	
	if (strstr(fi->url, ";noselect") == NULL)
		mail_vfolder_remove_uri(store, fi->url);
}

static void
store_folder_deleted(CamelObject *o, void *event_data, void *data)
{
	/* we only want deleted events to do more work if we dont support subscriptions */
	if (!camel_store_supports_subscriptions(CAMEL_STORE(o)))
		mail_msg_wait(mail_proxy_event((CamelObjectEventHookFunc)real_folder_deleted, o, NULL, event_data));
}

static void
store_folder_unsubscribed(CamelObject *o, void *event_data, void *data)
{
	mail_msg_wait(mail_proxy_event((CamelObjectEventHookFunc)real_folder_deleted, o, NULL, event_data));
}

static void
free_folder_info(char *path, struct _folder_info *info, void *data)
{
	g_free(info->path);
	g_free(info->name);
	g_free(info->full_name);
	g_free(info->uri);
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
		g_hash_table_foreach(si->folders, (GHFunc)free_folder_info, NULL);
		g_hash_table_destroy(si->folders);
		g_free(si);
	}
	UNLOCK(info_lock);
}

static void
create_folders(CamelFolderInfo *fi, struct _store_info *si)
{
	printf("Setup new folder: %s\n", fi->url);

	setup_folder(fi, si);

	if (fi->child)
		create_folders(fi->child, si);
	if (fi->sibling)
		create_folders(fi->sibling, si);
}

struct _update_data {
	struct _store_info *si;
	void (*done)(CamelStore *store, CamelFolderInfo *info, void *data);
	void *data;
};

static void
update_folders(CamelStore *store, CamelFolderInfo *info, void *data)
{
	struct _update_data *ud = data;

	if (info) {
		if (ud->si->storage)
			gtk_object_set_data (GTK_OBJECT (ud->si->storage), "connected", GINT_TO_POINTER (TRUE));
		create_folders(info, ud->si);
	}
	if (ud->done)
		ud->done(store, info, ud->data);
	g_free(ud);
}

void
mail_note_store(CamelStore *store, EvolutionStorage *storage, GNOME_Evolution_Storage corba_storage,
		void (*done)(CamelStore *store, CamelFolderInfo *info, void *data), void *data)
{
	struct _store_info *si;
	struct _update_data *ud;

	g_assert(CAMEL_IS_STORE(store));
	g_assert(pthread_self() == mail_gui_thread);
	g_assert(storage != NULL || corba_storage != CORBA_OBJECT_NIL);

	LOCK(info_lock);

	if (stores == NULL)
		stores = g_hash_table_new(NULL, NULL);

	si = g_hash_table_lookup(stores, store);
	if (si == NULL) {

		d(printf("Noting a new store: %p: %s\n", store, camel_url_to_string(((CamelService *)store)->url, 0)));

		/* FIXME: Need to ref the storages & store or something?? */

		si = g_malloc0(sizeof(*si));
		si->folders = g_hash_table_new(g_str_hash, g_str_equal);
		si->folders_uri = g_hash_table_new(CAMEL_STORE_CLASS(CAMEL_OBJECT_GET_CLASS(store))->hash_folder_name,
						   CAMEL_STORE_CLASS(CAMEL_OBJECT_GET_CLASS(store))->compare_folder_name);
		si->storage = storage;
		si->corba_storage = corba_storage;
		si->store = store;
		g_hash_table_insert(stores, store, si);

		camel_object_hook_event((CamelObject *)store, "folder_created", store_folder_created, NULL);
		camel_object_hook_event((CamelObject *)store, "folder_deleted", store_folder_deleted, NULL);
		camel_object_hook_event((CamelObject *)store, "folder_subscribed", store_folder_subscribed, NULL);
		camel_object_hook_event((CamelObject *)store, "folder_unsubscribed", store_folder_unsubscribed, NULL);
		camel_object_hook_event((CamelObject *)store, "finalize", store_finalised, NULL);
	}

	UNLOCK(info_lock);

	ud = g_malloc(sizeof(*ud));
	ud->si = si;
	ud->done = done;
	ud->data = data;

	mail_get_folderinfo(store, update_folders, ud);
}

struct _find_info {
	const char *uri;
	struct _folder_info *fi;
};

/* look up on each storeinfo using proper hash function for that stores uri's */
static void storeinfo_find_folder_info(CamelStore *store, struct _store_info *si, struct _find_info *fi)
{
	if (fi->fi == NULL)
		fi->fi = g_hash_table_lookup(si->folders_uri, fi->uri);
}

/* returns TRUE if the uri is available, folderp is set to a
   reffed folder if the folder has also already been opened */
int mail_note_get_folder_from_uri(const char *uri, CamelFolder **folderp)
{
	struct _find_info fi = { uri, NULL };

	if (stores == NULL)
		return FALSE;

	LOCK(info_lock);
	g_hash_table_foreach(stores, (GHFunc)storeinfo_find_folder_info, &fi);
	if (folderp) {
		if (fi.fi && fi.fi->folder) {
			*folderp = fi.fi->folder;
			camel_object_ref((CamelObject *)*folderp);
		} else {
			*folderp = NULL;
		}
	}
	UNLOCK(info_lock);

	return fi.fi != NULL;
}
