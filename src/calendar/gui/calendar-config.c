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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * calendar-config.c - functions to load/save/get/set user settings.
 */

#include "evolution-config.h"

#include <time.h>
#include <string.h>
#include <gio/gio.h>

#include <shell/e-shell.h>

#include "calendar-config-keys.h"
#include "calendar-config.h"

static GSettings *config = NULL;

static void
do_cleanup (void)
{
	g_object_unref (config);
	config = NULL;
}

static void
calendar_config_init (void)
{
	EShell *shell;

	if (config)
		return;

	config = e_util_ref_settings ("org.gnome.evolution.calendar");

	shell = e_shell_get_default ();

	if (shell) {
		/* will be freed together with EShell, or will leak */
		g_object_set_data_full (
			G_OBJECT (shell),
			"calendar-config-config-cleanup", (gpointer) "1",
			(GDestroyNotify) do_cleanup);
	}
}

void
calendar_config_remove_notification (CalendarConfigChangedFunc func,
                                     gpointer data)
{
	calendar_config_init ();

	g_signal_handlers_disconnect_by_func (config, G_CALLBACK (func), data);
}

/* Returns TRUE if the locale has 'am' and 'pm' strings defined, in which
 * case the user can choose between 12 and 24-hour time formats. */
gboolean
calendar_config_locale_supports_12_hour_format (void)
{
	gchar s[16];
	time_t t = 0;

	calendar_config_init ();

	e_utf8_strftime (s, sizeof s, "%p", gmtime (&t));
	return s[0] != '\0';
}

/*
 * Calendar Settings.
 */

static gchar *
calendar_config_get_timezone_stored (void)
{
	calendar_config_init ();

	return g_settings_get_string (config, "timezone");
}

static gchar *
calendar_config_get_timezone (void)
{
	GSettings *settings;
	gboolean use_system_timezone;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	use_system_timezone =
		g_settings_get_boolean (settings, "use-system-timezone");

	g_object_unref (settings);

	if (use_system_timezone)
		return e_cal_util_get_system_timezone_location ();

	return calendar_config_get_timezone_stored ();
}

ICalTimezone *
calendar_config_get_icaltimezone (void)
{
	gchar *location;
	ICalTimezone *zone = NULL;

	calendar_config_init ();

	location = calendar_config_get_timezone ();
	if (location) {
		zone = i_cal_timezone_get_builtin_timezone (location);

		g_free (location);
	}
	return zone;
}

/* Whether we use 24-hour format or 12-hour format (AM/PM). */
gboolean
calendar_config_get_24_hour_format (void)
{
	calendar_config_init ();

	/* If the locale defines 'am' and 'pm' strings then the user has the
	 * choice of 12-hour or 24-hour time format, with 12-hour as the
	 * default. If the locale doesn't have 'am' and 'pm' strings we have
	 * to use 24-hour format, or strftime ()/strptime () won't work. */
	if (calendar_config_locale_supports_12_hour_format ())
		return g_settings_get_boolean (config, "use-24hour-format");

	return TRUE;
}

/* Scroll in a month view by a week, not by a month */
gboolean
calendar_config_get_month_scroll_by_week (void)
{
	calendar_config_init ();

	return g_settings_get_boolean (config, "month-scroll-by-week");
}

void
calendar_config_add_notification_month_scroll_by_week (CalendarConfigChangedFunc func,
                                                       gpointer data)
{
	calendar_config_init ();

	g_signal_connect (
		config, "changed::month-scroll-by-week",
		G_CALLBACK (func), data);
}

/* Start month view with current week instead of first week of the month */
gboolean
calendar_config_get_month_start_with_current_week (void)
{
	calendar_config_init();

	return g_settings_get_boolean (config, "month-start-with-current-week");
}

/***************************************/

/* Settings to hide completed tasks. */
gboolean
calendar_config_get_hide_completed_tasks (void)
{
	calendar_config_init ();

	return g_settings_get_boolean (config, "hide-completed-tasks");
}

static EDurationType
calendar_config_get_hide_completed_tasks_units (void)
{
	gchar *units;
	EDurationType cu;

	calendar_config_init ();

	units = g_settings_get_string (config, "hide-completed-tasks-units");

	if (units && !strcmp (units, "minutes"))
		cu = E_DURATION_MINUTES;
	else if (units && !strcmp (units, "hours"))
		cu = E_DURATION_HOURS;
	else
		cu = E_DURATION_DAYS;

	g_free (units);

	return cu;
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
		EDurationType units;
		gint value;

		units = calendar_config_get_hide_completed_tasks_units ();
		value = g_settings_get_int (config, "hide-completed-tasks-value");

		if (value == 0) {
			/* If the value is 0, we want to hide completed tasks
			 * immediately, so we filter out all complete/incomplete tasks.*/
			if (!get_completed)
				sexp = g_strdup ("(not is-completed?)");
			else
				sexp = g_strdup ("(is-completed?)");
		} else {
			gchar *isodate;
			ICalTimezone *zone;
			ICalTime *tt;
			time_t t;

			/* Get the current time, and subtract the appropriate
			 * number of days/hours/minutes. */
			zone = calendar_config_get_icaltimezone ();
			tt = i_cal_time_new_current_with_zone (zone);

			switch (units) {
			case E_DURATION_DAYS:
				i_cal_time_adjust (tt, -value, 0, 0, 0);
				break;
			case E_DURATION_HOURS:
				i_cal_time_adjust (tt, 0, -value, 0, 0);
				break;
			case E_DURATION_MINUTES:
				i_cal_time_adjust (tt, 0, 0, -value, 0);
				break;
			default:
				g_clear_object (&tt);
				g_return_val_if_reached (NULL);
			}

			t = i_cal_time_as_timet_with_zone (tt, zone);

			g_clear_object (&tt);

			/* Convert the time to an ISO date string, and build
			 * the query sub-expression. */
			isodate = isodate_from_time_t (t);
			if (!get_completed)
				sexp = g_strdup_printf (
					"(not (completed-before? "
					"(make-time \"%s\")))", isodate);
			else
				sexp = g_strdup_printf (
					"(completed-before? "
					"(make-time \"%s\"))", isodate);
			g_free (isodate);
		}
	}

	return sexp;
}

