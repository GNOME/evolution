/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2000, Ximian, Inc.
 * Copyright 2000, Ximian, Inc.
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

#include <config.h>
#include <libgnomeui/gnome-color-picker.h>
#include <glade/glade.h>
#include <gal/util/e-util.h>
#include <e-util/e-dialog-widgets.h>
#include <widgets/misc/e-dateedit.h>
#include "../e-timezone-entry.h"
#include "cal-prefs-dialog.h"
#include "../calendar-config.h"
#include "../calendar-commands.h"
#include "../e-tasks.h"


struct _CalPrefsDialogPrivate {
	/* Glade XML data */
	GladeXML *xml;

	GtkWidget *dialog;

	GtkWidget *toplevel_notebook;

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

static const int week_start_day_map[] = {
	1, 2, 3, 4, 5, 6, 0, -1
};

static const int time_division_map[] = {
	60, 30, 15, 10, 5, -1
};

static const int hide_completed_units_map[] = {
	CAL_MINUTES, CAL_HOURS, CAL_DAYS, -1
};


static void cal_prefs_dialog_class_init		(CalPrefsDialogClass *class);
static void cal_prefs_dialog_init		(CalPrefsDialog	     *prefs);
static gboolean get_widgets			(CalPrefsDialog	     *prefs);
static void cal_prefs_dialog_destroy		(GtkObject	     *object);
static void cal_prefs_dialog_init_widgets	(CalPrefsDialog	     *prefs);
static void cal_prefs_dialog_button_clicked	(GtkWidget	     *dialog,
						 gint		      button,
						 CalPrefsDialog	     *prefs);
static void cal_prefs_dialog_use_24_hour_toggled(GtkWidget	     *button,
						 CalPrefsDialog	     *prefs);
static void cal_prefs_dialog_hide_completed_tasks_toggled (GtkWidget	*button,
							   CalPrefsDialog *prefs);
static void cal_prefs_dialog_show_config	(CalPrefsDialog	     *prefs);
static void cal_prefs_dialog_update_config	(CalPrefsDialog	     *prefs);

GtkWidget*  cal_prefs_dialog_create_time_edit	(void);

static GtkObjectClass *parent_class;

E_MAKE_TYPE (cal_prefs_dialog, "CalPrefsDialog", CalPrefsDialog,
	     cal_prefs_dialog_class_init, cal_prefs_dialog_init,
	     GTK_TYPE_OBJECT)


static void
cal_prefs_dialog_class_init (CalPrefsDialogClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

	object_class->destroy = cal_prefs_dialog_destroy;
}


static void
cal_prefs_dialog_init (CalPrefsDialog *prefs)
{
	CalPrefsDialogPrivate *priv;

	priv = g_new0 (CalPrefsDialogPrivate, 1);
	prefs->priv = priv;

}


/**
 * cal_prefs_dialog_new:
 * @page: Page to show when the dialog is popped up.
 *
 * Creates a new #CalPrefsDialog.
 *
 * Return value: a new #CalPrefsDialog.
 **/
CalPrefsDialog *
cal_prefs_dialog_new (CalPrefsDialogPage page)
{
	CalPrefsDialog *prefs;

	prefs = CAL_PREFS_DIALOG (gtk_type_new (cal_prefs_dialog_get_type ()));
	return cal_prefs_dialog_construct (prefs, page);
}


/**
 * cal_prefs_dialog_construct:
 * @prefs: A #CalPrefsDialog.
 * @page: Page to show when the dialog is popped up.
 * 
 * Constructs a task editor by loading its Glade XML file.
 * 
 * Return value: The same object as @prefs, or NULL if the widgets could not be
 * created.  In the latter case, the task editor will automatically be
 * destroyed.
 **/
CalPrefsDialog *
cal_prefs_dialog_construct (CalPrefsDialog *prefs, CalPrefsDialogPage page)
{
	CalPrefsDialogPrivate *priv;

	g_return_val_if_fail (IS_CAL_PREFS_DIALOG (prefs), NULL);

	priv = prefs->priv;

	/* Load the content widgets */

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/cal-prefs-dialog.glade", NULL);
	if (!priv->xml) {
		g_message ("cal_prefs_dialog_construct(): Could not load the Glade XML file!");
		goto error;
	}

	if (!get_widgets (prefs)) {
		g_message ("cal_prefs_dialog_construct(): Could not find all widgets in the XML file!");
		goto error;
	}

	cal_prefs_dialog_init_widgets (prefs);

	cal_prefs_dialog_show_config (prefs);

	cal_prefs_dialog_show (prefs, page);

	return prefs;

 error:

	gtk_object_unref (GTK_OBJECT (prefs));
	return NULL;
}


/* Gets the widgets from the XML file and returns if they are all available.
 */
static gboolean
get_widgets (CalPrefsDialog *prefs)
{
	CalPrefsDialogPrivate *priv;

	priv = prefs->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->dialog = GW ("cal-prefs-dialog");

	priv->toplevel_notebook = GW ("toplevel-notebook");

	/* The indices must be 0 (Sun) to 6 (Sat). */
	priv->working_days[0] = GW ("sun_button");
	priv->working_days[1] = GW ("mon_button");
	priv->working_days[2] = GW ("tue_button");
	priv->working_days[3] = GW ("wed_button");
	priv->working_days[4] = GW ("thu_button");
	priv->working_days[5] = GW ("fri_button");
	priv->working_days[6] = GW ("sat_button");

	priv->timezone = GW ("timezone");
	priv->week_start_day = GW ("first_day_of_week");
	priv->start_of_day = GW ("start_of_day");
	priv->end_of_day = GW ("end_of_day");
	priv->use_12_hour = GW ("use_12_hour");
	priv->use_24_hour = GW ("use_24_hour");
	priv->time_divisions = GW ("time_divisions");
	priv->show_end_times = GW ("show_end_times");
	priv->compress_weekend = GW ("compress_weekend");
	priv->dnav_show_week_no = GW ("dnav_show_week_no");

	priv->tasks_due_today_color = GW ("tasks_due_today_color");
	priv->tasks_overdue_color = GW ("tasks_overdue_color");

	priv->tasks_hide_completed_checkbutton = GW ("tasks-hide-completed-checkbutton");
	priv->tasks_hide_completed_spinbutton = GW ("tasks-hide-completed-spinbutton");
	priv->tasks_hide_completed_optionmenu = GW ("tasks-hide-completed-optionmenu");

	priv->confirm_delete = GW ("confirm-delete");
	priv->default_reminder = GW ("default-reminder");
	priv->default_reminder_interval = GW ("default-reminder-interval");
	priv->default_reminder_units = GW ("default-reminder-units");

#undef GW

	return (priv->dialog
		&& priv->toplevel_notebook
		&& priv->timezone
		&& priv->working_days[0]
		&& priv->working_days[1]
		&& priv->working_days[2]
		&& priv->working_days[3]
		&& priv->working_days[4]
		&& priv->working_days[5]
		&& priv->working_days[6]
		&& priv->week_start_day
		&& priv->start_of_day
		&& priv->end_of_day
		&& priv->use_12_hour
		&& priv->use_24_hour
		&& priv->time_divisions
		&& priv->show_end_times
		&& priv->compress_weekend
		&& priv->dnav_show_week_no
		&& priv->tasks_due_today_color
		&& priv->tasks_overdue_color
		&& priv->tasks_hide_completed_checkbutton
		&& priv->tasks_hide_completed_spinbutton
		&& priv->tasks_hide_completed_optionmenu
		&& priv->confirm_delete
		&& priv->default_reminder
		&& priv->default_reminder_interval
		&& priv->default_reminder_units);
}


static void
cal_prefs_dialog_destroy (GtkObject *object)
{
	CalPrefsDialog *prefs;
	CalPrefsDialogPrivate *priv;

	g_return_if_fail (IS_CAL_PREFS_DIALOG (object));

	prefs = CAL_PREFS_DIALOG (object);
	priv = prefs->priv;


	g_free (priv);
	prefs->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* Called by libglade to create our custom EDateEdit widgets. */
GtkWidget *
cal_prefs_dialog_create_time_edit (void)
{
	GtkWidget *dedit;

	dedit = e_date_edit_new ();

	e_date_edit_set_time_popup_range (E_DATE_EDIT (dedit), 0, 24);
	e_date_edit_set_show_date (E_DATE_EDIT (dedit), FALSE);

	return dedit;
}


void
cal_prefs_dialog_show (CalPrefsDialog *prefs, CalPrefsDialogPage page)
{
	CalPrefsDialogPrivate *priv;
	int page_num;

	g_return_if_fail (IS_CAL_PREFS_DIALOG (prefs));

	priv = prefs->priv;

	/* If the dialog is already show just raise it, otherwise refresh the
	   config settings and show it. */
	if (GTK_WIDGET_MAPPED (priv->dialog)) {
		gdk_window_raise (priv->dialog->window);
	} else {
		cal_prefs_dialog_show_config (prefs);
		gtk_widget_show (priv->dialog);
	}

	switch (page) {
	case CAL_PREFS_DIALOG_PAGE_CALENDAR:
		page_num = 0;
		break;

	case CAL_PREFS_DIALOG_PAGE_TASKS:
		page_num = 2;
		break;

	default:
		g_assert_not_reached ();
		return;
	}

	gtk_notebook_set_page (GTK_NOTEBOOK (priv->toplevel_notebook), page_num);
}


/* Connects any necessary signal handlers. */
static void
cal_prefs_dialog_init_widgets	(CalPrefsDialog	*prefs)
{
	CalPrefsDialogPrivate *priv;

	priv = prefs->priv;

	gtk_signal_connect (GTK_OBJECT (priv->dialog), "clicked",
			    GTK_SIGNAL_FUNC (cal_prefs_dialog_button_clicked),
			    prefs);

	gtk_signal_connect (GTK_OBJECT (priv->use_24_hour), "toggled",
			    GTK_SIGNAL_FUNC (cal_prefs_dialog_use_24_hour_toggled),
			    prefs);

	gtk_signal_connect (GTK_OBJECT (priv->tasks_hide_completed_checkbutton),
			    "toggled",
			    GTK_SIGNAL_FUNC (cal_prefs_dialog_hide_completed_tasks_toggled),
			    prefs);
}


static void
cal_prefs_dialog_button_clicked	(GtkWidget	*dialog,
				 gint		 button,
				 CalPrefsDialog *prefs)
{
	CalPrefsDialogPrivate *priv;

	g_return_if_fail (IS_CAL_PREFS_DIALOG (prefs));

	priv = prefs->priv;

	/* OK & Apply buttons update the config settings. */
	if (button == 0 || button == 1)
		cal_prefs_dialog_update_config	(prefs);

	/* OK & Close buttons close the dialog. */
	if (button == 0 || button == 2)
		gtk_widget_hide (priv->dialog);

	/* FIXME: Handle button 3 (Help). */

}


static void
cal_prefs_dialog_use_24_hour_toggled	(GtkWidget	*button,
					 CalPrefsDialog *prefs)
{
	CalPrefsDialogPrivate *priv;
	gboolean use_24_hour;

	priv = prefs->priv;

	use_24_hour = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->use_24_hour));

