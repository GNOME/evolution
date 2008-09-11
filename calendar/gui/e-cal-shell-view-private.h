/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-cal-shell-view-private.h
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

#ifndef E_CAL_SHELL_VIEW_PRIVATE_H
#define E_CAL_SHELL_VIEW_PRIVATE_H

#include "e-cal-shell-view.h"

#include <glib/gi18n.h>

#include <e-util/e-util.h>
#include <shell/e-shell-content.h>
#include <shell/e-shell-sidebar.h>

#include <gnome-cal.h>
#include <e-cal-shell-view-actions.h>

#define E_CAL_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_SHELL_VIEW, ECalShellViewPrivate))

/* Shorthand, requires a variable named "shell_window". */
#define ACTION(name) \
	(E_SHELL_WINDOW_ACTION_##name (shell_window))
#define ACTION_GROUP(name) \
	(E_SHELL_WINDOW_ACTION_GROUP_##name (shell_window))

/* For use in dispose() methods. */
#define DISPOSE(obj) \
	G_STMT_START { \
	if ((obj) != NULL) { g_object_unref (obj); (obj) = NULL; } \
	} G_STMT_END

G_BEGIN_DECLS

struct _ECalShellViewPrivate {

	/*** UI Management ***/

	GtkActionGroup *calendar_actions;
};

void		e_cal_shell_view_private_init
					(ECalShellView *cal_shell_view);
void		e_cal_shell_view_private_constructed
					(ECalShellView *cal_shell_view);
void		e_cal_shell_view_private_dispose
					(ECalShellView *cal_shell_view);
void		e_cal_shell_view_private_finalize
					(ECalShellView *cal_shell_view);

/* Private Utilities */

void		e_cal_shell_view_actions_init
					(ECalShellView *cal_shell_view);

G_END_DECLS

#endif /* E_CAL_SHELL_VIEW_PRIVATE_H */
