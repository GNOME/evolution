/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 * 
 * Authors: Peter Williams <peterw@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2000,2001 Ximian, Inc. (www.ximian.com)
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
#include "config.h"
#endif

#ifdef G_LOG_DOMAIN
#undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN "folder tree"

#include <pthread.h>
#include <string.h>
#include <time.h>

#include <libgnome/gnome-sound.h>
#include <libgnome/gnome-i18n.h>
#include <bonobo/bonobo-exception.h>
#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <camel/camel-vtrash-folder.h>
#include <camel/camel-vee-store.h>
#include <camel/camel-disco-store.h>

#include "mail-mt.h"
#include "mail-folder-cache.h"
#include "mail-ops.h"
#include "mail-session.h"
#include "mail-component.h"

/* For notifications of changes */
#include "mail-vfolder.h"
#include "mail-autofilter.h"
#include "mail-config.h"
#include "em-folder-tree-model.h"

#include "em-event.h"

#define w(x) 
#define d(x)

/* This code is a mess, there is no reason it should be so complicated. */

/* note that many things are effectively serialised by having them run in
   the main loop thread which they need to do because of corba/gtk calls */
#define LOCK(x) pthread_mutex_lock(&x)
#define UNLOCK(x) pthread_mutex_unlock(&x)

static pthread_mutex_t info_lock = PTHREAD_MUTEX_INITIALIZER;

struct _folder_info {
	struct _store_info *store_info;	/* 'parent' link */

	char *full_name;	/* full name of folder/folderinfo */
	char *uri;		/* uri of folder */
	
	guint32 flags;
	
	CamelFolder *folder;	/* if known */
};

/* pending list of updates */
struct _folder_update {
	struct _folder_update *next;
	struct _folder_update *prev;

	unsigned int remove:1;	/* removing from vfolders */
	unsigned int delete:1;	/* deleting as well? */
	unsigned int add:1;	/* add to vfolder */
	unsigned int unsub:1;   /* unsubcribing? */
	unsigned int new:1;     /* new mail arrived? */

	char *full_name;
	char *uri;
	char *oldfull;
	char *olduri;

	int unread;
	CamelStore *store;
};

struct _store_info {
	GHashTable *folders;	/* by full_name */
	GHashTable *folders_uri; /* by uri */

	CamelStore *store;	/* the store for these folders */

	/* Outstanding folderinfo requests */
	EDList folderinfo_updates;
};

static void folder_changed(CamelObject *o, gpointer event_data, gpointer user_data);
static void folder_renamed(CamelObject *o, gpointer event_data, gpointer user_data);
static void folder_finalised(CamelObject *o, gpointer event_data, gpointer user_data);

static guint ping_id = 0;
static gboolean ping_cb (gpointer user_data);

static guint notify_id = 0;
static int notify_type = -1;

static time_t last_notify = 0;
static guint notify_idle_id = 0;
static gboolean notify_idle_cb (gpointer user_data);


/* Store to storeinfo table, active stores */
static GHashTable *stores = NULL;

/* List of folder changes to be executed in gui thread */
static EDList updates = E_DLIST_INITIALISER(updates);
static int update_id = -1;

/* hack for people who LIKE to have unsent count */
static int count_sent = FALSE;
static int count_trash = FALSE;

static void
free_update(struct _folder_update *up)
{
	g_free(up->full_name);
	g_free(up->uri);
	if (up->store)
		camel_object_unref(up->store);
	g_free(up->oldfull);
	g_free(up->olduri);
	g_free(up);
}

static gboolean
notify_idle_cb (gpointer user_data)
{
	GConfClient *gconf;
	char *filename;
	
	gconf = mail_config_get_gconf_client ();
	
	switch (notify_type) {
	case MAIL_CONFIG_NOTIFY_PLAY_SOUND:
		filename = gconf_client_get_string (gconf, "/apps/evolution/mail/notify/sound", NULL);
		if (filename != NULL) {
			gnome_sound_play (filename);
			g_free (filename);
		}
		break;
	case MAIL_CONFIG_NOTIFY_BEEP:
		gdk_beep ();
		break;
	default:
		break;
	}
	
	time (&last_notify);
	
	notify_idle_id = 0;
	
	return FALSE;
}

