/*
 * e-mail-account-store.c
 *
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
 */

#include "evolution-config.h"

#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include <libemail-engine/libemail-engine.h>

#include "mail-vfolder-ui.h"

#include "e-mail-account-store.h"


typedef struct _IndexItem IndexItem;

struct _EMailAccountStorePrivate {
	CamelService *default_service;
	GHashTable *service_index;
	gchar *sort_order_filename;
	gpointer session;  /* weak pointer */
	guint busy_count;
};

struct _IndexItem {
	CamelService *service;
	GtkTreeRowReference *reference;
	gulong notify_handler_id;
};

enum {
	PROP_0,
	PROP_BUSY,
	PROP_DEFAULT_SERVICE,
	PROP_SESSION
};

enum {
	SERVICE_ADDED,
	SERVICE_REMOVED,
	SERVICE_ENABLED,
	SERVICE_DISABLED,
	SERVICES_REORDERED,
	REMOVE_REQUESTED,
	ENABLE_REQUESTED,
	DISABLE_REQUESTED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Forward Declarations */
static void	e_mail_account_store_interface_init
						(GtkTreeModelIface *iface);

G_DEFINE_TYPE_WITH_CODE (EMailAccountStore, e_mail_account_store, GTK_TYPE_LIST_STORE,
	G_ADD_PRIVATE (EMailAccountStore)
	G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, e_mail_account_store_interface_init)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
index_item_free (IndexItem *item)
{
	g_signal_handler_disconnect (
		item->service, item->notify_handler_id);

	g_object_unref (item->service);
	gtk_tree_row_reference_free (item->reference);

	g_slice_free (IndexItem, item);
}

static gboolean
mail_account_store_get_iter (EMailAccountStore *store,
                             CamelService *service,
                             GtkTreeIter *iter)
{
	IndexItem *item;
	GtkTreeModel *model;
	GtkTreePath *path;
	gboolean iter_set;

	g_return_val_if_fail (service != NULL, FALSE);

	item = g_hash_table_lookup (store->priv->service_index, service);

	if (item == NULL)
		return FALSE;

	if (!gtk_tree_row_reference_valid (item->reference))
		return FALSE;

	model = gtk_tree_row_reference_get_model (item->reference);
	path = gtk_tree_row_reference_get_path (item->reference);
	iter_set = gtk_tree_model_get_iter (model, iter, path);
	gtk_tree_path_free (path);

	return iter_set;
}

static gint
mail_account_store_default_compare (CamelService *service_a,
                                    CamelService *service_b,
                                    gpointer user_data)
{
	const gchar *display_name_a;
	const gchar *display_name_b;
	const gchar *uid_a;
	const gchar *uid_b;

	uid_a = camel_service_get_uid (service_a);
	uid_b = camel_service_get_uid (service_b);

	/* Check for special cases first. */

	if (g_str_equal (uid_a, E_MAIL_SESSION_LOCAL_UID))
		return -1;
	else if (g_str_equal (uid_b, E_MAIL_SESSION_LOCAL_UID))
		return 1;
	else if (g_str_equal (uid_a, E_MAIL_SESSION_VFOLDER_UID))
		return 1;
	else if (g_str_equal (uid_b, E_MAIL_SESSION_VFOLDER_UID))
		return -1;

	/* Otherwise sort them alphabetically. */

	display_name_a = camel_service_get_display_name (service_a);
	display_name_b = camel_service_get_display_name (service_b);

	if (display_name_a == NULL)
		display_name_a = "";

	if (display_name_b == NULL)
		display_name_b = "";

	return g_utf8_collate (display_name_a, display_name_b);
}

static gint
mail_account_store_get_defailt_index (EMailAccountStore *store,
				      CamelService *service)
{
	GQueue *current_order;
	gint intended_position;

	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), -1);
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), -1);

	current_order = g_queue_new ();
	e_mail_account_store_queue_services (store, current_order);

	g_queue_insert_sorted (current_order, service,
		(GCompareDataFunc) mail_account_store_default_compare, NULL);

	intended_position = g_queue_index (current_order, service);

	g_queue_free (current_order);

	return intended_position;
}

