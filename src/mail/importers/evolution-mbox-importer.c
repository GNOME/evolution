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
 *		Iain Holmes <iain@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "shell/e-shell.h"
#include "shell/e-shell-window.h"
#include "shell/e-shell-view.h"
#include "shell/e-shell-sidebar.h"

#include "mail/e-mail-backend.h"
#include "mail/em-folder-selection-button.h"
#include "mail/em-folder-tree-model.h"
#include "mail/em-folder-tree.h"

#include "mail-importer.h"

typedef struct {
	EImport *import;
	EImportTarget *target;

	GMutex status_lock;
	gchar *status_what;
	gint status_pc;
	gint status_timeout_id;
	GCancellable *cancellable;	/* cancel/status port */

	gchar *uri;
} MboxImporter;

static void
folder_selected (EMFolderSelectionButton *button,
                 EImportTargetURI *target)
{
	g_free (target->uri_dest);
	target->uri_dest = g_strdup (em_folder_selection_button_get_folder_uri (button));
}

static GtkWidget *
mbox_getwidget (EImport *ei,
                EImportTarget *target,
                EImportImporter *im)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;
	GtkWindow *window;
	GtkWidget *hbox, *w;
	GtkLabel *label;
	gchar *select_uri = NULL;

	/* XXX Dig up the mail backend from the default EShell.
	 *     Since the EImport framework doesn't allow for user
	 *     data, I don't see how else to get to it. */
	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	/* preselect the folder selected in a mail view */
	window = e_shell_get_active_window (shell);
	if (E_IS_SHELL_WINDOW (window)) {
		EShellWindow *shell_window;
		const gchar *view;

		shell_window = E_SHELL_WINDOW (window);
		view = e_shell_window_get_active_view (shell_window);

		if (view && g_str_equal (view, "mail")) {
			EShellView *shell_view;
			EShellSidebar *shell_sidebar;
			EMFolderTree *folder_tree = NULL;

			shell_view = e_shell_window_get_shell_view (
				shell_window, view);

			shell_sidebar =
				e_shell_view_get_shell_sidebar (shell_view);

			g_object_get (
				shell_sidebar, "folder-tree",
				&folder_tree, NULL);

			select_uri =
				em_folder_tree_get_selected_uri (folder_tree);

			g_object_unref (folder_tree);
		}
	}

	if (!select_uri) {
		const gchar *uri;
		uri = e_mail_session_get_local_folder_uri (
			session, E_MAIL_LOCAL_FOLDER_INBOX);
		select_uri = g_strdup (uri);
	}

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	w = gtk_label_new_with_mnemonic (_("_Destination folder:"));
	gtk_box_pack_start ((GtkBox *) hbox, w, FALSE, TRUE, 6);

	label = GTK_LABEL (w);

	w = em_folder_selection_button_new (
		session, _("Select folder"),
		_("Select folder to import into"));
	gtk_label_set_mnemonic_widget (label, w);
	em_folder_selection_button_set_folder_uri (
		EM_FOLDER_SELECTION_BUTTON (w), select_uri);
	folder_selected (
		EM_FOLDER_SELECTION_BUTTON (w), (EImportTargetURI *) target);
	g_signal_connect (
		w, "selected",
		G_CALLBACK (folder_selected), target);
	gtk_box_pack_start ((GtkBox *) hbox, w, FALSE, TRUE, 6);

	w = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start ((GtkBox *) w, hbox, FALSE, FALSE, 0);
	gtk_widget_show_all (w);

	g_free (select_uri);

	return w;
}

