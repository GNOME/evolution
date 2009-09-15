/*
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
 *		Damon Chaplin <damon@ximian.com>
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * calendar-config.c - functions to load/save/get/set user settings.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <string.h>
#include <e-util/e-util.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-data-server-util.h>
#include <widgets/e-timezone-dialog/e-timezone-dialog.h>
#include <shell/e-shell.h>

#include "calendar-config-keys.h"
#include "calendar-config.h"

static GConfClient *config = NULL;
static gboolean display_events_gradient = TRUE;
static gfloat display_events_alpha = 1.0;

static void
do_cleanup (void)
{
	g_object_unref (config);
	config = NULL;
}

static void
calendar_config_init (void)
{
	if (config)
		return;

	config = gconf_client_get_default ();
	g_atexit ((GVoidFunc) do_cleanup);

	gconf_client_add_dir (config, CALENDAR_CONFIG_PREFIX, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);

	display_events_gradient = gconf_client_get_bool (config, CALENDAR_CONFIG_DISPLAY_EVENTS_GRADIENT, NULL);
	display_events_alpha = gconf_client_get_float (config, CALENDAR_CONFIG_DISPLAY_EVENTS_ALPHA, NULL);
}

void
calendar_config_remove_notification (guint id)
{
	calendar_config_init ();

	gconf_client_notify_remove (config, id);
}

/* Returns TRUE if the locale has 'am' and 'pm' strings defined, in which
   case the user can choose between 12 and 24-hour time formats. */
gboolean
calendar_config_locale_supports_12_hour_format (void)
{
	gchar s[16];
	time_t t = 0;

	calendar_config_init ();

	e_utf8_strftime (s, sizeof s, "%p", gmtime (&t));
	return s[0] != '\0';
}

/* Returns the string representation of a units value */
static const gchar *
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
		g_return_val_if_reached (NULL);
	}
}

/* opposite function to 'units_to_string' */
static CalUnits
string_to_units (const gchar *units)
{
	CalUnits res;

	if (units && !strcmp (units, "days"))
		res = CAL_DAYS;
	else if (units && !strcmp (units, "hours"))
		res = CAL_HOURS;
	else
		res = CAL_MINUTES;

	return res;
}

/*
 * Calendar Settings.
 */

/* The current list of calendars selected */
GSList *
calendar_config_get_calendars_selected (void)
{
	calendar_config_init ();

	return gconf_client_get_list (config, CALENDAR_CONFIG_SELECTED_CALENDARS, GCONF_VALUE_STRING, NULL);
}

void
calendar_config_set_calendars_selected (GSList *selected)
{
	calendar_config_init ();

	gconf_client_set_list (config, CALENDAR_CONFIG_SELECTED_CALENDARS, GCONF_VALUE_STRING, selected, NULL);
}

static gchar *
calendar_config_get_timezone_stored (void)
{
	calendar_config_init ();

	return gconf_client_get_string (config, CALENDAR_CONFIG_TIMEZONE, NULL);
}

static gchar *
calendar_config_get_timezone (void)
{
	EShell *shell;
	EShellSettings *shell_settings;
	gboolean use_system_timezone;

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	use_system_timezone = e_shell_settings_get_boolean (
		shell_settings, "cal-use-system-timezone");

	if (use_system_timezone)
		return e_cal_util_get_system_timezone_location ();

	return calendar_config_get_timezone_stored ();
}

icaltimezone *
calendar_config_get_icaltimezone (void)
{
	gchar *location;
	icaltimezone *zone = NULL;

	calendar_config_init ();

	location = calendar_config_get_timezone ();
	if (location) {
		zone = icaltimezone_get_builtin_timezone (location);

		g_free (location);
	}
	return zone;
}

