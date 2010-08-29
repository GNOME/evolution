/*
 * e-memo-shell-sidebar.h
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

#ifndef E_MEMO_SHELL_SIDEBAR_H
#define E_MEMO_SHELL_SIDEBAR_H

#include <libecal/e-cal.h>
#include <libedataserverui/e-source-selector.h>

#include <shell/e-shell-sidebar.h>
#include <shell/e-shell-view.h>

/* Standard GObject macros */
#define E_TYPE_MEMO_SHELL_SIDEBAR \
	(e_memo_shell_sidebar_get_type ())
#define E_MEMO_SHELL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MEMO_SHELL_SIDEBAR, EMemoShellSidebar))
#define E_MEMO_SHELL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MEMO_SHELL_SIDEBAR, EMemoShellSidebarClass))
#define E_IS_MEMO_SHELL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MEMO_SHELL_SIDEBAR))
#define E_IS_MEMO_SHELL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MEMO_SHELL_SIDEBAR))
#define E_MEMO_SHELL_SIDEBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MEMO_SHELL_SIDEBAR, EMemoShellSidebarClass))

G_BEGIN_DECLS

typedef struct _EMemoShellSidebar EMemoShellSidebar;
typedef struct _EMemoShellSidebarClass EMemoShellSidebarClass;
typedef struct _EMemoShellSidebarPrivate EMemoShellSidebarPrivate;

enum {
	E_MEMO_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE		= 1 << 0,
	E_MEMO_SHELL_SIDEBAR_CAN_DELETE_PRIMARY_SOURCE	= 1 << 1,
	E_MEMO_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_SYSTEM	= 1 << 2,
	E_MEMO_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH	= 1 << 3
};

struct _EMemoShellSidebar {
	EShellSidebar parent;
	EMemoShellSidebarPrivate *priv;
};

struct _EMemoShellSidebarClass {
	EShellSidebarClass parent_class;

	/* Signals */
	void	(*client_added)			(EMemoShellSidebar *memo_shell_sidebar,
						 ECal *client);
	void	(*client_removed)		(EMemoShellSidebar *memo_shell_sidebar,
						 ECal *client);
	void	(*status_message)		(EMemoShellSidebar *memo_shell_sidebar,
						 const gchar *status_message,
						 gdouble percent);
};

GType		e_memo_shell_sidebar_get_type	(void);
void		e_memo_shell_sidebar_register_type
					(GTypeModule *type_module);
GtkWidget *	e_memo_shell_sidebar_new
					(EShellView *shell_view);
GList *		e_memo_shell_sidebar_get_clients
					(EMemoShellSidebar *memo_shell_sidebar);
ECal *		e_memo_shell_sidebar_get_default_client
					(EMemoShellSidebar *memo_shell_sidebar);
ESourceSelector *
		e_memo_shell_sidebar_get_selector
					(EMemoShellSidebar *memo_shell_sidebar);
void		e_memo_shell_sidebar_add_source
					(EMemoShellSidebar *memo_shell_sidebar,
					 ESource *source);
void		e_memo_shell_sidebar_remove_source
					(EMemoShellSidebar *memo_shell_sidebar,
					 ESource *source);

G_END_DECLS

#endif /* E_MEMO_SHELL_SIDEBAR_H */
