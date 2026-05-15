/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_SHELL_CONTENT_H
#define E_MAIL_SHELL_CONTENT_H

#include <shell/e-shell-content.h>
#include <shell/e-shell-searchbar.h>
#include <shell/e-shell-view.h>
#include <mail/e-mail-view.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_SHELL_CONTENT \
	(e_mail_shell_content_get_type ())
#define E_MAIL_SHELL_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_SHELL_CONTENT, EMailShellContent))
#define E_MAIL_SHELL_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_SHELL_CONTENT, EMailShellContentClass))
#define E_IS_MAIL_SHELL_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_SHELL_CONTENT))
#define E_IS_MAIL_SHELL_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_SHELL_CONTENT))
#define E_MAIL_SHELL_CONTENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_SHELL_CONTENT, EMailShellContentClass))

G_BEGIN_DECLS

typedef struct _EMailShellContent EMailShellContent;
typedef struct _EMailShellContentClass EMailShellContentClass;
typedef struct _EMailShellContentPrivate EMailShellContentPrivate;

struct _EMailShellContent {
	EShellContent parent;
	EMailShellContentPrivate *priv;
};

struct _EMailShellContentClass {
	EShellContentClass parent_class;
};

GType		e_mail_shell_content_get_type	(void);
void		e_mail_shell_content_type_register
					(GTypeModule *type_module);
GtkWidget *	e_mail_shell_content_new
					(EShellView *shell_view);
EMailView *	e_mail_shell_content_get_mail_view
					(EMailShellContent *mail_shell_content);
EShellSearchbar *
		e_mail_shell_content_get_searchbar
					(EMailShellContent *mail_shell_content);
GtkWidget *	e_mail_shell_content_get_to_do_pane
					(EMailShellContent *mail_shell_content);

G_END_DECLS

#endif /* E_MAIL_SHELL_CONTENT_H */
