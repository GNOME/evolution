/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors :
 *  Damon Chaplin <damon@ximian.com>
 *  Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2000-2004, Novell, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <time.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-config.h>
#include <libgnomeui/gnome-dialog.h>
#include <gal/util/e-util.h>
#include <widgets/e-timezone-dialog/e-timezone-dialog.h>
#include <libecal/e-cal-time-util.h>

#include "calendar-component.h"
#include "calendar-commands.h"
#include "e-tasks.h"
#include "e-cell-date-edit-text.h"
#include "calendar-config-keys.h"
#include "calendar-config.h"



static GConfClient *config = NULL;

static void on_timezone_set		(GnomeDialog	*dialog,
					 int		 button,
					 ETimezoneDialog *etd);
static gboolean on_timezone_dialog_delete_event	(GnomeDialog	*dialog,
						 GdkEvent	*event,
						 ETimezoneDialog *etd);

static void
do_cleanup (void)
{
	g_object_unref (config);
	config = NULL;
}

void
calendar_config_init			(void)
{
	if (config)
		return;

	config = gconf_client_get_default ();
	g_atexit ((GVoidFunc) do_cleanup);

	gconf_client_add_dir (config, CALENDAR_CONFIG_PREFIX, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
}

void
calendar_config_remove_notification (guint id)
{
	gconf_client_notify_remove (config, id);
}

/* Returns TRUE if the locale has 'am' and 'pm' strings defined, in which
   case the user can choose between 12 and 24-hour time formats. */
gboolean
calendar_config_locale_supports_12_hour_format (void)
{
	char s[16];
	time_t t = 0;

	e_utf8_strftime (s, sizeof s, "%p", gmtime (&t));
	return s[0] != '\0';
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

/*
 * Calendar Settings.
 */

/* The current list of calendars selected */
GSList *
calendar_config_get_calendars_selected (void)
{
	return gconf_client_get_list (config, CALENDAR_CONFIG_SELECTED_CALENDARS, GCONF_VALUE_STRING, NULL);
}

void
calendar_config_set_calendars_selected (GSList *selected)
{
	gconf_client_set_list (config, CALENDAR_CONFIG_SELECTED_CALENDARS, GCONF_VALUE_STRING, selected, NULL);
}

guint
calendar_config_add_notification_calendars_selected (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_SELECTED_CALENDARS, func, data, NULL, NULL);
	
	return id;
}

/* The primary calendar */
char *
calendar_config_get_primary_calendar (void)
{
	return gconf_client_get_string (config, CALENDAR_CONFIG_PRIMARY_CALENDAR, NULL);
}

void
calendar_config_set_primary_calendar (const char *primary_uid)
{
	gconf_client_set_string (config, CALENDAR_CONFIG_PRIMARY_CALENDAR, primary_uid, NULL);
}


guint
calendar_config_add_notification_primary_calendar (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_PRIMARY_CALENDAR, func, data, NULL, NULL);
	
	return id;
}


/* The current timezone, e.g. "Europe/London". It may be NULL, in which case
   you should assume UTC (though Evolution will show the timezone-setting
   dialog the next time a calendar or task folder is selected). */
gchar *
calendar_config_get_timezone (void)
{
	return gconf_client_get_string (config, CALENDAR_CONFIG_TIMEZONE, NULL);
}

icaltimezone *
calendar_config_get_icaltimezone (void)
{
	char *location;
	icaltimezone *zone = NULL;
	
	location = calendar_config_get_timezone ();
	if (location)
		zone = icaltimezone_get_builtin_timezone (location);

	if (!zone)
		zone = icaltimezone_get_utc_timezone ();
	
	return zone;
}


/* Sets the timezone. If set to NULL it defaults to UTC.
   FIXME: Should check it is being set to a valid timezone. */
void
calendar_config_set_timezone (gchar *timezone)
{
	if (timezone && timezone[0])
		gconf_client_set_string (config, CALENDAR_CONFIG_TIMEZONE, timezone, NULL);
	else
		gconf_client_set_string (config, CALENDAR_CONFIG_TIMEZONE, "UTC", NULL);
}

guint
calendar_config_add_notification_timezone (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_TIMEZONE, func, data, NULL, NULL);
	
	return id;
}

