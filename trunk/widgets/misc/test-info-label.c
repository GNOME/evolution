/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-title-bar.c
 *
 * Copyright (C) 2000 Ximian, Inc.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkbox.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>

#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-ui-init.h>
#include <e-util/e-icon-factory.h>
#include "e-info-label.h"

static void
delete_event_cb (GtkWidget *widget,
		 GdkEventAny *event,
		 gpointer data)
{
	gtk_main_quit ();
	e_icon_factory_shutdown ();
}

int
main (int argc, char **argv)
{
	GtkWidget *app;
	GtkWidget *info_label;
	GtkWidget *label;
	GtkWidget *vbox;

	gnome_init ("test-title-bar", "0.0", argc, argv);
	e_icon_factory_init ();

	app = gnome_app_new ("Test", "Test");
	gtk_window_set_default_size (GTK_WINDOW (app), 400, 400);
	gtk_window_set_policy (GTK_WINDOW (app), FALSE, TRUE, FALSE);

	g_signal_connect((app), "delete_event", G_CALLBACK (delete_event_cb), NULL);

	info_label = e_info_label_new ("stock_default-folder");
	e_info_label_set_info ((EInfoLabel *) info_label, "Component Name", "An annoyingly long component message");
	gtk_widget_show (info_label);

	label = gtk_label_new ("boo");
	gtk_widget_show (label);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), info_label, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	gnome_app_set_contents (GNOME_APP (app), vbox);
	gtk_widget_show (app);

	gtk_main ();

	return 0;
}
