/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Authors :
 *  Damon Chaplin <damon@ximian.com>
 *  Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright 2000, 2001, 2002 Ximian, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * CalPrefsDialog - a GtkObject which handles a libglade-loaded dialog
 * to edit the calendar preference settings.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "../e-timezone-entry.h"
#include "cal-prefs-dialog.h"
#include "../calendar-config.h"

#include <gtk/gtksignal.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtktogglebutton.h>
#include <libgnomeui/gnome-color-picker.h>
#include <glade/glade.h>
#include <gal/util/e-util.h>
#include <e-util/e-dialog-widgets.h>
#include <widgets/misc/e-dateedit.h>


struct _DialogData {
	/* Glade XML data */
	GladeXML *xml;

	GtkWidget *page;

	GtkWidget *timezone;
	GtkWidget *working_days[7];
	GtkWidget *week_start_day;
	GtkWidget *start_of_day;
	GtkWidget *end_of_day;
	GtkWidget *use_12_hour;
	GtkWidget *use_24_hour;
	GtkWidget *time_divisions;
	GtkWidget *show_end_times;
	GtkWidget *compress_weekend;
	GtkWidget *dnav_show_week_no;

	/* Widgets for the task list options */
	GtkWidget *tasks_due_today_color;
	GtkWidget *tasks_overdue_color;

	GtkWidget *tasks_hide_completed_checkbutton;
	GtkWidget *tasks_hide_completed_spinbutton;
	GtkWidget *tasks_hide_completed_optionmenu;

	/* Other page options */
	GtkWidget *confirm_delete;
	GtkWidget *default_reminder;
	GtkWidget *default_reminder_interval;
	GtkWidget *default_reminder_units;
};
typedef struct _DialogData DialogData;

static const int week_start_day_map[] = {
	1, 2, 3, 4, 5, 6, 0, -1
};

static const int time_division_map[] = {
	60, 30, 15, 10, 5, -1
};

/* The following two are kept separate in case we need to re-order each menu individually */
static const int hide_completed_units_map[] = {
	CAL_MINUTES, CAL_HOURS, CAL_DAYS, -1
};

static const int default_reminder_units_map[] = {
	CAL_MINUTES, CAL_HOURS, CAL_DAYS, -1
};


static gboolean get_widgets (DialogData *data);

static void widget_changed_callback (GtkWidget *, void *data);
static void connect_changed (GtkWidget *widget, const char *signal_name, EvolutionConfigControl *config_control);
static void setup_changes (DialogData *data, EvolutionConfigControl *config_control);

static void init_widgets (DialogData *data);
static void show_config (DialogData *data);
static void update_config (DialogData *dialog_data);

static void config_control_apply_callback (EvolutionConfigControl *config_control, void *data);
static void config_control_destroy_callback (GtkObject *object, void *data);

static void cal_prefs_dialog_use_24_hour_toggled(GtkWidget *button, void *data);
static void cal_prefs_dialog_end_of_day_changed (GtkWidget *button, void *data);
static void cal_prefs_dialog_start_of_day_changed (GtkWidget *button, void *data);
static void cal_prefs_dialog_hide_completed_tasks_toggled (GtkWidget *button, void *data);

GtkWidget *cal_prefs_dialog_create_time_edit (void);


/**
 * cal_prefs_dialog_new:
 *
 * Creates a new #CalPrefsDialog.
 *
 * Return value: a new #CalPrefsDialog.
 **/
