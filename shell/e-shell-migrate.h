/*
 * e-shell-migrate.h
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

/* This is an EShell extension that handles migrating from older versions. */

#ifndef E_SHELL_MIGRATE_H
#define E_SHELL_MIGRATE_H

#include <shell/e-shell-common.h>
#include <shell/e-shell.h>

/**
 * E_SHELL_MIGRATE_ERROR:
 *
 * Error domain for migration operations.  Errors in this domain will be
 * from the #EShellMigrateError enumeration.  See #GError for information
 * on error domains.
 **/
#define E_SHELL_MIGRATE_ERROR \
	(e_shell_migrate_error_quark ())

G_BEGIN_DECLS

/* XXX Need more specific error codes? */
typedef enum {
	E_SHELL_MIGRATE_ERROR_FAILED
} EShellMigrateError;

gboolean	e_shell_migrate_attempt			(EShell *shell);
GQuark		e_shell_migrate_error_quark		(void);

G_END_DECLS

#endif /* E_SHELL_MIGRATE_H */