/* Whether we use 24-hour format or 12-hour format (AM/PM). */
gboolean
calendar_config_get_24_hour_format	(void)
{
	/* If the locale defines 'am' and 'pm' strings then the user has the
	   choice of 12-hour or 24-hour time format, with 12-hour as the
	   default. If the locale doesn't have 'am' and 'pm' strings we have
	   to use 24-hour format, or strftime()/strptime() won't work. */
	if (calendar_config_locale_supports_12_hour_format ())
		return gconf_client_get_bool (config, CALENDAR_CONFIG_24HOUR, NULL);

	return TRUE;
}


void
calendar_config_set_24_hour_format	(gboolean     use_24_hour)
{
	gconf_client_set_bool (config, CALENDAR_CONFIG_24HOUR, use_24_hour, NULL);
}

guint
calendar_config_add_notification_24_hour_format (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_24HOUR, func, data, NULL, NULL);
	
	return id;
}

/* The start day of the week (0 = Sun to 6 = Mon). */
gint
calendar_config_get_week_start_day	(void)
{
	return gconf_client_get_int (config, CALENDAR_CONFIG_WEEK_START, NULL);
}


void
calendar_config_set_week_start_day	(gint	      week_start_day)
{
	gconf_client_set_int (config, CALENDAR_CONFIG_WEEK_START, week_start_day, NULL);
}

guint 
calendar_config_add_notification_week_start_day (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_WEEK_START, func, data, NULL, NULL);
	
	return id;	
}

/* The start and end times of the work-day. */
gint
calendar_config_get_day_start_hour	(void)
{
	return gconf_client_get_int (config, CALENDAR_CONFIG_DAY_START_HOUR, NULL);
}


void
calendar_config_set_day_start_hour	(gint	      day_start_hour)
{
	gconf_client_set_int (config, CALENDAR_CONFIG_DAY_START_HOUR, day_start_hour, NULL);
}

guint 
calendar_config_add_notification_day_start_hour (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_DAY_START_HOUR, func, data, NULL, NULL);
	
	return id;	
}

gint
calendar_config_get_day_start_minute	(void)
{
	return gconf_client_get_int (config, CALENDAR_CONFIG_DAY_START_MINUTE, NULL);
}


void
calendar_config_set_day_start_minute	(gint	      day_start_min)
{
	gconf_client_set_int (config, CALENDAR_CONFIG_DAY_START_MINUTE, day_start_min, NULL);
}

guint 
calendar_config_add_notification_day_start_minute (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_DAY_START_MINUTE, func, data, NULL, NULL);
	
	return id;	
}

gint
calendar_config_get_day_end_hour	(void)
{
	return gconf_client_get_int (config, CALENDAR_CONFIG_DAY_END_HOUR, NULL);
}


void
calendar_config_set_day_end_hour	(gint	      day_end_hour)
{
	gconf_client_set_int (config, CALENDAR_CONFIG_DAY_END_HOUR, day_end_hour, NULL);
}

guint 
calendar_config_add_notification_day_end_hour (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_DAY_END_HOUR, func, data, NULL, NULL);
	
	return id;	
}

gint
calendar_config_get_day_end_minute	(void)
{
	return gconf_client_get_int (config, CALENDAR_CONFIG_DAY_END_MINUTE, NULL);
}


void
calendar_config_set_day_end_minute	(gint	      day_end_min)
{
	gconf_client_set_int (config, CALENDAR_CONFIG_DAY_END_MINUTE, day_end_min, NULL);
}

guint 
calendar_config_add_notification_day_end_minute (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_DAY_END_MINUTE, func, data, NULL, NULL);
	
	return id;	
}

/* The time divisions in the Day/Work-Week view in minutes (5/10/15/30/60). */
gint
calendar_config_get_time_divisions	(void)
{
	return gconf_client_get_int (config, CALENDAR_CONFIG_TIME_DIVISIONS, NULL);
}


void
calendar_config_set_time_divisions	(gint	      divisions)
{
	gconf_client_set_int (config, CALENDAR_CONFIG_TIME_DIVISIONS, divisions, NULL);
}

guint
calendar_config_add_notification_time_divisions (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_TIME_DIVISIONS, func, data, NULL, NULL);
	
	return id;
}

