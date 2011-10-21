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

#include "shell/e-shell.h"
#include "shell/e-shell-settings.h"

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

static gboolean
special_mail_store_is_enabled (CamelStore *store)
{
	CamelService *service;
	EShell *shell;
	EShellSettings *shell_settings;
	const gchar *uid, *prop = NULL;

	service = CAMEL_SERVICE (store);
	g_return_val_if_fail (service, FALSE);

	uid = camel_service_get_uid (service);
	if (g_strcmp0 (uid, "local") == 0)
		prop = "mail-enable-local-folders";
	else if (g_strcmp0 (uid, "vfolder") == 0)
		prop = "mail-enable-search-folders";

	if (!prop)
		return TRUE;

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	return e_shell_settings_get_boolean (shell_settings, prop);
}

static void
mail_store_add (EMailBackend *backend,
                CamelStore *store,
                AddStoreCallback callback)
{
	EMailSession *session;
	EMFolderTreeModel *default_model;
	MailFolderCache *folder_cache;
	StoreInfo *store_info;

	g_return_if_fail (store_table != NULL);
	g_return_if_fail (store != NULL);
	g_return_if_fail (CAMEL_IS_STORE (store));

	session = e_mail_backend_get_session (backend);
	default_model = em_folder_tree_model_get_default ();
	folder_cache = e_mail_session_get_folder_cache (session);

	store_info = store_info_new (store);
	store_info->callback = callback;

	g_hash_table_insert (store_table, store, store_info);

	if (special_mail_store_is_enabled (store))
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
mail_store_load_accounts (EMailBackend *backend,
                          const gchar *data_dir)
{
	CamelStore *local_store;
	EMailSession *session;
	EAccountList *account_list;
	EIterator *iter;

	session = e_mail_backend_get_session (backend);

	/* Add the local store. */

	e_mail_local_init (session, data_dir);
	local_store = e_mail_local_get_store ();

	mail_store_add (
		backend, local_store, (AddStoreCallback)
		mail_store_add_local_done_cb);

	/* Add mail accounts.. */

	account_list = e_get_account_list ();

	for (iter = e_list_get_iterator ((EList *) account_list);
	     e_iterator_is_valid (iter); e_iterator_next (iter)) {
		EAccount *account;

		account = (EAccount *) e_iterator_get (iter);

		if (!account->enabled)
			continue;

		e_mail_store_add_by_account (backend, account);
	}

	g_object_unref (iter);
}

void
e_mail_store_init (EMailBackend *backend,
                   const gchar *data_dir)
{
	static gboolean initialized = FALSE;

	g_return_if_fail (E_IS_MAIL_BACKEND (backend));

	/* This function is idempotent because mail
	 * migration code may need to call it early. */
	if (initialized)
		return;

	/* Initialize global variables. */

	store_table = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) store_table_free);

	mail_store_load_accounts (backend, data_dir);

	initialized = TRUE;
}

void
e_mail_store_add (EMailBackend *backend,
                  CamelStore *store)
{
	g_return_if_fail (E_IS_MAIL_BACKEND (backend));
	g_return_if_fail (CAMEL_IS_STORE (store));

	mail_store_add (backend, store, NULL);
}

