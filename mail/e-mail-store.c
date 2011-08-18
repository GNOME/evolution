/*
 * e-mail-store.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-store.h"

#include <glib/gi18n.h>
#include <camel/camel.h>
#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>

#include "e-util/e-account-utils.h"

#include "mail/e-mail-local.h"
#include "mail/em-folder-tree-model.h"
#include "mail/em-utils.h"
#include "mail/mail-folder-cache.h"
#include "mail/mail-mt.h"
#include "mail/mail-ops.h"

typedef struct _StoreInfo StoreInfo;

typedef void	(*AddStoreCallback)	(MailFolderCache *folder_cache,
					 CamelStore *store,
					 CamelFolderInfo *info,
					 StoreInfo *store_info);

struct _StoreInfo {
	gint ref_count;

	CamelStore *store;

	/* Hold a reference to keep them alive. */
	CamelFolder *vtrash;
	CamelFolder *vjunk;

	AddStoreCallback callback;

	guint removed : 1;
};

CamelStore *vfolder_store;  /* XXX write a get () function for this */
static GHashTable *store_table;

static StoreInfo *
store_info_new (CamelStore *store)
{
	StoreInfo *store_info;

	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);

	store_info = g_slice_new0 (StoreInfo);
	store_info->ref_count = 1;

	store_info->store = g_object_ref (store);

	/* If these are vfolders then they need to be opened now,
	 * otherwise they won't keep track of all folders. */
	if (store->flags & CAMEL_STORE_VTRASH)
		store_info->vtrash =
			camel_store_get_trash_folder_sync (store, NULL, NULL);
	if (store->flags & CAMEL_STORE_VJUNK)
		store_info->vjunk =
			camel_store_get_junk_folder_sync (store, NULL, NULL);

	return store_info;
}

static StoreInfo *
store_info_ref (StoreInfo *store_info)
{
	g_return_val_if_fail (store_info != NULL, store_info);
	g_return_val_if_fail (store_info->ref_count > 0, store_info);

	g_atomic_int_inc (&store_info->ref_count);

	return store_info;
}

static void
store_info_unref (StoreInfo *store_info)
{
	g_return_if_fail (store_info != NULL);
	g_return_if_fail (store_info->ref_count > 0);

	if (g_atomic_int_dec_and_test (&store_info->ref_count)) {

		g_object_unref (store_info->store);

		if (store_info->vtrash != NULL)
			g_object_unref (store_info->vtrash);

		if (store_info->vjunk != NULL)
			g_object_unref (store_info->vjunk);

		g_slice_free (StoreInfo, store_info);
	}
}

static void
store_table_free (StoreInfo *store_info)
{
	store_info->removed = 1;
	store_info_unref (store_info);
}

static gboolean
mail_store_note_store_cb (MailFolderCache *folder_cache,
                          CamelStore *store,
                          CamelFolderInfo *info,
                          gpointer user_data)
{
	StoreInfo *store_info = user_data;

	if (store_info->callback != NULL)
		store_info->callback (
			folder_cache, store, info, store_info);

	if (!store_info->removed) {
		/* This keeps message counters up-to-date. */
		if (store_info->vtrash != NULL)
			mail_folder_cache_note_folder (
				folder_cache, store_info->vtrash);
		if (store_info->vjunk != NULL)
			mail_folder_cache_note_folder (
				folder_cache, store_info->vjunk);
	}

	store_info_unref (store_info);

	return TRUE;
}

static void
mail_store_add (EMailSession *session,
                CamelStore *store,
                AddStoreCallback callback)
{
	EMFolderTreeModel *default_model;
	MailFolderCache *folder_cache;
	CamelProvider *provider;
	StoreInfo *store_info;

	g_return_if_fail (store_table != NULL);
	g_return_if_fail (store != NULL);
	g_return_if_fail (CAMEL_IS_STORE (store));

	provider = camel_service_get_provider (CAMEL_SERVICE (store));

	g_return_if_fail (
		(provider->flags & CAMEL_PROVIDER_IS_STORAGE) != 0);

	default_model = em_folder_tree_model_get_default ();
	folder_cache = e_mail_session_get_folder_cache (session);

	store_info = store_info_new (store);
	store_info->callback = callback;

	g_hash_table_insert (store_table, store, store_info);

	em_folder_tree_model_add_store (default_model, store);

	mail_folder_cache_note_store (
		folder_cache, CAMEL_SESSION (session), store, NULL,
		mail_store_note_store_cb, store_info_ref (store_info));
}

static void
mail_store_add_local_done_cb (MailFolderCache *folder_cache,
                              CamelStore *store,
                              CamelFolderInfo *info,
                              StoreInfo *store_info)
{
	CamelFolder *folder;
	gint ii;

	for (ii = 0; ii < E_MAIL_NUM_LOCAL_FOLDERS; ii++) {
		folder = e_mail_local_get_folder (ii);
		if (folder == NULL)
			continue;
		mail_folder_cache_note_folder (folder_cache, folder);
	}
}