EvolutionConfigControl *
cal_prefs_dialog_new (void)
{
	DialogData *dialog_data;
	EvolutionConfigControl *config_control;

	dialog_data = g_new0 (DialogData, 1);

	/* Load the content widgets */

	dialog_data->xml = glade_xml_new (EVOLUTION_GLADEDIR "/cal-prefs-dialog.glade", NULL, NULL);
	if (!dialog_data->xml) {
		g_message ("cal_prefs_dialog_construct(): Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (dialog_data)) {
		g_message ("cal_prefs_dialog_construct(): Could not find all widgets in the XML file!");
		return NULL;
	}

	init_widgets (dialog_data);
	show_config (dialog_data);

	gtk_widget_ref (dialog_data->page);
	gtk_container_remove (GTK_CONTAINER (dialog_data->page->parent), dialog_data->page);
	config_control = evolution_config_control_new (dialog_data->page);
	gtk_widget_unref (dialog_data->page);

	g_signal_connect((config_control), "apply",
			    G_CALLBACK (config_control_apply_callback), dialog_data);
	g_signal_connect((config_control), "destroy",
			    G_CALLBACK (config_control_destroy_callback), dialog_data);

	setup_changes (dialog_data, config_control);

	return config_control;
}

static void
widget_changed_callback (GtkWidget *widget,
			 void *data)
{
	EvolutionConfigControl *config_control;

	config_control = EVOLUTION_CONFIG_CONTROL (data);

	evolution_config_control_changed (config_control);
}

/* ^*&%!!#! GnomeColorPicker.  */
static void
color_set_callback (GnomeColorPicker *cp,
		    guint r,
		    guint g,
		    guint b,
		    guint a,
		    void *data)
{
	EvolutionConfigControl *config_control;

	config_control = EVOLUTION_CONFIG_CONTROL (data);

	evolution_config_control_changed (config_control);
}

static void
connect_changed (GtkWidget *widget,
		 const char *signal_name,
		 EvolutionConfigControl *config_control)
{
	g_signal_connect((widget), signal_name,
			    G_CALLBACK (widget_changed_callback), config_control);
}

static void
setup_changes (DialogData *dialog_data,
	       EvolutionConfigControl *config_control)
{
	int i;

	for (i = 0; i < 7; i ++)
		connect_changed (dialog_data->working_days[i], "toggled", config_control);

	connect_changed (dialog_data->timezone, "changed", config_control);

	connect_changed (dialog_data->start_of_day, "changed", config_control);
	connect_changed (dialog_data->end_of_day, "changed", config_control);

	connect_changed (GTK_OPTION_MENU (dialog_data->week_start_day)->menu, "selection_done", config_control);

	connect_changed (dialog_data->use_12_hour, "toggled", config_control);

	connect_changed (GTK_OPTION_MENU (dialog_data->time_divisions)->menu, "selection_done", config_control);

	connect_changed (dialog_data->show_end_times, "toggled", config_control);
	connect_changed (dialog_data->compress_weekend, "toggled", config_control);
	connect_changed (dialog_data->dnav_show_week_no, "toggled", config_control);

	connect_changed (dialog_data->tasks_hide_completed_checkbutton, "toggled", config_control);
	connect_changed (dialog_data->tasks_hide_completed_spinbutton, "changed", config_control);
	connect_changed (GTK_OPTION_MENU (dialog_data->tasks_hide_completed_optionmenu)->menu, "selection_done", config_control);

	connect_changed (dialog_data->confirm_delete, "toggled", config_control);
	connect_changed (dialog_data->default_reminder, "toggled", config_control);
	connect_changed (dialog_data->default_reminder_interval, "changed", config_control);
	connect_changed (GTK_OPTION_MENU (dialog_data->default_reminder_units)->menu, "selection_done", config_control);

	/* These use GnomeColorPicker so we have to use a different signal.  */
	g_signal_connect((dialog_data->tasks_due_today_color), "color_set",
			    G_CALLBACK (color_set_callback), config_control);
	g_signal_connect((dialog_data->tasks_overdue_color), "color_set",
			    G_CALLBACK (color_set_callback), config_control);
}

/* Gets the widgets from the XML file and returns if they are all available.
 */
static gboolean
get_widgets (DialogData *data)
{
#define GW(name) glade_xml_get_widget (data->xml, name)

	data->page = GW ("toplevel-notebook");

	/* The indices must be 0 (Sun) to 6 (Sat). */
	data->working_days[0] = GW ("sun_button");
	data->working_days[1] = GW ("mon_button");
	data->working_days[2] = GW ("tue_button");
	data->working_days[3] = GW ("wed_button");
	data->working_days[4] = GW ("thu_button");
	data->working_days[5] = GW ("fri_button");
	data->working_days[6] = GW ("sat_button");

	data->timezone = GW ("timezone");
	data->week_start_day = GW ("first_day_of_week");
	data->start_of_day = GW ("start_of_day");
	gtk_widget_show (data->start_of_day);
	data->end_of_day = GW ("end_of_day");
	gtk_widget_show (data->end_of_day);
	data->use_12_hour = GW ("use_12_hour");
	data->use_24_hour = GW ("use_24_hour");
	data->time_divisions = GW ("time_divisions");
	data->show_end_times = GW ("show_end_times");
	data->compress_weekend = GW ("compress_weekend");
	data->dnav_show_week_no = GW ("dnav_show_week_no");

	data->tasks_due_today_color = GW ("tasks_due_today_color");
	data->tasks_overdue_color = GW ("tasks_overdue_color");

	data->tasks_hide_completed_checkbutton = GW ("tasks-hide-completed-checkbutton");
	data->tasks_hide_completed_spinbutton = GW ("tasks-hide-completed-spinbutton");
	data->tasks_hide_completed_optionmenu = GW ("tasks-hide-completed-optionmenu");

	data->confirm_delete = GW ("confirm-delete");
	data->default_reminder = GW ("default-reminder");
	data->default_reminder_interval = GW ("default-reminder-interval");
	data->default_reminder_units = GW ("default-reminder-units");

#undef GW

	return (data->page
		&& data->timezone
		&& data->working_days[0]
		&& data->working_days[1]
		&& data->working_days[2]
		&& data->working_days[3]
		&& data->working_days[4]
		&& data->working_days[5]
		&& data->working_days[6]
		&& data->week_start_day
		&& data->start_of_day
		&& data->end_of_day
		&& data->use_12_hour
		&& data->use_24_hour
		&& data->time_divisions
		&& data->show_end_times
		&& data->compress_weekend
		&& data->dnav_show_week_no
		&& data->tasks_due_today_color
		&& data->tasks_overdue_color
		&& data->tasks_hide_completed_checkbutton
		&& data->tasks_hide_completed_spinbutton
		&& data->tasks_hide_completed_optionmenu
		&& data->confirm_delete
		&& data->default_reminder
		&& data->default_reminder_interval
		&& data->default_reminder_units);
}


static void
config_control_destroy_callback (GtkObject *object,
				 void *data)
{
	DialogData *dialog_data;

	dialog_data = (DialogData *) data;

	g_object_unref((dialog_data->xml));
	
	g_free (dialog_data);
}


static void
config_control_apply_callback (EvolutionConfigControl *control,
			       void *data)
{
	DialogData *dialog_data;

	dialog_data = (DialogData *) data;

	update_config (dialog_data);
}


/* Called by libglade to create our custom EDateEdit widgets. */
GtkWidget *
cal_prefs_dialog_create_time_edit (void)
{
	GtkWidget *dedit;

	dedit = e_date_edit_new ();

	e_date_edit_set_use_24_hour_format (E_DATE_EDIT (dedit), calendar_config_get_24_hour_format ());
	e_date_edit_set_time_popup_range (E_DATE_EDIT (dedit), 0, 24);
	e_date_edit_set_show_date (E_DATE_EDIT (dedit), FALSE);

	return dedit;
}


/* Connects any necessary signal handlers. */
static void
init_widgets (DialogData *dialog_data)
{
	g_signal_connect((dialog_data->use_24_hour), "toggled",
			    G_CALLBACK (cal_prefs_dialog_use_24_hour_toggled),
			    dialog_data);

	g_signal_connect((dialog_data->start_of_day), "changed",
			    G_CALLBACK (cal_prefs_dialog_start_of_day_changed),
			    dialog_data);

	g_signal_connect((dialog_data->end_of_day), "changed",
			    G_CALLBACK (cal_prefs_dialog_end_of_day_changed),
			    dialog_data);

	g_signal_connect((dialog_data->tasks_hide_completed_checkbutton),
			    "toggled",
			    G_CALLBACK (cal_prefs_dialog_hide_completed_tasks_toggled),
			    dialog_data);
}


static void
cal_prefs_dialog_use_24_hour_toggled (GtkWidget	*button,
				      void *data)
{
	DialogData *dialog_data;
	gboolean use_24_hour;

	dialog_data = (DialogData *) data;

	use_24_hour = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog_data->use_24_hour));

	e_date_edit_set_use_24_hour_format (E_DATE_EDIT (dialog_data->start_of_day), use_24_hour);
	e_date_edit_set_use_24_hour_format (E_DATE_EDIT (dialog_data->end_of_day), use_24_hour);
}

