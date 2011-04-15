/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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

#include "e-util/e-mktemp.h"

#include "e-util/e-alert-dialog.h"

#include "em-vfolder-rule.h"

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-vfolder.h"
#include "mail-folder-cache.h"

#include "em-utils.h"
#include "em-folder-tree.h"
#include "em-folder-tree-model.h"
#include "em-folder-utils.h"
#include "em-folder-selector.h"
#include "em-folder-properties.h"

#include "e-mail-local.h"
#include "e-mail-session.h"
#include "e-mail-store.h"

#define d(x)

static gboolean
emfu_is_special_local_folder (const gchar *name)
{
	return (!strcmp (name, "Drafts") || !strcmp (name, "Inbox") || !strcmp (name, "Outbox") || !strcmp (name, "Sent") || !strcmp (name, "Templates"));
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

static gchar *
emft_copy_folders__desc (struct _EMCopyFolders *m, gint complete)
{
	if (m->delete)
		return g_strdup_printf (_("Moving folder %s"), m->frombase);
	else
		return g_strdup_printf (_("Copying folder %s"), m->frombase);
}

static void
emft_copy_folders__exec (struct _EMCopyFolders *m,
                         GCancellable *cancellable,
                         GError **error)
{
	guint32 flags;
	GList *pending = NULL, *deleting = NULL, *l;
	GString *fromname, *toname;
	CamelFolderInfo *fi;
	const gchar *tmp;
	gint fromlen;

	flags = CAMEL_STORE_FOLDER_INFO_FAST |
		CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;

	/* If we're copying, then we need to copy every subfolder. If we're
	   *moving*, though, then we only need to rename the top-level folder */
	if (!m->delete)
		flags |= CAMEL_STORE_FOLDER_INFO_RECURSIVE;

	fi = camel_store_get_folder_info_sync (
		m->fromstore, m->frombase, flags, cancellable, error);
	if (fi == NULL)
		return;

	pending = g_list_append (pending, fi);

	toname = g_string_new ("");
	fromname = g_string_new ("");

	tmp = strrchr (m->frombase, '/');
	if (tmp == NULL)
		fromlen = 0;
	else
		fromlen = tmp - m->frombase + 1;

	d(printf ("top name is '%s'\n", fi->full_name));

	while (pending) {
		CamelFolderInfo *info = pending->data;

		pending = g_list_remove_link (pending, pending);
		while (info) {
			CamelFolder *fromfolder, *tofolder;
			GPtrArray *uids;
			gint deleted = 0;

			/* We still get immediate children even without the
			   CAMEL_STORE_FOLDER_INFO_RECURSIVE flag. But we only
			   want to process the children too if we're *copying* */
			if (info->child && !m->delete)
				pending = g_list_append (pending, info->child);

			if (m->tobase[0])
				g_string_printf (toname, "%s/%s", m->tobase, info->full_name + fromlen);
			else
				g_string_printf (toname, "%s", info->full_name + fromlen);

			d(printf ("Copying from '%s' to '%s'\n", info->full_name, toname->str));

			/* This makes sure we create the same tree, e.g. from a nonselectable source */
			/* Not sure if this is really the 'right thing', e.g. for spool stores, but it makes the ui work */
			if ((info->flags & CAMEL_FOLDER_NOSELECT) == 0) {
				d(printf ("this folder is selectable\n"));
				if (m->tostore == m->fromstore && m->delete) {
					camel_store_rename_folder_sync (
						m->fromstore, info->full_name, toname->str,
						cancellable, error);
					if (error && *error)
						goto exception;

					/* this folder no longer exists, unsubscribe it */
					if (camel_store_supports_subscriptions (m->fromstore))
						camel_store_unsubscribe_folder_sync (
							m->fromstore, info->full_name, NULL, NULL);

					deleted = 1;
				} else {
					fromfolder = camel_store_get_folder_sync (
						m->fromstore, info->full_name, 0,
						cancellable, error);
					if (fromfolder == NULL)
						goto exception;

					tofolder = camel_store_get_folder_sync (
						m->tostore, toname->str,
						CAMEL_STORE_FOLDER_CREATE,
						cancellable, error);
					if (tofolder == NULL) {
						g_object_unref (fromfolder);
						goto exception;
					}

					uids = camel_folder_get_uids (fromfolder);
					camel_folder_transfer_messages_to_sync (
						fromfolder, uids, tofolder,
						m->delete, NULL,
						cancellable, error);
					camel_folder_free_uids (fromfolder, uids);

					if (m->delete && (!error || !*error))
						camel_folder_synchronize_sync (
							fromfolder, TRUE,
							NULL, NULL);

					g_object_unref (fromfolder);
					g_object_unref (tofolder);
				}
			}

			if (error && *error)
				goto exception;
			else if (m->delete && !deleted)
				deleting = g_list_prepend (deleting, info);

			/* subscribe to the new folder if appropriate */
			if (camel_store_supports_subscriptions (m->tostore)
			    && !camel_store_folder_is_subscribed (m->tostore, toname->str))
				camel_store_subscribe_folder_sync (
					m->tostore, toname->str, NULL, NULL);

			info = info->next;
		}
	}

	/* delete the folders in reverse order from how we copyied them, if we are deleting any */
	l = deleting;
	while (l) {
		CamelFolderInfo *info = l->data;

		d(printf ("deleting folder '%s'\n", info->full_name));

		/* FIXME: we need to do something with the exception
		   since otherwise the users sees a failed operation
		   with no error message or even any warnings */
		if (camel_store_supports_subscriptions (m->fromstore))
			camel_store_unsubscribe_folder_sync (
				m->fromstore, info->full_name, NULL, NULL);

		camel_store_delete_folder_sync (
			m->fromstore, info->full_name, NULL, NULL);
		l = l->next;
	}

 exception:

	camel_store_free_folder_info (m->fromstore, fi);
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
em_folder_utils_copy_folders (CamelStore *fromstore, const gchar *frombase, CamelStore *tostore, const gchar *tobase, gint delete)
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
	CamelFolderInfo *fi;
	gboolean delete;
};

static void
emfu_copy_folder_selected (EMailBackend *backend,
                           const gchar *uri,
                           gpointer data)
{
	EMailSession *session;
	struct _copy_folder_data *cfd = data;
	CamelStore *fromstore = NULL, *tostore = NULL;
	CamelStore *local_store;
	const gchar *tobase = NULL;
	CamelURL *url;
	GError *local_error = NULL;

	if (uri == NULL) {
		g_free (cfd);
		return;
	}

	local_store = e_mail_local_get_store ();
	session = e_mail_backend_get_session (backend);

	fromstore = camel_session_get_store (
		CAMEL_SESSION (session), cfd->fi->uri, &local_error);
	if (fromstore == NULL) {
		e_mail_backend_submit_alert (
			backend, cfd->delete ?
				"mail:no-move-folder-notexist" :
				"mail:no-copy-folder-notexist",
			cfd->fi->full_name, uri,
			local_error->message, NULL);
		goto fail;
	}

	if (cfd->delete && fromstore == local_store && emfu_is_special_local_folder (cfd->fi->full_name)) {
		e_mail_backend_submit_alert (
			backend, "mail:no-rename-special-folder",
			cfd->fi->full_name, NULL);
		goto fail;
	}

	tostore = camel_session_get_store (
		CAMEL_SESSION (session), uri, &local_error);
	if (tostore == NULL) {
		e_mail_backend_submit_alert (
			backend, cfd->delete ?
				"mail:no-move-folder-to-notexist" :
				"mail:no-copy-folder-to-notexist",
			cfd->fi->full_name, uri,
			local_error->message, NULL);
		goto fail;
	}

	url = camel_url_new (uri, NULL);
	if (((CamelService *)tostore)->provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)
		tobase = url->fragment;
	else if (url->path && url->path[0])
		tobase = url->path+1;
	if (tobase == NULL)
		tobase = "";

	em_folder_utils_copy_folders (
		fromstore, cfd->fi->full_name, tostore, tobase, cfd->delete);

	camel_url_free (url);
fail:
	if (fromstore)
		g_object_unref (fromstore);
	if (tostore)
		g_object_unref (tostore);

	g_clear_error (&local_error);

	g_free (cfd);
}

/* tree here is the 'destination' selector, not 'self' */
static gboolean
emfu_copy_folder_exclude (EMFolderTree *tree, GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	struct _copy_folder_data *cfd = data;
	gint fromvfolder, tovfolder;
	gchar *touri;
	guint flags;
	gboolean is_store;

	/* handles moving to/from vfolders */

	fromvfolder = strncmp(cfd->fi->uri, "vfolder:", 8) == 0;
	gtk_tree_model_get (model, iter, COL_STRING_URI, &touri, COL_UINT_FLAGS, &flags, COL_BOOL_IS_STORE, &is_store, -1);
	tovfolder = strncmp(touri, "vfolder:", 8) == 0;
	g_free (touri);

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

/* FIXME: this interface references the folderinfo without copying it  */
/* FIXME: these functions must be documented */
void
em_folder_utils_copy_folder (GtkWindow *parent,
                             EMailBackend *backend,
                             CamelFolderInfo *folderinfo,
                             gint delete)
{
	GtkWidget *dialog;
	EMFolderTree *emft;
	EMailSession *session;
	const gchar *label;
	const gchar *title;
	struct _copy_folder_data *cfd;

	g_return_if_fail (E_IS_MAIL_BACKEND (backend));
	g_return_if_fail (folderinfo != NULL);

	session = e_mail_backend_get_session (backend);

	cfd = g_malloc (sizeof (*cfd));
	cfd->fi = folderinfo;
	cfd->delete = delete;

	/* XXX Do we leak this reference. */
	emft = (EMFolderTree *) em_folder_tree_new (session);
	emu_restore_folder_tree_state (emft);

	em_folder_tree_set_excluded_func (
		emft, emfu_copy_folder_exclude, cfd);

	label = delete ? _("_Move") : _("C_opy");
	title = delete ? _("Move Folder To") : _("Copy Folder To");

	dialog = em_folder_selector_new (
		parent, emft,
		EM_FOLDER_SELECTOR_CAN_CREATE,
		title, NULL, label);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		const gchar *uri;

		uri = em_folder_selector_get_selected_uri (
			EM_FOLDER_SELECTOR (dialog));
		emfu_copy_folder_selected (backend, uri, cfd);
	}

	gtk_widget_destroy (dialog);
}

typedef struct {
	EMailBackend *backend;
	GtkWidget *dialog;
} DeleteFolderData;

static void
emfu_delete_done (CamelFolder *folder,
                  gboolean removed,
                  GError **error,
                  gpointer user_data)
{
	DeleteFolderData *data = user_data;

	if (error != NULL && *error != NULL) {
		e_mail_backend_submit_alert (
			data->backend,
			"mail:no-delete-folder",
			camel_folder_get_full_name (folder),
			(*error)->message, NULL);
		g_clear_error (error);
	}

	g_object_unref (data->backend);
	gtk_widget_destroy (data->dialog);
	g_slice_free (DeleteFolderData, data);
}

/* FIXME: these functions must be documented */
void
em_folder_utils_delete_folder (EMailBackend *backend,
                               CamelFolder *folder)
{
	CamelStore *local_store;
	CamelStore *parent_store;
	EMailSession *session;
	MailFolderCache *folder_cache;
	GtkWindow *parent = e_shell_get_active_window (NULL);
	GtkWidget *dialog;
	const gchar *full_name;
	gint flags = 0;

	g_return_if_fail (E_IS_MAIL_BACKEND (backend));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	local_store = e_mail_local_get_store ();
	session = e_mail_backend_get_session (backend);
	folder_cache = e_mail_session_get_folder_cache (session);

	if (parent_store == local_store && emfu_is_special_local_folder (full_name)) {
		e_mail_backend_submit_alert (
			backend, "mail:no-delete-special-folder",
			full_name, NULL);
		return;
	}

	if (mail_folder_cache_get_folder_info_flags (folder_cache, folder, &flags) && (flags & CAMEL_FOLDER_SYSTEM)) {
		e_mail_backend_submit_alert (
			backend, "mail:no-delete-special-folder",
			camel_folder_get_name (folder), NULL);
		return;
	}

	g_object_ref (folder);

	if (mail_folder_cache_get_folder_info_flags (folder_cache, folder, &flags) && (flags & CAMEL_FOLDER_CHILDREN)) {
		if (parent_store && CAMEL_IS_VEE_STORE (parent_store))
			dialog = e_alert_dialog_new_for_args (
				parent, "mail:ask-delete-vfolder",
				full_name, NULL);
		else
			dialog = e_alert_dialog_new_for_args (
				parent, "mail:ask-delete-folder",
				full_name, NULL);
	}
	else {
		if (parent_store && CAMEL_IS_VEE_STORE (parent_store))
			dialog = e_alert_dialog_new_for_args (
				parent, "mail:ask-delete-vfolder-nochild",
				full_name, NULL);
		else
			dialog = e_alert_dialog_new_for_args (
				parent, "mail:ask-delete-folder-nochild",
				full_name, NULL);
	}

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		DeleteFolderData *data;

		/* disable dialog until operation finishes */
		gtk_widget_set_sensitive (dialog, FALSE);

		data = g_slice_new0 (DeleteFolderData);
		data->backend = g_object_ref (backend);
		data->dialog = dialog;

		mail_remove_folder (folder, emfu_delete_done, data);
	} else
		gtk_widget_destroy (dialog);
}

