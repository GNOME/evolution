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

#include <gio/gio.h>
#include <gdk/gdk.h>
#include <libecal/libecal.h>

#include <e-util/e-util.h>

/* These are used to get/set the working days in the week. The bit-flags are
 * combined together. The bits must be from 0 (Sun) to 6 (Sat) to match the
 * day values used by localtime etc. */
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

typedef void (* CalendarConfigChangedFunc) (GSettings *settings,
					    const gchar *key,
					    gpointer user_data);

void calendar_config_remove_notification (CalendarConfigChangedFunc func, gpointer data);

/*
 * Calendar Settings.
 */

/* The current timezone, e.g. "Europe/London". */
icaltimezone *calendar_config_get_icaltimezone (void);

/* The working days of the week, a bit-wise combination of flags. */
CalWeekdays calendar_config_get_working_days	(void);

/* Whether we use 24-hour format or 12-hour format (AM/PM). */
gboolean  calendar_config_get_24_hour_format	(void);

/* Settings to hide completed tasks. */
gboolean  calendar_config_get_hide_completed_tasks	(void);

gchar *	  calendar_config_get_hide_completed_tasks_sexp (gboolean get_completed);

/* Returns TRUE if the locale has 'am' and 'pm' strings defined, i.e. it
 * supports 12-hour time format. */
gboolean  calendar_config_locale_supports_12_hour_format (void);

void	  calendar_config_set_dir_path (const gchar *);
gchar *	  calendar_config_get_dir_path (void);

GSList *calendar_config_get_day_second_zones (void);
void    calendar_config_free_day_second_zones (GSList *zones);
void    calendar_config_set_day_second_zone (const gchar *location);
gchar *  calendar_config_get_day_second_zone (void);
void    calendar_config_select_day_second_zone (void);

void   calendar_config_add_notification_day_second_zone (CalendarConfigChangedFunc func, gpointer data);

/* Scroll in a month view by a week, not by a month */
gboolean calendar_config_get_month_scroll_by_week (void);
void     calendar_config_add_notification_month_scroll_by_week (CalendarConfigChangedFunc func, gpointer data);

gboolean calendar_config_get_prefer_meeting (void);

#endif /* _CALENDAR_CONFIG_H_ */
