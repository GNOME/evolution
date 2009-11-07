/*
 * e-shell-utils.h
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

/**
 * SECTION: e-shell-utils
 * @short_description: high-level utilities with shell integration
 * @include: shell/e-shell-utils.h
 **/

#ifndef E_SHELL_UTILS_H
#define E_SHELL_UTILS_H

#include <shell/e-shell.h>

G_BEGIN_DECLS

GFile *		e_shell_run_save_dialog		(EShell *shell,
						 const gchar *title,
						 GtkCallback customize_func,
						 gpointer customize_data);

G_END_DECLS

#endif /* E_SHELL_UTILS_H */
