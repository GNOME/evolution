/*
 * e-task-shell-sidebar.h
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

#ifndef E_TASK_SHELL_SIDEBAR_H
#define E_TASK_SHELL_SIDEBAR_H

#include <libecal/e-cal.h>
#include <libedataserverui/e-source-selector.h>

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

enum {
	E_TASK_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE		= 1 << 0,
	E_TASK_SHELL_SIDEBAR_CAN_DELETE_PRIMARY_SOURCE	= 1 << 1,
	E_TASK_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_SYSTEM	= 1 << 2,
	E_TASK_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH	= 1 << 3
};

struct _ETaskShellSidebar {
	EShellSidebar parent;
	ETaskShellSidebarPrivate *priv;
};

struct _ETaskShellSidebarClass {
	EShellSidebarClass parent_class;

	/* Signals */
	void	(*client_added)			(ETaskShellSidebar *task_shell_sidebar,
						 ECal *client);
	void	(*client_removed)		(ETaskShellSidebar *task_shell_sidebar,
						 ECal *client);
	void	(*status_message)		(ETaskShellSidebar *task_shell_sidebar,
						 const gchar *status_message,
						 gdouble percent);
};

GType		e_task_shell_sidebar_get_type	(void);
void		e_task_shell_sidebar_register_type
					(GTypeModule *type_module);
GtkWidget *	e_task_shell_sidebar_new
					(EShellView *shell_view);
GList *		e_task_shell_sidebar_get_clients
					(ETaskShellSidebar *task_shell_sidebar);
ECal *		e_task_shell_sidebar_get_default_client
					(ETaskShellSidebar *task_shell_sidebar);
ESourceSelector *
		e_task_shell_sidebar_get_selector
					(ETaskShellSidebar *task_shell_sidebar);
void		e_task_shell_sidebar_add_source
					(ETaskShellSidebar *task_shell_sidebar,
					 ESource *source);
void		e_task_shell_sidebar_remove_source
					(ETaskShellSidebar *task_shell_sidebar,
					 ESource *source);

G_END_DECLS

#endif /* E_TASK_SHELL_SIDEBAR_H */