	e_date_edit_set_use_24_hour_format (E_DATE_EDIT (priv->start_of_day),
					    use_24_hour);
	e_date_edit_set_use_24_hour_format (E_DATE_EDIT (priv->end_of_day),
					    use_24_hour);
}

static void
cal_prefs_dialog_hide_completed_tasks_toggled	(GtkWidget	*button,
						 CalPrefsDialog *prefs)
{
	CalPrefsDialogPrivate *priv;
	gboolean hide_completed_tasks;

	priv = prefs->priv;

	hide_completed_tasks = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->tasks_hide_completed_checkbutton));

	gtk_widget_set_sensitive (priv->tasks_hide_completed_spinbutton,
				  hide_completed_tasks);
	gtk_widget_set_sensitive (priv->tasks_hide_completed_optionmenu,
				  hide_completed_tasks);
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
show_task_list_config (CalPrefsDialog *prefs)
{
	CalPrefsDialogPrivate *priv;
	CalUnits units;
	gboolean hide_completed_tasks;

	priv = prefs->priv;

	set_color_picker (priv->tasks_due_today_color, calendar_config_get_tasks_due_today_color ());
	set_color_picker (priv->tasks_overdue_color, calendar_config_get_tasks_overdue_color ());

	/* Hide Completed Tasks. */
	hide_completed_tasks = calendar_config_get_hide_completed_tasks ();
	e_dialog_toggle_set (priv->tasks_hide_completed_checkbutton,
			     hide_completed_tasks);

	/* Hide Completed Tasks Units. */
	units = calendar_config_get_hide_completed_tasks_units ();
	e_dialog_option_menu_set (priv->tasks_hide_completed_optionmenu,
				  units, hide_completed_units_map);

	/* Hide Completed Tasks Value. */
	e_dialog_spin_set (priv->tasks_hide_completed_spinbutton,
			   calendar_config_get_hide_completed_tasks_value ());

	gtk_widget_set_sensitive (priv->tasks_hide_completed_spinbutton,
				  hide_completed_tasks);
	gtk_widget_set_sensitive (priv->tasks_hide_completed_optionmenu,
				  hide_completed_tasks);
}

