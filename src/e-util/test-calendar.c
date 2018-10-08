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
 * test-calendar - tests the ECalendar widget.
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <e-util/e-util.h>

/* Drag and Drop stuff. */
enum {
	TARGET_SHORTCUT
};

static GtkTargetEntry target_table[] = {
	{ (gchar *) "E-SHORTCUT", 0, TARGET_SHORTCUT }
};

static void on_date_range_changed	(ECalendarItem *calitem);
static void on_selection_changed	(ECalendarItem *calitem);

static void
delete_event_cb (GtkWidget *widget,
                 GdkEventAny *event,
                 gpointer data)
{
	gtk_main_quit ();
}

gint
main (gint argc,
      gchar **argv)
{
	GtkWidget *window;
	GtkWidget *cal;
	GtkWidget *vbox;
	ECalendarItem *calitem;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "ECalendar Test");
	gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
	gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);

	g_signal_connect (
		window, "delete_event",
		G_CALLBACK (delete_event_cb), NULL);

	cal = e_calendar_new ();
	e_calendar_set_minimum_size (E_CALENDAR (cal), 1, 1);
	calitem = e_calendar_get_item (E_CALENDAR (cal));
	gtk_widget_show (cal);

	g_signal_connect (
		calitem, "date_range_changed",
		G_CALLBACK (on_date_range_changed), NULL);
	g_signal_connect (
		calitem, "selection_changed",
		G_CALLBACK (on_selection_changed), NULL);

	gtk_drag_dest_set (
		cal,
		GTK_DEST_DEFAULT_ALL,
		target_table, G_N_ELEMENTS (target_table),
		GDK_ACTION_COPY | GDK_ACTION_MOVE);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (vbox), cal, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_widget_show (window);

	gtk_main ();

	e_misc_util_free_global_memory ();

	return 0;
}

static void
on_date_range_changed (ECalendarItem *calitem)
{
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;

	if (!e_calendar_item_get_date_range (
		calitem,
		&start_year, &start_month, &start_day,
		&end_year, &end_month, &end_day)) {
		g_warning ("%s: Failed to get date range", G_STRFUNC);
		return;
	}

	g_print (
		"Date range changed (D/M/Y): %i/%i/%i - %i/%i/%i\n",
		start_day, start_month + 1, start_year,
		end_day, end_month + 1, end_year);

	/* These days should windowear bold. Remember month is 0 to 11. */
	e_calendar_item_mark_day (
		calitem, 2000, 7, 26, /* 26th Aug 2000. */
		E_CALENDAR_ITEM_MARK_BOLD, FALSE);
	e_calendar_item_mark_day (
		calitem, 2000, 8, 13, /* 13th Sep 2000. */
		E_CALENDAR_ITEM_MARK_BOLD, FALSE);
}

static void
on_selection_changed (ECalendarItem *calitem)
{
	GDate start_date, end_date;

	g_warn_if_fail (e_calendar_item_get_selection (calitem, &start_date, &end_date));

	g_print (
		"Selection changed (D/M/Y): %i/%i/%i - %i/%i/%i\n",
		g_date_get_day (&start_date),
		g_date_get_month (&start_date),
		g_date_get_year (&start_date),
		g_date_get_day (&end_date),
		g_date_get_month (&end_date),
		g_date_get_year (&end_date));
}