static void
mail_account_store_update_row (EMailAccountStore *store,
                               CamelService *service,
                               GtkTreeIter *iter)
{
	CamelProvider *provider;
	gboolean is_default;
	const gchar *backend_name;
	const gchar *display_name;
	gchar *from_transport_backend_name = NULL;

	if (!store->priv->default_service) {
		EMailSession *mail_session;
		ESourceRegistry *registry;
		ESource *source;

		mail_session = e_mail_account_store_get_session (store);
		registry = e_mail_session_get_registry (mail_session);
		source = e_source_registry_ref_default_mail_account (registry);

		if (source) {
			store->priv->default_service = camel_session_ref_service (CAMEL_SESSION (mail_session), e_source_get_uid (source));
			g_object_unref (source);
		}
	}

	is_default = (service == store->priv->default_service);
	display_name = camel_service_get_display_name (service);

	provider = camel_service_get_provider (service);
	backend_name = (provider != NULL) ? provider->protocol : NULL;

	if (g_strcmp0 (backend_name, "none") == 0) {
		ESourceRegistry *registry;
		ESource *mail_source;

		registry = e_mail_session_get_registry (e_mail_account_store_get_session (store));
		mail_source = e_source_registry_ref_source (registry, camel_service_get_uid (service));

		if (mail_source &&
		    !e_source_has_extension (mail_source, E_SOURCE_EXTENSION_MAIL_SUBMISSION) &&
		    e_source_has_extension (mail_source, E_SOURCE_EXTENSION_MAIL_ACCOUNT)) {
			ESourceMailAccount *mail_account;
			ESource *identity_source = NULL;
			const gchar *identity_uid;

			mail_account = e_source_get_extension (mail_source, E_SOURCE_EXTENSION_MAIL_ACCOUNT);

			e_source_extension_property_lock (E_SOURCE_EXTENSION (mail_account));

			identity_uid = e_source_mail_account_get_identity_uid (mail_account);
			if (identity_uid && *identity_uid)
				identity_source = e_source_registry_ref_source (registry, identity_uid);

			e_source_extension_property_unlock (E_SOURCE_EXTENSION (mail_account));

			g_object_unref (mail_source);
			mail_source = identity_source;
		}

		if (mail_source &&
		    e_source_has_extension (mail_source, E_SOURCE_EXTENSION_MAIL_SUBMISSION)) {
			ESourceMailSubmission *mail_submission;
			ESource *transport_source = NULL;
			const gchar *transport_uid;

			mail_submission = e_source_get_extension (mail_source, E_SOURCE_EXTENSION_MAIL_SUBMISSION);

			e_source_extension_property_lock (E_SOURCE_EXTENSION (mail_submission));

			transport_uid = e_source_mail_submission_get_transport_uid (mail_submission);
			if (transport_uid && *transport_uid)
				transport_source = e_source_registry_ref_source (registry, transport_uid);

			e_source_extension_property_unlock (E_SOURCE_EXTENSION (mail_submission));

			if (transport_source && e_source_has_extension (transport_source, E_SOURCE_EXTENSION_MAIL_TRANSPORT)) {
				ESourceMailTransport *mail_transport;

				mail_transport = e_source_get_extension (transport_source, E_SOURCE_EXTENSION_MAIL_TRANSPORT);

				from_transport_backend_name = e_source_backend_dup_backend_name (E_SOURCE_BACKEND (mail_transport));

				if (from_transport_backend_name && *from_transport_backend_name)
					backend_name = from_transport_backend_name;
			}

			g_clear_object (&transport_source);
		}

		g_clear_object (&mail_source);
	}

	gtk_list_store_set (
		GTK_LIST_STORE (store), iter,
		E_MAIL_ACCOUNT_STORE_COLUMN_DEFAULT, is_default,
		E_MAIL_ACCOUNT_STORE_COLUMN_BACKEND_NAME, backend_name,
		E_MAIL_ACCOUNT_STORE_COLUMN_DISPLAY_NAME, display_name,
		-1);

	g_free (from_transport_backend_name);
}

struct ServiceNotifyCbData
{
	EMailAccountStore *store;
	CamelService *service;
};

static void
service_notify_cb_data_free (gpointer ptr)
{
	struct ServiceNotifyCbData *data = ptr;

	g_clear_object (&data->store);
	g_clear_object (&data->service);
	g_slice_free (struct ServiceNotifyCbData, data);
}

static gboolean
mail_account_store_service_notify_idle_cb (gpointer user_data)
{
	struct ServiceNotifyCbData *data = user_data;
	GtkTreeIter iter;

	g_return_val_if_fail (data != NULL, FALSE);

	if (mail_account_store_get_iter (data->store, data->service, &iter))
		mail_account_store_update_row (data->store, data->service, &iter);

	return FALSE;
}

static void
mail_account_store_service_notify_cb (CamelService *service,
                                      GParamSpec *pspec,
                                      EMailAccountStore *store)
{
	struct ServiceNotifyCbData *data;

	data = g_slice_new0 (struct ServiceNotifyCbData);
	data->store = g_object_ref (store);
	data->service = g_object_ref (service);

	g_idle_add_full (
		G_PRIORITY_DEFAULT_IDLE,
		mail_account_store_service_notify_idle_cb,
		data,
		service_notify_cb_data_free);
}

static void
mail_account_store_remove_source_cb (ESource *source,
                                     GAsyncResult *result,
                                     EMailAccountStore *store)
{
	GError *error = NULL;

	/* FIXME EMailAccountStore should implement EAlertSink. */
	if (!e_source_remove_finish (source, result, &error)) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	g_return_if_fail (store->priv->busy_count > 0);
	store->priv->busy_count--;
	g_object_notify (G_OBJECT (store), "busy");

	g_object_unref (store);
}

static void
mail_account_store_write_source_cb (ESource *source,
                                    GAsyncResult *result,
                                    EMailAccountStore *store)
{
	GError *error = NULL;

	/* FIXME EMailAccountStore should implement EAlertSink. */
	if (!e_source_write_finish (source, result, &error)) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	g_return_if_fail (store->priv->busy_count > 0);
	store->priv->busy_count--;
	g_object_notify (G_OBJECT (store), "busy");

	g_object_unref (store);
}

static void
mail_account_store_clean_index (EMailAccountStore *store)
{
	GQueue trash = G_QUEUE_INIT;
	GHashTable *hash_table;
	GHashTableIter iter;
	gpointer key, value;

	hash_table = store->priv->service_index;
	g_hash_table_iter_init (&iter, hash_table);

	/* Remove index items with invalid GtkTreeRowReferences. */

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		IndexItem *item = value;

		if (!gtk_tree_row_reference_valid (item->reference))
			g_queue_push_tail (&trash, key);
	}

	while ((key = g_queue_pop_head (&trash)) != NULL)
		g_hash_table_remove (hash_table, key);
}

static void
mail_account_store_update_index (EMailAccountStore *store,
                                 GtkTreePath *path,
                                 GtkTreeIter *iter)
{
	CamelService *service = NULL;
	GHashTable *hash_table;
	GtkTreeModel *model;
	IndexItem *item;

	model = GTK_TREE_MODEL (store);
	hash_table = store->priv->service_index;

	gtk_tree_model_get (
		model, iter,
		E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE, &service, -1);

	if (service == NULL)
		return;

	item = g_hash_table_lookup (hash_table, service);

	if (item == NULL) {
		item = g_slice_new0 (IndexItem);
		item->service = g_object_ref (service);

		item->notify_handler_id = g_signal_connect (
			service, "notify", G_CALLBACK (
			mail_account_store_service_notify_cb), store);

		g_hash_table_insert (hash_table, item->service, item);
	}

	/* Update the row reference so the IndexItem will survive
	 * drag-and-drop (new row is inserted, old row is deleted). */
	gtk_tree_row_reference_free (item->reference);
	item->reference = gtk_tree_row_reference_new (model, path);

	g_object_unref (service);
}