/* Whether we show week numbers in the Date Navigator. */
gboolean
calendar_config_get_dnav_show_week_no	(void)
{
	return gconf_client_get_bool (config, CALENDAR_CONFIG_DN_SHOW_WEEK_NUMBERS, NULL);
}


void
calendar_config_set_dnav_show_week_no	(gboolean     show_week_no)
{
	gconf_client_set_bool (config, CALENDAR_CONFIG_DN_SHOW_WEEK_NUMBERS, show_week_no, NULL);
}

guint 
calendar_config_add_notification_dnav_show_week_no (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_DN_SHOW_WEEK_NUMBERS, func, data, NULL, NULL);
	
	return id;
}

/* The positions of the panes in the normal and month views. */
gint
calendar_config_get_hpane_pos		(void)
{
	return gconf_client_get_int (config, CALENDAR_CONFIG_HPANE_POS, NULL);
}


void
calendar_config_set_hpane_pos		(gint	      hpane_pos)
{
	gconf_client_set_int (config, CALENDAR_CONFIG_HPANE_POS, hpane_pos, NULL);
}


gint
calendar_config_get_vpane_pos		(void)
{
	return gconf_client_get_int (config, CALENDAR_CONFIG_VPANE_POS, NULL);
}


void
calendar_config_set_vpane_pos		(gint	      vpane_pos)
{
	gconf_client_set_int (config, CALENDAR_CONFIG_VPANE_POS, vpane_pos, NULL);
}


gint
calendar_config_get_month_hpane_pos	(void)
{
	return gconf_client_get_int (config, CALENDAR_CONFIG_MONTH_HPANE_POS, NULL);
}


void
calendar_config_set_month_hpane_pos	(gint	      hpane_pos)
{
	gconf_client_set_int (config, CALENDAR_CONFIG_MONTH_HPANE_POS, hpane_pos, NULL);
}


gint
calendar_config_get_month_vpane_pos	(void)
{
	return  gconf_client_get_int (config, CALENDAR_CONFIG_MONTH_VPANE_POS, NULL);
}


void
calendar_config_set_month_vpane_pos	(gint	      vpane_pos)
{
	gconf_client_set_int (config, CALENDAR_CONFIG_MONTH_VPANE_POS, vpane_pos, NULL);
}

/* The current list of task lists selected */
GSList   *
calendar_config_get_tasks_selected (void)
{
	return gconf_client_get_list (config, CALENDAR_CONFIG_TASKS_SELECTED_TASKS, GCONF_VALUE_STRING, NULL);
}

void
calendar_config_set_tasks_selected (GSList *selected)
{
	gconf_client_set_list (config, CALENDAR_CONFIG_TASKS_SELECTED_TASKS, GCONF_VALUE_STRING, selected, NULL);
}

guint
calendar_config_add_notification_tasks_selected (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_TASKS_SELECTED_TASKS, func, data, NULL, NULL);
	
	return id;
}

/* The primary task list */
char *
calendar_config_get_primary_tasks (void)
{
	return gconf_client_get_string (config, CALENDAR_CONFIG_PRIMARY_TASKS, NULL);
}

void
calendar_config_set_primary_tasks (const char *primary_uid)
{
	gconf_client_set_string (config, CALENDAR_CONFIG_PRIMARY_TASKS, primary_uid, NULL);
}


guint
calendar_config_add_notification_primary_tasks (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_PRIMARY_TASKS, func, data, NULL, NULL);
	
	return id;
}

gint
calendar_config_get_task_vpane_pos	(void)
{
	return  gconf_client_get_int (config, CALENDAR_CONFIG_TASK_VPANE_POS, NULL);
}


void
calendar_config_set_task_vpane_pos	(gint	      vpane_pos)
{
	gconf_client_set_int (config, CALENDAR_CONFIG_TASK_VPANE_POS, vpane_pos, NULL);
}


/* Whether we compress the weekend in the week/month views. */
gboolean
calendar_config_get_compress_weekend	(void)
{
	return gconf_client_get_bool (config, CALENDAR_CONFIG_COMPRESS_WEEKEND, NULL);
}


