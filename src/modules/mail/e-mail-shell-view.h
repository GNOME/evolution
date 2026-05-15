/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