static void
mail_account_store_set_session (EMailAccountStore *store,
                                EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (store->priv->session == NULL);

	store->priv->session = session;

	g_object_add_weak_pointer (
		G_OBJECT (store->priv->session),
		&store->priv->session);
}

static void
mail_account_store_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DEFAULT_SERVICE:
			e_mail_account_store_set_default_service (
				E_MAIL_ACCOUNT_STORE (object),
				g_value_get_object (value));
			return;

		case PROP_SESSION:
			mail_account_store_set_session (
				E_MAIL_ACCOUNT_STORE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_account_store_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BUSY:
			g_value_set_boolean (
				value,
				e_mail_account_store_get_busy (
				E_MAIL_ACCOUNT_STORE (object)));
			return;

		case PROP_DEFAULT_SERVICE:
			g_value_set_object (
				value,
				e_mail_account_store_get_default_service (
				E_MAIL_ACCOUNT_STORE (object)));
			return;

		case PROP_SESSION:
			g_value_set_object (
				value,
				e_mail_account_store_get_session (
				E_MAIL_ACCOUNT_STORE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_account_store_dispose (GObject *object)
{
	EMailAccountStore *self = E_MAIL_ACCOUNT_STORE (object);

	if (self->priv->session != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (self->priv->session), &self->priv->session);
		self->priv->session = NULL;
	}

	g_clear_object (&self->priv->default_service);

	g_hash_table_remove_all (self->priv->service_index);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_account_store_parent_class)->dispose (object);
}

static void
mail_account_store_finalize (GObject *object)
{
	EMailAccountStore *self = E_MAIL_ACCOUNT_STORE (object);

	g_warn_if_fail (self->priv->busy_count == 0);
	g_hash_table_destroy (self->priv->service_index);
	g_free (self->priv->sort_order_filename);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_account_store_parent_class)->finalize (object);
}

static void
mail_account_store_constructed (GObject *object)
{
	EMailAccountStore *store;
	EMailSession *session;
	ESourceRegistry *registry;
	const gchar *config_dir;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_account_store_parent_class)->constructed (object);

	store = E_MAIL_ACCOUNT_STORE (object);
	session = e_mail_account_store_get_session (store);
	registry = e_mail_session_get_registry (session);

	/* Bind the default mail account ESource to our default
	 * CamelService, with help from some transform functions. */
	e_binding_bind_property_full (
		registry, "default-mail-account",
		store, "default-service",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_source_to_service,
		e_binding_transform_service_to_source,
		session, (GDestroyNotify) NULL);

	config_dir = mail_session_get_config_dir ();

	/* XXX Should we take the filename as a constructor property? */
	store->priv->sort_order_filename = g_build_filename (
		config_dir, "sortorder.ini", NULL);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
call_allow_auth_prompt (ESource *source)
{
	EShell *shell;

	if (!source)
		return;

	g_return_if_fail (E_IS_SOURCE (source));

	shell = e_shell_get_default ();
	e_shell_allow_auth_prompt_for (shell, source);
}

static void
mail_account_store_service_added (EMailAccountStore *store,
                                  CamelService *service)
{
	/* Placeholder so subclasses can safely chain up. */
}

static void
mail_account_store_service_removed (EMailAccountStore *store,
                                    CamelService *service)
{
	EMailSession *session;
	MailFolderCache *cache;
	ESourceRegistry *registry;
	ESource *source;
	const gchar *uid;

	session = e_mail_account_store_get_session (store);
	cache = e_mail_session_get_folder_cache (session);

	mail_folder_cache_service_removed (cache, service);

	uid = camel_service_get_uid (service);
	registry = e_mail_session_get_registry (session);
	source = e_source_registry_ref_source (registry, uid);

	/* If this ESource is part of a collection, we need to remove
	 * the entire collection.  Check the ESource and its ancestors
	 * for a collection extension and remove the containing source. */
	if (source != NULL) {
		ESource *collection;

		collection = e_source_registry_find_extension (
			registry, source, E_SOURCE_EXTENSION_COLLECTION);
		if (collection != NULL) {
			g_object_unref (source);
			source = collection;
		}
	}

	if (source != NULL && e_source_get_removable (source)) {
		store->priv->busy_count++;
		g_object_notify (G_OBJECT (store), "busy");

		/* XXX Should this be cancellable? */
		e_source_remove (
			source, NULL, (GAsyncReadyCallback)
			mail_account_store_remove_source_cb,
			g_object_ref (store));

		g_object_unref (source);
	}
}

static void
mail_account_store_service_enabled (EMailAccountStore *store,
                                    CamelService *service)
{
	EMailSession *session;
	MailFolderCache *cache;
	ESourceRegistry *registry;
	ESource *source;
	const gchar *uid;

	session = e_mail_account_store_get_session (store);
	cache = e_mail_session_get_folder_cache (session);

	mail_folder_cache_service_enabled (cache, service);

	uid = camel_service_get_uid (service);
	registry = e_mail_session_get_registry (session);
	source = e_source_registry_ref_source (registry, uid);

	/* Locate the identity source referenced in the [Mail Account]
	 * extension.  We want to keep its enabled state synchronized
	 * with the account's enabled state.  (Need to do this before
	 * we swap the mail account ESource for a collection below.) */
	if (source != NULL) {
		ESource *identity = NULL;
		ESourceMailAccount *extension;
		const gchar *extension_name;

		extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
		extension = e_source_get_extension (source, extension_name);
		uid = e_source_mail_account_get_identity_uid (extension);

		if (uid != NULL)
			identity = e_source_registry_ref_source (registry, uid);

		if (identity != NULL && e_source_get_writable (identity) && !e_source_get_enabled (identity)) {
			e_source_set_enabled (identity, TRUE);

			store->priv->busy_count++;
			g_object_notify (G_OBJECT (store), "busy");

			/* XXX Should this be cancellable? */
			e_source_write (
				identity, NULL, (GAsyncReadyCallback)
				mail_account_store_write_source_cb,
				g_object_ref (store));

			g_object_unref (identity);
		}
	}

	/* If this ESource is part of a collection, we need to enable
	 * the entire collection.  Check the ESource and its ancestors
	 * for a collection extension and enable the containing source. */
	if (source != NULL) {
		ESource *collection;

		collection = e_source_registry_find_extension (
			registry, source, E_SOURCE_EXTENSION_COLLECTION);
		if (collection != NULL) {
			g_object_unref (source);
			source = collection;
		}
	}

	if (source != NULL && e_source_get_writable (source) && !e_source_get_enabled (source)) {
		e_source_set_enabled (source, TRUE);

		store->priv->busy_count++;
		g_object_notify (G_OBJECT (store), "busy");

		/* XXX Should this be cancellable? */
		e_source_write (
			source, NULL, (GAsyncReadyCallback)
			mail_account_store_write_source_cb,
			g_object_ref (store));

		g_object_unref (source);
	}
}

