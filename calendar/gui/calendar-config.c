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
#include <string.h>
#include <time.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-config.h>
#include <libgnomeui/gnome-dialog.h>
#include <gal/util/e-util.h>
#include <widgets/e-timezone-dialog/e-timezone-dialog.h>
#include <cal-util/timeutil.h>

#include "calendar-component.h"
#include "calendar-commands.h"
#include "e-tasks.h"
#include "e-cell-date-edit-text.h"
#include "calendar-config.h"
#include "e-util/e-config-listener.h"


static EConfigListener *config = NULL;

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

	config = e_config_listener_new ();
	g_atexit ((GVoidFunc) do_cleanup);
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

/* The current timezone, e.g. "Europe/London". It may be NULL, in which case
   you should assume UTC (though Evolution will show the timezone-setting
   dialog the next time a calendar or task folder is selected). */
gchar*
calendar_config_get_timezone		(void)
{
	static char *timezone = NULL;

	if (timezone)
		g_free (timezone);

	timezone = e_config_listener_get_string_with_default (config,
							      "/apps/evolution/calendar/display/timezone",
							      "UTC", NULL);
	if (!timezone)
		timezone = g_strdup ("UTC");

	return timezone;
}


/* Sets the timezone. If set to NULL it defaults to UTC.
   FIXME: Should check it is being set to a valid timezone. */
void
calendar_config_set_timezone		(gchar	     *timezone)
{
	if (timezone && timezone[0])
		e_config_listener_set_string (config, "/apps/evolution/calendar/display/timezone", timezone);
	else
		e_config_listener_set_string (config, "/apps/evolution/calendar/display/timezone", "UTC");
}


/* Whether we use 24-hour format or 12-hour format (AM/PM). */
gboolean
calendar_config_get_24_hour_format	(void)
{
	/* If the locale defines 'am' and 'pm' strings then the user has the
	   choice of 12-hour or 24-hour time format, with 12-hour as the
	   default. If the locale doesn't have 'am' and 'pm' strings we have
	   to use 24-hour format, or strftime()/strptime() won't work. */
	if (calendar_config_locale_supports_12_hour_format ()) {
		return e_config_listener_get_boolean_with_default (
			config, "/apps/evolution/calendar/display/use_24hour_format", FALSE, NULL);
	}

	return TRUE;
}


void
calendar_config_set_24_hour_format	(gboolean     use_24_hour)
{
	e_config_listener_set_boolean (config, "/apps/evolution/calendar/display/use_24hour_format", use_24_hour);
}


/* The start day of the week (0 = Sun to 6 = Mon). */
gint
calendar_config_get_week_start_day	(void)
{
	return e_config_listener_get_long_with_default (config, "/apps/evolution/calendar/display/week_start_day", 1, NULL);
}


void
calendar_config_set_week_start_day	(gint	      week_start_day)
{
	e_config_listener_set_long (config, "/apps/evolution/calendar/display/week_start_day", week_start_day);
}


/* The start and end times of the work-day. */
gint
calendar_config_get_day_start_hour	(void)
{
	return e_config_listener_get_long_with_default (config, "/apps/evolution/calendar/display/day_start_hour", 9, NULL);
}


void
calendar_config_set_day_start_hour	(gint	      day_start_hour)
{
	e_config_listener_set_long (config, "/apps/evolution/calendar/display/day_start_hour", day_start_hour);
}


gint
calendar_config_get_day_start_minute	(void)
{
	return e_config_listener_get_long_with_default (config, "/apps/evolution/calendar/display/day_start_minute", 0, NULL);
}


void
calendar_config_set_day_start_minute	(gint	      day_start_min)
{
	e_config_listener_set_long (config, "/apps/evolution/calendar/display/day_start_minute", day_start_min);
}


gint
calendar_config_get_day_end_hour	(void)
{
	return e_config_listener_get_long_with_default (config, "/apps/evolution/calendar/display/day_end_hour", 17, NULL);
}


void
calendar_config_set_day_end_hour	(gint	      day_end_hour)
{
	e_config_listener_set_long (config, "/apps/evolution/calendar/display/day_end_hour", day_end_hour);
}


gint
calendar_config_get_day_end_minute	(void)
{
	return e_config_listener_get_long_with_default (config, "/apps/evolution/calendar/display/day_end_minute", 0, NULL);
}


void
calendar_config_set_day_end_minute	(gint	      day_end_min)
{
	e_config_listener_set_long (config, "/apps/evolution/calendar/display/day_end_minute", day_end_min);
}


