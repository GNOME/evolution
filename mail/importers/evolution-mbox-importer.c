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
 *		Iain Holmes <iain@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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

#include "mail/e-mail-local.h"
#include "mail/e-mail-store.h"
#include "mail/em-folder-selection-button.h"
#include "mail/em-folder-tree-model.h"
#include "mail/em-folder-tree.h"
#include "mail/mail-mt.h"

#include "mail-importer.h"

#include "e-util/e-import.h"
#include "misc/e-web-view-preview.h"

typedef struct {
	EImport *import;
	EImportTarget *target;

	GMutex *status_lock;
	gchar *status_what;
	gint status_pc;
	gint status_timeout_id;
	CamelOperation *cancel;	/* cancel/status port */

	gchar *uri;
} MboxImporter;

static void
folder_selected(EMFolderSelectionButton *button, EImportTargetURI *target)
{
	g_free(target->uri_dest);
	target->uri_dest = g_strdup(em_folder_selection_button_get_selection(button));
}

static GtkWidget *
mbox_getwidget(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	GtkWindow *window;
	GtkWidget *hbox, *w;
	GtkLabel *label;
	gchar *select_uri = NULL;

	/* preselect the folder selected in a mail view */
	window = e_shell_get_active_window (e_shell_get_default ());
	if (E_IS_SHELL_WINDOW (window)) {
		EShellWindow *shell_window;
		const gchar *view;

		shell_window = E_SHELL_WINDOW (window);
		view = e_shell_window_get_active_view (shell_window);

		if (view && g_str_equal (view, "mail")) {
			EShellView *shell_view = e_shell_window_get_shell_view (shell_window, view);

			if (shell_view) {
				EMFolderTree *folder_tree = NULL;
				EShellSidebar *shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

				g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);

				if (folder_tree)
					select_uri = em_folder_tree_get_selected_uri (folder_tree);
			}
		}
	}

	if (!select_uri)
		select_uri = g_strdup (e_mail_local_get_folder_uri (E_MAIL_FOLDER_INBOX));

	hbox = gtk_hbox_new(FALSE, 0);

	w = gtk_label_new_with_mnemonic (_("_Destination folder:"));
	gtk_box_pack_start((GtkBox *)hbox, w, FALSE, TRUE, 6);

	label = GTK_LABEL (w);

	w = em_folder_selection_button_new(
		_("Select folder"), _("Select folder to import into"));
	gtk_label_set_mnemonic_widget (label, w);
	em_folder_selection_button_set_selection ((EMFolderSelectionButton *)w, select_uri);
	folder_selected (EM_FOLDER_SELECTION_BUTTON (w), (EImportTargetURI *)target);
	g_signal_connect (w, "selected", G_CALLBACK(folder_selected), target);
	gtk_box_pack_start((GtkBox *)hbox, w, FALSE, TRUE, 6);

	w = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start((GtkBox *)w, hbox, FALSE, FALSE, 0);
	gtk_widget_show_all(w);

	g_free (select_uri);

	return w;
}

static gboolean
mbox_supported(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	gchar signature[6];
	gboolean ret = FALSE;
	gint fd, n;
	EImportTargetURI *s;
	gchar *filename;

	if (target->type != E_IMPORT_TARGET_URI)
		return FALSE;

	s = (EImportTargetURI *)target;
	if (s->uri_src == NULL)
		return TRUE;

	if (strncmp(s->uri_src, "file:///", strlen("file:///")) != 0)
		return FALSE;

	filename = g_filename_from_uri(s->uri_src, NULL, NULL);
	fd = g_open(filename, O_RDONLY, 0);
	g_free(filename);
	if (fd != -1) {
		n = read(fd, signature, 5);
		ret = n == 5 && memcmp(signature, "From ", 5) == 0;
		close(fd);
	}

	return ret;
}

static void
mbox_status(CamelOperation *op, const gchar *what, gint pc, gpointer data)
{
	MboxImporter *importer = data;

	if (pc == CAMEL_OPERATION_START)
		pc = 0;
	else if (pc == CAMEL_OPERATION_END)
		pc = 100;

	g_mutex_lock(importer->status_lock);
	g_free(importer->status_what);
	importer->status_what = g_strdup(what);
	importer->status_pc = pc;
	g_mutex_unlock(importer->status_lock);
}

static gboolean
mbox_status_timeout(gpointer data)
{
	MboxImporter *importer = data;
	gint pc;
	gchar *what;

	if (importer->status_what) {
		g_mutex_lock(importer->status_lock);
		what = importer->status_what;
		importer->status_what = NULL;
		pc = importer->status_pc;
		g_mutex_unlock(importer->status_lock);

		e_import_status(importer->import, (EImportTarget *)importer->target, what, pc);
	}

	return TRUE;
}

static void
mbox_import_done(gpointer data, GError **error)
{
	MboxImporter *importer = data;

	g_source_remove(importer->status_timeout_id);
	g_free(importer->status_what);
	g_mutex_free(importer->status_lock);
	camel_operation_unref(importer->cancel);

	e_import_complete(importer->import, importer->target);
	g_free(importer);
}

