/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <libxml/tree.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n.h>

#include <e-util/e-util.h>

#include "em-vfolder-editor-rule.h"

#include "e-mail-ui-session.h"
#include "em-utils.h"
#include "em-folder-tree.h"
#include "em-folder-tree-model.h"
#include "em-folder-selector.h"
#include "em-folder-properties.h"
#include "mail-vfolder-ui.h"

#include "em-folder-utils.h"

#define d(x)

static gboolean
emfu_is_special_local_folder (const gchar *name)
{
	return (!strcmp (name, "Drafts") ||
		!strcmp (name, "Inbox") ||
		!strcmp (name, "Outbox") ||
		!strcmp (name, "Sent") ||
		!strcmp (name, "Templates"));
}

struct _EMCopyFolders {
	MailMsg base;

	/* input data */
	CamelStore *fromstore;
	CamelStore *tostore;

	gchar *frombase;
	gchar *tobase;

	gint delete;
};

static guint32
emft_copy_folders_count_n_folders (CamelFolderInfo *fi,
				   gboolean can_subfolders)
{
	guint32 n_folders = 0;

	while (fi) {
		n_folders++;

		if (fi->child && can_subfolders)
			n_folders += emft_copy_folders_count_n_folders (fi->child, can_subfolders);

		fi = fi->next;
	}

	return n_folders;
}

static void
emft_copy_folders_pass_progress_cb (GCancellable *local_cancellable,
				    gint percent,
				    gpointer user_data)
{
	GCancellable *caller_cancellable = user_data;

	camel_operation_progress (caller_cancellable, percent);
}

static gchar *
emft_copy_folders__desc (struct _EMCopyFolders *m,
                         gint complete)
{
	if (m->delete)
		return g_strdup_printf (_("Moving folder “%s”"), m->frombase);
	else
		return g_strdup_printf (_("Copying folder “%s”"), m->frombase);
}

