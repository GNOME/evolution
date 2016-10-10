/*
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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * test-dateedit - tests the EDateEdit widget.
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include "e-dateedit.h"
#include "e-misc-utils.h"

static void delete_event_cb		(GtkWidget	*widget,
					 GdkEventAny	*event,
					 GtkWidget	*window);
static void on_get_date_clicked		(GtkWidget	*button,
					 EDateEdit	*dedit);
static void on_toggle_24_hour_clicked	(GtkWidget	*button,
					 EDateEdit	*dedit);
static void on_changed			(EDateEdit	*dedit,
					 gchar		*name);
#if 0
static void on_date_changed		(EDateEdit	*dedit,
					 gchar		*name);
static void on_time_changed		(EDateEdit	*dedit,
					 gchar		*name);
#endif

gint
main (gint argc,
      gchar **argv)
{
	GtkWidget *window;
	EDateEdit *dedit;
	GtkWidget *table, *button;

	gtk_init (&argc, &argv);

	puts ("here");

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "EDateEdit Test");
	gtk_window_set_default_size (GTK_WINDOW (window), 300, 200);
	gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);

	g_signal_connect (
		window, "delete_event",
		G_CALLBACK (delete_event_cb), window);

	table = gtk_table_new (3, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table), 4);
	gtk_widget_show (table);

	gtk_container_add (GTK_CONTAINER (window), table);

	/* EDateEdit 1. */
	dedit = E_DATE_EDIT (e_date_edit_new ());
	gtk_table_attach (
		GTK_TABLE (table), GTK_WIDGET (dedit),
		0, 1, 0, 1, GTK_FILL, GTK_EXPAND, 0, 0);
	gtk_widget_show (GTK_WIDGET (dedit));

#if 0
	g_signal_connect (
		dedit, "date_changed",
		G_CALLBACK (on_date_changed), (gpointer) "1");
	g_signal_connect (
		dedit, "time_changed",
		G_CALLBACK (on_time_changed), (gpointer) "1");
#else
	g_signal_connect (
		dedit, "changed",
		G_CALLBACK (on_changed), (gpointer) "1");
#endif

	button = gtk_button_new_with_label ("Print Date");
	gtk_table_attach (
		GTK_TABLE (table), button,
		1, 2, 0, 1, 0, 0, 0, 0);
	gtk_widget_show (button);
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (on_get_date_clicked), dedit);

	/* EDateEdit 2. */
	dedit = E_DATE_EDIT (e_date_edit_new ());
	gtk_table_attach (
		GTK_TABLE (table), (GtkWidget *) dedit,
		0, 1, 1, 2, GTK_FILL, GTK_EXPAND, 0, 0);
	gtk_widget_show ((GtkWidget *) (dedit));
	e_date_edit_set_week_start_day (dedit, 1);
	e_date_edit_set_show_week_numbers (dedit, TRUE);
	e_date_edit_set_use_24_hour_format (dedit, FALSE);
	e_date_edit_set_time_popup_range (dedit, 8, 18);
	e_date_edit_set_show_time (dedit, FALSE);

#if 0
	g_signal_connect (
		dedit, "date_changed",
		G_CALLBACK (on_date_changed), (gpointer) "2");
	g_signal_connect (
		dedit, "time_changed",
		G_CALLBACK (on_time_changed), (gpointer) "2");
#else
	g_signal_connect (
		dedit, "changed",
		G_CALLBACK (on_changed), (gpointer) "2");
#endif

	button = gtk_button_new_with_label ("Print Date");
	gtk_table_attach (
		GTK_TABLE (table), button,
		1, 2, 1, 2, 0, 0, 0, 0);
	gtk_widget_show (button);
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (on_get_date_clicked), dedit);

	/* EDateEdit 3. */
	dedit = E_DATE_EDIT (e_date_edit_new ());
	gtk_table_attach (
		GTK_TABLE (table), (GtkWidget *) dedit,
		0, 1, 2, 3, GTK_FILL, GTK_EXPAND, 0, 0);
	gtk_widget_show ((GtkWidget *) (dedit));
	e_date_edit_set_week_start_day (dedit, 1);
	e_date_edit_set_show_week_numbers (dedit, TRUE);
	e_date_edit_set_use_24_hour_format (dedit, FALSE);
	e_date_edit_set_time_popup_range (dedit, 8, 18);
	e_date_edit_set_allow_no_date_set (dedit, TRUE);

