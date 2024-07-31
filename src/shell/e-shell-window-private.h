/*
 * e-shell-window-private.h
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

#ifndef E_SHELL_WINDOW_PRIVATE_H
#define E_SHELL_WINDOW_PRIVATE_H

#include "e-shell-window.h"

#include <string.h>
#include <glib/gi18n.h>

#include <libebackend/libebackend.h>

#include "e-util/e-util.h"

#include "shell/e-shell.h"
#include "shell/e-shell-content.h"
#include "shell/e-shell-view.h"
#include "shell/e-shell-searchbar.h"
#include "shell/e-shell-switcher.h"
#include "shell/e-shell-window-actions.h"
#include "shell/e-shell-utils.h"

/* Shorthand, requires a variable named "shell_window". */
#define ACTION(name) \
	(E_SHELL_WINDOW_ACTION_##name (shell_window))
#define ACTION_GROUP(name) \
	(E_SHELL_WINDOW_ACTION_GROUP_##name (shell_window))

#define E_SHELL_SWITCHER_FORMAT "switch-to-%s"

G_BEGIN_DECLS

struct _EShellWindowPrivate {

	gpointer shell;  /* weak pointer */

	/*** UI Management ***/

	EFocusTracker *focus_tracker;
	GHashTable *action_groups; /* gchar *name ~> EUIActionGroup * */

	/*** Shell Views ***/

	GHashTable *loaded_views;
	gchar active_view[32];
	GMenu *switch_to_menu;

	/*** Widgetry ***/

	GtkBox *headerbar_box;
	GtkWidget *alert_bar;
	GtkNotebook *views_notebook;

	/* Shell signal handlers. */
	GArray *signal_handler_ids;

	gchar *geometry;

	guint safe_mode : 1;
	guint is_main_instance : 1;

	GSList *postponed_alerts; /* EAlert * */
};

void		e_shell_window_private_init	(EShellWindow *shell_window);
void		e_shell_window_private_constructed
						(EShellWindow *shell_window);
void		e_shell_window_private_dispose	(EShellWindow *shell_window);
void		e_shell_window_private_finalize	(EShellWindow *shell_window);

/* Private Utilities */

void		e_shell_window_actions_constructed
						(EShellWindow *shell_window);
void		e_shell_window_init_ui_data	(EShellWindow *shell_window,
						 EShellView *shell_view);
void		e_shell_window_fill_switcher_actions
						(EShellWindow *shell_window,
						 EUIManager *ui_manager,
						 EShellSwitcher *switcher);
void		e_shell_window_switch_to_view	(EShellWindow *shell_window,
						 const gchar *view_name);
void		e_shell_window_update_icon	(EShellWindow *shell_window);
void		e_shell_window_update_title	(EShellWindow *shell_window);
GMenuModel *	e_shell_window_ref_switch_to_menu_model
						(EShellWindow *self);

G_END_DECLS

#endif /* E_SHELL_WINDOW_PRIVATE_H */
