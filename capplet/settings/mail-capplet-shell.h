/*
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
 * Authors:
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *		Srinivasa Ragavan <srini@linux.intel.com>
 *
 * Copyright (C) 2010 Intel Corporation. (www.intel.com)
 *
 */

#ifndef _MAIL_CAPPLET_SHELL_H_
#define _MAIL_CAPPLET_SHELL_H_

#include <gtk/gtk.h>
#include "mail-view.h"

#define MAIL_CAPPLET_SHELL_TYPE        (mail_capplet_shell_get_type ())
#define MAIL_CAPPLET_SHELL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), MAIL_CAPPLET_SHELL_TYPE, MailCappletShell))
#define MAIL_CAPPLET_SHELL_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), MAIL_CAPPLET_SHELL_TYPE, MailCappletShellClass))
#define IS_MAIL_CAPPLET_SHELL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAIL_CAPPLET_SHELL_TYPE))
#define IS_MAIL_CAPPLET_SHELL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), MAIL_CAPPLET_SHELL_TYPE))
#define MAIL_CAPPLET_SHELL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), MAIL_CAPPLET_SHELL_TYPE, MailCappletShellClass))

typedef struct _MailCappletShellPrivate MailCappletShellPrivate;

typedef struct _MailCappletShell {
	GtkWindow parent;
	MailView *view;

	MailCappletShellPrivate *priv;
} MailCappletShell;

typedef struct _MailCappletShellClass {
	GtkWindowClass parent_class;

	void (* ctrl_w_pressed)    (MailCappletShell *class);
	void (* ctrl_q_pressed)    (MailCappletShell *class);
} MailCappletShellClass;

GType		mail_capplet_shell_get_type	(void);
GtkWidget *	mail_capplet_shell_new		(gint socket_id,
						 gboolean just_druid,
						 gboolean main_loop);
gint		mail_capplet_shell_toolbar_height
						(MailCappletShell *shell);

#endif

