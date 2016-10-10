/*
 * e-memo-shell-migrate.c
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

#include "evolution-config.h"

#include "e-memo-shell-migrate.h"

gboolean
e_memo_shell_backend_migrate (EShellBackend *shell_backend,
                              gint major,
                              gint minor,
                              gint revision,
                              GError **error)
{
	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), FALSE);

	return TRUE;
}
