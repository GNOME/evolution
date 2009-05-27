/*
 * Go to date dialog for Evolution
 *
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
 *		Federico Mena <federico@ximian.com>
 *      JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 1998 Red Hat, Inc.
 *
 */

#include <config.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include "e-util/e-util-private.h"
#include "calendar-commands.h"
#include "calendar-config.h"
#include "tag-calendar.h"
#include "goto.h"

typedef struct
{
	GladeXML *xml;
	GtkWidget *dialog;

	GtkWidget *month_combobox;
	GtkWidget *year;
	ECalendar *ecal;
	GtkWidget *vbox;

	GnomeCalendar *gcal;
	gint year_val;
	gint month_val;
	gint day_val;

} GoToDialog;

static GoToDialog *dlg = NULL;

/* Callback used when the year adjustment is changed */
static void
year_changed (GtkAdjustment *adj, gpointer data)
{
	GoToDialog *dlg = data;

	dlg->year_val = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (dlg->year));
	e_calendar_item_set_first_month (dlg->ecal->calitem, dlg->year_val, dlg->month_val);
}

/* Callback used when a month button is toggled */
static void
month_changed (GtkToggleButton *toggle, gpointer data)
{
	GoToDialog *dlg = data;

	dlg->month_val = gtk_combo_box_get_active (GTK_COMBO_BOX (dlg->month_combobox));

	e_calendar_item_set_first_month (dlg->ecal->calitem, dlg->year_val, dlg->month_val);
}

static void
ecal_date_range_changed (ECalendarItem *calitem, gpointer user_data)
{
	GoToDialog *dlg = user_data;
	ECal *client;

	client = gnome_calendar_get_default_client (dlg->gcal);
	if (client)
		tag_calendar_by_client (dlg->ecal, client);
}

/* Event handler for day groups in the month item.  A button press makes the calendar jump to the
 * selected day and destroys the Go-to dialog box.
 */
static void
ecal_event (ECalendarItem *calitem, gpointer user_data)
{
	GoToDialog *dlg = user_data;
	GDate start_date, end_date;
	struct icaltimetype tt = icaltime_null_time ();
	time_t et;

	e_calendar_item_get_selection (calitem, &start_date, &end_date);

	tt.year = g_date_get_year (&start_date);
	tt.month = g_date_get_month (&start_date);
	tt.day = g_date_get_day (&start_date);

	et = icaltime_as_timet_with_zone (tt, gnome_calendar_get_timezone (dlg->gcal));

	gnome_calendar_goto (dlg->gcal, et);

	gtk_dialog_response (GTK_DIALOG (dlg->dialog), GTK_RESPONSE_NONE);
}

/* Returns the current time, for the ECalendarItem. */
static struct tm
get_current_time (ECalendarItem *calitem, gpointer data)
{
	icaltimezone *zone;
	struct tm tmp_tm = { 0 };
	struct icaltimetype tt;

	/* Get the current timezone. */
	zone = calendar_config_get_icaltimezone ();

	tt = icaltime_from_timet_with_zone (time (NULL), FALSE, zone);

	/* Now copy it to the struct tm and return it. */
	tmp_tm.tm_year  = tt.year - 1900;
	tmp_tm.tm_mon   = tt.month - 1;
	tmp_tm.tm_mday  = tt.day;
	tmp_tm.tm_hour  = tt.hour;
	tmp_tm.tm_min   = tt.minute;
	tmp_tm.tm_sec   = tt.second;
	tmp_tm.tm_isdst = -1;

	return tmp_tm;
}

/* Creates the ecalendar */
static void
create_ecal (GoToDialog *dlg)
{
	ECalendarItem *calitem;

	dlg->ecal = E_CALENDAR (e_calendar_new ());
	calitem = dlg->ecal->calitem;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (calitem),
			"move_selection_when_moving", FALSE,
			NULL);
	e_calendar_item_set_display_popup (calitem, FALSE);
	gtk_widget_show (GTK_WIDGET (dlg->ecal));
	gtk_box_pack_start (GTK_BOX (dlg->vbox), GTK_WIDGET (dlg->ecal), TRUE, TRUE, 0);

	e_calendar_item_set_first_month (calitem, dlg->year_val, dlg->month_val);
	e_calendar_item_set_get_time_callback (calitem,
					       get_current_time,
					       dlg, NULL);

	ecal_date_range_changed (calitem, dlg);
}