static void
cal_prefs_dialog_start_of_day_changed (GtkWidget *button, void *data)
{
	DialogData *dialog_data;
	EDateEdit *start, *end;
	int start_hour, start_minute, end_hour, end_minute;
	
	dialog_data = (DialogData *) data;

	start = E_DATE_EDIT (dialog_data->start_of_day);
	end = E_DATE_EDIT (dialog_data->end_of_day);
	
	e_date_edit_get_time_of_day (start, &start_hour, &start_minute);
	e_date_edit_get_time_of_day (end, &end_hour, &end_minute);

	if ((start_hour > end_hour) 
	    || (start_hour == end_hour && start_minute > end_minute)) {

		if (start_hour < 23)
			e_date_edit_set_time_of_day (end, start_hour + 1, start_minute);
		else
			e_date_edit_set_time_of_day (end, 23, 59);
	}
}

static void
cal_prefs_dialog_end_of_day_changed (GtkWidget *button, void *data)
{
	DialogData *dialog_data;
	EDateEdit *start, *end;
	int start_hour, start_minute, end_hour, end_minute;
	
	dialog_data = (DialogData *) data;

	start = E_DATE_EDIT (dialog_data->start_of_day);
	end = E_DATE_EDIT (dialog_data->end_of_day);
	
	e_date_edit_get_time_of_day (start, &start_hour, &start_minute);
	e_date_edit_get_time_of_day (end, &end_hour, &end_minute);

	if ((end_hour < start_hour) 
	    || (end_hour == start_hour && end_minute < start_minute)) {
		if (end_hour < 1)
			e_date_edit_set_time_of_day (start, 0, 0);
		else
			e_date_edit_set_time_of_day (start, end_hour - 1, end_minute);
	}
}

