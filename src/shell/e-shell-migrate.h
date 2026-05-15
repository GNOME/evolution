/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/* This is an EShell extension that handles migrating from older versions. */

#ifndef E_SHELL_MIGRATE_H
#define E_SHELL_MIGRATE_H

#include <shell/e-shell-common.h>
#include <shell/e-shell.h>

G_BEGIN_DECLS

gboolean	e_shell_migrate_attempt			(EShell *shell);
void		e_shell_maybe_migrate_mail_folders_db	(EShell *shell);
GQuark		e_shell_migrate_error_quark		(void);

G_END_DECLS

#endif /* E_SHELL_MIGRATE_H */