static gboolean
mbox_supported (EImport *ei,
                EImportTarget *target,
                EImportImporter *im)
{
	gchar signature[1024];
	gboolean ret = FALSE;
	gint fd, n;
	EImportTargetURI *s;
	gchar *filename;

	if (target->type != E_IMPORT_TARGET_URI)
		return FALSE;

	s = (EImportTargetURI *) target;
	if (s->uri_src == NULL)
		return TRUE;

	if (strncmp (s->uri_src, "file:///", strlen ("file:///")) != 0)
		return FALSE;

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	fd = g_open (filename, O_RDONLY, 0);
	if (fd != -1) {
		n = read (fd, signature, 1024);
		ret = n >= 5 && memcmp (signature, "From ", 5) == 0;
		close (fd);

		/* An artificial number, at least 256 bytes message
		   to be able to try to import it as an MBOX */
		if (!ret && n >= 256) {
			gint ii;

			ret = (signature[0] >= 'a' && signature[0] <= 'z') ||
			      (signature[0] >= 'A' && signature[0] <= 'Z');

			for (ii = 0; ii < n && ret; ii++) {
				ret = signature[ii] == '-' ||
				      signature[ii] == ' ' ||
				      signature[ii] == '\t' ||
				     (signature[ii] >= 'a' && signature[ii] <= 'z') ||
				     (signature[ii] >= 'A' && signature[ii] <= 'Z') ||
				     (signature[ii] >= '0' && signature[ii] <= '9');
			}

			/* It's probably a header name which starts with ASCII letter and
			   contains only [a..z][A..Z][\t, ,-] and the read stopped on ':'. */
			if (ii > 0 && ii < n && !ret && signature[ii - 1] == ':') {
				CamelStream *stream;

				stream = camel_stream_fs_new_with_name (filename, O_RDONLY, 0, NULL);
				if (stream) {
					CamelMimeMessage *msg;

					msg = camel_mime_message_new ();

					/* Check whether the message can be parsed and whether
					   it contains any mandatory fields. */
					ret = camel_data_wrapper_construct_from_stream_sync ((CamelDataWrapper *) msg, stream, NULL, NULL) &&
					      camel_mime_message_get_message_id (msg) &&
					      camel_mime_message_get_subject (msg) &&
					      camel_mime_message_get_from (msg) &&
					      (camel_mime_message_get_recipients (msg, CAMEL_RECIPIENT_TYPE_TO) ||
					       camel_mime_message_get_recipients (msg, CAMEL_RECIPIENT_TYPE_RESENT_TO));

					g_object_unref (msg);
					g_object_unref (stream);
				}
			}
		}
	}

	g_free (filename);

	return ret;
}

static void
mbox_status (CamelOperation *op,
             const gchar *what,
             gint pc,
             gpointer data)
{
	MboxImporter *importer = data;

	g_mutex_lock (&importer->status_lock);
	g_free (importer->status_what);
	importer->status_what = g_strdup (what);
	importer->status_pc = pc;
	g_mutex_unlock (&importer->status_lock);
}

static gboolean
mbox_status_timeout (gpointer data)
{
	MboxImporter *importer = data;
	gint pc;
	gchar *what;

	if (importer->status_what) {
		g_mutex_lock (&importer->status_lock);
		what = importer->status_what;
		importer->status_what = NULL;
		pc = importer->status_pc;
		g_mutex_unlock (&importer->status_lock);

		e_import_status (
			importer->import, (EImportTarget *)
			importer->target, what, pc);
	}

	return TRUE;
}

static void
mbox_import_done (gpointer data,
                  GError **error)
{
	MboxImporter *importer = data;

	g_source_remove (importer->status_timeout_id);
	g_free (importer->status_what);
	g_mutex_clear (&importer->status_lock);
	g_object_unref (importer->cancellable);

	e_import_complete (importer->import, importer->target, error ? *error : NULL);
	g_free (importer);
}

static void
mbox_import (EImport *ei,
             EImportTarget *target,
             EImportImporter *im)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailSession *session;
	MboxImporter *importer;
	gchar *filename;

	/* XXX Dig up the EMailSession from the default EShell.
	 *     Since the EImport framework doesn't allow for user
	 *     data, I don't see how else to get to it. */
	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));

	/* TODO: do we validate target? */

	importer = g_malloc0 (sizeof (*importer));
	g_datalist_set_data (&target->data, "mbox-data", importer);
	importer->import = ei;
	importer->target = target;
	g_mutex_init (&importer->status_lock);
	importer->status_timeout_id =
		e_named_timeout_add (100, mbox_status_timeout, importer);
	importer->cancellable = camel_operation_new ();

	g_signal_connect (
		importer->cancellable, "status",
		G_CALLBACK (mbox_status), importer);

	filename = g_filename_from_uri (
		((EImportTargetURI *) target)->uri_src, NULL, NULL);
	mail_importer_import_mbox (
		session, filename, ((EImportTargetURI *) target)->uri_dest,
		importer->cancellable, mbox_import_done, importer);
	g_free (filename);
}

