/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-taskbar.h
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

#ifndef E_SHELL_TASKBAR_H
#define E_SHELL_TASKBAR_H

#include <e-shell-common.h>

/* Standard GObject macros */
#define E_TYPE_SHELL_TASKBAR \
	(e_shell_taskbar_get_type ())
#define E_SHELL_TASKBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_TASKBAR, EShellTaskbar))
#define E_SHELL_TASKBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_TASKBAR, EShellTaskbarClass))
#define E_IS_SHELL_TASKBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_TASKBAR))
#define E_IS_SHELL_TASKBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SHELL_TASKBAR))
#define E_SHELL_TASKBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_TASKBAR, EShellTaskbarClass))

G_BEGIN_DECLS

/* Avoid including <e-shell-view.h> */
struct _EShellView;

typedef struct _EShellTaskbar EShellTaskbar;
typedef struct _EShellTaskbarClass EShellTaskbarClass;
typedef struct _EShellTaskbarPrivate EShellTaskbarPrivate;

struct _EShellTaskbar {
	GtkHBox parent;
	EShellTaskbarPrivate *priv;
};

struct _EShellTaskbarClass {
	GtkHBoxClass parent_class;
};

GType		e_shell_taskbar_get_type	(void);
GtkWidget *	e_shell_taskbar_new		(struct _EShellView *shell_view);
struct _EShellView *
		e_shell_taskbar_get_shell_view	(EShellTaskbar *shell_taskbar);
const gchar *	e_shell_taskbar_get_message	(EShellTaskbar *shell_taskbar);
void		e_shell_taskbar_set_message	(EShellTaskbar *shell_taskbar,
						 const gchar *message);
void		e_shell_taskbar_unset_message	(EShellTaskbar *shell_taskbar);

G_END_DECLS

#endif /* E_SHELL_TASKBAR_H */