static void
emft_copy_folders__exec (struct _EMCopyFolders *m,
                         GCancellable *cancellable,
                         GError **error)
{
	guint32 flags, n_folders, nth_folder = 1;
	GList *pending = NULL, *deleting = NULL, *l;
	GString *fromname, *toname;
	CamelFolderInfo *fi;
	const gchar *tmp;
	gint fromlen;
	gboolean same_store = m->fromstore == m->tostore;

	flags = CAMEL_STORE_FOLDER_INFO_FAST |
		CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;

	/* If we're copying, then we need to copy every subfolder. If we're
	 * *moving*, though, then we only need to rename the top-level folder
	 * when moving within the same store, otherwise move whole structure */
	if (!m->delete || !same_store)
		flags |= CAMEL_STORE_FOLDER_INFO_RECURSIVE;

	fi = camel_store_get_folder_info_sync (
		m->fromstore, m->frombase, flags, cancellable, error);
	if (fi == NULL)
		return;

	n_folders = emft_copy_folders_count_n_folders (fi, (!m->delete || !same_store));
	pending = g_list_append (pending, fi);

	toname = g_string_new ("");
	fromname = g_string_new ("");

	tmp = strrchr (m->frombase, '/');
	if (tmp == NULL)
		fromlen = 0;
	else
		fromlen = tmp - m->frombase + 1;

	d (printf ("top name is '%s'\n", fi->full_name));

	while (pending) {
		CamelFolderInfo *info = pending->data;

		pending = g_list_delete_link (pending, pending);
		while (info) {
			CamelFolder *fromfolder, *tofolder;
			GPtrArray *uids;
			gint deleted = 0;

			/* We still get immediate children even without the
			 * CAMEL_STORE_FOLDER_INFO_RECURSIVE flag. But we only
			 * want to process the children too if we're *copying*
			 * or moving between different stores */
			if (info->child && (!m->delete || !same_store))
				pending = g_list_append (pending, info->child);

			if (m->tobase[0])
				g_string_printf (
					toname, "%s/%s", m->tobase,
					info->full_name + fromlen);
			else
				g_string_printf (
					toname, "%s",
					info->full_name + fromlen);

			d (printf (
				"Copying from '%s' to '%s'\n",
				info->full_name, toname->str));

			/* This makes sure we create the same tree,
			 * e.g. from a nonselectable source. */
			/* Not sure if this is really the 'right thing',
			 * e.g. for spool stores, but it makes the ui work. */
			if ((info->flags & CAMEL_FOLDER_NOSELECT) == 0) {
				d (printf ("this folder is selectable\n"));
				if (same_store && m->delete) {
					if (!camel_store_rename_folder_sync (
						m->fromstore,
						info->full_name,
						toname->str,
						cancellable, error))
						goto exception;

					/* this folder no longer exists, unsubscribe it */
					if (CAMEL_IS_SUBSCRIBABLE (m->fromstore))
						camel_subscribable_unsubscribe_folder_sync (
							CAMEL_SUBSCRIBABLE (m->fromstore),
							info->full_name, cancellable, NULL);

					deleted = 1;
				} else {
					GCancellable *local_cancellable;
					gulong handler_id_cancelled;
					gulong handler_id_progress;
					gboolean success;

					fromfolder = camel_store_get_folder_sync (
						m->fromstore, info->full_name, 0,
						cancellable, error);
					if (fromfolder == NULL)
						goto exception;

					/* Refresh the source folder first, to get up-to-date content */
					if (!camel_folder_refresh_info_sync (fromfolder, cancellable, error)) {
						g_object_unref (fromfolder);
						goto exception;
					}

					tofolder = camel_store_get_folder_sync (
						m->tostore, toname->str,
						CAMEL_STORE_FOLDER_CREATE,
						cancellable, error);
					if (tofolder == NULL) {
						g_object_unref (fromfolder);
						goto exception;
					}

					/* to not propagate status messages from sub-functions into UI */
					if (cancellable) {
						local_cancellable = camel_operation_new ();
						handler_id_cancelled = g_signal_connect_swapped (cancellable, "cancelled",
							G_CALLBACK (g_cancellable_cancel), local_cancellable);
						handler_id_progress = g_signal_connect (local_cancellable, "progress",
							G_CALLBACK (emft_copy_folders_pass_progress_cb), cancellable);
					} else {
						local_cancellable = NULL;
						handler_id_cancelled = 0;
						handler_id_progress = 0;
					}

					if (n_folders > 1) {
						gchar *full_display_name;

						full_display_name = e_mail_folder_to_full_display_name (fromfolder, NULL);
						camel_operation_push_message (cancellable,
							m->delete ? _("Moving folder “%s” (%d/%d)") : _("Copying folder “%s” (%d/%d)"),
							full_display_name ? full_display_name : camel_folder_get_display_name (fromfolder),
							nth_folder, n_folders);
						g_free (full_display_name);
					}

					uids = camel_folder_dup_uids (fromfolder);
					success = camel_folder_transfer_messages_to_sync (
						fromfolder, uids, tofolder,
						m->delete, NULL,
						local_cancellable, error);
					g_ptr_array_unref (uids);

					if (n_folders > 1)
						camel_operation_pop_message (cancellable);

					if (cancellable) {
						g_signal_handler_disconnect (cancellable, handler_id_cancelled);
						g_signal_handler_disconnect (local_cancellable, handler_id_progress);
					}

					g_clear_object (&local_cancellable);

					if (m->delete && success) {
						camel_folder_synchronize_sync (
							fromfolder, TRUE,
							cancellable, NULL);
					}

					g_object_unref (fromfolder);
					g_object_unref (tofolder);

					if (!success)
						goto exception;
				}
			}

			if (error && *error)
				goto exception;
			else if (m->delete && !deleted)
				deleting = g_list_prepend (deleting, info);

			/* subscribe to the new folder if appropriate */
			if (CAMEL_IS_SUBSCRIBABLE (m->tostore)
			    && !camel_subscribable_folder_is_subscribed (
					CAMEL_SUBSCRIBABLE (m->tostore),
					toname->str))
				camel_subscribable_subscribe_folder_sync (
					CAMEL_SUBSCRIBABLE (m->tostore),
					toname->str, cancellable, NULL);

			info = info->next;
			nth_folder++;
		}
	}

	/* Delete the folders in reverse order from how we copied them,
	 * if we are deleting any. */
	l = deleting;
	while (l) {
		CamelFolderInfo *info = l->data;

		d (printf ("deleting folder '%s'\n", info->full_name));

		/* FIXME: we need to do something with the exception
		 * since otherwise the users sees a failed operation
		 * with no error message or even any warnings */
		if (CAMEL_IS_SUBSCRIBABLE (m->fromstore))
			camel_subscribable_unsubscribe_folder_sync (
				CAMEL_SUBSCRIBABLE (m->fromstore),
				info->full_name, cancellable, NULL);

		camel_store_delete_folder_sync (m->fromstore, info->full_name, cancellable, NULL);
		l = l->next;
	}

exception:
	camel_folder_info_free (fi);
	g_list_free (deleting);

	g_string_free (toname, TRUE);
	g_string_free (fromname, TRUE);
}

static void
emft_copy_folders__free (struct _EMCopyFolders *m)
{
	g_object_unref (m->fromstore);
	g_object_unref (m->tostore);

	g_free (m->frombase);
	g_free (m->tobase);
}

