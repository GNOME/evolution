/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef EAB_COMPOSER_UTIL_H
#define EAB_COMPOSER_UTIL_H

#include <shell/e-shell.h>

G_BEGIN_DECLS

void		eab_send_as_to			(EShell *shell,
						 GSList *destinations);
void		eab_send_as_attachment		(EShell *shell,
						 GSList *destinations);

G_END_DECLS

#endif /* EAB_COMPOSER_UTIL_H */
