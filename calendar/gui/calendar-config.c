/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2000, Ximian, Inc.
 * Copyright 2000, Ximian, Inc.
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
 * calendar-config.c - functions to load/save/get/set user settings.
 */

#include <config.h>
#include <time.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-config.h>
#include <libgnomeui/gnome-dialog.h>
#include <widgets/e-timezone-dialog/e-timezone-dialog.h>
#include "component-factory.h"
#include "calendar-commands.h"
#include "e-tasks.h"
#include "calendar-config.h"
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo-conf/bonobo-config-database.h>

typedef struct
{
	gchar	       *timezone;
	CalWeekdays	working_days;
	gboolean	use_24_hour_format;
	gint		week_start_day;
	gint		day_start_hour;
	gint		day_start_minute;
	gint		day_end_hour;
	gint		day_end_minute;
	gint		time_divisions;
	gboolean	dnav_show_week_no;
	gint		view;
	gfloat		hpane_pos;
	gfloat		vpane_pos;
	gfloat		month_hpane_pos;
	gfloat		month_vpane_pos;
	gboolean	compress_weekend;
	gboolean	show_event_end;
	char           *tasks_due_today_color;
	char           *tasks_overdue_color;
} CalendarConfig;


static CalendarConfig *config = NULL;

static void config_read			(void);

static void on_timezone_set		(GnomeDialog	*dialog,
					 int		 button,
					 ETimezoneDialog *etd);
static gboolean on_timezone_dialog_delete_event	(GnomeDialog	*dialog,
						 GdkEvent	*event,
						 ETimezoneDialog *etd);

void
calendar_config_init			(void)
{
	if (config)
		return;

	config = g_new0 (CalendarConfig, 1);

	config_read ();
}

static gboolean
locale_uses_24h_time_format (void)
{  
	char s[16];
	time_t t = 0;

	strftime (s, sizeof s, "%p", gmtime (&t));
	return s[0] == '\0';
}

static void
config_read				(void)
{
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	
	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		CORBA_exception_free (&ev);
		return;
 	}

	CORBA_exception_free (&ev);

	config->timezone =  bonobo_config_get_string (db, 
                "/Calendar/Display/Timezone", NULL);

 	config->working_days = bonobo_config_get_long_with_default (db, 
                "/Calendar/Display/WorkingDays", CAL_MONDAY | CAL_TUESDAY |
		CAL_WEDNESDAY | CAL_THURSDAY | CAL_FRIDAY, NULL);
 
	config->week_start_day = bonobo_config_get_long_with_default (db, 
                "/Calendar/Display/WeekStartDay", 1, NULL);
 
	config->use_24_hour_format = bonobo_config_get_boolean_with_default (
		db, "/Calendar/Display/Use24HourFormat", locale_uses_24h_time_format (), NULL);

	config->week_start_day = bonobo_config_get_long_with_default (db, 
                "/Calendar/Display/WeekStartDay", 1, NULL);
 
	config->day_start_hour = bonobo_config_get_long_with_default (db, 
                "/Calendar/Display/DayStartHour", 9, NULL);
 
	config->day_start_minute = bonobo_config_get_long_with_default (db, 
                "/Calendar/Display/DayStartMinute", 0, NULL);
 
	config->day_end_hour =  bonobo_config_get_long_with_default (db, 
                "/Calendar/Display/DayEndHour", 17, NULL);

	config->day_end_minute = bonobo_config_get_long_with_default (db, 
                "/Calendar/Display/DayEndMinute", 0, NULL);

	config->time_divisions = bonobo_config_get_long_with_default (db, 
                "/Calendar/Display/TimeDivisions", 30, NULL);

	config->view = bonobo_config_get_long_with_default (db, 
		"/Calendar/Display/View", 0, NULL);

	config->hpane_pos = bonobo_config_get_float_with_default (db, 
                "/Calendar/Display/HPanePosition", 1.0, NULL);

	config->vpane_pos = bonobo_config_get_float_with_default (db, 
                "/Calendar/Display/VPanePosition", 1.0, NULL);

	config->month_hpane_pos = bonobo_config_get_float_with_default (db, 
                "/Calendar/Display/MonthHPanePosition", 0.0, NULL);

	config->month_vpane_pos = bonobo_config_get_float_with_default (db, 
                "/Calendar/Display/MonthVPanePosition", 1.0, NULL);

	config->compress_weekend =  bonobo_config_get_boolean_with_default (db,
                "/Calendar/Display/CompressWeekend", TRUE, NULL);

	config->show_event_end =  bonobo_config_get_boolean_with_default (db, 
                "/Calendar/Display/ShowEventEndTime", TRUE, NULL);

	/* 'DateNavigator' settings. */

	config->dnav_show_week_no = bonobo_config_get_boolean_with_default (db,
                "/Calendar/DateNavigator/ShowWeekNumbers", FALSE, NULL);

	/* Task list settings */

	config->tasks_due_today_color = bonobo_config_get_string_with_default (
		db, "/Calendar/Tasks/Colors/TasksDueToday", "blue", NULL);

	config->tasks_overdue_color = bonobo_config_get_string_with_default (
		db, "/Calendar/Tasks/Colors/TasksOverdue", "red", NULL);

	bonobo_object_release_unref (db, NULL);
}


