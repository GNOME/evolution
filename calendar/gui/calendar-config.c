/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Authors :
 *  Damon Chaplin <damon@ximian.com>
 *  Rodrigo Moya <rodrigo@ximian.com>
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
 * calendar-config.c - functions to load/save/get/set user settings.
 */

#include <config.h>
#include <time.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-config.h>
#include <libgnomeui/gnome-dialog.h>
#include <widgets/e-timezone-dialog/e-timezone-dialog.h>
#include <cal-util/timeutil.h>
#include "component-factory.h"
#include "calendar-commands.h"
#include "e-tasks.h"
#include "e-cell-date-edit-text.h"
#include "calendar-config.h"
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo-conf/bonobo-config-database.h>

typedef struct
{
	gchar          *default_uri;
	gchar          *default_tasks_uri;
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
	gboolean	hide_completed_tasks;
	CalUnits	hide_completed_tasks_units;
	gint		hide_completed_tasks_value;
	gboolean	confirm_delete;
	gboolean        use_default_reminder;
	int             default_reminder_interval;
	CalUnits        default_reminder_units;
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

/* Returns TRUE if the locale has 'am' and 'pm' strings defined, in which
   case the user can choose between 12 and 24-hour time formats. */
gboolean
calendar_config_locale_supports_12_hour_format (void)
{
	char s[16];
	time_t t = 0;

	strftime (s, sizeof s, "%p", gmtime (&t));
	return s[0] != '\0';
}

static void
config_read				(void)
{
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;
	char *units;

	CORBA_exception_init (&ev);

	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);

	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		CORBA_exception_free (&ev);
		return;
 	}

	CORBA_exception_free (&ev);

	CORBA_exception_init (&ev);
	config->default_uri = bonobo_config_get_string (db,
		"/Calendar/DefaultUri", &ev);
	if (BONOBO_USER_EX (&ev, ex_Bonobo_ConfigDatabase_NotFound))
		config->default_uri = NULL;
	else if (BONOBO_EX (&ev))
		g_message ("config_read(): Could not get the /Calendar/DefaultUri");

	CORBA_exception_free (&ev);

	CORBA_exception_init (&ev);
	config->default_tasks_uri = bonobo_config_get_string (db,
		"/Calendar/DefaultTasksUri", &ev);
	if (BONOBO_USER_EX (&ev, ex_Bonobo_ConfigDatabase_NotFound))
		config->default_tasks_uri = NULL;
	else if (BONOBO_EX (&ev))
		g_message ("config_read(): Could not get the /Calendar/DefaultTasksUri");

	CORBA_exception_free (&ev);

	/* Default to UTC if the timezone is not set or is "". */
	config->timezone =  bonobo_config_get_string_with_default (db,
                "/Calendar/Display/Timezone", "UTC", NULL);
	if (!config->timezone || !config->timezone[0]) {
		g_free (config->timezone);
		config->timezone = g_strdup ("UTC");
	}

 	config->working_days = bonobo_config_get_long_with_default (db,
                "/Calendar/Display/WorkingDays", CAL_MONDAY | CAL_TUESDAY |
		CAL_WEDNESDAY | CAL_THURSDAY | CAL_FRIDAY, NULL);

	config->week_start_day = bonobo_config_get_long_with_default (db,
                "/Calendar/Display/WeekStartDay", 1, NULL);

	/* If the locale defines 'am' and 'pm' strings then the user has the
	   choice of 12-hour or 24-hour time format, with 12-hour as the
	   default. If the locale doesn't have 'am' and 'pm' strings we have
	   to use 24-hour format, or strftime()/strptime() won't work. */
	if (calendar_config_locale_supports_12_hour_format ()) {
		config->use_24_hour_format = bonobo_config_get_boolean_with_default (db, "/Calendar/Display/Use24HourFormat", FALSE, NULL);
	} else {
		config->use_24_hour_format = TRUE;
	}

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

	config->hide_completed_tasks = bonobo_config_get_boolean_with_default (
		db, "/Calendar/Tasks/HideCompletedTasks", FALSE, NULL);

	units = bonobo_config_get_string_with_default (db,
		"/Calendar/Tasks/HideCompletedTasksUnits", "days", NULL);

	if (!strcmp (units, "minutes"))
		config->hide_completed_tasks_units = CAL_MINUTES;
	else if (!strcmp (units, "hours"))
		config->hide_completed_tasks_units = CAL_HOURS;
	else
		config->hide_completed_tasks_units = CAL_DAYS;

	g_free (units);

	config->hide_completed_tasks_value = bonobo_config_get_long_with_default (
		db, "/Calendar/Tasks/HideCompletedTasksValue", 1, NULL);

	/* Confirmation */
	config->confirm_delete = bonobo_config_get_boolean_with_default (
		db, "/Calendar/Other/ConfirmDelete", TRUE, NULL);

	/* Default reminders */
	config->use_default_reminder = bonobo_config_get_boolean_with_default (
		db, "/Calendar/Other/UseDefaultReminder", FALSE, NULL);

	config->default_reminder_interval = bonobo_config_get_long_with_default (
		db, "/Calendar/Other/DefaultReminderInterval", 15, NULL);

	units = bonobo_config_get_string_with_default (db,
		"/Calendar/Other/DefaultReminderUnits", "minutes", NULL);

	if (!strcmp (units, "days"))
		config->default_reminder_units = CAL_DAYS;
	else if (!strcmp (units, "hours"))
		config->default_reminder_units = CAL_HOURS;
	else
		config->default_reminder_units = CAL_MINUTES; /* changed from above because
							       * if bonobo-config fucks up
							       * we want minutes, not days!
							       */
	g_free (units);

	bonobo_object_release_unref (db, NULL);
}

