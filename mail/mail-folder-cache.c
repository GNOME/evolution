/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-folder-cache.c: Stores information about open folders */

/* 
 * Authors: Peter Williams <peterw@ximian.com>
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
#define G_LOG_DOMAIN "folder cache"

#include <bonobo/bonobo-exception.h>

#include "mail-mt.h"
#include "mail-folder-cache.h"

#define ld(x) 
#define d(x) 

/* Structures */

typedef enum mail_folder_info_flags {
	MAIL_FIF_UNREAD_VALID = (1 << 0),
	MAIL_FIF_TOTAL_VALID = (1 << 1),
	MAIL_FIF_HIDDEN_VALID = (1 << 2),
	MAIL_FIF_FOLDER_VALID = (1 << 3),
	MAIL_FIF_NEED_UPDATE = (1 << 4),
	MAIL_FIF_PATH_VALID = (1 << 5),
	MAIL_FIF_NAME_VALID = (1 << 6),
	MAIL_FIF_FB_VALID = (1 << 8),
	MAIL_FIF_SELECTED_VALID = (1 << 9)
} mfif;

typedef enum mail_folder_info_update_mode {
	MAIL_FIUM_UNKNOWN,
	MAIL_FIUM_EVOLUTION_STORAGE,
	MAIL_FIUM_LOCAL_STORAGE /*,*/
	/*MAIL_FIUM_SHELL_VIEW*/
} mfium;

typedef union mail_folder_info_update_info {
	EvolutionStorage *es;
	GNOME_Evolution_Storage ls;
} mfiui;

typedef struct _mail_folder_info {
	gchar *uri;
	gchar *path;

	CamelFolder *folder;
	gchar *name;

	guint flags;
	guint unread, total, hidden, selected;

	FolderBrowser *fb;

	int timeout_id;
	int mail_info_id;

	mfium update_mode;
	mfiui update_info;
} mail_folder_info;

static GHashTable *folders = NULL;
static GStaticMutex folders_lock = G_STATIC_MUTEX_INIT;

#define LOCK_FOLDERS()   G_STMT_START { ld(g_message ("Locking folders")); g_static_mutex_lock (&folders_lock); } G_STMT_END
#define UNLOCK_FOLDERS() G_STMT_START { ld(g_message ("Unocking folders")); g_static_mutex_unlock (&folders_lock); } G_STMT_END

static GNOME_Evolution_ShellView shell_view = CORBA_OBJECT_NIL;
static FolderBrowser *folder_browser = NULL;

extern CamelFolder *outbox_folder;

/* Private functions */

/* call this with the folders locked */
static mail_folder_info *
get_folder_info (const gchar *uri)
{
	mail_folder_info *mfi;

	g_return_val_if_fail (uri, NULL);

	if (folders == NULL) {
		d(g_message("Initializing"));
		folders = g_hash_table_new (g_str_hash, g_str_equal);
	}

	mfi = g_hash_table_lookup (folders, uri);

	if (!mfi) {
		d(g_message("New entry for uri %s", uri));

		mfi = g_new (mail_folder_info, 1);
		mfi->uri = g_strdup (uri); /* XXX leak leak leak */
		mfi->path = NULL;
		mfi->folder = NULL;
		mfi->name = NULL;
		mfi->fb = NULL;
		mfi->flags = 0;
		mfi->update_mode = MAIL_FIUM_UNKNOWN;
		mfi->update_info.es = NULL;
		mfi->timeout_id = 0;
		mfi->mail_info_id = 0;

		g_hash_table_insert (folders, mfi->uri, mfi);
	} else
		d(g_message("Hit cache for uri %s", uri));

	return mfi;
}

/* call with the folders locked */