void
calendar_config_set_compress_weekend	(gboolean     compress)
{
	gconf_client_set_bool (config, CALENDAR_CONFIG_COMPRESS_WEEKEND, compress, NULL);
}

guint
calendar_config_add_notification_compress_weekend (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_COMPRESS_WEEKEND, func, data, NULL, NULL);
	
	return id;
}

/* Whether we show event end times. */
gboolean
calendar_config_get_show_event_end	(void)
{
	return gconf_client_get_bool (config, CALENDAR_CONFIG_SHOW_EVENT_END, NULL);
}


void
calendar_config_set_show_event_end	(gboolean     show_end)
{
	gconf_client_set_bool (config, CALENDAR_CONFIG_SHOW_EVENT_END, show_end, NULL);
}

guint
calendar_config_add_notification_show_event_end (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_SHOW_EVENT_END, func, data, NULL, NULL);
	
	return id;
}

/* The working days of the week, a bit-wise combination of flags. */
CalWeekdays
calendar_config_get_working_days	(void)
{
	return gconf_client_get_int (config, CALENDAR_CONFIG_WORKING_DAYS, NULL);
}


void
calendar_config_set_working_days	(CalWeekdays  days)
{
	gconf_client_set_int (config, CALENDAR_CONFIG_WORKING_DAYS, days, NULL);
}

guint 
calendar_config_add_notification_working_days (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_WORKING_DAYS , func, data, NULL, NULL);
	
	return id;	
}

/* Settings to hide completed tasks. */
gboolean
calendar_config_get_hide_completed_tasks	(void)
{
	return gconf_client_get_bool (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED, NULL);
}


void
calendar_config_set_hide_completed_tasks	(gboolean	hide)
{
	gconf_client_set_bool (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED, hide, NULL);
}

guint 
calendar_config_add_notification_hide_completed_tasks (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED , func, data, NULL, NULL);
	
	return id;	
}

CalUnits
calendar_config_get_hide_completed_tasks_units	(void)
{
	char *units;
	CalUnits cu;

	units = gconf_client_get_string (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED_UNITS, NULL);

	if (units && !strcmp (units, "minutes"))
		cu = CAL_MINUTES;
	else if (units && !strcmp (units, "hours"))
		cu = CAL_HOURS;
	else
		cu = CAL_DAYS;

	g_free (units);

	return cu;
}


void
calendar_config_set_hide_completed_tasks_units	(CalUnits	cu)
{
	char *units;

	switch (cu) {
	case CAL_MINUTES :
		units = g_strdup ("minutes");
		break;
	case CAL_HOURS :
		units = g_strdup ("hours");
		break;
	default :
		units = g_strdup ("days");
	}

	gconf_client_set_string (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED_UNITS, units, NULL);

	g_free (units);
}

guint 
calendar_config_add_notification_hide_completed_tasks_units (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED_UNITS , func, data, NULL, NULL);
	
	return id;	
}

gint
calendar_config_get_hide_completed_tasks_value	(void)
{
	return gconf_client_get_int (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED_VALUE, NULL);
}


void
calendar_config_set_hide_completed_tasks_value	(gint		value)
{
	gconf_client_set_int (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED_VALUE, value, NULL);
}

guint 
calendar_config_add_notification_hide_completed_tasks_value (GConfClientNotifyFunc func, gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED_VALUE , func, data, NULL, NULL);
	
	return id;	
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
	return gconf_client_get_bool (config, CALENDAR_CONFIG_PROMPT_DELETE, NULL);
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
	gconf_client_set_bool (config, CALENDAR_CONFIG_PROMPT_DELETE, confirm, NULL);
}

/**
 * calendar_config_get_confirm_purge:
 *
 * Queries the configuration value for whether a confirmation dialog is
 * presented when purging calendar/tasks items.
 *
 * Return value: Whether confirmation is required when purging items.
 **/
gboolean
calendar_config_get_confirm_purge (void)
{
	return gconf_client_get_bool (config, CALENDAR_CONFIG_PROMPT_PURGE, NULL);
}

/**
 * calendar_config_set_confirm_purge:
 * @confirm: Whether confirmation is required when purging items.
 *
 * Sets the configuration value for whether a confirmation dialog is presented
 * when purging calendar/tasks items.
 **/
