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

#include "e-shell-utils.h"

#include <glib/gi18n-lib.h>

/**
 * e_shell_run_open_dialog:
 * @shell: an #EShell
 * @title: file chooser dialog title
 * @customize_func: optional dialog customization function
 * @customize_data: optional data to pass to @customize_func
 *
 * Runs a #GtkFileChooserDialog in open mode with the given title and
 * returns the selected #GFile.  It automatically remembers the selected
 * folder.  If @customize_func is provided, the function is called just
 * prior to running the dialog (the file chooser is the first argument,
 * @customize data is the second).  If the user cancels the dialog the
 * function will return %NULL.
 *
 * Returns: the #GFile to open, or %NULL
 **/
GFile *
e_shell_run_open_dialog (EShell *shell,
                         const gchar *title,
                         GtkCallback customize_func,
                         gpointer customize_data)
{
	EShellSettings *shell_settings;
	GtkFileChooser *file_chooser;
	GFile *chosen_file = NULL;
	GtkWidget *dialog;
	GtkWindow *parent;
	const gchar *property_name;
	gchar *uri;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	property_name = "file-chooser-folder";
	shell_settings = e_shell_get_shell_settings (shell);

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

	/* Restore the current folder from the previous file chooser. */
	uri = e_shell_settings_get_string (shell_settings, property_name);
	if (uri != NULL)
		gtk_file_chooser_set_current_folder_uri (file_chooser, uri);
	g_free (uri);

	/* Allow further customizations before running the dialog. */
	if (customize_func != NULL)
		customize_func (dialog, customize_data);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
		goto exit;

	chosen_file = gtk_file_chooser_get_file (file_chooser);

	/* Save the current folder for subsequent file choosers. */
	uri = gtk_file_chooser_get_current_folder_uri (file_chooser);
	e_shell_settings_set_string (shell_settings, property_name, uri);
	g_free (uri);

exit:
	gtk_widget_destroy (dialog);

	return chosen_file;
}

/**
 * e_shell_run_save_dialog:
 * @shell: an #EShell
 * @title: file chooser dialog title
 * @suggestion: file name suggestion, or %NULL
 * @customize_func: optional dialog customization function
 * @customize_data: optional data to pass to @customize_func
 *
 * Runs a #GtkFileChooserDialog in save mode with the given title and
 * returns the selected #GFile.  It automatically remembers the selected
 * folder.  If @customize_func is provided, the function is called just
 * prior to running the dialog (the file chooser is the first argument,
 * @customize_data is the second).  If the user cancels the dialog the
 * function will return %NULL.
 *
 * Returns: the #GFile to save to, or %NULL
 **/
GFile *
e_shell_run_save_dialog (EShell *shell,
                         const gchar *title,
                         const gchar *suggestion,
                         GtkCallback customize_func,
                         gpointer customize_data)
{
	EShellSettings *shell_settings;
	GtkFileChooser *file_chooser;
	GFile *chosen_file = NULL;
	GtkWidget *dialog;
	GtkWindow *parent;
	const gchar *property_name;
	gchar *uri;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	property_name = "file-chooser-folder";
	shell_settings = e_shell_get_shell_settings (shell);

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

	/* Restore the current folder from the previous file chooser. */
	uri = e_shell_settings_get_string (shell_settings, property_name);
	if (uri != NULL)
		gtk_file_chooser_set_current_folder_uri (file_chooser, uri);
	g_free (uri);

	if (suggestion != NULL)
		gtk_file_chooser_set_current_name (file_chooser, suggestion);

	/* Allow further customizations before running the dialog. */
	if (customize_func != NULL)
		customize_func (dialog, customize_data);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
		goto exit;

	chosen_file = gtk_file_chooser_get_file (file_chooser);

	/* Save the current folder for subsequent file choosers. */
	uri = gtk_file_chooser_get_current_folder_uri (file_chooser);
	e_shell_settings_set_string (shell_settings, property_name, uri);
	g_free (uri);

exit:
	gtk_widget_destroy (dialog);

	return chosen_file;
}
