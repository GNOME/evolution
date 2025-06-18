/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

#include "shell/e-shell-view.h"

#include "e-mail-templates-store.h"

/* where on a GMenu the index of the action data is stored */
#define TEMPLATES_STORE_ACTIONS_INDEX_KEY "templates-store-actions-index-key"

struct _EMailTemplatesStorePrivate {
	GWeakRef *account_store_weakref; /* EMailAccountStore * */

	gulong service_enabled_handler_id;
	gulong service_disabled_handler_id;
	gulong service_removed_handler_id;
	gulong source_changed_handler_id;

	GMutex busy_lock;
	GCancellable *cancellable;
	GSList *stores; /* TmplStoreData *, sorted by account_store options; those with set templates dir */
	guint menu_refresh_idle_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailTemplatesStore, e_mail_templates_store, G_TYPE_OBJECT);

enum {
	PROP_0,
	PROP_ACCOUNT_STORE
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
templates_store_lock (EMailTemplatesStore *templates_store)
{
	g_return_if_fail (E_IS_MAIL_TEMPLATES_STORE (templates_store));

	g_mutex_lock (&templates_store->priv->busy_lock);
}

static void
templates_store_unlock (EMailTemplatesStore *templates_store)
{
	g_return_if_fail (E_IS_MAIL_TEMPLATES_STORE (templates_store));

	g_mutex_unlock (&templates_store->priv->busy_lock);
}

static void
templates_store_emit_changed (EMailTemplatesStore *templates_store)
{
	g_return_if_fail (E_IS_MAIL_TEMPLATES_STORE (templates_store));

	g_signal_emit (templates_store, signals[CHANGED], 0, NULL);
}

static void
tmpl_folder_data_folder_changed_cb (CamelFolder *folder,
				    CamelFolderChangeInfo *change_info,
				    gpointer user_data);

static void
tmpl_store_data_folder_created_cb (CamelStore *store,
				   CamelFolderInfo *folder_info,
				   gpointer user_data);
static void
tmpl_store_data_folder_deleted_cb (CamelStore *store,
				   CamelFolderInfo *folder_info,
				   gpointer user_data);
static void
tmpl_store_data_folder_renamed_cb (CamelStore *store,
				   const gchar *old_name,
				   CamelFolderInfo *folder_info,
				   gpointer user_data);

static void
tmpl_store_data_notify_display_name_cb (CamelService *service,
					GParamSpec *param,
					gpointer user_data);

typedef struct _TmplMessageData {
	const gchar *subject; /* Allocated by camel-pstring */
	const gchar *uid; /* Allocated by camel-pstring */
} TmplMessageData;

static const gchar *
tmpl_sanitized_subject (const gchar *subject)
{
	if (!subject || !*subject)
		subject = _("No Title");

	return subject;
}

static TmplMessageData *
tmpl_message_data_new (CamelMessageInfo *info)
{
	TmplMessageData *tmd;

	g_return_val_if_fail (info != NULL, NULL);

	tmd = g_new0 (TmplMessageData, 1);
	tmd->subject = camel_pstring_strdup (tmpl_sanitized_subject (camel_message_info_get_subject (info)));
	tmd->uid = camel_pstring_strdup (camel_message_info_get_uid (info));

	return tmd;
}

static void
tmpl_message_data_free (gpointer ptr)
{
	TmplMessageData *tmd = ptr;

	if (tmd) {
		camel_pstring_free (tmd->subject);
		camel_pstring_free (tmd->uid);
		g_free (tmd);
	}
}

static void
tmpl_message_data_change_subject (TmplMessageData *tmd,
				  const gchar *subject)
{
	g_return_if_fail (tmd != NULL);

	if (subject != tmd->subject) {
		camel_pstring_free (tmd->subject);
		tmd->subject = camel_pstring_strdup (tmpl_sanitized_subject (subject));
	}
}

static gint
tmpl_message_data_compare (gconstpointer ptr1,
			   gconstpointer ptr2)
{
	const TmplMessageData *tmd1 = ptr1, *tmd2 = ptr2;

	if (!tmd1 || !tmd2) {
		if (tmd1 == tmd2)
			return 0;
		if (tmd1)
			return -1;
		return 1;
	}

	return g_utf8_collate (tmd1->subject ? tmd1->subject : "", tmd2->subject ? tmd2->subject : "");
}

typedef struct _TmplFolderData {
	volatile gint ref_count;
	GWeakRef *templates_store_weakref; /* EMailTemplatesStore * */
	CamelFolder *folder;
	gulong changed_handler_id;

	GMutex busy_lock;
	/* This might look inefficient, but the rebuild of the menu is called
	   much more often then the remove of a message from the folder, thus
	   it's cheaper to traverse one by one here, then re-sort the content
	   every time the menu is being rebuild. */
	GSList *messages; /* TmplMessageData *, ordered by data->subject */
} TmplFolderData;

static TmplFolderData *
tmpl_folder_data_new (EMailTemplatesStore *templates_store,
		      CamelFolder *folder)
{
	TmplFolderData *tfd;

	g_return_val_if_fail (E_IS_MAIL_TEMPLATES_STORE (templates_store), NULL);
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	tfd = g_new0 (TmplFolderData, 1);
	tfd->ref_count = 1;
	tfd->templates_store_weakref = e_weak_ref_new (templates_store);
	tfd->folder = g_object_ref (folder);
	tfd->changed_handler_id = g_signal_connect (folder, "changed",
		G_CALLBACK (tmpl_folder_data_folder_changed_cb), tfd);
	g_mutex_init (&tfd->busy_lock);
	tfd->messages = NULL;

	return tfd;
}

static TmplFolderData *
tmpl_folder_data_ref (TmplFolderData *tfd)
{
	g_return_val_if_fail (tfd != NULL, NULL);

	g_atomic_int_inc (&tfd->ref_count);

	return tfd;
}

static void
tmpl_folder_data_unref (gpointer ptr)
{
	TmplFolderData *tfd = ptr;

	if (tfd) {
		if (!g_atomic_int_dec_and_test (&tfd->ref_count))
			return;

		if (tfd->folder && tfd->changed_handler_id) {
			g_signal_handler_disconnect (tfd->folder, tfd->changed_handler_id);
			tfd->changed_handler_id = 0;
		}

		g_clear_pointer (&tfd->templates_store_weakref, e_weak_ref_free);
		g_clear_object (&tfd->folder);

		g_mutex_clear (&tfd->busy_lock);
		g_slist_free_full (tfd->messages, tmpl_message_data_free);
		tfd->messages = NULL;

		g_free (tfd);
	}
}

static void
tmpl_folder_data_lock (TmplFolderData *tfd)
{
	g_return_if_fail (tfd != NULL);

	g_mutex_lock (&tfd->busy_lock);
}

static void
tmpl_folder_data_unlock (TmplFolderData *tfd)
{
	g_return_if_fail (tfd != NULL);

	g_mutex_unlock (&tfd->busy_lock);
}

static void
tmpl_folder_data_add_message (TmplFolderData *tfd,
			      CamelMessageInfo *info)
{
	TmplMessageData *tmd;

	g_return_if_fail (tfd != NULL);
	g_return_if_fail (info != NULL);

	tmd = tmpl_message_data_new (info);
	g_return_if_fail (tmd != NULL);

	/* The caller is responsible to call tmpl_folder_data_sort() */
	tfd->messages = g_slist_prepend (tfd->messages, tmd);
}

static TmplMessageData *
tmpl_folder_data_find_message (TmplFolderData *tfd,
			       const gchar *uid)
{
	GSList *link;

	g_return_val_if_fail (tfd != NULL, NULL);
	g_return_val_if_fail (uid != NULL, NULL);

	for (link = tfd->messages; link; link = g_slist_next (link)) {
		TmplMessageData *tmd = link->data;

		if (!tmd)
			continue;

		if (uid == tmd->uid || g_strcmp0 (uid, tmd->uid) == 0)
			return tmd;
	}

	return NULL;
}

static gboolean
tmpl_folder_data_remove_message (TmplFolderData *tfd,
				 const gchar *uid)
{
	TmplMessageData *tmd;

	g_return_val_if_fail (tfd != NULL, FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);

	tmd = tmpl_folder_data_find_message (tfd, uid);
	if (tmd) {
		tfd->messages = g_slist_remove (tfd->messages, tmd);
		tmpl_message_data_free (tmd);

		return TRUE;
	}

	return FALSE;
}

static gboolean
tmpl_folder_data_change_message (TmplFolderData *tfd,
				 CamelMessageInfo *info)
{
	TmplMessageData *tmd;
	const gchar *subject;
	gboolean changed = FALSE;

	g_return_val_if_fail (tfd != NULL, FALSE);
	g_return_val_if_fail (info != NULL, FALSE);

	tmd = tmpl_folder_data_find_message (tfd, camel_message_info_get_uid (info));
	if (!tmd) {
		if (!(camel_message_info_get_flags (info) & (CAMEL_MESSAGE_JUNK | CAMEL_MESSAGE_DELETED))) {
			tmpl_folder_data_add_message (tfd, info);
			return TRUE;
		}

		return FALSE;
	}

	if ((camel_message_info_get_flags (info) & (CAMEL_MESSAGE_JUNK | CAMEL_MESSAGE_DELETED)) != 0) {
		return tmpl_folder_data_remove_message (tfd, camel_message_info_get_uid (info));
	}

	subject = tmpl_sanitized_subject (camel_message_info_get_subject (info));

	if (g_strcmp0 (subject, tmd->subject) != 0) {
		tmpl_message_data_change_subject (tmd, subject);
		/* The caller is responsible to call tmpl_folder_data_sort() */
		changed = TRUE;
	}

	return changed;
}

static void
tmpl_folder_data_sort (TmplFolderData *tfd)
{
	g_return_if_fail (tfd != NULL);

	tfd->messages = g_slist_sort (tfd->messages, tmpl_message_data_compare);
}

static gint
tmpl_folder_data_compare (gconstpointer ptr1,
			  gconstpointer ptr2)
{
	const TmplFolderData *tfd1 = ptr1, *tfd2 = ptr2;
	const gchar *display_name1, *display_name2;

	if (!tfd1 || !tfd2) {
		if (tfd1 == tfd2)
			return 0;

		return tfd1 ? -1 : 1;
	}

	display_name1 = camel_folder_get_display_name (tfd1->folder);
	display_name2 = camel_folder_get_display_name (tfd2->folder);

	return g_utf8_collate (display_name1 ? display_name1 : "", display_name2 ? display_name2 : "");
}

static void
tmpl_folder_data_update_done_cb (GObject *source,
				 GAsyncResult *result,
				 gpointer user_data)
{
	TmplFolderData *tfd = user_data;
	GError *local_error = NULL;

	g_return_if_fail (tfd != NULL);
	g_return_if_fail (g_task_is_valid (result, source));

	if (g_task_propagate_boolean (G_TASK (result), &local_error)) {
		/* Content changed, rebuild menu when needed */
		EMailTemplatesStore *templates_store;

		templates_store = g_weak_ref_get (tfd->templates_store_weakref);
		if (templates_store) {
			templates_store_emit_changed (templates_store);
			g_object_unref (templates_store);
		}
	} else if (local_error) {
		g_debug ("%s: Failed with error: %s", G_STRFUNC, local_error->message);
	}

	g_clear_error (&local_error);
}

typedef struct _TfdUpdateData {
	TmplFolderData *tfd;
	GPtrArray *added_uids;
	GPtrArray *changed_uids;
} TfdUpdateData;

static void
tfd_update_data_free (gpointer ptr)
{
	TfdUpdateData *tud = ptr;

	if (tud) {
		tmpl_folder_data_unref (tud->tfd);
		g_ptr_array_free (tud->added_uids, TRUE);
		g_ptr_array_free (tud->changed_uids, TRUE);
		g_free (tud);
	}
}

static gboolean
tmpl_folder_data_update_sync (TmplFolderData *tfd,
			      const GPtrArray *added_uids,
			      const GPtrArray *changed_uids,
			      GCancellable *cancellable)
{
	GPtrArray *all_uids = NULL;
	CamelMessageInfo *info;
	guint ii;
	gboolean changed = FALSE;

	g_return_val_if_fail (tfd != NULL, FALSE);
	g_return_val_if_fail (CAMEL_IS_FOLDER (tfd->folder), FALSE);

	if (!added_uids || !changed_uids || added_uids->len + changed_uids->len > 10)
		camel_folder_summary_prepare_fetch_all (camel_folder_get_folder_summary (tfd->folder), NULL);

	if (!added_uids && !changed_uids) {
		all_uids = camel_folder_summary_dup_uids (camel_folder_get_folder_summary (tfd->folder));
		added_uids = all_uids;
	}

	tmpl_folder_data_lock (tfd);

	for (ii = 0; added_uids && ii < added_uids->len; ii++) {
		const gchar *uid = added_uids->pdata[ii];

		info = camel_folder_summary_get (camel_folder_get_folder_summary (tfd->folder), uid);
		if (info) {
			if (!(camel_message_info_get_flags (info) & (CAMEL_MESSAGE_JUNK | CAMEL_MESSAGE_DELETED))) {
				/* Sometimes the 'add' notification can come after the 'change',
				   thus use the change_message() which covers both cases. */
				changed = tmpl_folder_data_change_message (tfd, info) || changed;
			} else {
				changed = tmpl_folder_data_remove_message (tfd, camel_message_info_get_uid (info)) || changed;
			}

			g_clear_object (&info);
		}
	}

	for (ii = 0; changed_uids && ii < changed_uids->len; ii++) {
		const gchar *uid = changed_uids->pdata[ii];

		info = camel_folder_summary_get (camel_folder_get_folder_summary (tfd->folder), uid);
		if (info) {
			changed = tmpl_folder_data_change_message (tfd, info) || changed;
			g_clear_object (&info);
		}
	}

	if (changed)
		tmpl_folder_data_sort (tfd);

	if (all_uids)
		g_ptr_array_unref (all_uids);

	tmpl_folder_data_unlock (tfd);

	return changed;
}

static void
tmpl_folder_data_update_thread (GTask *task,
				gpointer source_object,
				gpointer task_data,
				GCancellable *cancellable)
{
	TfdUpdateData *tud = task_data;
	gboolean changed;

	g_return_if_fail (tud != NULL);
	g_return_if_fail (tud->tfd != NULL);
	g_return_if_fail (tud->added_uids != NULL);
	g_return_if_fail (tud->changed_uids != NULL);

	changed = tmpl_folder_data_update_sync (tud->tfd, tud->added_uids, tud->changed_uids, cancellable);

	g_task_return_boolean (task, changed);
}

static void
tmpl_folder_data_schedule_update (TmplFolderData *tfd,
				  CamelFolderChangeInfo *change_info)
{
	EMailTemplatesStore *templates_store;
	TfdUpdateData *tud;
	GTask *task;
	guint ii;

	g_return_if_fail (tfd != NULL);

	templates_store = g_weak_ref_get (tfd->templates_store_weakref);
	if (!templates_store)
		return;

	tud = g_new0 (TfdUpdateData, 1);
	tud->tfd = tmpl_folder_data_ref (tfd);
	tud->added_uids = g_ptr_array_new_full (
		change_info->uid_added ? change_info->uid_added->len : 0,
		(GDestroyNotify) camel_pstring_free);
	tud->changed_uids = g_ptr_array_new_full (
		(change_info->uid_changed ? change_info->uid_changed->len : 0),
		(GDestroyNotify) camel_pstring_free);

	for (ii = 0; change_info->uid_added && ii < change_info->uid_added->len; ii++) {
		const gchar *uid = change_info->uid_added->pdata[ii];

		if (uid && *uid)
			g_ptr_array_add (tud->added_uids, (gpointer) camel_pstring_strdup (uid));
	}

	for (ii = 0; change_info->uid_changed && ii < change_info->uid_changed->len; ii++) {
		const gchar *uid = change_info->uid_changed->pdata[ii];

		if (uid && *uid)
			g_ptr_array_add (tud->changed_uids, (gpointer) camel_pstring_strdup (uid));
	}

	task = g_task_new (NULL, templates_store->priv->cancellable, tmpl_folder_data_update_done_cb, tfd);
	g_task_set_task_data (task, tud, tfd_update_data_free);
	g_task_run_in_thread (task, tmpl_folder_data_update_thread);
	g_object_unref (task);

	g_object_unref (templates_store);
}

static void
tmpl_folder_data_folder_changed_cb (CamelFolder *folder,
				    CamelFolderChangeInfo *change_info,
				    gpointer user_data)
{
	TmplFolderData *tfd = user_data;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (change_info != NULL);
	g_return_if_fail (tfd != NULL);

	tmpl_folder_data_ref (tfd);

	if ((change_info->uid_added && change_info->uid_added->len) ||
	    (change_info->uid_changed && change_info->uid_changed->len)) {
		tmpl_folder_data_schedule_update (tfd, change_info);
	} else if (change_info->uid_removed && change_info->uid_removed->len) {
		EMailTemplatesStore *templates_store;

		templates_store = g_weak_ref_get (tfd->templates_store_weakref);
		if (templates_store) {
			guint ii;

			tmpl_folder_data_lock (tfd);

			for (ii = 0; ii < change_info->uid_removed->len; ii++) {
				const gchar *uid = change_info->uid_removed->pdata[ii];

				if (uid && *uid)
					tmpl_folder_data_remove_message (tfd, uid);
			}

			tmpl_folder_data_unlock (tfd);

			templates_store_emit_changed (templates_store);

			g_object_unref (templates_store);
		}
	}

	tmpl_folder_data_unref (tfd);
}

static gboolean
tmpl_store_data_traverse_to_free_cb (GNode *node,
				     gpointer user_data)
{
	if (node && node->data) {
		tmpl_folder_data_unref (node->data);
		node->data = NULL;
	}

	return FALSE;
}

typedef struct _TmplStoreData {
	volatile gint ref_count;
	GWeakRef *templates_store_weakref; /* EMailTemplatesStore * */
	GWeakRef *store_weakref; /* CamelStore * */

	gulong folder_created_handler_id;
	gulong folder_deleted_handler_id;
	gulong folder_renamed_handler_id;
	gulong notify_display_name_id;

	GMutex busy_lock;
	gchar *root_folder_path;
	gchar *templates_folder_uri;
	gchar *identity_source_uid;
	GNode *folders; /* data is TmplFolderData * */
} TmplStoreData;

static void
tmpl_store_data_set_root_folder_path (TmplStoreData *tsd,
				      const gchar *root_folder_path);

static TmplStoreData *
tmpl_store_data_new (EMailTemplatesStore *templates_store,
		     CamelStore *store,
		     const gchar *root_folder_path,
		     const gchar *templates_folder_uri,
		     const gchar *identity_source_uid)
{
	TmplStoreData *tsd;

	g_return_val_if_fail (E_IS_MAIL_TEMPLATES_STORE (templates_store), NULL);
	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);
	g_return_val_if_fail (root_folder_path && *root_folder_path, NULL);
	g_return_val_if_fail (templates_folder_uri && *templates_folder_uri, NULL);

