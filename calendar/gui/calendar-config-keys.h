/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors :
 *  JP Rosevear <jpr@ximian.com>
 *
 * Copyright 2003, Ximian, Inc.
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

#ifndef _CALENDAR_CONFIG_KEYS_H_
#define _CALENDAR_CONFIG_KEYS_H_

G_BEGIN_DECLS

#define CALENDAR_CONFIG_PREFIX "/apps/evolution/calendar"

/* Display settings */
#define CALENDAR_CONFIG_TIMEZONE CALENDAR_CONFIG_PREFIX "/display/timezone"
#define CALENDAR_CONFIG_SELECTED_CALENDARS CALENDAR_CONFIG_PREFIX "/display/selected_calendars"
#define CALENDAR_CONFIG_PRIMARY_CALENDAR CALENDAR_CONFIG_PREFIX "/display/primary_calendar"
#define CALENDAR_CONFIG_24HOUR CALENDAR_CONFIG_PREFIX "/display/use_24hour_format"
#define CALENDAR_CONFIG_WEEK_START CALENDAR_CONFIG_PREFIX "/display/week_start_day"
#define CALENDAR_CONFIG_DAY_START_HOUR CALENDAR_CONFIG_PREFIX "/display/day_start_hour"
#define CALENDAR_CONFIG_DAY_START_MINUTE CALENDAR_CONFIG_PREFIX "/display/day_start_minute"
#define CALENDAR_CONFIG_DAY_END_HOUR CALENDAR_CONFIG_PREFIX "/display/day_end_hour"
#define CALENDAR_CONFIG_DAY_END_MINUTE CALENDAR_CONFIG_PREFIX "/display/day_end_minute"
#define CALENDAR_CONFIG_TIME_DIVISIONS CALENDAR_CONFIG_PREFIX "/display/time_divisions"
#define CALENDAR_CONFIG_TIME_DIVISIONS CALENDAR_CONFIG_PREFIX "/display/time_divisions"
#define CALENDAR_CONFIG_DEFAULT_VIEW CALENDAR_CONFIG_PREFIX "/display/default_view"
#define CALENDAR_CONFIG_HPANE_POS CALENDAR_CONFIG_PREFIX "/display/hpane_position"
#define CALENDAR_CONFIG_VPANE_POS CALENDAR_CONFIG_PREFIX "/display/vpane_position"
#define CALENDAR_CONFIG_MONTH_HPANE_POS CALENDAR_CONFIG_PREFIX "/display/month_hpane_position"
#define CALENDAR_CONFIG_MONTH_VPANE_POS CALENDAR_CONFIG_PREFIX "/display/month_vpane_position"
#define CALENDAR_CONFIG_TASK_VPANE_POS CALENDAR_CONFIG_PREFIX "/display/task_vpane_position"
#define CALENDAR_CONFIG_COMPRESS_WEEKEND CALENDAR_CONFIG_PREFIX "/display/compress_weekend"
#define CALENDAR_CONFIG_SHOW_EVENT_END CALENDAR_CONFIG_PREFIX "/display/show_event_end"
#define CALENDAR_CONFIG_WORKING_DAYS CALENDAR_CONFIG_PREFIX "/display/working_days"

/* Date navigator settings */
#define CALENDAR_CONFIG_DN_SHOW_WEEK_NUMBERS CALENDAR_CONFIG_PREFIX "/date_navigator/show_week_numbers"

/* Task display settings */
#define CALENDAR_CONFIG_TASKS_SELECTED_TASKS CALENDAR_CONFIG_PREFIX "/tasks/selected_tasks"
#define CALENDAR_CONFIG_PRIMARY_TASKS CALENDAR_CONFIG_PREFIX "/tasks/primary_tasks"
#define CALENDAR_CONFIG_TASKS_HIDE_COMPLETED CALENDAR_CONFIG_PREFIX "/tasks/hide_completed"
#define CALENDAR_CONFIG_TASKS_HIDE_COMPLETED_UNITS CALENDAR_CONFIG_PREFIX "/tasks/hide_completed_units"
#define CALENDAR_CONFIG_TASKS_HIDE_COMPLETED_VALUE CALENDAR_CONFIG_PREFIX "/tasks/hide_completed_value"
#define CALENDAR_CONFIG_TASKS_DUE_TODAY_COLOR CALENDAR_CONFIG_PREFIX "/tasks/colors/due_today"
#define CALENDAR_CONFIG_TASKS_OVERDUE_COLOR CALENDAR_CONFIG_PREFIX "/tasks/colors/overdue"

/* Prompt settings */
#define CALENDAR_CONFIG_PROMPT_DELETE CALENDAR_CONFIG_PREFIX "/prompts/confirm_delete"
#define CALENDAR_CONFIG_PROMPT_PURGE CALENDAR_CONFIG_PREFIX "/prompts/confirm_purge"

/* Default reminder */
#define CALENDAR_CONFIG_DEFAULT_REMINDER CALENDAR_CONFIG_PREFIX "/other/use_default_reminder"
#define CALENDAR_CONFIG_DEFAULT_REMINDER_INTERVAL CALENDAR_CONFIG_PREFIX "/other/default_reminder_interval"
#define CALENDAR_CONFIG_DEFAULT_REMINDER_UNITS CALENDAR_CONFIG_PREFIX "/other/default_reminder_units"

/* Free/Busy settings */
#define CALENDAR_CONFIG_PUBLISH CALENDAR_CONFIG_PREFIX"/publish/uris"
#define CALENDAR_CONFIG_TEMPLATE CALENDAR_CONFIG_PREFIX"/publish/template"

G_END_DECLS

#endif