static void
mail_account_store_service_disabled (EMailAccountStore *store,
                                     CamelService *service)
{
	EMailSession *session;
	MailFolderCache *cache;
	ESourceRegistry *registry;
	ESource *source;
	const gchar *uid;

	session = e_mail_account_store_get_session (store);
	cache = e_mail_session_get_folder_cache (session);

	mail_folder_cache_service_disabled (cache, service);

	uid = camel_service_get_uid (service);
	registry = e_mail_session_get_registry (session);
	source = e_source_registry_ref_source (registry, uid);

	/* Locate the identity source referenced in the [Mail Account]
	 * extension.  We want to keep its enabled state synchronized
	 * with the account's enabled state.  (Need to do this before
	 * we swap the mail account ESource for a collection below.) */
	if (source != NULL) {
		ESource *identity = NULL;
		ESourceMailAccount *extension;
		const gchar *extension_name;

		call_allow_auth_prompt (source);

		extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
		extension = e_source_get_extension (source, extension_name);
		uid = e_source_mail_account_get_identity_uid (extension);

		if (uid != NULL) {
			identity = e_source_registry_ref_source (registry, uid);
			call_allow_auth_prompt (identity);
		}

		if (identity != NULL && e_source_get_writable (identity) && e_source_get_enabled (identity)) {
			e_source_set_enabled (identity, FALSE);

			store->priv->busy_count++;
			g_object_notify (G_OBJECT (store), "busy");

			/* XXX Should this be cancellable? */
			e_source_write (
				identity, NULL, (GAsyncReadyCallback)
				mail_account_store_write_source_cb,
				g_object_ref (store));

			g_object_unref (identity);
		}
	}

	/* If this ESource is part of a collection, we need to disable
	 * the entire collection.  Check the ESource and its ancestors
	 * for a collection extension and disable the containing source. */
	if (source != NULL) {
		ESource *collection;

		collection = e_source_registry_find_extension (
			registry, source, E_SOURCE_EXTENSION_COLLECTION);
		if (collection != NULL) {
			call_allow_auth_prompt (collection);

			g_object_unref (source);
			source = collection;
		}
	}

	if (source != NULL && e_source_get_writable (source) && e_source_get_enabled (source)) {
		e_source_set_enabled (source, FALSE);

		store->priv->busy_count++;
		g_object_notify (G_OBJECT (store), "busy");

		/* XXX Should this be cancellable? */
		e_source_write (
			source, NULL, (GAsyncReadyCallback)
			mail_account_store_write_source_cb,
			g_object_ref (store));

		g_object_unref (source);
	}
}

static void
mail_account_store_services_reordered (EMailAccountStore *store,
                                       gboolean default_restored)
{
	/* XXX Should this be made asynchronous? */

	GError *error = NULL;

	if (default_restored) {
		const gchar *filename;

		filename = store->priv->sort_order_filename;

		if (g_file_test (filename, G_FILE_TEST_EXISTS))
			g_unlink (filename);

		return;
	}

	if (!e_mail_account_store_save_sort_order (store, &error)) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
}

static gboolean
mail_account_store_remove_requested (EMailAccountStore *store,
                                     GtkWindow *parent_window,
                                     CamelService *service)
{
	gint response;

	/* FIXME Need to use "mail:ask-delete-account-with-proxies" if the
	 *       mail account has proxies.  But this is groupwise-specific
	 *       and doesn't belong here anyway.  Think of a better idea. */

	response = e_alert_run_dialog_for_args (
		parent_window, "mail:ask-delete-account", camel_service_get_display_name (service), NULL);

	return (response == GTK_RESPONSE_YES);
}

static gboolean
mail_account_store_enable_requested (EMailAccountStore *store,
                                     GtkWindow *parent_window,
                                     CamelService *service)
{
	return TRUE;
}

static gboolean
mail_account_store_disable_requested (EMailAccountStore *store,
                                      GtkWindow *parent_window,
                                      CamelService *service)
{
	/* FIXME Need to check whether the account has proxies and run a
	 *       "mail:ask-delete-proxy-accounts" alert dialog, but this
	 *       is groupwise-specific and doesn't belong here anyway.
	 *       Think of a better idea. */

	return TRUE;
}

static void
mail_account_store_row_changed (GtkTreeModel *tree_model,
                                GtkTreePath *path,
                                GtkTreeIter *iter)
{
	EMailAccountStore *store;

	/* Neither GtkTreeModel nor GtkListStore implements
	 * this method, so there is nothing to chain up to. */

	store = E_MAIL_ACCOUNT_STORE (tree_model);
	mail_account_store_update_index (store, path, iter);
}

static void
mail_account_store_row_inserted (GtkTreeModel *tree_model,
                                 GtkTreePath *path,
                                 GtkTreeIter *iter)
{
	EMailAccountStore *store;

	/* Neither GtkTreeModel nor GtkListStore implements
	 * this method, so there is nothing to chain up to. */

	store = E_MAIL_ACCOUNT_STORE (tree_model);
	mail_account_store_update_index (store, path, iter);
}

