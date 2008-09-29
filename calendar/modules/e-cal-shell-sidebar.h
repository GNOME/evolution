/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-cal-shell-sidebar.h
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

#ifndef E_CAL_SHELL_SIDEBAR_H
#define E_CAL_SHELL_SIDEBAR_H

#include <shell/e-shell-sidebar.h>
#include <shell/e-shell-view.h>

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

struct _ECalShellSidebar {
	EShellSidebar parent;
	ECalShellSidebarPrivate *priv;
};

struct _ECalShellSidebarClass {
	EShellSidebarClass parent_class;
};

GType		e_cal_shell_sidebar_get_type	(void);
GtkWidget *	e_cal_shell_sidebar_new		(EShellView *shell_view);
GtkWidget *	e_cal_shell_sidebar_get_selector(ECalShellSidebar *cal_shell_sidebar);

G_END_DECLS

#endif /* E_CAL_SHELL_SIDEBAR_H */