static MailMsgInfo copy_folders_info = {
	sizeof (struct _EMCopyFolders),
	(MailMsgDescFunc) emft_copy_folders__desc,
	(MailMsgExecFunc) emft_copy_folders__exec,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) emft_copy_folders__free
};

gint
em_folder_utils_copy_folders (CamelStore *fromstore,
                              const gchar *frombase,
                              CamelStore *tostore,
                              const gchar *tobase,
                              gint delete)
{
	struct _EMCopyFolders *m;
	gint seq;

	m = mail_msg_new (&copy_folders_info);
	g_object_ref (fromstore);
	m->fromstore = fromstore;
	g_object_ref (tostore);
	m->tostore = tostore;
	m->frombase = g_strdup (frombase);
	m->tobase = g_strdup (tobase);
	m->delete = delete;
	seq = m->base.seq;

	mail_msg_unordered_push (m);

	return seq;
}

struct _copy_folder_data {
	CamelStore *source_store;
	gchar *source_folder_name;
	gboolean delete;
};

static void
emfu_copy_folder_selected (EMailSession *session,
                           EAlertSink *alert_sink,
                           const gchar *uri,
                           gpointer data)
{
	struct _copy_folder_data *cfd = data;
	CamelProvider *provider, *toprovider;
	CamelStore *tostore = NULL;
	CamelService *service, *toservice;
	gboolean store_is_local, tostore_is_local, session_is_online;
	const gchar *display_name, *todisplay_name;
	gchar *tobase = NULL;
	gchar *folder_name = NULL, *tofolder_name = NULL;
	GError *local_error = NULL;

	if (uri == NULL)
		goto fail;

	session_is_online = camel_session_get_online (CAMEL_SESSION (session));

	service = CAMEL_SERVICE (cfd->source_store);
	provider = camel_service_get_provider (service);
	store_is_local = (provider->flags & CAMEL_PROVIDER_IS_LOCAL) != 0;
	display_name = camel_service_get_display_name (service);
	folder_name = g_strdup_printf ("%s: %s", display_name, cfd->source_folder_name);

	e_mail_folder_uri_parse (
		CAMEL_SESSION (session), uri,
		&tostore, &tobase, &local_error);

	if (local_error != NULL) {
		e_alert_submit (
			alert_sink, cfd->delete ?
			"mail:no-move-folder-to-nostore" :
			"mail:no-copy-folder-to-nostore",
			folder_name, uri,
			local_error->message, NULL);
		goto fail;
	}

	g_return_if_fail (CAMEL_IS_STORE (service));

	toservice = CAMEL_SERVICE (tostore);
	toprovider = camel_service_get_provider (toservice);
	tostore_is_local = (toprovider->flags & CAMEL_PROVIDER_IS_LOCAL) != 0;
	todisplay_name = camel_service_get_display_name (toservice);
	tofolder_name = g_strdup_printf ("%s: %s", todisplay_name, tobase);

	if (!session_is_online && (!store_is_local || !tostore_is_local)) {
		e_alert_submit (
			alert_sink,
			"mail:online-operation",
			store_is_local ? tofolder_name : folder_name,
			NULL);
		goto fail;
	}

	camel_service_connect_sync (service, NULL, &local_error);
	if (local_error != NULL) {
		e_alert_submit (
			alert_sink, cfd->delete ?
			"mail:no-move-folder-nostore" :
			"mail:no-copy-folder-nostore",
			folder_name, tofolder_name,
			local_error->message, NULL);
		goto fail;
	}

	if (cfd->delete && store_is_local &&
		emfu_is_special_local_folder (cfd->source_folder_name)) {
		e_alert_submit (
			alert_sink,
			"mail:no-rename-special-folder",
			folder_name, NULL);
		goto fail;
	}

	camel_service_connect_sync (toservice, NULL, &local_error);
	if (local_error != NULL) {
		e_alert_submit (
			alert_sink, cfd->delete ?
			"mail:no-move-folder-to-nostore" :
			"mail:no-copy-folder-to-nostore",
			folder_name, tofolder_name,
			local_error->message, NULL);
		goto fail;
	}

	g_return_if_fail (CAMEL_IS_STORE (tostore));

	em_folder_utils_copy_folders (
		cfd->source_store, cfd->source_folder_name,
		tostore, tobase ? tobase : "", cfd->delete);

fail:
	g_clear_error (&local_error);

	g_object_unref (cfd->source_store);
	g_free (cfd->source_folder_name);
	g_free (cfd);

	if (tostore)
		g_object_unref (tostore);
	g_free (tobase);
	g_free (folder_name);
	g_free (tofolder_name);
}