static gboolean
mail_account_store_true_proceed (GSignalInvocationHint *ihint,
                                 GValue *return_accumulator,
                                 const GValue *handler_return,
                                 gpointer not_used)
{
	gboolean proceed;

	proceed = g_value_get_boolean (handler_return);
	g_value_set_boolean (return_accumulator, proceed);

	return proceed;
}

static void
e_mail_account_store_class_init (EMailAccountStoreClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_account_store_set_property;
	object_class->get_property = mail_account_store_get_property;
	object_class->dispose = mail_account_store_dispose;
	object_class->finalize = mail_account_store_finalize;
	object_class->constructed = mail_account_store_constructed;

	class->service_added = mail_account_store_service_added;
	class->service_removed = mail_account_store_service_removed;
	class->service_enabled = mail_account_store_service_enabled;
	class->service_disabled = mail_account_store_service_disabled;
	class->services_reordered = mail_account_store_services_reordered;
	class->remove_requested = mail_account_store_remove_requested;
	class->enable_requested = mail_account_store_enable_requested;
	class->disable_requested = mail_account_store_disable_requested;

	g_object_class_install_property (
		object_class,
		PROP_BUSY,
		g_param_spec_boolean (
			"busy",
			"Busy",
			"Whether async operations are in progress",
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_SERVICE,
		g_param_spec_object (
			"default-service",
			"Default Service",
			"Default mail store",
			CAMEL_TYPE_SERVICE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			"Session",
			"Mail session",
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	signals[SERVICE_ADDED] = g_signal_new (
		"service-added",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, service_added),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		CAMEL_TYPE_SERVICE);

	signals[SERVICE_REMOVED] = g_signal_new (
		"service-removed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, service_removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		CAMEL_TYPE_SERVICE);

	signals[SERVICE_ENABLED] = g_signal_new (
		"service-enabled",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, service_enabled),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		CAMEL_TYPE_SERVICE);

	signals[SERVICE_DISABLED] = g_signal_new (
		"service-disabled",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, service_disabled),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		CAMEL_TYPE_SERVICE);

	signals[SERVICES_REORDERED] = g_signal_new (
		"services-reordered",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, services_reordered),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOOLEAN,
		G_TYPE_NONE, 1,
		G_TYPE_BOOLEAN);

	signals[REMOVE_REQUESTED] = g_signal_new (
		"remove-requested",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, remove_requested),
		mail_account_store_true_proceed, NULL,
		e_marshal_BOOLEAN__OBJECT_OBJECT,
		G_TYPE_BOOLEAN, 2,
		GTK_TYPE_WINDOW,
		CAMEL_TYPE_SERVICE);

	signals[ENABLE_REQUESTED] = g_signal_new (
		"enable-requested",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, enable_requested),
		mail_account_store_true_proceed, NULL,
		e_marshal_BOOLEAN__OBJECT_OBJECT,
		G_TYPE_BOOLEAN, 2,
		GTK_TYPE_WINDOW,
		CAMEL_TYPE_SERVICE);

	signals[DISABLE_REQUESTED] = g_signal_new (
		"disable-requested",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, disable_requested),
		mail_account_store_true_proceed, NULL,
		e_marshal_BOOLEAN__OBJECT_OBJECT,
		G_TYPE_BOOLEAN, 2,
		GTK_TYPE_WINDOW,
		CAMEL_TYPE_SERVICE);
}

static void
e_mail_account_store_interface_init (GtkTreeModelIface *iface)
{
	iface->row_changed = mail_account_store_row_changed;
	iface->row_inserted = mail_account_store_row_inserted;
}

static void
e_mail_account_store_init (EMailAccountStore *store)
{
	GType types[E_MAIL_ACCOUNT_STORE_NUM_COLUMNS];
	GHashTable *service_index;
	gint ii = 0;

	service_index = g_hash_table_new_full (
		(GHashFunc) g_direct_hash,
		(GEqualFunc) g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) index_item_free);

	store->priv = e_mail_account_store_get_instance_private (store);
	store->priv->service_index = service_index;

	types[ii++] = CAMEL_TYPE_SERVICE;	/* COLUMN_SERVICE */
	types[ii++] = G_TYPE_BOOLEAN;		/* COLUMN_BUILTIN */
	types[ii++] = G_TYPE_BOOLEAN;		/* COLUMN_ENABLED */
	types[ii++] = G_TYPE_BOOLEAN;		/* COLUMN_DEFAULT */
	types[ii++] = G_TYPE_STRING;		/* COLUMN_BACKEND_NAME */
	types[ii++] = G_TYPE_STRING;		/* COLUMN_DISPLAY_NAME */
	types[ii++] = G_TYPE_STRING;		/* COLUMN_ICON_NAME */
	types[ii++] = G_TYPE_BOOLEAN;		/* COLUMN_ONLINE_ACCOUNT */
	types[ii++] = G_TYPE_BOOLEAN;		/* COLUMN_ENABLED_VISIBLE */

	g_return_if_fail (ii == E_MAIL_ACCOUNT_STORE_NUM_COLUMNS);

	gtk_list_store_set_column_types (
		GTK_LIST_STORE (store),
		G_N_ELEMENTS (types), types);
}

EMailAccountStore *
e_mail_account_store_new (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		E_TYPE_MAIL_ACCOUNT_STORE,
		"session", session, NULL);
}

void
e_mail_account_store_clear (EMailAccountStore *store)
{
	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));

	gtk_list_store_clear (GTK_LIST_STORE (store));
	g_hash_table_remove_all (store->priv->service_index);
}

gboolean
e_mail_account_store_get_busy (EMailAccountStore *store)
{
	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), FALSE);

	return (store->priv->busy_count > 0);
}

EMailSession *
e_mail_account_store_get_session (EMailAccountStore *store)
{
	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), NULL);

	return E_MAIL_SESSION (store->priv->session);
}

CamelService *
e_mail_account_store_get_default_service (EMailAccountStore *store)
{
	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), NULL);

	return store->priv->default_service;
}