	tsd = g_new0 (TmplStoreData, 1);
	tsd->ref_count = 1;
	tsd->templates_store_weakref = e_weak_ref_new (templates_store);
	tsd->store_weakref = e_weak_ref_new (store);
	g_mutex_init (&tsd->busy_lock);
	tsd->root_folder_path = NULL;
	tsd->templates_folder_uri = g_strdup (templates_folder_uri);
	tsd->identity_source_uid = g_strdup (identity_source_uid);
	tsd->folders = g_node_new (NULL);

	if (CAMEL_IS_SUBSCRIBABLE (store)) {
		tsd->folder_created_handler_id = g_signal_connect (store, "folder-subscribed",
			G_CALLBACK (tmpl_store_data_folder_created_cb), tsd);
		tsd->folder_deleted_handler_id = g_signal_connect (store, "folder-unsubscribed",
			G_CALLBACK (tmpl_store_data_folder_deleted_cb), tsd);
	} else {
		tsd->folder_created_handler_id = g_signal_connect (store, "folder-created",
			G_CALLBACK (tmpl_store_data_folder_created_cb), tsd);
		tsd->folder_deleted_handler_id = g_signal_connect (store, "folder-deleted",
			G_CALLBACK (tmpl_store_data_folder_deleted_cb), tsd);
	}