static void
cal_prefs_dialog_hide_completed_tasks_toggled	(GtkWidget *button,
						 void *data)
{
	DialogData *dialog_data;
	gboolean hide_completed_tasks;

	dialog_data = (DialogData *) data;

	hide_completed_tasks = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog_data->tasks_hide_completed_checkbutton));

	gtk_widget_set_sensitive (dialog_data->tasks_hide_completed_spinbutton, hide_completed_tasks);
	gtk_widget_set_sensitive (dialog_data->tasks_hide_completed_optionmenu, hide_completed_tasks);
}

/* Sets the color in a color picker from an X color spec */
static void
set_color_picker (GtkWidget *picker, const char *spec)
{
	GdkColor color;

	g_assert (spec != NULL);

	if (!gdk_color_parse (spec, &color)) {
		color.red = color.green = color.blue = 0;
		return;
	}

	gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (picker),
				    color.red,
				    color.green,
				    color.blue,
				    65535);
}

/* Shows the current task list settings in the dialog */
static void
show_task_list_config (DialogData *dialog_data)
{
	CalUnits units;
	gboolean hide_completed_tasks;

	set_color_picker (dialog_data->tasks_due_today_color, calendar_config_get_tasks_due_today_color ());
	set_color_picker (dialog_data->tasks_overdue_color, calendar_config_get_tasks_overdue_color ());

	/* Hide Completed Tasks. */
	hide_completed_tasks = calendar_config_get_hide_completed_tasks ();
	e_dialog_toggle_set (dialog_data->tasks_hide_completed_checkbutton,
			     hide_completed_tasks);

	/* Hide Completed Tasks Units. */
	units = calendar_config_get_hide_completed_tasks_units ();
	e_dialog_option_menu_set (dialog_data->tasks_hide_completed_optionmenu,
				  units, hide_completed_units_map);

	/* Hide Completed Tasks Value. */
	e_dialog_spin_set (dialog_data->tasks_hide_completed_spinbutton,
			   calendar_config_get_hide_completed_tasks_value ());

	gtk_widget_set_sensitive (dialog_data->tasks_hide_completed_spinbutton,
				  hide_completed_tasks);
	gtk_widget_set_sensitive (dialog_data->tasks_hide_completed_optionmenu,
				  hide_completed_tasks);
}

