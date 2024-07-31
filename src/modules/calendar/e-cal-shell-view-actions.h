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

#include <shell/e-shell-view.h>

/* Calendar Actions */
#define E_SHELL_VIEW_ACTION_CALENDAR_COPY(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-copy")
#define E_SHELL_VIEW_ACTION_CALENDAR_DELETE(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-delete")
#define E_SHELL_VIEW_ACTION_CALENDAR_GO_BACK(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-go-back")
#define E_SHELL_VIEW_ACTION_CALENDAR_GO_FORWARD(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-go-forward")
#define E_SHELL_VIEW_ACTION_CALENDAR_GO_TODAY(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-go-today")
#define E_SHELL_VIEW_ACTION_CALENDAR_JUMP_TO(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-jump-to")
#define E_SHELL_VIEW_ACTION_CALENDAR_NEW(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-new")
#define E_SHELL_VIEW_ACTION_CALENDAR_PREVIEW(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-preview")
#define E_SHELL_VIEW_ACTION_CALENDAR_PREVIEW_MENU(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-preview-menu")
#define E_SHELL_VIEW_ACTION_CALENDAR_PREVIEW_HORIZONTAL(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-preview-horizontal")
#define E_SHELL_VIEW_ACTION_CALENDAR_PREVIEW_VERTICAL(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-preview-vertical")
#define E_SHELL_VIEW_ACTION_CALENDAR_PRINT(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-print")
#define E_SHELL_VIEW_ACTION_CALENDAR_PRINT_PREVIEW(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-print-preview")
#define E_SHELL_VIEW_ACTION_CALENDAR_PROPERTIES(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-properties")
#define E_SHELL_VIEW_ACTION_CALENDAR_PURGE(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-purge")
#define E_SHELL_VIEW_ACTION_CALENDAR_REFRESH(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-refresh")
#define E_SHELL_VIEW_ACTION_CALENDAR_REFRESH_BACKEND(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-refresh-backend")
#define E_SHELL_VIEW_ACTION_CALENDAR_RENAME(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-rename")
#define E_SHELL_VIEW_ACTION_CALENDAR_SEARCH_PREV(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-search-prev")
#define E_SHELL_VIEW_ACTION_CALENDAR_SEARCH_NEXT(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-search-next")
#define E_SHELL_VIEW_ACTION_CALENDAR_SEARCH_STOP(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-search-stop")
#define E_SHELL_VIEW_ACTION_CALENDAR_SELECT_ALL(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-select-all")
#define E_SHELL_VIEW_ACTION_CALENDAR_SELECT_ONE(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-select-one")
#define E_SHELL_VIEW_ACTION_CALENDAR_SHOW_TAG_VPANE(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-show-tag-vpane")
#define E_SHELL_VIEW_ACTION_CALENDAR_VIEW_DAY(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-view-day")
#define E_SHELL_VIEW_ACTION_CALENDAR_VIEW_LIST(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-view-list")
#define E_SHELL_VIEW_ACTION_CALENDAR_VIEW_MONTH(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-view-month")
#define E_SHELL_VIEW_ACTION_CALENDAR_VIEW_WEEK(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-view-week")
#define E_SHELL_VIEW_ACTION_CALENDAR_VIEW_WORKWEEK(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-view-workweek")
#define E_SHELL_VIEW_ACTION_CALENDAR_VIEW_YEAR(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-view-year")

/* Event Actions */
#define E_SHELL_VIEW_ACTION_EVENT_DELEGATE(view) \
	E_SHELL_VIEW_ACTION ((view), "event-delegate")
#define E_SHELL_VIEW_ACTION_EVENT_DELETE(view) \
	E_SHELL_VIEW_ACTION ((view), "event-delete")
#define E_SHELL_VIEW_ACTION_EVENT_DELETE_OCCURRENCE(view) \
	E_SHELL_VIEW_ACTION ((view), "event-delete-occurrence")
