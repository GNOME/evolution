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
 *		Rodrigo Moya <rodrigo@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* This is prototype code only, this may, or may not, use undocumented
 * unstable or private internal function calls. */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>

#include <shell/e-shell-sidebar.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>

#include "format-handler.h"

/* Plugin entry points */
gboolean	save_calendar_calendar_save_as_init	(EUIManager *ui_manager,
							 EShellView *shell_view);
gboolean	save_calendar_memo_list_save_as_init	(EUIManager *ui_manager,
							 EShellView *shell_view);
gboolean	save_calendar_task_list_save_as_init	(EUIManager *ui_manager,
							 EShellView *shell_view);

gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	return 0;
}

enum {  /* GtkComboBox enum */
	DEST_NAME_COLUMN,
	DEST_HANDLER,
	N_DEST_COLUMNS

};

static void
extra_widget_foreach_hide (GtkWidget *widget,
                           gpointer data)
{
	if (widget != data)
		gtk_widget_hide (widget);
}

static void
on_type_combobox_changed (GtkComboBox *combobox,
                          gpointer data)
{
	FormatHandler *handler = NULL;
	GtkWidget *extra_widget = data;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_combo_box_get_model (combobox);

	gtk_container_foreach (
		GTK_CONTAINER (extra_widget),
		extra_widget_foreach_hide,
		g_object_get_data (G_OBJECT (combobox), "format-box"));

	if (!gtk_combo_box_get_active_iter (combobox, &iter))
		return;

	gtk_tree_model_get (
		model, &iter,
		DEST_HANDLER, &handler, -1);

	if (handler && handler->options_widget) {
		gtk_widget_show (handler->options_widget);
	}

}

static void
format_handlers_foreach_free (gpointer data)
{
	FormatHandler *handler = data;

	if (handler->options_widget)
		gtk_widget_destroy (handler->options_widget);

	g_free (handler->data);
	g_free (data);
}

static void
ask_destination_and_save (ESourceSelector *selector,
			  EClientCache *client_cache)
{
	FormatHandler *handler = NULL;
	GtkWidget *extra_widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	GtkLabel *label = GTK_LABEL (gtk_label_new_with_mnemonic (_("_Format:")));
	GtkComboBox *combo = GTK_COMBO_BOX (gtk_combo_box_new ());
	GtkTreeModel *model = GTK_TREE_MODEL (gtk_list_store_new
		(N_DEST_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER));
	GtkCellRenderer *renderer = NULL;
	GtkListStore *store = GTK_LIST_STORE (model);
	GtkTreeIter iter;
	GtkFileChooserNative *native;
	GtkWidget *toplevel;
	gchar *dest_uri = NULL;
	GList *format_handlers = NULL, *link;

	/* The available formathandlers */
	format_handlers = g_list_append (format_handlers,
		ical_format_handler_new ());
	format_handlers = g_list_append (format_handlers,
		csv_format_handler_new ());
	format_handlers = g_list_append (format_handlers,
		rdf_format_handler_new ());

	gtk_box_pack_start (GTK_BOX (extra_widget), GTK_WIDGET (hbox), FALSE, FALSE, 0);
	gtk_label_set_mnemonic_widget (label, GTK_WIDGET (combo));

	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (label), FALSE, FALSE, 0);

	/* The Type GtkComboBox */
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (combo), TRUE, TRUE, 0);
	gtk_combo_box_set_model (combo, model);

	gtk_list_store_clear (store);
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (
		GTK_CELL_LAYOUT (combo),
		renderer, "text", DEST_NAME_COLUMN, NULL);

	for (link = format_handlers; link; link = g_list_next (link)) {
		handler = link->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter, DEST_NAME_COLUMN,
			handler->combo_label, -1);
		gtk_list_store_set (store, &iter, DEST_HANDLER, handler, -1);

		if (handler->options_widget) {
			gtk_box_pack_start (
				GTK_BOX (extra_widget),
				GTK_WIDGET (handler->options_widget), TRUE, TRUE, 0);
			gtk_widget_hide (handler->options_widget);
		}

		if (handler->isdefault) {
			gtk_combo_box_set_active_iter (combo, &iter);
			if (handler->options_widget)
				gtk_widget_show (handler->options_widget);
		}
	}

	g_signal_connect (
		combo, "changed",
		G_CALLBACK (on_type_combobox_changed), extra_widget);
	g_object_set_data (G_OBJECT (combo), "format-box", hbox);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (selector));

	native = gtk_file_chooser_native_new (
		_("Select destination file"),
		GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		_("_Save As"), _("_Cancel"));

	gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (native), extra_widget);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (native), FALSE);
	gtk_widget_show (hbox);
	gtk_widget_show (GTK_WIDGET (label));
	gtk_widget_show (GTK_WIDGET (combo));
	gtk_widget_show (extra_widget);

	e_util_load_file_chooser_folder (GTK_FILE_CHOOSER (native));

	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (native)) == GTK_RESPONSE_ACCEPT) {
		gchar *tmp = NULL;

		e_util_save_file_chooser_folder (GTK_FILE_CHOOSER (native));

		if (gtk_combo_box_get_active_iter (combo, &iter))
			gtk_tree_model_get (
				model, &iter,
				DEST_HANDLER, &handler, -1);
		else
			handler = NULL;

		dest_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (native));

		if (handler) {
			tmp = strstr (dest_uri, handler->filename_ext);

			if (!(tmp && *(tmp + strlen (handler->filename_ext)) == '\0')) {

				gchar *temp;
				temp = g_strconcat (dest_uri, handler->filename_ext, NULL);
				g_free (dest_uri);
				dest_uri = temp;
			}

			handler->save (handler, selector, client_cache, dest_uri);
		} else {
			g_warn_if_reached ();
		}
	}

	/* Free the handlers */
	g_list_free_full (format_handlers, format_handlers_foreach_free);

	/* Now we can destroy it */
	g_object_unref (native);
	g_free (dest_uri);
}

