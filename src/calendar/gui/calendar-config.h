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
 * calendar-config.h - functions to load/save/get/set user settings.
 */

#ifndef _CALENDAR_CONFIG_H_
#define _CALENDAR_CONFIG_H_

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libecal/libecal.h>

#include <e-util/e-util.h>

#define CALENDAR_CONFIG_CANCELLED_TASKS_SEXP "(contains? \"status\" \"CANCELLED\")"
#define CALENDAR_CONFIG_NOT_CANCELLED_TASKS_SEXP "(not " CALENDAR_CONFIG_CANCELLED_TASKS_SEXP ")"

typedef void (* CalendarConfigChangedFunc) (GSettings *settings,
					    const gchar *key,
					    gpointer user_data);

void calendar_config_remove_notification (CalendarConfigChangedFunc func, gpointer data);

/*
 * Calendar Settings.
 */

/* The current timezone, e.g. "Europe/London". */
ICalTimezone *calendar_config_get_icaltimezone (void);

/* Whether we use 24-hour format or 12-hour format (AM/PM). */
gboolean  calendar_config_get_24_hour_format	(void);

/* Settings to hide completed tasks. */
gboolean  calendar_config_get_hide_completed_tasks	(void);

gchar *	  calendar_config_get_hide_completed_tasks_sexp (gboolean get_completed);

gboolean  calendar_config_get_hide_cancelled_tasks	(void);


/* Returns TRUE if the locale has 'am' and 'pm' strings defined, i.e. it
 * supports 12-hour time format. */
gboolean  calendar_config_locale_supports_12_hour_format (void);

void	  calendar_config_set_dir_path (const gchar *);
gchar *	  calendar_config_get_dir_path (void);

GSList *calendar_config_get_day_second_zones (void);
void    calendar_config_free_day_second_zones (GSList *zones);
void    calendar_config_set_day_second_zone (const gchar *location);
gchar *  calendar_config_get_day_second_zone (void);
void    calendar_config_select_day_second_zone (GtkWidget *parent);

void   calendar_config_add_notification_day_second_zone (CalendarConfigChangedFunc func, gpointer data);

/* Scroll in a month view by a week, not by a month */
gboolean calendar_config_get_month_scroll_by_week (void);
void     calendar_config_add_notification_month_scroll_by_week (CalendarConfigChangedFunc func, gpointer data);

/* Start month view with current week instead of first week of the month */
gboolean calendar_config_get_month_start_with_current_week (void);

gboolean calendar_config_get_prefer_meeting (void);

GDateWeekday	calendar_config_get_week_start_day		(void);
gint		calendar_config_get_default_reminder_interval	(void);
EDurationType	calendar_config_get_default_reminder_units	(void);
gboolean	calendar_config_get_itip_attach_components	(void);

#endif /* _CALENDAR_CONFIG_H_ */
