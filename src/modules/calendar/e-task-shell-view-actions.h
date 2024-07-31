/*
 * e-task-shell-view-actions.h
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

#ifndef E_TASK_SHELL_VIEW_ACTIONS_H
#define E_TASK_SHELL_VIEW_ACTIONS_H

#include <shell/e-shell-view.h>

/* Task Actions */
#define E_SHELL_VIEW_ACTION_TASK_ASSIGN(view) \
	E_SHELL_VIEW_ACTION ((view), "task-assign")
#define E_SHELL_VIEW_ACTION_TASK_BULK_EDIT(view) \
	E_SHELL_VIEW_ACTION ((view), "task-bulk-edit")
#define E_SHELL_VIEW_ACTION_TASK_DELETE(view) \
	E_SHELL_VIEW_ACTION ((view), "task-delete")
#define E_SHELL_VIEW_ACTION_TASK_FIND(view) \
	E_SHELL_VIEW_ACTION ((view), "task-find")
#define E_SHELL_VIEW_ACTION_TASK_FORWARD(view) \
	E_SHELL_VIEW_ACTION ((view), "task-forward")
#define E_SHELL_VIEW_ACTION_TASK_MARK_COMPLETE(view) \
	E_SHELL_VIEW_ACTION ((view), "task-mark-complete")
#define E_SHELL_VIEW_ACTION_TASK_MARK_INCOMPLETE(view) \
	E_SHELL_VIEW_ACTION ((view), "task-mark-incomplete")
#define E_SHELL_VIEW_ACTION_TASK_NEW(view) \
	E_SHELL_VIEW_ACTION ((view), "task-new")
#define E_SHELL_VIEW_ACTION_TASK_OPEN(view) \
	E_SHELL_VIEW_ACTION ((view), "task-open")
#define E_SHELL_VIEW_ACTION_TASK_OPEN_URL(view) \
	E_SHELL_VIEW_ACTION ((view), "task-open-url")
#define E_SHELL_VIEW_ACTION_TASK_PREVIEW(view) \
	E_SHELL_VIEW_ACTION ((view), "task-preview")
#define E_SHELL_VIEW_ACTION_TASK_PRINT(view) \
	E_SHELL_VIEW_ACTION ((view), "task-print")
#define E_SHELL_VIEW_ACTION_TASK_PURGE(view) \
	E_SHELL_VIEW_ACTION ((view), "task-purge")
#define E_SHELL_VIEW_ACTION_TASK_SAVE_AS(view) \
	E_SHELL_VIEW_ACTION ((view), "task-save-as")
#define E_SHELL_VIEW_ACTION_TASK_VIEW_CLASSIC(view) \
	E_SHELL_VIEW_ACTION ((view), "task-view-classic")
#define E_SHELL_VIEW_ACTION_TASK_VIEW_VERTICAL(view) \
	E_SHELL_VIEW_ACTION ((view), "task-view-vertical")

/* Task List Actions */
#define E_SHELL_VIEW_ACTION_TASK_LIST_COPY(view) \
	E_SHELL_VIEW_ACTION ((view), "task-list-copy")
#define E_SHELL_VIEW_ACTION_TASK_LIST_DELETE(view) \
	E_SHELL_VIEW_ACTION ((view), "task-list-delete")
#define E_SHELL_VIEW_ACTION_TASK_LIST_NEW(view) \
	E_SHELL_VIEW_ACTION ((view), "task-list-new")
#define E_SHELL_VIEW_ACTION_TASK_LIST_PRINT(view) \
	E_SHELL_VIEW_ACTION ((view), "task-list-print")
#define E_SHELL_VIEW_ACTION_TASK_LIST_PRINT_PREVIEW(view) \
	E_SHELL_VIEW_ACTION ((view), "task-list-print-preview")
#define E_SHELL_VIEW_ACTION_TASK_LIST_PROPERTIES(view) \
	E_SHELL_VIEW_ACTION ((view), "task-list-properties")
#define E_SHELL_VIEW_ACTION_TASK_LIST_REFRESH(view) \
	E_SHELL_VIEW_ACTION ((view), "task-list-refresh")
#define E_SHELL_VIEW_ACTION_TASK_LIST_REFRESH_BACKEND(view) \
	E_SHELL_VIEW_ACTION ((view), "task-list-refresh-backend")
#define E_SHELL_VIEW_ACTION_TASK_LIST_RENAME(view) \
	E_SHELL_VIEW_ACTION ((view), "task-list-rename")
#define E_SHELL_VIEW_ACTION_TASK_LIST_SELECT_ALL(view) \
	E_SHELL_VIEW_ACTION ((view), "task-list-select-all")
#define E_SHELL_VIEW_ACTION_TASK_LIST_SELECT_ONE(view) \
	E_SHELL_VIEW_ACTION ((view), "task-list-select-one")

/* Task Query Actions */
#define E_SHELL_VIEW_ACTION_TASK_FILTER_ACTIVE_TASKS(view) \
	E_SHELL_VIEW_ACTION ((view), "task-filter-active-tasks")
#define E_SHELL_VIEW_ACTION_TASK_FILTER_ANY_CATEGORY(view) \
	E_SHELL_VIEW_ACTION ((view), "task-filter-any-category")
#define E_SHELL_VIEW_ACTION_TASK_FILTER_CANCELLED_TASKS(view) \
	E_SHELL_VIEW_ACTION ((view), "task-filter-cancelled-tasks")
#define E_SHELL_VIEW_ACTION_TASK_FILTER_COMPLETED_TASKS(view) \
	E_SHELL_VIEW_ACTION ((view), "task-filter-completed-tasks")
#define E_SHELL_VIEW_ACTION_TASK_FILTER_NEXT_7_DAYS_TASKS(view) \
	E_SHELL_VIEW_ACTION ((view), "task-filter-next-7-days-tasks")
#define E_SHELL_VIEW_ACTION_TASK_FILTER_OVERDUE_TASKS(view) \
	E_SHELL_VIEW_ACTION ((view), "task-filter-overdue-tasks")
#define E_SHELL_VIEW_ACTION_TASK_FILTER_SCHEDULED_TASKS(view) \
	E_SHELL_VIEW_ACTION ((view), "task-filter-scheduled-tasks")
#define E_SHELL_VIEW_ACTION_TASK_FILTER_TASKS_WITH_ATTACHMENTS(view) \
	E_SHELL_VIEW_ACTION ((view), "task-filter-tasks-with-attachments")
#define E_SHELL_VIEW_ACTION_TASK_FILTER_TASK_FILTER_STARTED(view) \
	E_SHELL_VIEW_ACTION ((view), "task-filter-started")
#define E_SHELL_VIEW_ACTION_TASK_FILTER_UNMATCHED(view) \
	E_SHELL_VIEW_ACTION ((view), "task-filter-unmatched")
#define E_SHELL_VIEW_ACTION_TASK_SEARCH_ADVANCED_HIDDEN(view) \
	E_SHELL_VIEW_ACTION ((view), "task-search-advanced-hidden")
#define E_SHELL_VIEW_ACTION_TASK_SEARCH_ANY_FIELD_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "task-search-any-field-contains")
#define E_SHELL_VIEW_ACTION_TASK_SEARCH_DESCRIPTION_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "task-search-description-contains")
#define E_SHELL_VIEW_ACTION_TASK_SEARCH_SUMMARY_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "task-search-summary-contains")

#endif /* E_TASK_SHELL_VIEW_ACTIONS_H */