/* Returns the string representation of a units value */
static const char *
units_to_string (CalUnits units)
{
	switch (units) {
	case CAL_DAYS:
		return "days";

	case CAL_HOURS:
		return "hours";

	case CAL_MINUTES:
		return "minutes";

	default:
		g_assert_not_reached ();
		return NULL;
	}
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

	if (config->default_uri)
		bonobo_config_set_string (db, "/Calendar/DefaultUri",
					  config->default_uri, NULL);

	if (config->default_tasks_uri)
		bonobo_config_set_string (db, "/Calendar/DefaultTasksUri",
					  config->default_tasks_uri, NULL);

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

	bonobo_config_set_boolean (db, "/Calendar/Tasks/HideCompletedTasks",
				   config->hide_completed_tasks, NULL);

	bonobo_config_set_string (db,
				  "/Calendar/Tasks/HideCompletedTasksUnits",
				  units_to_string (config->hide_completed_tasks_units), NULL);
	bonobo_config_set_long (db,
				"/Calendar/Tasks/HideCompletedTasksValue",
				config->hide_completed_tasks_value, NULL);

	bonobo_config_set_boolean (db, "/Calendar/Other/ConfirmDelete", config->confirm_delete, NULL);

	bonobo_config_set_boolean (db, "/Calendar/Other/UseDefaultReminder",
				   config->use_default_reminder, NULL);

	bonobo_config_set_long (db, "/Calendar/Other/DefaultReminderInterval",
				config->default_reminder_interval, NULL);

	bonobo_config_set_string (db, "/Calendar/Other/DefaultReminderUnits",
				  units_to_string (config->default_reminder_units), NULL);

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

/* The default URI is the one that will be used in places where there
   might be some action on a calendar from outside, such as adding
   a meeting request. */
gchar *
calendar_config_get_default_uri (void)
{
	static gchar *default_uri = NULL;

	if (config->default_uri)
		return config->default_uri;

	if (!default_uri)
		default_uri = g_strdup_printf ("%s/evolution/local/Calendar/calendar.ics",
					       g_get_home_dir ());

	return default_uri;
}

/* Sets the default calendar URI */
void
calendar_config_set_default_uri (gchar *default_uri)
{
	g_free (config->default_uri);

	if (default_uri && default_uri[0])
		config->default_uri = g_strdup (default_uri);
	else
		config->default_uri = NULL;
}

gchar *
calendar_config_get_default_tasks_uri (void)
{
	static gchar *default_tasks_uri = NULL;

	if (config->default_tasks_uri)
		return config->default_tasks_uri;

	if (!default_tasks_uri)
		default_tasks_uri = g_strdup_printf ("%s/evolution/local/Tasks/tasks.ics",
						     g_get_home_dir ());

	return default_tasks_uri;
}

void
calendar_config_set_default_tasks_uri (gchar *default_tasks_uri)
{
	g_free (config->default_tasks_uri);

	if (default_tasks_uri && default_tasks_uri[0])
		config->default_tasks_uri = g_strdup (default_tasks_uri);
	else
		config->default_tasks_uri = NULL;
}

/* The current timezone, e.g. "Europe/London". It may be NULL, in which case
   you should assume UTC (though Evolution will show the timezone-setting
   dialog the next time a calendar or task folder is selected). */
gchar*
calendar_config_get_timezone		(void)
{
	return config->timezone;
}


/* Sets the timezone. If set to NULL it defaults to UTC.
   FIXME: Should check it is being set to a valid timezone. */
void
calendar_config_set_timezone		(gchar	     *timezone)
{
	g_free (config->timezone);

	if (timezone && timezone[0])
		config->timezone = g_strdup (timezone);
	else
		config->timezone = g_strdup ("UTC");
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


/* Settings to hide completed tasks. */
gboolean
calendar_config_get_hide_completed_tasks	(void)
{
	return config->hide_completed_tasks;
}


void
calendar_config_set_hide_completed_tasks	(gboolean	hide)
{
	config->hide_completed_tasks = hide;
}


CalUnits
calendar_config_get_hide_completed_tasks_units	(void)
{
	return config->hide_completed_tasks_units;
}


void
calendar_config_set_hide_completed_tasks_units	(CalUnits	units)
{
	config->hide_completed_tasks_units = units;
}


gint
calendar_config_get_hide_completed_tasks_value	(void)
{
	return config->hide_completed_tasks_value;
}


void
calendar_config_set_hide_completed_tasks_value	(gint		value)
{
	config->hide_completed_tasks_value = value;
}

/**
 * calendar_config_get_confirm_delete:
 *
 * Queries the configuration value for whether a confirmation dialog is
 * presented when deleting calendar/tasks items.
 *
 * Return value: Whether confirmation is required when deleting items.
 **/
gboolean
calendar_config_get_confirm_delete (void)
{
	return config->confirm_delete;
}

/**
 * calendar_config_set_confirm_delete:
 * @confirm: Whether confirmation is required when deleting items.
 *
 * Sets the configuration value for whether a confirmation dialog is presented
 * when deleting calendar/tasks items.
 **/
void
calendar_config_set_confirm_delete (gboolean confirm)
{
	config->confirm_delete = confirm;
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
	ECellPopup *ecp;
	ECellDateEditText *ecd;
	char *location;
	icaltimezone *zone;

	g_return_if_fail (E_IS_CELL_DATE_EDIT (ecde));

	ecp = E_CELL_POPUP (ecde);
	ecd = E_CELL_DATE_EDIT_TEXT (ecp->child);

	location = calendar_config_get_timezone ();
	zone = icaltimezone_get_builtin_timezone (location);

	calendar_config_configure_e_calendar (E_CALENDAR (ecde->calendar));

	use_24_hour = calendar_config_get_24_hour_format ();

	start_hour = calendar_config_get_day_start_hour ();
	end_hour = calendar_config_get_day_end_hour ();

	/* Round up the end hour. */
	if (calendar_config_get_day_end_minute () != 0)
		end_hour++;

	e_cell_date_edit_freeze (ecde);
	gtk_object_set (GTK_OBJECT (ecde),
			"use_24_hour_format", use_24_hour,
#if 0
			/* We use the default 0 - 24 now. */
			"lower_hour", start_hour,
			"upper_hour", end_hour,
#endif
			NULL);
	e_cell_date_edit_thaw (ecde);

	e_cell_date_edit_text_set_timezone (ecd, zone);
	e_cell_date_edit_text_set_use_24_hour_format (ecd, use_24_hour);
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

	/* Reload the event/tasks, since the 'Hide Completed Tasks' option
	   may have been changed, so the query needs to be updated. */
	calendar_model_refresh (model);
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
	icaltimezone *zone;

	e_timezone_dialog_get_timezone (etd, &display_name);

	/* We know it can only be a builtin timezone, since there is no way
	   to set it to anything else. */
	zone = e_timezone_dialog_get_builtin_timezone (display_name);
	if (zone) {
		calendar_config_set_timezone (icaltimezone_get_location (zone));

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

/**
 * calendar_config_get_use_default_reminder:
 *
 * Queries whether new appointments should be created with a default reminder.
 *
 * Return value: Boolean value indicating whether new appointments should be
 * created with a default reminder from the values of
 * calendar_config_get_default_reminder_interval() and
 * calendar_config_get_default_reminder_units().
 **/
gboolean
calendar_config_get_use_default_reminder (void)
{
	return config->use_default_reminder;
}

/**
 * calendar_config_set_use_default_reminder:
 * @value: Whether to create new appointments with a default reminder.
 *
 * Sets whether newly-created appointments should get a default reminder set
 * them.
 **/
void
calendar_config_set_use_default_reminder (gboolean value)
{
	config->use_default_reminder = value;
}

/**
 * calendar_config_get_default_reminder_interval:
 *
 * Queries the interval for the default reminder of newly-created
 * appointments, i.e. 5 in "5 minutes".
 *
 * Return value: Interval for default reminders.
 **/
int
calendar_config_get_default_reminder_interval (void)
{
	return config->default_reminder_interval;
}

/**
 * calendar_config_set_default_reminder_interval:
 * @interval: Interval value, e.g. 5 for "5 minutes".
 *
 * Sets the interval that should be used for the default reminder in new
 * appointments.
 **/
void
calendar_config_set_default_reminder_interval (int interval)
{
	config->default_reminder_interval = interval;
}

/**
 * calendar_config_get_default_reminder_units:
 *
 * Queries the units of time in which default reminders should be created for
 * new appointments, e.g. CAL_MINUTES for "5 minutes".
 *
 * Return value: Time units for default reminders.
 **/
CalUnits
calendar_config_get_default_reminder_units (void)
{
	return config->default_reminder_units;
}

/**
 * calendar_config_set_default_reminder_units:
 * @units: Time units, e.g. CAL_MINUTES for "5 minutes".
 *
 * Sets the units to be used for default reminders in new appointments.
 **/
void
calendar_config_set_default_reminder_units (CalUnits units)
{
	config->default_reminder_units = units;
}

/**
 * calendar_config_get_hide_completed_tasks_sexp:
 *
 * Returns the subexpression to use to filter out completed tasks according
 * to the config settings. The returned sexp should be freed.
 **/
char*
calendar_config_get_hide_completed_tasks_sexp (void)
{
	char *sexp = NULL;

	if (calendar_config_get_hide_completed_tasks ()) {
		CalUnits units;
		gint value;

		units = calendar_config_get_hide_completed_tasks_units ();
		value = calendar_config_get_hide_completed_tasks_value ();

		if (value == 0) {
			/* If the value is 0, we want to hide completed tasks
			   immediately, so we filter out all completed tasks.*/
			sexp = g_strdup ("(not is-completed?)");
		} else {
			char *location, *isodate;
			icaltimezone *zone;
			struct icaltimetype tt;
			time_t t;

			/* Get the current time, and subtract the appropriate
			   number of days/hours/minutes. */
			location = calendar_config_get_timezone ();
			zone = icaltimezone_get_builtin_timezone (location);
			tt = icaltime_current_time_with_zone (zone);

			switch (units) {
			case CAL_DAYS:
				icaltime_adjust (&tt, -value, 0, 0, 0);
				break;
			case CAL_HOURS:
				icaltime_adjust (&tt, 0, -value, 0, 0);
				break;
			case CAL_MINUTES:
				icaltime_adjust (&tt, 0, 0, -value, 0);
				break;
			default:
				g_assert_not_reached ();
			}

			t = icaltime_as_timet_with_zone (tt, zone);

			/* Convert the time to an ISO date string, and build
			   the query sub-expression. */
			isodate = isodate_from_time_t (t);
			sexp = g_strdup_printf ("(not (completed-before? (make-time \"%s\")))", isodate);
		}
	}

	return sexp;
}
