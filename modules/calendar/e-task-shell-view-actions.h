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

#include <shell/e-shell-window-actions.h>

/* Task Actions */
#define E_SHELL_WINDOW_ACTION_TASK_ASSIGN(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-assign")
#define E_SHELL_WINDOW_ACTION_TASK_DELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-delete")
#define E_SHELL_WINDOW_ACTION_TASK_FIND(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-find")
#define E_SHELL_WINDOW_ACTION_TASK_FORWARD(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-forward")
#define E_SHELL_WINDOW_ACTION_TASK_MARK_COMPLETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-mark-complete")
#define E_SHELL_WINDOW_ACTION_TASK_MARK_INCOMPLETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-mark-incomplete")
#define E_SHELL_WINDOW_ACTION_TASK_NEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-new")
#define E_SHELL_WINDOW_ACTION_TASK_OPEN(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-open")
#define E_SHELL_WINDOW_ACTION_TASK_OPEN_URL(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-open-url")
#define E_SHELL_WINDOW_ACTION_TASK_PREVIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-preview")
#define E_SHELL_WINDOW_ACTION_TASK_PRINT(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-print")
#define E_SHELL_WINDOW_ACTION_TASK_PURGE(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-purge")
#define E_SHELL_WINDOW_ACTION_TASK_SAVE_AS(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-save-as")
#define E_SHELL_WINDOW_ACTION_TASK_VIEW_CLASSIC(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-view-classic")
#define E_SHELL_WINDOW_ACTION_TASK_VIEW_VERTICAL(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-view-vertical")

/* Task List Actions */
#define E_SHELL_WINDOW_ACTION_TASK_LIST_COPY(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-list-copy")
#define E_SHELL_WINDOW_ACTION_TASK_LIST_DELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-list-delete")
#define E_SHELL_WINDOW_ACTION_TASK_LIST_NEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-list-new")
#define E_SHELL_WINDOW_ACTION_TASK_LIST_PRINT(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-list-print")
#define E_SHELL_WINDOW_ACTION_TASK_LIST_PRINT_PREVIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-list-print-preview")
#define E_SHELL_WINDOW_ACTION_TASK_LIST_PROPERTIES(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-list-properties")
#define E_SHELL_WINDOW_ACTION_TASK_LIST_REFRESH(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-list-refresh")
#define E_SHELL_WINDOW_ACTION_TASK_LIST_RENAME(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-list-rename")
#define E_SHELL_WINDOW_ACTION_TASK_LIST_SELECT_ALL(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-list-select-all")
#define E_SHELL_WINDOW_ACTION_TASK_LIST_SELECT_ONE(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-list-select-one")

/* Task Query Actions */
#define E_SHELL_WINDOW_ACTION_TASK_FILTER_ACTIVE_TASKS(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-filter-active-tasks")
#define E_SHELL_WINDOW_ACTION_TASK_FILTER_ANY_CATEGORY(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-filter-any-category")
#define E_SHELL_WINDOW_ACTION_TASK_FILTER_COMPLETED_TASKS(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-filter-completed-tasks")
#define E_SHELL_WINDOW_ACTION_TASK_FILTER_NEXT_7_DAYS_TASKS(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-filter-next-7-days-tasks")
#define E_SHELL_WINDOW_ACTION_TASK_FILTER_OVERDUE_TASKS(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-filter-overdue-tasks")
#define E_SHELL_WINDOW_ACTION_TASK_FILTER_TASKS_WITH_ATTACHMENTS(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-filter-tasks-with-attachments")
#define E_SHELL_WINDOW_ACTION_TASK_FILTER_UNMATCHED(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-filter-unmatched")
#define E_SHELL_WINDOW_ACTION_TASK_SEARCH_ADVANCED_HIDDEN(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-search-advanced-hidden")
#define E_SHELL_WINDOW_ACTION_TASK_SEARCH_ANY_FIELD_CONTAINS(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-search-any-field-contains")
#define E_SHELL_WINDOW_ACTION_TASK_SEARCH_DESCRIPTION_CONTAINS(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-search-description-contains")
#define E_SHELL_WINDOW_ACTION_TASK_SEARCH_SUMMARY_CONTAINS(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-search-summary-contains")

/* Action Groups */
#define E_SHELL_WINDOW_ACTION_GROUP_TASKS(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "tasks")
#define E_SHELL_WINDOW_ACTION_GROUP_TASKS_FILTER(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "tasks-filter")

#endif /* E_TASK_SHELL_VIEW_ACTIONS_H */
