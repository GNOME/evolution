/*
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifndef E_ACCOUNT_UTILS_H
#define E_ACCOUNT_UTILS_H

#include <glib.h>
#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>

G_BEGIN_DECLS

EAccountList *	e_get_account_list		(void);
EAccount *	e_get_default_account		(void);
void		e_set_default_account		(EAccount *account);
EAccount *	e_get_account_by_name		(const gchar *name);
EAccount *	e_get_account_by_uid		(const gchar *uid);
EAccount *	e_get_any_enabled_account	(void);

G_END_DECLS

#endif /* E_ACCOUNT_UTILS_H */