static void
notify_type_changed (GConfClient *client, guint cnxn_id,
		     GConfEntry *entry, gpointer user_data)
{
	notify_type = gconf_client_get_int (client, "/apps/evolution/mail/notify/type", NULL);
}

static void
real_flush_updates(void *o, void *event_data, void *data)
{
	struct _MailComponent *component;
	struct _EMFolderTreeModel *model;
	struct _folder_update *up;
	time_t now;
	
	component = mail_component_peek ();
	model = mail_component_peek_tree_model (component);
	
	LOCK(info_lock);
	while ((up = (struct _folder_update *)e_dlist_remhead(&updates))) {
		UNLOCK(info_lock);

		if (up->remove) {
			if (up->delete) {
				mail_vfolder_delete_uri(up->store, up->uri);
				mail_filter_delete_uri(up->store, up->uri);
				mail_config_uri_deleted(CAMEL_STORE_CLASS(CAMEL_OBJECT_GET_CLASS(up->store))->compare_folder_name, up->uri);

			} else
				mail_vfolder_add_uri(up->store, up->uri, TRUE);
		} else {
			/* We can tell the vfolder code though */
			if (up->olduri && up->add) {
				d(printf("renaming folder '%s' to '%s'\n", up->olduri, up->uri));
				mail_vfolder_rename_uri(up->store, up->olduri, up->uri);
				mail_filter_rename_uri(up->store, up->olduri, up->uri);
				mail_config_uri_renamed(CAMEL_STORE_CLASS(CAMEL_OBJECT_GET_CLASS(up->store))->compare_folder_name,
							up->olduri, up->uri);
			}
			
			if (!up->olduri && up->add)
				mail_vfolder_add_uri(up->store, up->uri, FALSE);
		}
		
		/* update unread counts */
		em_folder_tree_model_set_unread_count (model, up->store, up->full_name, up->unread);
		
		/* new mail notification */
		if (notify_type == -1) {
			/* need to track the user's new-mail-notification settings... */
			GConfClient *gconf;
			
			gconf = mail_config_get_gconf_client ();
			gconf_client_add_dir (gconf, "/apps/evolution/mail/notify",
					      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
			notify_id = gconf_client_notify_add (gconf, "/apps/evolution/mail/notify",
							     notify_type_changed, NULL, NULL, NULL);
			notify_type = gconf_client_get_int (gconf, "/apps/evolution/mail/notify/type", NULL);
		}
		
		time (&now);
		if (notify_type != 0 && up->new && notify_idle_id == 0 && (now - last_notify >= 5))
			notify_idle_id = g_idle_add_full (G_PRIORITY_LOW, notify_idle_cb, NULL, NULL);
		
		if (up->uri) {
			EMEvent *e = em_event_peek();
			EMEventTargetFolder *t = em_event_target_new_folder(e, up->uri, up->new?EM_EVENT_FOLDER_NEWMAIL:0);

			/**
			 * @Event: folder.changed
			 * @Title: Folder changed
			 * @Target: EMEventTargetFolder
			 * 
			 * folder.changed is emitted whenever a folder changes.  There is no detail on how the folder has changed.
			 */
			e_event_emit((EEvent *)e, "folder.changed", (EEventTarget *)t);
		}

		free_update(up);
		
		LOCK(info_lock);
	}
	update_id = -1;
	UNLOCK(info_lock);
}

static void
flush_updates(void)
{
	if (update_id == -1 && !e_dlist_empty(&updates))
		update_id = mail_async_event_emit(mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc)real_flush_updates, 0, 0, 0);
}