void
calendar_config_set_confirm_purge (gboolean confirm)
{
	gconf_client_set_bool (config, CALENDAR_CONFIG_PROMPT_PURGE, confirm, NULL);
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

	g_signal_connect (dialog, "clicked",
			  G_CALLBACK (on_timezone_set), timezone_dialog);
	g_signal_connect (dialog, "delete-event",
			  G_CALLBACK (on_timezone_dialog_delete_event), timezone_dialog);

	gtk_widget_show (dialog);
}


static void
on_timezone_set			(GnomeDialog	*dialog,
				 int		 button,
				 ETimezoneDialog *etd)
{
	icaltimezone *zone;

	zone = e_timezone_dialog_get_timezone (etd);
	if (zone)
		calendar_config_set_timezone (icaltimezone_get_location (zone));

	g_object_unref (etd);
}


static gboolean
on_timezone_dialog_delete_event	(GnomeDialog	*dialog,
				 GdkEvent	*event,
				 ETimezoneDialog *etd)
{
	g_object_unref (etd);
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
	static char *color = NULL;

	if (color)
		g_free (color);

	color = gconf_client_get_string (config, CALENDAR_CONFIG_TASKS_DUE_TODAY_COLOR, NULL);
	return color;
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

	gconf_client_set_string (config, CALENDAR_CONFIG_TASKS_DUE_TODAY_COLOR, color, NULL);
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
	static char *color = NULL;

	if (color)
		g_free (color);

	color = gconf_client_get_string (config, CALENDAR_CONFIG_TASKS_OVERDUE_COLOR, NULL);
	return color;
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

	gconf_client_set_string (config, CALENDAR_CONFIG_TASKS_OVERDUE_COLOR, color, NULL);
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
	return gconf_client_get_bool (config, CALENDAR_CONFIG_DEFAULT_REMINDER, NULL);
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
	gconf_client_set_bool (config, CALENDAR_CONFIG_DEFAULT_REMINDER, value, NULL);
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
	return gconf_client_get_int (config, CALENDAR_CONFIG_DEFAULT_REMINDER_INTERVAL, NULL);
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
	gconf_client_set_int (config, CALENDAR_CONFIG_DEFAULT_REMINDER_INTERVAL, interval, NULL);
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
	char *units;
	CalUnits cu;

	units = gconf_client_get_string (config, CALENDAR_CONFIG_DEFAULT_REMINDER_UNITS, NULL);

	if (units && !strcmp (units, "days"))
		cu = CAL_DAYS;
	else if (units && !strcmp (units, "hours"))
		cu = CAL_HOURS;
	else
		cu = CAL_MINUTES;
	g_free (units);

	return cu;
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
	gconf_client_set_string (config, CALENDAR_CONFIG_DEFAULT_REMINDER_UNITS, units_to_string(units), NULL);
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
			char *isodate;
			icaltimezone *zone;
			struct icaltimetype tt;
			time_t t;

			/* Get the current time, and subtract the appropriate
			   number of days/hours/minutes. */
			zone = calendar_config_get_icaltimezone ();
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

GSList *
calendar_config_get_free_busy (void)
{	
	return gconf_client_get_list (config, CALENDAR_CONFIG_PUBLISH, 
				      GCONF_VALUE_STRING, NULL);
}

void
calendar_config_set_free_busy (GSList *url_list)
{
	gconf_client_set_list (config, CALENDAR_CONFIG_PUBLISH, 
			       GCONF_VALUE_STRING, url_list, NULL);
}

gchar *
calendar_config_get_free_busy_template (void)
{
	return gconf_client_get_string (config, CALENDAR_CONFIG_TEMPLATE, NULL);
}

void
calendar_config_set_free_busy_template (const gchar *template)
{
	gconf_client_set_string (config, CALENDAR_CONFIG_TEMPLATE, template, NULL);
}

guint 
calendar_config_add_notification_free_busy_template (GConfClientNotifyFunc func, 
						     gpointer data)
{
	guint id;
	
	id = gconf_client_notify_add (config, CALENDAR_CONFIG_TEMPLATE, func, data, 
				      NULL, NULL);
	
	return id;	
}