#define E_SHELL_VIEW_ACTION_EVENT_DELETE_OCCURRENCE_THIS_AND_FUTURE(view) \
	E_SHELL_VIEW_ACTION ((view), "event-delete-occurrence-this-and-future")
#define E_SHELL_VIEW_ACTION_EVENT_DELETE_OCCURRENCE_ALL(view) \
	E_SHELL_VIEW_ACTION ((view), "event-delete-occurrence-all")
#define E_SHELL_VIEW_ACTION_EVENT_EDIT_AS_NEW(view) \
	E_SHELL_VIEW_ACTION ((view), "event-edit-as-new")
#define E_SHELL_VIEW_ACTION_EVENT_FORWARD(view) \
	E_SHELL_VIEW_ACTION ((view), "event-forward")
#define E_SHELL_VIEW_ACTION_EVENT_OPEN(view) \
	E_SHELL_VIEW_ACTION ((view), "event-open")
#define E_SHELL_VIEW_ACTION_EVENT_PRINT(view) \
	E_SHELL_VIEW_ACTION ((view), "event-print")
#define E_SHELL_VIEW_ACTION_EVENT_SAVE_AS(view) \
	E_SHELL_VIEW_ACTION ((view), "event-save-as")
#define E_SHELL_VIEW_ACTION_EVENT_SCHEDULE(view) \
	E_SHELL_VIEW_ACTION ((view), "event-schedule")
#define E_SHELL_VIEW_ACTION_EVENT_SCHEDULE_APPOINTMENT(view) \
	E_SHELL_VIEW_ACTION ((view), "event-schedule-appointment")
#define E_SHELL_VIEW_ACTION_EVENT_REPLY(view) \
	E_SHELL_VIEW_ACTION ((view), "event-reply")
#define E_SHELL_VIEW_ACTION_EVENT_REPLY_ALL(view) \
	E_SHELL_VIEW_ACTION ((view), "event-reply-all")
#define E_SHELL_VIEW_ACTION_EVENT_OCCURRENCE_MOVABLE(view) \
	E_SHELL_VIEW_ACTION ((view), "event-occurrence-movable")
#define E_SHELL_VIEW_ACTION_EVENT_MEETING_NEW(view) \
	E_SHELL_VIEW_ACTION ((view), "event-meeting-new")
#define E_SHELL_VIEW_ACTION_EVENT_RSVP_SUBMENU(view) \
	E_SHELL_VIEW_ACTION ((view), "event-rsvp-submenu")
#define E_SHELL_VIEW_ACTION_EVENT_RSVP_ACCEPT(view) \
	E_SHELL_VIEW_ACTION ((view), "event-rsvp-accept")
#define E_SHELL_VIEW_ACTION_EVENT_RSVP_ACCEPT_1(view) \
	E_SHELL_VIEW_ACTION ((view), "event-rsvp-accept-1")
#define E_SHELL_VIEW_ACTION_EVENT_RSVP_DECLINE(view) \
	E_SHELL_VIEW_ACTION ((view), "event-rsvp-decline")
#define E_SHELL_VIEW_ACTION_EVENT_RSVP_DECLINE_1(view) \
	E_SHELL_VIEW_ACTION ((view), "event-rsvp-decline-1")
#define E_SHELL_VIEW_ACTION_EVENT_RSVP_TENTATIVE(view) \
	E_SHELL_VIEW_ACTION ((view), "event-rsvp-tentative")
#define E_SHELL_VIEW_ACTION_EVENT_RSVP_TENTATIVE_1(view) \
	E_SHELL_VIEW_ACTION ((view), "event-rsvp-tentative-1")

/* Memo Pad Actions */
#define E_SHELL_VIEW_ACTION_CALENDAR_MEMOPAD_FORWARD(view) \
        E_SHELL_VIEW_ACTION ((view), "calendar-memopad-forward")
#define E_SHELL_VIEW_ACTION_CALENDAR_MEMOPAD_NEW(view) \
        E_SHELL_VIEW_ACTION ((view), "calendar-memopad-new")