/* Returns output stream for the uri, or NULL on any error.
 * When done with the stream, just g_output_stream_close and g_object_unref it.
 * It will ask for overwrite if file already exists.
*/
GOutputStream *
open_for_writing (GtkWindow *parent,
                  const gchar *uri,
                  GError **error)
{
	GFile *file;
	GFileOutputStream *fostream;
	GError *err = NULL;

	g_return_val_if_fail (uri != NULL, NULL);

	file = g_file_new_for_uri (uri);

	g_return_val_if_fail (file != NULL, NULL);

	fostream = g_file_create (file, G_FILE_CREATE_NONE, NULL, &err);

	if (err && err->code == G_IO_ERROR_EXISTS) {
		gint response;
		g_clear_error (&err);

		response = e_alert_run_dialog_for_args (
			parent, E_ALERT_ASK_FILE_EXISTS_OVERWRITE,
			uri, NULL);
		if (response == GTK_RESPONSE_OK) {
			fostream = g_file_replace (
				file, NULL, FALSE, G_FILE_CREATE_NONE,
				NULL, &err);

			if (err && fostream) {
				g_object_unref (fostream);
				fostream = NULL;
			}
		} else {
			g_clear_object (&fostream);
		}
	}

	g_object_unref (file);

	if (error && err)
		*error = err;
	else if (err)
		g_error_free (err);

	if (fostream)
		return G_OUTPUT_STREAM (fostream);

	return NULL;
}

static void
save_general (EShellView *shell_view)
{
	EShellSidebar *shell_sidebar;
	EShellBackend *shell_backend;
	EShell *shell;
	ESourceSelector *selector = NULL;

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);
	g_object_get (shell_sidebar, "selector", &selector, NULL);
	g_return_if_fail (selector != NULL);

	ask_destination_and_save (selector, e_shell_get_client_cache (shell));

	g_object_unref (selector);
}

static void
action_calendar_save_as_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EShellView *shell_view = user_data;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	save_general (shell_view);
}

static void
action_memo_list_save_as_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EShellView *shell_view = user_data;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	save_general (shell_view);
}

static void
action_task_list_save_as_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EShellView *shell_view = user_data;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	save_general (shell_view);
}

static void
save_calendar_init_ui (EUIManager *ui_manager,
		       EShellView *shell_view,
		       const gchar *select_one_action_name,
		       const EUIActionEntry *entry,
		       const gchar *eui)
{
	EUIAction *action, *select_one_action;

	e_ui_manager_add_actions_with_eui_data (ui_manager, "lockdown-save-to-disk", NULL, entry, 1, shell_view, eui);

	/* select-one is always sensitive, except when the clicked and the primary source differ */
	select_one_action = e_ui_manager_get_action (ui_manager, select_one_action_name);
	action = e_ui_manager_get_action (ui_manager, entry->name);

	e_binding_bind_property (
		select_one_action, "sensitive",
		action, "visible",
		G_BINDING_SYNC_CREATE);
}

gboolean
save_calendar_calendar_save_as_init (EUIManager *ui_manager,
				     EShellView *shell_view)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='calendar-popup'>"
		    "<placeholder id='calendar-popup-actions'>"
		      "<item action='calendar-save-as'/>"
		    "</placeholder>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry entry = {
		"calendar-save-as",
		"document-save-as",
		N_("Save _As"),
		NULL,
		N_("Save the selected calendar to disk"),
		action_calendar_save_as_cb, NULL, NULL, NULL
	};

	save_calendar_init_ui (ui_manager, shell_view, "calendar-select-one", &entry, eui);

	return TRUE;
}

gboolean
save_calendar_memo_list_save_as_init (EUIManager *ui_manager,
				      EShellView *shell_view)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='memo-list-popup'>"
		    "<placeholder id='memo-list-popup-actions'>"
		      "<item action='memo-list-save-as'/>"
		    "</placeholder>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry entry = {
		"memo-list-save-as",
		"document-save-as",
		N_("Save _As"),
		NULL,
		N_("Save the selected memo list to disk"),
		action_memo_list_save_as_cb, NULL, NULL, NULL
	};

	save_calendar_init_ui (ui_manager, shell_view, "memo-list-select-one", &entry, eui);

	return TRUE;
}

gboolean
save_calendar_task_list_save_as_init (EUIManager *ui_manager,
				      EShellView *shell_view)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='task-list-popup'>"
		    "<placeholder id='task-list-popup-actions'>"
		      "<item action='task-list-save-as'/>"
		    "</placeholder>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry entry = {
		"task-list-save-as",
		"document-save-as",
		N_("Save _As"),
		NULL,
		N_("Save the selected task list to disk"),
		action_task_list_save_as_cb, NULL, NULL, NULL
	};

	save_calendar_init_ui (ui_manager, shell_view, "task-list-select-one", &entry, eui);

	return TRUE;
}
