/*
 * e-mail-pane.h
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

#ifndef E_MAIL_MESSAGE_PANE_H
#define E_MAIL_MESSAGE_PANE_H

#include <gtk/gtk.h>
#include "e-mail-pane.h"
#include <misc/e-focus-tracker.h>
#include <shell/e-shell-backend.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_MESSAGE_PANE \
	(e_mail_message_pane_get_type ())
#define E_MAIL_MESSAGE_PANE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_MESSAGE_PANE, EMailMessagePane))
#define E_MAIL_MESSAGE_PANE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_MESSAGE_PANE, EMailMessagePaneClass))
#define E_IS_MAIL_MESSAGE_PANE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_MESSAGE_PANE))
#define E_IS_MAIL_MESSAGE_PANE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_MESSAGE_PANE))
#define E_MAIL_MESSAGE_PANE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_MESSAGE_PANE, EMailMessagePaneClass))

G_BEGIN_DECLS

typedef struct _EMailMessagePane EMailMessagePane;
typedef struct _EMailMessagePaneClass EMailMessagePaneClass;
typedef struct _EMailMessagePanePrivate EMailMessagePanePrivate;

struct _EMailMessagePane {
	EMailPane parent;
	EMailMessagePanePrivate *priv;
};

struct _EMailMessagePaneClass {
	EMailPaneClass parent_class;
};

GType		e_mail_message_pane_get_type		(void);
GtkWidget *	e_mail_message_pane_new		(EShellBackend *shell_backend);
void		e_mail_message_pane_close		(EMailMessagePane *browser);
gboolean	e_mail_message_pane_get_show_deleted	(EMailMessagePane *browser);
void		e_mail_message_pane_set_show_deleted (EMailMessagePane *browser,
						 gboolean show_deleted);
EFocusTracker *	e_mail_message_pane_get_focus_tracker(EMailMessagePane *browser);
GtkUIManager *	e_mail_message_pane_get_ui_manager	(EMailMessagePane *browser);

G_END_DECLS

#endif /* E_MAIL_MESSAGE_PANE_H */
