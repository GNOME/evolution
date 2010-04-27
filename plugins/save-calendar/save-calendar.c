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
 *		Rodrigo Moya <rodrigo@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* This is prototype code only, this may, or may not, use undocumented
 * unstable or private internal function calls. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-source-selector.h>
#include <libecal/e-cal.h>
#include <e-util/e-alert-dialog.h>
#include <e-util/e-plugin.h>
#include <string.h>

#include <shell/e-shell-sidebar.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>

#include "format-handler.h"

/* Plugin entry points */
gboolean	calendar_save_as_init		(GtkUIManager *ui_manager,
						 EShellView *shell_view);
gboolean	memo_list_save_as_init		(GtkUIManager *ui_manager,
						 EShellView *shell_view);
gboolean	task_list_save_as_init		(GtkUIManager *ui_manager,
						 EShellView *shell_view);

gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
{
	return 0;
}

enum {  /* GtkComboBox enum */
	DEST_NAME_COLUMN,
	DEST_HANDLER,
	N_DEST_COLUMNS

};

static void
extra_widget_foreach_hide (GtkWidget *widget, gpointer data)
{
	if (widget != data)
		gtk_widget_hide (widget);
}

static void
on_type_combobox_changed (GtkComboBox *combobox, gpointer data)
{
	FormatHandler *handler = NULL;
	GtkWidget *extra_widget = data;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_combo_box_get_model (combobox);

	gtk_container_foreach (GTK_CONTAINER (extra_widget),
		extra_widget_foreach_hide, g_object_get_data (G_OBJECT (combobox), "format-box"));

	gtk_combo_box_get_active_iter (combobox, &iter);

	gtk_tree_model_get (model, &iter,
		DEST_HANDLER, &handler, -1);

	if (handler->options_widget)
	{
		gtk_widget_show (handler->options_widget);
	}

}

static void
format_handlers_foreach_free (gpointer data, gpointer user_data)
{
	FormatHandler *handler = data;

	if (handler->options_widget)
		gtk_widget_destroy (handler->options_widget);

	if (handler->data)
		g_free (handler->data);

	g_free (data);
}

static void
ask_destination_and_save (ESourceSelector *selector, ECalSourceType type)
{
	FormatHandler *handler = NULL;

	GtkWidget *extra_widget = gtk_vbox_new (FALSE, 0);
	GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
	GtkLabel *label = GTK_LABEL (gtk_label_new_with_mnemonic (_("_Format:")));
	GtkComboBox *combo = GTK_COMBO_BOX(gtk_combo_box_new ());
	GtkTreeModel *model = GTK_TREE_MODEL (gtk_list_store_new
		(N_DEST_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER));
	GtkCellRenderer *renderer=NULL;
	GtkListStore *store = GTK_LIST_STORE (model);
	GtkTreeIter iter;
	GtkWidget *dialog = NULL;
	gchar *dest_uri = NULL;

	GList *format_handlers = NULL;

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
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo),
			renderer, "text", DEST_NAME_COLUMN, NULL);

	while (format_handlers) {
		handler = format_handlers->data;
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, DEST_NAME_COLUMN,
			handler->combo_label, -1);
		gtk_list_store_set (store, &iter, DEST_HANDLER, handler, -1);

		if (handler->options_widget) {
			gtk_box_pack_start (GTK_BOX (extra_widget),
				GTK_WIDGET (handler->options_widget), TRUE, TRUE, 0);
			gtk_widget_hide (handler->options_widget);
		}

		if (handler->isdefault) {
			gtk_combo_box_set_active_iter (combo, &iter);
			if (handler->options_widget)
				gtk_widget_show (handler->options_widget);
		}

		format_handlers = g_list_next (format_handlers);
	}

	g_signal_connect (G_OBJECT(combo), "changed",
		G_CALLBACK (on_type_combobox_changed), extra_widget);
	g_object_set_data (G_OBJECT (combo), "format-box", hbox);

	dialog = gtk_file_chooser_dialog_new (_("Select destination file"),
					      NULL,
					      GTK_FILE_CHOOSER_ACTION_SAVE,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_SAVE_AS, GTK_RESPONSE_OK,
					      NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dialog), extra_widget);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);
	gtk_widget_show (hbox);
	gtk_widget_show (GTK_WIDGET (label));
	gtk_widget_show (GTK_WIDGET (combo));
	gtk_widget_show (extra_widget);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		gchar *tmp = NULL;

		gtk_combo_box_get_active_iter (combo, &iter);
		gtk_tree_model_get (model, &iter,
			DEST_HANDLER, &handler, -1);

	       dest_uri = gtk_file_chooser_get_uri
			(GTK_FILE_CHOOSER (dialog));

		tmp = strstr (dest_uri, handler->filename_ext);

		if (!(tmp && *(tmp + strlen (handler->filename_ext)) == '\0')) {

			gchar *temp;
			temp = g_strconcat (dest_uri, handler->filename_ext, NULL);
			g_free (dest_uri);
			dest_uri = temp;
		}

		handler->save (handler, selector, type, dest_uri);
	}

	/* Free the handlers */
	g_list_foreach (format_handlers, format_handlers_foreach_free, NULL);
	g_list_free (format_handlers);

	/* Now we can destroy it */
	gtk_widget_destroy (dialog);
	g_free (dest_uri);

}