static void
mail_store_load_accounts (EMailSession *session,
                          const gchar *data_dir)
{
	CamelStore *local_store;
	EAccountList *account_list;
	EIterator *iter;

	/* Set up the local store. */

	e_mail_local_init (session, data_dir);
	local_store = e_mail_local_get_store ();

	mail_store_add (
		session, local_store, (AddStoreCallback)
		mail_store_add_local_done_cb);

	/* Set up remote stores. */

	account_list = e_get_account_list ();

	for (iter = e_list_get_iterator ((EList *) account_list);
	     e_iterator_is_valid (iter); e_iterator_next (iter)) {
		EAccount *account;

		account = (EAccount *) e_iterator_get (iter);

		if (!account->enabled)
			continue;

		e_mail_store_add_by_account (session, account);
	}

	g_object_unref (iter);
}

void
e_mail_store_init (EMailSession *session,
                   const gchar *data_dir)
{
	static gboolean initialized = FALSE;

	g_return_if_fail (E_IS_MAIL_SESSION (session));

	/* This function is idempotent because mail
	 * migration code may need to call it early. */
	if (initialized)
		return;

	/* Initialize global variables. */

	store_table = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) store_table_free);

	mail_store_load_accounts (session, data_dir);

	initialized = TRUE;
}

void
e_mail_store_add (EMailSession *session,
                  CamelStore *store)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (CAMEL_IS_STORE (store));

	mail_store_add (session, store, NULL);
}

CamelStore *
e_mail_store_add_by_account (EMailSession *session,
                             EAccount *account)
{
	CamelService *service = NULL;
	CamelProvider *provider;
	CamelURL *url;
	gboolean skip;
	GError *error = NULL;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);
	g_return_val_if_fail (E_IS_ACCOUNT (account), NULL);

	/* Load the service, but don't connect.  Check its provider,
	 * and if this belongs in the folder tree model, add it. */

	provider = camel_provider_get (account->source->url, &error);
	if (provider == NULL) {
	/* In case we do not have a provider here, we handle
	 * the special case of having multiple mail identities
	 * eg. a dummy account having just SMTP server defined */
		goto handle_transport;
	}

	service = camel_session_add_service (
		CAMEL_SESSION (session),
		account->uid, account->source->url,
		CAMEL_PROVIDER_STORE, &error);

	camel_service_set_display_name (service, account->name);

handle_transport:

	if (account->transport) {
		/* While we're at it, add the account's transport to the
		 * CamelSession.  The transport's UID is a kludge for now.
		 * We take the EAccount's UID and tack on "-transport". */
		gchar *transport_uid;
		GError *transport_error = NULL;

		transport_uid = g_strconcat (
			account->uid, "-transport", NULL);

		camel_session_add_service (
			CAMEL_SESSION (session),
			transport_uid, account->transport->url,
			CAMEL_PROVIDER_TRANSPORT, &transport_error);

		g_free (transport_uid);

		if (transport_error) {
			g_warning (
				"%s: Failed to add transport service: %s",
				G_STRFUNC, transport_error->message);
			g_error_free (transport_error);
		}
	}

	if (!CAMEL_IS_STORE (service))
		goto fail;

	/* Do not add local-delivery files,
	 * but make them ready for later use. */
	url = camel_url_new (account->source->url, NULL);
	if (url != NULL) {
		skip = em_utils_is_local_delivery_mbox_file (url);
		camel_url_free (url);
	}

	if (!skip && (provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		e_mail_store_add (session, CAMEL_STORE (service));

	return CAMEL_STORE (service);

fail:
	/* FIXME: Show an error dialog. */
	g_warning (
		"Couldn't get service: %s: %s", account->name,
		error ? error->message : "Not a CamelStore");
	if (error)
		g_error_free (error);

	return NULL;
}

void
e_mail_store_remove (EMailSession *session,
                     CamelStore *store)
{
	MailFolderCache *folder_cache;
	EMFolderTreeModel *default_model;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (store_table != NULL);

	/* Because the store table holds a reference to each store used
	 * as a key in it, none of them will ever be gc'ed, meaning any
	 * call to camel_session_get_{service,store} with the same URL
	 * will always return the same object.  So this works. */

	if (g_hash_table_lookup (store_table, store) == NULL)
		return;

	g_object_ref (store);

	g_hash_table_remove (store_table, store);

	folder_cache = e_mail_session_get_folder_cache (session);
	mail_folder_cache_note_store_remove (folder_cache, store);

	default_model = em_folder_tree_model_get_default ();
	em_folder_tree_model_remove_store (default_model, store);

	mail_disconnect_store (store);

	g_object_unref (store);
}

void
e_mail_store_remove_by_account (EMailSession *session,
                                EAccount *account)
{
	CamelService *service;
	CamelProvider *provider;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (E_IS_ACCOUNT (account));

	provider = camel_provider_get (account->source->url, NULL);
	if (provider == NULL)
		return;

	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		return;

	service = camel_session_get_service (
		CAMEL_SESSION (session), account->uid);

	g_return_if_fail (CAMEL_IS_STORE (service));

	e_mail_store_remove (session, CAMEL_STORE (service));
}

void
e_mail_store_foreach (GFunc func,
                      gpointer user_data)
{
	GHashTableIter iter;
	gpointer store;

	g_return_if_fail (func != NULL);

	/* case when started in other than mailer component and closing evolution */
	if (!store_table)
		return;

	g_hash_table_iter_init (&iter, store_table);

	while (g_hash_table_iter_next (&iter, &store, NULL)) {

		/* Just being paranoid. */
		g_return_if_fail (CAMEL_IS_STORE (store));

		func (store, user_data);
	}
}