void
calendar_config_write			(void)
{
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;


	CORBA_exception_init (&ev);
	
	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		CORBA_exception_free (&ev);
		return;
 	}

	if (config->timezone)
		bonobo_config_set_string (db, "/Calendar/Display/Timezone", 
					  config->timezone, NULL);

	bonobo_config_set_long (db, "/Calendar/Display/WorkingDays", 
				config->working_days, NULL);
	bonobo_config_set_boolean (db, "/Calendar/Display/Use24HourFormat", 
				   config->use_24_hour_format, NULL);
	bonobo_config_set_long (db, "/Calendar/Display/WeekStartDay", 
				config->week_start_day, NULL);
	bonobo_config_set_long (db, "/Calendar/Display/DayStartHour", 
				config->day_start_hour, NULL);
	bonobo_config_set_long (db, "/Calendar/Display/DayStartMinute", 
				config->day_start_minute, NULL);
	bonobo_config_set_long (db, "/Calendar/Display/DayEndHour", 
				config->day_end_hour, NULL);
	bonobo_config_set_long (db, "/Calendar/Display/DayEndMinute", 
				config->day_end_minute, NULL);
	bonobo_config_set_boolean (db, "/Calendar/Display/CompressWeekend", 
				   config->compress_weekend, NULL);
	bonobo_config_set_boolean (db, "/Calendar/Display/ShowEventEndTime", 
				   config->show_event_end, NULL);

	bonobo_config_set_boolean (db, 
				   "/Calendar/DateNavigator/ShowWeekNumbers", 
				   config->dnav_show_week_no, NULL);

	bonobo_config_set_string (db, "/Calendar/Tasks/Colors/TasksDueToday", 
				  config->tasks_due_today_color, NULL);

	bonobo_config_set_string (db, "/Calendar/Tasks/Colors/TasksOverdue", 
				  config->tasks_overdue_color, NULL);

	Bonobo_ConfigDatabase_sync (db, &ev);
 
	bonobo_object_release_unref (db, NULL);

	CORBA_exception_free (&ev);
}

void
calendar_config_write_on_exit		(void)
{
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		CORBA_exception_free (&ev);
		return;
 	}

	bonobo_config_set_long (db, "/Calendar/Display/View", 
				config->view, NULL);
	bonobo_config_set_long (db, "/Calendar/Display/TimeDivisions", 
				config->time_divisions, NULL);
	bonobo_config_set_float (db, "/Calendar/Display/HPanePosition", 
				 config->hpane_pos, NULL);
	bonobo_config_set_float (db, "/Calendar/Display/VPanePosition", 
				 config->vpane_pos, NULL);
	bonobo_config_set_float (db, "/Calendar/Display/MonthHPanePosition", 
				 config->month_hpane_pos, NULL);
	bonobo_config_set_float (db, "/Calendar/Display/MonthVPanePosition", 
				 config->month_vpane_pos, NULL);

	Bonobo_ConfigDatabase_sync (db, &ev);
 
	bonobo_object_release_unref (db, NULL);

	CORBA_exception_free (&ev);
}


