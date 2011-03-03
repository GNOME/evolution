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
	gchar *display_name;

	/* Hold a reference to keep them alive. */
	CamelFolder *vtrash;
	CamelFolder *vjunk;

	AddStoreCallback callback;

	guint removed : 1;
};

CamelStore *vfolder_store;  /* XXX write a get () function for this */
static GHashTable *store_table;

static StoreInfo *
store_info_new (CamelStore *store,
                const gchar *display_name)
{
	CamelService *service;
	StoreInfo *store_info;

	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);

	service = CAMEL_SERVICE (store);

	store_info = g_slice_new0 (StoreInfo);
	store_info->ref_count = 1;

	g_object_ref (store);
	store_info->store = store;

	if (display_name == NULL)
		store_info->display_name =
			camel_service_get_name (service, TRUE);
	else
		store_info->display_name = g_strdup (display_name);

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

	g_atomic_int_add (&store_info->ref_count, 1);

	return store_info;
}

static void
store_info_unref (StoreInfo *store_info)
{
	g_return_if_fail (store_info != NULL);
	g_return_if_fail (store_info->ref_count > 0);

	if (g_atomic_int_exchange_and_add (&store_info->ref_count, -1) > 1)
		return;

	g_object_unref (store_info->store);
	g_free (store_info->display_name);

	if (store_info->vtrash != NULL)
		g_object_unref (store_info->vtrash);

	if (store_info->vjunk != NULL)
		g_object_unref (store_info->vjunk);

	g_slice_free (StoreInfo, store_info);
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
                const gchar *display_name,
                AddStoreCallback callback)
{
	EMFolderTreeModel *default_model;
	MailFolderCache *folder_cache;
	StoreInfo *store_info;

	g_return_if_fail (store_table != NULL);
	g_return_if_fail (store != NULL);
	g_return_if_fail (CAMEL_IS_STORE (store));

	g_return_if_fail (
		(CAMEL_SERVICE (store)->provider->flags &
		CAMEL_PROVIDER_IS_STORAGE) != 0);

	default_model = em_folder_tree_model_get_default ();
	folder_cache = e_mail_session_get_folder_cache (session);

	store_info = store_info_new (store, display_name);
	store_info->callback = callback;

	g_hash_table_insert (store_table, store, store_info);

	em_folder_tree_model_add_store (
		default_model, store, store_info->display_name);

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
		session, local_store, _("On This Computer"),
		(AddStoreCallback) mail_store_add_local_done_cb);

	/* Set up remote stores. */

	account_list = e_get_account_list ();

	for (iter = e_list_get_iterator ((EList *) account_list);
		e_iterator_is_valid (iter); e_iterator_next (iter)) {

		EAccountService *service;
		EAccount *account;
		const gchar *display_name;
		const gchar *uri;

		account = (EAccount *) e_iterator_get (iter);
		display_name = account->name;
		service = account->source;
		uri = service->url;

		if (!account->enabled)
			continue;

		if (uri == NULL || *uri == '\0')
			continue;

		/* do not add local-delivery files into the tree those are server specifically */
		if (em_utils_is_local_delivery_mbox_file (uri))
			continue;

		e_mail_store_add_by_uri (session, uri, display_name);
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
                  CamelStore *store,
                  const gchar *display_name)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (display_name != NULL);

	mail_store_add (session, store, display_name, NULL);
}

CamelStore *
e_mail_store_add_by_uri (EMailSession *session,
                         const gchar *uri,
                         const gchar *display_name)
{
	CamelService *service;
	CamelProvider *provider;
	GError *local_error = NULL;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);
	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (display_name != NULL, NULL);

	/* Load the service, but don't connect.  Check its provider,
	 * and if this belongs in the folder tree model, add it. */

	provider = camel_provider_get (uri, &local_error);
	if (provider == NULL)
		goto fail;

	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		return NULL;

	service = camel_session_get_service (
		CAMEL_SESSION (session), uri,
		CAMEL_PROVIDER_STORE, &local_error);
	if (service == NULL)
		goto fail;

	e_mail_store_add (session, CAMEL_STORE (service), display_name);

	g_object_unref (service);

	return CAMEL_STORE (service);

fail:
	/* FIXME: Show an error dialog. */
	g_warning (
		"Couldn't get service: %s: %s", uri,
		local_error->message);
	g_error_free (local_error);

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
e_mail_store_remove_by_uri (EMailSession *session,
                            const gchar *uri)
{
	CamelService *service;
	CamelProvider *provider;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (uri != NULL);

	provider = camel_provider_get (uri, NULL);
	if (provider == NULL)
		return;

	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		return;

	service = camel_session_get_service (
		CAMEL_SESSION (session), uri,
		CAMEL_PROVIDER_STORE, NULL);
	if (service == NULL)
		return;

	e_mail_store_remove (session, CAMEL_STORE (service));

	g_object_unref (service);
}

void
e_mail_store_foreach (GHFunc func,
                      gpointer user_data)
{
	GHashTableIter iter;
	gpointer key, value;

	g_return_if_fail (func != NULL);

	/* case when started in other than mailer component and closing evolution */
	if (!store_table)
		return;

	g_hash_table_iter_init (&iter, store_table);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		StoreInfo *store_info = value;

		/* Just being paranoid. */
		g_return_if_fail (CAMEL_IS_STORE (key));
		g_return_if_fail (store_info != NULL);

		func (key, store_info->display_name, user_data);
	}
}
