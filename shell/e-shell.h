/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell.h
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

#ifndef E_SHELL_H
#define E_SHELL_H

#include "e-shell-common.h"
#include "e-shell-window.h"

G_BEGIN_DECLS

typedef enum _EShellLineStatus EShellLineStatus;
typedef enum _EShellStartupLineMode EShellStartupLineMode;

enum _EShellLineStatus {
	E_SHELL_LINE_STATUS_ONLINE,
	E_SHELL_LINE_STATUS_GOING_OFFLINE, /* NB: really means changing state in either direction */
	E_SHELL_LINE_STATUS_OFFLINE,
	E_SHELL_LINE_STATUS_FORCED_OFFLINE
};

enum _EShellStartupLineMode {
	E_SHELL_STARTUP_LINE_MODE_CONFIG,
	E_SHELL_STARTUP_LINE_MODE_ONLINE,
	E_SHELL_STARTUP_LINE_MODE_OFFLINE
};

EShellWindow *	e_shell_create_window		(void);
void		e_shell_send_receive		(GtkWindow *parent);
void		e_shell_show_preferences	(GtkWindow *parent);
void		e_shell_go_offline		(void);
void		e_shell_go_online		(void);
EShellLineStatus
		e_shell_get_line_status		(void);
gboolean	e_shell_is_busy			(void);
gboolean	e_shell_do_quit			(void);
gboolean	e_shell_quit			(void);

G_END_DECLS

#endif /* E_SHELL_H */
