/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* test-multi-config-dialog.c
 *
 * Copyright (C) 2002 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Ettore Perazzoli <ettore@ximian.com>
 */

#include "e-multi-config-dialog.c"

#include <gnome.h>


#define NUM_PAGES 10


static void
add_pages (EMultiConfigDialog *multi_config_dialog)
{
	int i;

	for (i = 0; i < NUM_PAGES; i ++) {
		GtkWidget *widget;
		GtkWidget *page;
		char *string;
		char *title;
		char *description;

		string = g_strdup_printf ("This is page %d", i);
		description = g_strdup_printf ("Description of page %d", i);
		title = g_strdup_printf ("Title of page %d", i);

		widget = gtk_label_new (string);
		gtk_widget_show (widget);

		page = e_config_page_new ();
		gtk_container_add (GTK_CONTAINER (page), widget);

		e_multi_config_dialog_add_page (multi_config_dialog, title, description, NULL,
						E_CONFIG_PAGE (page));

		g_free (string);
		g_free (title);
		g_free (description);
	}
}

static int
delete_event_callback (GtkWidget *widget,
		       GdkEventAny *event,
		       void *data)
{
	gtk_main_quit ();

	return TRUE;
}


int
main (int argc, char **argv)
{
	GtkWidget *dialog;

	gnome_program_init (
		"test-multi-config-dialog", "0.0", LIBGNOMEUI_MODULE,
		argc, argv, GNOME_PARAM_NONE);

	dialog = e_multi_config_dialog_new ();

	gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 300);
	g_signal_connect((dialog), "delete_event",
			    G_CALLBACK (delete_event_callback), NULL);

	add_pages (E_MULTI_CONFIG_DIALOG (dialog));

	gtk_widget_show (dialog);

	gtk_main ();

	return 0;
}