struct _EMCreateFolder {
	MailMsg base;

	/* input data */
	CamelStore *store;
	gchar *full_name;
	gchar *parent;
	gchar *name;

	/* output data */
	CamelFolderInfo *fi;

	/* callback data */
	void (* done) (CamelFolderInfo *fi, gpointer user_data);
	gpointer user_data;
};

/* Temporary Structure to hold data to pass across function */
struct _EMCreateFolderTempData
{
	EMFolderTree * emft;
	EMFolderSelector * emfs;
	gchar *uri;
};

static gchar *
emfu_create_folder__desc (struct _EMCreateFolder *m)
{
	return g_strdup_printf (_("Creating folder '%s'"), m->full_name);
}

static void
emfu_create_folder__exec (struct _EMCreateFolder *m,
                          GCancellable *cancellable,
                          GError **error)
{
	if ((m->fi = camel_store_create_folder_sync (
		m->store, m->parent, m->name, cancellable, error))) {

		if (camel_store_supports_subscriptions (m->store))
			camel_store_subscribe_folder_sync (
				m->store, m->full_name,
				cancellable, error);
	}
}

static void
emfu_create_folder__done (struct _EMCreateFolder *m)
{
	if (m->done)
		m->done (m->fi, m->user_data);
}

static void
emfu_create_folder__free (struct _EMCreateFolder *m)
{
	camel_store_free_folder_info (m->store, m->fi);
	g_object_unref (m->store);
	g_free (m->full_name);
	g_free (m->parent);
	g_free (m->name);
}

