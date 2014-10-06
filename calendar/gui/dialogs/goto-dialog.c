/*
 * Go to date dialog for Evolution
 *
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
 *		Federico Mena <federico@ximian.com>
 *      JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 1998 Red Hat, Inc.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "goto-dialog.h"

#include "e-util/e-util.h"
#include "e-util/e-util-private.h"
#include "calendar/gui/calendar-config.h"
#include "calendar/gui/tag-calendar.h"

typedef struct
{
	GtkBuilder *builder;
	GtkWidget *dialog;

	GtkWidget *month_combobox;
	GtkWidget *year;
	ECalendar *ecal;
	GtkWidget *vbox;

	gint year_val;
	gint month_val;
	gint day_val;

	ETagCalendar *tag_calendar;

	ECalDataModel *data_model;
	ECalendarViewMoveType *out_move_type;
	time_t *out_exact_date;
} GoToDialog;

static GoToDialog *dlg = NULL;

/* Callback used when the year adjustment is changed */
static void
year_changed (GtkAdjustment *adj,
              gpointer data)
{
	GtkSpinButton *spin_button;
	GoToDialog *dlg = data;

	spin_button = GTK_SPIN_BUTTON (dlg->year);
	dlg->year_val = gtk_spin_button_get_value_as_int (spin_button);

	e_calendar_item_set_first_month (
		dlg->ecal->calitem, dlg->year_val, dlg->month_val);
}

/* Callback used when a month button is toggled */
static void
month_changed (GtkToggleButton *toggle,
               gpointer data)
{
	GtkComboBox *combo_box;
	GoToDialog *dlg = data;

	combo_box = GTK_COMBO_BOX (dlg->month_combobox);
	dlg->month_val = gtk_combo_box_get_active (combo_box);

	e_calendar_item_set_first_month (
		dlg->ecal->calitem, dlg->year_val, dlg->month_val);
}

/* Event handler for day groups in the month item.  A button press makes
 * the calendar jump to the selected day and destroys the Go-to dialog box. */
static void
ecal_event (ECalendarItem *calitem,
            gpointer user_data)
{
	GoToDialog *dlg = user_data;
	GDate start_date, end_date;
	struct icaltimetype tt = icaltime_null_time ();
	icaltimezone *timezone;
	time_t et;

	e_calendar_item_get_selection (calitem, &start_date, &end_date);
	timezone = e_cal_data_model_get_timezone (dlg->data_model);

	tt.year = g_date_get_year (&start_date);
	tt.month = g_date_get_month (&start_date);
	tt.day = g_date_get_day (&start_date);

	et = icaltime_as_timet_with_zone (tt, timezone);

	*(dlg->out_move_type) = E_CALENDAR_VIEW_MOVE_TO_EXACT_DAY;
	*(dlg->out_exact_date) = et;

	gtk_dialog_response (GTK_DIALOG (dlg->dialog), GTK_RESPONSE_APPLY);
}

/* Returns the current time, for the ECalendarItem. */
static struct tm
get_current_time (ECalendarItem *calitem,
                  gpointer data)
{
	icaltimezone *zone;
	struct tm tmp_tm = { 0 };
	struct icaltimetype tt;

	/* Get the current timezone. */
	zone = calendar_config_get_icaltimezone ();

	tt = icaltime_from_timet_with_zone (time (NULL), FALSE, zone);

	/* Now copy it to the struct tm and return it. */
	tmp_tm.tm_year = tt.year - 1900;
	tmp_tm.tm_mon = tt.month - 1;
	tmp_tm.tm_mday = tt.day;
	tmp_tm.tm_hour = tt.hour;
	tmp_tm.tm_min = tt.minute;
	tmp_tm.tm_sec = tt.second;
	tmp_tm.tm_isdst = -1;

	return tmp_tm;
}