static void
unset_folder_info(struct _folder_info *mfi, int delete, int unsub)
{
	struct _folder_update *up;

	d(printf("unset folderinfo '%s'\n", mfi->uri));

	if (mfi->folder) {
		CamelFolder *folder = mfi->folder;

		camel_object_unhook_event(folder, "folder_changed", folder_changed, NULL);
		camel_object_unhook_event(folder, "renamed", folder_renamed, NULL);
		camel_object_unhook_event(folder, "finalize", folder_finalised, NULL);
	}

	if ((mfi->flags & CAMEL_FOLDER_NOSELECT) == 0) {
		up = g_malloc0(sizeof(*up));

		up->remove = TRUE;
		up->delete = delete;
		up->unsub = unsub;
		up->store = mfi->store_info->store;
		up->full_name = g_strdup (mfi->full_name);
		camel_object_ref(up->store);
		up->uri = g_strdup(mfi->uri);

		e_dlist_addtail(&updates, (EDListNode *)up);
		flush_updates();
	}
}

static void
free_folder_info(struct _folder_info *mfi)
{
	g_free(mfi->full_name);
	g_free(mfi->uri);
	g_free(mfi);
}

/* This is how unread counts work (and don't work):
 *
 * camel_folder_unread_message_count() only gives a correct answer if
 * the store is paying attention to the folder. (Some stores always
 * pay attention to all folders, but IMAP can only pay attention to
 * one folder at a time.) But it doesn't have any way to know when
 * it's lying, so it's only safe to call it when you know for sure
 * that the store is paying attention to the folder, such as when it's
 * just been created, or you get a folder_changed signal on it.
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
update_1folder(struct _folder_info *mfi, int new, CamelFolderInfo *info)
{
	struct _store_info *si;
	struct _folder_update *up;
	CamelFolder *folder;
	int unread = -1;
	int deleted;
	
	si  = mfi->store_info;

	folder = mfi->folder;
	if (folder) {
		d(printf("update 1 folder '%s'\n", folder->full_name));
		if ((count_trash && (CAMEL_IS_VTRASH_FOLDER (folder)))
		    || folder == mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_OUTBOX)
		    || (count_sent && folder == mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_SENT))) {
			d(printf(" total count\n"));
			unread = camel_folder_get_message_count (folder);
			if (folder == mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_OUTBOX)) {
				if ((deleted = camel_folder_get_deleted_message_count (folder)) > 0)
					unread -= deleted;
			}
		} else {
			d(printf(" unread count\n"));
			if (info)
				unread = info->unread;
			else
				unread = camel_folder_get_unread_message_count (folder);
		}
	} else if (info)
		unread = info->unread;

	d(printf("folder updated: unread %d: '%s'\n", unread, mfi->full_name));

	if (unread == -1)
		return;

	up = g_malloc0(sizeof(*up));
	up->full_name = g_strdup(mfi->full_name);
	up->unread = unread;
	up->new = new ? 1 : 0;
	up->store = mfi->store_info->store;
	up->uri = g_strdup(mfi->uri);
	camel_object_ref(up->store);
	e_dlist_addtail(&updates, (EDListNode *)up);
	flush_updates();
}

static void
setup_folder(CamelFolderInfo *fi, struct _store_info *si)
{
	struct _folder_info *mfi;
	struct _folder_update *up;

	mfi = g_hash_table_lookup(si->folders, fi->full_name);
	if (mfi) {
		update_1folder(mfi, 0, fi);
	} else {
		/*d(printf("Adding new folder: %s (%s) %d unread\n", fi->path, fi->url, fi->unread_message_count));*/
		mfi = g_malloc0(sizeof(*mfi));
		mfi->full_name = g_strdup(fi->full_name);
		mfi->uri = g_strdup(fi->uri);
		mfi->store_info = si;
		mfi->flags = fi->flags;
		
		g_hash_table_insert(si->folders, mfi->full_name, mfi);
		g_hash_table_insert(si->folders_uri, mfi->uri, mfi);

		up = g_malloc0(sizeof(*up));
		up->full_name = g_strdup(mfi->full_name);
		up->uri = g_strdup(fi->uri);
		up->unread = (fi->unread==-1)?0:fi->unread;
		up->store = si->store;
		camel_object_ref(up->store);
		
		if ((fi->flags & CAMEL_FOLDER_NOSELECT) == 0)
			up->add = TRUE;

		e_dlist_addtail(&updates, (EDListNode *)up);
		flush_updates();
	}
}

