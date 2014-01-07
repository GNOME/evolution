/*
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
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