/* Whether we use 24-hour format or 12-hour format (AM/PM). */
gboolean
calendar_config_get_24_hour_format	(void)
{
	calendar_config_init ();

	/* If the locale defines 'am' and 'pm' strings then the user has the
	   choice of 12-hour or 24-hour time format, with 12-hour as the
	   default. If the locale doesn't have 'am' and 'pm' strings we have
	   to use 24-hour format, or strftime()/strptime() won't work. */
	if (calendar_config_locale_supports_12_hour_format ())
		return gconf_client_get_bool (config, CALENDAR_CONFIG_24HOUR, NULL);

	return TRUE;
}

/* The start day of the week (0 = Sun to 6 = Mon). */
gint
calendar_config_get_week_start_day	(void)
{
	calendar_config_init ();

	return gconf_client_get_int (config, CALENDAR_CONFIG_WEEK_START, NULL);
}

/* The start and end times of the work-day. */
gint
calendar_config_get_day_start_hour	(void)
{
	calendar_config_init ();

	return gconf_client_get_int (config, CALENDAR_CONFIG_DAY_START_HOUR, NULL);
}

void
calendar_config_set_day_start_hour	(gint	      day_start_hour)
{
	calendar_config_init ();

	gconf_client_set_int (config, CALENDAR_CONFIG_DAY_START_HOUR, day_start_hour, NULL);
}

gint
calendar_config_get_day_start_minute	(void)
{
	calendar_config_init ();

	return gconf_client_get_int (config, CALENDAR_CONFIG_DAY_START_MINUTE, NULL);
}

void
calendar_config_set_day_start_minute	(gint	      day_start_min)
{
	calendar_config_init ();

	gconf_client_set_int (config, CALENDAR_CONFIG_DAY_START_MINUTE, day_start_min, NULL);
}

gint
calendar_config_get_day_end_hour	(void)
{
	calendar_config_init ();

	return gconf_client_get_int (config, CALENDAR_CONFIG_DAY_END_HOUR, NULL);
}

void
calendar_config_set_day_end_hour	(gint	      day_end_hour)
{
	calendar_config_init ();

	gconf_client_set_int (config, CALENDAR_CONFIG_DAY_END_HOUR, day_end_hour, NULL);
}

gint
calendar_config_get_day_end_minute	(void)
{
	calendar_config_init ();

	return gconf_client_get_int (config, CALENDAR_CONFIG_DAY_END_MINUTE, NULL);
}

void
calendar_config_set_day_end_minute	(gint	      day_end_min)
{
	calendar_config_init ();

	gconf_client_set_int (config, CALENDAR_CONFIG_DAY_END_MINUTE, day_end_min, NULL);
}

/* The time divisions in the Day/Work-Week view in minutes (5/10/15/30/60). */
gint
calendar_config_get_time_divisions	(void)
{
	calendar_config_init ();

	return gconf_client_get_int (config, CALENDAR_CONFIG_TIME_DIVISIONS, NULL);
}

void
calendar_config_set_time_divisions	(gint	      divisions)
{
	calendar_config_init ();

	gconf_client_set_int (config, CALENDAR_CONFIG_TIME_DIVISIONS, divisions, NULL);
}

/* Scroll in a month view by a week, not by a month */
gboolean
calendar_config_get_month_scroll_by_week (void)
{
	calendar_config_init ();

	return gconf_client_get_bool (config, CALENDAR_CONFIG_MONTH_SCROLL_BY_WEEK, NULL);
}

void
calendar_config_set_month_scroll_by_week (gboolean value)
{
	calendar_config_init ();

	gconf_client_set_bool (config, CALENDAR_CONFIG_MONTH_SCROLL_BY_WEEK, value, NULL);
}

guint
calendar_config_add_notification_month_scroll_by_week (GConfClientNotifyFunc func, gpointer data)
{
	guint id;

	calendar_config_init ();

	id = gconf_client_notify_add (config, CALENDAR_CONFIG_MONTH_SCROLL_BY_WEEK, func, data, NULL, NULL);

	return id;
}

/* The positions of the panes in the normal and month views. */
void
calendar_config_set_hpane_pos		(gint	      hpane_pos)
{
	calendar_config_init ();

	gconf_client_set_int (config, CALENDAR_CONFIG_HPANE_POS, hpane_pos, NULL);
}

