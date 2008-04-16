/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-print.c - Uniform print setting/dialog routines for Evolution
 *
 * Copyright (C) 2005 Novell, Inc.
 *
 * Authors: JP Rosevear <jpr@novell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "e-print.h"

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>

#define PRINTING "/apps/evolution/shell/printing"

static void
pack_settings (const gchar *key, const gchar *value, GSList **p_list)
{
	gchar *item;
	item = g_strdup_printf ("%s=%s", key, value);
	*p_list = g_slist_prepend (*p_list, item);
}

static void
unpack_settings (gchar *item, GtkPrintSettings *settings)
{
	gchar *cp, *key, *value;
	cp = strchr (item, '=');
	if (cp == NULL)
		return;
	*cp ++ = '\0';
	key = g_strstrip (item);
	value = g_strstrip (cp);
	gtk_print_settings_set (settings, key, value);
	g_free (item);
}

static GtkPrintSettings *
load_settings (void)
{
	GConfClient *client;
	GtkPrintSettings *settings;
	GSList *list;
	GError *error = NULL;

	client = gconf_client_get_default ();
	settings = gtk_print_settings_new ();

	list = gconf_client_get_list (
		client, PRINTING, GCONF_VALUE_STRING, &error);
	if (error == NULL) {
		g_slist_foreach (list, (GFunc) unpack_settings, settings);
		g_slist_free (list);
	} else {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	g_object_unref (client);

	return settings;
}

static void
save_settings (GtkPrintSettings *settings)
{
	GConfClient *client;
	GSList *list = NULL;
	GError *error = NULL;

	client = gconf_client_get_default ();

	gtk_print_settings_foreach (
		settings, (GtkPrintSettingsFunc) pack_settings, &list);
	gconf_client_set_list (
		client, PRINTING, GCONF_VALUE_STRING, list, &error);
	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);

	g_object_unref (client);
}

static GtkPageSetup *
load_page_setup (GtkPrintSettings *settings)
{
	GtkPageSetup *page_setup;
	GtkPaperSize *paper_size;

	page_setup = gtk_page_setup_new ();
	gtk_page_setup_set_orientation (
		page_setup, gtk_print_settings_get_orientation (settings));
	paper_size = gtk_print_settings_get_paper_size (settings);
	if (paper_size != NULL)
		gtk_page_setup_set_paper_size_and_default_margins (
			page_setup, paper_size);

	return page_setup;
}

static void
save_page_setup (GtkPrintSettings *settings, GtkPageSetup *page_setup)
{
	gtk_print_settings_set_orientation (
		settings, gtk_page_setup_get_orientation (page_setup));
	gtk_print_settings_set_paper_size (
		settings, gtk_page_setup_get_paper_size (page_setup));
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
print_done_cb (GtkPrintOperation *operation, GtkPrintOperationResult result)
{
	GtkPrintSettings *settings;

	settings = gtk_print_operation_get_print_settings (operation);

	if (result == GTK_PRINT_OPERATION_RESULT_APPLY)
		save_settings (settings);
	if (result == GTK_PRINT_OPERATION_RESULT_ERROR)
		handle_error (operation);
}

GtkPrintOperation *
e_print_operation_new (void)
{
	GtkPrintOperation *operation;
	GtkPrintSettings *settings;

	operation = gtk_print_operation_new ();

	settings = load_settings ();
	gtk_print_operation_set_print_settings (operation, settings);
	g_object_unref (settings);

	g_signal_connect (operation, "done", G_CALLBACK (print_done_cb), NULL);

	return operation;
}

void
e_print_run_page_setup_dialog (GtkWindow *parent)
{
	GtkPageSetup *new_page_setup;
	GtkPageSetup *old_page_setup;
	GtkPrintSettings *settings;

	settings = load_settings ();
	old_page_setup = load_page_setup (settings);
	new_page_setup = gtk_print_run_page_setup_dialog (
		parent, old_page_setup, settings);
	save_page_setup (settings, new_page_setup);
	save_settings (settings);

	g_object_unref (new_page_setup);
	g_object_unref (old_page_setup);
	g_object_unref (settings);
}
