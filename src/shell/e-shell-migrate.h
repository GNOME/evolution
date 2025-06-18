/*
 * e-shell-migrate.h
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
