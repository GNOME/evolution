/*
 * Copyright (C) 2015 SUSE (www.suse.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 * Authors:
 *           David Liang <dliang@suse.com>
 *
 */

#include "evolution-config.h"

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "kmail-libs.h"
#include "mail-importer.h"

#include "libemail-engine/libemail-engine.h"
#include "e-util/e-util.h"
#include "shell/e-shell.h"
#include "shell/e-shell-window.h"
#include "shell/e-shell-view.h"
#include "shell/e-shell-sidebar.h"

#include "mail/e-mail-backend.h"
#include "mail/em-folder-selection-button.h"
#include "mail/em-folder-tree-model.h"
#include "mail/em-folder-tree.h"

#define ENABLE_SELECT 0
#define d(x)

typedef struct {
	EImport *import;
	EImportTarget *target;

	GMutex status_lock;
	gchar *status_what;
	gint status_pc;
	gint status_timeout_id;
	GCancellable *cancellable;      /* cancel/status port */

	gchar *uri;
} KMailImporter;

static gboolean
kmail_supported (EImport *ei,
                 EImportTarget *target,
                 EImportImporter *im)
{
	return kmail_is_supported ();
}

static void
kmail_import_done (gpointer data,
                   GError **error)
{
	KMailImporter *importer = data;

	g_source_remove (importer->status_timeout_id);
	g_free (importer->status_what);

	g_mutex_clear (&importer->status_lock);
	g_object_unref (importer->cancellable);

	e_import_complete (importer->import, importer->target, error ? *error : NULL);
	g_free (importer);
}

static void
kmail_status (CamelOperation *op,
              const gchar *what,
              gint pc,
              gpointer data)
{
	KMailImporter *importer = data;
	g_mutex_lock (&importer->status_lock);
	g_free (importer->status_what);
	importer->status_what = g_strdup (what);
	importer->status_pc = pc;
	g_mutex_unlock (&importer->status_lock);
}

static gboolean
kmail_status_timeout (gpointer data)
{
	KMailImporter *importer = data;
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
checkbox_toggle_cb (GtkToggleButton *tb,
                    EImportTarget *target)
{
	g_datalist_set_data (
		&target->data, "kmail-do-mail",
		GINT_TO_POINTER (gtk_toggle_button_get_active (tb)));
}

#if ENABLE_SELECT
static void
folder_selected (EMFolderSelectionButton *button,
                 EImportTargetURI *target)
{
	g_free (target->uri_dest);
	target->uri_dest = g_strdup (em_folder_selection_button_get_folder_uri (button));
}

static GtkWidget *
import_folder_getwidget (EImport *ei,
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

	hbox = gtk_hbox_new (FALSE, 0);

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

	w = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start ((GtkBox *) w, hbox, FALSE, FALSE, 0);
	gtk_widget_show_all (w);

	g_free (select_uri);

	return w;
}
#endif

static GtkWidget *
kmail_getwidget (EImport *ei,
                 EImportTarget *target,
                 EImportImporter *im)
{
	GtkWidget *box, *w;
	GSList *contact_list;
	gint count;
	gchar *contact_str;


	g_datalist_set_data (
		&target->data, "kmail-do-mail", GINT_TO_POINTER (TRUE));

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	w = gtk_check_button_new_with_label (_("Mail"));
	gtk_toggle_button_set_active ((GtkToggleButton *) w, TRUE);
	g_signal_connect (
		w, "toggled",
		G_CALLBACK (checkbox_toggle_cb), target);

	gtk_box_pack_start ((GtkBox *) box, w, FALSE, FALSE, 0);

	contact_list = kcontact_get_list ();
	count = g_slist_length (contact_list);
	contact_str = g_strdup_printf (ngettext ("%d Address", "%d Addresses", count), count);
	w = gtk_check_button_new_with_label (contact_str);
	gtk_toggle_button_set_active ((GtkToggleButton *) w, TRUE);
	g_signal_connect (
		w, "toggled",
		G_CALLBACK (checkbox_toggle_cb), target);

	gtk_box_pack_start ((GtkBox *) box, w, FALSE, FALSE, 0);

	/* for now, we don't allow to select a folder */
	#if ENABLE_SELECT
	w = import_folder_getwidget (ei, target, im);
	gtk_box_pack_start ((GtkBox *) box, w, FALSE, FALSE, 0);
	#endif

	gtk_widget_show_all (box);
	g_slist_free_full (contact_list, g_free);
	g_free (contact_str);

	return box;
}

static void
kmail_import (EImport *ei,
              EImportTarget *target,
              EImportImporter *im)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailSession *session;
	KMailImporter *importer;
	gchar *path;
	GSList *contact_list;

	/* XXX Dig up the EMailSession from the default EShell.
	 *     Since the EImport framework doesn't allow for user
	 *     data, I don't see how else to get to it. */
	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));

	importer = g_malloc0 (sizeof (*importer));
	g_datalist_set_data (&target->data, "kmail-data", importer);
	importer->status_what = NULL;
	importer->import = ei;
	importer->target = target;
	importer->cancellable = camel_operation_new ();
	g_mutex_init (&importer->status_lock);
	importer->status_timeout_id = g_timeout_add (100, kmail_status_timeout, importer);

	g_signal_connect (
		importer->cancellable, "status",
		G_CALLBACK (kmail_status), importer);

	/* import emails */
	path = kmail_get_base_dir ();
	mail_importer_import_kmail (
		session, path, NULL,
		importer->cancellable, kmail_import_done, importer);
	g_free (path);

	/* import contacts */
	contact_list = kcontact_get_list ();
	kcontact_load (contact_list);
	g_slist_free_full (contact_list, g_free);
}

static void
kmail_cancel (EImport *ei,
              EImportTarget *target,
              EImportImporter *im)
{
	KMailImporter *m = g_datalist_get_data (&target->data, "kmail-data");

	if (m)
		g_cancellable_cancel (m->cancellable);
}

static EImportImporter kmail_importer = {
	E_IMPORT_TARGET_HOME,
	0,
	kmail_supported,
	kmail_getwidget,
	kmail_import,
	kmail_cancel,
	NULL, /* get_preview */
};

EImportImporter *
kmail_importer_peek (void)
{
	kmail_importer.name = _("Evolution KMail importer");
	kmail_importer.description = _("Import mail and contacts from KMail.");

	return &kmail_importer;
}