	tsd->folder_renamed_handler_id = g_signal_connect (store, "folder-renamed",
		G_CALLBACK (tmpl_store_data_folder_renamed_cb), tsd);

	tsd->notify_display_name_id = e_signal_connect_notify (store, "notify::display-name",
		G_CALLBACK (tmpl_store_data_notify_display_name_cb), tsd);

	tmpl_store_data_set_root_folder_path (tsd, root_folder_path);

	return tsd;
}

static TmplStoreData *
tmpl_store_data_ref (TmplStoreData *tsd)
{
	g_return_val_if_fail (tsd != NULL, NULL);

	g_atomic_int_inc (&tsd->ref_count);

	return tsd;
}

static void
tmpl_store_data_unref (gpointer ptr)
{
	TmplStoreData *tsd = ptr;

	if (tsd) {
		if (!g_atomic_int_dec_and_test (&tsd->ref_count))
			return;

		g_clear_pointer (&tsd->templates_store_weakref, e_weak_ref_free);

		if (tsd->store_weakref) {
			CamelStore *store;

			store = g_weak_ref_get (tsd->store_weakref);
			if (store) {
				if (tsd->folder_created_handler_id) {
					g_signal_handler_disconnect (store, tsd->folder_created_handler_id);
					tsd->folder_created_handler_id = 0;
				}

				if (tsd->folder_deleted_handler_id) {
					g_signal_handler_disconnect (store, tsd->folder_deleted_handler_id);
					tsd->folder_deleted_handler_id = 0;
				}

				if (tsd->folder_renamed_handler_id) {
					g_signal_handler_disconnect (store, tsd->folder_renamed_handler_id);
					tsd->folder_renamed_handler_id = 0;
				}

				e_signal_disconnect_notify_handler (store, &tsd->notify_display_name_id);

				g_clear_object (&store);
			}

			e_weak_ref_free (tsd->store_weakref);
			tsd->store_weakref = NULL;
		}

		g_mutex_clear (&tsd->busy_lock);

		g_free (tsd->root_folder_path);
		tsd->root_folder_path = NULL;

		g_free (tsd->templates_folder_uri);
		tsd->templates_folder_uri = NULL;

		g_free (tsd->identity_source_uid);
		tsd->identity_source_uid = NULL;

		if (tsd->folders) {
			g_node_traverse (tsd->folders, G_IN_ORDER, G_TRAVERSE_ALL, -1, tmpl_store_data_traverse_to_free_cb, NULL);
			g_node_destroy (tsd->folders);
			tsd->folders = NULL;
		}

		g_free (tsd);
	}
}

static void
tmpl_store_data_lock (TmplStoreData *tsd)
{
	g_return_if_fail (tsd != NULL);

	g_mutex_lock (&tsd->busy_lock);
}

static void
tmpl_store_data_unlock (TmplStoreData *tsd)
{
	g_return_if_fail (tsd != NULL);

	g_mutex_unlock (&tsd->busy_lock);
}

static gint
tmpl_store_data_compare (gconstpointer ptr1,
			 gconstpointer ptr2,
			 gpointer user_data)
{
	const TmplStoreData *tsd1 = ptr1, *tsd2 = ptr2;
	EMailAccountStore *account_store = user_data;
	CamelService *service1, *service2;
	gint res;

	service1 = tsd1 ? g_weak_ref_get (tsd1->store_weakref) : NULL;
	service2 = tsd2 ? g_weak_ref_get (tsd2->store_weakref) : NULL;

	if (account_store && service1 && service2)
		res = e_mail_account_store_compare_services (account_store, service1, service2);
	else
		res = g_utf8_collate (service1 ? camel_service_get_display_name (service1) : "",
				      service2 ? camel_service_get_display_name (service2) : "");

	g_clear_object (&service1);
	g_clear_object (&service2);

	return res;
}

static void
tmpl_store_data_set_root_folder_path (TmplStoreData *tsd,
				      const gchar *root_folder_path)
{
	g_return_if_fail (tsd != NULL);
	g_return_if_fail (root_folder_path && *root_folder_path);

	tmpl_store_data_lock (tsd);

	if (g_strcmp0 (tsd->root_folder_path, root_folder_path) != 0) {
		guint len;

		g_free (tsd->root_folder_path);
		tsd->root_folder_path = g_strdup (root_folder_path);

		len = strlen (tsd->root_folder_path);
		if (tsd->root_folder_path[len - 1] == '/')
			tsd->root_folder_path[len - 1] = '\0';
	}

	tmpl_store_data_unlock (tsd);
}

static GNode *
tmpl_store_data_find_parent_node_locked (TmplStoreData *tsd,
					 const gchar *full_name,
					 gboolean for_insert)
{
	GNode *from_node, *node, *parent;

	g_return_val_if_fail (tsd != NULL, NULL);
	g_return_val_if_fail (full_name != NULL, NULL);

	parent = tsd->folders;
	from_node = tsd->folders;
	while (from_node) {
		node = g_node_first_child (from_node);
		from_node = NULL;

		while (node) {
			TmplFolderData *tfd = node->data;

			if (tfd && tfd->folder &&
			    g_str_has_prefix (full_name, camel_folder_get_full_name (tfd->folder)) &&
			    g_strcmp0 (full_name, camel_folder_get_full_name (tfd->folder)) != 0) {
				parent = node;
				from_node = node;
				break;
			}

			node = g_node_next_sibling (node);
		}
	}

	if (for_insert && parent) {
		if (parent->data) {
			TmplFolderData *tfd = parent->data;

			if (g_strcmp0 (full_name, camel_folder_get_full_name (tfd->folder)) == 0) {
				/* The folder is already in the list of folders */
				parent = NULL;
			}
		}

		for (node = parent ? parent->children : NULL; node; node = node->next) {
			TmplFolderData *tfd = node->data;

			if (!tfd)
				continue;

			if (g_strcmp0 (full_name, camel_folder_get_full_name (tfd->folder)) == 0) {
				/* The folder is already in the list of folders */
				parent = NULL;
				break;
			}
		}
	}

	return parent;
}

static GNode *
tmpl_store_data_find_node_locked (TmplStoreData *tsd,
				  const gchar *full_name)
{
	GNode *node, *parent;

	g_return_val_if_fail (tsd != NULL, NULL);
	g_return_val_if_fail (full_name != NULL, NULL);

	parent = tmpl_store_data_find_parent_node_locked (tsd, full_name, FALSE);
	if (!parent)
		return NULL;

	for (node = g_node_first_child (parent); node; node = g_node_next_sibling (node)) {
		TmplFolderData *tfd = node->data;

		if (!tfd)
			continue;

		if (tfd->folder && g_strcmp0 (full_name, camel_folder_get_full_name (tfd->folder)) == 0) {
			return node;
		}
	}

	return NULL;
}

static GNode *
tmpl_store_data_find_node_with_folder_locked (TmplStoreData *tsd,
					      CamelFolder *folder)
{
	GNode *node;

	g_return_val_if_fail (tsd != NULL, NULL);
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	node = tsd->folders;
	while (node) {
		TmplFolderData *tfd = node->data;
		GNode *next;

		if (tfd && tfd->folder == folder) {
			return node;
		}

		/* Traverse the tree */
		next = node->children;
		if (!next)
			next = node->next;
		if (!next) {
			next = node->parent;
			while (next) {
				GNode *sibl = next->next;

				if (sibl) {
					next = sibl;
					break;
				} else {
					next = next->parent;
				}
			}
		}

		node = next;
	}

	return NULL;
}

