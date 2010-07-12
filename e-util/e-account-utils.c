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

/**
 * SECTION: e-account-utils
 * @include: e-util/e-account-utils.h
 **/

#include "e-account-utils.h"

#include <gconf/gconf-client.h>

static EAccountList *global_account_list;

/**
 * e_get_account_list:
 *
 * Returns the global #EAccountList.
 *
 * Returns: the global #EAccountList
 **/
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

/**
 * e_get_default_account:
 *
 * Returns the #EAccount marked as the default mail account.
 *
 * Returns: the default #EAccount
 **/
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

/**
 * e_set_default_account:
 * @account: an #EAccount
 *
 * Marks @account as the default mail account.
 **/
void
e_set_default_account (EAccount *account)
{
	EAccountList *account_list;

	g_return_if_fail (E_IS_ACCOUNT (account));

	account_list = e_get_account_list ();
	e_account_list_set_default (account_list, account);
}

/**
 * e_get_account_by_name:
 * @name: a mail account name
 *
 * Returns the #EAccount with the given name, or %NULL if no such
 * account exists.
 *
 * Returns: an #EAccount having the given account name, or %NULL
 **/
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

/**
 * e_get_account_by_uid:
 * @uid: a mail account UID
 *
 * Returns the #EAccount with the given unique ID, or %NULL if no such
 * account exists.
 *
 * Returns: an #EAccount having the given unique ID, or %NULL
 **/
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

/**
 * e_get_any_enabled_account:
 *
 * Returns the default mail account if it's enabled, otherwise the first
 * enabled mail account in the global #EAccountList, or finally %NULL if
 * all mail accounts are disabled or none exist.
 *
 * Returns: an enabled #EAccount, or %NULL if there are none
 **/
EAccount *
e_get_any_enabled_account (void)
{
	EAccount *account;
	EAccountList *account_list;
	EIterator *iter;

	account = e_get_default_account ();
	if (account != NULL && account->enabled)
		return account;

	account = NULL;

	account_list = e_get_account_list ();
	iter = e_list_get_iterator (E_LIST (account_list));

	while (e_iterator_is_valid (iter) && account == NULL) {
		EAccount *candidate;

		/* XXX EIterator misuses const. */
		candidate = (EAccount *) e_iterator_get (iter);

		if (candidate->enabled)
			account = candidate;
		else
			e_iterator_next (iter);
	}

	g_object_unref (iter);

	return account;
}