/* The time divisions in the Day/Work-Week view in minutes (5/10/15/30/60). */
gint
calendar_config_get_time_divisions	(void)
{
	return e_config_listener_get_long_with_default (config, "/apps/evolution/calendar/display/time_divisions", 30, NULL);
}


void
calendar_config_set_time_divisions	(gint	      divisions)
{
	e_config_listener_set_long (config, "/apps/evolution/calendar/display/time_divisions", divisions);
}


/* Whether we show week numbers in the Date Navigator. */
gboolean
calendar_config_get_dnav_show_week_no	(void)
{
	return e_config_listener_get_boolean_with_default (config, "/apps/evolution/calendar/date_navigator/show_week_numbers", FALSE, NULL);
}


void
calendar_config_set_dnav_show_week_no	(gboolean     show_week_no)
{
	e_config_listener_set_boolean (config, "/apps/evolution/calendar/date_navigator/show_week_numbers", show_week_no);
}


/* The view to show on start-up, 0 = Day, 1 = WorkWeek, 2 = Week, 3 = Month. */
gint
calendar_config_get_default_view	(void)
{
	return e_config_listener_get_long_with_default (config, "/apps/evolution/calendar/display/default_view", 0, NULL);
}


void
calendar_config_set_default_view	(gint	      view)
{
	e_config_listener_set_long (config, "/apps/evolution/calendar/display/default_view", view);
}


/* The positions of the panes in the normal and month views. */
gint
calendar_config_get_hpane_pos		(void)
{
	return e_config_listener_get_long_with_default (config,
						       "/apps/evolution/calendar/display/hpane_position",
						       -1, NULL);
}


void
calendar_config_set_hpane_pos		(gint	      hpane_pos)
{
	e_config_listener_set_long (config, "/apps/evolution/calendar/display/hpane_position", hpane_pos);
}


gint
calendar_config_get_vpane_pos		(void)
{
	return e_config_listener_get_long_with_default (config, "/apps/evolution/calendar/display/vpane_position", -1, NULL);
}


void
calendar_config_set_vpane_pos		(gint	      vpane_pos)
{
	e_config_listener_set_long (config, "/apps/evolution/calendar/display/vpane_position", vpane_pos);
}


gint
calendar_config_get_month_hpane_pos	(void)
{
	return e_config_listener_get_long_with_default (config, "/apps/evolution/calendar/display/month_hpane_position", -1, NULL);
}


void
calendar_config_set_month_hpane_pos	(gint	      hpane_pos)
{
	e_config_listener_set_long (config, "/apps/evolution/calendar/display/month_hpane_position", hpane_pos);
}


gint
calendar_config_get_month_vpane_pos	(void)
{
	return  e_config_listener_get_long_with_default (config, "/apps/evolution/calendar/display/month_vpane_position", 0, NULL);
}


void
calendar_config_set_month_vpane_pos	(gint	      vpane_pos)
{
	e_config_listener_set_long (config, "/apps/evolution/calendar/display/month_vpane_position", vpane_pos);
}


/* Whether we compress the weekend in the week/month views. */
gboolean
calendar_config_get_compress_weekend	(void)
{
	return e_config_listener_get_boolean_with_default (config, "/apps/evolution/calendar/display/compress_weekend", TRUE, NULL);
}


void
calendar_config_set_compress_weekend	(gboolean     compress)
{
	e_config_listener_set_boolean (config, "/apps/evolution/calendar/display/compress_weekend", compress);
}


/* Whether we show event end times. */
gboolean
calendar_config_get_show_event_end	(void)
{
	return e_config_listener_get_boolean_with_default (config, "/apps/evolution/calendar/display/show_event_end", TRUE, NULL);
}


void
calendar_config_set_show_event_end	(gboolean     show_end)
{
	e_config_listener_set_boolean (config, "/apps/evolution/calendar/display/show_event_end", show_end);
}


/* The working days of the week, a bit-wise combination of flags. */
CalWeekdays
calendar_config_get_working_days	(void)
{
	return e_config_listener_get_long_with_default (config,
                "/apps/evolution/calendar/display/working_days", CAL_MONDAY | CAL_TUESDAY |
		CAL_WEDNESDAY | CAL_THURSDAY | CAL_FRIDAY, NULL);
}


void
calendar_config_set_working_days	(CalWeekdays  days)
{
	e_config_listener_set_long (config, "/apps/evolution/calendar/display/working_days", days);
}


/* Settings to hide completed tasks. */
gboolean
calendar_config_get_hide_completed_tasks	(void)
{
	return e_config_listener_get_boolean_with_default (config, "/apps/evolution/calendar/tasks/hide_completed", FALSE, NULL);
}