/*
 * Calendar Settings.
 */

/* The current timezone, e.g. "Europe/London". It may be NULL, in which case
   you should assume UTC (though Evolution will show the timezone-setting
   dialog the next time a calendar or task folder is selected). */
gchar*
calendar_config_get_timezone		(void)
{
	return config->timezone;
}


/* Sets the timezone. You shouldn't really set it to the empty string or NULL,
   as this means that Evolution will show the timezone-setting dialog to ask
   the user for the timezone. It copies the string. */
void
calendar_config_set_timezone		(gchar	     *timezone)
{
	g_free (config->timezone);

	if (timezone && timezone[0])
		config->timezone = g_strdup (timezone);
	else
		config->timezone = NULL;
}


/* Whether we use 24-hour format or 12-hour format (AM/PM). */
gboolean
calendar_config_get_24_hour_format	(void)
{
	return config->use_24_hour_format;
}


void
calendar_config_set_24_hour_format	(gboolean     use_24_hour)
{
	config->use_24_hour_format = use_24_hour;
}


/* The start day of the week (0 = Sun to 6 = Mon). */
gint
calendar_config_get_week_start_day	(void)
{
	return config->week_start_day;
}


void
calendar_config_set_week_start_day	(gint	      week_start_day)
{
	config->week_start_day = week_start_day;
}


/* The start and end times of the work-day. */
gint
calendar_config_get_day_start_hour	(void)
{
	return config->day_start_hour;
}


void
calendar_config_set_day_start_hour	(gint	      day_start_hour)
{
	config->day_start_hour = day_start_hour;
}


gint
calendar_config_get_day_start_minute	(void)
{
	return config->day_start_minute;
}


void
calendar_config_set_day_start_minute	(gint	      day_start_min)
{
	config->day_start_minute = day_start_min;
}


gint
calendar_config_get_day_end_hour	(void)
{
	return config->day_end_hour;
}


void
calendar_config_set_day_end_hour	(gint	      day_end_hour)
{
	config->day_end_hour = day_end_hour;
}


gint
calendar_config_get_day_end_minute	(void)
{
	return config->day_end_minute;
}


void
calendar_config_set_day_end_minute	(gint	      day_end_min)
{
	config->day_end_minute = day_end_min;
}


/* The time divisions in the Day/Work-Week view in minutes (5/10/15/30/60). */
gint
calendar_config_get_time_divisions	(void)
{
	return config->time_divisions;
}


void
calendar_config_set_time_divisions	(gint	      divisions)
{
	config->time_divisions = divisions;
}


/* Whether we show week numbers in the Date Navigator. */
gboolean
calendar_config_get_dnav_show_week_no	(void)
{
	return config->dnav_show_week_no;
}


void
calendar_config_set_dnav_show_week_no	(gboolean     show_week_no)
{
	config->dnav_show_week_no = show_week_no;
}


/* The view to show on start-up, 0 = Day, 1 = WorkWeek, 2 = Week, 3 = Month. */
gint
calendar_config_get_default_view	(void)
{
	return config->view;
}


void
calendar_config_set_default_view	(gint	      view)
{
	config->view = view;
}


/* The positions of the panes in the normal and month views. */
gfloat
calendar_config_get_hpane_pos		(void)
{
	return config->hpane_pos;
}


void
calendar_config_set_hpane_pos		(gfloat	      hpane_pos)
{
	config->hpane_pos = hpane_pos;
}


gfloat
calendar_config_get_vpane_pos		(void)
{
	return config->vpane_pos;
}


void
calendar_config_set_vpane_pos		(gfloat	      vpane_pos)
{
	config->vpane_pos = vpane_pos;
}


gfloat
calendar_config_get_month_hpane_pos	(void)
{
	return config->month_hpane_pos;
}


void
calendar_config_set_month_hpane_pos	(gfloat	      hpane_pos)
{
	config->month_hpane_pos = hpane_pos;
}


gfloat
calendar_config_get_month_vpane_pos	(void)
{
	return config->month_vpane_pos;
}