static void
mbox_import(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	MboxImporter *importer;
	gchar *filename;

	/* TODO: do we validate target? */

	importer = g_malloc0(sizeof(*importer));
	g_datalist_set_data(&target->data, "mbox-data", importer);
	importer->import = ei;
	importer->target = target;
	importer->status_lock = g_mutex_new();
	importer->status_timeout_id = g_timeout_add(100, mbox_status_timeout, importer);
	importer->cancel = camel_operation_new(mbox_status, importer);

	filename = g_filename_from_uri(((EImportTargetURI *)target)->uri_src, NULL, NULL);
	mail_importer_import_mbox (
		filename, ((EImportTargetURI *)target)->uri_dest,
		importer->cancel, mbox_import_done, importer);
	g_free(filename);
}

static void
mbox_cancel(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	MboxImporter *importer = g_datalist_get_data(&target->data, "mbox-data");

	if (importer)
		camel_operation_cancel(importer->cancel);
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
preview_selection_changed_cb (GtkTreeSelection *selection, EWebViewPreview *preview)
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

static GtkWidget *
mbox_get_preview (EImport *ei, EImportTarget *target, EImportImporter *im)
{
	GtkWidget *preview = NULL;
	EImportTargetURI *s = (EImportTargetURI *)target;
	gchar *filename;
	gint fd;
	CamelMimeParser *mp;
	GtkListStore *store = NULL;
	GtkTreeIter iter;
	GtkWidget *preview_widget = NULL;

	if (!create_preview_func || !fill_preview_func)
		return NULL;

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	if (!filename) {
		g_message (G_STRLOC ": Couldn't get filename from URI '%s'", s->uri_src);
		return NULL;
	}

	fd = g_open (filename, O_RDONLY|O_BINARY, 0);
	if (fd == -1) {
		g_warning (
			"Cannot find source file to import '%s': %s",
			filename, g_strerror (errno));
		g_free (filename);
		return NULL;
	}

	g_free (filename);

	mp = camel_mime_parser_new();
	camel_mime_parser_scan_from (mp, TRUE);
	if (camel_mime_parser_init_with_fd (mp, fd) == -1) {
		g_object_unref (mp);
		return NULL;
	}

	while (camel_mime_parser_step (mp, NULL, NULL) == CAMEL_MIME_PARSER_STATE_FROM) {
		CamelMimeMessage *msg;
		gchar *from;

		msg = camel_mime_message_new();
		if (camel_mime_part_construct_from_parser (
			(CamelMimePart *)msg, mp, NULL) == -1) {
			g_object_unref (msg);
			break;
		}

		if (!store)
			store = gtk_list_store_new (
				3, G_TYPE_STRING, G_TYPE_STRING,
				CAMEL_TYPE_MIME_MESSAGE);

		from = NULL;
		if (camel_mime_message_get_from (msg))
			from = camel_address_encode (
				CAMEL_ADDRESS (
				camel_mime_message_get_from (msg)));

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
			0, camel_mime_message_get_subject (msg) ?
			camel_mime_message_get_subject (msg) : "",
			1, from ? from : "", 2, msg, -1);

		g_object_unref (msg);
		g_free (from);

		camel_mime_parser_step (mp, NULL, NULL);
	}

	if (store) {
		GtkTreeView *tree_view;
		GtkTreeSelection *selection;
		gboolean valid;

		preview = e_web_view_preview_new ();
		gtk_widget_show (preview);

		tree_view = e_web_view_preview_get_tree_view (
			E_WEB_VIEW_PREVIEW (preview));
		g_return_val_if_fail (tree_view != NULL, NULL);

		gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (store));
		g_object_unref (store);

		/* Translators: Column header for a message subject */
		gtk_tree_view_insert_column_with_attributes (
			tree_view, -1, C_("mboxImp", "Subject"),
			gtk_cell_renderer_text_new (), "text", 0, NULL);

		/* Translators: Column header for a message From address */
		gtk_tree_view_insert_column_with_attributes (
			tree_view, -1, C_("mboxImp", "From"),
			gtk_cell_renderer_text_new (), "text", 1, NULL);

		if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) > 1)
			e_web_view_preview_show_tree_view (
				E_WEB_VIEW_PREVIEW (preview));

		create_preview_func (G_OBJECT (preview), &preview_widget);
		g_return_val_if_fail (preview_widget != NULL, NULL);

		e_web_view_preview_set_preview (
			E_WEB_VIEW_PREVIEW (preview), preview_widget);
		gtk_widget_show (preview_widget);

		selection = gtk_tree_view_get_selection (tree_view);
		valid = gtk_tree_model_get_iter_first (
			GTK_TREE_MODEL (store), &iter);
		g_return_val_if_fail (valid, NULL);
		gtk_tree_selection_select_iter (selection, &iter);

		g_signal_connect (
			selection, "changed",
			G_CALLBACK (preview_selection_changed_cb), preview);

		preview_selection_changed_cb (
			selection, E_WEB_VIEW_PREVIEW (preview));
	}

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
mbox_importer_peek(void)
{
	mbox_importer.name = _("Berkeley Mailbox (mbox)");
	mbox_importer.description = _("Importer Berkeley Mailbox format folders");

	return &mbox_importer;
}