gboolean
calendar_config_get_hide_cancelled_tasks (void)
{
	calendar_config_init ();

	return g_settings_get_boolean (config, "hide-cancelled-tasks");
}

void
calendar_config_set_dir_path (const gchar *path)
{
	calendar_config_init ();

	g_settings_set_string (config, "audio-dir", path);
}

gchar *
calendar_config_get_dir_path (void)
{
	gchar *path;

	calendar_config_init ();

	path = g_settings_get_string (config, "audio-dir");

	return path;
}

/* contains list of strings, locations, recently used as the second timezone
 * in a day view.  Free with calendar_config_free_day_second_zones. */
GSList *
calendar_config_get_day_second_zones (void)
{
	GSList *res = NULL;
	gchar **strv;
	gint i;

	calendar_config_init ();

	strv = g_settings_get_strv (config, "day-second-zones");
	for (i = 0; i < g_strv_length (strv); i++) {
		if (strv[i] != NULL)
			res = g_slist_append (res, g_strdup (strv[i]));
	}

	g_strfreev (strv);

	return res;
}

/* frees list from calendar_config_get_day_second_zones */
void
calendar_config_free_day_second_zones (GSList *zones)
{
	if (zones) {
		g_slist_foreach (zones, (GFunc) g_free, NULL);
		g_slist_free (zones);
	}
}

/* keeps max 'day_second_zones_max' zones, if 'location'
 * is already in a list, then it'll became first there */
void
calendar_config_set_day_second_zone (const gchar *location)
{
	calendar_config_init ();

	if (location && *location) {
		GSList *lst, *l;
		gint max_zones;
		GPtrArray *array;
		gint i;

		/* configurable max number of timezones to remember */
		max_zones = g_settings_get_int (config, "day-second-zones-max");

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

		array = g_ptr_array_new ();
		for (i = 0, l = lst; i < max_zones && l != NULL; i++, l = l->next)
			g_ptr_array_add (array, l->data);
		g_ptr_array_add (array, NULL);

		g_settings_set_strv (
			config, "day-second-zones",
			(const gchar * const *) array->pdata);

		calendar_config_free_day_second_zones (lst);
		g_ptr_array_free (array, FALSE);
	}

	g_settings_set_string (
		config, "day-second-zone",
		(location != NULL) ? location : "");
}

/* location of the second time zone user has selected. Free with g_free. */
gchar *
calendar_config_get_day_second_zone (void)
{
	calendar_config_init ();

	return g_settings_get_string (config, "day-second-zone");
}

void
calendar_config_select_day_second_zone (GtkWidget *parent)
{
	ICalTimezone *zone = NULL;
	ETimezoneDialog *tzdlg;
	GtkWidget *dialog;
	gchar *second_location;

	second_location = calendar_config_get_day_second_zone ();
	if (second_location && *second_location)
		zone = i_cal_timezone_get_builtin_timezone (second_location);
	g_free (second_location);

	if (!zone)
		zone = calendar_config_get_icaltimezone ();

	tzdlg = e_timezone_dialog_new ();
	e_timezone_dialog_set_timezone (tzdlg, zone);

	dialog = e_timezone_dialog_get_toplevel (tzdlg);

	if (GTK_IS_WINDOW (parent))
		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		const gchar *location = NULL;

		zone = e_timezone_dialog_get_timezone (tzdlg);
		if (zone == i_cal_timezone_get_utc_timezone ()) {
			location = "UTC";
		} else if (zone) {
			location = i_cal_timezone_get_location (zone);
		}

		calendar_config_set_day_second_zone (location);
	}

	g_object_unref (tzdlg);
}

void
calendar_config_add_notification_day_second_zone (CalendarConfigChangedFunc func,
                                                  gpointer data)
{
	calendar_config_init ();

	g_signal_connect (
		config, "changed::day-second-zone",
		G_CALLBACK (func), data);
}

gboolean
calendar_config_get_prefer_meeting (void)
{
	GSettings *settings;
	gchar *prefer_new_item;
	gboolean prefer_meeting;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	prefer_new_item = g_settings_get_string (settings, "prefer-new-item");
	prefer_meeting = g_strcmp0 (prefer_new_item, "event-meeting-new") == 0;
	g_free (prefer_new_item);

	g_object_unref (settings);

	return prefer_meeting;
}

GDateWeekday
calendar_config_get_week_start_day (void)
{
	GSettings *settings;
	GDateWeekday res;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	res = g_settings_get_enum (settings, "week-start-day-name");

	g_object_unref (settings);

	return res;
}

gint
calendar_config_get_default_reminder_interval (void)
{
	GSettings *settings;
	gint res;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	res = g_settings_get_int (settings, "default-reminder-interval");

	g_object_unref (settings);

	return res;
}

EDurationType
calendar_config_get_default_reminder_units (void)
{
	GSettings *settings;
	EDurationType res;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	res = g_settings_get_enum (settings, "default-reminder-units");

	g_object_unref (settings);

	return res;
}

gboolean
calendar_config_get_itip_attach_components (void)
{
	GSettings *settings;
	gboolean res;

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.itip");

	res = g_settings_get_boolean (settings, "attach-components");

	g_object_unref (settings);

	return res;
}
