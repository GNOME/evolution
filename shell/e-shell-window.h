/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-window.h
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

#ifndef E_SHELL_WINDOW_H
#define E_SHELL_WINDOW_H

#include "e-shell-common.h"
#include "e-shell-view.h"

/* Standard GObject macros */
#define E_TYPE_SHELL_WINDOW \
	(e_shell_window_get_type ())
#define E_SHELL_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_WINDOW, EShellWindow))
#define E_SHELL_WINDOW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_WINDOW, EShellWindowClass))
#define E_IS_SHELL_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_WINDOW))
#define E_IS_SHELL_WINDOW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_SHELL_WINDOW))
#define E_SHELL_WINDOW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_WINDOW, EShellWindowClass))

G_BEGIN_DECLS

typedef struct _EShellWindow EShellWindow;
typedef struct _EShellWindowClass EShellWindowClass;
typedef struct _EShellWindowPrivate EShellWindowPrivate;

struct _EShellWindow {
	GtkWindow parent;
	EShellWindowPrivate *priv;
};

struct _EShellWindowClass {
	GtkWindowClass parent_class;
};

GType		e_shell_window_get_type		(void);
GtkWidget *	e_shell_window_new		(gboolean safe_mode);
GtkUIManager *	e_shell_window_get_ui_manager	(EShellWindow *window);
GtkAction *	e_shell_window_get_action	(EShellWindow *window,
						 const gchar *action_name);
GtkActionGroup *e_shell_window_get_action_group	(EShellWindow *window,
						 const gchar *group_name);
GtkWidget *	e_shell_window_get_managed_widget
						(EShellWindow *window,
						 const gchar *widget_path);
gboolean	e_shell_window_get_safe_mode	(EShellWindow *window);
void		e_shell_window_set_safe_mode	(EShellWindow *window,
						 gboolean safe_mode);

G_END_DECLS

#endif /* E_SHELL_WINDOW_H */