static MailMsgInfo create_folder_info = {
	sizeof (struct _EMCreateFolder),
	(MailMsgDescFunc) emfu_create_folder__desc,
	(MailMsgExecFunc) emfu_create_folder__exec,
	(MailMsgDoneFunc) emfu_create_folder__done,
	(MailMsgFreeFunc) emfu_create_folder__free
};

static gint
emfu_create_folder_real (CamelStore *store, const gchar *full_name, void (* done) (CamelFolderInfo *fi, gpointer user_data), gpointer user_data)
{
	gchar *name, *namebuf = NULL;
	struct _EMCreateFolder *m;
	const gchar *parent;
	gint id;

	namebuf = g_strdup (full_name);
	if (!(name = strrchr (namebuf, '/'))) {
		name = namebuf;
		parent = "";
	} else {
		*name++ = '\0';
		parent = namebuf;
	}

	m = mail_msg_new (&create_folder_info);
	g_object_ref (store);
	m->store = store;
	m->full_name = g_strdup (full_name);
	m->parent = g_strdup (parent);
	m->name = g_strdup (name);
	m->user_data = user_data;
	m->done = done;

	g_free (namebuf);

	id = m->base.seq;
	mail_msg_unordered_push (m);

	return id;
}

static void
new_folder_created_cb (CamelFolderInfo *fi, gpointer user_data)
{
	struct _EMCreateFolderTempData *emcftd=user_data;
	if (fi) {
		/* Exapnding newly created folder */
		if (emcftd->emft)
			em_folder_tree_set_selected ((EMFolderTree *) emcftd->emft, emcftd->uri, GPOINTER_TO_INT(g_object_get_data ((GObject *)emcftd->emft, "select")) ? FALSE : TRUE);

		gtk_widget_destroy ((GtkWidget *) emcftd->emfs);
	}
	g_object_unref (emcftd->emfs);
	g_free (emcftd->uri);
	g_free (emcftd);
}