#if 0
	g_signal_connect (
		dedit, "date_changed",
		G_CALLBACK (on_date_changed), (gpointer) "3");
	g_signal_connect (
		dedit, "time_changed",
		G_CALLBACK (on_time_changed), (gpointer) "3");
#else
	g_signal_connect (
		dedit, "changed",
		G_CALLBACK (on_changed), (gpointer) "3");
#endif

	button = gtk_button_new_with_label ("Print Date");
	gtk_table_attach (
		GTK_TABLE (table), button,
		1, 2, 2, 3, 0, 0, 0, 0);
	gtk_widget_show (button);
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (on_get_date_clicked), dedit);

	button = gtk_button_new_with_label ("Toggle 24-hour");
	gtk_table_attach (
		GTK_TABLE (table), button,
		2, 3, 2, 3, 0, 0, 0, 0);
	gtk_widget_show (button);
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (on_toggle_24_hour_clicked), dedit);

	gtk_widget_show (window);

	gtk_main ();

	e_util_cleanup_settings ();

	return 0;
}

static void
delete_event_cb (GtkWidget *widget,
                 GdkEventAny *event,
                 GtkWidget *window)
{
	gtk_widget_destroy (window);

	gtk_main_quit ();
}

static void
on_get_date_clicked (GtkWidget *button,
                     EDateEdit *dedit)
{
	time_t t;

	t = e_date_edit_get_time (dedit);
	if (t == -1)
		g_print ("Time: None\n");
	else
		g_print ("Time: %s", ctime (&t));

	if (!e_date_edit_date_is_valid (dedit))
		g_print ("  Date invalid\n");

	if (!e_date_edit_time_is_valid (dedit))
		g_print ("  Time invalid\n");
}

static void
on_toggle_24_hour_clicked (GtkWidget *button,
                           EDateEdit *dedit)
{
	gboolean use_24_hour_format;

	use_24_hour_format = e_date_edit_get_use_24_hour_format (dedit);
	e_date_edit_set_use_24_hour_format (dedit, !use_24_hour_format);
}

#if 0
static void
on_date_changed (EDateEdit *dedit,
                 gchar *name)
{
	gint year, month, day;

	if (e_date_edit_date_is_valid (dedit)) {
		if (e_date_edit_get_date (dedit, &year, &month, &day)) {
			g_print (
				"Date %s changed to: %i/%i/%i (M/D/Y)\n",
				name, month, day, year);
		} else {
			g_print ("Date %s changed to: None\n", name);
		}
	} else {
		g_print ("Date %s changed to: Not Valid\n", name);
	}
}

static void
on_time_changed (EDateEdit *dedit,
                 gchar *name)
{
	gint hour, minute;

	if (e_date_edit_time_is_valid (dedit)) {
		if (e_date_edit_get_time_of_day (dedit, &hour, &minute)) {
			g_print (
				"Time %s changed to: %02i:%02i\n", name,
				hour, minute);
		} else {
			g_print ("Time %s changed to: None\n", name);
		}
	} else {
		g_print ("Time %s changed to: Not Valid\n", name);
	}
}
#endif

static void
on_changed (EDateEdit *dedit,
            gchar *name)
{
	gint year, month, day, hour, minute;

	g_print ("Date %s changed ", name);

	if (e_date_edit_date_is_valid (dedit)) {
		if (e_date_edit_get_date (dedit, &year, &month, &day)) {
			g_print ("M/D/Y: %i/%i/%i", month, day, year);
		} else {
			g_print ("None");
		}
	} else {
		g_print ("Date Invalid");
	}

	if (e_date_edit_time_is_valid (dedit)) {
		if (e_date_edit_get_time_of_day (dedit, &hour, &minute)) {
			g_print (" %02i:%02i\n", hour, minute);
		} else {
			g_print (" None\n");
		}
	} else {
		g_print (" Time Invalid\n");
	}
}