/* tree here is the 'destination' selector, not 'self' */
static gboolean
emfu_copy_folder_exclude (EMFolderTree *tree,
                          GtkTreeModel *model,
                          GtkTreeIter *iter,
                          gpointer data)
{
	struct _copy_folder_data *cfd = data;
	CamelStore *store;
	const gchar *uid;
	gint fromvfolder, tovfolder;
	guint flags;

	/* handles moving to/from vfolders */

	uid = camel_service_get_uid (CAMEL_SERVICE (cfd->source_store));
	fromvfolder = (g_strcmp0 (uid, E_MAIL_SESSION_VFOLDER_UID) == 0);

	gtk_tree_model_get (
		model, iter,
		COL_UINT_FLAGS, &flags,
		COL_OBJECT_CAMEL_STORE, &store, -1);

	uid = camel_service_get_uid (CAMEL_SERVICE (store));
	tovfolder = (g_strcmp0 (uid, E_MAIL_SESSION_VFOLDER_UID) == 0);
	g_object_unref (store);

	/* moving from vfolder to normal- not allowed */
	if (fromvfolder && !tovfolder && cfd->delete)
		return FALSE;
	/* copy/move from normal folder to vfolder - not allowed */
	if (!fromvfolder && tovfolder)
		return FALSE;
	/* copying to vfolder - not allowed */
	if (tovfolder && !cfd->delete)
		return FALSE;

	return (flags & EMFT_EXCLUDE_NOINFERIORS) == 0;
}

void
em_folder_utils_copy_folder (GtkWindow *parent,
                             EMailSession *session,
                             EAlertSink *alert_sink,
                             const gchar *folder_uri,
                             gint delete)
{
	GtkWidget *dialog;
	EMFolderSelector *selector;
	EMFolderTree *folder_tree;
	EMFolderTreeModel *model;
	const gchar *label;
	const gchar *title;
	struct _copy_folder_data *cfd;
	GError *error = NULL;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (E_IS_ALERT_SINK (alert_sink));
	g_return_if_fail (folder_uri != NULL);

	cfd = g_malloc (sizeof (*cfd));
	cfd->delete = delete;

	e_mail_folder_uri_parse (
		CAMEL_SESSION (session), folder_uri,
		&cfd->source_store, &cfd->source_folder_name, &error);

	if (error != NULL) {
		e_notice (parent, GTK_MESSAGE_ERROR, "%s", error->message);
		g_error_free (error);
		g_free (cfd);
		return;
	}

	label = delete ? _("_Move") : _("C_opy");
	title = delete ? _("Move Folder To") : _("Copy Folder To");

	model = em_folder_tree_model_get_default ();

	dialog = em_folder_selector_new (parent, model);

	gtk_window_set_title (GTK_WINDOW (dialog), title);

	selector = EM_FOLDER_SELECTOR (dialog);
	em_folder_selector_set_can_create (selector, TRUE);
	em_folder_selector_set_default_button_label (selector, label);

	folder_tree = em_folder_selector_get_folder_tree (selector);

	em_folder_tree_set_excluded_func (
		folder_tree, emfu_copy_folder_exclude, cfd);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		const gchar *uri;

		uri = em_folder_selector_get_selected_uri (selector);
		emfu_copy_folder_selected (session, alert_sink, uri, cfd);
	}

	gtk_widget_destroy (dialog);
}

const gchar *
em_folder_utils_get_icon_name (guint32 flags)
{
	const gchar *icon_name;

	switch (flags & CAMEL_FOLDER_TYPE_MASK) {
		case CAMEL_FOLDER_TYPE_INBOX:
			icon_name = "mail-inbox";
			break;
		case CAMEL_FOLDER_TYPE_OUTBOX:
			icon_name = "mail-outbox";
			break;
		case CAMEL_FOLDER_TYPE_TRASH:
			icon_name = "user-trash";
			break;
		case CAMEL_FOLDER_TYPE_JUNK:
			icon_name = "mail-mark-junk";
			break;
		case CAMEL_FOLDER_TYPE_SENT:
			icon_name = "mail-sent";
			break;
		case CAMEL_FOLDER_TYPE_CONTACTS:
			icon_name = "x-office-address-book";
			break;
		case CAMEL_FOLDER_TYPE_EVENTS:
			icon_name = "x-office-calendar";
			break;
		case CAMEL_FOLDER_TYPE_MEMOS:
			icon_name = "evolution-memos";
			break;
		case CAMEL_FOLDER_TYPE_TASKS:
			icon_name = "evolution-tasks";
			break;
		default:
			if (flags & CAMEL_FOLDER_SHARED_TO_ME)
				icon_name = "stock_shared-to-me";
			else if (flags & CAMEL_FOLDER_SHARED_BY_ME)
				icon_name = "stock_shared-by-me";
			else if (flags & CAMEL_FOLDER_VIRTUAL)
				icon_name = "folder-saved-search";
			else
				icon_name = "folder";
	}

	return icon_name;
}