void
calendar_config_set_month_vpane_pos	(gfloat	      vpane_pos)
{
	config->month_vpane_pos = vpane_pos;
}


/* Whether we compress the weekend in the week/month views. */
gboolean
calendar_config_get_compress_weekend	(void)
{
	return config->compress_weekend;
}


void
calendar_config_set_compress_weekend	(gboolean     compress)
{
	config->compress_weekend = compress;
}


/* Whether we show event end times. */
gboolean
calendar_config_get_show_event_end	(void)
{
	return config->show_event_end;
}


void
calendar_config_set_show_event_end	(gboolean     show_end)
{
	config->show_event_end = show_end;
}


/* The working days of the week, a bit-wise combination of flags. */
CalWeekdays
calendar_config_get_working_days	(void)
{
	return config->working_days;
}


void
calendar_config_set_working_days	(CalWeekdays  days)
{
	config->working_days = days;
}


/* This sets all the common config settings for an ECalendar widget.
   These are the week start day, and whether we show week numbers. */
void
calendar_config_configure_e_calendar	(ECalendar	*cal)
{
	gboolean dnav_show_week_no;
	gint week_start_day;

	g_return_if_fail (E_IS_CALENDAR (cal));

	dnav_show_week_no = calendar_config_get_dnav_show_week_no ();

	/* Note that this is 0 (Sun) to 6 (Sat). */
	week_start_day = calendar_config_get_week_start_day ();

	/* Convert it to 0 (Mon) to 6 (Sun), which is what we use. */
	week_start_day = (week_start_day + 6) % 7;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (cal->calitem),
			       "show_week_numbers", dnav_show_week_no,
			       "week_start_day", week_start_day,
			       NULL);
}


/* This sets all the common config settings for an EDateEdit widget.
   These are the week start day, whether we show week numbers, and whether we
   use 24 hour format. */
void
calendar_config_configure_e_date_edit	(EDateEdit	*dedit)
{
	gboolean dnav_show_week_no, use_24_hour;
	gint week_start_day;

	g_return_if_fail (E_IS_DATE_EDIT (dedit));

	dnav_show_week_no = calendar_config_get_dnav_show_week_no ();

	/* Note that this is 0 (Sun) to 6 (Sat). */
	week_start_day = calendar_config_get_week_start_day ();

	/* Convert it to 0 (Mon) to 6 (Sun), which is what we use. */
	week_start_day = (week_start_day + 6) % 7;

	use_24_hour = calendar_config_get_24_hour_format ();

	e_date_edit_set_week_start_day (dedit, week_start_day);
	e_date_edit_set_show_week_numbers (dedit, dnav_show_week_no);
	e_date_edit_set_use_24_hour_format (dedit, use_24_hour);
}


/* This sets all the common config settings for an ECellDateEdit ETable item.
   These are the settings for the ECalendar popup and the time list (if we use
   24 hour format, and the hours of the working day). */
void
calendar_config_configure_e_cell_date_edit	(ECellDateEdit	*ecde)
{
	gboolean use_24_hour;
	gint start_hour, end_hour;

	g_return_if_fail (E_IS_CELL_DATE_EDIT (ecde));

	calendar_config_configure_e_calendar (E_CALENDAR (ecde->calendar));

	use_24_hour = calendar_config_get_24_hour_format ();

	start_hour = calendar_config_get_day_start_hour ();
	end_hour = calendar_config_get_day_end_hour ();
	/* Round up the end hour. */
	if (calendar_config_get_day_end_minute () != 0)
		end_hour = end_hour + 1 % 24;

	e_cell_date_edit_freeze (ecde);
	gtk_object_set (GTK_OBJECT (ecde),
			"use_24_hour_format", use_24_hour,
			"lower_hour", start_hour,
			"upper_hour", end_hour,
			NULL);
	e_cell_date_edit_thaw (ecde);
}


/* This sets all the common config settings for an ECalendarTable widget.
   These are the settings for the ECalendar popup and the time list (if we use
   24 hour format, and the hours of the working day). */
