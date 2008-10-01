/*
 * e-task-shell-view-actions.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>  
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
#define E_SHELL_WINDOW_ACTION_TASK_CLIPBOARD_COPY(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-clipboard-copy")
#define E_SHELL_WINDOW_ACTION_TASK_CLIPBOARD_CUT(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-clibpard-cut")
#define E_SHELL_WINDOW_ACTION_TASK_CLIPBOARD_PASTE(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-clipboard-paste")
#define E_SHELL_WINDOW_ACTION_TASK_DELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-delete")
#define E_SHELL_WINDOW_ACTION_TASK_FORWARD(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-forward")
#define E_SHELL_WINDOW_ACTION_TASK_MARK_COMPLETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-mark-complete")
#define E_SHELL_WINDOW_ACTION_TASK_OPEN(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-open")
#define E_SHELL_WINDOW_ACTION_TASK_PREVIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-preview")
#define E_SHELL_WINDOW_ACTION_TASK_PRINT(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-print")
#define E_SHELL_WINDOW_ACTION_TASK_PRINT_PREVIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-print-preview")
#define E_SHELL_WINDOW_ACTION_TASK_PURGE(window) \
	E_SHELL_WINDOW_ACTION ((window), "task-purge")

/* Action Groups */
#define E_SHELL_WINDOW_ACTION_GROUP_TASKS(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "tasks")

#endif /* E_TASK_SHELL_VIEW_ACTIONS_H */