static void
create_folders(CamelFolderInfo *fi, struct _store_info *si)
{
	d(printf("Setup new folder: %s\n  %s\n", fi->uri, fi->full_name));

	while (fi) {
		setup_folder(fi, si);

		if (fi->child)
			create_folders(fi->child, si);

		fi = fi->next;
	}
}

static void
folder_changed (CamelObject *o, gpointer event_data, gpointer user_data)
{
	CamelFolderChangeInfo *changes = event_data;
	CamelFolder *folder = (CamelFolder *)o;
	CamelStore *store = folder->parent_store;
	struct _store_info *si;
	struct _folder_info *mfi;
	int new = 0;
	
	d(printf("folder '%s' changed\n", folder->full_name));

	if (!CAMEL_IS_VEE_FOLDER(folder)
	    && folder != mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_OUTBOX)
	    && folder != mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_DRAFTS)
	    && folder != mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_SENT)
	    && changes && changes->uid_added)
		new = changes->uid_added->len;
	
	LOCK(info_lock);
	if (stores != NULL
	    && (si = g_hash_table_lookup(stores, store)) != NULL
	    && (mfi = g_hash_table_lookup(si->folders, folder->full_name)) != NULL
	    && mfi->folder == folder) {
		update_1folder(mfi, new, NULL);
	}
	UNLOCK(info_lock);
}

static void
folder_finalised(CamelObject *o, gpointer event_data, gpointer user_data)
{
	CamelFolder *folder = (CamelFolder *)o;
	CamelStore *store = folder->parent_store;
	struct _store_info *si;
	struct _folder_info *mfi;

	d(printf("Folder finalised '%s'!\n", ((CamelFolder *)o)->full_name));
	LOCK(info_lock);
	if (stores != NULL
	    && (si = g_hash_table_lookup(stores, store)) != NULL
	    && (mfi = g_hash_table_lookup(si->folders, folder->full_name)) != NULL
	    && mfi->folder == folder) {
		mfi->folder = NULL;
	}
	UNLOCK(info_lock);
}

static void
folder_renamed(CamelObject *o, gpointer event_data, gpointer user_data)
{
	CamelFolder *folder = (CamelFolder *)o;
	char *old = event_data;

	d(printf("Folder renamed from '%s' to '%s'\n", old, folder->full_name));

	old = old;
	folder = folder;
	/* Dont do anything, do it from the store rename event? */
}

void mail_note_folder(CamelFolder *folder)
{
	CamelStore *store = folder->parent_store;
	struct _store_info *si;
	struct _folder_info *mfi;

	d(printf("noting folder '%s'\n", folder->full_name));

	LOCK(info_lock);
	if (stores == NULL
	    || (si = g_hash_table_lookup(stores, store)) == NULL
	    || (mfi = g_hash_table_lookup(si->folders, folder->full_name)) == NULL) {
		w(g_warning("Noting folder before store initialised"));
		UNLOCK(info_lock);
		return;
	}

	/* dont do anything if we already have this */
	if (mfi->folder == folder) {
		UNLOCK(info_lock);
		return;
	}

	mfi->folder = folder;

	update_1folder(mfi, 0, NULL);

	UNLOCK(info_lock);

	camel_object_hook_event(folder, "folder_changed", folder_changed, NULL);
	camel_object_hook_event(folder, "renamed", folder_renamed, NULL);
	camel_object_hook_event(folder, "finalize", folder_finalised, NULL);
}

static void
store_folder_subscribed(CamelObject *o, void *event_data, void *data)
{
	struct _store_info *si;
	CamelFolderInfo *fi = event_data;

	d(printf("Store folder subscribed '%s' store '%s' \n", fi->full_name, camel_url_to_string(((CamelService *)o)->url, 0)));

	LOCK(info_lock);
	si = g_hash_table_lookup(stores, o);
	if (si)
		setup_folder(fi, si);
	UNLOCK(info_lock);
}