void
calendar_config_set_month_hpane_pos	(gint	      hpane_pos)
{
	calendar_config_init ();

	gconf_client_set_int (config, CALENDAR_CONFIG_MONTH_HPANE_POS, hpane_pos, NULL);
}

/* The current list of task lists selected */
GSList   *
calendar_config_get_tasks_selected (void)
{
	calendar_config_init();

	return gconf_client_get_list (config, CALENDAR_CONFIG_TASKS_SELECTED_TASKS, GCONF_VALUE_STRING, NULL);
}

void
calendar_config_set_tasks_selected (GSList *selected)
{
	calendar_config_init ();

	gconf_client_set_list (config, CALENDAR_CONFIG_TASKS_SELECTED_TASKS, GCONF_VALUE_STRING, selected, NULL);
}

/***************************************/

/* The current list of memo lists selected */
GSList   *
calendar_config_get_memos_selected (void)
{
	calendar_config_init ();

	return gconf_client_get_list (config, CALENDAR_CONFIG_MEMOS_SELECTED_MEMOS, GCONF_VALUE_STRING, NULL);
}

void
calendar_config_set_memos_selected (GSList *selected)
{
	calendar_config_init ();

	gconf_client_set_list (config, CALENDAR_CONFIG_MEMOS_SELECTED_MEMOS, GCONF_VALUE_STRING, selected, NULL);
}

/***************************************/

/* Whether we compress the weekend in the week/month views. */
gboolean
calendar_config_get_compress_weekend	(void)
{
	calendar_config_init ();

	return gconf_client_get_bool (config, CALENDAR_CONFIG_COMPRESS_WEEKEND, NULL);
}

/* The working days of the week, a bit-wise combination of flags. */
CalWeekdays
calendar_config_get_working_days	(void)
{
	calendar_config_init ();

	return gconf_client_get_int (config, CALENDAR_CONFIG_WORKING_DAYS, NULL);
}

/* Settings to hide completed tasks. */
gboolean
calendar_config_get_hide_completed_tasks	(void)
{
	calendar_config_init ();

	return gconf_client_get_bool (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED, NULL);
}

void
calendar_config_set_hide_completed_tasks	(gboolean	hide)
{
	calendar_config_init ();

	gconf_client_set_bool (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED, hide, NULL);
}