CamelStore *
e_mail_store_add_by_account (EMailBackend *backend,
                             EAccount *account)
{
	EMailSession *session;
	CamelService *service = NULL;
	CamelProvider *provider;
	CamelURL *url;
	gboolean transport_only;
	gboolean service_is_local_delivery;
	gboolean service_belongs_in_tree_model;
	GError *error = NULL;

	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), NULL);
	g_return_val_if_fail (E_IS_ACCOUNT (account), NULL);

	session = e_mail_backend_get_session (backend);

	/* check whether it's transport-only accounts */
	transport_only =
		(account->source == NULL) ||
		(account->source->url == NULL) ||
		(*account->source->url == '\0');
	if (transport_only)
		goto handle_transport;

	/* Load the service, but don't connect.  Check its provider,
	 * and if this belongs in the folder tree model, add it. */

	url = camel_url_new (account->source->url, NULL);
	if (url != NULL) {
		service_is_local_delivery =
			em_utils_is_local_delivery_mbox_file (url);
		provider = camel_provider_get (url->protocol, NULL);
		camel_url_free (url);
	} else {
		service_is_local_delivery = FALSE;
		provider = NULL;
	}

	if (provider == NULL) {
		/* In case we do not have a provider here, we handle
		 * the special case of having multiple mail identities
		 * eg. a dummy account having just SMTP server defined */
		goto handle_transport;
	}

	service = camel_session_add_service (
		CAMEL_SESSION (session),
		account->uid, provider->protocol,
		CAMEL_PROVIDER_STORE, &error);

	if (!CAMEL_IS_STORE (service))
		goto fail;

	camel_service_set_display_name (service, account->name);

	service_belongs_in_tree_model =
		(provider->flags & CAMEL_PROVIDER_IS_STORAGE) &&
		!service_is_local_delivery;

	if (service_belongs_in_tree_model && store_table != NULL)
		e_mail_store_add (backend, CAMEL_STORE (service));

handle_transport:

	/* While we're at it, add the account's transport (if it has one)
	 * to the CamelSession.  The transport's UID is a kludge for now.
	 * We take the EAccount's UID and tack on "-transport". */

	if (account->transport) {
		GError *transport_error = NULL;

		url = camel_url_new (
			account->transport->url,
			&transport_error);

		if (url != NULL) {
			provider = camel_provider_get (
				url->protocol, &transport_error);
			camel_url_free (url);
		} else
			provider = NULL;

		if (provider != NULL) {
			gchar *transport_uid;

			transport_uid = g_strconcat (
				account->uid, "-transport", NULL);

			camel_session_add_service (
				CAMEL_SESSION (session),
				transport_uid, provider->protocol,
				CAMEL_PROVIDER_TRANSPORT, &transport_error);

			g_free (transport_uid);
		}

		if (transport_error) {
			g_warning (
				"%s: Failed to add transport service: %s",
				G_STRFUNC, transport_error->message);
			g_error_free (transport_error);
		}
	}

	if (transport_only)
		return NULL;

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
e_mail_store_remove (EMailBackend *backend,
                     CamelStore *store)
{
	EMailSession *session;
	MailFolderCache *folder_cache;
	EMFolderTreeModel *default_model;

	g_return_if_fail (E_IS_MAIL_BACKEND (backend));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (store_table != NULL);

	session = e_mail_backend_get_session (backend);

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
e_mail_store_remove_by_account (EMailBackend *backend,
                                EAccount *account)
{
	EMailSession *session;
	CamelService *service;
	CamelProvider *provider;
	const gchar *uid;

	g_return_if_fail (E_IS_MAIL_BACKEND (backend));
	g_return_if_fail (E_IS_ACCOUNT (account));

	uid = account->uid;
	session = e_mail_backend_get_session (backend);

	service = camel_session_get_service (CAMEL_SESSION (session), uid);
	g_return_if_fail (CAMEL_IS_STORE (service));

	provider = camel_service_get_provider (service);
	g_return_if_fail (provider != NULL);

	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE) || store_table == NULL)
		return;

	e_mail_store_remove (backend, CAMEL_STORE (service));
}

void
e_mail_store_foreach (EMailBackend *backend,
                      GFunc func,
                      gpointer user_data)
{
	EMailSession *session;
	GList *list, *link;

	/* XXX This is a silly convenience function.
	 *     Could probably just get rid of it. */

	g_return_if_fail (E_IS_MAIL_BACKEND (backend));
	g_return_if_fail (func != NULL);

	session = e_mail_backend_get_session (backend);

	list = camel_session_list_services (CAMEL_SESSION (session));

	for (link = list; link != NULL; link = g_list_next (link)) {
		CamelService *service = CAMEL_SERVICE (link->data);

		if (CAMEL_IS_STORE (service))
			func (service, user_data);
	}

	g_list_free (list);
}