static void
store_folder_created(CamelObject *o, void *event_data, void *data)
{
	/* we only want created events to do more work if we dont support subscriptions */
	if (!camel_store_supports_subscriptions(CAMEL_STORE(o)))
		store_folder_subscribed(o, event_data, data);
}

static void
store_folder_opened(CamelObject *o, void *event_data, void *data)
{
	CamelFolder *folder = event_data;

	mail_note_folder(folder);
}

static void
store_folder_unsubscribed(CamelObject *o, void *event_data, void *data)
{
	struct _store_info *si;
	CamelFolderInfo *fi = event_data;
	struct _folder_info *mfi;
	CamelStore *store = (CamelStore *)o;

	d(printf("Store Folder deleted: %s\n", fi->full_name));

	LOCK(info_lock);
	si = g_hash_table_lookup(stores, store);
	if (si) {
		mfi = g_hash_table_lookup(si->folders, fi->full_name);
		if (mfi) {
			g_hash_table_remove(si->folders, mfi->full_name);
			g_hash_table_remove(si->folders_uri, mfi->uri);
			unset_folder_info(mfi, TRUE, TRUE);
			free_folder_info(mfi);
		}
	}
	UNLOCK(info_lock);
}

static void
store_folder_deleted(CamelObject *o, void *event_data, void *data)
{
	/* we only want deleted events to do more work if we dont support subscriptions */
	if (!camel_store_supports_subscriptions(CAMEL_STORE(o)))
		store_folder_unsubscribed(o, event_data, data);
}

static void
rename_folders(struct _store_info *si, const char *oldbase, const char *newbase, CamelFolderInfo *fi)
{
	char *old;
	struct _folder_info *mfi;
	struct _folder_update *up;

	up = g_malloc0(sizeof(*up));

	d(printf("oldbase '%s' newbase '%s' new '%s'\n", oldbase, newbase, fi->full_name));

	/* Form what was the old name, and try and look it up */
	old = g_strdup_printf("%s%s", oldbase, fi->full_name + strlen(newbase));
	mfi = g_hash_table_lookup(si->folders, old);
	if (mfi) {
		d(printf("Found old folder '%s' renaming to '%s'\n", mfi->full_name, fi->full_name));

		up->oldfull = mfi->full_name;
		up->olduri = mfi->uri;

		/* Its a rename op */
		g_hash_table_remove(si->folders, mfi->full_name);
		g_hash_table_remove(si->folders, mfi->uri);
		mfi->full_name = g_strdup(fi->full_name);
		mfi->uri = g_strdup(fi->uri);
		mfi->flags = fi->flags;
		
		g_hash_table_insert(si->folders, mfi->full_name, mfi);
		g_hash_table_insert(si->folders_uri, mfi->uri, mfi);
	} else {
		d(printf("Rename found a new folder? old '%s' new '%s'\n", old, fi->full_name));
		/* Its a new op */
		mfi = g_malloc0(sizeof(*mfi));
		mfi->full_name = g_strdup(fi->full_name);
		mfi->uri = g_strdup(fi->uri);
		mfi->store_info = si;
		mfi->flags = fi->flags;
		
		g_hash_table_insert(si->folders, mfi->full_name, mfi);
		g_hash_table_insert(si->folders_uri, mfi->uri, mfi);
	}

	g_free(old);

	up->full_name = g_strdup(mfi->full_name);
	up->uri = g_strdup(mfi->uri);
	up->unread = fi->unread==-1?0:fi->unread;
	up->store = si->store;
	camel_object_ref(up->store);
	
	if ((fi->flags & CAMEL_FOLDER_NOSELECT) == 0)
		up->add = TRUE;

	e_dlist_addtail(&updates, (EDListNode *)up);
	flush_updates();
#if 0
	if (fi->sibling)
		rename_folders(si, oldbase, newbase, fi->sibling, folders);
	if (fi->child)
		rename_folders(si, oldbase, newbase, fi->child, folders);
#endif
}