CalUnits
calendar_config_get_hide_completed_tasks_units	(void)
{
	gchar *units;
	CalUnits cu;

	calendar_config_init ();

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
	gchar *units;

	calendar_config_init ();

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

gint
calendar_config_get_hide_completed_tasks_value	(void)
{
	calendar_config_init ();

	return gconf_client_get_int (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED_VALUE, NULL);
}

void
calendar_config_set_hide_completed_tasks_value	(gint		value)
{
	calendar_config_init ();

	gconf_client_set_int (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED_VALUE, value, NULL);
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
	calendar_config_init ();

	return gconf_client_get_bool (config, CALENDAR_CONFIG_PROMPT_DELETE, NULL);
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
	calendar_config_init ();

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
	calendar_config_init ();

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
gint
calendar_config_get_default_reminder_interval (void)
{
	calendar_config_init ();

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
calendar_config_set_default_reminder_interval (gint interval)
{
	calendar_config_init ();

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
	gchar *units;
	CalUnits cu;

	calendar_config_init ();

	units = gconf_client_get_string (config, CALENDAR_CONFIG_DEFAULT_REMINDER_UNITS, NULL);
	cu = string_to_units (units);
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
	calendar_config_init ();

	gconf_client_set_string (config, CALENDAR_CONFIG_DEFAULT_REMINDER_UNITS, units_to_string(units), NULL);
}

/**
 * calendar_config_get_ba_reminder:
 * Retrieves setup of the Birthdays & Anniversaries reminder.
 *
 * @interval: Retrieves the interval setup for the reminder; can be NULL.
 * @units: Retrieves units for the interval; can be NULL.
 *
 * Returns whether the reminder is on or off. The values for interval and/or units
 * are retrieved even when returns FALSE.
 **/
gboolean
calendar_config_get_ba_reminder (gint *interval, CalUnits *units)
{
	calendar_config_init ();

	if (interval) {
		*interval = gconf_client_get_int (config, CALENDAR_CONFIG_BA_REMINDER_INTERVAL, NULL);
	}

	if (units) {
		gchar *str;

		str = gconf_client_get_string (config, CALENDAR_CONFIG_BA_REMINDER_UNITS, NULL);
		*units = string_to_units (str);
		g_free (str);
	}

	return gconf_client_get_bool (config, CALENDAR_CONFIG_BA_REMINDER, NULL);
}

/**
 * calendar_config_set_ba_reminder:
 * Stores new values for Birthdays & Anniversaries reminder to GConf. Only those, which are not NULL.
 *
 * @enabled: The enabled state; can be NULL.
 * @interval: The reminder interval; can be NULL.
 * @units: The units of the reminder; can be NULL.
 **/
void
calendar_config_set_ba_reminder (gboolean *enabled, gint *interval, CalUnits *units)
{
	calendar_config_init ();

	if (enabled)
		gconf_client_set_bool (config, CALENDAR_CONFIG_BA_REMINDER, *enabled, NULL);

	if (interval)
		gconf_client_set_int (config, CALENDAR_CONFIG_BA_REMINDER_INTERVAL, *interval, NULL);

	if (units)
		gconf_client_set_string (config, CALENDAR_CONFIG_BA_REMINDER_UNITS, units_to_string (*units), NULL);
}

/**
 * calendar_config_get_hide_completed_tasks_sexp:
 *
 * @get_completed: Whether to form subexpression that
 * gets completed or not completed tasks.
 * Returns the subexpression to use to filter out completed tasks according
 * to the config settings. The returned sexp should be freed.
 **/
gchar *
calendar_config_get_hide_completed_tasks_sexp (gboolean get_completed)
{
	gchar *sexp = NULL;

	if (calendar_config_get_hide_completed_tasks ()) {
		CalUnits units;
		gint value;

		units = calendar_config_get_hide_completed_tasks_units ();
		value = calendar_config_get_hide_completed_tasks_value ();

		if (value == 0) {
			/* If the value is 0, we want to hide completed tasks
			   immediately, so we filter out all complete/incomplete tasks.*/
			if (!get_completed)
				sexp = g_strdup ("(not is-completed?)");
			else
				sexp = g_strdup ("(is-completed?)");
		} else {
			gchar *isodate;
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
				g_return_val_if_reached (NULL);
			}

			t = icaltime_as_timet_with_zone (tt, zone);

			/* Convert the time to an ISO date string, and build
			   the query sub-expression. */
			isodate = isodate_from_time_t (t);
			if (!get_completed)
				sexp = g_strdup_printf ("(not (completed-before? (make-time \"%s\")))", isodate);
			else
				sexp = g_strdup_printf ("(completed-before? (make-time \"%s\"))", isodate);
			g_free (isodate);
		}
	}

	return sexp;
}

void
calendar_config_set_dir_path (const gchar *path)
{
	calendar_config_init ();

	gconf_client_set_string (config, CALENDAR_CONFIG_SAVE_DIR, path, NULL);
}

gchar *
calendar_config_get_dir_path (void)
{
	gchar *path;

	calendar_config_init ();

	path = gconf_client_get_string (config, CALENDAR_CONFIG_SAVE_DIR, NULL);

	return path;
}

/* contains list of strings, locations, recently used as the second timezone in a day view.
   Free with calendar_config_free_day_second_zones. */
GSList *
calendar_config_get_day_second_zones (void)
{
	GSList *res;

	calendar_config_init ();

	res = gconf_client_get_list (config, CALENDAR_CONFIG_DAY_SECOND_ZONES_LIST, GCONF_VALUE_STRING, NULL);

	return res;
}

/* frees list from calendar_config_get_day_second_zones */
void
calendar_config_free_day_second_zones (GSList *zones)
{
	if (zones) {
		g_slist_foreach (zones, (GFunc)g_free, NULL);
		g_slist_free (zones);
	}
}

/* keeps max 'day_second_zones_max' zones, if 'location' is already in a list, then it'll became first there */
void
calendar_config_set_day_second_zone (const gchar *location)
{
	calendar_config_init ();

	if (location && *location) {
		GSList *lst, *l;
		GError *error = NULL;
		gint max_zones;

		/* configurable max number of timezones to remember */
		max_zones = gconf_client_get_int (config, CALENDAR_CONFIG_DAY_SECOND_ZONES_MAX, &error);

		if (error) {
			g_error_free (error);
			max_zones = -1;
		}

		if (max_zones <= 0)
			max_zones = 5;

		lst = calendar_config_get_day_second_zones ();
		for (l = lst; l; l = l->next) {
			if (l->data && g_str_equal (l->data, location)) {
				if (l != lst) {
					/* isn't first in the list */
					gchar *val = l->data;

					lst = g_slist_remove (lst, val);
					lst = g_slist_prepend (lst, val);
				}
				break;
			}
		}

		if (!l) {
			/* not in the list yet */
			lst = g_slist_prepend (lst, g_strdup (location));
		}

		while (g_slist_length (lst) > max_zones) {
			l = g_slist_last (lst);
			g_free (l->data);
			lst = g_slist_delete_link (lst, l);
		}

		gconf_client_set_list (config, CALENDAR_CONFIG_DAY_SECOND_ZONES_LIST, GCONF_VALUE_STRING, lst, NULL);

		calendar_config_free_day_second_zones (lst);
	}

	gconf_client_set_string (config, CALENDAR_CONFIG_DAY_SECOND_ZONE, location ? location : "", NULL);
}

/* location of the second time zone user has selected. Free with g_free. */
gchar *
calendar_config_get_day_second_zone (void)
{
	calendar_config_init ();

	return gconf_client_get_string (config, CALENDAR_CONFIG_DAY_SECOND_ZONE, NULL);
}

void
calendar_config_select_day_second_zone (void)
{
	icaltimezone *zone = NULL;
	ETimezoneDialog *tzdlg;
	GtkWidget *dialog;
	gchar *second_location;

	second_location = calendar_config_get_day_second_zone ();
	if (second_location && *second_location)
		zone = icaltimezone_get_builtin_timezone (second_location);
	g_free (second_location);

	if (!zone)
		zone = calendar_config_get_icaltimezone ();

	tzdlg = e_timezone_dialog_new ();
	e_timezone_dialog_set_timezone (tzdlg, zone);

	dialog = e_timezone_dialog_get_toplevel (tzdlg);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		const gchar *location = NULL;

		zone = e_timezone_dialog_get_timezone (tzdlg);
		if (zone == icaltimezone_get_utc_timezone ()) {
			location = "UTC";
		} else if (zone) {
			location = icaltimezone_get_location (zone);
		}

		calendar_config_set_day_second_zone (location);
	}

	g_object_unref (tzdlg);
}

guint
calendar_config_add_notification_day_second_zone (GConfClientNotifyFunc func, gpointer data)
{
	guint id;

	calendar_config_init ();

	id = gconf_client_notify_add (config, CALENDAR_CONFIG_DAY_SECOND_ZONE, func, data, NULL, NULL);

	return id;
}

/* default count for recurring events */
gint
calendar_config_get_default_count (void)
{
	gint res;

	calendar_config_init ();

	res = gconf_client_get_int (config, CALENDAR_CONFIG_DEF_RECUR_COUNT, NULL);
	if (res <= 0 && res != -1)
		res = 2;

	return res;
}

gboolean
calendar_config_get_display_events_gradient (void)
{
	calendar_config_init ();

	return display_events_gradient;
}

gfloat
calendar_config_get_display_events_alpha (void)
{
	calendar_config_init ();

	return display_events_alpha;
}
