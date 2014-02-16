/*
 * e-cal-shell-view-private.h
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

#ifndef E_CAL_SHELL_VIEW_PRIVATE_H
#define E_CAL_SHELL_VIEW_PRIVATE_H

#include "e-cal-shell-view.h"

#include <string.h>
#include <glib/gi18n.h>

#include <libecal/libecal.h>

#include <shell/e-shell-utils.h>

#include <calendar/gui/calendar-config.h>
#include <calendar/gui/comp-util.h>
#include <calendar/gui/e-cal-list-view.h>
#include <calendar/gui/e-cal-model-tasks.h>
#include <calendar/gui/e-calendar-view.h>
#include <calendar/gui/e-day-view.h>
#include <calendar/gui/e-week-view.h>
#include <calendar/gui/gnome-cal.h>
#include <calendar/gui/print.h>
#include <calendar/gui/dialogs/copy-source-dialog.h>
#include <calendar/gui/dialogs/event-editor.h>
#include <calendar/gui/dialogs/goto-dialog.h>
#include <calendar/gui/dialogs/memo-editor.h>
#include <calendar/gui/dialogs/select-source-dialog.h>
#include <calendar/gui/dialogs/task-editor.h>

#include "e-cal-shell-backend.h"
#include "e-cal-shell-content.h"
#include "e-cal-shell-sidebar.h"
#include "e-cal-shell-view-actions.h"

#define E_CAL_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_SHELL_VIEW, ECalShellViewPrivate))

/* Shorthand, requires a variable named "shell_window". */
#define ACTION(name) \
	(E_SHELL_WINDOW_ACTION_##name (shell_window))
#define ACTION_GROUP(name) \
	(E_SHELL_WINDOW_ACTION_GROUP_##name (shell_window))

/* ETable Specifications */
#define ETSPEC_FILENAME		"e-calendar-table.etspec"
#define CHECK_NB	5

G_BEGIN_DECLS

/* Filter items are displayed in ascending order.
 * Non-negative values are reserved for categories. */
enum {
	CALENDAR_FILTER_ANY_CATEGORY = -5,
	CALENDAR_FILTER_UNMATCHED = -4,
	CALENDAR_FILTER_ACTIVE_APPOINTMENTS = -3,
	CALENDAR_FILTER_NEXT_7_DAYS_APPOINTMENTS = -2,
	CALENDAR_FILTER_OCCURS_LESS_THAN_5_TIMES = -1
};

/* Search items are displayed in ascending order. */
enum {
	CALENDAR_SEARCH_ADVANCED = -1,
	CALENDAR_SEARCH_SUMMARY_CONTAINS,
	CALENDAR_SEARCH_DESCRIPTION_CONTAINS,
	CALENDAR_SEARCH_ANY_FIELD_CONTAINS
};

struct _ECalShellViewPrivate {

	/* These are just for convenience. */
	ECalShellBackend *cal_shell_backend;
	ECalShellContent *cal_shell_content;
	ECalShellSidebar *cal_shell_sidebar;

	/* sidebar signal handlers */
	gulong client_added_handler_id;
	gulong client_removed_handler_id;

	EShell *shell;
	gulong prepare_for_quit_handler_id;

	EClientCache *client_cache;
	gulong backend_error_handler_id;

	GnomeCalendar *calendar;
	gulong dates_shown_changed_handler_id;

	struct {
		ECalendarView *calendar_view;
		gulong popup_event_handler_id;
		gulong selection_changed_handler_id;
		gulong user_created_handler_id;
	} views[GNOME_CAL_LAST_VIEW];

	ECalModel *model;
	gulong status_message_handler_id;

	ECalendar *date_navigator;
	gulong scroll_event_handler_id;
	gulong date_range_changed_handler_id;
	gulong selection_changed_handler_id;

	ESourceSelector *selector;
	gulong selector_popup_event_handler_id;

	EMemoTable *memo_table;
	gulong memo_table_popup_event_handler_id;
	gulong memo_table_selection_change_handler_id;
	gulong memo_table_status_message_handler_id;

	ETaskTable *task_table;
	gulong task_table_popup_event_handler_id;
	gulong task_table_selection_change_handler_id;
	gulong task_table_status_message_handler_id;

	/* The last time explicitly selected by the user. */
	time_t base_view_time;

	EActivity *calendar_activity;
	EActivity *memopad_activity;
	EActivity *taskpad_activity;

	/* Time-range searching */
	EActivity *searching_activity;
	gpointer search_alert; /* weak pointer to EAlert * */
	gint search_pending_count; /* how many clients are pending */
	time_t search_time; /* current search time from */
	time_t search_min_time, search_max_time; /* time boundary for searching */
	gint search_direction; /* negative value is backward, positive is forward, zero is error; in days */
	GSList *search_hit_cache; /* pointers on time_t for matched events */

	/* Event/Task/Memo transferring */
	gpointer transfer_alert; /* weak pointer to EAlert * */

        GFileMonitor *monitors[CHECK_NB];
};

void		e_cal_shell_view_private_init
					(ECalShellView *cal_shell_view);
void		e_cal_shell_view_private_constructed
					(ECalShellView *cal_shell_view);
void		e_cal_shell_view_private_dispose
					(ECalShellView *cal_shell_view);
void		e_cal_shell_view_private_finalize
					(ECalShellView *cal_shell_view);

/* Private Utilities */

void		e_cal_shell_view_actions_init
					(ECalShellView *cal_shell_view);
void		e_cal_shell_view_execute_search
					(ECalShellView *cal_shell_view);
void		e_cal_shell_view_open_event
					(ECalShellView *cal_shell_view,
					 ECalModelComponent *comp_data);
void		e_cal_shell_view_set_status_message
					(ECalShellView *cal_shell_view,
					 const gchar *status_message,
					 gdouble percent);
void		e_cal_shell_view_transfer_item_to
					(ECalShellView *cal_shell_view,
					 ECalendarViewEvent *event,
					 ECalClient *destination_client,
					 gboolean remove);
void		e_cal_shell_view_update_sidebar
					(ECalShellView *cal_shell_view);
void		e_cal_shell_view_update_search_filter
					(ECalShellView *cal_shell_view);
void		e_cal_shell_view_search_events
					(ECalShellView *cal_shell_view,
					 gboolean search_forward);
void		e_cal_shell_view_search_stop
					(ECalShellView *cal_shell_view);

/* Memo Pad Utilities */

void		e_cal_shell_view_memopad_actions_init
					(ECalShellView *cal_shell_view);
void		e_cal_shell_view_memopad_actions_update
					(ECalShellView *cal_shell_view);
void		e_cal_shell_view_memopad_open_memo
					(ECalShellView *cal_shell_view,
					 ECalModelComponent *comp_data);
void		e_cal_shell_view_memopad_set_status_message
					(ECalShellView *cal_shell_view,
					 const gchar *status_message,
					 gdouble percent);

/* Task Pad Utilities */

void		e_cal_shell_view_taskpad_actions_init
					(ECalShellView *cal_shell_view);
void		e_cal_shell_view_taskpad_actions_update
					(ECalShellView *cal_shell_view);
void		e_cal_shell_view_taskpad_open_task
					(ECalShellView *cal_shell_view,
					 ECalModelComponent *comp_data);
void		e_cal_shell_view_taskpad_set_status_message
					(ECalShellView *cal_shell_view,
					 const gchar *status_message,
					 gdouble percent);

G_END_DECLS

#endif /* E_CAL_SHELL_VIEW_PRIVATE_H */
