/*
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
 * Authors:
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-print.h"

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libedataserver/libedataserver.h>

/* XXX Would be better if GtkPrint exposed these. */
#define PAGE_SETUP_GROUP_NAME		"Page Setup"
#define PRINT_SETTINGS_GROUP_NAME	"Print Settings"

static gchar *
key_file_get_filename (void)
{
	return g_build_filename (e_get_user_data_dir (), "printing.ini", NULL);
}

static void
load_key_file (GKeyFile *key_file)
{
	gchar *filename;
	GError *error = NULL;

	filename = key_file_get_filename ();

	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
		goto exit;

	g_key_file_load_from_file (
		key_file, filename, G_KEY_FILE_KEEP_COMMENTS |
		G_KEY_FILE_KEEP_TRANSLATIONS, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

exit:
	g_free (filename);
}

static void
save_key_file (GKeyFile *key_file)
{
	gchar *contents;
	gchar *filename;
	gsize length;
	GError *error = NULL;

	filename = key_file_get_filename ();
	contents = g_key_file_to_data (key_file, &length, NULL);

	g_file_set_contents (filename, contents, length, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_free (contents);
	g_free (filename);
}

static GtkPrintSettings *
load_settings (GKeyFile *key_file)
{
	GtkPrintSettings *settings;
	GError *error = NULL;

	settings = gtk_print_settings_new ();

	if (g_key_file_has_group (key_file, PRINT_SETTINGS_GROUP_NAME))
		gtk_print_settings_load_key_file (
			settings, key_file, PRINT_SETTINGS_GROUP_NAME, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	return settings;
}

static void
save_settings (GtkPrintSettings *settings,
               GKeyFile *key_file)
{
	/* XXX GtkPrintSettings does not distinguish between settings
	 *     that should persist and one-time-only settings, such as
	 *     page range or number of copies.  All print settings are
	 *     persistent by default and we opt out particular keys by
	 *     popular demand. */

	gtk_print_settings_unset (settings, GTK_PRINT_SETTINGS_N_COPIES);
	gtk_print_settings_unset (settings, GTK_PRINT_SETTINGS_PAGE_RANGES);
	gtk_print_settings_unset (settings, GTK_PRINT_SETTINGS_PAGE_SET);
	gtk_print_settings_unset (settings, GTK_PRINT_SETTINGS_PRINT_PAGES);

	/* Remove old values first */
	g_key_file_remove_group (key_file, PRINT_SETTINGS_GROUP_NAME, NULL);

	gtk_print_settings_to_key_file (settings, key_file, PRINT_SETTINGS_GROUP_NAME);
}

static GtkPageSetup *
load_page_setup (GKeyFile *key_file)
{
	GtkPageSetup *page_setup;

	page_setup = gtk_page_setup_new ();

	if (g_key_file_has_group (key_file, PAGE_SETUP_GROUP_NAME))
		gtk_page_setup_load_key_file (
			page_setup, key_file, PAGE_SETUP_GROUP_NAME, NULL);

	return page_setup;
}

static void
save_page_setup (GtkPageSetup *page_setup,
                 GKeyFile *key_file)
{
	/* Remove old values first */
	g_key_file_remove_group (key_file, PAGE_SETUP_GROUP_NAME, NULL);

	gtk_page_setup_to_key_file (page_setup, key_file, PAGE_SETUP_GROUP_NAME);
}

static void
handle_error (GtkPrintOperation *operation)
{
	GtkWidget *dialog;
	GError *error = NULL;

	dialog = gtk_message_dialog_new_with_markup (
		NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"<span weight=\"bold\" size=\"larger\">%s</span>",
		_("An error occurred while printing"));

	gtk_print_operation_get_error (operation, &error);

	if (error != NULL && error->message != NULL)
		gtk_message_dialog_format_secondary_text (
			GTK_MESSAGE_DIALOG (dialog), "%s\n\n%s",
			_("The printing system reported the "
			"following details about the error:"),
			error->message);
	else
		gtk_message_dialog_format_secondary_text (
			GTK_MESSAGE_DIALOG (dialog), "%s",
			_("The printing system did not report "
			"any additional details about the error."));

	if (error != NULL)
		g_error_free (error);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
}

static void
print_done_cb (GtkPrintOperation *operation,
               GtkPrintOperationResult result,
               GKeyFile *key_file)
{
	GtkPrintSettings *settings;

	settings = gtk_print_operation_get_print_settings (operation);

	if (result == GTK_PRINT_OPERATION_RESULT_APPLY)
		save_settings (settings, key_file);
	if (result == GTK_PRINT_OPERATION_RESULT_ERROR)
		handle_error (operation);

	save_key_file (key_file);
	g_key_file_free (key_file);
}

GtkPrintOperation *
e_print_operation_new (void)
{
	GtkPrintOperation *operation;
	GtkPrintSettings *settings;
	GtkPageSetup *page_setup;
	GKeyFile *key_file;

	operation = gtk_print_operation_new ();

	gtk_print_operation_set_embed_page_setup (operation, TRUE);

	key_file = g_key_file_new ();
	load_key_file (key_file);

	settings = load_settings (key_file);
	gtk_print_operation_set_print_settings (operation, settings);
	g_object_unref (settings);

	page_setup = load_page_setup (key_file);
	gtk_print_operation_set_default_page_setup (operation, page_setup);
	g_object_unref (page_setup);

	g_signal_connect (
		operation, "done",
		G_CALLBACK (print_done_cb), key_file);

	return operation;
}

void
e_print_run_page_setup_dialog (GtkWindow *parent)
{
	GtkPageSetup *new_page_setup;
	GtkPageSetup *old_page_setup;
	GtkPrintSettings *settings;
	GKeyFile *key_file;

	key_file = g_key_file_new ();
	load_key_file (key_file);

	settings = load_settings (key_file);
	old_page_setup = load_page_setup (key_file);
	new_page_setup = gtk_print_run_page_setup_dialog (
		parent, old_page_setup, settings);
	save_page_setup (new_page_setup, key_file);
	save_settings (settings, key_file);

	g_object_unref (new_page_setup);
	g_object_unref (old_page_setup);
	g_object_unref (settings);

	save_key_file (key_file);
	g_key_file_free (key_file);
}

void
e_print_load_settings (GtkPrintSettings **out_settings,
		       GtkPageSetup **out_page_setup)
{
	GKeyFile *key_file;

	g_return_if_fail (out_settings != NULL);
	g_return_if_fail (out_page_setup != NULL);

	key_file = g_key_file_new ();
	load_key_file (key_file);

	*out_settings = load_settings (key_file);
	*out_page_setup = load_page_setup (key_file);

	g_key_file_free (key_file);
}

void
e_print_save_settings (GtkPrintSettings *settings,
		       GtkPageSetup *page_setup)
{
	GKeyFile *key_file;

	key_file = g_key_file_new ();
	load_key_file (key_file);

	save_settings (settings, key_file);
	save_page_setup (page_setup, key_file);

	save_key_file (key_file);
	g_key_file_free (key_file);
}
