/*
 * e-cal-shell-view-actions.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
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
#define E_SHELL_WINDOW_ACTION_CALENDAR_REFRESH(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-refresh")
#define E_SHELL_WINDOW_ACTION_CALENDAR_RENAME(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-rename")
#define E_SHELL_WINDOW_ACTION_CALENDAR_SEARCH_PREV(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-search-prev")
#define E_SHELL_WINDOW_ACTION_CALENDAR_SEARCH_NEXT(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-search-next")
#define E_SHELL_WINDOW_ACTION_CALENDAR_SEARCH_STOP(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-search-stop")
#define E_SHELL_WINDOW_ACTION_CALENDAR_SELECT_ALL(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-select-all")
#define E_SHELL_WINDOW_ACTION_CALENDAR_SELECT_ONE(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-select-one")
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
#define E_SHELL_WINDOW_ACTION_EVENT_DELEGATE(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-delegate")
#define E_SHELL_WINDOW_ACTION_EVENT_DELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-delete")
#define E_SHELL_WINDOW_ACTION_EVENT_DELETE_OCCURRENCE(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-delete-occurrence")
#define E_SHELL_WINDOW_ACTION_EVENT_DELETE_OCCURRENCE_ALL(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-delete-occurrence-all")
#define E_SHELL_WINDOW_ACTION_EVENT_FORWARD(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-forward")
#define E_SHELL_WINDOW_ACTION_EVENT_OPEN(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-open")
#define E_SHELL_WINDOW_ACTION_EVENT_PRINT(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-print")
#define E_SHELL_WINDOW_ACTION_EVENT_SAVE_AS(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-save-as")
#define E_SHELL_WINDOW_ACTION_EVENT_SCHEDULE(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-schedule")
#define E_SHELL_WINDOW_ACTION_EVENT_SCHEDULE_APPOINTMENT(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-schedule-appointment")
#define E_SHELL_WINDOW_ACTION_EVENT_REPLY(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-reply")
#define E_SHELL_WINDOW_ACTION_EVENT_REPLY_ALL(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-reply-all")
#define E_SHELL_WINDOW_ACTION_EVENT_OCCURRENCE_MOVABLE(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-occurrence-movable")
#define E_SHELL_WINDOW_ACTION_EVENT_MEETING_NEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "event-meeting-new")

/* Memo Pad Actions */
#define E_SHELL_WINDOW_ACTION_CALENDAR_MEMOPAD_FORWARD(window) \
        E_SHELL_WINDOW_ACTION ((window), "calendar-memopad-forward")
#define E_SHELL_WINDOW_ACTION_CALENDAR_MEMOPAD_NEW(window) \
        E_SHELL_WINDOW_ACTION ((window), "calendar-memopad-new")
#define E_SHELL_WINDOW_ACTION_CALENDAR_MEMOPAD_OPEN(window) \
        E_SHELL_WINDOW_ACTION ((window), "calendar-memopad-open")
#define E_SHELL_WINDOW_ACTION_CALENDAR_MEMOPAD_OPEN_URL(window) \
        E_SHELL_WINDOW_ACTION ((window), "calendar-memopad-open-url")
#define E_SHELL_WINDOW_ACTION_CALENDAR_MEMOPAD_PRINT(window) \
        E_SHELL_WINDOW_ACTION ((window), "calendar-memopad-print")
#define E_SHELL_WINDOW_ACTION_CALENDAR_MEMOPAD_SAVE_AS(window) \
        E_SHELL_WINDOW_ACTION ((window), "calendar-memopad-save-as")

/* Task Pad Actions */
#define E_SHELL_WINDOW_ACTION_CALENDAR_TASKPAD_ASSIGN(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-taskpad-assign")
#define E_SHELL_WINDOW_ACTION_CALENDAR_TASKPAD_FORWARD(window) \
        E_SHELL_WINDOW_ACTION ((window), "calendar-taskpad-forward")
#define E_SHELL_WINDOW_ACTION_CALENDAR_TASKPAD_MARK_COMPLETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-taskpad-mark-complete")
#define E_SHELL_WINDOW_ACTION_CALENDAR_TASKPAD_MARK_INCOMPLETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-taskpad-mark-incomplete")
#define E_SHELL_WINDOW_ACTION_CALENDAR_TASKPAD_NEW(window) \
        E_SHELL_WINDOW_ACTION ((window), "calendar-taskpad-new")
#define E_SHELL_WINDOW_ACTION_CALENDAR_TASKPAD_OPEN(window) \
        E_SHELL_WINDOW_ACTION ((window), "calendar-taskpad-open")
#define E_SHELL_WINDOW_ACTION_CALENDAR_TASKPAD_OPEN_URL(window) \
        E_SHELL_WINDOW_ACTION ((window), "calendar-taskpad-open-url")
#define E_SHELL_WINDOW_ACTION_CALENDAR_TASKPAD_PRINT(window) \
        E_SHELL_WINDOW_ACTION ((window), "calendar-taskpad-print")
#define E_SHELL_WINDOW_ACTION_CALENDAR_TASKPAD_SAVE_AS(window) \
        E_SHELL_WINDOW_ACTION ((window), "calendar-taskpad-save-as")

/* Calendar Query Actions */
#define E_SHELL_WINDOW_ACTION_CALENDAR_FILTER_ACTIVE_APPOINTMENTS(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-filter-active-appointments")
#define E_SHELL_WINDOW_ACTION_CALENDAR_FILTER_ANY_CATEGORY(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-filter-any-category")
#define E_SHELL_WINDOW_ACTION_CALENDAR_FILTER_NEXT_7_DAYS_APPOINTMENTS(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-filter-next-7-days-appointments")
#define E_SHELL_WINDOW_ACTION_CALENDAR_FILTER_OCCURS_LESS_THAN_5_TIMES(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-filter-occurs-less-than-5-times")
#define E_SHELL_WINDOW_ACTION_CALENDAR_FILTER_UNMATCHED(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-filter-unmatched")
#define E_SHELL_WINDOW_ACTION_CALENDAR_SEARCH_ADVANCED_HIDDEN(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-search-advanced-hidden")
#define E_SHELL_WINDOW_ACTION_CALENDAR_SEARCH_ANY_FIELD_CONTAINS(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-search-any-field-contains")
#define E_SHELL_WINDOW_ACTION_CALENDAR_SEARCH_DESCRIPTION_CONTAINS(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-search-description-contains")
#define E_SHELL_WINDOW_ACTION_CALENDAR_SEARCH_SUMMARY_CONTAINS(window) \
	E_SHELL_WINDOW_ACTION ((window), "calendar-search-summary-contains")

/* Action Groups */
#define E_SHELL_WINDOW_ACTION_GROUP_CALENDAR(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "calendar")
#define E_SHELL_WINDOW_ACTION_GROUP_CALENDAR_FILTER(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "calendar-filter")

#endif /* E_CAL_SHELL_VIEW_ACTIONS_H */
