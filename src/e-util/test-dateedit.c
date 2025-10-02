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
#include "e-split-date-edit.h"
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

static void
on_split_changed (ESplitDateEdit *split_date,
		  gpointer user_data)
{
	GtkLabel *label = user_data;
	guint year = 0, month = 0, day = 0;
	gchar buff[128];
	static gint change_counter = 0;

	e_split_date_edit_get_ymd (split_date, &year, &month, &day);
	change_counter++;

	g_warn_if_fail (g_snprintf (buff, sizeof (buff) - 1, "change[%u] year:%u month:%u day:%u", change_counter, year, month, day));
	gtk_label_set_text (label, buff);
}

static void
on_split_set_format_clicked (GtkButton *button,
			     gpointer user_data)
{
	ESplitDateEdit *split_date = user_data;
	GtkEntry *entry;

	entry = g_object_get_data (G_OBJECT (split_date), "format-entry");
	e_split_date_edit_set_format (split_date, gtk_entry_get_text (entry));
}

static void
on_split_set_date_clicked (GtkButton *button,
			   gpointer user_data)
{
	ESplitDateEdit *split_date = user_data;
	GtkEntry *entry;
	const gchar *text;
	guint year, month, day;

	entry = g_object_get_data (G_OBJECT (button), "year-entry");
	text = gtk_entry_get_text (entry);
	if (text)
		year = g_ascii_strtoull (text, NULL, 10);
	else
		year = 0;

	entry = g_object_get_data (G_OBJECT (button), "month-entry");
	text = gtk_entry_get_text (entry);
	if (text)
		month = g_ascii_strtoull (text, NULL, 10);
	else
		month = 0;

	entry = g_object_get_data (G_OBJECT (button), "day-entry");
	text = gtk_entry_get_text (entry);
	if (text)
		day = g_ascii_strtoull (text, NULL, 10);
	else
		day = 0;

	e_split_date_edit_set_ymd (split_date, year, month, day);
}

gint
main (gint argc,
      gchar **argv)
{
	GtkWidget *window;
	EDateEdit *dedit;
	ESplitDateEdit *split_date;
	GtkWidget *button, *widget;
	GtkGrid *grid;
	GtkBox *box;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "EDateEdit/ESPlitDateEdit Test");
	gtk_window_set_default_size (GTK_WINDOW (window), 300, 200);
	gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);

	g_signal_connect (
		window, "delete_event",
		G_CALLBACK (delete_event_cb), window);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 4);
	gtk_grid_set_column_spacing (grid, 4);

	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (grid));

	/* EDateEdit 1. */
	dedit = E_DATE_EDIT (e_date_edit_new ());
	gtk_grid_attach (grid, GTK_WIDGET (dedit), 0, 0, 1, 1);

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
	gtk_grid_attach (grid, button, 1, 0, 1, 1);
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (on_get_date_clicked), dedit);

	/* EDateEdit 2. */
	dedit = E_DATE_EDIT (e_date_edit_new ());
	gtk_grid_attach (grid, (GtkWidget *) dedit, 0, 1, 1, 1);
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
	gtk_grid_attach (grid, button, 1, 1, 1, 1);
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (on_get_date_clicked), dedit);

	/* EDateEdit 3. */
	dedit = E_DATE_EDIT (e_date_edit_new ());
	gtk_grid_attach (grid, (GtkWidget *) dedit, 0, 2, 1, 1);
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
	gtk_grid_attach (grid, button, 1, 2, 1, 1);
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (on_get_date_clicked), dedit);

	button = gtk_button_new_with_label ("Toggle 24-hour");
	gtk_grid_attach (grid, button, 2, 2, 1, 1);
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (on_toggle_24_hour_clicked), dedit);

	widget = gtk_label_new ("Split Date Edit:");
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_grid_attach (grid, widget, 0, 3, 3, 1);

	widget = e_split_date_edit_new ();
	split_date = E_SPLIT_DATE_EDIT (widget);
	gtk_grid_attach (grid, widget, 0, 4, 1, 1);

	widget = gtk_label_new ("");
	gtk_grid_attach (grid, widget, 1, 4, 2, 1);

	g_signal_connect (split_date, "changed", G_CALLBACK (on_split_changed), widget);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_grid_attach (grid, widget, 0, 5, 3, 1);
	box = GTK_BOX (widget);

	widget = gtk_label_new ("Format:");
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	widget = gtk_entry_new ();
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	gtk_entry_set_text (GTK_ENTRY (widget), e_split_date_edit_get_format (split_date));

	g_object_set_data (G_OBJECT (split_date), "format-entry", widget);

	widget = gtk_button_new_with_label ("Set format");
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (on_split_set_format_clicked), split_date);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_grid_attach (grid, widget, 0, 6, 3, 1);
	box = GTK_BOX (widget);

	widget = gtk_button_new_with_label ("Set date");
	g_signal_connect (widget, "clicked",
		G_CALLBACK (on_split_set_date_clicked), split_date);
	button = widget;

	widget = gtk_label_new ("Year:");
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	widget = gtk_entry_new ();
	gtk_entry_set_width_chars (GTK_ENTRY (widget), 5);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (button), "year-entry", widget);

	widget = gtk_label_new ("Month:");
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	widget = gtk_entry_new ();
	gtk_entry_set_width_chars (GTK_ENTRY (widget), 3);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (button), "month-entry", widget);

	widget = gtk_label_new ("Day:");
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	widget = gtk_entry_new ();
	gtk_entry_set_width_chars (GTK_ENTRY (widget), 3);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (button), "day-entry", widget);

	gtk_box_pack_start (box, button, FALSE, FALSE, 0);

	gtk_widget_show_all (GTK_WIDGET (grid));

	gtk_widget_show (window);

	gtk_main ();

	e_misc_util_free_global_memory ();

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

