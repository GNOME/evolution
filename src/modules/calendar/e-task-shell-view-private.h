/*
 * e-task-shell-view-private.h
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

#ifndef E_TASK_SHELL_VIEW_PRIVATE_H
#define E_TASK_SHELL_VIEW_PRIVATE_H

#include "e-task-shell-view.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libecal/libecal.h>

#include "shell/e-shell-utils.h"

#include "calendar/gui/calendar-config.h"
#include "calendar/gui/comp-util.h"
#include "calendar/gui/e-cal-component-preview.h"
#include "calendar/gui/e-cal-model-tasks.h"
#include "calendar/gui/print.h"

#include "e-cal-base-shell-sidebar.h"

#include "e-task-shell-backend.h"
#include "e-task-shell-content.h"
#include "e-task-shell-view-actions.h"

/* Shorthand, requires a variable named "shell_view". */
#define ACTION(name) \
	(E_SHELL_VIEW_ACTION_##name (shell_view))

G_BEGIN_DECLS

/* Filter items are displayed in ascending order.
 * Non-negative values are reserved for categories. */
enum {
	TASK_FILTER_ANY_CATEGORY = -11,
	TASK_FILTER_UNMATCHED = -10,
	TASK_FILTER_UNCOMPLETED_TASKS = -9,
	TASK_FILTER_NEXT_7_DAYS_TASKS = -8,
	TASK_FILTER_STARTED = -7,
	TASK_FILTER_ACTIVE_TASKS = -6,
	TASK_FILTER_OVERDUE_TASKS = -5,
	TASK_FILTER_COMPLETED_TASKS = -4,
	TASK_FILTER_CANCELLED_TASKS = -3,
	TASK_FILTER_SCHEDULED_TASKS = -2,
	TASK_FILTER_TASKS_WITH_ATTACHMENTS = -1
};

/* Search items are displayed in ascending order. */
enum {
	TASK_SEARCH_ADVANCED = -1,
	TASK_SEARCH_SUMMARY_CONTAINS,
	TASK_SEARCH_DESCRIPTION_CONTAINS,
	TASK_SEARCH_ANY_FIELD_CONTAINS
};

struct _ETaskShellViewPrivate {

	/* These are just for convenience. */
	ETaskShellBackend *task_shell_backend;
	ETaskShellContent *task_shell_content;
	ECalBaseShellSidebar *task_shell_sidebar;

	EClientCache *client_cache;
	gulong backend_error_handler_id;

	ETaskTable *task_table;
	gulong open_component_handler_id;
	gulong popup_event_handler_id;
	gulong selection_change_1_handler_id;
	gulong selection_change_2_handler_id;

	ECalModel *model;
	gulong model_changed_handler_id;
	gulong model_rows_deleted_handler_id;
	gulong model_rows_inserted_handler_id;
	gulong rows_appended_handler_id;

	ESourceSelector *selector;
	gulong selector_popup_event_handler_id;
	gulong primary_selection_changed_handler_id;

	/* org.gnome.evolution.calendar */
	GSettings *settings;
	gulong settings_hide_completed_tasks_handler_id;
	gulong settings_hide_completed_tasks_units_handler_id;
	gulong settings_hide_completed_tasks_value_handler_id;
	gulong settings_hide_cancelled_tasks_handler_id;

	guint update_timeout;
	guint update_completed_timeout;

	guint confirm_purge : 1;

	GHashTable *old_settings;
};

void		e_task_shell_view_private_init
					(ETaskShellView *task_shell_view);
void		e_task_shell_view_private_constructed
					(ETaskShellView *task_shell_view);
void		e_task_shell_view_private_dispose
					(ETaskShellView *task_shell_view);
void		e_task_shell_view_private_finalize
					(ETaskShellView *task_shell_view);

/* Private Utilities */

void		e_task_shell_view_actions_init
					(ETaskShellView *task_shell_view);
void		e_task_shell_view_open_task
					(ETaskShellView *task_shell_view,
					 ECalModelComponent *comp_data,
					 gboolean force_attendees);
void		e_task_shell_view_delete_completed
					(ETaskShellView *task_shell_view);
void		e_task_shell_view_update_sidebar
					(ETaskShellView *task_shell_view);
void		e_task_shell_view_update_search_filter
					(ETaskShellView *task_shell_view);

G_END_DECLS

#endif /* E_TASK_SHELL_VIEW_PRIVATE_H */
