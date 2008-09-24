/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-task-shell-sidebar.h
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

#ifndef E_TASK_SHELL_SIDEBAR_H
#define E_TASK_SHELL_SIDEBAR_H

#include <shell/e-shell-sidebar.h>
#include <shell/e-shell-view.h>

/* Standard GObject macros */
#define E_TYPE_TASK_SHELL_SIDEBAR \
	(e_task_shell_sidebar_get_type ())
#define E_TASK_SHELL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TASK_SHELL_SIDEBAR, ETaskShellSidebar))
#define E_TASK_SHELL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TASK_SHELL_SIDEBAR, ETaskShellSidebarClass))
#define E_IS_TASK_SHELL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TASK_SHELL_SIDEBAR))
#define E_IS_TASK_SHELL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TASK_SHELL_SIDEBAR))
#define E_TASK_SHELL_SIDEBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TASK_SHELL_SIDEBAR, ETaskShellSidebarClass))

G_BEGIN_DECLS

typedef struct _ETaskShellSidebar ETaskShellSidebar;
typedef struct _ETaskShellSidebarClass ETaskShellSidebarClass;
typedef struct _ETaskShellSidebarPrivate ETaskShellSidebarPrivate;

struct _ETaskShellSidebar {
	EShellSidebar parent;
	ETaskShellSidebarPrivate *priv;
};

struct _ETaskShellSidebarClass {
	EShellSidebarClass parent_class;
};

GType		e_task_shell_sidebar_get_type	(void);
GtkWidget *	e_task_shell_sidebar_new	(EShellView *shell_view);
GtkWidget *	e_task_shell_sidebar_get_selector
						(ETaskShellSidebar *task_shell_sidebar);

G_END_DECLS

#endif /* E_TASK_SHELL_SIDEBAR_H */
