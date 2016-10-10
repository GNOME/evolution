/*
 * e-book-shell-backend-migrate.c
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
 * Authors:
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-book-shell-migrate.h"

gboolean
e_book_shell_backend_migrate (EShellBackend *shell_backend,
                              gint major,
                              gint minor,
                              gint micro,
                              GError **error)
{
	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), FALSE);

	return TRUE;
}