static void
tmpl_store_data_update_done_cb (GObject *source,
				GAsyncResult *result,
				gpointer user_data)
{
	TmplStoreData *tsd = user_data;
	GError *local_error = NULL;

	g_return_if_fail (tsd != NULL);
	g_return_if_fail (g_task_is_valid (result, source));

	if (g_task_propagate_boolean (G_TASK (result), &local_error)) {
		/* Content changed, rebuild menu when needed */
		EMailTemplatesStore *templates_store;

		templates_store = g_weak_ref_get (tsd->templates_store_weakref);
		if (templates_store) {
			templates_store_emit_changed (templates_store);
			g_object_unref (templates_store);
		}
	} else if (local_error) {
		g_debug ("%s: Failed with error: %s", G_STRFUNC, local_error->message);
	}

	g_clear_error (&local_error);
}

static void
tmpl_store_data_initial_setup_thread (GTask *task,
				      gpointer source_object,
				      gpointer task_data,
				      GCancellable *cancellable)
{
	EMailTemplatesStore *templates_store;
	TmplStoreData *tsd = task_data;
	CamelStore *store;
	gboolean changed = FALSE;

	g_return_if_fail (tsd != NULL);

	templates_store = g_weak_ref_get (tsd->templates_store_weakref);
	store = g_weak_ref_get (tsd->store_weakref);
	if (store && templates_store) {
		CamelFolderInfo *folder_info = NULL, *fi;
		gchar *root_folder_path;
		GError *local_error = NULL;

		tmpl_store_data_lock (tsd);
		root_folder_path = g_strdup (tsd->root_folder_path);
		tmpl_store_data_unlock (tsd);

		if (root_folder_path) {
			folder_info = camel_store_get_folder_info_sync (
				store, root_folder_path,
				CAMEL_STORE_FOLDER_INFO_RECURSIVE |
				CAMEL_STORE_FOLDER_INFO_SUBSCRIBED |
				CAMEL_STORE_FOLDER_INFO_FAST, cancellable, &local_error);

			if (local_error) {
				g_debug ("%s: Failed to get folder info for '%s : %s': %s", G_STRFUNC,
					camel_service_get_display_name (CAMEL_SERVICE (store)), root_folder_path, local_error->message);
			}

			g_clear_error (&local_error);
		}

		fi = folder_info;
		while (fi && !g_cancellable_is_cancelled (cancellable)) {
			CamelFolderInfo *next;
			CamelFolder *folder;

			folder = camel_store_get_folder_sync (store, fi->full_name, 0, cancellable, &local_error);
			if (folder) {
				GNode *parent;

				tmpl_store_data_lock (tsd);

				parent = tmpl_store_data_find_parent_node_locked (tsd, fi->full_name, TRUE);
				if (parent) {
					TmplFolderData *tfd;

					tfd = tmpl_folder_data_new (templates_store, folder);
					if (tfd) {
						changed = tmpl_folder_data_update_sync (tfd, NULL, NULL, cancellable) || changed;

						g_node_append_data (parent, tfd);
					}
				}

				tmpl_store_data_unlock (tsd);
			}

			if (local_error)
				g_debug ("%s: Failed to get folder '%s': %s", G_STRFUNC, fi->full_name, local_error->message);

			g_clear_object (&folder);
			g_clear_error (&local_error);

			/* Traverse the tree of folders */
			next = fi->child;
			if (!next)
				next = fi->next;
			if (!next) {
				next = fi->parent;
				while (next) {
					CamelFolderInfo *sibl = next->next;

					if (sibl) {
						next = sibl;
						break;
					} else {
						next = next->parent;
					}
				}
			}

			fi = next;
		}

		camel_folder_info_free (folder_info);
		g_free (root_folder_path);
	}

	g_clear_object (&templates_store);
	g_clear_object (&store);

	g_task_return_boolean (task, changed);
}

static void
tmpl_store_data_schedule_initial_setup (TmplStoreData *tsd)
{
	EMailTemplatesStore *templates_store;
	GTask *task;

	g_return_if_fail (tsd != NULL);

	templates_store = g_weak_ref_get (tsd->templates_store_weakref);
	if (!templates_store)
		return;

	tmpl_store_data_ref (tsd);

	task = g_task_new (NULL, templates_store->priv->cancellable, tmpl_store_data_update_done_cb, tsd);
	g_task_set_task_data (task, tsd, tmpl_store_data_unref);
	g_task_run_in_thread (task, tmpl_store_data_initial_setup_thread);
	g_object_unref (task);

	g_object_unref (templates_store);
}

typedef struct _TsdFolderData {
	TmplStoreData *tsd;
	gchar *fullname;
	gchar *old_fullname; /* If set, then it's "rename", otherwise it's "create" */
} TsdFolderData;

static void
tsd_folder_data_free (gpointer ptr)
{
	TsdFolderData *fd = ptr;

	if (fd) {
		tmpl_store_data_unref (fd->tsd);
		g_free (fd->fullname);
		g_free (fd->old_fullname);
		g_free (fd);
	}
}

static void
tmpl_store_data_folder_thread (GTask *task,
			       gpointer source_object,
			       gpointer task_data,
			       GCancellable *cancellable)
{
	EMailTemplatesStore *templates_store;
	TsdFolderData *fd = task_data;
	CamelStore *store;
	gboolean changed = FALSE;

	g_return_if_fail (fd != NULL);
	g_return_if_fail (fd->tsd != NULL);
	g_return_if_fail (fd->fullname != NULL);

	templates_store = g_weak_ref_get (fd->tsd->templates_store_weakref);
	store = g_weak_ref_get (fd->tsd->store_weakref);
	if (store && templates_store) {
		GError *local_error = NULL;
		CamelFolder *folder;

		folder = camel_store_get_folder_sync (store, fd->fullname, 0, cancellable, &local_error);
		if (folder) {
			GNode *parent = NULL;

			tmpl_store_data_lock (fd->tsd);

			if (fd->old_fullname) {
				GNode *node;

				node = tmpl_store_data_find_node_locked (fd->tsd, fd->old_fullname);
				if (!node) {
					/* Sometimes the CamelFolder can be renamed in-place,
					   thus lookup with the CamelFolder structure as well. */
					node = tmpl_store_data_find_node_with_folder_locked (fd->tsd, folder);
				}
				if (node) {
					TmplFolderData *tfd = node->data;

					changed = TRUE;

					tmpl_folder_data_lock (tfd);

					if (tfd->folder != folder) {
						g_clear_object (&tfd->folder);
						tfd->folder = g_object_ref (folder);
					}

					parent = tmpl_store_data_find_parent_node_locked (fd->tsd, fd->fullname, FALSE);
					if (parent && node->parent != parent) {
						g_node_unlink (node);
						g_node_append (parent, node);
					}

					tmpl_folder_data_unlock (tfd);
				}
			} else {
				parent = tmpl_store_data_find_parent_node_locked (fd->tsd, fd->fullname, TRUE);
				if (parent) {
					TmplFolderData *tfd;

					tfd = tmpl_folder_data_new (templates_store, folder);
					if (tfd) {
						changed = tmpl_folder_data_update_sync (tfd, NULL, NULL, cancellable);

						g_node_append_data (parent, tfd);
					}
				}
			}

			if (parent) {
				GSList *data = NULL, *link;
				GNode *node;

				for (node = parent->children; node; node = node->next) {
					if (node->data)
						data = g_slist_prepend (data, node->data);
				}

				data = g_slist_sort (data, tmpl_folder_data_compare);

				for (node = parent->children, link = data; node && link; node = node->next) {
					if (node->data) {
						node->data = link->data;
						link = g_slist_next (link);
					}
				}

				g_slist_free (data);
			}

			tmpl_store_data_unlock (fd->tsd);
		}

		if (local_error)
			g_debug ("%s: Failed to get folder '%s': %s", G_STRFUNC, fd->fullname, local_error->message);

		g_clear_object (&folder);
		g_clear_error (&local_error);
	}

	g_clear_object (&templates_store);
	g_clear_object (&store);

	g_task_return_boolean (task, changed);
}

static void
tmpl_store_data_folder_created_cb (CamelStore *store,
				   CamelFolderInfo *folder_info,
				   gpointer user_data)
{
	TmplStoreData *tsd = user_data;
	EMailTemplatesStore *templates_store;

	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (folder_info != NULL);
	g_return_if_fail (folder_info->full_name != NULL);
	g_return_if_fail (tsd != NULL);

	templates_store = g_weak_ref_get (tsd->templates_store_weakref);

	tmpl_store_data_lock (tsd);

	if (templates_store && g_str_has_prefix (folder_info->full_name, tsd->root_folder_path)) {
		GNode *parent;

		parent = tmpl_store_data_find_parent_node_locked (tsd, folder_info->full_name, TRUE);
		if (parent) {
			TsdFolderData *fd;
			GTask *task;

			fd = g_new0 (TsdFolderData, 1);
			fd->tsd = tmpl_store_data_ref (tsd);
			fd->fullname = g_strdup (folder_info->full_name);
			fd->old_fullname = NULL;

			task = g_task_new (NULL, templates_store->priv->cancellable, tmpl_store_data_update_done_cb, tsd);
			g_task_set_task_data (task, fd, tsd_folder_data_free);
			g_task_run_in_thread (task, tmpl_store_data_folder_thread);
			g_object_unref (task);
		}
	}

	tmpl_store_data_unlock (tsd);
	g_clear_object (&templates_store);
}