static void
mbox_cancel (EImport *ei,
             EImportTarget *target,
             EImportImporter *im)
{
	MboxImporter *importer = g_datalist_get_data (&target->data, "mbox-data");

	if (importer)
		g_cancellable_cancel (importer->cancellable);
}

static MboxImporterCreatePreviewFunc create_preview_func = NULL;
static MboxImporterFillPreviewFunc fill_preview_func = NULL;

void
mbox_importer_set_preview_funcs (MboxImporterCreatePreviewFunc create_func,
                                 MboxImporterFillPreviewFunc fill_func)
{
	create_preview_func = create_func;
	fill_preview_func = fill_func;
}

static void
preview_selection_changed_cb (GtkTreeSelection *selection,
                              EWebViewPreview *preview)
{
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	gboolean found = FALSE;

	g_return_if_fail (selection != NULL);
	g_return_if_fail (preview != NULL);
	g_return_if_fail (fill_preview_func != NULL);

	if (gtk_tree_selection_get_selected (selection, &model, &iter) && model) {
		CamelMimeMessage *msg = NULL;

		gtk_tree_model_get (model, &iter, 2, &msg, -1);

		if (msg) {
			found = TRUE;
			fill_preview_func (G_OBJECT (preview), msg);
			g_object_unref (msg);
		}
	}

	if (!found) {
		e_web_view_preview_begin_update (preview);
		e_web_view_preview_end_update (preview);
	}
}

static void
mbox_preview_add_message (CamelMimeMessage *msg,
			  GtkListStore **pstore)
{
	GtkTreeIter iter;
	gchar *from;

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (msg));
	g_return_if_fail (pstore != NULL);

	if (!*pstore)
		*pstore = gtk_list_store_new (
			3, G_TYPE_STRING, G_TYPE_STRING,
			CAMEL_TYPE_MIME_MESSAGE);

	from = NULL;

	if (camel_mime_message_get_from (msg))
		from = camel_address_encode (CAMEL_ADDRESS (camel_mime_message_get_from (msg)));

	gtk_list_store_append (*pstore, &iter);
	gtk_list_store_set (
		*pstore, &iter,
		0, camel_mime_message_get_subject (msg) ?
		camel_mime_message_get_subject (msg) : "",
		1, from ? from : "", 2, msg, -1);

	g_free (from);
}