/* Shows the current config settings in the dialog. */
static void
show_config (DialogData *dialog_data)
{
	CalWeekdays working_days;
	gint mask, day, week_start_day, time_divisions;
	char *zone_name;
	icaltimezone *zone;
	gboolean sensitive;

	/* Timezone. */
	zone_name = calendar_config_get_timezone ();
	zone = icaltimezone_get_builtin_timezone (zone_name);
	e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (dialog_data->timezone),
				       zone);

	/* Working Days. */
	working_days = calendar_config_get_working_days ();
	mask = 1 << 0;
	for (day = 0; day < 7; day++) {
		e_dialog_toggle_set (dialog_data->working_days[day], (working_days & mask) ? TRUE : FALSE);
		mask <<= 1;
	}

	/* Week Start Day. */
	week_start_day = calendar_config_get_week_start_day ();
	e_dialog_option_menu_set (dialog_data->week_start_day, week_start_day,
				  week_start_day_map);

	/* Start of Day. */
	e_date_edit_set_time_of_day (E_DATE_EDIT (dialog_data->start_of_day),
				     calendar_config_get_day_start_hour (),
				     calendar_config_get_day_start_minute ());

	/* End of Day. */
	e_date_edit_set_time_of_day (E_DATE_EDIT (dialog_data->end_of_day),
				     calendar_config_get_day_end_hour (),
				     calendar_config_get_day_end_minute ());

	/* 12/24 Hour Format. */
	if (calendar_config_get_24_hour_format ())
		e_dialog_toggle_set (dialog_data->use_24_hour, TRUE);
	else
		e_dialog_toggle_set (dialog_data->use_12_hour, TRUE);

	sensitive = calendar_config_locale_supports_12_hour_format ();
	gtk_widget_set_sensitive (dialog_data->use_12_hour, sensitive);
	gtk_widget_set_sensitive (dialog_data->use_24_hour, sensitive);


	/* Time Divisions. */
	time_divisions = calendar_config_get_time_divisions ();
	e_dialog_option_menu_set (dialog_data->time_divisions, time_divisions,
				  time_division_map);

	/* Show Appointment End Times. */
	e_dialog_toggle_set (dialog_data->show_end_times, calendar_config_get_show_event_end ());

	/* Compress Weekend. */
	e_dialog_toggle_set (dialog_data->compress_weekend, calendar_config_get_compress_weekend ());

	/* Date Navigator - Show Week Numbers. */
	e_dialog_toggle_set (dialog_data->dnav_show_week_no, calendar_config_get_dnav_show_week_no ());

	/* Task list */

	show_task_list_config (dialog_data);

	/* Other page */

	e_dialog_toggle_set (dialog_data->confirm_delete, calendar_config_get_confirm_delete ());

	e_dialog_toggle_set (dialog_data->default_reminder,
			     calendar_config_get_use_default_reminder ());
	e_dialog_spin_set (dialog_data->default_reminder_interval,
			   calendar_config_get_default_reminder_interval ());
	e_dialog_option_menu_set (dialog_data->default_reminder_units,
				  calendar_config_get_default_reminder_units (),
				  default_reminder_units_map);
}

