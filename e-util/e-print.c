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
#include <gtk/gtk.h>
#include <gtk/gtkprintunixdialog.h>
#define PRINTER "/apps/evolution/shell/printer"
#define SCALE "/apps/evolution/shell/scale"
#define PRINT_PAGES "/apps/evolution/shell/print_pages"
#define PAGE_RANGES "/apps/evolution/shell/page_ranges"
#define PAGE_SET "/apps/evolution/shell/page_set"
#define COLLATE "/apps/evolution/shell/collate"
#define REVERSE "/apps/evolution/shell/reverse"
#define N_COPIES "/apps/evolution/shell/n_copies"
#define LPR_COMMANDLINE "/apps/evolution/shell/lpr_commandline"
#define OUTPUT_URI "/apps/evolution/shell/output_uri"
#define OUTPUT_FILE_FORMAT "/apps/evolution/shell/output_file_format"

/* Loads the print settings that were saved previously */

GtkPrintSettings *
e_print_load_config (void)
{
	GConfClient *gconf;
	GtkPrintSettings *settings;
	gchar *printer_name;
	gchar *collate;
	gchar *n_copies;
	gchar *print_pages;
	gchar *page_set;
	gchar *scale;
	gchar *output_uri;
	gchar *output_file_format;
	gchar *reverse;
	gchar *page_ranges;
	gchar *lpr_commandline;
	
	settings = gtk_print_settings_new ();

	gconf = gconf_client_get_default ();

	printer_name = gconf_client_get_string (gconf, PRINTER, NULL);
	gtk_print_settings_set (settings, "printer", printer_name);
	
	n_copies = gconf_client_get_string (gconf, N_COPIES, NULL);
	gtk_print_settings_set (settings, "n-copies",n_copies);

	collate = gconf_client_get_string (gconf, COLLATE, NULL);
	gtk_print_settings_set (settings, "collate", collate);

	lpr_commandline = gconf_client_get_string (gconf, LPR_COMMANDLINE, NULL);
	gtk_print_settings_set (settings, "lpr-commandline", lpr_commandline);

	print_pages = gconf_client_get_string (gconf, PRINT_PAGES, NULL);
 	gtk_print_settings_set (settings, "print-pages", print_pages);

	page_set = gconf_client_get_string (gconf, PAGE_SET, NULL);
	gtk_print_settings_set (settings, "page-set", page_set);
	
	output_uri = gconf_client_get_string (gconf, OUTPUT_URI, NULL);
	gtk_print_settings_set (settings, "output-uri", output_uri);

	output_file_format = gconf_client_get_string (gconf, OUTPUT_FILE_FORMAT, NULL);
	gtk_print_settings_set (settings, "output-file-format",output_file_format);

	reverse = gconf_client_get_string (gconf, REVERSE, NULL);
	gtk_print_settings_set (settings, "reverse", reverse);

	scale = gconf_client_get_string (gconf, SCALE, NULL);
	gtk_print_settings_set (settings, "scale", scale);

	page_ranges = gconf_client_get_string (gconf, PAGE_RANGES, NULL);
	gtk_print_settings_set (settings, "page-ranges", page_ranges);

	g_free (printer_name);
	g_free (collate);
	g_free (n_copies);
	g_free (print_pages);
	g_free (page_set);
	g_free (scale);
	g_free (output_uri);
	g_free (output_file_format);
	g_free (reverse);
	g_free (page_ranges);
	g_free (lpr_commandline);
	g_object_unref (gconf); 
	return settings;
}

/* Saves the print settings */
 
void
e_print_save_config (GtkPrintSettings *settings)
{
	GConfClient *gconf;
	const gchar *printer_name;
	const gchar *collate;
	const gchar *n_copies;
	const gchar *print_pages;
	const gchar *page_set;
	const gchar *scale;
	const gchar *output_uri;
	const gchar *output_file_format;
	const gchar *reverse;
	const gchar *page_ranges;
	const gchar *lpr_commandline;

	gconf = gconf_client_get_default ();
	printer_name = gtk_print_settings_get (settings, "printer");
	gconf_client_set_string (gconf, PRINTER, printer_name, NULL);
		
	scale = gtk_print_settings_get (settings, "scale");
	gconf_client_set_string (gconf, SCALE, scale, NULL);
			
	page_set = gtk_print_settings_get (settings, "page-set");
	gconf_client_set_string (gconf, PAGE_SET, page_set, NULL);
				
	print_pages = gtk_print_settings_get (settings, "print-pages");
	gconf_client_set_string (gconf, PRINT_PAGES, print_pages, NULL);
				
	lpr_commandline = gtk_print_settings_get (settings, "lpr-commandline");
	gconf_client_set_string (gconf, LPR_COMMANDLINE, lpr_commandline, NULL);
	
	collate = gtk_print_settings_get (settings, "collate");
	gconf_client_set_string (gconf, COLLATE, collate, NULL);
	
	output_uri = gtk_print_settings_get (settings, "output-uri");
	gconf_client_set_string (gconf, OUTPUT_URI, output_uri, NULL);

	output_file_format = gtk_print_settings_get (settings, "output-file-format");
	gconf_client_set_string (gconf, OUTPUT_FILE_FORMAT, output_file_format, NULL);

	reverse = gtk_print_settings_get (settings, "reverse");
	gconf_client_set_string (gconf, REVERSE, reverse, NULL);

	n_copies = gtk_print_settings_get (settings, "n_copies");
	gconf_client_set_string (gconf, N_COPIES, n_copies, NULL);

	page_ranges = gtk_print_settings_get (settings, "page-ranges");
	gconf_client_set_string (gconf, PAGE_RANGES, "page-ranges",NULL);

	g_object_unref (gconf);
}

static void
print_dialog_response(GtkWidget *widget, int resp, gpointer data)
{
	if (resp == GTK_RESPONSE_OK) {
	    e_print_save_config (gtk_print_unix_dialog_get_settings(GTK_PRINT_UNIX_DIALOG (widget)));	
	}
}

/* Creates a dialog with the print settings */
GtkWidget *
e_print_get_dialog (const char *title, int flags)
{
	GtkPrintSettings *settings;
	GtkWidget *dialog;
	
	settings = e_print_load_config ();
	dialog = e_print_get_dialog_with_config (title, flags, settings);
	g_object_unref (settings);
	return dialog;
}

GtkWidget *
e_print_get_dialog_with_config (const char *title, int flags, GtkPrintSettings *settings)
{
	GtkWidget *dialog;
	
	dialog = gtk_print_unix_dialog_new (title, NULL);
	gtk_print_unix_dialog_set_settings (GTK_PRINT_UNIX_DIALOG(dialog), settings);
	g_signal_connect(dialog, "response", G_CALLBACK(print_dialog_response), NULL); 
	return dialog;
}
