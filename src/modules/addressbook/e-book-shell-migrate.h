/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Toshok (toshok@ximian.com)
 */

#ifndef E_BOOK_SHELL_MIGRATE_H
#define E_BOOK_SHELL_MIGRATE_H

#include <shell/e-shell-backend.h>

G_BEGIN_DECLS

gboolean	e_book_shell_backend_migrate	(EShellBackend *shell_backend,
						 gint major,
						 gint minor,
						 gint micro,
						 GError **error);

G_END_DECLS

#endif /* E_BOOK_SHELL_MIGRATE_H */
