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
 * calendar-config.h - functions to load/save/get/set user settings.
 */

#ifndef _CALENDAR_CONFIG_H_
#define _CALENDAR_CONFIG_H_

#include <gconf/gconf-client.h>
#include <widgets/misc/e-calendar.h>
#include <widgets/misc/e-dateedit.h>
#include <widgets/misc/e-cell-date-edit.h>
#include "e-calendar-table.h"


/* These are used to get/set the working days in the week. The bit-flags are
   combined together. The bits must be from 0 (Sun) to 6 (Sat) to match the
   day values used by localtime etc. */
typedef enum
{
	CAL_SUNDAY	= 1 << 0,
	CAL_MONDAY	= 1 << 1,
	CAL_TUESDAY	= 1 << 2,
	CAL_WEDNESDAY	= 1 << 3,
	CAL_THURSDAY	= 1 << 4,
	CAL_FRIDAY	= 1 << 5,
	CAL_SATURDAY	= 1 << 6
} CalWeekdays;


/* Units for settings. */
typedef enum
{
	CAL_DAYS,
	CAL_HOURS,
	CAL_MINUTES
} CalUnits;


void	  calendar_config_init			(void);
void calendar_config_remove_notification (guint id);

/*
 * Calendar Settings.
 */

/* The current list of calendars selected */
GSList   *calendar_config_get_calendars_selected (void);
void	  calendar_config_set_calendars_selected (GSList *selected);
guint	  calendar_config_add_notification_calendars_selected (GConfClientNotifyFunc func, gpointer data);

/* The primary calendar */
char     *calendar_config_get_primary_calendar (void);
void	  calendar_config_set_primary_calendar (const char *primary_uid);
guint	  calendar_config_add_notification_primary_calendar (GConfClientNotifyFunc func, gpointer data);

/* The current timezone, e.g. "Europe/London". */
gchar*	  calendar_config_get_timezone		(void);
icaltimezone *calendar_config_get_icaltimezone (void);
void	  calendar_config_set_timezone		(gchar	     *timezone);
guint calendar_config_add_notification_timezone (GConfClientNotifyFunc func, gpointer data);

/* The working days of the week, a bit-wise combination of flags. */
CalWeekdays calendar_config_get_working_days	(void);
void	  calendar_config_set_working_days	(CalWeekdays  days);
guint calendar_config_add_notification_working_days (GConfClientNotifyFunc func, gpointer data);

/* The start day of the week (0 = Sun to 6 = Sat). */
gint	  calendar_config_get_week_start_day	(void);
void	  calendar_config_set_week_start_day	(gint	      week_start_day);
guint calendar_config_add_notification_week_start_day (GConfClientNotifyFunc func, gpointer data);

/* The start and end times of the work-day. */
gint	  calendar_config_get_day_start_hour	(void);
void	  calendar_config_set_day_start_hour	(gint	      day_start_hour);
guint calendar_config_add_notification_day_start_hour (GConfClientNotifyFunc func, gpointer data);

gint	  calendar_config_get_day_start_minute	(void);
void	  calendar_config_set_day_start_minute	(gint	      day_start_min);
guint calendar_config_add_notification_day_start_minute (GConfClientNotifyFunc func, gpointer data);

gint	  calendar_config_get_day_end_hour	(void);
void	  calendar_config_set_day_end_hour	(gint	      day_end_hour);
guint calendar_config_add_notification_day_end_hour (GConfClientNotifyFunc func, gpointer data);

gint	  calendar_config_get_day_end_minute	(void);
void	  calendar_config_set_day_end_minute	(gint	      day_end_min);
guint calendar_config_add_notification_day_end_minute (GConfClientNotifyFunc func, gpointer data);

/* Whether we use 24-hour format or 12-hour format (AM/PM). */
gboolean  calendar_config_get_24_hour_format	(void);
void	  calendar_config_set_24_hour_format	(gboolean     use_24_hour);
guint calendar_config_add_notification_24_hour_format (GConfClientNotifyFunc func, gpointer data);

/* The time divisions in the Day/Work-Week view in minutes (5/10/15/30/60). */
gint	  calendar_config_get_time_divisions	(void);
void	  calendar_config_set_time_divisions	(gint	      divisions);
guint calendar_config_add_notification_time_divisions (GConfClientNotifyFunc func, gpointer data);

/* Whether we show event end times. */
gboolean  calendar_config_get_show_event_end	(void);
void	  calendar_config_set_show_event_end	(gboolean     show_end);
guint calendar_config_add_notification_show_event_end (GConfClientNotifyFunc func, gpointer data);

