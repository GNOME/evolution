/*
 * e-mail-shell-sidebar.h
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

#ifndef E_MAIL_SHELL_SIDEBAR_H
#define E_MAIL_SHELL_SIDEBAR_H

#include <shell/e-shell-sidebar.h>
#include <shell/e-shell-view.h>
#include <mail/em-folder-tree.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_SHELL_SIDEBAR \
	(e_mail_shell_sidebar_get_type ())
#define E_MAIL_SHELL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_SHELL_SIDEBAR, EMailShellSidebar))
#define E_MAIL_SHELL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_SHELL_SIDEBAR, EMailShellSidebarClass))
#define E_IS_MAIL_SHELL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_SHELL_SIDEBAR))
#define E_IS_MAIL_SHELL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_SHELL_SIDEBAR))
#define E_MAIL_SHELL_SIDEBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_SHELL_SIDEBAR, EMailShellSidebarClass))

G_BEGIN_DECLS

typedef struct _EMailShellSidebar EMailShellSidebar;
typedef struct _EMailShellSidebarClass EMailShellSidebarClass;
typedef struct _EMailShellSidebarPrivate EMailShellSidebarPrivate;

struct _EMailShellSidebar {
	EShellSidebar parent;
	EMailShellSidebarPrivate *priv;
};

struct _EMailShellSidebarClass {
	EShellSidebarClass parent_class;
};

GType		e_mail_shell_sidebar_get_type	(void);
void		e_mail_shell_sidebar_type_register
					(GTypeModule *type_module);
GtkWidget *	e_mail_shell_sidebar_new
					(EShellView *shell_view);
EMFolderTree *	e_mail_shell_sidebar_get_folder_tree
					(EMailShellSidebar *mail_shell_sidebar);

G_END_DECLS

#endif /* E_MAIL_SHELL_SIDEBAR_H */
