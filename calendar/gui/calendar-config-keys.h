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
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _CALENDAR_CONFIG_KEYS_H_
#define _CALENDAR_CONFIG_KEYS_H_

G_BEGIN_DECLS

#define CALENDAR_CONFIG_PREFIX "/apps/evolution/calendar"

/* Display settings */
#define CALENDAR_CONFIG_TIMEZONE CALENDAR_CONFIG_PREFIX "/display/timezone"
#define CALENDAR_CONFIG_SELECTED_CALENDARS CALENDAR_CONFIG_PREFIX "/display/selected_calendars"
#define CALENDAR_CONFIG_24HOUR CALENDAR_CONFIG_PREFIX "/display/use_24hour_format"
#define CALENDAR_CONFIG_DAY_START_HOUR CALENDAR_CONFIG_PREFIX "/display/day_start_hour"
#define CALENDAR_CONFIG_DAY_START_MINUTE CALENDAR_CONFIG_PREFIX "/display/day_start_minute"
#define CALENDAR_CONFIG_DAY_END_HOUR CALENDAR_CONFIG_PREFIX "/display/day_end_hour"
#define CALENDAR_CONFIG_DAY_END_MINUTE CALENDAR_CONFIG_PREFIX "/display/day_end_minute"
#define CALENDAR_CONFIG_TIME_DIVISIONS CALENDAR_CONFIG_PREFIX "/display/time_divisions"
#define CALENDAR_CONFIG_MONTH_SCROLL_BY_WEEK CALENDAR_CONFIG_PREFIX "/display/month_scroll_by_week"
#define CALENDAR_CONFIG_COMPRESS_WEEKEND CALENDAR_CONFIG_PREFIX "/display/compress_weekend"
#define CALENDAR_CONFIG_WORKING_DAYS CALENDAR_CONFIG_PREFIX "/display/working_days"
#define CALENDAR_CONFIG_DAY_SECOND_ZONE CALENDAR_CONFIG_PREFIX "/display/day_second_zone"
#define CALENDAR_CONFIG_DAY_SECOND_ZONES_LIST CALENDAR_CONFIG_PREFIX "/display/day_second_zones"
#define CALENDAR_CONFIG_DAY_SECOND_ZONES_MAX CALENDAR_CONFIG_PREFIX "/display/day_second_zones_max"

/* Task display settings */
#define CALENDAR_CONFIG_TASKS_SELECTED_TASKS CALENDAR_CONFIG_PREFIX "/tasks/selected_tasks"
#define CALENDAR_CONFIG_TASKS_HIDE_COMPLETED CALENDAR_CONFIG_PREFIX "/tasks/hide_completed"
#define CALENDAR_CONFIG_TASKS_HIDE_COMPLETED_UNITS CALENDAR_CONFIG_PREFIX "/tasks/hide_completed_units"
#define CALENDAR_CONFIG_TASKS_HIDE_COMPLETED_VALUE CALENDAR_CONFIG_PREFIX "/tasks/hide_completed_value"

/* Memo display settings */
#define CALENDAR_CONFIG_MEMOS_SELECTED_MEMOS CALENDAR_CONFIG_PREFIX "/memos/selected_memos"

/* Prompt settings */
#define CALENDAR_CONFIG_PROMPT_DELETE CALENDAR_CONFIG_PREFIX "/prompts/confirm_delete"

/* Default reminder */
#define CALENDAR_CONFIG_DEFAULT_REMINDER CALENDAR_CONFIG_PREFIX "/other/use_default_reminder"
#define CALENDAR_CONFIG_DEFAULT_REMINDER_INTERVAL CALENDAR_CONFIG_PREFIX "/other/default_reminder_interval"
#define CALENDAR_CONFIG_DEFAULT_REMINDER_UNITS CALENDAR_CONFIG_PREFIX "/other/default_reminder_units"

/* Free/Busy settings */
#define CALENDAR_CONFIG_TEMPLATE CALENDAR_CONFIG_PREFIX"/publish/template"

#define CALENDAR_CONFIG_SAVE_DIR CALENDAR_CONFIG_PREFIX"/audio_dir"

/* Birthday & Anniversary reminder */
#define CALENDAR_CONFIG_BA_REMINDER CALENDAR_CONFIG_PREFIX "/other/use_ba_reminder"
#define CALENDAR_CONFIG_BA_REMINDER_INTERVAL CALENDAR_CONFIG_PREFIX "/other/ba_reminder_interval"
#define CALENDAR_CONFIG_BA_REMINDER_UNITS CALENDAR_CONFIG_PREFIX "/other/ba_reminder_units"

/* drawing of events */
#define CALENDAR_CONFIG_DISPLAY_EVENTS_GRADIENT CALENDAR_CONFIG_PREFIX "/display/events_gradient"
#define CALENDAR_CONFIG_DISPLAY_EVENTS_ALPHA CALENDAR_CONFIG_PREFIX "/display/events_transparency"

G_END_DECLS

#endif
