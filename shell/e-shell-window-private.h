/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 * e-shell-window-private.h
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

#ifndef E_SHELL_WINDOW_PRIVATE_H
#define E_SHELL_WINDOW_PRIVATE_H

#include "e-shell-window.h"

#include <glib/gi18n.h>

#include "e-shell.h"
#include "e-shell-view.h"
#include "e-shell-registry.h"
#include "e-shell-window-actions.h"

#include "e-online-button.h"
#include "e-sidebar.h"

#define E_SHELL_WINDOW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_WINDOW, EShellWindowPrivate))

/* Shorthand, requires a variable named "window". */
#define ACTION(name)		(E_SHELL_WINDOW_ACTION_##name (window))
#define ACTION_GROUP(name)	(E_SHELL_WINDOW_ACTION_GROUP_##name (window))

/* For use in dispose() methods. */
#define DISPOSE(obj) \
	G_STMT_START { \
	if ((obj) != NULL) { g_object_unref (obj); (obj) = NULL; } \
	} G_STMT_END

G_BEGIN_DECLS

struct _EShellWindowPrivate {

	/*** UI Management ***/

	GtkUIManager *manager;
	GtkActionGroup *shell_actions;
	GtkActionGroup *new_item_actions;
	GtkActionGroup *new_source_actions;
	GtkActionGroup *shell_view_actions;

	/*** Shell Views ***/

	GHashTable *loaded_views;
	const gchar *current_view;
	const gchar *default_view;

	/*** Widgetry ***/

	GtkWidget *main_menu;
	GtkWidget *main_toolbar;
	GtkWidget *content_pane;
	GtkWidget *content_notebook;
	GtkWidget *sidebar;
	GtkWidget *sidebar_notebook;
	GtkWidget *status_area;
	GtkWidget *online_button;
	GtkWidget *tooltip_label;
	GtkWidget *status_notebook;

	/* Miscellaneous */

	gboolean destroyed;  /* XXX Do we still need this? */
	gboolean safe_mode;
};

void		e_shell_window_private_init	(EShellWindow *window);
void		e_shell_window_private_dispose	(EShellWindow *window);
void		e_shell_window_private_finalize	(EShellWindow *window);

/* Private Utilities */

void		e_shell_window_actions_init	(EShellWindow *window);
GtkWidget *	e_shell_window_create_new_menu	(EShellWindow *window);
void		e_shell_window_create_shell_view_actions
						(EShellWindow *window);

G_END_DECLS

#endif /* E_SHELL_WINDOW_PRIVATE_H */