static gchar *
make_folder_status (mail_folder_info *mfi)
{
	gboolean set_one = FALSE;
	GString *work;
	gchar *ret;

	/* Build the display string */

	work = g_string_new ("");

	if (mfi->flags & MAIL_FIF_UNREAD_VALID) {
		g_string_sprintfa (work, _("%d new"), mfi->unread);
		set_one = TRUE;
	}
		
	if (mfi->flags & MAIL_FIF_HIDDEN_VALID && mfi->hidden) {
		if (set_one)
			work = g_string_append (work, _(", "));
		g_string_sprintfa (work, _("%d hidden"), mfi->hidden);
		set_one = TRUE;
	}
		
	if (mfi->flags & MAIL_FIF_SELECTED_VALID && mfi->selected > 1) {
		if (set_one)
			work = g_string_append (work, _(", "));
		g_string_sprintfa (work, _("%d selected"), mfi->selected);
		set_one = TRUE;
	}
		
	if (mfi->flags & MAIL_FIF_TOTAL_VALID) {
		if (set_one)
			work = g_string_append (work, _(", "));

		if (mfi->flags & MAIL_FIF_FOLDER_VALID && mfi->folder == outbox_folder)
			g_string_sprintfa (work, _("%d unsent"), mfi->total);
		else
			g_string_sprintfa (work, _("%d total"), mfi->total);
	}

	ret = work->str;
	g_string_free (work, FALSE);
	return ret;
}