void
e_mail_account_store_set_default_service (EMailAccountStore *store,
                                          CamelService *service)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean iter_set;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));

	if (service == store->priv->default_service)
		return;

	if (service != NULL) {
		g_return_if_fail (CAMEL_IS_SERVICE (service));
		g_object_ref (service);
	}

	if (store->priv->default_service != NULL)
		g_object_unref (store->priv->default_service);

	store->priv->default_service = service;

	model = GTK_TREE_MODEL (store);
	iter_set = gtk_tree_model_get_iter_first (model, &iter);

	while (iter_set) {
		CamelService *candidate;

		gtk_tree_model_get (
			model, &iter,
			E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE,
			&candidate, -1);

		gtk_list_store_set (
			GTK_LIST_STORE (model), &iter,
			E_MAIL_ACCOUNT_STORE_COLUMN_DEFAULT,
			service == candidate, -1);

		g_object_unref (candidate);

		iter_set = gtk_tree_model_iter_next (model, &iter);
	}

	g_object_notify (G_OBJECT (store), "default-service");
}

void
e_mail_account_store_add_service (EMailAccountStore *store,
                                  CamelService *service)
{
	EMailSession *session;
	ESourceRegistry *registry;
	ESource *collection;
	ESource *source;
	GtkTreeIter iter, sibling;
	const gchar *icon_name = NULL;
	const gchar *uid;
	gint intended_position;
	gboolean builtin = FALSE;
	gboolean enabled;
	gboolean online_account = FALSE;
	gboolean enabled_visible = TRUE;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	/* Avoid duplicate services in the account store. */
	if (mail_account_store_get_iter (store, service, &iter))
		g_return_if_reached ();

	uid = camel_service_get_uid (service);

	if (CAMEL_IS_STORE (service))
		builtin = (camel_store_get_flags (CAMEL_STORE (service)) & CAMEL_STORE_IS_BUILTIN) != 0;

	builtin = builtin ||
		(g_strcmp0 (uid, E_MAIL_SESSION_LOCAL_UID) == 0) ||
		(g_strcmp0 (uid, E_MAIL_SESSION_VFOLDER_UID) == 0);

	session = e_mail_account_store_get_session (store);

	registry = e_mail_session_get_registry (session);
	source = e_source_registry_ref_source (registry, uid);
	g_return_if_fail (source != NULL);

	/* If this ESource is part of a collection, we need to
	 * pick up the enabled state for the entire collection.
	 * Check the ESource and its ancestors for a collection
	 * extension and read from the containing source. */
	collection = e_source_registry_find_extension (
		registry, source, E_SOURCE_EXTENSION_COLLECTION);
	if (collection != NULL) {
		const gchar *extension_name;

		enabled = e_source_get_enabled (collection);

		/* Check for GNOME Online Accounts linkage. */
		extension_name = E_SOURCE_EXTENSION_GOA;
		if (e_source_has_extension (collection, extension_name)) {
			online_account = TRUE;
			enabled_visible = FALSE;

			/* Provided by gnome-control-center-data. */
			icon_name = "goa-panel";
		}

		/* Check for Ubuntu Online Accounts linkage. */
		extension_name = E_SOURCE_EXTENSION_UOA;
		if (e_source_has_extension (collection, extension_name)) {
			online_account = TRUE;
			enabled_visible = FALSE;

			/* Provided by gnome-control-center-signon. */
			icon_name = "credentials-preferences";
		}

		g_object_unref (collection);
	} else {
		enabled = e_source_get_enabled (source);
	}

	g_object_unref (source);

	intended_position = mail_account_store_get_defailt_index (store, service);
	if (intended_position >= 0 &&
	    gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &sibling, NULL, intended_position))
		gtk_list_store_insert_before (GTK_LIST_STORE (store), &iter, &sibling);
	else
		gtk_list_store_prepend (GTK_LIST_STORE (store), &iter);

	gtk_list_store_set (
		GTK_LIST_STORE (store), &iter,
		E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE, service,
		E_MAIL_ACCOUNT_STORE_COLUMN_BUILTIN, builtin,
		E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED, enabled,
		E_MAIL_ACCOUNT_STORE_COLUMN_ICON_NAME, icon_name,
		E_MAIL_ACCOUNT_STORE_COLUMN_ONLINE_ACCOUNT, online_account,
		E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED_VISIBLE, enabled_visible,
		-1);

	/* This populates the rest of the columns. */
	mail_account_store_update_row (store, service, &iter);

	/* No need to connect to "service-added" emissions since it's
	 * always immediately followed by either "service-enabled" or
	 * "service-disabled" in MailFolderCache */

	g_signal_emit (store, signals[SERVICE_ADDED], 0, service);

	if (enabled)
		g_signal_emit (store, signals[SERVICE_ENABLED], 0, service);
	else
		g_signal_emit (store, signals[SERVICE_DISABLED], 0, service);
}

void
e_mail_account_store_remove_service (EMailAccountStore *store,
                                     GtkWindow *parent_window,
                                     CamelService *service)
{
	GtkTreeIter iter;
	gboolean proceed = TRUE;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	/* XXX Our service_removed() class method calls e_source_remove(),
	 *     which causes the registry service to emit a "source-removed"
	 *     signal.  But since other applications may also induce signal
	 *     emissions from the registry service, EMailUISession handles
	 *     "source-removed" by calling this function.  So quietly break
	 *     the cycle if we don't find the service in our tree model. */
	if (!mail_account_store_get_iter (store, service, &iter))
		return;

	/* If no parent window was given, skip the request signal. */
	if (GTK_IS_WINDOW (parent_window))
		g_signal_emit (
			store, signals[REMOVE_REQUESTED], 0,
			parent_window, service, &proceed);

	if (proceed) {
		g_object_ref (service);

		gtk_list_store_remove (GTK_LIST_STORE (store), &iter);

		mail_account_store_clean_index (store);

		g_signal_emit (store, signals[SERVICE_REMOVED], 0, service);

		g_object_unref (service);
	}
}