/* Returns output stream for the uri, or NULL on any error.
   When done with the stream, just g_output_stream_close and g_object_unref it.
   It will ask for overwrite if file already exists.
*/
GOutputStream *
open_for_writing (GtkWindow *parent, const gchar *uri, GError **error)
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
		} else if (fostream) {
			g_object_unref (fostream);
			fostream = NULL;
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
save_general (EShellView *shell_view,
              ECalSourceType type)
{
	EShellSidebar *shell_sidebar;
	ESourceSelector *selector = NULL;

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_object_get (shell_sidebar, "selector", &selector, NULL);
	g_return_if_fail (selector != NULL);

	ask_destination_and_save (selector, type);

	g_object_unref (selector);
}

static void
action_calendar_save_as_cb (GtkAction *action,
                            EShellView *shell_view)
{
	save_general (shell_view, E_CAL_SOURCE_TYPE_EVENT);
}

static void
action_memo_list_save_as_cb (GtkAction *action,
                             EShellView *shell_view)
{
	save_general (shell_view, E_CAL_SOURCE_TYPE_JOURNAL);
}

static void
action_task_list_save_as_cb (GtkAction *action,
                             EShellView *shell_view)
{
	save_general (shell_view, E_CAL_SOURCE_TYPE_TODO);
}

gboolean
calendar_save_as_init (GtkUIManager *ui_manager,
                       EShellView *shell_view)
{
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	GtkAction *action;
	const gchar *tooltip;
	const gchar *stock_id;
	const gchar *name;

	shell_window = e_shell_view_get_shell_window (shell_view);

	name = "calendar-save-as";
	tooltip = _("Save the selected calendar to disk");
	stock_id = GTK_STOCK_SAVE_AS;
	action = gtk_action_new (name, NULL, tooltip, stock_id);

	name = "lockdown-save-to-disk";
	action_group = e_shell_window_get_action_group (shell_window, name);
	gtk_action_group_add_action (action_group, action);

	g_signal_connect (
		action, "activate",
		G_CALLBACK (action_calendar_save_as_cb), shell_view);

	g_object_unref (action);

	return TRUE;
}

gboolean
memo_list_save_as_init (GtkUIManager *ui_manager,
                        EShellView *shell_view)
{
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	GtkAction *action;
	const gchar *tooltip;
	const gchar *stock_id;
	const gchar *name;

	shell_window = e_shell_view_get_shell_window (shell_view);

	name = "memo-list-save-as";
	tooltip = _("Save the selected memo list to disk");
	stock_id = GTK_STOCK_SAVE_AS;
	action = gtk_action_new (name, NULL, tooltip, stock_id);

	name = "lockdown-save-to-disk";
	action_group = e_shell_window_get_action_group (shell_window, name);
	gtk_action_group_add_action (action_group, action);

	g_signal_connect (
		action, "activate",
		G_CALLBACK (action_memo_list_save_as_cb), shell_view);

	g_object_unref (action);

	return TRUE;
}

gboolean
task_list_save_as_init (GtkUIManager *ui_manager,
                        EShellView *shell_view)
{
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	GtkAction *action;
	const gchar *tooltip;
	const gchar *stock_id;
	const gchar *name;

	shell_window = e_shell_view_get_shell_window (shell_view);

	name = "task-list-save-as";
	tooltip = _("Save the selected task list to disk");
	stock_id = GTK_STOCK_SAVE_AS;
	action = gtk_action_new (name, NULL, tooltip, stock_id);

	name = "lockdown-save-to-disk";
	action_group = e_shell_window_get_action_group (shell_window, name);
	gtk_action_group_add_action (action_group, action);

	g_signal_connect (
		action, "activate",
		G_CALLBACK (action_task_list_save_as_cb), shell_view);

	g_object_unref (action);

	return TRUE;
}