/* Shows the current config settings in the dialog. */
static void
cal_prefs_dialog_show_config	(CalPrefsDialog	*prefs)
{
	CalPrefsDialogPrivate *priv;
	CalWeekdays working_days;
	gint mask, day, week_start_day, time_divisions;
	char *zone_name;
	icaltimezone *zone;
	gboolean sensitive;

	priv = prefs->priv;

	/* Timezone. */
	zone_name = calendar_config_get_timezone ();
	zone = icaltimezone_get_builtin_timezone (zone_name);
	e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (priv->timezone),
				       zone);

	/* Working Days. */
	working_days = calendar_config_get_working_days ();
	mask = 1 << 0;
	for (day = 0; day < 7; day++) {
		e_dialog_toggle_set (priv->working_days[day], (working_days & mask) ? TRUE : FALSE);
		mask <<= 1;
	}

	/* Week Start Day. */
	week_start_day = calendar_config_get_week_start_day ();
	e_dialog_option_menu_set (priv->week_start_day, week_start_day,
				  week_start_day_map);

	/* Start of Day. */
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->start_of_day),
				     calendar_config_get_day_start_hour (),
				     calendar_config_get_day_start_minute ());

	/* End of Day. */
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->end_of_day),
				     calendar_config_get_day_end_hour (),
				     calendar_config_get_day_end_minute ());

	/* 12/24 Hour Format. */
	if (calendar_config_get_24_hour_format ())
		e_dialog_toggle_set (priv->use_24_hour, TRUE);
	else
		e_dialog_toggle_set (priv->use_12_hour, TRUE);

	sensitive = calendar_config_locale_supports_12_hour_format ();
	gtk_widget_set_sensitive (priv->use_12_hour, sensitive);
	gtk_widget_set_sensitive (priv->use_24_hour, sensitive);


	/* Time Divisions. */
	time_divisions = calendar_config_get_time_divisions ();
	e_dialog_option_menu_set (priv->time_divisions, time_divisions,
				  time_division_map);

	/* Show Appointment End Times. */
	e_dialog_toggle_set (priv->show_end_times, calendar_config_get_show_event_end ());

	/* Compress Weekend. */
	e_dialog_toggle_set (priv->compress_weekend, calendar_config_get_compress_weekend ());

	/* Date Navigator - Show Week Numbers. */
	e_dialog_toggle_set (priv->dnav_show_week_no, calendar_config_get_dnav_show_week_no ());

	/* Task list */

	show_task_list_config (prefs);

	/* Other page */

	e_dialog_toggle_set (priv->confirm_delete, calendar_config_get_confirm_delete ());
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
update_task_list_config (CalPrefsDialog *prefs)
{
	CalPrefsDialogPrivate *priv;

	priv = prefs->priv;

	calendar_config_set_tasks_due_today_color (spec_from_picker (priv->tasks_due_today_color));
	calendar_config_set_tasks_overdue_color (spec_from_picker (priv->tasks_overdue_color));

	calendar_config_set_hide_completed_tasks (e_dialog_toggle_get (priv->tasks_hide_completed_checkbutton));
	calendar_config_set_hide_completed_tasks_units (e_dialog_option_menu_get (priv->tasks_hide_completed_optionmenu, hide_completed_units_map));
	calendar_config_set_hide_completed_tasks_value (e_dialog_spin_get_int (priv->tasks_hide_completed_spinbutton));
}