void
e_mail_account_store_enable_service (EMailAccountStore *store,
                                     GtkWindow *parent_window,
                                     CamelService *service)
{
	GtkTreeIter iter;
	gboolean proceed = FALSE;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	if (!mail_account_store_get_iter (store, service, &iter))
		g_return_if_reached ();

	/* If no parent window was given, skip the request signal. */
	if (GTK_IS_WINDOW (parent_window))
		g_signal_emit (
			store, signals[ENABLE_REQUESTED], 0,
			parent_window, service, &proceed);

	if (proceed) {
		gtk_list_store_set (
			GTK_LIST_STORE (store), &iter,
			E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED, TRUE, -1);
		g_signal_emit (store, signals[SERVICE_ENABLED], 0, service);
	}
}

void
e_mail_account_store_disable_service (EMailAccountStore *store,
                                      GtkWindow *parent_window,
                                      CamelService *service)
{
	GtkTreeIter iter;
	gboolean proceed = FALSE;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	if (!mail_account_store_get_iter (store, service, &iter))
		g_return_if_reached ();

	/* If no parent window was given, skip the request signal. */
	if (GTK_IS_WINDOW (parent_window))
		g_signal_emit (
			store, signals[DISABLE_REQUESTED], 0,
			parent_window, service, &proceed);

	if (proceed) {
		gtk_list_store_set (
			GTK_LIST_STORE (store), &iter,
			E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED, FALSE, -1);
		g_signal_emit (store, signals[SERVICE_DISABLED], 0, service);
	}
}

void
e_mail_account_store_queue_services (EMailAccountStore *store,
                                     GQueue *out_queue)
{
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean iter_set;
	gint column;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));
	g_return_if_fail (out_queue != NULL);

	tree_model = GTK_TREE_MODEL (store);

	iter_set = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (iter_set) {
		GValue value = G_VALUE_INIT;

		column = E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE;
		gtk_tree_model_get_value (tree_model, &iter, column, &value);
		g_queue_push_tail (out_queue, g_value_get_object (&value));
		g_value_unset (&value);

		iter_set = gtk_tree_model_iter_next (tree_model, &iter);
	}
}

void
e_mail_account_store_queue_enabled_services (EMailAccountStore *store,
                                             GQueue *out_queue)
{
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean iter_set;
	gint column;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));
	g_return_if_fail (out_queue != NULL);

	tree_model = GTK_TREE_MODEL (store);

	iter_set = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (iter_set) {
		GValue value = G_VALUE_INIT;
		gboolean enabled;

		column = E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED;
		gtk_tree_model_get_value (tree_model, &iter, column, &value);
		enabled = g_value_get_boolean (&value);
		g_value_unset (&value);

		if (enabled) {
			column = E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE;
			gtk_tree_model_get_value (tree_model, &iter, column, &value);
			g_queue_push_tail (out_queue, g_value_get_object (&value));
			g_value_unset (&value);
		}

		iter_set = gtk_tree_model_iter_next (tree_model, &iter);
	}
}

gboolean
e_mail_account_store_have_enabled_service (EMailAccountStore *store,
                                           GType service_type)
{
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean iter_set;
	gint column;
	gboolean found = FALSE;

	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), FALSE);

	tree_model = GTK_TREE_MODEL (store);

	iter_set = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (iter_set && !found) {
		GValue value = G_VALUE_INIT;
		gboolean enabled;

		column = E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED;
		gtk_tree_model_get_value (tree_model, &iter, column, &value);
		enabled = g_value_get_boolean (&value);
		g_value_unset (&value);

		if (enabled) {
			CamelService *service;

			column = E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE;
			gtk_tree_model_get_value (tree_model, &iter, column, &value);
			service = g_value_get_object (&value);
			found = service && G_TYPE_CHECK_INSTANCE_TYPE (service, service_type);
			g_value_unset (&value);
		}

		iter_set = gtk_tree_model_iter_next (tree_model, &iter);
	}

	return found;
}

static GQueue *
mail_account_store_ensure_all_services_in_queue (GQueue *current_order,
						 GQueue *ordered_services)
{
	GHashTable *known_services;
	GHashTableIter iter;
	gpointer key, value;
	GQueue *use_order;
	GList *link;

	g_return_val_if_fail (current_order != NULL, NULL);
	g_return_val_if_fail (ordered_services != NULL, NULL);

	known_services = g_hash_table_new (g_str_hash, g_str_equal);

	for (link = g_queue_peek_head_link (current_order); link != NULL; link = g_list_next (link)) {
		CamelService *service = link->data;

		if (!service)
			continue;

		g_hash_table_insert (known_services, (gpointer) camel_service_get_uid (service), service);
	}

	use_order = g_queue_new ();

	for (link = g_queue_peek_head_link (ordered_services); link != NULL; link = g_list_next (link)) {
		CamelService *service = link->data, *found;

		if (!service)
			continue;

		found = g_hash_table_lookup (known_services, camel_service_get_uid (service));
		if (found) {
			g_hash_table_remove (known_services, camel_service_get_uid (found));
			g_queue_push_tail (use_order, found);
		}
	}

	g_hash_table_iter_init (&iter, known_services);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		g_queue_insert_sorted (use_order, value, (GCompareDataFunc)
			mail_account_store_default_compare, NULL);
	}

	g_hash_table_destroy (known_services);

	return use_order;
}