/* Returns a pointer to a static string with an X color spec for the current
 * value of a color picker.
 */
static const char *
spec_from_picker (GtkWidget *picker)
{
	static char spec[8];
	guint8 r, g, b;

	gnome_color_picker_get_i8 (GNOME_COLOR_PICKER (picker), &r, &g, &b, NULL);
	g_snprintf (spec, sizeof (spec), "#%02x%02x%02x", r, g, b);

	return spec;
}

/* Updates the task list config values from the settings in the dialog */
static void
update_task_list_config (DialogData *dialog_data)
{
	calendar_config_set_tasks_due_today_color (spec_from_picker (dialog_data->tasks_due_today_color));
	calendar_config_set_tasks_overdue_color (spec_from_picker (dialog_data->tasks_overdue_color));

	calendar_config_set_hide_completed_tasks (e_dialog_toggle_get (dialog_data->tasks_hide_completed_checkbutton));
	calendar_config_set_hide_completed_tasks_units (e_dialog_option_menu_get (dialog_data->tasks_hide_completed_optionmenu, hide_completed_units_map));
	calendar_config_set_hide_completed_tasks_value (e_dialog_spin_get_int (dialog_data->tasks_hide_completed_spinbutton));
}

/* Updates the config values based on the settings in the dialog. */
static void
update_config (DialogData *dialog_data)
{
	CalWeekdays working_days;
	gint mask, day, week_start_day, time_divisions, hour, minute;
	icaltimezone *zone;

	/* Timezone. */
	zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (dialog_data->timezone));
	calendar_config_set_timezone (icaltimezone_get_location (zone));

	/* Working Days. */
	working_days = 0;
	mask = 1 << 0;
	for (day = 0; day < 7; day++) {
		if (e_dialog_toggle_get (dialog_data->working_days[day]))
			working_days |= mask;
		mask <<= 1;
	}
	calendar_config_set_working_days (working_days);

	/* Week Start Day. */
	week_start_day = e_dialog_option_menu_get (dialog_data->week_start_day, week_start_day_map);
	calendar_config_set_week_start_day (week_start_day);

	/* Start of Day. */
	e_date_edit_get_time_of_day (E_DATE_EDIT (dialog_data->start_of_day), &hour, &minute);
	calendar_config_set_day_start_hour (hour);
	calendar_config_set_day_start_minute (minute);

	/* End of Day. */
	e_date_edit_get_time_of_day (E_DATE_EDIT (dialog_data->end_of_day), &hour, &minute);
	calendar_config_set_day_end_hour (hour);
	calendar_config_set_day_end_minute (minute);

	/* 12/24 Hour Format. */
	calendar_config_set_24_hour_format (e_dialog_toggle_get (dialog_data->use_24_hour));

	/* Time Divisions. */
	time_divisions = e_dialog_option_menu_get (dialog_data->time_divisions, time_division_map);
	calendar_config_set_time_divisions (time_divisions);

	/* Show Appointment End Times. */
	calendar_config_set_show_event_end (e_dialog_toggle_get (dialog_data->show_end_times));

	/* Compress Weekend. */
	calendar_config_set_compress_weekend (e_dialog_toggle_get (dialog_data->compress_weekend));

	/* Date Navigator - Show Week Numbers. */
	calendar_config_set_dnav_show_week_no (e_dialog_toggle_get (dialog_data->dnav_show_week_no));

	/* Task list */
	update_task_list_config (dialog_data);

	/* Other page */

	calendar_config_set_confirm_delete (e_dialog_toggle_get (dialog_data->confirm_delete));

	calendar_config_set_use_default_reminder (e_dialog_toggle_get (dialog_data->default_reminder));

	calendar_config_set_default_reminder_interval (
		e_dialog_spin_get_int (dialog_data->default_reminder_interval));

	calendar_config_set_default_reminder_units (
		e_dialog_option_menu_get (dialog_data->default_reminder_units, default_reminder_units_map));
}