static void
get_folders(CamelFolderInfo *fi, GPtrArray *folders)
{
	while (fi) {
		g_ptr_array_add(folders, fi);

		if (fi->child)
			get_folders(fi->child, folders);

		fi = fi->next;
	}
}

static int
folder_cmp(const void *ap, const void *bp)
{
	const CamelFolderInfo *a = ((CamelFolderInfo **)ap)[0];
	const CamelFolderInfo *b = ((CamelFolderInfo **)bp)[0];

	return strcmp(a->full_name, b->full_name);
}

static void
store_folder_renamed(CamelObject *o, void *event_data, void *data)
{
	CamelStore *store = (CamelStore *)o;
	CamelRenameInfo *info = event_data;
	struct _store_info *si;

	d(printf("Folder renamed: oldbase = '%s' new->full = '%s'\n", info->old_base, info->new->full_name));

	LOCK(info_lock);
	si = g_hash_table_lookup(stores, store);
	if (si) {
		GPtrArray *folders = g_ptr_array_new();
		CamelFolderInfo *top;
		int i;

		/* Ok, so for some reason the folderinfo we have comes in all messed up from
		   imap, should find out why ... this makes it workable */
		get_folders(info->new, folders);
		qsort(folders->pdata, folders->len, sizeof(folders->pdata[0]), folder_cmp);

		top = folders->pdata[0];
		for (i=0;i<folders->len;i++) {
			rename_folders(si, info->old_base, top->full_name, folders->pdata[i]);
		}

		g_ptr_array_free(folders, TRUE);

	}
	UNLOCK(info_lock);
}

struct _update_data {
	struct _update_data *next;
	struct _update_data *prev;
	
	int id;			/* id for cancellation */
	int cancel:1;		/* also tells us we're cancelled */

	void (*done)(CamelStore *store, CamelFolderInfo *info, void *data);
	void *data;
};

static void
unset_folder_info_hash(char *path, struct _folder_info *mfi, void *data)
{
	unset_folder_info(mfi, FALSE, FALSE);
}

static void
free_folder_info_hash(char *path, struct _folder_info *mfi, void *data)
{
	free_folder_info(mfi);
}

void
mail_note_store_remove(CamelStore *store)
{
	struct _update_data *ud;
	struct _store_info *si;

	g_assert(CAMEL_IS_STORE(store));

	if (stores == NULL)
		return;

	d(printf("store removed!!\n"));
	LOCK(info_lock);
	si = g_hash_table_lookup(stores, store);
	if (si) {
		g_hash_table_remove(stores, store);

		camel_object_unhook_event(store, "folder_opened", store_folder_opened, NULL);
		camel_object_unhook_event(store, "folder_created", store_folder_created, NULL);
		camel_object_unhook_event(store, "folder_deleted", store_folder_deleted, NULL);
		camel_object_unhook_event(store, "folder_renamed", store_folder_renamed, NULL);
		camel_object_unhook_event(store, "folder_subscribed", store_folder_subscribed, NULL);
		camel_object_unhook_event(store, "folder_unsubscribed", store_folder_unsubscribed, NULL);
		g_hash_table_foreach(si->folders, (GHFunc)unset_folder_info_hash, NULL);

		ud = (struct _update_data *)si->folderinfo_updates.head;
		while (ud->next) {
			d(printf("Cancelling outstanding folderinfo update %d\n", ud->id));
			mail_msg_cancel(ud->id);
			ud->cancel = 1;
			ud = ud->next;
		}
		
		camel_object_unref(si->store);
		g_hash_table_foreach(si->folders, (GHFunc)free_folder_info_hash, NULL);
		g_hash_table_destroy(si->folders);
		g_hash_table_destroy(si->folders_uri);
		g_free(si);
	}

	UNLOCK(info_lock);
}

