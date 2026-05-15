/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include "e-cal-shell-migrate.h"

gboolean
e_cal_shell_backend_migrate (EShellBackend *shell_backend,
                             gint major,
                             gint minor,
                             gint micro,
                             GError **error)
{
	return TRUE;
}

