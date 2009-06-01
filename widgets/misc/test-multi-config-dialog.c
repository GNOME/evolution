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
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-multi-config-dialog.c"

#include <gtk/gtk.h>


#define NUM_PAGES 10


static void
add_pages (EMultiConfigDialog *multi_config_dialog)
{
	gint i;

	for (i = 0; i < NUM_PAGES; i ++) {
		GtkWidget *widget;
		GtkWidget *page;
		gchar *string;
		gchar *title;
		gchar *description;

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

static gint
delete_event_callback (GtkWidget *widget,
		       GdkEventAny *event,
		       gpointer data)
{
	gtk_main_quit ();

	return TRUE;
}


gint
main (gint argc, gchar **argv)
{
	GtkWidget *dialog;

	gtk_init (&argc, &argv);

	dialog = e_multi_config_dialog_new ();

	g_signal_connect(
		dialog, "delete-event",
		G_CALLBACK (delete_event_callback), NULL);

	add_pages (E_MULTI_CONFIG_DIALOG (dialog));

	gtk_widget_show (dialog);

	gtk_main ();

	return 0;
}
