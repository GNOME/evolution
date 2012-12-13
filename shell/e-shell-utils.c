/*
 * e-shell-utils.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/**
 * SECTION: e-shell-utils
 * @short_description: high-level utilities with shell integration
 * @include: shell/e-shell-utils.h
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-utils.h"

#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>

/**
 * e_shell_configure_ui_manager:
 * @shell: an #EShell
 * @ui_manager: an #EUIManager
 *
 * Adds shell integration to @ui_manager.  In particular, it keeps
 * @ui_manager's EUIManager:express-mode property synchronized with
 * @shell's EShell:express-mode property.
 **/
void
e_shell_configure_ui_manager (EShell *shell,
                              EUIManager *ui_manager)
{
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (E_IS_UI_MANAGER (ui_manager));

	g_object_bind_property (
		shell, "express-mode",
		ui_manager, "express-mode",
		G_BINDING_SYNC_CREATE);
}

/**
 * e_shell_run_open_dialog:
 * @shell: an #EShell
 * @title: file chooser dialog title
 * @customize_func: optional dialog customization function
 * @customize_data: optional data to pass to @customize_func
 *
 * Runs a #GtkFileChooserDialog in open mode with the given title and
 * returns the selected #GFile.  If @customize_func is provided, the
 * function is called just prior to running the dialog (the file chooser
 * is the first argument, @customize data is the second).  If the user
 * cancels the dialog the function will return %NULL.
 *
 * Returns: the #GFile to open, or %NULL
 **/