static void
emfu_popup_new_folder_response (EMFolderSelector *emfs,
                                gint response,
                                EMFolderTree *folder_tree)
{
	EMFolderTreeModelStoreInfo *si;
	EMailSession *session;
	GtkTreeModel *model;
	const gchar *uri, *path;
	CamelStore *store;
	struct _EMCreateFolderTempData  *emcftd;

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy ((GtkWidget *) emfs);
		return;
	}

	uri = em_folder_selector_get_selected_uri (emfs);
	path = em_folder_selector_get_selected_path (emfs);

	d(printf ("Creating new folder: %s (%s)\n", path, uri));

	g_print ("DEBUG: %s (%s)\n", path, uri);

	session = em_folder_tree_get_session (folder_tree);

	store = (CamelStore *) camel_session_get_service (
		CAMEL_SESSION (session), uri,
		CAMEL_PROVIDER_STORE, NULL);
	if (store == NULL)
		return;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (emfs->emft));
	si = em_folder_tree_model_lookup_store_info (
		EM_FOLDER_TREE_MODEL (model), store);
	if (si == NULL) {
		g_object_unref (store);
		g_return_if_reached ();
	}

	/* HACK: we need to create vfolders using the vfolder editor */
	if (CAMEL_IS_VEE_STORE (store)) {
		EFilterRule *rule;

		rule = em_vfolder_rule_new (session);
		e_filter_rule_set_name (rule, path);
		vfolder_gui_add_rule (EM_VFOLDER_RULE (rule));
		gtk_widget_destroy ((GtkWidget *)emfs);
	} else {
		/* Temp data to pass to create_folder_real function */
		emcftd = (struct _EMCreateFolderTempData *) g_malloc (sizeof (struct _EMCreateFolderTempData));
		emcftd->emfs = emfs;
		emcftd->uri = g_strdup (uri);
		emcftd->emft = folder_tree;

		g_object_ref (emfs);
		emfu_create_folder_real (si->store, path, new_folder_created_cb, emcftd);
	}

	g_object_unref (store);
}

/* FIXME: these functions must be documented */
void
em_folder_utils_create_folder (CamelFolderInfo *folderinfo,
                               EMFolderTree *emft,
			       EMailSession *session,
                               GtkWindow *parent)
{
	EMFolderTree *folder_tree;
	GtkWidget *dialog;

	folder_tree = (EMFolderTree *) em_folder_tree_new (session);
	emu_restore_folder_tree_state (folder_tree);

	dialog = em_folder_selector_create_new (
		parent, folder_tree, 0,
		_("Create Folder"),
		_("Specify where to create the folder:"));
	if (folderinfo != NULL)
		em_folder_selector_set_selected ((EMFolderSelector *) dialog, folderinfo->uri);
	g_signal_connect (dialog, "response", G_CALLBACK (emfu_popup_new_folder_response), emft ? emft : folder_tree);

	if (!parent || !GTK_IS_DIALOG (parent))
		gtk_widget_show (dialog);
	else
		gtk_dialog_run (GTK_DIALOG (dialog));
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
