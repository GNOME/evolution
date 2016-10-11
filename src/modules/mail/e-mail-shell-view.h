/*
 * e-mail-shell-view.h
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

#ifndef E_MAIL_SHELL_VIEW_H
#define E_MAIL_SHELL_VIEW_H

#include <shell/e-shell-view.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_SHELL_VIEW \
	(e_mail_shell_view_get_type ())
#define E_MAIL_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_SHELL_VIEW, EMailShellView))
#define E_MAIL_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_SHELL_VIEW, EMailShellViewClass))
#define E_IS_MAIL_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_SHELL_VIEW))
#define E_IS_MAIL_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_SHELL_VIEW))
#define E_MAIL_SHELL_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_SHELL_VIEW, EMailShellViewClass))

G_BEGIN_DECLS

typedef struct _EMailShellView EMailShellView;
typedef struct _EMailShellViewClass EMailShellViewClass;
typedef struct _EMailShellViewPrivate EMailShellViewPrivate;

struct _EMailShellView {
	EShellView parent;
	EMailShellViewPrivate *priv;
};

struct _EMailShellViewClass {
	EShellViewClass parent_class;
};

GType		e_mail_shell_view_get_type	(void);
void		e_mail_shell_view_type_register	(GTypeModule *type_module);

G_END_DECLS

#endif /* E_MAIL_SHELL_VIEW_H */
