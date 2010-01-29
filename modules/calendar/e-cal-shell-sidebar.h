/*
 * e-cal-shell-sidebar.h
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

#ifndef E_CAL_SHELL_SIDEBAR_H
#define E_CAL_SHELL_SIDEBAR_H

#include <libecal/e-cal.h>
#include <libedataserverui/e-source-selector.h>

#include <shell/e-shell-sidebar.h>
#include <shell/e-shell-view.h>
#include <misc/e-calendar.h>

/* Standard GObject macros */
#define E_TYPE_CAL_SHELL_SIDEBAR \
	(e_cal_shell_sidebar_get_type ())
#define E_CAL_SHELL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_SHELL_SIDEBAR, ECalShellSidebar))
#define E_CAL_SHELL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_SHELL_SIDEBAR, ECalShellSidebarClass))
#define E_IS_CAL_SHELL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_SHELL_SIDEBAR))
#define E_IS_CAL_SHELL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_SHELL_SIDEBAR))
#define E_CAL_SHELL_SIDEBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_SHELL_SIDEBAR, ECalShellSidebarClass))

G_BEGIN_DECLS

typedef struct _ECalShellSidebar ECalShellSidebar;
typedef struct _ECalShellSidebarClass ECalShellSidebarClass;
typedef struct _ECalShellSidebarPrivate ECalShellSidebarPrivate;

enum {
	E_CAL_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE		= 1 << 0,
	E_CAL_SHELL_SIDEBAR_CAN_DELETE_PRIMARY_SOURCE	= 1 << 1,
	E_CAL_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_SYSTEM	= 1 << 2,
	E_CAL_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH	= 1 << 3
};

struct _ECalShellSidebar {
	EShellSidebar parent;
	ECalShellSidebarPrivate *priv;
};

struct _ECalShellSidebarClass {
	EShellSidebarClass parent_class;

	/* Signals */
	void	(*client_added)			(ECalShellSidebar *cal_shell_sidebar,
						 ECal *client);
	void	(*client_removed)		(ECalShellSidebar *cal_shell_sidebar,
						 ECal *client);
	void	(*status_message)		(ECalShellSidebar *cal_shell_sidebar,
						 const gchar *status_message);
};

GType		e_cal_shell_sidebar_get_type	(void);
void		e_cal_shell_sidebar_register_type
					(GTypeModule *type_module);
GtkWidget *	e_cal_shell_sidebar_new	(EShellView *shell_view);
GList *		e_cal_shell_sidebar_get_clients
					(ECalShellSidebar *cal_shell_sidebar);
ECalendar *	e_cal_shell_sidebar_get_date_navigator
					(ECalShellSidebar *cal_shell_sidebar);
ECal *		e_cal_shell_sidebar_get_default_client
					(ECalShellSidebar *cal_shell_sidebar);
ESourceSelector *
		e_cal_shell_sidebar_get_selector
					(ECalShellSidebar *cal_shell_sidebar);
void		e_cal_shell_sidebar_add_source
					(ECalShellSidebar *cal_shell_sidebar,
					 ESource *source);
void		e_cal_shell_sidebar_remove_source
					(ECalShellSidebar *cal_shell_sidebar,
					 ESource *source);

G_END_DECLS

#endif /* E_CAL_SHELL_SIDEBAR_H */