static gboolean
update_idle (gpointer user_data)
{
	mail_folder_info *mfi = (mail_folder_info *) user_data;
	gchar *f_status;
	gchar *uri, *path;
	mfiui info;
	mfium mode;
	FolderBrowser *fb;
	CORBA_Environment ev;

	LOCK_FOLDERS ();

	d(g_message("update_idle called"));

	mfi->timeout_id = 0;

	/* Check if this makes sense */

	if (!(mfi->flags & MAIL_FIF_NAME_VALID)) {
		g_warning ("Folder cache update info of \'%s\' without \'name\' set", mfi->uri);
		UNLOCK_FOLDERS ();
		return FALSE;
	}

	if (mfi->update_mode == MAIL_FIUM_UNKNOWN) {
		g_warning ("Folder cache update info of \'%s\' without \'mode\' set", mfi->uri);
		UNLOCK_FOLDERS ();
		return FALSE;
	}

	/* Get the display string */

	f_status = make_folder_status (mfi);

	/* Set the value */

	/* Who knows how long these corba calls will take? 
	 * Copy the data from mfi so we can UNLOCK_FOLDERS
	 * before the calls.
	 */

	info = mfi->update_info;
	uri = g_strdup (mfi->uri);
	if (mfi->flags & MAIL_FIF_PATH_VALID)
		path = g_strdup (mfi->path);
	else
		path = NULL;
	mode = mfi->update_mode;
	if (mfi->flags & MAIL_FIF_FB_VALID)
		fb = mfi->fb;
	else
		fb = NULL;

	UNLOCK_FOLDERS ();

	switch (mode) {
	case MAIL_FIUM_LOCAL_STORAGE:
		CORBA_exception_init (&ev);
		GNOME_Evolution_Storage_updateFolder (info.ls,
						      mfi->path,
						      mfi->name,
						      mfi->unread,
						      &ev);
		if (BONOBO_EX (&ev))
			g_warning ("Exception in local storage update: %s",
				   bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		break;
	case MAIL_FIUM_EVOLUTION_STORAGE:
		d(g_message("Updating via EvolutionStorage"));
		evolution_storage_update_folder_by_uri (info.es,
							uri,
							mfi->name,
							mfi->unread);
		break;
	case MAIL_FIUM_UNKNOWN:
	default:
		g_assert_not_reached ();
		break;
	}

	/* Now set the folder bar if reasonable -- we need a shell view,
	 * and the active folder browser should be the one associated with
	 * this MFI */

	if (shell_view != CORBA_OBJECT_NIL &&
	    fb && folder_browser == fb) {
		d(g_message("Updating via ShellView"));
		CORBA_exception_init (&ev);
		GNOME_Evolution_ShellView_setFolderBarLabel (shell_view,
							     f_status,
							     &ev);
		if (BONOBO_EX (&ev))
			g_warning ("Exception in folder bar label update: %s",
				   bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);

#if 0
		GNOME_Evolution_ShellView_setTitle (shell_view,
						    wide,
						    &ev);
		if (BONOBO_EX (&ev))
			g_warning ("Exception in shell view title update: %s",
				   bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
#endif
	}

	/* Cleanup */

	g_free (uri);
	g_free (path);
	g_free (f_status);
	return FALSE;
}

static void
maybe_update (mail_folder_info *mfi)
{
	if (mfi->timeout_id)
		return;

	mfi->timeout_id = g_timeout_add (100, update_idle, mfi);
}

static void
update_message_counts_main (CamelObject *object, gpointer event_data,
			    gpointer user_data)
{
	mail_folder_info *mfi;
	gchar *uri = (gchar *) user_data;

	LOCK_FOLDERS ();
	
	mfi = get_folder_info (uri);

	/* if folder is no longer valid, these numbers are useless */
	if (mfi->flags & MAIL_FIF_FOLDER_VALID) {
		d(g_message("Message counts in CamelFolder changed, queuing idle"));
		mfi->flags &= (~MAIL_FIF_NEED_UPDATE);
		maybe_update (mfi);
	}

	UNLOCK_FOLDERS ();

	g_free (uri);
}

static void
update_message_counts (CamelObject *object, gpointer event_data,
		       gpointer user_data)
{
	CamelFolder *folder = CAMEL_FOLDER (object);
	mail_folder_info *mfi;
	gchar *uri = g_quark_to_string (GPOINTER_TO_UINT (user_data));
	int unread;
	int total;

	d(g_message("CamelFolder %p changed, examining message counts", object));

	unread = camel_folder_get_unread_message_count (folder);
	total = camel_folder_get_message_count (folder);

	LOCK_FOLDERS ();

	mfi = get_folder_info (uri);

	/* we might have been removed in the meantime... this could 
	 * conceivably fail if _note_folder was called immediately
	 * after the URL was removed, and we would get the counts
	 * for the wrong folder, but the chances of that happening
	 * are miniscule.
	 */

	if (!(mfi->flags & MAIL_FIF_FOLDER_VALID)) {
		d(g_message("-> Folder is no longer valid, cancelling updates"));
		UNLOCK_FOLDERS ();
		return;
	}

	mfi->flags &= (~MAIL_FIF_NEED_UPDATE);

	/* '-1' seems to show up a lot, just skip it.
	 * Probably a better way. */
	if (unread == -1) {
		/* nuttzing */
	} else if (mfi->flags & MAIL_FIF_UNREAD_VALID) {
		if (mfi->unread != unread) {
			d(g_message("-> Unread value is changed"));
			mfi->unread = unread;
			mfi->flags |= MAIL_FIF_NEED_UPDATE;
		} else 
			d(g_message("-> Unread value is the same"));
	} else {
		d(g_message("-> Unread value being initialized"));
		mfi->flags |= (MAIL_FIF_UNREAD_VALID | MAIL_FIF_NEED_UPDATE);
		mfi->unread = unread;
	}

	if (mfi->flags & MAIL_FIF_TOTAL_VALID) {
		if (mfi->total != total) {
			d(g_message("-> Total value is changed"));
			mfi->total = total;
			mfi->flags |= MAIL_FIF_NEED_UPDATE;
		} else 
			d(g_message("-> Total value is the same"));
	} else {
		d(g_message("-> Total value being initialized"));
		mfi->flags |= (MAIL_FIF_TOTAL_VALID | MAIL_FIF_NEED_UPDATE);
		mfi->total = total;
	}

	/* while we're here... */
	if (!(mfi->flags & MAIL_FIF_NAME_VALID)) {
		mfi->name = g_strdup (camel_folder_get_name (CAMEL_FOLDER (object)));
		d(g_message("-> setting name to %s as well", mfi->name));
		mfi->flags |= MAIL_FIF_NAME_VALID;
	}

	if (mfi->flags & MAIL_FIF_NEED_UPDATE) {
		UNLOCK_FOLDERS ();
		d(g_message("-> Queuing change"));
		mail_proxy_event (update_message_counts_main, object, event_data, g_strdup (uri));
	} else {
		UNLOCK_FOLDERS ();
		d(g_message("-> No proxy event needed"));
	}
}

static void
camel_folder_finalized (CamelObject *object, gpointer event_data,
			gpointer user_data)
{
	mail_folder_info *mfi;
	gchar *uri = g_quark_to_string (GPOINTER_TO_UINT (user_data));

	d(g_message("CamelFolder %p finalized, unsetting FOLDER_VALID", object));

	LOCK_FOLDERS ();

	mfi = get_folder_info (uri);

	/* we could check for FOLDER_VALID, but why bother? */
	mfi->flags &= (~MAIL_FIF_FOLDER_VALID);
	mfi->folder = NULL;

	UNLOCK_FOLDERS ();
}

static void
message_list_built (MessageList *ml, gpointer user_data)
{
	mail_folder_info *mfi;
	gchar *uri = g_quark_to_string (GPOINTER_TO_UINT (user_data));

	d(g_message("Message list %p rebuilt, checking hidden", ml));

	LOCK_FOLDERS ();

	mfi = get_folder_info (uri);

	if (!(mfi->flags & MAIL_FIF_FB_VALID)) {
		d(g_message ("-> we seem to have been removed. Returning."));
		UNLOCK_FOLDERS ();
		return;
	}

	if (ml->folder != mfi->folder) {
		g_warning ("folder cache: different folders in cache and messagelist");
		gtk_signal_disconnect_by_data (GTK_OBJECT (ml), user_data);
		UNLOCK_FOLDERS ();
		return;
	}

	MESSAGE_LIST_LOCK (ml, hide_lock);
	if (ml->hidden)
		mfi->hidden = g_hash_table_size (ml->hidden);
	else
		mfi->hidden = 0;
	MESSAGE_LIST_UNLOCK (ml, hide_lock);

	mfi->flags |= MAIL_FIF_HIDDEN_VALID;

	UNLOCK_FOLDERS ();

	maybe_update (mfi);
}

static void
selection_changed (ESelectionModel *esm, gpointer user_data)
{
	mail_folder_info *mfi;
	gchar *uri = g_quark_to_string (GPOINTER_TO_UINT (user_data));
	
	d(g_message ("Selection model %p changed, checking selected", esm));

	LOCK_FOLDERS ();

	mfi = get_folder_info (uri);

	if (!(mfi->flags & MAIL_FIF_FB_VALID)) {
		d(g_message ("-> hm, fb is not valid. We must have been removed."));
		UNLOCK_FOLDERS ();
		return;
	}

	mfi->selected = e_selection_model_selected_count (esm);
	mfi->flags |= MAIL_FIF_SELECTED_VALID;

	UNLOCK_FOLDERS ();

	maybe_update (mfi);
}

static void
folder_browser_destroyed (GtkObject *fb, gpointer user_data)
{
	mail_folder_info *mfi;
	gchar *uri = g_quark_to_string (GPOINTER_TO_UINT (user_data));
	
	d(g_message ("folder browser destroyed. Unsetting FB_VALID."));

	LOCK_FOLDERS ();

	mfi = get_folder_info (uri);

	/* we could check for FB_VALID, but why bother? */
	mfi->flags &= (~MAIL_FIF_FB_VALID);
	mfi->fb = NULL;
	UNLOCK_FOLDERS ();
}

static void
check_for_fb_match (gpointer key, gpointer value, gpointer user_data)
{
	mail_folder_info *mfi = (mail_folder_info *) value;

	d(g_message("-> checking uri \"%s\" if it has active fb", (gchar *) key));
	/* This should only be true for one item, but no real
	 * way to stop the foreach...
	 */
 
	if (mfi->fb == folder_browser) {
		d(g_message("-> -> it does!"));
		maybe_update (mfi);
	}
}

/* get folder info operation */

struct get_mail_info_msg {
	struct _mail_msg msg;

	CamelFolder *folder;
	gchar *uri;
};

static char *
get_mail_info_describe (struct _mail_msg *msg, int complete)
{
	struct get_mail_info_msg *gmim = (struct get_mail_info_msg *) msg;

	return g_strdup_printf ("Examining \'%s\'", camel_folder_get_full_name (gmim->folder));
}

static void
get_mail_info_receive (struct _mail_msg *msg)
{
	struct get_mail_info_msg *gmim = (struct get_mail_info_msg *) msg;
	mail_folder_info *mfi;

	LOCK_FOLDERS ();

	mfi = get_folder_info (gmim->uri);

	if (!(mfi->flags & MAIL_FIF_NAME_VALID)) {
		mfi->name = g_strdup (camel_folder_get_name (gmim->folder));
		mfi->flags |= MAIL_FIF_NAME_VALID;
	}

	mfi->unread = camel_folder_get_unread_message_count (gmim->folder);
	if (mfi->unread != -1)
		mfi->flags |= MAIL_FIF_UNREAD_VALID;

	mfi->total = camel_folder_get_message_count (gmim->folder);
	mfi->flags |= MAIL_FIF_TOTAL_VALID;

	UNLOCK_FOLDERS ();
}

static void
get_mail_info_reply (struct _mail_msg *msg)
{
	struct get_mail_info_msg *gmim = (struct get_mail_info_msg *) msg;
	mail_folder_info *mfi;

	LOCK_FOLDERS ();

	mfi = get_folder_info (gmim->uri);
	maybe_update (mfi);
	mfi->mail_info_id = 0;

	UNLOCK_FOLDERS ();
}

static void
get_mail_info_destroy (struct _mail_msg *msg)
{
	struct get_mail_info_msg *gmim = (struct get_mail_info_msg *) msg;

	camel_object_unref (CAMEL_OBJECT (gmim->folder));
	g_free (gmim->uri);
}

static mail_msg_op_t get_mail_info_op = {
	get_mail_info_describe,
	get_mail_info_receive,
	get_mail_info_reply,
	get_mail_info_destroy
};

static int
get_mail_info (CamelFolder *folder, const char *uri)
{
	struct get_mail_info_msg *gmim;
	int id;

	gmim = mail_msg_new (&get_mail_info_op, NULL, sizeof (*gmim));
	gmim->folder = folder;
	camel_object_ref (CAMEL_OBJECT (folder));
	gmim->uri = g_strdup (uri);

	id = gmim->msg.seq;
	e_thread_put (mail_thread_new, (EMsg *) gmim);
	return id;
}

/* Public functions */

void 
mail_folder_cache_set_update_estorage (const gchar *uri,
				       EvolutionStorage *estorage)
{
	mail_folder_info *mfi;

	g_return_if_fail (uri);

	LOCK_FOLDERS ();

	mfi = get_folder_info (uri);

	if (mfi->update_mode != MAIL_FIUM_UNKNOWN) {
		/* we could check to see that update_mode = ESTORAGE */
		/*g_warning ("folder cache: update mode already set??");*/
		UNLOCK_FOLDERS ();
		return;
	}

	d(g_message("Uri %s updates with EVOLUTION_STORAGE", uri));
	mfi->update_mode = MAIL_FIUM_EVOLUTION_STORAGE;
	mfi->update_info.es = estorage;

	UNLOCK_FOLDERS ();
}

void 
mail_folder_cache_set_update_lstorage (const gchar *uri,
				       GNOME_Evolution_Storage lstorage,
				       const gchar *path)
{
	mail_folder_info *mfi;

	g_return_if_fail (uri);

	LOCK_FOLDERS ();

	mfi = get_folder_info (uri);

	if (mfi->update_mode != MAIL_FIUM_UNKNOWN) {
		/*we could check to see that update_mode = lstorage */
		/*g_warning ("folder cache: update mode already set??");*/
		UNLOCK_FOLDERS ();
		return;
	}

	d(g_message("Uri %s updates with LOCAL_STORAGE", uri));
	/* Note that we don't dup the object or anything. Too lazy. */
	mfi->update_mode = MAIL_FIUM_LOCAL_STORAGE;
	mfi->update_info.ls = lstorage;

	mfi->path = g_strdup (path);
	mfi->flags |= MAIL_FIF_PATH_VALID;

	UNLOCK_FOLDERS ();
}

void
mail_folder_cache_remove_folder (const gchar *uri)
{
	mail_folder_info *mfi;
	GQuark uri_key;

	g_return_if_fail (uri);

	LOCK_FOLDERS ();
	
	mfi = g_hash_table_lookup (folders, uri);
	if (!mfi) {
		UNLOCK_FOLDERS ();
		g_warning ("folder cache: trying to remove uri \"%s\": not found in the cache",
			   uri);
		return;
	}
	g_hash_table_remove (folders, uri);

	d(g_message ("folder cache: removing uri \"%s\".", uri));

	if (mfi->timeout_id)
		g_source_remove (mfi->timeout_id);

	if (mfi->mail_info_id)
		mail_msg_cancel (mfi->mail_info_id);

	uri_key = g_quark_from_string (uri);

	if (mfi->flags & MAIL_FIF_FB_VALID) {
		ESelectionModel *esm;

		esm = e_tree_get_selection_model (mfi->fb->message_list->tree);

		gtk_signal_disconnect_by_data (GTK_OBJECT (mfi->fb), GUINT_TO_POINTER (uri_key));
		gtk_signal_disconnect_by_data (GTK_OBJECT (mfi->fb->message_list), 
					       GUINT_TO_POINTER (uri_key));
		gtk_signal_disconnect_by_data (GTK_OBJECT (esm), GUINT_TO_POINTER (uri_key));
	}

	if (mfi->flags & MAIL_FIF_FOLDER_VALID) {
		camel_object_unhook_event (CAMEL_OBJECT (mfi->folder), "message_changed",
					   update_message_counts, GUINT_TO_POINTER (uri_key));
		camel_object_unhook_event (CAMEL_OBJECT (mfi->folder), "folder_changed",
					   update_message_counts, GUINT_TO_POINTER (uri_key));
		camel_object_unhook_event (CAMEL_OBJECT (mfi->folder), "finalize",
					   camel_folder_finalized, GUINT_TO_POINTER (uri_key));
	}

	UNLOCK_FOLDERS ();

	g_free (mfi->uri);
	g_free (mfi->path);
	g_free (mfi->name);
	g_free (mfi);
}

void
mail_folder_cache_note_folder (const gchar *uri, CamelFolder *folder)
{
	mail_folder_info *mfi;
	GQuark uri_key;

	g_return_if_fail (uri);
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	LOCK_FOLDERS ();

	mfi = get_folder_info (uri);

	if (mfi->flags & MAIL_FIF_FOLDER_VALID) {
		if (mfi->folder != folder)
			g_warning ("folder cache: CamelFolder being changed for %s??? I refuse.", uri);
		UNLOCK_FOLDERS ();
		return;
	}

	d(g_message("Setting uri %s to watch folder %p", uri, folder));

	mfi->flags |= MAIL_FIF_FOLDER_VALID;
	mfi->folder = folder;
	
	if (mfi->mail_info_id == 0)
		mfi->mail_info_id = get_mail_info (folder, uri);
	
	UNLOCK_FOLDERS ();

	uri_key = g_quark_from_string (uri);
	camel_object_hook_event (CAMEL_OBJECT (folder), "message_changed",
				 update_message_counts, GUINT_TO_POINTER (uri_key));
	camel_object_hook_event (CAMEL_OBJECT (folder), "folder_changed",
				 update_message_counts, GUINT_TO_POINTER (uri_key));
	camel_object_hook_event (CAMEL_OBJECT (folder), "finalize",
				 camel_folder_finalized, GUINT_TO_POINTER (uri_key));

}

void 
mail_folder_cache_note_fb (const gchar *uri, FolderBrowser *fb)
{
	mail_folder_info *mfi;
	GQuark uri_key;

	g_return_if_fail (uri);
	g_return_if_fail (IS_FOLDER_BROWSER (fb));

	LOCK_FOLDERS ();

	mfi = get_folder_info (uri);

	if (!(mfi->flags & MAIL_FIF_FOLDER_VALID)) {
		d(g_message("No folder specified so ignoring NOTE_FB at %s", uri));
		UNLOCK_FOLDERS ();
		return;
	}

	d(g_message("Noting folder browser %p for %s", fb, uri));

	mfi->fb = fb;
	mfi->flags |= MAIL_FIF_FB_VALID;

	uri_key = g_quark_from_string (uri);
	gtk_signal_connect (GTK_OBJECT (fb->message_list), "message_list_built",
			    message_list_built, GUINT_TO_POINTER (uri_key));
	gtk_signal_connect (GTK_OBJECT (e_tree_get_selection_model (fb->message_list->tree)),
			    "selection_changed", selection_changed, GUINT_TO_POINTER (uri_key));
	gtk_signal_connect (GTK_OBJECT (fb), "destroy", 
			    folder_browser_destroyed, GUINT_TO_POINTER (uri_key));

	UNLOCK_FOLDERS ();

	d(g_message("-> faking message_list_built"));
	message_list_built (fb->message_list, GUINT_TO_POINTER (uri_key));
}

void 
mail_folder_cache_note_folderinfo (const gchar *uri, CamelFolderInfo *fi)
{
	mail_folder_info *mfi;

	g_return_if_fail (uri);
	g_return_if_fail (fi);

	LOCK_FOLDERS ();

	mfi = get_folder_info (uri);

	d(g_message("Noting folderinfo %p for %s", fi, uri));

	if (fi->unread_message_count != -1) {
		mfi->unread = fi->unread_message_count;
		mfi->flags |= MAIL_FIF_UNREAD_VALID;
	} else {
		mfi->unread = 0;
	}

	if (!(mfi->flags & MAIL_FIF_NAME_VALID)) {
		d(g_message("-> setting name %s", fi->name));
		mfi->name = g_strdup (fi->name);
		mfi->flags |= MAIL_FIF_NAME_VALID;
	}

	UNLOCK_FOLDERS ();

	maybe_update (mfi);
}

void 
mail_folder_cache_note_name (const gchar *uri, const gchar *name)
{
	mail_folder_info *mfi;

	g_return_if_fail (uri);
	g_return_if_fail (name);

	LOCK_FOLDERS ();

	mfi = get_folder_info (uri);

	d(g_message("Noting name %s for %s", name, uri));

	if (mfi->flags & MAIL_FIF_NAME_VALID) {
		/* we could complain.. */
		d(g_message("-> name already set: %s", mfi->name));
		UNLOCK_FOLDERS ();
		return;
	}

	mfi->name = g_strdup (name);
	mfi->flags |= MAIL_FIF_NAME_VALID;

	UNLOCK_FOLDERS ();

	maybe_update (mfi);
}

CamelFolder *
mail_folder_cache_try_folder (const gchar *uri)
{
	mail_folder_info *mfi;
	CamelFolder *ret;

	g_return_val_if_fail (uri, NULL);

	LOCK_FOLDERS ();

	mfi = get_folder_info (uri);

	if (mfi->flags & MAIL_FIF_FOLDER_VALID)
		ret = mfi->folder;
	else
		ret = NULL;

	UNLOCK_FOLDERS ();

	return ret;
}

gchar *
mail_folder_cache_try_name (const gchar *uri)
{
	mail_folder_info *mfi;
	gchar *ret;

	g_return_val_if_fail (uri, NULL);

	LOCK_FOLDERS ();

	mfi = get_folder_info (uri);

	if (mfi->flags & MAIL_FIF_NAME_VALID)
		ret = g_strdup (mfi->name);
	else
		ret = NULL;

	UNLOCK_FOLDERS ();

	return ret;
}

void
mail_folder_cache_set_shell_view (GNOME_Evolution_ShellView sv)
{
	CORBA_Environment ev;

	g_return_if_fail (sv != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);

	if (shell_view != CORBA_OBJECT_NIL)
		CORBA_Object_release (shell_view, &ev);

	if (BONOBO_EX (&ev))
		g_warning ("Exception in releasing old shell view: %s",
			   bonobo_exception_get_text (&ev));
	CORBA_exception_free (&ev);

	shell_view = CORBA_Object_duplicate (sv, &ev);
	if (BONOBO_EX (&ev))
		g_warning ("Exception in duping new shell view: %s",
			   bonobo_exception_get_text (&ev));
	CORBA_exception_free (&ev);
}
		
void
mail_folder_cache_set_folder_browser (FolderBrowser *fb)
{
	d(g_message("Setting new folder browser: %p", fb));

	if (folder_browser != NULL) {
		d(g_message("Unreffing old folder browser %p", folder_browser));
		gtk_object_unref (GTK_OBJECT (folder_browser));
	}

	folder_browser = fb;

	if (fb) {
		d(g_message("Reffing new browser %p", fb));
		gtk_object_ref (GTK_OBJECT (fb));

		LOCK_FOLDERS ();
		d(g_message("Checking folders for this fb"));
		g_hash_table_foreach (folders, check_for_fb_match, fb);
		UNLOCK_FOLDERS ();
	} else if (shell_view != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		GNOME_Evolution_ShellView_setFolderBarLabel (shell_view,
							     "",
							     &ev);
		if (BONOBO_EX (&ev))
			g_warning ("Exception in folder bar label clear: %s",
				   bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
	}
}
		
#if d(!)0
#include <stdio.h>

static void
print_item (gpointer key, gpointer value, gpointer user_data)
{
	gchar *uri = key;
	mail_folder_info *mfi = value;

	printf ("* %s\n", uri);

	if (mfi->flags & MAIL_FIF_PATH_VALID)
		printf ("         Path: %s\n", mfi->path);
	if (mfi->flags & MAIL_FIF_NAME_VALID)
		printf ("         Name: %s\n", mfi->name);
	if (mfi->flags & MAIL_FIF_FOLDER_VALID)
		printf ("       Folder: %p\n", mfi->folder);
	if (mfi->flags & MAIL_FIF_UNREAD_VALID)
		printf ("       Unread: %d\n", mfi->unread);
	if (mfi->flags & MAIL_FIF_HIDDEN_VALID)
		printf ("       Hidden: %d\n", mfi->hidden);
	if (mfi->flags & MAIL_FIF_TOTAL_VALID)
		printf ("        Total: %d\n", mfi->total);
	if (mfi->flags & MAIL_FIF_NEED_UPDATE)
		printf ("       Needs an update\n");
	switch (mfi->update_mode) {
	case MAIL_FIUM_UNKNOWN:
		printf ("       Update mode: UNKNOWN\n");
		break;
	case MAIL_FIUM_EVOLUTION_STORAGE:
		printf ("       Update mode: Evolution\n");
		break;
	case MAIL_FIUM_LOCAL_STORAGE:
		printf ("       Update mode: Local\n");
		break;
/*
	case MAIL_FIUM_SHELL_VIEW:
		printf ("       Update mode: Shell View\n");
		break;
*/
	}

	printf ("\n");
}
		
void mail_folder_cache_dump_cache (void);

void
mail_folder_cache_dump_cache (void)
{
	printf ("********** MAIL FOLDER CACHE DUMP ************\n\n");
	LOCK_FOLDERS ();
	g_hash_table_foreach (folders, print_item, NULL);
	UNLOCK_FOLDERS ();
	printf ("********** END OF CACHE DUMP. ****************\n");
}

#endif
