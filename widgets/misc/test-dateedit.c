/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * test-dateedit - tests the EDateEdit widget.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "e-dateedit.h"

static void delete_event_cb (GtkWidget *widget,
			     GdkEventAny *event,
			     gpointer data);
static void on_get_date_clicked	(GtkWidget *button,
				 EDateEdit *dedit);

int
main (int argc, char **argv)
{
	GtkWidget *app;
	EDateEdit *dedit;
	GtkWidget *vbox, *button;

	gnome_init ("test-dateedit", "0.0", argc, argv);

	app = gnome_app_new ("Test", "Test");
	gtk_window_set_default_size (GTK_WINDOW (app), 400, 200);
	gtk_window_set_policy (GTK_WINDOW (app), FALSE, TRUE, TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (app), 8);

	gtk_signal_connect (GTK_OBJECT (app), "delete_event",
			    GTK_SIGNAL_FUNC (delete_event_cb), NULL);

	dedit = E_DATE_EDIT (e_date_edit_new ());
	gtk_widget_show ((GtkWidget*) (dedit));

	e_date_edit_set_week_start_day (dedit, 1);
	e_date_edit_set_show_week_numbers (dedit, TRUE);
	e_date_edit_set_use_24_hour_format (dedit, FALSE);
	e_date_edit_set_time_popup_range (dedit, 8, 18);
#if 0
	e_date_edit_set_show_time (dedit, FALSE);
#endif

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), (GtkWidget*) dedit,
			    FALSE, TRUE, 0);
	gtk_widget_show (vbox);

	button = gtk_button_new_with_label ("Get Date");
	gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, TRUE, 0);
	gtk_widget_show (button);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (on_get_date_clicked), dedit);


	gnome_app_set_contents (GNOME_APP (app), vbox);
	gtk_widget_show (app);

	gtk_main ();

	return 0;
}


static void
delete_event_cb (GtkWidget *widget,
		 GdkEventAny *event,
		 gpointer data)
{
	gtk_main_quit ();
}


static void
on_get_date_clicked	(GtkWidget *button,
			 EDateEdit *dedit)
{
	time_t t;

	t = e_date_edit_get_time (dedit);

	if (t == -1)
		g_print ("Time: None\n");
	else
		g_print ("Time: %s", ctime (&t));
}

