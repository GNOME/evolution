/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Toshok <toshok@ximian.com>
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