void
calendar_config_configure_e_calendar_table	(ECalendarTable	*cal_table)
{
	CalendarModel *model;
	gboolean use_24_hour;
	char *location;
	icaltimezone *zone;

	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	use_24_hour = calendar_config_get_24_hour_format ();

	model = e_calendar_table_get_model (cal_table);
	calendar_model_set_use_24_hour_format (model, use_24_hour);

	location = calendar_config_get_timezone ();
	zone = icaltimezone_get_builtin_timezone (location);
	calendar_model_set_timezone (model, zone);

	calendar_config_configure_e_cell_date_edit (cal_table->dates_cell);

	/* This is for changing the colors of the text; they will be re-fetched
	 * by ECellText when the table is redrawn.
	 */
	e_table_model_changed (E_TABLE_MODEL (model));
}



void
calendar_config_check_timezone_set ()
{
	ETimezoneDialog *timezone_dialog;
	GtkWidget *dialog;
	GList *elem;
	char *zone;

	zone = calendar_config_get_timezone ();
	if (zone && zone[0])
		return;

	/* Show timezone dialog. */
	timezone_dialog = e_timezone_dialog_new ();
	dialog = e_timezone_dialog_get_toplevel (timezone_dialog);

	/* Hide the cancel button, which is the 2nd button. */
	elem = g_list_nth (GNOME_DIALOG (dialog)->buttons, 1);
	gtk_widget_hide (elem->data);

	gtk_signal_connect (GTK_OBJECT (dialog), "clicked",
			    GTK_SIGNAL_FUNC (on_timezone_set),
			    timezone_dialog);
	gtk_signal_connect (GTK_OBJECT (dialog), "delete-event",
			    GTK_SIGNAL_FUNC (on_timezone_dialog_delete_event),
			    timezone_dialog);

	gtk_widget_show (dialog);
}


static void
on_timezone_set			(GnomeDialog	*dialog,
				 int		 button,
				 ETimezoneDialog *etd)
{
	char *display_name;

	e_timezone_dialog_get_timezone (etd, &display_name);

	g_print ("Location: %s\n", display_name ? display_name : "");

	if (display_name && display_name[0]) {
		calendar_config_set_timezone (display_name);

		calendar_config_write ();
		update_all_config_settings ();
		e_tasks_update_all_config_settings ();
	}

	gtk_object_unref (GTK_OBJECT (etd));
}


static gboolean
on_timezone_dialog_delete_event	(GnomeDialog	*dialog,
				 GdkEvent	*event,
				 ETimezoneDialog *etd)
{
	gtk_object_unref (GTK_OBJECT (etd));
	return TRUE;
}


/**
 * calendar_config_get_tasks_due_today_color:
 * 
 * Queries the color to be used to display tasks that are due today.
 * 
 * Return value: An X color specification.
 **/
const char *
calendar_config_get_tasks_due_today_color (void)
{
	g_assert (config->tasks_due_today_color != NULL);
	return config->tasks_due_today_color;
}

/**
 * calendar_config_set_tasks_due_today_color:
 * @color: X color specification
 * 
 * Sets the color to be used to display tasks that are due today.
 **/
void
calendar_config_set_tasks_due_today_color (const char *color)
{
	g_return_if_fail (color != NULL);

	g_assert (config->tasks_due_today_color != NULL);

	g_free (config->tasks_due_today_color);
	config->tasks_due_today_color = g_strdup (color);
}

/**
 * calendar_config_get_tasks_overdue_color:
 * 
 * Queries the color to be used to display overdue tasks.
 * 
 * Return value: An X color specification.
 **/
const char *
calendar_config_get_tasks_overdue_color (void)
{
	g_assert (config->tasks_overdue_color != NULL);
	return config->tasks_overdue_color;
}

/**
 * calendar_config_set_tasks_overdue_color:
 * @color: X color specification
 * 
 * Sets the color to be used to display overdue tasks.
 **/
void
calendar_config_set_tasks_overdue_color (const char *color)
{
	g_return_if_fail (color != NULL);

	g_assert (config->tasks_overdue_color != NULL);

	g_free (config->tasks_overdue_color);
	config->tasks_overdue_color = g_strdup (color);
}
