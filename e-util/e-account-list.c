/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-account-list.h"
#include "e-account.h"
#include "e-util-marshal.h"

#include <string.h>

struct EAccountListPrivate {
	GConfClient *gconf;
	guint notify_id;
};

enum {
	ACCOUNT_ADDED,
	ACCOUNT_CHANGED,
	ACCOUNT_REMOVED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

static void e_account_list_dispose (GObject *);
static void e_account_list_finalize (GObject *);

G_DEFINE_TYPE (EAccountList, e_account_list, E_TYPE_LIST)

static void
e_account_list_class_init (EAccountListClass *klass)
{
	GObjectClass *object_class;
	
	/* virtual method override */
	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = e_account_list_dispose;
	object_class->finalize = e_account_list_finalize;

	/* signals */
	signals[ACCOUNT_ADDED] =
		g_signal_new ("account-added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAccountListClass, account_added),
			      NULL, NULL,
			      e_util_marshal_NONE__OBJECT,
			      G_TYPE_NONE, 1,
			      E_TYPE_ACCOUNT);
	signals[ACCOUNT_CHANGED] =
		g_signal_new ("account-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAccountListClass, account_changed),
			      NULL, NULL,
			      e_util_marshal_NONE__OBJECT,
			      G_TYPE_NONE, 1,
			      E_TYPE_ACCOUNT);
	signals[ACCOUNT_REMOVED] =
		g_signal_new ("account-removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAccountListClass, account_removed),
			      NULL, NULL,
			      e_util_marshal_NONE__OBJECT,
			      G_TYPE_NONE, 1,
			      E_TYPE_ACCOUNT);
}

static void
e_account_list_init (EAccountList *account_list)
{
	account_list->priv = g_new0 (EAccountListPrivate, 1);
}

static void
e_account_list_dispose (GObject *object)
{
	EAccountList *account_list = E_ACCOUNT_LIST (object);

	if (account_list->priv->gconf) {
		if (account_list->priv->notify_id) {
			gconf_client_notify_remove (account_list->priv->gconf,
						    account_list->priv->notify_id);
		}
		g_object_unref (account_list->priv->gconf);
		account_list->priv->gconf = NULL;
	}

	G_OBJECT_CLASS (e_account_list_parent_class)->dispose (object);
}

static void
e_account_list_finalize (GObject *object)
{
	EAccountList *account_list = E_ACCOUNT_LIST (object);

	g_free (account_list->priv);

	G_OBJECT_CLASS (e_account_list_parent_class)->finalize (object);
}

static void
gconf_accounts_changed (GConfClient *client, guint cnxn_id,
			GConfEntry *entry, gpointer user_data)
{
	EAccountList *account_list = user_data;
	GSList *list, *l, *new_accounts = NULL;
	EAccount *account;
	EList *old_accounts;
	EIterator *iter;
	char *uid;

	old_accounts = e_list_duplicate (E_LIST (account_list));

	list = gconf_client_get_list (client, "/apps/evolution/mail/accounts",
				      GCONF_VALUE_STRING, NULL);
	for (l = list; l; l = l->next) {
		uid = e_account_uid_from_xml (l->data);
		if (!uid)
			continue;

		/* See if this is an existing account */
		for (iter = e_list_get_iterator (old_accounts);
		     e_iterator_is_valid (iter);
		     e_iterator_next (iter)) {
			account = (EAccount *)e_iterator_get (iter);
			if (!strcmp (account->uid, uid)) {
				/* The account still exists, so remove
				 * it from "old_accounts" and update it.
				 */
				e_iterator_delete (iter);
				if (e_account_set_from_xml (account, l->data))
					g_signal_emit (account_list, signals[ACCOUNT_CHANGED], 0, account);
				goto next;
			}
		}

		/* Must be a new account */
		account = e_account_new_from_xml (l->data);
		e_list_append (E_LIST (account_list), account);
		new_accounts = g_slist_prepend (new_accounts, account);

	next:
		g_free (uid);
		g_object_unref (iter);
	}

	/* Now emit signals for each added account. (We do this after
	 * adding all of them because otherwise if the signal handler
	 * calls e_account_list_get_default_account() it will end up
	 * causing the first account in the list to become the
	 * default.)
	 */
	for (l = new_accounts; l; l = l->next) {
		account = l->data;
		g_signal_emit (account_list, signals[ACCOUNT_ADDED], 0, account);
		g_object_unref (account);
	}
	g_slist_free (new_accounts);

	/* Anything left in old_accounts must have been deleted */
	for (iter = e_list_get_iterator (old_accounts);
	     e_iterator_is_valid (iter);
	     e_iterator_next (iter)) {
		account = (EAccount *)e_iterator_get (iter);
		e_list_remove (E_LIST (account_list), account);
		g_signal_emit (account_list, signals[ACCOUNT_REMOVED], 0, account);
	}
	g_object_unref (iter);
	g_object_unref (old_accounts);
}

static void *
copy_func (const void *data, void *closure)
{
	GObject *object = (GObject *)data;

	g_object_ref (object);
	return object;
}

static void
free_func (void *data, void *closure)
{
	g_object_unref (data);
}

/**
 * e_account_list_new:
 * @gconf: a #GConfClient
 *
 * Reads the list of accounts from @gconf and listens for changes.
 * Will emit %account_added, %account_changed, and %account_removed
 * signals according to notifications from GConf.
 *
 * You can modify the list using e_list_append(), e_list_remove(), and
 * e_iterator_delete(). After adding, removing, or changing accounts,
 * you must call e_account_list_save() to push the changes back to
 * GConf.
 *
 * Return value: the list of accounts
 **/
EAccountList *
e_account_list_new (GConfClient *gconf)
{
	EAccountList *account_list;

	g_return_val_if_fail (GCONF_IS_CLIENT (gconf), NULL);

	account_list = g_object_new (E_TYPE_ACCOUNT_LIST, NULL);
	e_account_list_construct (account_list, gconf);

	return account_list;
}

void
e_account_list_construct (EAccountList *account_list, GConfClient *gconf)
{
	g_return_if_fail (GCONF_IS_CLIENT (gconf));

	e_list_construct (E_LIST (account_list), copy_func, free_func, NULL);
	account_list->priv->gconf = gconf;
	g_object_ref (gconf);

	gconf_client_add_dir (account_list->priv->gconf,
			      "/apps/evolution/mail/accounts",
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	account_list->priv->notify_id =
		gconf_client_notify_add (account_list->priv->gconf,
					 "/apps/evolution/mail/accounts",
					 gconf_accounts_changed, account_list,
					 NULL, NULL);

	gconf_accounts_changed (account_list->priv->gconf,
				account_list->priv->notify_id,
				NULL, account_list);
}

/**
 * e_account_list_save:
 * @account_list: an #EAccountList
 *
 * Saves @account_list to GConf. Signals will be emitted for changes.
 **/
void
e_account_list_save (EAccountList *account_list)
{
	GSList *list = NULL;
	EAccount *account;
	EIterator *iter;
	char *xmlbuf;

	for (iter = e_list_get_iterator (E_LIST (account_list));
	     e_iterator_is_valid (iter);
	     e_iterator_next (iter)) {
		account = (EAccount *)e_iterator_get (iter);

		xmlbuf = e_account_to_xml (account);
		if (xmlbuf)
			list = g_slist_append (list, xmlbuf);
	}
	g_object_unref (iter);

	gconf_client_set_list (account_list->priv->gconf,
			       "/apps/evolution/mail/accounts",
			       GCONF_VALUE_STRING, list, NULL);

	while (list) {
		g_free (list->data);
		list = g_slist_remove (list, list->data);
	}

	gconf_client_suggest_sync (account_list->priv->gconf, NULL);
}

/**
 * e_account_list_add:
 * @accounts: 
 * @account: 
 * 
 * Add an account to the account list.  Will emit the account-changed
 * event.
 **/
void
e_account_list_add(EAccountList *accounts, EAccount *account)
{
	/* FIXME: should we check for duplicate accounts? */

	e_list_append ((EList *)accounts, account);
	g_signal_emit(accounts, signals[ACCOUNT_ADDED], 0, account);
}

/**
 * e_account_list_change:
 * @accounts: 
 * @account: 
 * 
 * Signal that the details of an account have changed.
 **/
void
e_account_list_change(EAccountList *accounts, EAccount *account)
{
	/* maybe the account should do this itself ... */
	g_signal_emit(accounts, signals[ACCOUNT_CHANGED], 0, account);
}

/**
 * e_account_list_remove:
 * @accounts: 
 * @account: 
 * 
 * Remove an account from the account list, and emit the
 * account-removed signal.  If the account was the default account,
 * then reset the default to the first account.
 **/
void
e_account_list_remove(EAccountList *accounts, EAccount *account)
{
	if (account == e_account_list_get_default(accounts))
		gconf_client_unset (accounts->priv->gconf, "/apps/evolution/mail/default_account", NULL);

	/* not sure if need to ref but no harm */
	g_object_ref (account);
	e_list_remove ((EList *) accounts, account);
	g_signal_emit(accounts, signals[ACCOUNT_REMOVED], 0, account);
	g_object_unref (account);
}

/**
 * e_account_list_get_default:
 * @accounts: 
 * 
 * Get the default account.  If no default is specified, or the default
 * has become stale, then the first account is made the default.
 * 
 * Return value: The account or NULL if no accounts are defined.
 **/
const EAccount *
e_account_list_get_default(EAccountList *accounts)
{
	char *uid;
	EIterator *it;
	const EAccount *account = NULL;

	uid = gconf_client_get_string (accounts->priv->gconf, "/apps/evolution/mail/default_account", NULL);
	it = e_list_get_iterator ((EList *)accounts);

	if (uid) {
		for (;e_iterator_is_valid (it);e_iterator_next (it)) {
			account = (const EAccount *)e_iterator_get (it);

			if (!strcmp(uid, account->uid))
				break;
			account = NULL;
		}
		e_iterator_reset(it);
	}

	/* no uid or uid not found, @it will be at the first account */
	if (account == NULL && e_iterator_is_valid(it)) {
		account = (const EAccount *) e_iterator_get (it);
		gconf_client_set_string (accounts->priv->gconf, "/apps/evolution/mail/default_account", account->uid, NULL);
	}

	g_object_unref(it);
	g_free(uid);

	return account;
}

/**
 * e_account_list_set_default:
 * @accounts: 
 * @account: 
 * 
 * Set the account @account to be the default account.
 **/
void
e_account_list_set_default(EAccountList *accounts, EAccount *account)
{
	gconf_client_set_string (accounts->priv->gconf, "/apps/evolution/mail/default_account", account->uid, NULL);
}

/**
 * e_account_list_find:
 * @accounts: 
 * @type: Type of search.
 * @key: Search key.
 * 
 * Perform a search of the account list on a single key.
 *
 * @type must be set from one of the following search types:
 * E_ACCOUNT_FIND_NAME - Find an account by account name.
 * E_ACCOUNT_FIND_ID_NAME - Find an account by the owner's identity name.
 * E_ACCOUNT_FIND_ID_ADDRESS - Find an account by the owner's identity address.
 * 
 * Return value: The account or NULL if it doesn't exist.
 **/
const EAccount *
e_account_list_find(EAccountList *accounts, e_account_find_t type, const char *key)
{
	char *val;
	EIterator *it;
	const EAccount *account = NULL;

	/* this could use a callback for more flexibility ...
	   ... but this makes the common cases easier */

	if (!key)
		return NULL;
	
	for (it = e_list_get_iterator ((EList *)accounts);
	     e_iterator_is_valid (it);
	     e_iterator_next (it)) {
		int found = 0;

		account = (const EAccount *)e_iterator_get (it);

		val = NULL;
		switch(type) {
		case E_ACCOUNT_FIND_NAME:
			found = strcmp(account->name, key) == 0;
			break;
		case E_ACCOUNT_FIND_UID:
			found = strcmp(account->uid, key) == 0;
			break;
		case E_ACCOUNT_FIND_ID_NAME:
			if (account->id)
				found = strcmp(account->id->name, key) == 0;
			break;
		case E_ACCOUNT_FIND_ID_ADDRESS:
			if (account->id)
				found = g_ascii_strcasecmp(account->id->address, key) == 0;
			break;
		}

		if (found)
			break;

		account = NULL;
	}
	g_object_unref(it);

	return account;
}

