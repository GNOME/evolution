/*
 * e-mail-shell-content.h
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

#ifndef E_MAIL_SHELL_CONTENT_H
#define E_MAIL_SHELL_CONTENT_H

#include <shell/e-shell-content.h>
#include <shell/e-shell-view.h>

#include "em-format-html-display.h"

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

enum {
	E_MAIL_SHELL_CONTENT_SELECTION_SINGLE		= 1 << 0,
	E_MAIL_SHELL_CONTENT_SELECTION_MULTIPLE		= 1 << 1,
	E_MAIL_SHELL_CONTENT_SELECTION_CAN_ADD_SENDER	= 1 << 2,
	E_MAIL_SHELL_CONTENT_SELECTION_CAN_EDIT		= 1 << 3,
	E_MAIL_SHELL_CONTENT_SELECTION_FLAG_CLEAR	= 1 << 4,
	E_MAIL_SHELL_CONTENT_SELECTION_FLAG_COMPLETED	= 1 << 5,
	E_MAIL_SHELL_CONTENT_SELECTION_FLAG_FOLLOWUP	= 1 << 6,
	E_MAIL_SHELL_CONTENT_SELECTION_HAS_DELETED	= 1 << 7,
	E_MAIL_SHELL_CONTENT_SELECTION_HAS_IMPORTANT	= 1 << 8,
	E_MAIL_SHELL_CONTENT_SELECTION_HAS_JUNK		= 1 << 9,
	E_MAIL_SHELL_CONTENT_SELECTION_HAS_NOT_JUNK	= 1 << 10,
	E_MAIL_SHELL_CONTENT_SELECTION_HAS_READ		= 1 << 11,
	E_MAIL_SHELL_CONTENT_SELECTION_HAS_UNDELETED	= 1 << 12,
	E_MAIL_SHELL_CONTENT_SELECTION_HAS_UNIMPORTANT	= 1 << 13,
	E_MAIL_SHELL_CONTENT_SELECTION_HAS_UNREAD	= 1 << 14,
	E_MAIL_SHELL_CONTENT_SELECTION_HAS_URI_CALLTO	= 1 << 15,
	E_MAIL_SHELL_CONTENT_SELECTION_HAS_URI_HTTP	= 1 << 16,
	E_MAIL_SHELL_CONTENT_SELECTION_HAS_URI_MAILTO	= 1 << 17,
	E_MAIL_SHELL_CONTENT_SELECTION_IS_MAILING_LIST	= 1 << 18
};

struct _EMailShellContent {
	EShellContent parent;
	EMailShellContentPrivate *priv;
};

struct _EMailShellContentClass {
	EShellContentClass parent_class;
};

GType		e_mail_shell_content_get_type	(void);
GtkWidget *	e_mail_shell_content_new	(EShellView *shell_view);
gboolean	e_mail_shell_content_get_preview_visible
						(EMailShellContent *mail_shell_content);
void		e_mail_shell_content_set_preview_visible
						(EMailShellContent *mail_shell_content,
						 gboolean preview_visible);
gboolean	e_mail_shell_content_get_vertical_view
						(EMailShellContent *mail_shell_content);
void		e_mail_shell_content_set_vertical_view
						(EMailShellContent *mail_shell_content,
						 gboolean vertical_view);
void		e_mail_shell_content_update_view_instance
						(EMailShellContent *mail_shell_content);

G_END_DECLS

#endif /* E_MAIL_SHELL_CONTENT_H */
