/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-cal-shell-view-actions.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_CAL_SHELL_VIEW_ACTIONS_H
#define E_CAL_SHELL_VIEW_ACTIONS_H

#include <shell/e-shell-window-actions.h>

/* Calendar Actions */
#define E_SHELL_WINDOW_ACTION_CALENDAR_COPY(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-copy")
#define E_SHELL_WINDOW_ACTION_CALENDAR_DELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-delete")
#define E_SHELL_WINDOW_ACTION_CALENDAR_GO_BACK(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-go-back")
#define E_SHELL_WINDOW_ACTION_CALENDAR_GO_FORWARD(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-go-forward")
#define E_SHELL_WINDOW_ACTION_CALENDAR_GO_TODAY(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-go-today")
#define E_SHELL_WINDOW_ACTION_CALENDAR_JUMP_TO(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-jump-to")
#define E_SHELL_WINDOW_ACTION_CALENDAR_NEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-new")
#define E_SHELL_WINDOW_ACTION_CALENDAR_PRINT(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-print")
#define E_SHELL_WINDOW_ACTION_CALENDAR_PRINT_PREVIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-print-preview")
#define E_SHELL_WINDOW_ACTION_CALENDAR_PROPERTIES(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-properties")
#define E_SHELL_WINDOW_ACTION_CALENDAR_PURGE(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-purge")
#define E_SHELL_WINDOW_ACTION_CALENDAR_VIEW_DAY(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-view-day")
#define E_SHELL_WINDOW_ACTION_CALENDAR_VIEW_LIST(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-view-list")
#define E_SHELL_WINDOW_ACTION_CALENDAR_VIEW_MONTH(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-view-month")
#define E_SHELL_WINDOW_ACTION_CALENDAR_VIEW_WEEK(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-view-week")
#define E_SHELL_WINDOW_ACTION_CALENDAR_VIEW_WORKWEEK(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-view-workweek")

/* Event Actions */
#define E_SHELL_WINDOW_ACTION_EVENT_CLIPBOARD_COPY(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-clipboard-copy")
#define E_SHELL_WINDOW_ACTION_EVENT_CLIPBOARD_CUT(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-clipboard-cut")
#define E_SHELL_WINDOW_ACTION_EVENT_CLIPBOARD_PASTE(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-clipboard-paste")
#define E_SHELL_WINDOW_ACTION_EVENT_DELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-delete")
#define E_SHELL_WINDOW_ACTION_EVENT_DELETE_OCCURRENCE(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-delete-occurrence")
#define E_SHELL_WINDOW_ACTION_EVENT_DELETE_OCCURRENCE_ALL(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-delete-occurrence-all")
#define E_SHELL_WINDOW_ACTION_EVENT_OPEN(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-open")

/* Action Groups */
#define E_SHELL_WINDOW_ACTION_GROUP_CALS(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "calendars")

#endif /* E_CAL_SHELL_VIEW_ACTIONS_H */
