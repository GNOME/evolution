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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "e-print.h"

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtkprintunixdialog.h>
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
GtkPrintSettings *
e_print_load_settings (void)
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

/* Saves the print settings */
 
void
e_print_save_settings (GtkPrintSettings *settings)
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

static void
print_dialog_response(GtkWidget *widget, int resp, gpointer data)
{
#ifdef G_OS_UNIX		/* Just to get it to build on Win32 */
	if (resp == GTK_RESPONSE_OK) {
	    e_print_save_settings (gtk_print_unix_dialog_get_settings(GTK_PRINT_UNIX_DIALOG (widget)));	
	}
#endif
}

/* Creates a dialog with the print settings */
GtkWidget *
e_print_get_dialog (const char *title, int flags)
{
	GtkPrintSettings *settings;
	GtkWidget *dialog;
	
	settings = gtk_print_settings_new ();
	settings = e_print_load_settings ();
	dialog = e_print_get_dialog_with_config (title, flags, settings);
	g_object_unref (settings);
	return dialog;
}

GtkWidget *
e_print_get_dialog_with_config (const char *title, int flags, GtkPrintSettings *settings)
{
	GtkWidget *dialog = NULL;
	
#ifdef G_OS_UNIX		/* Just to get it to build on Win32 */
	dialog = gtk_print_unix_dialog_new (title, NULL);
	gtk_print_unix_dialog_set_settings (GTK_PRINT_UNIX_DIALOG(dialog), settings);
	g_signal_connect(dialog, "response", G_CALLBACK(print_dialog_response), NULL); 
#endif
	return dialog;
}