void
e_mail_account_store_reorder_services (EMailAccountStore *store,
                                       GQueue *ordered_services)
{
	GQueue *current_order = NULL;
	GQueue *default_order = NULL;
	GtkTreeModel *tree_model;
	gboolean use_default_order;
	GList *head, *link;
	gint *new_order;
	gint n_children;
	gint new_pos = 0;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));

	tree_model = GTK_TREE_MODEL (store);
	n_children = gtk_tree_model_iter_n_children (tree_model, NULL);

	/* Treat NULL queues and empty queues the same. */
	if (ordered_services != NULL && g_queue_is_empty (ordered_services))
		ordered_services = NULL;

	use_default_order = (ordered_services == NULL);

	/* Build a queue of CamelServices in the order they appear in
	 * the list store.  We'll use this to construct the mapping to
	 * pass to gtk_list_store_reorder(). */
	current_order = g_queue_new ();
	e_mail_account_store_queue_services (store, current_order);

	/* If a custom ordering was not given, revert to default. */
	if (use_default_order) {
		default_order = g_queue_copy (current_order);

		g_queue_sort (
			default_order, (GCompareDataFunc)
			mail_account_store_default_compare, NULL);

		ordered_services = default_order;
	} else {
		default_order = mail_account_store_ensure_all_services_in_queue (current_order, ordered_services);

		ordered_services = default_order;
	}

	new_order = g_new0 (gint, n_children);
	head = g_queue_peek_head_link (ordered_services);

	for (link = head; link != NULL; link = g_list_next (link)) {
		GList *matching_link;
		gint old_pos;

		matching_link = g_queue_find (current_order, link->data);

		if (matching_link == NULL || matching_link->data == NULL)
			break;

		old_pos = g_queue_link_index (current_order, matching_link);

		matching_link->data = NULL;
		if (new_pos < n_children)
			new_order[new_pos++] = old_pos;
	}

	if (new_pos == n_children) {
		gtk_list_store_reorder (GTK_LIST_STORE (store), new_order);
		g_signal_emit (
			store, signals[SERVICES_REORDERED], 0,
			use_default_order);
	} else {
		g_warn_if_reached ();
	}

	g_free (new_order);

	if (current_order != NULL)
		g_queue_free (current_order);

	if (default_order != NULL)
		g_queue_free (default_order);
}

gint
e_mail_account_store_compare_services (EMailAccountStore *store,
                                       CamelService *service_a,
                                       CamelService *service_b)
{
	GtkTreeModel *model;
	GtkTreePath *path_a;
	GtkTreePath *path_b;
	GtkTreeIter iter_a;
	GtkTreeIter iter_b;
	gboolean iter_a_set;
	gboolean iter_b_set;
	gint result;

	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), -1);
	g_return_val_if_fail (CAMEL_IS_SERVICE (service_a), -1);
	g_return_val_if_fail (CAMEL_IS_SERVICE (service_b), -1);

	/* XXX This is horribly inefficient but should be
	 *     over a small enough set to not be noticable. */

	iter_a_set = mail_account_store_get_iter (store, service_a, &iter_a);
	iter_b_set = mail_account_store_get_iter (store, service_b, &iter_b);

	if (!iter_a_set && !iter_b_set)
		return 0;

	if (!iter_a_set)
		return -1;

	if (!iter_b_set)
		return 1;

	model = GTK_TREE_MODEL (store);

	path_a = gtk_tree_model_get_path (model, &iter_a);
	path_b = gtk_tree_model_get_path (model, &iter_b);

	result = gtk_tree_path_compare (path_a, path_b);

	gtk_tree_path_free (path_a);
	gtk_tree_path_free (path_b);

	return result;
}

gboolean
e_mail_account_store_load_sort_order (EMailAccountStore *store,
                                      GError **error)
{
	GQueue service_queue = G_QUEUE_INIT;
	EMailSession *session;
	GKeyFile *key_file;
	const gchar *filename;
	gchar **service_uids;
	gboolean success = TRUE;
	gsize ii, length;

	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), FALSE);

	session = e_mail_account_store_get_session (store);

	key_file = g_key_file_new ();
	filename = store->priv->sort_order_filename;

	if (g_file_test (filename, G_FILE_TEST_EXISTS))
		success = g_key_file_load_from_file (
			key_file, filename, G_KEY_FILE_NONE, error);

	if (!success) {
		g_key_file_free (key_file);
		return FALSE;
	}

	/* If the key is not present, length is set to zero. */
	service_uids = g_key_file_get_string_list (
		key_file, "Accounts", "SortOrder", &length, NULL);

	for (ii = 0; ii < length; ii++) {
		CamelService *service;

		service = camel_session_ref_service (
			CAMEL_SESSION (session), service_uids[ii]);
		if (service != NULL)
			g_queue_push_tail (&service_queue, service);
	}

	e_mail_account_store_reorder_services (store, &service_queue);

	while (!g_queue_is_empty (&service_queue))
		g_object_unref (g_queue_pop_head (&service_queue));

	g_strfreev (service_uids);

	g_key_file_free (key_file);

	return TRUE;
}

gboolean
e_mail_account_store_save_sort_order (EMailAccountStore *store,
                                      GError **error)
{
	GKeyFile *key_file;
	GtkTreeModel *model;
	GtkTreeIter iter;
	const gchar **service_uids;
	const gchar *filename;
	gchar *contents;
	gboolean iter_set;
	gboolean success;
	gsize length;
	gsize ii = 0;

	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), FALSE);

	model = GTK_TREE_MODEL (store);
	length = gtk_tree_model_iter_n_children (model, NULL);

	/* Empty store, nothing to save. */
	if (length == 0)
		return TRUE;

	service_uids = g_new0 (const gchar *, length);

	iter_set = gtk_tree_model_get_iter_first (model, &iter);

	while (iter_set) {
		GValue value = G_VALUE_INIT;
		const gint column = E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE;
		CamelService *service;

		gtk_tree_model_get_value (model, &iter, column, &value);
		service = g_value_get_object (&value);
		service_uids[ii++] = camel_service_get_uid (service);
		g_value_unset (&value);

		iter_set = gtk_tree_model_iter_next (model, &iter);
	}

	key_file = g_key_file_new ();
	filename = store->priv->sort_order_filename;

	g_key_file_set_string_list (
		key_file, "Accounts", "SortOrder", service_uids, length);

	contents = g_key_file_to_data (key_file, &length, NULL);
	success = g_file_set_contents (filename, contents, length, error);
	g_free (contents);

	g_key_file_free (key_file);

	g_free (service_uids);

	return success;
}