/* Updates the config values based on the settings in the dialog. */
static void
cal_prefs_dialog_update_config	(CalPrefsDialog	*prefs)
{
	CalPrefsDialogPrivate *priv;
	CalWeekdays working_days;
	gint mask, day, week_start_day, time_divisions, hour, minute;
	icaltimezone *zone;

	priv = prefs->priv;

	/* Timezone. */
	zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->timezone));
	calendar_config_set_timezone (icaltimezone_get_location (zone));

	/* Working Days. */
	working_days = 0;
	mask = 1 << 0;
	for (day = 0; day < 7; day++) {
		if (e_dialog_toggle_get (priv->working_days[day]))
			working_days |= mask;
		mask <<= 1;
	}
	calendar_config_set_working_days (working_days);

	/* Week Start Day. */
	week_start_day = e_dialog_option_menu_get (priv->week_start_day, week_start_day_map);
	calendar_config_set_week_start_day (week_start_day);

	/* Start of Day. */
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_of_day), &hour, &minute);
	calendar_config_set_day_start_hour (hour);
	calendar_config_set_day_start_minute (minute);

	/* End of Day. */
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->end_of_day), &hour, &minute);
	calendar_config_set_day_end_hour (hour);
	calendar_config_set_day_end_minute (minute);

	/* 12/24 Hour Format. */
	calendar_config_set_24_hour_format (e_dialog_toggle_get (priv->use_24_hour));

	/* Time Divisions. */
	time_divisions = e_dialog_option_menu_get (priv->time_divisions, time_division_map);
	calendar_config_set_time_divisions (time_divisions);

	/* Show Appointment End Times. */
	calendar_config_set_show_event_end (e_dialog_toggle_get (priv->show_end_times));

	/* Compress Weekend. */
	calendar_config_set_compress_weekend (e_dialog_toggle_get (priv->compress_weekend));

	/* Date Navigator - Show Week Numbers. */
	calendar_config_set_dnav_show_week_no (e_dialog_toggle_get (priv->dnav_show_week_no));

	/* Task list */
	update_task_list_config (prefs);

	/* Other page */

	calendar_config_set_confirm_delete (e_dialog_toggle_get (priv->confirm_delete));

	/* Done */

	calendar_config_write ();
	update_all_config_settings ();
	e_tasks_update_all_config_settings ();
}
