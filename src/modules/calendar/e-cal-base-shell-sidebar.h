/*
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Milan Crha <mcrha@redhat.com>
 */

#ifndef E_CAL_BASE_SHELL_SIDEBAR_H
#define E_CAL_BASE_SHELL_SIDEBAR_H

#include <libecal/libecal.h>

#include <shell/e-shell-sidebar.h>
#include <shell/e-shell-view.h>

/* Standard GObject macros */
#define E_TYPE_CAL_BASE_SHELL_SIDEBAR \
	(e_cal_base_shell_sidebar_get_type ())
#define E_CAL_BASE_SHELL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_BASE_SHELL_SIDEBAR, ECalBaseShellSidebar))
#define E_CAL_BASE_SHELL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_BASE_SHELL_SIDEBAR, ECalBaseShellSidebarClass))
#define E_IS_CAL_BASE_SHELL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_BASE_SHELL_SIDEBAR))
#define E_IS_CAL_BASE_SHELL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_BASE_SHELL_SIDEBAR))
#define E_CAL_BASE_SHELL_SIDEBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_BASE_SHELL_SIDEBAR, ECalBaseShellSidebarClass))

G_BEGIN_DECLS

typedef struct _ECalBaseShellSidebar ECalBaseShellSidebar;
typedef struct _ECalBaseShellSidebarClass ECalBaseShellSidebarClass;
typedef struct _ECalBaseShellSidebarPrivate ECalBaseShellSidebarPrivate;

typedef void (* ECalBaseShellSidebarOpenFunc)	(ECalBaseShellSidebar *cal_base_shell_sidebar,
						 EClient *client,
						 gpointer user_data);

enum {
	E_CAL_BASE_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE = 1 << 0,
	E_CAL_BASE_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_WRITABLE = 1 << 1,
	E_CAL_BASE_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOVABLE = 1 << 2,
	E_CAL_BASE_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOTE_CREATABLE = 1 << 3,
	E_CAL_BASE_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOTE_DELETABLE = 1 << 4,
	E_CAL_BASE_SHELL_SIDEBAR_PRIMARY_SOURCE_IN_COLLECTION = 1 << 5,
	E_CAL_BASE_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH = 1 << 6,
	E_CAL_BASE_SHELL_SIDEBAR_ALL_SOURCES_SELECTED = 1 << 7,
	E_CAL_BASE_SHELL_SIDEBAR_CLICKED_SOURCE_IS_PRIMARY = 1 << 8,
	E_CAL_BASE_SHELL_SIDEBAR_CLICKED_SOURCE_IS_COLLECTION = 1 << 9
};

struct _ECalBaseShellSidebar {
	EShellSidebar parent;
	ECalBaseShellSidebarPrivate *priv;
};

struct _ECalBaseShellSidebarClass {
	EShellSidebarClass parent_class;

	/* Signals */
	void	(*client_opened)	(ECalBaseShellSidebar *cal_shell_sidebar,
					 ECalClient *client);
	void	(*client_closed)	(ECalBaseShellSidebar *cal_shell_sidebar,
					 ESource *source);
};

GType		e_cal_base_shell_sidebar_get_type	(void);
void		e_cal_base_shell_sidebar_type_register	(GTypeModule *type_module);
GtkWidget *	e_cal_base_shell_sidebar_new		(EShellView *shell_view);

ECalendar *	e_cal_base_shell_sidebar_get_date_navigator
							(ECalBaseShellSidebar *cal_base_shell_sidebar);
ESourceSelector *
		e_cal_base_shell_sidebar_get_selector	(ECalBaseShellSidebar *cal_base_shell_sidebar);
void		e_cal_base_shell_sidebar_ensure_sources_open
							(ECalBaseShellSidebar *cal_base_shell_sidebar);
void		e_cal_base_shell_sidebar_open_source	(ECalBaseShellSidebar *cal_base_shell_sidebar,
							 ESource *source,
							 ECalBaseShellSidebarOpenFunc cb,
							 gpointer cb_user_data);

G_END_DECLS

#endif /* E_CAL_BASE_SHELL_SIDEBAR_H */