/* Creates the ecalendar */
static void
create_ecal (GoToDialog *dlg)
{
	ECalendarItem *calitem;

	dlg->ecal = E_CALENDAR (e_calendar_new ());
	dlg->tag_calendar = e_tag_calendar_new (dlg->ecal);

	calitem = dlg->ecal->calitem;

	gnome_canvas_item_set (
		GNOME_CANVAS_ITEM (calitem),
		"move_selection_when_moving", FALSE,
		NULL);
	e_calendar_item_set_display_popup (calitem, FALSE);
	gtk_widget_show (GTK_WIDGET (dlg->ecal));
	gtk_box_pack_start (GTK_BOX (dlg->vbox), GTK_WIDGET (dlg->ecal), TRUE, TRUE, 0);

	e_calendar_item_set_first_month (calitem, dlg->year_val, dlg->month_val);
	e_calendar_item_set_get_time_callback (
		calitem,
		get_current_time,
		dlg, NULL);
}

/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (GoToDialog *dlg)
{
	#define GW(name) e_builder_get_widget (dlg->builder, name)

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

	g_signal_connect (
		dlg->month_combobox, "changed",
		G_CALLBACK (month_changed), dlg);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (dlg->year));
	g_signal_connect (
		adj, "value_changed",
		G_CALLBACK (year_changed), dlg);

	g_signal_connect (
		dlg->ecal->calitem, "selection_changed",
		G_CALLBACK (ecal_event), dlg);
}

/* Create a copy, thus a move to a distant date will not cause large event lookups */

/* Creates a "goto date" dialog and runs it */
gboolean
goto_dialog_run (GtkWindow *parent,
		 ECalDataModel *data_model,
		 const GDate *from_date,
		 ECalendarViewMoveType *out_move_type,
		 time_t *out_exact_date)
{
	gint response;

	if (dlg) {
		return FALSE;
	}

	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), FALSE);
	g_return_val_if_fail (out_move_type != NULL, FALSE);
	g_return_val_if_fail (out_exact_date != NULL, FALSE);

	dlg = g_new0 (GoToDialog, 1);

	/* Load the content widgets */
	dlg->builder = gtk_builder_new ();
	e_load_ui_builder_definition (dlg->builder, "goto-dialog.ui");

	if (!get_widgets (dlg)) {
		g_message ("goto_dialog_run(): Could not find all widgets in the XML file!");
		g_free (dlg);
		dlg = NULL;
		return FALSE;
	}

	dlg->data_model = e_cal_data_model_new_clone (data_model);
	dlg->out_move_type = out_move_type;
	dlg->out_exact_date = out_exact_date;

	if (from_date) {
		dlg->year_val = g_date_get_year (from_date);
		dlg->month_val = g_date_get_month (from_date) - 1;
		dlg->day_val = g_date_get_day (from_date);
	} else {
		struct icaltimetype tt;
		icaltimezone *timezone;

		timezone = e_cal_data_model_get_timezone (dlg->data_model);
		tt = icaltime_current_time_with_zone (timezone);

		dlg->year_val = tt.year;
		dlg->month_val = tt.month - 1;
		dlg->day_val = tt.day;
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (dlg->month_combobox), dlg->month_val);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (dlg->year), dlg->year_val);

	create_ecal (dlg);

	goto_dialog_init_widgets (dlg);

	gtk_window_set_transient_for (GTK_WINDOW (dlg->dialog), parent);

	/* set initial selection to current day */

	dlg->ecal->calitem->selection_set = TRUE;
	dlg->ecal->calitem->selection_start_month_offset = 0;
	dlg->ecal->calitem->selection_start_day = dlg->day_val;
	dlg->ecal->calitem->selection_end_month_offset = 0;
	dlg->ecal->calitem->selection_end_day = dlg->day_val;

	gnome_canvas_item_grab_focus (GNOME_CANVAS_ITEM (dlg->ecal->calitem));

	e_tag_calendar_subscribe (dlg->tag_calendar, dlg->data_model);

	response = gtk_dialog_run (GTK_DIALOG (dlg->dialog));

	e_tag_calendar_unsubscribe (dlg->tag_calendar, dlg->data_model);

	gtk_widget_destroy (dlg->dialog);

	if (response == GTK_RESPONSE_ACCEPT)
		*(dlg->out_move_type) = E_CALENDAR_VIEW_MOVE_TO_TODAY;

	g_object_unref (dlg->builder);
	g_clear_object (&dlg->tag_calendar);
	g_clear_object (&dlg->data_model);

	g_free (dlg);
	dlg = NULL;

	return response == GTK_RESPONSE_ACCEPT || response == GTK_RESPONSE_APPLY;
}