void
calendar_config_set_hide_completed_tasks	(gboolean	hide)
{
	e_config_listener_set_boolean (config, "/apps/evolution/calendar/tasks/hide_completed", hide);
}


CalUnits
calendar_config_get_hide_completed_tasks_units	(void)
{
	char *units;
	CalUnits cu;

	units = e_config_listener_get_string_with_default (config, "/apps/evolution/calendar/tasks/hide_completed_units", "days", NULL);

	if (!strcmp (units, "minutes"))
		cu = CAL_MINUTES;
	else if (!strcmp (units, "hours"))
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

	e_config_listener_set_string (config, "/apps/evolution/calendar/tasks/hide_completed_sunits", units);

	g_free (units);
}


gint
calendar_config_get_hide_completed_tasks_value	(void)
{
	return e_config_listener_get_long_with_default (config, "/apps/evolution/calendar/tasks/hide_completed_value", 1, NULL);
}


void
calendar_config_set_hide_completed_tasks_value	(gint		value)
{
	e_config_listener_set_long (config, "/apps/evolution/calendar/tasks/hide_completed_value", value);
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
	return e_config_listener_get_boolean_with_default (config, "/apps/evolution/calendar/prompts/confirm_delete", TRUE, NULL);
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
	e_config_listener_set_boolean (config, "/apps/evolution/calendar/prompts/confirm_delete", confirm);
}

/**
 * calendar_config_get_confirm_expunge:
 *
 * Queries the configuration value for whether a confirmation dialog is
 * presented when expunging calendar/tasks items.
 *
 * Return value: Whether confirmation is required when expunging items.
 **/
gboolean
calendar_config_get_confirm_expunge (void)
{
	return e_config_listener_get_boolean_with_default (config, "/apps/evolution/calendar/prompts/confirm_expunge", TRUE, NULL);
}

/**
 * calendar_config_set_confirm_expunge:
 * @confirm: Whether confirmation is required when expunging items.
 *
 * Sets the configuration value for whether a confirmation dialog is presented
 * when expunging calendar/tasks items.
 **/
void
calendar_config_set_confirm_expunge (gboolean confirm)
{
	e_config_listener_set_boolean (config, "/apps/evolution/calendar/prompts/confirm_expunge", confirm);
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
	g_object_set (G_OBJECT (ecde),
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
	if (zone) {
		calendar_config_set_timezone (icaltimezone_get_location (zone));

		update_all_config_settings ();
		e_tasks_update_all_config_settings ();
	}

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

	color = e_config_listener_get_string_with_default (config, "/apps/evolution/calendar/tasks/colors/due_today", "blue", NULL);
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

	e_config_listener_set_string (config, "/apps/evolution/calendar/tasks/colors/due_today", color);
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

	color = e_config_listener_get_string_with_default (config, "/apps/evolution/calendar/tasks/colors/overdue", "red", NULL);
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

	e_config_listener_set_string (config, "/apps/evolution/calendar/tasks/colors/overdue", color);
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
	return e_config_listener_get_boolean_with_default (config, "/apps/evolution/calendar/other/use_default_reminder", FALSE, NULL);
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
	e_config_listener_set_boolean (config, "/apps/evolution/calendar/other/use_default_reminder", value);
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
	return e_config_listener_get_long_with_default (config, "/apps/evolution/calendar/other/default_reminder_interval", 15, NULL);
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
	e_config_listener_set_long (config, "/apps/evolution/calendar/other/default_reminder_interval", interval);
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

	units = e_config_listener_get_string_with_default (config, "/apps/evolution/calendar/other/default_reminder_units", "minutes", NULL);

	if (!strcmp (units, "days"))
		cu = CAL_DAYS;
	else if (!strcmp (units, "hours"))
		cu = CAL_HOURS;
	else
		cu = CAL_MINUTES; /* changed from above because
				   * if bonobo-config fucks up
				   * we want minutes, not days!
				   */
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
	e_config_listener_set_string (config, "/apps/evolution/calendar/other/default_reminder_units", units_to_string(units));
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

char *
calendar_config_default_calendar_folder (void)
{
	char *uri;
	
	uri = e_config_listener_get_string_with_default (config, "/apps/evolution/shell/default_folders/calendar_uri", NULL, NULL);
	return uri;	
}

char *
calendar_config_default_tasks_folder (void)
{
	char *uri;
	
	uri = e_config_listener_get_string_with_default (config, "/apps/evolution/shell/default_folders/tasks_uri", NULL, NULL);
	return uri;	
}

