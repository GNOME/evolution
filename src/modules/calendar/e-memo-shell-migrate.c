/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
