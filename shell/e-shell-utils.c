/*
 * e-shell-utils.c
 *
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
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Open"), GTK_RESPONSE_ACCEPT, NULL);

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
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Save"), GTK_RESPONSE_ACCEPT, NULL);

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

		for (i = 0; flts && flts[i]; i++) {
			GtkFileFilter *filter = gtk_file_filter_new ();
			gchar *flt = flts[i];
			gchar *delim = strchr (flt, ':'), *next = NULL;

			if (delim) {
				*delim = 0;
				next = strchr (delim + 1, ',');
			}

			gtk_file_filter_add_pattern (filter, flt);
			if (g_ascii_strcasecmp (flt, "*.mbox") == 0)
				gtk_file_filter_set_name (
					filter, _("Berkeley Mailbox (mbox)"));
			else if (g_ascii_strcasecmp (flt, "*.vcf") == 0)
				gtk_file_filter_set_name (
					filter, _("vCard (.vcf)"));
			else if (g_ascii_strcasecmp (flt, "*.ics") == 0)
				gtk_file_filter_set_name (
					filter, _("iCalendar (.ics)"));

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