#define E_SHELL_VIEW_ACTION_CALENDAR_MEMOPAD_OPEN(view) \
        E_SHELL_VIEW_ACTION ((view), "calendar-memopad-open")
#define E_SHELL_VIEW_ACTION_CALENDAR_MEMOPAD_OPEN_URL(view) \
        E_SHELL_VIEW_ACTION ((view), "calendar-memopad-open-url")
#define E_SHELL_VIEW_ACTION_CALENDAR_MEMOPAD_PRINT(view) \
        E_SHELL_VIEW_ACTION ((view), "calendar-memopad-print")
#define E_SHELL_VIEW_ACTION_CALENDAR_MEMOPAD_SAVE_AS(view) \
        E_SHELL_VIEW_ACTION ((view), "calendar-memopad-save-as")

/* Task Pad Actions */
#define E_SHELL_VIEW_ACTION_CALENDAR_TASKPAD_ASSIGN(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-taskpad-assign")
#define E_SHELL_VIEW_ACTION_CALENDAR_TASKPAD_FORWARD(view) \
        E_SHELL_VIEW_ACTION ((view), "calendar-taskpad-forward")
#define E_SHELL_VIEW_ACTION_CALENDAR_TASKPAD_MARK_COMPLETE(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-taskpad-mark-complete")
#define E_SHELL_VIEW_ACTION_CALENDAR_TASKPAD_MARK_INCOMPLETE(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-taskpad-mark-incomplete")
#define E_SHELL_VIEW_ACTION_CALENDAR_TASKPAD_NEW(view) \
        E_SHELL_VIEW_ACTION ((view), "calendar-taskpad-new")
#define E_SHELL_VIEW_ACTION_CALENDAR_TASKPAD_OPEN(view) \
        E_SHELL_VIEW_ACTION ((view), "calendar-taskpad-open")
#define E_SHELL_VIEW_ACTION_CALENDAR_TASKPAD_OPEN_URL(view) \
        E_SHELL_VIEW_ACTION ((view), "calendar-taskpad-open-url")
#define E_SHELL_VIEW_ACTION_CALENDAR_TASKPAD_PRINT(view) \
        E_SHELL_VIEW_ACTION ((view), "calendar-taskpad-print")
#define E_SHELL_VIEW_ACTION_CALENDAR_TASKPAD_SAVE_AS(view) \
        E_SHELL_VIEW_ACTION ((view), "calendar-taskpad-save-as")

/* Calendar Query Actions */
#define E_SHELL_VIEW_ACTION_CALENDAR_FILTER_ACTIVE_APPOINTMENTS(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-filter-active-appointments")
#define E_SHELL_VIEW_ACTION_CALENDAR_FILTER_ANY_CATEGORY(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-filter-any-category")
#define E_SHELL_VIEW_ACTION_CALENDAR_FILTER_NEXT_7_DAYS_APPOINTMENTS(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-filter-next-7-days-appointments")
#define E_SHELL_VIEW_ACTION_CALENDAR_FILTER_OCCURS_LESS_THAN_5_TIMES(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-filter-occurs-less-than-5-times")
#define E_SHELL_VIEW_ACTION_CALENDAR_FILTER_UNMATCHED(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-filter-unmatched")
#define E_SHELL_VIEW_ACTION_CALENDAR_SEARCH_ADVANCED_HIDDEN(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-search-advanced-hidden")
#define E_SHELL_VIEW_ACTION_CALENDAR_SEARCH_ANY_FIELD_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-search-any-field-contains")
#define E_SHELL_VIEW_ACTION_CALENDAR_SEARCH_DESCRIPTION_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-search-description-contains")
#define E_SHELL_VIEW_ACTION_CALENDAR_SEARCH_SUMMARY_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "calendar-search-summary-contains")

#endif /* E_CAL_SHELL_VIEW_ACTIONS_H */