GFile *
e_shell_run_open_dialog (EShell *shell,
                         const gchar *title,
                         GtkCallback customize_func,
                         gpointer customize_data)
{
	GtkFileChooser *file_chooser;
	GFile *chosen_file = NULL;
	GtkWidget *dialog;
	GtkWindow *parent;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	parent = e_shell_get_active_window (shell);

	dialog = gtk_file_chooser_dialog_new (
		title, parent,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

	file_chooser = GTK_FILE_CHOOSER (dialog);

	gtk_dialog_set_default_response (
		GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

	gtk_file_chooser_set_local_only (file_chooser, FALSE);

	/* Allow further customizations before running the dialog. */
	if (customize_func != NULL)
		customize_func (dialog, customize_data);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		chosen_file = gtk_file_chooser_get_file (file_chooser);

	gtk_widget_destroy (dialog);

	return chosen_file;
}

/**
 * e_shell_run_save_dialog:
 * @shell: an #EShell
 * @title: file chooser dialog title
 * @suggestion: file name suggestion, or %NULL
 * @filters: Possible filters for dialog, or %NULL
 * @customize_func: optional dialog customization function
 * @customize_data: optional data to pass to @customize_func
 *
 * Runs a #GtkFileChooserDialog in save mode with the given title and
 * returns the selected #GFile.  If @customize_func is provided, the
 * function is called just prior to running the dialog (the file chooser
 * is the first argument, @customize_data is the second).  If the user
 * cancels the dialog the function will return %NULL.
 *
 * With non-%NULL @filters will be added also file filters to the dialog.
 * The string format is "pat1:mt1;pat2:mt2:...", where 'pat' is a pattern
 * and 'mt' is a MIME type for the pattern to be used.  There can be more
 * than one MIME type, those are separated by comma.
 *
 * Returns: the #GFile to save to, or %NULL
 **/
GFile *
e_shell_run_save_dialog (EShell *shell,
                         const gchar *title,
                         const gchar *suggestion,
                         const gchar *filters,
                         GtkCallback customize_func,
                         gpointer customize_data)
{
	GtkFileChooser *file_chooser;
	GFile *chosen_file = NULL;
	GtkWidget *dialog;
	GtkWindow *parent;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	parent = e_shell_get_active_window (shell);

	dialog = gtk_file_chooser_dialog_new (
		title, parent,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);

	file_chooser = GTK_FILE_CHOOSER (dialog);

	gtk_dialog_set_default_response (
		GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

	gtk_file_chooser_set_local_only (file_chooser, FALSE);
	gtk_file_chooser_set_do_overwrite_confirmation (file_chooser, TRUE);

	if (suggestion != NULL) {
		gchar *current_name;

		current_name = g_strdup (suggestion);
		e_filename_make_safe (current_name);
		gtk_file_chooser_set_current_name (file_chooser, current_name);
		g_free (current_name);
	}

	if (filters != NULL) {
		gchar **flts = g_strsplit (filters, ";", -1);
		gint i;

		for (i = 0; flts[i]; i++) {
			GtkFileFilter *filter = gtk_file_filter_new ();
			gchar *flt = flts[i];
			gchar *delim = strchr (flt, ':'), *next = NULL;

			if (delim) {
				*delim = 0;
				next = strchr (delim + 1, ',');
			}

			gtk_file_filter_add_pattern (filter, flt);
			if (g_ascii_strcasecmp (flt, "*.mbox") == 0)
				gtk_file_filter_set_name (filter, _("Berkeley Mailbox (mbox)"));
			else if (g_ascii_strcasecmp (flt, "*.vcf") == 0)
				gtk_file_filter_set_name (filter, _("vCard (.vcf)"));
			else if (g_ascii_strcasecmp (flt, "*.ics") == 0)
				gtk_file_filter_set_name (filter, _("iCalendar (.ics)"));

			while (delim) {
				delim++;
				if (next)
					*next = 0;

				gtk_file_filter_add_mime_type (filter, delim);

				delim = next;
				if (next)
					next = strchr (next + 1, ',');
			}

			gtk_file_chooser_add_filter (file_chooser, filter);
		}

		if (flts && flts[0]) {
			GtkFileFilter *filter = gtk_file_filter_new ();

			gtk_file_filter_add_pattern (filter, "*");
			gtk_file_filter_set_name (filter, _("All Files (*)"));
			gtk_file_chooser_add_filter (file_chooser, filter);
		}

		g_strfreev (flts);
	}

	/* Allow further customizations before running the dialog. */
	if (customize_func != NULL)
		customize_func (dialog, customize_data);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		chosen_file = gtk_file_chooser_get_file (file_chooser);

	gtk_widget_destroy (dialog);

	return chosen_file;
}

/**
 * e_shell_utils_import_uris:
 * @shell: The #EShell instance
 * @uris: %NULL-terminated list of URIs to import
 *
 * Imports given URIs to Evolution, giving user a choice what to import
 * if more than one importer can be applied, and where to import it, if
 * the importer itself is configurable.
 *
 * URIs should be either a filename or URI of form file://.
 * All others are skipped.
 *
 * Returns: the number of URIs successfully handled
 **/
guint
e_shell_utils_import_uris (EShell *shell,
                           const gchar * const *uris)
{
	GtkWindow *parent;
	GtkWidget *assistant;

	g_return_val_if_fail (shell != NULL, 0);
	g_return_val_if_fail (uris != NULL, 0);

	parent = e_shell_get_active_window (shell);
	assistant = e_import_assistant_new_simple (parent, uris);

	if (assistant) {
		g_signal_connect_after (
			assistant, "cancel",
			G_CALLBACK (gtk_widget_destroy), NULL);

		g_signal_connect_after (
			assistant, "finished",
			G_CALLBACK (gtk_widget_destroy), NULL);

		gtk_application_add_window (
			GTK_APPLICATION (shell),
			GTK_WINDOW (assistant));

		gtk_widget_show (assistant);
	} else
		g_warning ("Cannot import any of the given URIs");

	return g_strv_length ((gchar **) uris);
}

/**
 * e_shell_hide_widgets_for_express_mode:
 * @shell: an #EShell
 * @builder: a #GtkBuilder
 * @widget_name: first widget name to hide
 * @...: %NULL-terminated list of additional widget names to hide
 *
 * If Evolution is running in Express mode (i.e. if the specified @shell is
 * in Express mode), then this function will hide a list of widgets, based
 * on their specified names.  The list of names must be %NULL-terminated,
 * and each element of that list must be the name of a widget present in
 * @builder.  Those widgets will then get hidden.
 *
 * This can be used to simplify preference dialogs and such in an easy
 * fashion, for use in Express mode.
 *
 * If Evolution is not running in Express mode, this function does nothing.
 */
void
e_shell_hide_widgets_for_express_mode (EShell *shell,
                                       GtkBuilder *builder,
                                       const gchar *widget_name,
                                       ...)
{
	va_list args;

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (GTK_IS_BUILDER (builder));
	g_return_if_fail (widget_name != NULL);

	if (!e_shell_get_express_mode (shell))
		return;

	va_start (args, widget_name);

	while (widget_name != NULL) {
		GObject *object;

		object = gtk_builder_get_object (builder, widget_name);
		if (!GTK_IS_WIDGET (object)) {
			g_error (
				"Object '%s' was not found in the builder "
				"file, or it is not a GtkWidget", widget_name);
			g_assert_not_reached ();
		}

		gtk_widget_hide (GTK_WIDGET (object));

		widget_name = va_arg (args, const gchar *);
	}

	va_end (args);
}