static void
tmpl_store_data_folder_deleted_cb (CamelStore *store,
				   CamelFolderInfo *folder_info,
				   gpointer user_data)
{
	TmplStoreData *tsd = user_data;
	EMailTemplatesStore *templates_store;
	gboolean changed = FALSE;

	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (folder_info != NULL);
	g_return_if_fail (tsd != NULL);

	templates_store = g_weak_ref_get (tsd->templates_store_weakref);

	tmpl_store_data_lock (tsd);

	if (templates_store && g_str_has_prefix (folder_info->full_name, tsd->root_folder_path)) {
		GNode *node;

		node = tmpl_store_data_find_node_locked (tsd, folder_info->full_name);
		if (node) {
			g_node_traverse (node, G_IN_ORDER, G_TRAVERSE_ALL, -1, tmpl_store_data_traverse_to_free_cb, NULL);
			g_node_destroy (node);

			changed = TRUE;
		}
	}

	tmpl_store_data_unlock (tsd);

	if (changed)
		templates_store_emit_changed (templates_store);

	g_clear_object (&templates_store);
}

static void
tmpl_store_data_folder_renamed_cb (CamelStore *store,
				   const gchar *old_name,
				   CamelFolderInfo *folder_info,
				   gpointer user_data)
{
	TmplStoreData *tsd = user_data;
	EMailTemplatesStore *templates_store;
	gboolean changed = FALSE;

	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (old_name != NULL);
	g_return_if_fail (folder_info != NULL);
	g_return_if_fail (tsd != NULL);

	templates_store = g_weak_ref_get (tsd->templates_store_weakref);

	tmpl_store_data_lock (tsd);

	if (templates_store && g_str_has_prefix (old_name, tsd->root_folder_path)) {
		if (g_str_has_prefix (folder_info->full_name, tsd->root_folder_path)) {
			TsdFolderData *fd;
			GTask *task;

			fd = g_new0 (TsdFolderData, 1);
			fd->tsd = tmpl_store_data_ref (tsd);
			fd->fullname = g_strdup (folder_info->full_name);
			fd->old_fullname = g_strdup (old_name);

			task = g_task_new (NULL, templates_store->priv->cancellable, tmpl_store_data_update_done_cb, tsd);
			g_task_set_task_data (task, fd, tsd_folder_data_free);
			g_task_run_in_thread (task, tmpl_store_data_folder_thread);
			g_object_unref (task);
		} else {
			GNode *node;

			node = tmpl_store_data_find_node_locked (tsd, old_name);
			if (node) {
				g_node_traverse (node, G_IN_ORDER, G_TRAVERSE_ALL, -1, tmpl_store_data_traverse_to_free_cb, NULL);
				g_node_destroy (node);

				changed = TRUE;
			}
		}
	} else if (templates_store && g_str_has_prefix (folder_info->full_name, tsd->root_folder_path)) {
		TsdFolderData *fd;
		GTask *task;

		fd = g_new0 (TsdFolderData, 1);
		fd->tsd = tmpl_store_data_ref (tsd);
		fd->fullname = g_strdup (folder_info->full_name);
		fd->old_fullname = NULL;

		task = g_task_new (NULL, templates_store->priv->cancellable, tmpl_store_data_update_done_cb, tsd);
		g_task_set_task_data (task, fd, tsd_folder_data_free);
		g_task_run_in_thread (task, tmpl_store_data_folder_thread);
		g_object_unref (task);
	}

	tmpl_store_data_unlock (tsd);

	if (changed)
		templates_store_emit_changed (templates_store);

	g_clear_object (&templates_store);
}

static void
tmpl_store_data_notify_display_name_cb (CamelService *service,
					GParamSpec *param,
					gpointer user_data)
{
	TmplStoreData *tsd = user_data;
	EMailTemplatesStore *templates_store;

	g_return_if_fail (CAMEL_IS_SERVICE (service));
	g_return_if_fail (tsd != NULL);

	templates_store = g_weak_ref_get (tsd->templates_store_weakref);
	if (templates_store) {
		EMailAccountStore *account_store;
		gboolean changed = FALSE;

		account_store = e_mail_templates_store_ref_account_store (templates_store);

		templates_store_lock (templates_store);

		changed = templates_store->priv->stores && templates_store->priv->stores->next;
		templates_store->priv->stores = g_slist_sort_with_data (templates_store->priv->stores,
			tmpl_store_data_compare, account_store);

		templates_store_unlock (templates_store);

		if (changed)
			templates_store_emit_changed (templates_store);

		g_object_unref (templates_store);
		g_clear_object (&account_store);
	}
}

static gchar *
templates_store_find_custom_templates_root_folder_path (EMailTemplatesStore *mail_templates_store,
							CamelStore *store,
							EMailSession *mail_session,
							ESource **out_identity_source,
							CamelStore **out_use_store,
							gchar **out_templates_folder_uri)
{
	ESource *identity_source;
	gchar *root_folder_path = NULL;

	g_return_val_if_fail (E_IS_MAIL_TEMPLATES_STORE (mail_templates_store), NULL);
	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);
	g_return_val_if_fail (out_identity_source != NULL, NULL);
	g_return_val_if_fail (out_use_store != NULL, NULL);
	g_return_val_if_fail (out_templates_folder_uri != NULL, NULL);

	*out_identity_source = NULL;
	*out_use_store = NULL;
	*out_templates_folder_uri = NULL;

	if (g_strcmp0 (E_MAIL_SESSION_LOCAL_UID, camel_service_get_uid (CAMEL_SERVICE (store))) == 0) {
		*out_templates_folder_uri = g_strdup (e_mail_session_get_local_folder_uri (mail_session, E_MAIL_LOCAL_FOLDER_TEMPLATES));
		return g_strdup ("Templates");
	}

	identity_source = em_utils_ref_mail_identity_for_store (e_mail_session_get_registry (mail_session), store);
	if (identity_source) {
		if (e_source_has_extension (identity_source, E_SOURCE_EXTENSION_MAIL_COMPOSITION)) {
			ESourceMailComposition *mail_composition;
			CamelStore *templates_store = NULL;
			gchar *templates_folder;
			GError *local_error = NULL;

			mail_composition = e_source_get_extension (identity_source, E_SOURCE_EXTENSION_MAIL_COMPOSITION);
			templates_folder = e_source_mail_composition_dup_templates_folder (mail_composition);

			if (templates_folder && *templates_folder &&
			    g_strcmp0 (templates_folder, e_mail_session_get_local_folder_uri (mail_session, E_MAIL_LOCAL_FOLDER_TEMPLATES)) != 0 &&
			    e_mail_folder_uri_parse (CAMEL_SESSION (mail_session), templates_folder, &templates_store, &root_folder_path, &local_error)) {
				if (g_strcmp0 (E_MAIL_SESSION_LOCAL_UID, camel_service_get_uid (CAMEL_SERVICE (templates_store))) == 0 &&
				    g_strcmp0 (root_folder_path, "Templates") == 0) {
					g_free (root_folder_path);
					root_folder_path = NULL;
				} else {
					*out_identity_source = g_object_ref (identity_source);
					*out_use_store = g_object_ref (templates_store);
					*out_templates_folder_uri = g_strdup (templates_folder);
				}

				g_clear_object (&templates_store);
			}

			if (local_error) {
				g_debug ("%s: Failed to parse templates folder URI '%s': %s", G_STRFUNC, templates_folder, local_error->message);
				g_clear_error (&local_error);
			}

			g_free (templates_folder);
		}
	}

	g_clear_object (&identity_source);

	return root_folder_path;
}

static void
templates_store_maybe_add_store (EMailTemplatesStore *templates_store,
				 CamelStore *store)
{
	ESource *identity_source = NULL;
	EMailAccountStore *account_store;
	EMailSession *mail_session;
	CamelStore *use_store = NULL;
	gchar *root_folder_path, *templates_folder_uri = NULL;
	gboolean changed = FALSE;

	g_return_if_fail (E_IS_MAIL_TEMPLATES_STORE (templates_store));
	g_return_if_fail (CAMEL_IS_STORE (store));

	account_store = e_mail_templates_store_ref_account_store (templates_store);
	if (!account_store)
		return;

	mail_session = e_mail_account_store_get_session (account_store);

	templates_store_lock (templates_store);

	root_folder_path = templates_store_find_custom_templates_root_folder_path (
		templates_store, store, mail_session, &identity_source, &use_store, &templates_folder_uri);

	if (root_folder_path) {
		TmplStoreData *tsd;
		GSList *link;

		for (link = templates_store->priv->stores; link; link = g_slist_next (link)) {
			CamelStore *tsd_store;

			tsd = link->data;

			if (!tsd)
				continue;

			tsd_store = g_weak_ref_get (tsd->store_weakref);
			if (tsd_store == (use_store ? use_store : store) &&
			    g_strcmp0 (tsd->root_folder_path, root_folder_path) == 0) {
				g_clear_object (&tsd_store);
				break;
			}

			g_clear_object (&tsd_store);
		}

		/* The store is not in the list of stores yet */
		if (!link) {
			tsd = tmpl_store_data_new (templates_store, use_store ? use_store : store, root_folder_path,
				templates_folder_uri, identity_source ? e_source_get_uid (identity_source) : NULL);

			templates_store->priv->stores = g_slist_insert_sorted_with_data (templates_store->priv->stores,
				tsd, tmpl_store_data_compare, account_store);

			tmpl_store_data_schedule_initial_setup (tsd);

			changed = TRUE;
		}
	}

	templates_store_unlock (templates_store);

	if (changed)
		templates_store_emit_changed (templates_store);

	g_free (root_folder_path);
	g_free (templates_folder_uri);
	g_clear_object (&use_store);
	g_clear_object (&identity_source);
	g_clear_object (&account_store);
}

