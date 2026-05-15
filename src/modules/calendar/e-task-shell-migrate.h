/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_TASK_SHELL_BACKEND_MIGRATE_H
#define E_TASK_SHELL_BACKEND_MIGRATE_H

#include <shell/e-shell-backend.h>

G_BEGIN_DECLS

gboolean	e_task_shell_backend_migrate	(EShellBackend *shell_backend,
						 gint major,
						 gint minor,
						 gint micro,
						 GError **error);

G_END_DECLS

#endif /* E_TASK_SHELL_BACKEND_MIGRATE_H */