static void
goto_today (GoToDialog *dlg)
{
	gnome_calendar_goto_today (dlg->gcal);
}

/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (GoToDialog *dlg)
{
#define GW(name) glade_xml_get_widget (dlg->xml, name)

	dlg->dialog = GW ("goto-dialog");

	dlg->month_combobox = GW ("month-combobox");
	dlg->year = GW ("year");
	dlg->vbox = GW ("vbox");

#undef GW

	return (dlg->dialog
		&& dlg->month_combobox
		&& dlg->year
		&& dlg->vbox);
}

static void
goto_dialog_init_widgets (GoToDialog *dlg)
{
	GtkAdjustment *adj;

	g_signal_connect (dlg->month_combobox, "changed", G_CALLBACK (month_changed), dlg);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (dlg->year));
	g_signal_connect (adj, "value_changed", G_CALLBACK (year_changed), dlg);

	g_signal_connect (dlg->ecal->calitem, "date_range_changed", G_CALLBACK (ecal_date_range_changed), dlg);
	g_signal_connect (dlg->ecal->calitem, "selection_changed", G_CALLBACK (ecal_event), dlg);
}

/* Creates a "goto date" dialog and runs it */
void
goto_dialog (GnomeCalendar *gcal)
{
	time_t start_time;
	struct icaltimetype tt;
	gint b;
	gchar *gladefile;

	if (dlg) {
		return;
	}

	dlg = g_new0 (GoToDialog, 1);

	/* Load the content widgets */
	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "goto-dialog.glade",
				      NULL);
	dlg->xml = glade_xml_new (gladefile, NULL, NULL);
	g_free (gladefile);
	if (!dlg->xml) {
		g_message ("goto_dialog(): Could not load the Glade XML file!");
		g_free (dlg);
		return;
	}

	if (!get_widgets (dlg)) {
		g_message ("goto_dialog(): Could not find all widgets in the XML file!");
		g_free (dlg);
		return;
	}
	dlg->gcal = gcal;

	gnome_calendar_get_selected_time_range (dlg->gcal, &start_time, NULL);
	tt = icaltime_from_timet_with_zone (start_time, FALSE, gnome_calendar_get_timezone (gcal));
	dlg->year_val = tt.year;
	dlg->month_val = tt.month - 1;
	dlg->day_val = tt.day;

	gtk_combo_box_set_active (GTK_COMBO_BOX (dlg->month_combobox), dlg->month_val);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (dlg->year), dlg->year_val);

	create_ecal (dlg);

	goto_dialog_init_widgets (dlg);

	gtk_window_set_transient_for (GTK_WINDOW (dlg->dialog),
				      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))));

	/* set initial selection to current day */

	dlg->ecal->calitem->selection_set = TRUE;
	dlg->ecal->calitem->selection_start_month_offset = 0;
	dlg->ecal->calitem->selection_start_day = tt.day;
	dlg->ecal->calitem->selection_end_month_offset = 0;
	dlg->ecal->calitem->selection_end_day = tt.day;

	/* Set week_start_day. Convert it to 0 (Mon) to 6 (Sun), which is what we use. */
	dlg->ecal->calitem->week_start_day = (calendar_config_get_week_start_day () + 6) % 7;

	gnome_canvas_item_grab_focus (GNOME_CANVAS_ITEM (dlg->ecal->calitem));

	b = gtk_dialog_run (GTK_DIALOG (dlg->dialog));
	gtk_widget_destroy (dlg->dialog);

	if (b == 0)
		goto_today (dlg);

	g_object_unref (dlg->xml);
	g_free (dlg);
	dlg = NULL;
}