static GtkWidget *
mbox_get_preview (EImport *ei,
                  EImportTarget *target,
                  EImportImporter *im)
{
	GtkWidget *preview = NULL;
	EImportTargetURI *s = (EImportTargetURI *) target;
	gchar *filename;
	gint fd;
	GtkListStore *store = NULL;
	GtkTreeIter iter;
	GtkWidget *preview_widget = NULL;
	gint n_read = 0;
	gboolean shortened_list = FALSE;

	if (!create_preview_func || !fill_preview_func)
		return NULL;

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	if (!filename) {
		g_message (G_STRLOC ": Couldn't get filename from URI '%s'", s->uri_src);
		return NULL;
	}

	fd = g_open (filename, O_RDONLY | O_BINARY, 0);
	if (fd == -1) {
		g_warning (
			"Cannot find source file to import '%s': %s",
			filename, g_strerror (errno));
		g_free (filename);
		return NULL;
	}

	if (mail_importer_file_is_mbox (filename)) {
		CamelMimeParser *mp;

		mp = camel_mime_parser_new ();
		camel_mime_parser_scan_from (mp, TRUE);
		if (camel_mime_parser_init_with_fd (mp, fd) == -1) {
			g_clear_object (&mp);
			goto cleanup;
		}

		while (camel_mime_parser_step (mp, NULL, NULL) == CAMEL_MIME_PARSER_STATE_FROM) {
			CamelMimeMessage *msg;

			n_read++;

			/* Read only first few messages, not the whole mbox */
			if (n_read > 10) {
				shortened_list = TRUE;
				break;
			}

			msg = camel_mime_message_new ();
			if (!camel_mime_part_construct_from_parser_sync (
				(CamelMimePart *) msg, mp, NULL, NULL)) {
				g_object_unref (msg);
				break;
			}

			mbox_preview_add_message (msg, &store);

			g_object_unref (msg);

			camel_mime_parser_step (mp, NULL, NULL);
		}

		g_clear_object (&mp);
	} else {
		close (fd);
	}

	if (!n_read) {
		CamelStream *stream;

		stream = camel_stream_fs_new_with_name (filename, O_RDONLY, 0, NULL);
		if (stream) {
			CamelMimeMessage *msg;

			msg = camel_mime_message_new ();

			if (camel_data_wrapper_construct_from_stream_sync ((CamelDataWrapper *) msg, stream, NULL, NULL))
				mbox_preview_add_message (msg, &store);

			g_object_unref (msg);
			g_object_unref (stream);
		}
	}

	if (store) {
		GtkTreeView *tree_view;
		GtkTreeSelection *selection;

		preview = e_web_view_preview_new ();
		gtk_widget_show (preview);

		tree_view = e_web_view_preview_get_tree_view (
			E_WEB_VIEW_PREVIEW (preview));
		if (!tree_view) {
			g_warn_if_reached ();
			gtk_widget_destroy (preview);
			preview = NULL;
			goto cleanup;
		}

		gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (store));
		g_object_unref (store);

		gtk_tree_view_insert_column_with_attributes (
			/* Translators: Column header for a message subject */
			tree_view, -1, C_("mboxImp", "Subject"),
			gtk_cell_renderer_text_new (), "text", 0, NULL);

		gtk_tree_view_insert_column_with_attributes (
			/* Translators: Column header for a message From address */
			tree_view, -1, C_("mboxImp", "From"),
			gtk_cell_renderer_text_new (), "text", 1, NULL);

		if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) > 1) {
			e_web_view_preview_show_tree_view (E_WEB_VIEW_PREVIEW (preview));

			if (shortened_list) {
				GtkTreeIter titer;

				gtk_list_store_append (store, &titer);
				gtk_list_store_set (store, &titer,
					0, _("Showing only first few messages, more will be imported"),
					1, "",
					2, NULL,
					-1);
			}
		}

		create_preview_func (G_OBJECT (preview), &preview_widget);
		if (!preview_widget) {
			g_warn_if_reached ();
			goto cleanup;
		}

		e_web_view_preview_set_preview (
			E_WEB_VIEW_PREVIEW (preview), preview_widget);
		gtk_widget_show (preview_widget);

		selection = gtk_tree_view_get_selection (tree_view);
		if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter)) {
			g_warn_if_reached ();
			goto cleanup;
		}
		gtk_tree_selection_select_iter (selection, &iter);

		g_signal_connect (
			selection, "changed",
			G_CALLBACK (preview_selection_changed_cb), preview);

		preview_selection_changed_cb (
			selection, E_WEB_VIEW_PREVIEW (preview));
	}

 cleanup:
	g_free (filename);

	/* 'fd' is freed together with 'mp' */
	/* coverity[leaked_handle] */
	return preview;
}

static EImportImporter mbox_importer = {
	E_IMPORT_TARGET_URI,
	0,
	mbox_supported,
	mbox_getwidget,
	mbox_import,
	mbox_cancel,
	mbox_get_preview,
};

EImportImporter *
mbox_importer_peek (void)
{
	mbox_importer.name = _("Berkeley Mailbox (mbox)");
	mbox_importer.description = _("Importer Berkeley Mailbox format folders");

	return &mbox_importer;
}
