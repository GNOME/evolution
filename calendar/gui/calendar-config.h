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
 * calendar-config.h - functions to load/save/get/set user settings.
 */

#ifndef _CALENDAR_CONFIG_H_
#define _CALENDAR_CONFIG_H_

#include <glib.h>
#include <gdk/gdk.h>
#include <libecal/e-cal.h>
#include <gconf/gconf-client.h>

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

void calendar_config_remove_notification (guint id);

/*
 * Calendar Settings.
 */

/* The current list of calendars selected */
GSList   *calendar_config_get_calendars_selected (void);
void	  calendar_config_set_calendars_selected (GSList *selected);

/* The current timezone, e.g. "Europe/London". */
icaltimezone *calendar_config_get_icaltimezone (void);

/* The working days of the week, a bit-wise combination of flags. */
CalWeekdays calendar_config_get_working_days	(void);

/* The start day of the week (0 = Sun to 6 = Sat). */
gint	  calendar_config_get_week_start_day	(void);

/* The start and end times of the work-day. */
gint	  calendar_config_get_day_start_hour	(void);
void	  calendar_config_set_day_start_hour	(gint	      day_start_hour);

gint	  calendar_config_get_day_start_minute	(void);
void	  calendar_config_set_day_start_minute	(gint	      day_start_min);

gint	  calendar_config_get_day_end_hour	(void);
void	  calendar_config_set_day_end_hour	(gint	      day_end_hour);

gint	  calendar_config_get_day_end_minute	(void);
void	  calendar_config_set_day_end_minute	(gint	      day_end_min);

/* Whether we use 24-hour format or 12-hour format (AM/PM). */
gboolean  calendar_config_get_24_hour_format	(void);

/* The time divisions in the Day/Work-Week view in minutes (5/10/15/30/60). */
gint	  calendar_config_get_time_divisions	(void);
void	  calendar_config_set_time_divisions	(gint	      divisions);

/* Whether we compress the weekend in the week/month views. */
gboolean  calendar_config_get_compress_weekend	(void);

/* The positions of the panes in the normal and month views. */
void	  calendar_config_set_hpane_pos		(gint	      hpane_pos);

void	  calendar_config_set_month_hpane_pos	(gint	      hpane_pos);

/* The current list of task lists selected */
GSList   *calendar_config_get_tasks_selected (void);
void	  calendar_config_set_tasks_selected (GSList *selected);

/* The current list of memo lists selected */
GSList   *calendar_config_get_memos_selected (void);
void	  calendar_config_set_memos_selected (GSList *selected);

/* Settings to hide completed tasks. */
gboolean  calendar_config_get_hide_completed_tasks	(void);
void	  calendar_config_set_hide_completed_tasks	(gboolean	hide);

CalUnits  calendar_config_get_hide_completed_tasks_units(void);
void	  calendar_config_set_hide_completed_tasks_units(CalUnits	units);

gint	  calendar_config_get_hide_completed_tasks_value(void);
void	  calendar_config_set_hide_completed_tasks_value(gint		value);

gchar *	  calendar_config_get_hide_completed_tasks_sexp (gboolean get_completed);

/* Confirmation options */
gboolean  calendar_config_get_confirm_delete (void);

/* Default reminder options */
gboolean calendar_config_get_use_default_reminder (void);
void     calendar_config_set_use_default_reminder (gboolean value);

gint      calendar_config_get_default_reminder_interval (void);
void     calendar_config_set_default_reminder_interval (gint interval);

CalUnits calendar_config_get_default_reminder_units (void);
void     calendar_config_set_default_reminder_units (CalUnits units);

/* Free/Busy Settings */
GSList * calendar_config_get_free_busy (void);
void calendar_config_set_free_busy (GSList * url_list);

/* Returns TRUE if the locale has 'am' and 'pm' strings defined, i.e. it
   supports 12-hour time format. */
gboolean  calendar_config_locale_supports_12_hour_format(void);

void	  calendar_config_set_dir_path (const gchar *);
gchar *	  calendar_config_get_dir_path (void);

GSList *calendar_config_get_day_second_zones (void);
void    calendar_config_free_day_second_zones (GSList *zones);
void    calendar_config_set_day_second_zone (const gchar *location);
gchar *  calendar_config_get_day_second_zone (void);
void    calendar_config_select_day_second_zone (void);
guint   calendar_config_add_notification_day_second_zone (GConfClientNotifyFunc func, gpointer data);

/* Birthdays & Anniversaries reminder settings */
gboolean calendar_config_get_ba_reminder (gint *interval, CalUnits *units);
void calendar_config_set_ba_reminder (gboolean *enabled, gint *interval, CalUnits *units);

/* Scroll in a month view by a week, not by a month */
gboolean calendar_config_get_month_scroll_by_week (void);
void calendar_config_set_month_scroll_by_week (gboolean value);
guint calendar_config_add_notification_month_scroll_by_week (GConfClientNotifyFunc func, gpointer data);

/* default count for recurring events */
gint calendar_config_get_default_count (void);

/* event drawing customization, one-time read on start only */
gboolean calendar_config_get_display_events_gradient (void);
gfloat   calendar_config_get_display_events_alpha (void);

#endif /* _CALENDAR_CONFIG_H_ */