static void
templates_store_maybe_remove_store (EMailTemplatesStore *templates_store,
				    CamelStore *store)
{
	GSList *link;
	gboolean changed = FALSE;

	g_return_if_fail (E_IS_MAIL_TEMPLATES_STORE (templates_store));
	g_return_if_fail (CAMEL_IS_STORE (store));

	templates_store_lock (templates_store);

	for (link = templates_store->priv->stores; link && !changed; link = g_slist_next (link)) {
		TmplStoreData *tsd = link->data;
		CamelStore *other_store;

		if (!tsd)
			continue;

		other_store = g_weak_ref_get (tsd->store_weakref);
		if (other_store == store) {
			changed = TRUE;
			templates_store->priv->stores = g_slist_remove (templates_store->priv->stores, tsd);
			tmpl_store_data_unref (tsd);

			g_object_unref (other_store);
			break;
		}

		g_clear_object (&other_store);
	}

	templates_store_unlock (templates_store);

	if (changed)
		templates_store_emit_changed (templates_store);
}

static void
templates_store_maybe_add_enabled_services (EMailTemplatesStore *templates_store)
{
	EMailAccountStore *account_store;
	GQueue queue = G_QUEUE_INIT;

	g_return_if_fail (E_IS_MAIL_TEMPLATES_STORE (templates_store));
	g_return_if_fail (templates_store->priv->stores == NULL);

	account_store = e_mail_templates_store_ref_account_store (templates_store);
	g_return_if_fail (account_store != NULL);

	e_mail_account_store_queue_enabled_services (account_store, &queue);

	while (!g_queue_is_empty (&queue)) {
		CamelService *service;

		service = g_queue_pop_head (&queue);

		if (CAMEL_IS_STORE (service))
			templates_store_maybe_add_store (templates_store, CAMEL_STORE (service));
	}

	g_clear_object (&account_store);
}

static void
templates_store_service_enabled_cb (EMailAccountStore *account_store,
				    CamelService *service,
				    GWeakRef *weak_ref)
{
	EMailTemplatesStore *templates_store;

	if (!CAMEL_IS_STORE (service))
		return;

	templates_store = g_weak_ref_get (weak_ref);

	if (templates_store) {
		templates_store_maybe_add_store (templates_store, CAMEL_STORE (service));
		g_object_unref (templates_store);
	}
}

static void
templates_store_service_disabled_cb (EMailAccountStore *account_store,
				     CamelService *service,
				     GWeakRef *weak_ref)
{
	EMailTemplatesStore *templates_store;

	if (!CAMEL_IS_STORE (service))
		return;

	templates_store = g_weak_ref_get (weak_ref);

	if (templates_store) {
		templates_store_maybe_remove_store (templates_store, CAMEL_STORE (service));
		g_object_unref (templates_store);
	}
}

static void
templates_store_service_removed_cb (EMailAccountStore *account_store,
				    CamelService *service,
				    GWeakRef *weak_ref)
{
	EMailTemplatesStore *templates_store;

	if (!CAMEL_IS_STORE (service))
		return;

	templates_store = g_weak_ref_get (weak_ref);

	if (templates_store) {
		templates_store_maybe_remove_store (templates_store, CAMEL_STORE (service));
		g_object_unref (templates_store);
	}
}

static void
templates_store_source_changed_cb (ESourceRegistry *registry,
				   ESource *source,
				   GWeakRef *weak_ref)
{
	EMailTemplatesStore *templates_store;

	g_return_if_fail (E_IS_SOURCE (source));

	if (!e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_COMPOSITION))
		return;

	templates_store = g_weak_ref_get (weak_ref);

	if (templates_store) {
		TmplStoreData *corresponding_tsd = NULL;
		ESourceMailComposition *mail_composition;
		gboolean rebuild_all = FALSE;
		gchar *templates_folder;
		GSList *link;

		mail_composition = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_COMPOSITION);
		templates_folder = e_source_mail_composition_dup_templates_folder (mail_composition);

		templates_store_lock (templates_store);

		for (link = templates_store->priv->stores; link; link = g_slist_next (link)) {
			TmplStoreData *tsd = link->data;

			if (!tsd)
				continue;

			if (g_strcmp0 (tsd->identity_source_uid, e_source_get_uid (source)) == 0) {
				g_warn_if_fail (!corresponding_tsd);
				corresponding_tsd = tsd;
				break;
			}
		}

		if (corresponding_tsd) {
			if (g_strcmp0 (templates_folder, corresponding_tsd->templates_folder_uri) != 0) {
				/* Should not happen that often (inefficient, but avoids code complexity). */
				rebuild_all = TRUE;
			}
		} else if (templates_folder && *templates_folder) {
			EMailAccountStore *account_store;
			EMailSession *mail_session;
			CamelStore *found_store = NULL;
			gchar *root_folder_path = NULL;
			GError *local_error = NULL;

			account_store = g_weak_ref_get (templates_store->priv->account_store_weakref);

			if (account_store && (mail_session = e_mail_account_store_get_session (account_store)) != NULL &&
			    g_strcmp0 (templates_folder, e_mail_session_get_local_folder_uri (mail_session, E_MAIL_LOCAL_FOLDER_TEMPLATES)) != 0 &&
			    e_mail_folder_uri_parse (CAMEL_SESSION (mail_session), templates_folder, &found_store, &root_folder_path, &local_error)) {
				if (g_strcmp0 (E_MAIL_SESSION_LOCAL_UID, camel_service_get_uid (CAMEL_SERVICE (found_store))) == 0 &&
				    g_strcmp0 (root_folder_path, "Templates") == 0) {
					g_free (root_folder_path);
					root_folder_path = NULL;
				} else {
					/* One of the templates folders had been changed to a real non-default folder;
					   rebuild everything in this case (inefficient, but avoids code complexity). */
					rebuild_all = TRUE;
				}
			}

			if (local_error) {
				g_debug ("%s: Failed to parse templates folder URI '%s': %s", G_STRFUNC, templates_folder, local_error->message);
				g_clear_error (&local_error);
			}

			g_clear_object (&found_store);
			g_clear_object (&account_store);
			g_free (root_folder_path);
		}

		if (rebuild_all) {
			g_slist_free_full (templates_store->priv->stores, tmpl_store_data_unref);
			templates_store->priv->stores = NULL;
		}

		templates_store_unlock (templates_store);

		if (rebuild_all)
			templates_store_maybe_add_enabled_services (templates_store);

		g_object_unref (templates_store);
		g_free (templates_folder);
	}
}

static void
templates_store_set_account_store (EMailTemplatesStore *templates_store,
				   EMailAccountStore *account_store)
{
	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (account_store));

	g_weak_ref_set (templates_store->priv->account_store_weakref, account_store);
}

