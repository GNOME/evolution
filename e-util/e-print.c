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

#include <stdio.h>
#include <string.h>
#include <gconf/gconf-client.h>
#include "e-print.h"

#define PRINT_CONFIG_KEY "/apps/evolution/shell/print_config"

GnomePrintConfig *
e_print_load_config (void)
{
	GConfClient *gconf;
	GnomePrintConfig *config;
	char *str;

	gconf = gconf_client_get_default ();
	str = gconf_client_get_string (gconf, PRINT_CONFIG_KEY, NULL);
	g_object_unref (gconf);
	
	if (!str)
		return gnome_print_config_default ();

	config = gnome_print_config_from_string (str, 0);

	/* Its unlikely people will want to preserve this too often */
	gnome_print_config_set_int (config, GNOME_PRINT_KEY_NUM_COPIES, 1);
	gnome_print_config_set_boolean (config, GNOME_PRINT_KEY_COLLATE, FALSE);
	
	return config;
}


void
e_print_save_config (GnomePrintConfig *config)
{
	GConfClient *gconf;
	char *str;

	str = gnome_print_config_to_string (config, 0);

	gconf = gconf_client_get_default ();
	gconf_client_set_string (gconf, PRINT_CONFIG_KEY, str, NULL);
	g_object_unref (gconf);
}

static void
print_dialog_response(GtkWidget *widget, int resp, gpointer data)
{
	if (resp == GNOME_PRINT_DIALOG_RESPONSE_PRINT)
		e_print_save_config (gnome_print_dialog_get_config (GNOME_PRINT_DIALOG (widget)));
}

GtkWidget *
e_print_get_dialog (const char *title, int flags)
{
	GnomePrintConfig *config;
	GtkWidget *dialog;
	
	config = e_print_load_config ();
	dialog = e_print_get_dialog_with_config (title, flags, config);
	g_object_unref (config);

	return dialog;
}

GtkWidget *
e_print_get_dialog_with_config (const char *title, int flags, GnomePrintConfig *config)
{
	GtkWidget *dialog;

	dialog = g_object_new (GNOME_TYPE_PRINT_DIALOG, "print_config", config, NULL);
	gnome_print_dialog_construct (GNOME_PRINT_DIALOG (dialog), title, flags);

	g_signal_connect(dialog, "response", G_CALLBACK(print_dialog_response), NULL);

	return dialog;
}
