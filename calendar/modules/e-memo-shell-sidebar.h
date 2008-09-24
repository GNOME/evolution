/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-memo-shell-sidebar.h
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

#ifndef E_MEMO_SHELL_SIDEBAR_H
#define E_MEMO_SHELL_SIDEBAR_H

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

struct _EMemoShellSidebar {
	EShellSidebar parent;
	EMemoShellSidebarPrivate *priv;
};

struct _EMemoShellSidebarClass {
	EShellSidebarClass parent_class;
};

GType		e_memo_shell_sidebar_get_type	(void);
GtkWidget *	e_memo_shell_sidebar_new	(EShellView *shell_view);
ESourceSelector *
		e_memo_shell_sidebar_get_selector
						(EMemoShellSidebar *memo_shell_sidebar);

G_END_DECLS

#endif /* E_MEMO_SHELL_SIDEBAR_H */