static void
templates_store_set_property (GObject *object,
			      guint property_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_STORE:
			templates_store_set_account_store (
				E_MAIL_TEMPLATES_STORE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
templates_store_get_property (GObject *object,
			      guint property_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_STORE:
			g_value_take_object (
				value,
				e_mail_templates_store_ref_account_store (
				E_MAIL_TEMPLATES_STORE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
templates_store_dispose (GObject *object)
{
	EMailTemplatesStore *templates_store;
	EMailAccountStore *account_store;

	templates_store = E_MAIL_TEMPLATES_STORE (object);

	account_store = e_mail_templates_store_ref_account_store (templates_store);

	if (account_store) {
		if (templates_store->priv->service_enabled_handler_id) {
			g_signal_handler_disconnect (account_store, templates_store->priv->service_enabled_handler_id);
			templates_store->priv->service_enabled_handler_id = 0;
		}

		if (templates_store->priv->service_disabled_handler_id) {
			g_signal_handler_disconnect (account_store, templates_store->priv->service_disabled_handler_id);
			templates_store->priv->service_disabled_handler_id = 0;
		}

		if (templates_store->priv->service_removed_handler_id) {
			g_signal_handler_disconnect (account_store, templates_store->priv->service_removed_handler_id);
			templates_store->priv->service_removed_handler_id = 0;
		}

		if (templates_store->priv->source_changed_handler_id) {
			EMailSession *session;
			ESourceRegistry *registry;

			session = e_mail_account_store_get_session (account_store);
			registry = e_mail_session_get_registry (session);

			g_signal_handler_disconnect (registry, templates_store->priv->source_changed_handler_id);
			templates_store->priv->source_changed_handler_id = 0;
		}
	}

	if (templates_store->priv->cancellable) {
		g_cancellable_cancel (templates_store->priv->cancellable);
		g_clear_object (&templates_store->priv->cancellable);
	}

	g_clear_object (&account_store);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_templates_store_parent_class)->dispose (object);
}

static void
templates_store_finalize (GObject *object)
{
	EMailTemplatesStore *templates_store;

	templates_store = E_MAIL_TEMPLATES_STORE (object);

	g_slist_free_full (templates_store->priv->stores, tmpl_store_data_unref);
	templates_store->priv->stores = NULL;

	e_weak_ref_free (templates_store->priv->account_store_weakref);
	templates_store->priv->account_store_weakref = NULL;

	g_mutex_clear (&templates_store->priv->busy_lock);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_templates_store_parent_class)->finalize (object);
}

static void
templates_store_constructed (GObject *object)
{
	EMailTemplatesStore *templates_store;
	ESourceRegistry *registry;
	EMailAccountStore *account_store;
	EMailSession *session;

	templates_store = E_MAIL_TEMPLATES_STORE (object);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_templates_store_parent_class)->constructed (object);

	templates_store->priv->cancellable = g_cancellable_new ();

	account_store = e_mail_templates_store_ref_account_store (templates_store);
	g_return_if_fail (account_store != NULL);

	session = e_mail_account_store_get_session (account_store);
	registry = e_mail_session_get_registry (session);

	templates_store->priv->service_enabled_handler_id = g_signal_connect_data (
		account_store, "service-enabled",
		G_CALLBACK (templates_store_service_enabled_cb),
		e_weak_ref_new (templates_store),
		(GClosureNotify) e_weak_ref_free, 0);

	templates_store->priv->service_disabled_handler_id = g_signal_connect_data (
		account_store, "service-disabled",
		G_CALLBACK (templates_store_service_disabled_cb),
		e_weak_ref_new (templates_store),
		(GClosureNotify) e_weak_ref_free, 0);

	templates_store->priv->service_removed_handler_id = g_signal_connect_data (
		account_store, "service-removed",
		G_CALLBACK (templates_store_service_removed_cb),
		e_weak_ref_new (templates_store),
		(GClosureNotify) e_weak_ref_free, 0);

	templates_store->priv->source_changed_handler_id = g_signal_connect_data (
		registry, "source-changed",
		G_CALLBACK (templates_store_source_changed_cb),
		e_weak_ref_new (templates_store),
		(GClosureNotify) e_weak_ref_free, 0);

	templates_store_maybe_add_enabled_services (templates_store);

	g_clear_object (&account_store);
}

static void
e_mail_templates_store_class_init (EMailTemplatesStoreClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = templates_store_set_property;
	object_class->get_property = templates_store_get_property;
	object_class->dispose = templates_store_dispose;
	object_class->finalize = templates_store_finalize;
	object_class->constructed = templates_store_constructed;

	g_object_class_install_property (
		object_class,
		PROP_ACCOUNT_STORE,
		g_param_spec_object (
			"account-store",
			"Account Store",
			"EMailAccountStore",
			E_TYPE_MAIL_ACCOUNT_STORE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailTemplatesStoreClass, changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0, G_TYPE_NONE);
}

static void
e_mail_templates_store_init (EMailTemplatesStore *templates_store)
{
	templates_store->priv = e_mail_templates_store_get_instance_private (templates_store);

	g_mutex_init (&templates_store->priv->busy_lock);
	templates_store->priv->account_store_weakref = e_weak_ref_new (NULL);
}

EMailTemplatesStore *
e_mail_templates_store_ref_default (EMailAccountStore *account_store)
{
	static gpointer def_templates_store = NULL;

	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (account_store), NULL);

	if (def_templates_store) {
		g_object_ref (def_templates_store);
	} else {
		def_templates_store = g_object_new (E_TYPE_MAIL_TEMPLATES_STORE,
			"account-store", account_store,
			NULL);

		g_object_add_weak_pointer (G_OBJECT (def_templates_store), &def_templates_store);
	}

	return def_templates_store;
}

EMailAccountStore *
e_mail_templates_store_ref_account_store (EMailTemplatesStore *templates_store)
{
	g_return_val_if_fail (E_IS_MAIL_TEMPLATES_STORE (templates_store), NULL);

	return g_weak_ref_get (templates_store->priv->account_store_weakref);
}

static gboolean
tmpl_store_data_folder_has_messages_cb (GNode *node,
					gpointer user_data)
{
	TmplFolderData *tfd;
	gint *pmultiple_accounts = user_data;

	g_return_val_if_fail (node != NULL, TRUE);
	g_return_val_if_fail (pmultiple_accounts != NULL, TRUE);

	if (!node->data)
		return FALSE;

	tfd = node->data;

	if (tfd->messages) {
		*pmultiple_accounts = *pmultiple_accounts + 1;
		return TRUE;
	}

	return FALSE;
}

typedef struct _TmplActionData {
	EMailTemplatesStore *templates_store; /* not referenced */
	CamelFolder *folder;
	const gchar *uid; /* from camel_pstring */
	EMailTemplatesStoreActionFunc action_cb;
	gpointer action_cb_user_data;
} TmplActionData;

static TmplActionData *
tmpl_action_data_new (EMailTemplatesStore *templates_store,
		      CamelFolder *folder,
		      const gchar *uid,
		      EMailTemplatesStoreActionFunc action_cb,
		      gpointer action_cb_user_data)
{
	TmplActionData *tad;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (uid && *uid, NULL);

	tad = g_new0 (TmplActionData, 1);
	tad->templates_store = templates_store;
	tad->folder = g_object_ref (folder);
	tad->uid = camel_pstring_strdup (uid);
	tad->action_cb = action_cb;
	tad->action_cb_user_data = action_cb_user_data;

	return tad;
}

static void
tmpl_action_data_free (gpointer ptr)
{
	TmplActionData *tad = ptr;

	if (tad) {
		g_clear_object (&tad->folder);
		camel_pstring_free (tad->uid);
		g_free (tad);
	}
}

static void
templates_store_action_activate_cb (EUIAction *action,
				    GVariant *parameter,
				    gpointer user_data)
{
	GMenu *top_menu = user_data;
	GHashTable *actions_index;
	TmplActionData *tad;

	g_return_if_fail (G_IS_MENU (top_menu));

	actions_index = g_object_get_data (G_OBJECT (top_menu), TEMPLATES_STORE_ACTIONS_INDEX_KEY);

	g_return_if_fail (actions_index != NULL);

	tad = g_hash_table_lookup (actions_index, GUINT_TO_POINTER (g_variant_get_uint32 (parameter)));

	g_return_if_fail (tad != NULL);
	g_return_if_fail (tad->action_cb != NULL);

	tad->action_cb (tad->templates_store, tad->folder, tad->uid, tad->action_cb_user_data);
}

static void
templates_store_add_to_menu_recurse (EMailTemplatesStore *templates_store,
				     GNode *node,
				     GMenu *parent_menu,
				     EMailTemplatesStoreActionFunc action_cb,
				     gpointer action_cb_user_data,
				     gboolean with_folder_menu,
				     GHashTable *actions_index)
{
	TmplFolderData *tfd;

	g_return_if_fail (node != NULL);

	while (node) {
		tfd = node->data;
		if (tfd) {
			tmpl_folder_data_lock (tfd);

			if (tfd->folder) {
				GMenu *use_parent_menu = parent_menu;
				GSList *link;

				if (with_folder_menu)
					use_parent_menu = g_menu_new ();

				if (node->children) {
					templates_store_add_to_menu_recurse (templates_store, node->children,
						use_parent_menu, action_cb, action_cb_user_data, TRUE, actions_index);
				}

				for (link = tfd->messages; link; link = g_slist_next (link)) {
					TmplMessageData *tmd = link->data;

					if (tmd && tmd->uid && tmd->subject) {
						GMenuItem *item;
						guint action_idx = g_hash_table_size (actions_index) + 1;

						item = g_menu_item_new (tmd->subject, "templates-store.template-use-this");
						g_menu_item_set_attribute (item, G_MENU_ATTRIBUTE_TARGET, "u", action_idx);
						g_menu_append_item (use_parent_menu, item);
						g_clear_object (&item);

						g_hash_table_insert (actions_index, GUINT_TO_POINTER (action_idx),
							tmpl_action_data_new (templates_store, tfd->folder, tmd->uid, action_cb, action_cb_user_data));
					}
				}

				if (use_parent_menu != parent_menu) {
					if (g_menu_model_get_n_items (G_MENU_MODEL (use_parent_menu)) > 0)
						g_menu_append_submenu (parent_menu, camel_folder_get_display_name (tfd->folder), G_MENU_MODEL (use_parent_menu));

					g_clear_object (&use_parent_menu);
				}
			}

			tmpl_folder_data_unlock (tfd);
		}

		node = node->next;
	}
}

void
e_mail_templates_store_update_menu (EMailTemplatesStore *templates_store,
				    GMenu *menu_to_update,
				    EUIManager *ui_manager,
				    EMailTemplatesStoreActionFunc action_cb,
				    gpointer action_cb_user_data)
{
	GSList *link;
	GHashTable *actions_index;
	gint multiple_accounts = 0;

	g_return_if_fail (E_IS_MAIL_TEMPLATES_STORE (templates_store));
	g_return_if_fail (G_IS_MENU (menu_to_update));
	g_return_if_fail (action_cb != NULL);

	templates_store_lock (templates_store);

	g_menu_remove_all (menu_to_update);
	actions_index = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, tmpl_action_data_free);

	if (!e_ui_manager_has_action_group (ui_manager, "templates-store")) {
		EUIAction *action;

		action = e_ui_action_new ("templates-store", "template-use-this", G_VARIANT_TYPE_UINT32);
		e_ui_action_set_label (action, "template-use-this");

		e_ui_manager_add_action (ui_manager, e_ui_action_get_map_name (action), action, templates_store_action_activate_cb, NULL, menu_to_update);
	}

	for (link = templates_store->priv->stores; link && multiple_accounts <= 1; link = g_slist_next (link)) {
		TmplStoreData *tsd = link->data;

		if (!tsd)
			continue;

		tmpl_store_data_lock (tsd);

		if (tsd->folders && tsd->folders->children) {
			CamelStore *store;

			store = g_weak_ref_get (tsd->store_weakref);
			if (store) {
				g_node_traverse (tsd->folders, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
					tmpl_store_data_folder_has_messages_cb, &multiple_accounts);
			}

			g_clear_object (&store);
		}

		tmpl_store_data_unlock (tsd);
	}

	for (link = templates_store->priv->stores; link && multiple_accounts > 0; link = g_slist_next (link)) {
		TmplStoreData *tsd = link->data;

		if (!tsd)
			continue;

		tmpl_store_data_lock (tsd);

		if (tsd->folders && tsd->folders->children) {
			CamelStore *store;

			store = g_weak_ref_get (tsd->store_weakref);
			if (store) {
				GMenu *parent_menu = menu_to_update;

				if (multiple_accounts > 1) {
					parent_menu = g_menu_new ();
				}

				templates_store_add_to_menu_recurse (templates_store, tsd->folders->children,
					parent_menu, action_cb, action_cb_user_data, FALSE, actions_index);

				if (parent_menu != menu_to_update) {
					if (g_menu_model_get_n_items (G_MENU_MODEL (parent_menu)) > 0)
						g_menu_append_submenu (menu_to_update, camel_service_get_display_name (CAMEL_SERVICE (store)), G_MENU_MODEL (parent_menu));

					g_object_unref (parent_menu);
				}
			}

			g_clear_object (&store);
		}

		tmpl_store_data_unlock (tsd);
	}

	templates_store_unlock (templates_store);

	if (g_hash_table_size (actions_index) > 0) {
		g_object_set_data_full (G_OBJECT (menu_to_update), TEMPLATES_STORE_ACTIONS_INDEX_KEY,
			actions_index, (GDestroyNotify) g_hash_table_unref);
	} else {
		g_object_set_data_full (G_OBJECT (menu_to_update), TEMPLATES_STORE_ACTIONS_INDEX_KEY, NULL, NULL);
		g_hash_table_unref (actions_index);
	}
}

static void
templates_store_add_to_tree_store_recurse (EMailTemplatesStore *templates_store,
					   GNode *node,
					   GtkTreeStore *tree_store,
					   GtkTreeIter *parent,
					   gboolean with_folder_name,
					   const gchar *find_folder_uri,
					   const gchar *find_message_uid,
					   gboolean *out_found_message,
					   GtkTreeIter *out_found_iter,
					   gboolean *out_found_first_message,
					   GtkTreeIter *out_found_first_iter)
{
	TmplFolderData *tfd;

	g_return_if_fail (node != NULL);
	g_return_if_fail (tree_store != NULL);

	while (node) {
		tfd = node->data;
		if (tfd) {
			tmpl_folder_data_lock (tfd);

			if (tfd->folder) {
				GtkTreeIter *pparent = parent, iparent, iter;
				GSList *link;
				gboolean is_the_folder = FALSE;

				if (out_found_message && !*out_found_message && out_found_iter && find_folder_uri && *find_folder_uri) {
					gchar *folder_uri;

					folder_uri = e_mail_folder_uri_from_folder (tfd->folder);
					is_the_folder = g_strcmp0 (folder_uri, find_folder_uri) == 0;
					g_free (folder_uri);
				}

				if (with_folder_name) {
					gtk_tree_store_append (tree_store, &iparent, pparent);
					gtk_tree_store_set (tree_store, &iparent,
						E_MAIL_TEMPLATES_STORE_COLUMN_DISPLAY_NAME, camel_folder_get_display_name (tfd->folder),
						-1);

					pparent = &iparent;
				}

				if (node->children) {
					templates_store_add_to_tree_store_recurse (templates_store, node->children, tree_store, pparent,
						TRUE, find_folder_uri, find_message_uid, out_found_message, out_found_iter,
						out_found_first_message, out_found_first_iter);
				}

				for (link = tfd->messages; link; link = g_slist_next (link)) {
					TmplMessageData *tmd = link->data;

					if (tmd && tmd->uid && tmd->subject) {
						gtk_tree_store_append (tree_store, &iter, pparent);
						gtk_tree_store_set (tree_store, &iter,
							E_MAIL_TEMPLATES_STORE_COLUMN_DISPLAY_NAME, tmd->subject,
							E_MAIL_TEMPLATES_STORE_COLUMN_FOLDER, tfd->folder,
							E_MAIL_TEMPLATES_STORE_COLUMN_MESSAGE_UID, tmd->uid,
							-1);

						if (!*out_found_first_message) {
							*out_found_first_message = TRUE;
							*out_found_first_iter = iter;
						}

						if (is_the_folder && out_found_message && !*out_found_message) {
							*out_found_message = g_strcmp0 (tmd->uid, find_message_uid) == 0;

							if (*out_found_message && out_found_iter)
								*out_found_iter = iter;
						}
					}
				}
			}

			tmpl_folder_data_unlock (tfd);
		}

		node = node->next;
	}
}

GtkTreeStore *
e_mail_templates_store_build_model (EMailTemplatesStore *templates_store,
				    const gchar *find_folder_uri,
				    const gchar *find_message_uid,
				    gboolean *out_found_message,
				    GtkTreeIter *out_found_iter)
{
	GtkTreeStore *tree_store;
	GSList *link;
	gint multiple_accounts = 0;
	gboolean found_first_message = FALSE;
	GtkTreeIter found_first_iter = { 0, };

	g_return_val_if_fail (E_IS_MAIL_TEMPLATES_STORE (templates_store), NULL);

	if (out_found_message)
		*out_found_message = FALSE;

	tree_store = gtk_tree_store_new (E_MAIL_TEMPLATES_STORE_N_COLUMNS,
		G_TYPE_STRING,		/* E_MAIL_TEMPLATES_STORE_COLUMN_DISPLAY_NAME */
		CAMEL_TYPE_FOLDER,	/* E_MAIL_TEMPLATES_STORE_COLUMN_FOLDER */
		G_TYPE_STRING);		/* E_MAIL_TEMPLATES_STORE_COLUMN_MESSAGE_UID */

	templates_store_lock (templates_store);

	for (link = templates_store->priv->stores; link && multiple_accounts <= 1; link = g_slist_next (link)) {
		TmplStoreData *tsd = link->data;

		if (!tsd)
			continue;

		tmpl_store_data_lock (tsd);

		if (tsd->folders && tsd->folders->children) {
			CamelStore *store;

			store = g_weak_ref_get (tsd->store_weakref);
			if (store) {
				g_node_traverse (tsd->folders, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
					tmpl_store_data_folder_has_messages_cb, &multiple_accounts);
			}

			g_clear_object (&store);
		}

		tmpl_store_data_unlock (tsd);
	}

	for (link = templates_store->priv->stores; link && multiple_accounts > 0; link = g_slist_next (link)) {
		TmplStoreData *tsd = link->data;

		if (!tsd)
			continue;

		tmpl_store_data_lock (tsd);

		if (tsd->folders && tsd->folders->children) {
			CamelStore *store;

			store = g_weak_ref_get (tsd->store_weakref);
			if (store) {
				GtkTreeIter *pparent = NULL, parent;

				if (multiple_accounts > 1) {
					gtk_tree_store_append (tree_store, &parent, NULL);
					gtk_tree_store_set (tree_store, &parent,
						E_MAIL_TEMPLATES_STORE_COLUMN_DISPLAY_NAME, camel_service_get_display_name (CAMEL_SERVICE (store)),
						-1);

					pparent = &parent;
				}

				templates_store_add_to_tree_store_recurse (templates_store, tsd->folders->children, tree_store, pparent, FALSE,
					find_folder_uri, find_message_uid, out_found_message, out_found_iter,
					&found_first_message, &found_first_iter);
			}

			g_clear_object (&store);
		}

		tmpl_store_data_unlock (tsd);
	}

	templates_store_unlock (templates_store);

	if (out_found_message && !*out_found_message && out_found_iter) {
		*out_found_message = found_first_message;
		*out_found_iter = found_first_iter;
	}

	return tree_store;
}