/* Whether we compress the weekend in the week/month views. */
gboolean  calendar_config_get_compress_weekend	(void);
void	  calendar_config_set_compress_weekend	(gboolean     compress);
guint calendar_config_add_notification_compress_weekend (GConfClientNotifyFunc func, gpointer data);

/* Whether we show week numbers in the Date Navigator. */
gboolean  calendar_config_get_dnav_show_week_no	(void);
void	  calendar_config_set_dnav_show_week_no	(gboolean     show_week_no);
guint calendar_config_add_notification_dnav_show_week_no (GConfClientNotifyFunc func, gpointer data);

/* The positions of the panes in the normal and month views. */
gint      calendar_config_get_hpane_pos		(void);
void	  calendar_config_set_hpane_pos		(gint	      hpane_pos);

gint      calendar_config_get_vpane_pos		(void);
void	  calendar_config_set_vpane_pos		(gint	      vpane_pos);

gint      calendar_config_get_month_hpane_pos	(void);
void	  calendar_config_set_month_hpane_pos	(gint	      hpane_pos);

gint      calendar_config_get_month_vpane_pos	(void);
void	  calendar_config_set_month_vpane_pos	(gint	      vpane_pos);

/* The current list of task lists selected */
GSList   *calendar_config_get_tasks_selected (void);
void	  calendar_config_set_tasks_selected (GSList *selected);
guint	  calendar_config_add_notification_tasks_selected (GConfClientNotifyFunc func, gpointer data);

/* The primary calendar */
char     *calendar_config_get_primary_tasks (void);
void	  calendar_config_set_primary_tasks (const char *primary_uid);
guint	  calendar_config_add_notification_primary_tasks (GConfClientNotifyFunc func, gpointer data);

/* The pane position */
gint      calendar_config_get_task_vpane_pos    (void);
void      calendar_config_set_task_vpane_pos    (gint         vpane_pos);

/* Colors for the task list */
const char *calendar_config_get_tasks_due_today_color	(void);
void	    calendar_config_set_tasks_due_today_color	(const char *color);

const char *calendar_config_get_tasks_overdue_color	(void);
void	    calendar_config_set_tasks_overdue_color	(const char *color);

/* Settings to hide completed tasks. */
gboolean  calendar_config_get_hide_completed_tasks	(void);
void	  calendar_config_set_hide_completed_tasks	(gboolean	hide);
guint	  calendar_config_add_notification_hide_completed_tasks (GConfClientNotifyFunc func, gpointer data);

CalUnits  calendar_config_get_hide_completed_tasks_units(void);
void	  calendar_config_set_hide_completed_tasks_units(CalUnits	units);
guint	  calendar_config_add_notification_hide_completed_tasks_units (GConfClientNotifyFunc func, gpointer data);

gint	  calendar_config_get_hide_completed_tasks_value(void);
void	  calendar_config_set_hide_completed_tasks_value(gint		value);
guint	  calendar_config_add_notification_hide_completed_tasks_value (GConfClientNotifyFunc func, gpointer data);

char*	  calendar_config_get_hide_completed_tasks_sexp (void);

/* Confirmation options */
gboolean  calendar_config_get_confirm_delete (void);
void      calendar_config_set_confirm_delete (gboolean confirm);

gboolean  calendar_config_get_confirm_purge (void);
void      calendar_config_set_confirm_purge (gboolean confirm);

/* Default reminder options */
gboolean calendar_config_get_use_default_reminder (void);
void     calendar_config_set_use_default_reminder (gboolean value);

int      calendar_config_get_default_reminder_interval (void);
void     calendar_config_set_default_reminder_interval (int interval);

CalUnits calendar_config_get_default_reminder_units (void);
void     calendar_config_set_default_reminder_units (CalUnits units);

/* Free/Busy Settings */
GSList * calendar_config_get_free_busy (void);
void calendar_config_set_free_busy (GSList * url_list);

/* Convenience functions to configure common properties of ECalendar,
   EDateEdit & ECalendarTable widgets, and the ECellDateEdit ETable cell. */
void	  calendar_config_configure_e_calendar		(ECalendar	*cal);
void	  calendar_config_configure_e_date_edit		(EDateEdit	*dedit);
void	  calendar_config_configure_e_calendar_table	(ECalendarTable	*cal_table);
void	  calendar_config_configure_e_cell_date_edit	(ECellDateEdit	*ecde);

/* Shows the timezone dialog if the user hasn't set a default timezone. */
void	  calendar_config_check_timezone_set	(void);

/* Returns TRUE if the locale has 'am' and 'pm' strings defined, i.e. it
   supports 12-hour time format. */
gboolean  calendar_config_locale_supports_12_hour_format(void);

#endif /* _CALENDAR_CONFIG_H_ */