static void
update_folders(CamelStore *store, CamelFolderInfo *fi, void *data)
{
	struct _update_data *ud = data;
	struct _store_info *si;

	d(printf("Got folderinfo for store\n"));

	LOCK(info_lock);
	si = g_hash_table_lookup(stores, store);
	if (si && !ud->cancel) {
		/* the 'si' is still there, so we can remove ourselves from its list */
		/* otherwise its not, and we're on our own and free anyway */
		e_dlist_remove((EDListNode *)ud);

		if (fi)
			create_folders(fi, si);
	}
	UNLOCK(info_lock);

	if (ud->done)
		ud->done(store, fi, ud->data);
	g_free(ud);
}


struct _ping_store_msg {
	struct _mail_msg msg;
	
	CamelStore *store;
};

static char *
ping_store_desc (struct _mail_msg *mm, int done)
{
	struct _ping_store_msg *m = (struct _ping_store_msg *) mm;
	char *service_name = camel_service_get_name (CAMEL_SERVICE (m->store), TRUE);
	char *msg;
	
	msg = g_strdup_printf (_("Pinging %s"), service_name);
	g_free (service_name);
	
	return msg;
}

static void
ping_store_ping (struct _mail_msg *mm)
{
	struct _ping_store_msg *m = (struct _ping_store_msg *) mm;
	
	if (CAMEL_SERVICE (m->store)->status == CAMEL_SERVICE_CONNECTED)
		camel_store_noop (m->store, &mm->ex);
}

static void
ping_store_free (struct _mail_msg *mm)
{
	struct _ping_store_msg *m = (struct _ping_store_msg *) mm;
	
	camel_object_unref (m->store);
}

static struct _mail_msg_op ping_store_op = {
	ping_store_desc,
	ping_store_ping,
	NULL,
	ping_store_free
};

static void
ping_store (gpointer key, gpointer val, gpointer user_data)
{
	CamelStore *store = (CamelStore *) key;
	struct _ping_store_msg *m;
	
	if (CAMEL_SERVICE (store)->status != CAMEL_SERVICE_CONNECTED)
		return;
	
	m = mail_msg_new (&ping_store_op, NULL, sizeof (struct _ping_store_msg));
	m->store = store;
	camel_object_ref (store);
	
	e_thread_put (mail_thread_queued_slow, (EMsg *) m);
}

static gboolean
ping_cb (gpointer user_data)
{
	LOCK (info_lock);
	
	g_hash_table_foreach (stores, ping_store, NULL);
	
	UNLOCK (info_lock);
	
	return TRUE;
}

static void
store_online_cb (CamelStore *store, void *data)
{
	struct _update_data *ud = data;

	LOCK(info_lock);

	if (g_hash_table_lookup(stores, store) != NULL && !ud->cancel) {
		/* re-use the cancel id.  we're already in the store update list too */
		ud->id = mail_get_folderinfo(store, NULL, update_folders, ud);
	} else {
		/* the store vanished, that means we were probably cancelled, or at any rate,
		   need to clean ourselves up */
		g_free(ud);
	}

	UNLOCK(info_lock);
}

