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
#include "config.h"
#endif

#include "e-account-list.h"
#include "e-account.h"
#include "e-util-marshal.h"

#include <gal/util/e-util.h>

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

#define PARENT_TYPE E_TYPE_LIST
static EListClass *parent_class = NULL;

static void dispose (GObject *);
static void finalize (GObject *);

static void
class_init (GObjectClass *object_class)
{
	parent_class = g_type_class_ref (PARENT_TYPE);

	/* virtual method override */
	object_class->dispose = dispose;
	object_class->finalize = finalize;

	/* signals */
	signals[ACCOUNT_ADDED] =
		g_signal_new ("account_added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAccountListClass, account_added),
			      NULL, NULL,
			      e_util_marshal_NONE__OBJECT,
			      G_TYPE_NONE, 1,
			      E_TYPE_ACCOUNT);
	signals[ACCOUNT_CHANGED] =
		g_signal_new ("account_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAccountListClass, account_changed),
			      NULL, NULL,
			      e_util_marshal_NONE__OBJECT,
			      G_TYPE_NONE, 1,
			      E_TYPE_ACCOUNT);
	signals[ACCOUNT_REMOVED] =
		g_signal_new ("account_removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAccountListClass, account_removed),
			      NULL, NULL,
			      e_util_marshal_NONE__OBJECT,
			      G_TYPE_NONE, 1,
			      E_TYPE_ACCOUNT);
}

static void
init (GObject *object)
{
	EAccountList *account_list = E_ACCOUNT_LIST (object);

	account_list->priv = g_new0 (EAccountListPrivate, 1);
}

static void
dispose (GObject *object)
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

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	EAccountList *account_list = E_ACCOUNT_LIST (object);

	g_free (account_list->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

E_MAKE_TYPE (e_account_list, "EAccountList", EAccountList, class_init, init, PARENT_TYPE)


static void
gconf_accounts_changed (GConfClient *client, guint cnxn_id,
			GConfEntry *entry, gpointer user_data)
{
	EAccountList *account_list = user_data;
	GSList *list, *l;
	EAccount *account;
	EList *old_accounts;
	EIterator *iter;
	const char *uid;

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
		g_signal_emit (account_list, signals[ACCOUNT_ADDED], 0, account);
		g_object_unref (account);

	next:
		g_object_unref (iter);
	}

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
