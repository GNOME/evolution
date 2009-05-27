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

#include "e-account-utils.h"

#include <gconf/gconf-client.h>

static EAccountList *global_account_list;

EAccountList *
e_get_account_list (void)
{
	if (G_UNLIKELY (global_account_list == NULL)) {
		GConfClient *client;

		client = gconf_client_get_default ();
		global_account_list = e_account_list_new (client);
		g_object_unref (client);
	}

	g_return_val_if_fail (global_account_list != NULL, NULL);

	return global_account_list;
}

EAccount *
e_get_default_account (void)
{
	EAccountList *account_list;
	const EAccount *account;

	account_list = e_get_account_list ();
	account = e_account_list_get_default (account_list);

	/* XXX EAccountList misuses const. */
	return (EAccount *) account;
}

void
e_set_default_account (EAccount *account)
{
	EAccountList *account_list;

	g_return_if_fail (E_IS_ACCOUNT (account));

	account_list = e_get_account_list ();
	e_account_list_set_default (account_list, account);
}

EAccount *
e_get_account_by_name (const gchar *name)
{
	EAccountList *account_list;
	const EAccount *account;
	e_account_find_t find;

	g_return_val_if_fail (name != NULL, NULL);

	find = E_ACCOUNT_FIND_NAME;
	account_list = e_get_account_list ();
	account = e_account_list_find (account_list, find, name);

	/* XXX EAccountList misuses const. */
	return (EAccount *) account;
}

EAccount *
e_get_account_by_uid (const gchar *uid)
{
	EAccountList *account_list;
	const EAccount *account;
	e_account_find_t find;

	g_return_val_if_fail (uid != NULL, NULL);

	find = E_ACCOUNT_FIND_UID;
	account_list = e_get_account_list ();
	account = e_account_list_find (account_list, find, uid);

	/* XXX EAccountList misuses const. */
	return (EAccount *) account;
}