void
mail_note_store(CamelStore *store, CamelOperation *op,
		void (*done)(CamelStore *store, CamelFolderInfo *info, void *data), void *data)
{
	struct _store_info *si;
	struct _update_data *ud;
	const char *buf;
	guint timeout;
	int hook = 0;
	
	g_assert(CAMEL_IS_STORE(store));
	g_assert(pthread_self() == mail_gui_thread);

	LOCK(info_lock);

	if (stores == NULL) {
		stores = g_hash_table_new(NULL, NULL);
		count_sent = getenv("EVOLUTION_COUNT_SENT") != NULL;
		count_trash = getenv("EVOLUTION_COUNT_TRASH") != NULL;
		buf = getenv ("EVOLUTION_PING_TIMEOUT");
		timeout = buf ? strtoul (buf, NULL, 10) * 1000 : 600000;
		ping_id = g_timeout_add (timeout, ping_cb, NULL);
	}

	si = g_hash_table_lookup(stores, store);
	if (si == NULL) {
		d(printf("Noting a new store: %p: %s\n", store, camel_url_to_string(((CamelService *)store)->url, 0)));

		si = g_malloc0(sizeof(*si));
		si->folders = g_hash_table_new(g_str_hash, g_str_equal);
		si->folders_uri = g_hash_table_new(CAMEL_STORE_CLASS(CAMEL_OBJECT_GET_CLASS(store))->hash_folder_name,
						   CAMEL_STORE_CLASS(CAMEL_OBJECT_GET_CLASS(store))->compare_folder_name);
		si->store = store;
		camel_object_ref((CamelObject *)store);
		g_hash_table_insert(stores, store, si);
		e_dlist_init(&si->folderinfo_updates);
		hook = TRUE;
	}

	/* We might get a race when setting up a store, such that it is still left in offline mode,
	   after we've gone online.  This catches and fixes it up when the shell opens us */
	if (CAMEL_IS_DISCO_STORE(store)
	    && camel_session_is_online(session)
	    && camel_disco_store_status (CAMEL_DISCO_STORE (store)) == CAMEL_DISCO_STORE_OFFLINE) {
		ud = g_malloc(sizeof(*ud));
		ud->done = done;
		ud->data = data;
		ud->cancel = 0;
		/* Note: we use the 'id' here, even though its not the right id, its still ok */
		ud->id = mail_store_set_offline (store, FALSE, store_online_cb, ud);

		e_dlist_addtail (&si->folderinfo_updates, (EDListNode *) ud);
	} else if (!CAMEL_IS_DISCO_STORE(store)
		   || camel_disco_store_status (CAMEL_DISCO_STORE (store)) == CAMEL_DISCO_STORE_ONLINE
		   || camel_disco_store_can_work_offline (CAMEL_DISCO_STORE (store))) {
		ud = g_malloc (sizeof (*ud));
		ud->done = done;
		ud->data = data;
		ud->cancel = 0;
		ud->id = mail_get_folderinfo (store, op, update_folders, ud);

		e_dlist_addtail (&si->folderinfo_updates, (EDListNode *) ud);
	}

	UNLOCK(info_lock);

	/* there is potential for race here, but it is safe as we check for the store anyway */
	if (hook) {
		camel_object_hook_event(store, "folder_opened", store_folder_opened, NULL);
		camel_object_hook_event(store, "folder_created", store_folder_created, NULL);
		camel_object_hook_event(store, "folder_deleted", store_folder_deleted, NULL);
		camel_object_hook_event(store, "folder_renamed", store_folder_renamed, NULL);
		camel_object_hook_event(store, "folder_subscribed", store_folder_subscribed, NULL);
		camel_object_hook_event(store, "folder_unsubscribed", store_folder_unsubscribed, NULL);
	}
}

struct _find_info {
	const char *uri;
	struct _folder_info *fi;
	CamelURL *url;
};

/* look up on each storeinfo using proper hash function for that stores uri's */
static void storeinfo_find_folder_info(CamelStore *store, struct _store_info *si, struct _find_info *fi)
{
	if (fi->fi == NULL) {
		if (((CamelService *)store)->provider->url_equal(fi->url, ((CamelService *)store)->url)) {
			char *path = fi->url->fragment?fi->url->fragment:fi->url->path;

			if (path[0] == '/')
				path++;
			fi->fi = g_hash_table_lookup(si->folders, path);
		}
	}
}

/* returns TRUE if the uri is available, folderp is set to a
   reffed folder if the folder has also already been opened */
int mail_note_get_folder_from_uri(const char *uri, CamelFolder **folderp)
{
	struct _find_info fi = { uri, NULL, NULL };

	if (stores == NULL)
		return FALSE;

	fi.url = camel_url_new(uri, NULL);

	LOCK(info_lock);
	g_hash_table_foreach(stores, (GHFunc)storeinfo_find_folder_info, &fi);
	if (folderp) {
		if (fi.fi && fi.fi->folder) {
			*folderp = fi.fi->folder;
			camel_object_ref(*folderp);
		} else {
			*folderp = NULL;
		}
	}
	UNLOCK(info_lock);

	camel_url_free(fi.url);

	return fi.fi != NULL;
}
