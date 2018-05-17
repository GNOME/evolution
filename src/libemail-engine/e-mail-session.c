/*
 * e-mail-session.c
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
 *
 * Authors:
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 *
 */

/* mail-session.c: handles the session information and resource manipulation */

#include "evolution-config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#ifdef HAVE_CANBERRA
#include <canberra-gtk.h>
#endif

#include <libebackend/libebackend.h>
#include <libedataserver/libedataserver.h>

#include "libemail-engine/mail-mt.h"

/* This is our hack, not part of libcamel. */
#include "camel-null-store.h"

#include "e-mail-session.h"
#include "e-mail-folder-utils.h"
#include "e-mail-utils.h"
#include "mail-config.h"
#include "mail-ops.h"
#include "mail-tools.h"

#define E_MAIL_SESSION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SESSION, EMailSessionPrivate))

typedef struct _AsyncContext AsyncContext;
typedef struct _ServiceProxyData ServiceProxyData;

struct _EMailSessionPrivate {
	MailFolderCache *folder_cache;
	ESourceRegistry *registry;

	/* ESource UID -> Timeout ID */
	GHashTable *auto_refresh_table;

	gulong source_added_handler_id;
	gulong source_removed_handler_id;
	gulong source_enabled_handler_id;
	gulong source_disabled_handler_id;
	gulong default_mail_account_handler_id;
	gulong outbox_changed_handler_id;

	CamelService *local_store;
	CamelService *vfolder_store;

	FILE *filter_logfile;
	GHashTable *junk_filters;

	/* Local folder cache. */
	GPtrArray *local_folders;
	GPtrArray *local_folder_uris;

	guint preparing_flush;
	guint outbox_flush_id;
	GMutex preparing_flush_lock;

	GMutex used_services_lock;
	GCond used_services_cond;
	GHashTable *used_services;
};

struct _AsyncContext {
	/* arguments */
	CamelStoreGetFolderFlags flags;
	gchar *uid;
	gchar *uri;

	/* results */
	CamelFolder *folder;
};

struct _ServiceProxyData {
	ESource *authentication_source;
	gulong auth_source_changed_handler_id;
};

enum {
	PROP_0,
	PROP_FOLDER_CACHE,
	PROP_LOCAL_STORE,
	PROP_REGISTRY,
	PROP_VFOLDER_STORE
};

static const gchar *local_folder_names[E_MAIL_NUM_LOCAL_FOLDERS] = {
	N_("Inbox"),		/* E_MAIL_LOCAL_FOLDER_INBOX */
	N_("Drafts"),		/* E_MAIL_LOCAL_FOLDER_DRAFTS */
	N_("Outbox"),		/* E_MAIL_LOCAL_FOLDER_OUTBOX */
	N_("Sent"),		/* E_MAIL_LOCAL_FOLDER_SENT */
	N_("Templates"),	/* E_MAIL_LOCAL_FOLDER_TEMPLATES */
	"Inbox"			/* E_MAIL_LOCAL_FOLDER_LOCAL_INBOX */
};

enum {
	FLUSH_OUTBOX,
	REFRESH_SERVICE,
	STORE_ADDED,
	STORE_REMOVED,
	ALLOW_AUTH_PROMPT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static gchar *mail_data_dir;
static gchar *mail_cache_dir;
static gchar *mail_config_dir;

G_DEFINE_TYPE_WITH_CODE (
	EMailSession,
	e_mail_session,
	CAMEL_TYPE_SESSION,
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static gboolean
session_forward_to_flush_outbox_cb (gpointer user_data)
{
	EMailSession *session = E_MAIL_SESSION (user_data);

	g_mutex_lock (&session->priv->preparing_flush_lock);
	session->priv->preparing_flush = 0;
	g_mutex_unlock (&session->priv->preparing_flush_lock);

	e_mail_session_flush_outbox (session);

	return FALSE;
}

static void
async_context_free (AsyncContext *context)
{
	if (context->folder != NULL)
		g_object_unref (context->folder);

	g_free (context->uid);
	g_free (context->uri);

	g_slice_free (AsyncContext, context);
}

static void
service_proxy_data_free (ServiceProxyData *proxy_data)
{
	g_signal_handler_disconnect (
		proxy_data->authentication_source,
		proxy_data->auth_source_changed_handler_id);

	g_clear_object (&proxy_data->authentication_source);

	g_slice_free (ServiceProxyData, proxy_data);
}

static void
mail_session_update_proxy_resolver (CamelService *service,
                                    ESource *authentication_source)
{
	GProxyResolver *proxy_resolver = NULL;
	ESourceAuthentication *extension;
	CamelSession *session;
	ESource *source = NULL;
	gchar *uid;

	session = camel_service_ref_session (service);

	extension = e_source_get_extension (
		authentication_source,
		E_SOURCE_EXTENSION_AUTHENTICATION);

	uid = e_source_authentication_dup_proxy_uid (extension);
	if (uid != NULL) {
		ESourceRegistry *registry;
		EMailSession *mail_session;

		mail_session = E_MAIL_SESSION (session);
		registry = e_mail_session_get_registry (mail_session);
		source = e_source_registry_ref_source (registry, uid);
		g_free (uid);
	}

	if (source != NULL) {
		proxy_resolver = G_PROXY_RESOLVER (source);
		if (!g_proxy_resolver_is_supported (proxy_resolver))
			proxy_resolver = NULL;
	}

	camel_service_set_proxy_resolver (service, proxy_resolver);

	g_clear_object (&session);
	g_clear_object (&source);
}

static void
mail_session_auth_source_changed_cb (ESource *authentication_source,
                                     GWeakRef *service_weak_ref)
{
	CamelService *service;

	service = g_weak_ref_get (service_weak_ref);

	if (service != NULL) {
		mail_session_update_proxy_resolver (
			service, authentication_source);
		g_object_unref (service);
	}
}

static void
mail_session_configure_proxy_resolver (ESourceRegistry *registry,
                                       CamelService *service)
{
	ESource *source;
	ESource *authentication_source;
	const gchar *uid;

	uid = camel_service_get_uid (service);
	source = e_source_registry_ref_source (registry, uid);
	g_return_if_fail (source != NULL);

	authentication_source =
		e_source_registry_find_extension (
		registry, source, E_SOURCE_EXTENSION_AUTHENTICATION);

	if (authentication_source != NULL) {
		ServiceProxyData *proxy_data;
		gulong handler_id;

		mail_session_update_proxy_resolver (
			service, authentication_source);

		handler_id = g_signal_connect_data (
			authentication_source, "changed",
			G_CALLBACK (mail_session_auth_source_changed_cb),
			e_weak_ref_new (service),
			(GClosureNotify) e_weak_ref_free, 0);

		/* This takes ownership of the authentication source. */
		proxy_data = g_slice_new0 (ServiceProxyData);
		proxy_data->authentication_source = authentication_source;
		proxy_data->auth_source_changed_handler_id = handler_id;

		/* Tack the proxy data on to the CamelService,
		 * so it's destroyed along with the CamelService. */
		g_object_set_data_full (
			G_OBJECT (service),
			"proxy-data", proxy_data,
			(GDestroyNotify) service_proxy_data_free);
	}

	g_object_unref (source);
}

static gchar *
mail_session_resolve_popb4smtp (ESourceRegistry *registry,
                                CamelService *smtp_service)
{
	GList *list, *link;
	const gchar *extension_name;
	const gchar *smtp_uid;
	gchar *pop_uid = NULL;

	/* Find a POP account that uses the given smtp_service as its
	 * transport.  XXX This isn't foolproof though, since we don't
	 * check that the POP server is at the same domain as the SMTP
	 * server, which is kind of the point of POPB4SMTP. */

	smtp_uid = camel_service_get_uid (smtp_service);
	g_return_val_if_fail (smtp_uid != NULL, NULL);

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESourceExtension *extension;
		const gchar *backend_name;
		gchar *uid;

		extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
		extension = e_source_get_extension (source, extension_name);

		/* We're only interested in POP accounts. */

		backend_name = e_source_backend_get_backend_name (
			E_SOURCE_BACKEND (extension));
		if (g_strcmp0 (backend_name, "pop") != 0)
			continue;

		/* Get the mail account's default mail identity. */

		uid = e_source_mail_account_dup_identity_uid (
			E_SOURCE_MAIL_ACCOUNT (extension));
		source = e_source_registry_ref_source (registry, uid);
		g_free (uid);

		if (source == NULL)
			continue;

		/* Get the mail identity's default mail transport. */

		extension_name = E_SOURCE_EXTENSION_MAIL_SUBMISSION;
		extension = e_source_get_extension (source, extension_name);

		uid = e_source_mail_submission_dup_transport_uid (
			E_SOURCE_MAIL_SUBMISSION (extension));

		g_object_unref (source);

		if (g_strcmp0 (uid, smtp_uid) == 0) {
			pop_uid = uid;
			break;
		}

		g_free (uid);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	return pop_uid;
}

static void
mail_session_refresh_cb (ESource *source,
                         EMailSession *session)
{
	ESourceRegistry *registry;
	CamelService *service;
	const gchar *uid;

	registry = e_mail_session_get_registry (session);

	/* Skip the signal emission if the source
	 * or any of its ancestors are disabled. */
	if (!e_source_registry_check_enabled (registry, source))
		return;

	uid = e_source_get_uid (source);
	service = camel_session_ref_service (CAMEL_SESSION (session), uid);
	g_return_if_fail (service != NULL);

	g_signal_emit (session, signals[REFRESH_SERVICE], 0, service);

	g_object_unref (service);
}

static gboolean
mail_session_check_goa_mail_disabled (EMailSession *session,
                                      ESource *source)
{
	ESource *goa_source;
	ESourceRegistry *registry;
	gboolean goa_mail_disabled = FALSE;

	registry = e_mail_session_get_registry (session);

	goa_source = e_source_registry_find_extension (
		registry, source, E_SOURCE_EXTENSION_GOA);

	if (goa_source != NULL) {
		goa_mail_disabled = !e_source_get_enabled (source);
		g_object_unref (goa_source);
	}

	return goa_mail_disabled;
}

static gboolean
mail_session_check_uoa_mail_disabled (EMailSession *session,
                                      ESource *source)
{
	ESource *uoa_source;
	ESourceRegistry *registry;
	gboolean uoa_mail_disabled = FALSE;

	registry = e_mail_session_get_registry (session);

	uoa_source = e_source_registry_find_extension (
		registry, source, E_SOURCE_EXTENSION_UOA);

	if (uoa_source != NULL) {
		uoa_mail_disabled = !e_source_get_enabled (source);
		g_object_unref (uoa_source);
	}

	return uoa_mail_disabled;
}

static void
mail_session_add_from_source (EMailSession *session,
                              CamelProviderType type,
                              ESource *source)
{
	ESourceBackend *extension;
	CamelService *service;
	const gchar *uid;
	const gchar *backend_name;
	const gchar *display_name;
	const gchar *extension_name;
	GError *error = NULL;

	switch (type) {
		case CAMEL_PROVIDER_STORE:
			extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
			break;
		case CAMEL_PROVIDER_TRANSPORT:
			extension_name = E_SOURCE_EXTENSION_MAIL_TRANSPORT;
			break;
		default:
			g_return_if_reached ();
	}

	uid = e_source_get_uid (source);
	display_name = e_source_get_display_name (source);

	extension = e_source_get_extension (source, extension_name);
	backend_name = e_source_backend_get_backend_name (extension);

	/* Sanity checks. */
	g_return_if_fail (uid != NULL);
	g_return_if_fail (backend_name != NULL);

	/* Collection sources with a [GNOME Online Accounts] extension
	 * require special handling.  If the collection's mail-enabled
	 * flag is FALSE, do not add a CamelService.  The account must
	 * not appear anywhere, not even in the Mail Accounts list. */
	if (mail_session_check_goa_mail_disabled (session, source))
		return;

	/* Same deal for the [Ubuntu Online Accounts] extension. */
	if (mail_session_check_uoa_mail_disabled (session, source))
		return;

	service = camel_session_add_service (
		CAMEL_SESSION (session), uid,
		backend_name, type, &error);

	/* Our own CamelSession.add_service() method will handle the
	 * new CamelService, so we only need to unreference it here. */
	if (service != NULL)
		g_object_unref (service);

	if (error != NULL) {
		g_warning (
			"Failed to add service '%s' (%s): %s",
			display_name, uid, error->message);
		g_error_free (error);
	}

	/* Set up auto-refresh. */
	extension_name = E_SOURCE_EXTENSION_REFRESH;
	if (e_source_has_extension (source, extension_name)) {
		guint timeout_id;

		/* Transports should not have a refresh extension. */
		g_warn_if_fail (type != CAMEL_PROVIDER_TRANSPORT);

		timeout_id = e_source_refresh_add_timeout (
			source, NULL, (ESourceRefreshFunc)
			mail_session_refresh_cb, session,
			(GDestroyNotify) NULL);

		g_hash_table_insert (
			session->priv->auto_refresh_table,
			g_strdup (uid),
			GUINT_TO_POINTER (timeout_id));
	}
}

static void
mail_session_source_added_cb (ESourceRegistry *registry,
                              ESource *source,
                              EMailSession *session)
{
	CamelProviderType provider_type;
	const gchar *extension_name;

	provider_type = CAMEL_PROVIDER_STORE;
	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;

	if (e_source_has_extension (source, extension_name))
		mail_session_add_from_source (session, provider_type, source);

	provider_type = CAMEL_PROVIDER_TRANSPORT;
	extension_name = E_SOURCE_EXTENSION_MAIL_TRANSPORT;

	if (e_source_has_extension (source, extension_name))
		mail_session_add_from_source (session, provider_type, source);
}

static void
mail_session_source_removed_cb (ESourceRegistry *registry,
                                ESource *source,
                                EMailSession *session)
{
	CamelSession *camel_session;
	CamelService *service;
	const gchar *uid;

	camel_session = CAMEL_SESSION (session);

	uid = e_source_get_uid (source);
	service = camel_session_ref_service (camel_session, uid);

	if (service != NULL) {
		camel_session_remove_service (camel_session, service);
		g_object_unref (service);
	}
}

static void
mail_session_source_enabled_cb (ESourceRegistry *registry,
                                ESource *source,
                                EMailSession *session)
{
	ESource *goa_source;
	ESource *uoa_source;

	/* If the source is linked to a GNOME Online Account
	 * or Ubuntu Online Account, enabling the source is
	 * equivalent to adding it. */

	goa_source = e_source_registry_find_extension (
		registry, source, E_SOURCE_EXTENSION_GOA);

	uoa_source = e_source_registry_find_extension (
		registry, source, E_SOURCE_EXTENSION_UOA);

	if (goa_source != NULL || uoa_source != NULL)
		mail_session_source_added_cb (registry, source, session);

	if (goa_source != NULL)
		g_object_unref (goa_source);

	if (uoa_source != NULL)
		g_object_unref (uoa_source);
}

static void
mail_session_source_disabled_cb (ESourceRegistry *registry,
                                 ESource *source,
                                 EMailSession *session)
{
	ESource *goa_source;
	ESource *uoa_source;

	/* If the source is linked to a GNOME Online Account
	 * or Ubuntu Online Account, disabling the source is
	 * equivalent to removing it. */

	goa_source = e_source_registry_find_extension (
		registry, source, E_SOURCE_EXTENSION_GOA);

	uoa_source = e_source_registry_find_extension (
		registry, source, E_SOURCE_EXTENSION_UOA);

	if (goa_source != NULL || uoa_source != NULL)
		mail_session_source_removed_cb (registry, source, session);

	if (goa_source != NULL)
		g_object_unref (goa_source);

	if (uoa_source != NULL)
		g_object_unref (uoa_source);
}

static void
mail_session_default_mail_account_cb (ESourceRegistry *registry,
                                      GParamSpec *pspec,
                                      EMailSession *session)
{
	ESource *source;
	ESourceMailAccount *extension;
	const gchar *extension_name;
	gchar *uid;

	/* If the default mail account names a valid mail
	 * identity, make it the default mail identity. */

	/* XXX I debated whether to have ESourceRegistry do this
	 *     itself but it seems like an Evolution policy to me
	 *     right now.  I may change my mind in the future, or
	 *     decide not to do this synchronization at all. */

	source = e_source_registry_ref_default_mail_account (registry);
	g_return_if_fail (source != NULL);

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	extension = e_source_get_extension (source, extension_name);
	uid = e_source_mail_account_dup_identity_uid (extension);

	g_object_unref (source);
	source = NULL;

	if (uid != NULL) {
		source = e_source_registry_ref_source (registry, uid);
		g_free (uid);
	}

	if (source != NULL) {
		e_source_registry_set_default_mail_identity (registry, source);
		g_object_unref (source);
	}
}

static void
mail_session_outbox_folder_changed_cb (CamelFolder *folder,
				       CamelFolderChangeInfo *changes,
				       EMailSession *session)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (changes != NULL);
	g_return_if_fail (E_IS_MAIL_SESSION (session));

	if (changes->uid_added && changes->uid_added->len) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		if (g_settings_get_boolean (settings, "composer-use-outbox")) {
			gint delay_flush = g_settings_get_int (settings, "composer-delay-outbox-flush");

			if (delay_flush > 0)
				e_mail_session_schedule_outbox_flush (session, delay_flush);
		}
		g_object_unref (settings);
	}
}

static void
mail_session_configure_local_store (EMailSession *session)
{
	CamelLocalSettings *local_settings;
	CamelSession *camel_session;
	CamelSettings *settings;
	CamelService *service;
	CamelFolder *folder;
	const gchar *data_dir;
	const gchar *uid;
	gchar *path;
	gint ii;

	camel_session = CAMEL_SESSION (session);

	uid = E_MAIL_SESSION_LOCAL_UID;
	service = camel_session_ref_service (camel_session, uid);
	session->priv->local_store = service;  /* takes ownership */
	g_return_if_fail (service != NULL);

	settings = camel_service_ref_settings (service);

	data_dir = camel_session_get_user_data_dir (camel_session);
	path = g_build_filename (data_dir, E_MAIL_SESSION_LOCAL_UID, NULL);

	local_settings = CAMEL_LOCAL_SETTINGS (settings);
	camel_local_settings_set_path (local_settings, path);

	g_free (path);

	g_object_unref (settings);

	/* Shouldn't need to worry about other mail applications
	 * altering files in our local mail store. */
	g_object_set (service, "need-summary-check", FALSE, NULL);

	/* Populate the local folder cache. */
	for (ii = 0; ii < E_MAIL_NUM_LOCAL_FOLDERS; ii++) {
		gchar *folder_uri;
		const gchar *display_name;
		GError *error = NULL;

		display_name = local_folder_names[ii];

		/* XXX This blocks but should be fast. */
		if (ii == E_MAIL_LOCAL_FOLDER_LOCAL_INBOX)
			folder = camel_store_get_inbox_folder_sync (
				CAMEL_STORE (service), NULL, &error);
		else
			folder = camel_store_get_folder_sync (
				CAMEL_STORE (service), display_name,
				CAMEL_STORE_FOLDER_CREATE, NULL, &error);

		folder_uri = e_mail_folder_uri_build (
			CAMEL_STORE (service), display_name);

		/* The arrays take ownership of the items added. */
		g_ptr_array_add (session->priv->local_folders, folder);
		g_ptr_array_add (session->priv->local_folder_uris, folder_uri);

		if (error != NULL) {
			g_critical ("%s: %s", G_STRFUNC, error->message);
			g_error_free (error);
		}
	}

	folder = e_mail_session_get_local_folder (session, E_MAIL_LOCAL_FOLDER_OUTBOX);
	if (folder) {
		session->priv->outbox_changed_handler_id = g_signal_connect (folder, "changed",
			G_CALLBACK (mail_session_outbox_folder_changed_cb), session);
	}
}

static void
mail_session_configure_vfolder_store (EMailSession *session)
{
	CamelSession *camel_session;
	CamelService *service;
	const gchar *uid;

	camel_session = CAMEL_SESSION (session);

	uid = E_MAIL_SESSION_VFOLDER_UID;
	service = camel_session_ref_service (camel_session, uid);
	session->priv->vfolder_store = service;  /* takes ownership */
	g_return_if_fail (service != NULL);

	camel_service_connect_sync (service, NULL, NULL);

	/* XXX There's more configuration to do in vfolder_load_storage()
	 *     but it requires an EMailBackend, which we don't have access
	 *     to from here, so it has to be called from elsewhere.  Kinda
	 *     thinking about reworking that... */
}

static void
mail_session_force_refresh (EMailSession *session)
{
	ESourceRegistry *registry;
	GHashTableIter iter;
	GSettings *settings;
	gboolean unconditionally;
	gpointer key;

	/* Only refresh when the session is online. */
	if (!camel_session_get_online (CAMEL_SESSION (session)))
		return;

	/* FIXME EMailSession should define properties for these. */
	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	unconditionally =
		g_settings_get_boolean (settings, "send-recv-on-start") &&
		g_settings_get_boolean (settings, "send-recv-all-on-start");
	g_object_unref (settings);

	registry = e_mail_session_get_registry (session);
	g_hash_table_iter_init (&iter, session->priv->auto_refresh_table);

	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		ESource *source;
		ESourceRefresh *extension;
		const gchar *extension_name;
		gboolean refresh_enabled;

		/* The hash table key is the ESource UID. */
		source = e_source_registry_ref_source (registry, key);

		if (source == NULL)
			continue;

		extension_name = E_SOURCE_EXTENSION_REFRESH;
		extension = e_source_get_extension (source, extension_name);
		refresh_enabled = e_source_refresh_get_enabled (extension);

		if (refresh_enabled || unconditionally)
			e_source_refresh_force_timeout (source);

		g_object_unref (source);
	}
}

static void
mail_session_cancel_refresh (EMailSession *session)
{
	ESourceRegistry *registry;
	GHashTableIter iter;
	gpointer key, value;

	registry = e_mail_session_get_registry (session);
	g_hash_table_iter_init (&iter, session->priv->auto_refresh_table);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		ESource *source;
		guint timeout_id;

		/* The hash table key is the ESource UID. */
		source = e_source_registry_ref_source (registry, key);

		/* The hash table value is the refresh timeout ID. */
		timeout_id = GPOINTER_TO_UINT (value);

		if (source == NULL)
			continue;

		e_source_refresh_remove_timeout (source, timeout_id);

		g_object_unref (source);
	}

	/* All timeouts cancelled so clear the auto-refresh table. */
	g_hash_table_remove_all (session->priv->auto_refresh_table);
}

static gboolean
mail_session_idle_refresh_cb (EMailSession *session)
{
	/* This only runs once at startup (if settings allow). */

	if (camel_session_get_online (CAMEL_SESSION (session))) {
		mail_session_force_refresh (session);

		/* Also flush the Outbox. */
		g_signal_emit (session, signals[FLUSH_OUTBOX], 0);
	}

	/* Listen for network state changes and force a
	 * mail store refresh when coming back online. */
	e_signal_connect_notify (
		session, "notify::online",
		G_CALLBACK (mail_session_force_refresh), NULL);

	return FALSE;
}

static void
mail_session_set_registry (EMailSession *session,
                           ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (session->priv->registry == NULL);

	session->priv->registry = g_object_ref (registry);
}

static void
mail_session_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			mail_session_set_registry (
				E_MAIL_SESSION (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_session_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FOLDER_CACHE:
			g_value_set_object (
				value,
				e_mail_session_get_folder_cache (
				E_MAIL_SESSION (object)));
			return;

		case PROP_LOCAL_STORE:
			g_value_set_object (
				value,
				e_mail_session_get_local_store (
				E_MAIL_SESSION (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_mail_session_get_registry (
				E_MAIL_SESSION (object)));
			return;

		case PROP_VFOLDER_STORE:
			g_value_set_object (
				value,
				e_mail_session_get_vfolder_store (
				E_MAIL_SESSION (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_session_dispose (GObject *object)
{
	EMailSessionPrivate *priv;

	priv = E_MAIL_SESSION_GET_PRIVATE (object);

	if (priv->outbox_changed_handler_id) {
		CamelFolder *folder;

		folder = e_mail_session_get_local_folder (E_MAIL_SESSION (object), E_MAIL_LOCAL_FOLDER_OUTBOX);
		if (folder)
			g_signal_handler_disconnect (folder, priv->outbox_changed_handler_id);

		priv->outbox_changed_handler_id = 0;
	}

	if (priv->folder_cache != NULL) {
		g_object_unref (priv->folder_cache);
		priv->folder_cache = NULL;
	}

	g_ptr_array_set_size (priv->local_folders, 0);
	g_ptr_array_set_size (priv->local_folder_uris, 0);

	g_mutex_lock (&priv->preparing_flush_lock);

	if (priv->preparing_flush > 0) {
		g_source_remove (priv->preparing_flush);
		priv->preparing_flush = 0;
	}

	if (priv->outbox_flush_id > 0) {
		g_source_remove (priv->outbox_flush_id);
		priv->outbox_flush_id = 0;
	}

	g_mutex_unlock (&priv->preparing_flush_lock);

	if (priv->local_store != NULL) {
		g_object_unref (priv->local_store);
		priv->local_store = NULL;
	}

	if (priv->vfolder_store != NULL) {
		g_object_unref (priv->vfolder_store);
		priv->vfolder_store = NULL;
	}

	if (priv->registry != NULL) {
		g_signal_handler_disconnect (
			priv->registry,
			priv->source_added_handler_id);
		g_signal_handler_disconnect (
			priv->registry,
			priv->source_removed_handler_id);
		g_signal_handler_disconnect (
			priv->registry,
			priv->source_enabled_handler_id);
		g_signal_handler_disconnect (
			priv->registry,
			priv->source_disabled_handler_id);
		g_signal_handler_disconnect (
			priv->registry,
			priv->default_mail_account_handler_id);

		/* This requires the registry. */
		mail_session_cancel_refresh (E_MAIL_SESSION (object));

		g_object_unref (priv->registry);
		priv->registry = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_session_parent_class)->dispose (object);
}

static void
mail_session_finalize (GObject *object)
{
	EMailSessionPrivate *priv;

	priv = E_MAIL_SESSION_GET_PRIVATE (object);

	g_hash_table_destroy (priv->auto_refresh_table);
	g_hash_table_destroy (priv->junk_filters);
	g_hash_table_destroy (priv->used_services);

	g_ptr_array_free (priv->local_folders, TRUE);
	g_ptr_array_free (priv->local_folder_uris, TRUE);

	g_mutex_clear (&priv->preparing_flush_lock);
	g_mutex_clear (&priv->used_services_lock);
	g_cond_clear (&priv->used_services_cond);

	g_free (mail_data_dir);
	g_free (mail_config_dir);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_session_parent_class)->finalize (object);
}

static void
mail_session_constructed (GObject *object)
{
	EMailSession *session;
	EExtensible *extensible;
	ESourceRegistry *registry;
	GType extension_type;
	GList *list, *link;
	GSettings *settings;
	CamelProviderType provider_type;
	const gchar *extension_name;
	gulong handler_id;

	session = E_MAIL_SESSION (object);
	registry = e_mail_session_get_registry (session);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_session_parent_class)->constructed (object);

	camel_session_set_network_monitor (CAMEL_SESSION (session), e_network_monitor_get_default ());

	/* Add available mail accounts. */

	provider_type = CAMEL_PROVIDER_STORE;
	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;

	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);

		mail_session_add_from_source (session, provider_type, source);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	/* Add available mail transports. */

	provider_type = CAMEL_PROVIDER_TRANSPORT;
	extension_name = E_SOURCE_EXTENSION_MAIL_TRANSPORT;

	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);

		mail_session_add_from_source (session, provider_type, source);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	/* Built-in stores require extra configuration. */

	mail_session_configure_local_store (session);
	mail_session_configure_vfolder_store (session);

	/* Listen for registry changes. */

	handler_id = g_signal_connect (
		registry, "source-added",
		G_CALLBACK (mail_session_source_added_cb), session);
	session->priv->source_added_handler_id = handler_id;

	handler_id = g_signal_connect (
		registry, "source-removed",
		G_CALLBACK (mail_session_source_removed_cb), session);
	session->priv->source_removed_handler_id = handler_id;

	handler_id = g_signal_connect (
		registry, "source-enabled",
		G_CALLBACK (mail_session_source_enabled_cb), session);
	session->priv->source_enabled_handler_id = handler_id;

	handler_id = g_signal_connect (
		registry, "source-disabled",
		G_CALLBACK (mail_session_source_disabled_cb), session);
	session->priv->source_disabled_handler_id = handler_id;

	handler_id = e_signal_connect_notify (
		registry, "notify::default-mail-account",
		G_CALLBACK (mail_session_default_mail_account_cb), session);
	session->priv->default_mail_account_handler_id = handler_id;

	extensible = E_EXTENSIBLE (object);
	e_extensible_load_extensions (extensible);

	/* Add junk filter extensions to an internal hash table. */

	extension_type = E_TYPE_MAIL_JUNK_FILTER;
	list = e_extensible_list_extensions (extensible, extension_type);

	for (link = list; link != NULL; link = g_list_next (link)) {
		EMailJunkFilter *junk_filter;
		EMailJunkFilterClass *class;

		junk_filter = E_MAIL_JUNK_FILTER (link->data);
		class = E_MAIL_JUNK_FILTER_GET_CLASS (junk_filter);

		if (!CAMEL_IS_JUNK_FILTER (junk_filter)) {
			g_warning (
				"Skipping %s: Does not implement "
				"CamelJunkFilterInterface",
				G_OBJECT_TYPE_NAME (junk_filter));
			continue;
		}

		if (class->filter_name == NULL) {
			g_warning (
				"Skipping %s: filter_name unset",
				G_OBJECT_TYPE_NAME (junk_filter));
			continue;
		}

		if (class->display_name == NULL) {
			g_warning (
				"Skipping %s: display_name unset",
				G_OBJECT_TYPE_NAME (junk_filter));
			continue;
		}

		/* No need to reference the EMailJunkFilter since
		 * EMailSession owns the reference to it already. */
		g_hash_table_insert (
			session->priv->junk_filters,
			(gpointer) class->filter_name,
			junk_filter);
	}

	g_list_free (list);

	mail_config_reload_junk_headers (session);

	/* Initialize the legacy message-passing framework
	 * before starting the first mail store refresh. */
	mail_msg_init ();

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	/* The application is not yet fully initialized at this point,
	 * so run the first mail store refresh from an idle callback. */
	if (g_settings_get_boolean (settings, "send-recv-on-start"))
		g_idle_add_full (
			G_PRIORITY_DEFAULT,
			(GSourceFunc) mail_session_idle_refresh_cb,
			g_object_ref (session),
			(GDestroyNotify) g_object_unref);

	g_object_unref (settings);
}

static CamelService *
mail_session_add_service (CamelSession *session,
                          const gchar *uid,
                          const gchar *protocol,
                          CamelProviderType type,
                          GError **error)
{
	ESourceRegistry *registry;
	CamelService *service;
	const gchar *extension_name;

	registry = e_mail_session_get_registry (E_MAIL_SESSION (session));
	extension_name = e_source_camel_get_extension_name (protocol);

	/* Chain up to parents add_service() method. */
	service = CAMEL_SESSION_CLASS (e_mail_session_parent_class)->
		add_service (session, uid, protocol, type, error);

	/* Configure the CamelService from the corresponding ESource. */

	if (CAMEL_IS_SERVICE (service)) {
		ESource *source;
		ESource *tmp_source;

		/* Each CamelService has a corresponding ESource. */
		source = e_source_registry_ref_source (registry, uid);
		g_return_val_if_fail (source != NULL, service);

		tmp_source = e_source_registry_find_extension (
			registry, source, extension_name);
		if (tmp_source != NULL) {
			g_object_unref (source);
			source = tmp_source;
		}

		/* This handles all the messy property bindings. */
		e_source_camel_configure_service (source, service);

		/* Track the proxy resolver for this service. */
		mail_session_configure_proxy_resolver (registry, service);

		e_binding_bind_property (
			source, "display-name",
			service, "display-name",
			G_BINDING_SYNC_CREATE);

		/* Migrate files for this service from its old
		 * URL-based directory to a UID-based directory
		 * if necessary. */
		camel_service_migrate_files (service);
	}

	return service;
}

static gchar *
mail_session_get_password (CamelSession *session,
                           CamelService *service,
                           const gchar *prompt,
                           const gchar *item,
                           guint32 flags,
                           GError **error)
{
	ESourceRegistry *registry;
	gchar *password = NULL;

	/* XXX This method is now only for fringe cases.  For normal
	 *     CamelService authentication, use authenticate_sync().
	 *
	 *     The two known fringe cases that still need this are:
	 *
	 *     1) CamelSaslPOPB4SMTP, where the CamelService is an SMTP
	 *        transport and the item name is always "popb4smtp_uid".
	 *        (This is a dirty hack, Camel just needs some way to
	 *        pair up a CamelService and CamelTransport.  Not sure
	 *        what that should look like just yet...)
	 *
	 *     2) CamelGpgContext, where the CamelService is NULL and
	 *        the item name is a user ID (I think).  (Seahorse, or
	 *        one of its dependent libraries, ought to handle this
	 *        transparently once Camel fully transitions to GIO.)
	 */

	registry = e_mail_session_get_registry (E_MAIL_SESSION (session));

	/* Handle the CamelSaslPOPB4SMTP case. */
	if (g_strcmp0 (item, "popb4smtp_uid") == 0)
		return mail_session_resolve_popb4smtp (registry, service);

	/* Otherwise this had better be the CamelGpgContext case. */
	g_return_val_if_fail (service == NULL, NULL);

	password = e_passwords_get_password (item);

	if (password == NULL || (flags & CAMEL_SESSION_PASSWORD_REPROMPT)) {
		gboolean remember;
		guint eflags = 0;

		if (flags & CAMEL_SESSION_PASSWORD_STATIC)
			eflags |= E_PASSWORDS_REMEMBER_NEVER;
		else
			eflags |= E_PASSWORDS_REMEMBER_SESSION;

		if (flags & CAMEL_SESSION_PASSWORD_REPROMPT)
			eflags |= E_PASSWORDS_REPROMPT;

		if (flags & CAMEL_SESSION_PASSWORD_SECRET)
			eflags |= E_PASSWORDS_SECRET;

		if (flags & CAMEL_SESSION_PASSPHRASE)
			eflags |= E_PASSWORDS_PASSPHRASE;

		password = e_passwords_ask_password (
			"", item, prompt, eflags, &remember, NULL);

		if (password == NULL)
			e_passwords_forget_password (item);
	}

	if (password == NULL)
		g_set_error (
			error, G_IO_ERROR,
			G_IO_ERROR_CANCELLED,
			_("User cancelled operation"));

	return password;
}

static gboolean
mail_session_forget_password (CamelSession *session,
                              CamelService *service,
                              const gchar *item,
                              GError **error)
{
	/* XXX The only remaining user of this method is CamelGpgContext,
	 *     which does not provide a CamelService.  Use 'item' as the
	 *     password key. */

	g_return_val_if_fail (service == NULL, FALSE);

	e_passwords_forget_password (item);

	return TRUE;
}

static gboolean
mail_session_forward_to_sync (CamelSession *session,
                              CamelFolder *folder,
                              CamelMimeMessage *message,
                              const gchar *address,
                              GCancellable *cancellable,
                              GError **error)
{
	EMailSessionPrivate *priv;
	ESource *source;
	ESourceRegistry *registry;
	ESourceMailIdentity *extension;
	CamelMimeMessage *forward;
	CamelStream *mem;
	CamelInternetAddress *addr;
	CamelFolder *out_folder;
	CamelMessageInfo *info;
	CamelMedium *medium;
	const gchar *extension_name;
	const gchar *from_address;
	const gchar *from_name;
	const gchar *header_name;
	gboolean success;
	gchar *subject;
	gchar *alias_name = NULL, *alias_address = NULL;

	g_return_val_if_fail (folder != NULL, FALSE);
	g_return_val_if_fail (message != NULL, FALSE);
	g_return_val_if_fail (address != NULL, FALSE);

	priv = E_MAIL_SESSION_GET_PRIVATE (session);

	if (!*address) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("No destination address provided, forwarding "
			"of the message has been cancelled."));
		return FALSE;
	}

	registry = e_mail_session_get_registry (E_MAIL_SESSION (session));

	/* This returns a new ESource reference. */
	source = em_utils_guess_mail_identity_with_recipients (
		registry, message, folder, NULL, &alias_name, &alias_address);
	if (source == NULL) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("No identity found to use, forwarding "
			"of the message has been cancelled."));
		return FALSE;
	}

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	extension = e_source_get_extension (source, extension_name);
	if (alias_address) {
		from_name = alias_name;
		from_address = alias_address;
	} else {
		from_name = e_source_mail_identity_get_name (extension);
		from_address = e_source_mail_identity_get_address (extension);
	}

	if (!from_name || !*from_name)
		from_name = e_source_mail_identity_get_name (extension);

	forward = camel_mime_message_new ();

	/* make copy of the message, because we are going to modify it */
	mem = camel_stream_mem_new ();
	camel_data_wrapper_write_to_stream_sync (
		CAMEL_DATA_WRAPPER (message), mem, NULL, NULL);
	g_seekable_seek (G_SEEKABLE (mem), 0, G_SEEK_SET, NULL, NULL);
	camel_data_wrapper_construct_from_stream_sync (
		CAMEL_DATA_WRAPPER (forward), mem, NULL, NULL);
	g_object_unref (mem);

	/* clear previous recipients */
	camel_mime_message_set_recipients (
		forward, CAMEL_RECIPIENT_TYPE_TO, NULL);
	camel_mime_message_set_recipients (
		forward, CAMEL_RECIPIENT_TYPE_CC, NULL);
	camel_mime_message_set_recipients (
		forward, CAMEL_RECIPIENT_TYPE_BCC, NULL);
	camel_mime_message_set_recipients (
		forward, CAMEL_RECIPIENT_TYPE_RESENT_TO, NULL);
	camel_mime_message_set_recipients (
		forward, CAMEL_RECIPIENT_TYPE_RESENT_CC, NULL);
	camel_mime_message_set_recipients (
		forward, CAMEL_RECIPIENT_TYPE_RESENT_BCC, NULL);

	medium = CAMEL_MEDIUM (forward);

	/* remove all delivery and notification headers */
	header_name = "Disposition-Notification-To";
	while (camel_medium_get_header (medium, header_name))
		camel_medium_remove_header (medium, header_name);

	header_name = "Delivered-To";
	while (camel_medium_get_header (medium, header_name))
		camel_medium_remove_header (medium, header_name);

	/* remove any X-Evolution-* headers that may have been set */
	camel_name_value_array_free (mail_tool_remove_xevolution_headers (forward));

	/* from */
	addr = camel_internet_address_new ();
	camel_internet_address_add (addr, from_name, from_address);
	camel_mime_message_set_from (forward, addr);
	g_object_unref (addr);

	/* to */
	addr = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (addr), address);
	camel_mime_message_set_recipients (
		forward, CAMEL_RECIPIENT_TYPE_TO, addr);
	g_object_unref (addr);

	/* subject */
	subject = mail_tool_generate_forward_subject (message);
	camel_mime_message_set_subject (forward, subject);
	g_free (subject);

	/* and send it */
	info = camel_message_info_new (NULL);
	out_folder = e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_OUTBOX);
	camel_message_info_set_flags (
		info, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);

	success = e_mail_folder_append_message_sync (
		out_folder, forward, info, NULL, cancellable, error);

	if (success) {
		GSettings *settings;
		gboolean flush_outbox;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		flush_outbox = g_settings_get_boolean (settings, "flush-outbox");
		g_object_unref (settings);

		g_mutex_lock (&priv->preparing_flush_lock);

		if (priv->preparing_flush > 0) {
			g_source_remove (priv->preparing_flush);
			flush_outbox = TRUE;
		}

		if (flush_outbox) {
			GMainContext *main_context;
			GSource *timeout_source;

			main_context =
				camel_session_ref_main_context (session);

			timeout_source =
				g_timeout_source_new_seconds (60);
			g_source_set_callback (
				timeout_source,
				session_forward_to_flush_outbox_cb,
				session, (GDestroyNotify) NULL);
			priv->preparing_flush = g_source_attach (
				timeout_source, main_context);
			g_source_unref (timeout_source);

			g_main_context_unref (main_context);
		}

		g_mutex_unlock (&priv->preparing_flush_lock);
	}

	g_clear_object (&info);

	g_object_unref (source);
	g_free (alias_address);
	g_free (alias_name);

	return success;
}

static gboolean
mail_session_get_oauth2_access_token_sync (CamelSession *session,
					   CamelService *service,
					   gchar **out_access_token,
					   gint *out_expires_in,
					   GCancellable *cancellable,
					   GError **error)
{
	EMailSession *mail_session;
	ESource *source, *cred_source;
	GError *local_error = NULL;
	gboolean success;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), FALSE);
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), FALSE);

	mail_session = E_MAIL_SESSION (session);
	source = e_source_registry_ref_source (mail_session->priv->registry, camel_service_get_uid (service));
	if (!source) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
			_("Corresponding source for service with UID “%s” not found"),
			camel_service_get_uid (service));

		return FALSE;
	}

	cred_source = e_source_registry_find_extension (mail_session->priv->registry, source, E_SOURCE_EXTENSION_COLLECTION);
	if (cred_source && !e_util_can_use_collection_as_credential_source (cred_source, source)) {
		g_clear_object (&cred_source);
	}

	success = e_source_get_oauth2_access_token_sync (cred_source ? cred_source : source, cancellable, out_access_token, out_expires_in, &local_error);

	/* The Connection Refused error can be returned when the OAuth2 token is expired or
	   when its refresh failed for some reason. In that case change the error domain/code,
	   thus the other Camel/mail code understands it. */
	if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED) ||
	    g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
		local_error->domain = CAMEL_SERVICE_ERROR;
		local_error->code = CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE;

		e_source_invoke_credentials_required_sync (cred_source ? cred_source : source,
			E_SOURCE_CREDENTIALS_REASON_REJECTED, NULL, 0, local_error, cancellable, NULL);
	}

	if (local_error)
		g_propagate_error (error, local_error);

	g_clear_object (&cred_source);
	g_object_unref (source);

	return success;
}

static EMVFolderContext *
mail_session_create_vfolder_context (EMailSession *session)
{
	return em_vfolder_context_new ();
}

static void
e_mail_session_class_init (EMailSessionClass *class)
{
	GObjectClass *object_class;
	CamelSessionClass *session_class;

	g_type_class_add_private (class, sizeof (EMailSessionPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_session_set_property;
	object_class->get_property = mail_session_get_property;
	object_class->dispose = mail_session_dispose;
	object_class->finalize = mail_session_finalize;
	object_class->constructed = mail_session_constructed;

	session_class = CAMEL_SESSION_CLASS (class);
	session_class->add_service = mail_session_add_service;
	session_class->get_password = mail_session_get_password;
	session_class->forget_password = mail_session_forget_password;
	session_class->forward_to_sync = mail_session_forward_to_sync;
	session_class->get_oauth2_access_token_sync = mail_session_get_oauth2_access_token_sync;

	class->create_vfolder_context = mail_session_create_vfolder_context;

	g_object_class_install_property (
		object_class,
		PROP_FOLDER_CACHE,
		g_param_spec_object (
			"folder-cache",
			NULL,
			NULL,
			MAIL_TYPE_FOLDER_CACHE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_LOCAL_STORE,
		g_param_spec_object (
			"local-store",
			"Local Store",
			"Built-in local store",
			CAMEL_TYPE_STORE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Data source registry",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_VFOLDER_STORE,
		g_param_spec_object (
			"vfolder-store",
			"Search Folder Store",
			"Built-in search folder store",
			CAMEL_TYPE_STORE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EMailSession::flush-outbox
	 * @session: the email session
	 *
	 * Emitted if the send folder should be flushed.
	 **/
	signals[FLUSH_OUTBOX] = g_signal_new (
		"flush-outbox",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMailSessionClass, flush_outbox),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/**
	 * EMailSession::refresh-service
	 * @session: the #EMailSession that emitted the signal
	 * @service: a #CamelService
	 *
	 * Emitted when @service should be refreshed.
	 **/
	signals[REFRESH_SERVICE] = g_signal_new (
		"refresh-service",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailSessionClass, refresh_service),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		CAMEL_TYPE_SERVICE);

	/**
	 * EMailSession::store-added
	 * @session: the #EMailSession that emitted the signal
	 * @store: a #CamelStore
	 *
	 * Emitted when a store is added
	 **/
	signals[STORE_ADDED] = g_signal_new (
		"store-added",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMailSessionClass, store_added),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		CAMEL_TYPE_STORE);

	/**
	 * EMailSession::store-removed
	 * @session: the #EMailSession that emitted the signal
	 * @store: a #CamelStore
	 *
	 * Emitted when a store is removed 
	 **/
	signals[STORE_REMOVED] = g_signal_new (
		"store-removed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMailSessionClass, store_removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		CAMEL_TYPE_STORE);

	/**
	 * EMailSession::store-removed
	 * @session: the #EMailSession that emitted the signal
	 * @source: an #ESource
	 *
	 * This signal is emitted with e_mail_session_emit_allow_auth_prompt() to let
	 * any listeners know to enable credentials prompt for the given @source.
	 *
	 * Since: 3.16
	 **/
	signals[ALLOW_AUTH_PROMPT] = g_signal_new (
		"allow-auth-prompt",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMailSessionClass, allow_auth_prompt),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_SOURCE);

	camel_null_store_register_provider ();

	/* Make sure ESourceCamel picks up the "none" provider. */
	e_source_camel_generate_subtype ("none", CAMEL_TYPE_SETTINGS);
}

static void
e_mail_session_init (EMailSession *session)
{
	GHashTable *auto_refresh_table;
	GHashTable *junk_filters;

	auto_refresh_table = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

	junk_filters = g_hash_table_new (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal);

	session->priv = E_MAIL_SESSION_GET_PRIVATE (session);
	session->priv->folder_cache = mail_folder_cache_new ();
	session->priv->auto_refresh_table = auto_refresh_table;
	session->priv->junk_filters = junk_filters;

	session->priv->local_folders =
		g_ptr_array_new_with_free_func (
		(GDestroyNotify) g_object_unref);
	session->priv->local_folder_uris =
		g_ptr_array_new_with_free_func (
		(GDestroyNotify) g_free);

	g_mutex_init (&session->priv->preparing_flush_lock);
	g_mutex_init (&session->priv->used_services_lock);
	g_cond_init (&session->priv->used_services_cond);

	session->priv->used_services = g_hash_table_new (g_direct_hash, g_direct_equal);
}

EMailSession *
e_mail_session_new (ESourceRegistry *registry)
{
	const gchar *user_data_dir;
	const gchar *user_cache_dir;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	user_data_dir = mail_session_get_data_dir ();
	user_cache_dir = mail_session_get_cache_dir ();

	return g_object_new (
		E_TYPE_MAIL_SESSION,
		"user-data-dir", user_data_dir,
		"user-cache-dir", user_cache_dir,
		"registry", registry,
		NULL);
}

ESourceRegistry *
e_mail_session_get_registry (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return session->priv->registry;
}

MailFolderCache *
e_mail_session_get_folder_cache (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return session->priv->folder_cache;
}

CamelStore *
e_mail_session_get_local_store (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return CAMEL_STORE (session->priv->local_store);
}

CamelFolder *
e_mail_session_get_local_folder (EMailSession *session,
                                 EMailLocalFolder type)
{
	GPtrArray *local_folders;
	CamelFolder *folder;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	local_folders = session->priv->local_folders;
	g_return_val_if_fail (type < local_folders->len, NULL);

	folder = g_ptr_array_index (local_folders, type);
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	return folder;
}

const gchar *
e_mail_session_get_local_folder_uri (EMailSession *session,
                                     EMailLocalFolder type)
{
	GPtrArray *local_folder_uris;
	const gchar *folder_uri;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	local_folder_uris = session->priv->local_folder_uris;
	g_return_val_if_fail (type < local_folder_uris->len, NULL);

	folder_uri = g_ptr_array_index (local_folder_uris, type);
	g_return_val_if_fail (folder_uri != NULL, NULL);

	return folder_uri;
}

GList *
e_mail_session_get_available_junk_filters (EMailSession *session)
{
	GList *list, *link;
	GQueue trash = G_QUEUE_INIT;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	list = g_hash_table_get_values (session->priv->junk_filters);

	/* Discard unavailable junk filters.  (e.g. Junk filter
	 * requires Bogofilter but Bogofilter is not installed,
	 * hence the junk filter is unavailable.) */

	for (link = list; link != NULL; link = g_list_next (link)) {
		EMailJunkFilter *junk_filter;

		junk_filter = E_MAIL_JUNK_FILTER (link->data);
		if (!e_mail_junk_filter_available (junk_filter))
			g_queue_push_tail (&trash, link);
	}

	while ((link = g_queue_pop_head (&trash)) != NULL)
		list = g_list_delete_link (list, link);

	/* Sort the remaining junk filters by display name. */

	return g_list_sort (list, (GCompareFunc) e_mail_junk_filter_compare);
}

/**
 * e_mail_session_get_junk_filter_by_name:
 * @session: an #EMailSession
 * @filter_name: a junk filter name
 *
 * Looks up an #EMailJunkFilter extension by its filter name, as specified
 * in its class structure.  If no match is found, the function returns %NULL.
 *
 * Returns: an #EMailJunkFilter, or %NULL
 **/
EMailJunkFilter *
e_mail_session_get_junk_filter_by_name (EMailSession *session,
                                        const gchar *filter_name)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);
	g_return_val_if_fail (filter_name != NULL, NULL);

	return g_hash_table_lookup (session->priv->junk_filters, filter_name);
}

static void
mail_session_get_inbox_thread (GSimpleAsyncResult *simple,
                               EMailSession *session,
                               GCancellable *cancellable)
{
	AsyncContext *context;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	context->folder = e_mail_session_get_inbox_sync (
		session, context->uid, cancellable, &error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);
}

CamelFolder *
e_mail_session_get_inbox_sync (EMailSession *session,
                               const gchar *service_uid,
                               GCancellable *cancellable,
                               GError **error)
{
	CamelService *service;
	CamelFolder *folder = NULL;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);
	g_return_val_if_fail (service_uid != NULL, NULL);

	service = camel_session_ref_service (
		CAMEL_SESSION (session), service_uid);

	if (service == NULL)
		return NULL;

	if (!CAMEL_IS_STORE (service))
		goto exit;

	if (!camel_service_connect_sync (service, cancellable, error))
		goto exit;

	folder = camel_store_get_inbox_folder_sync (
		CAMEL_STORE (service), cancellable, error);

exit:
	g_object_unref (service);

	return folder;
}

void
e_mail_session_get_inbox (EMailSession *session,
                          const gchar *service_uid,
                          gint io_priority,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (service_uid != NULL);

	context = g_slice_new0 (AsyncContext);
	context->uid = g_strdup (service_uid);

	simple = g_simple_async_result_new (
		G_OBJECT (session), callback,
		user_data, e_mail_session_get_inbox);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) async_context_free);

	g_simple_async_result_run_in_thread (
		simple, (GSimpleAsyncThreadFunc)
		mail_session_get_inbox_thread,
		io_priority, cancellable);

	g_object_unref (simple);
}

CamelFolder *
e_mail_session_get_inbox_finish (EMailSession *session,
                                 GAsyncResult *result,
                                 GError **error)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (session),
		e_mail_session_get_inbox), NULL);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	context = g_simple_async_result_get_op_res_gpointer (simple);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	g_return_val_if_fail (CAMEL_IS_FOLDER (context->folder), NULL);

	return g_object_ref (context->folder);
}

static void
mail_session_get_trash_thread (GSimpleAsyncResult *simple,
                               EMailSession *session,
                               GCancellable *cancellable)
{
	AsyncContext *context;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	context->folder = e_mail_session_get_trash_sync (
		session, context->uid, cancellable, &error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);
}

CamelFolder *
e_mail_session_get_trash_sync (EMailSession *session,
                               const gchar *service_uid,
                               GCancellable *cancellable,
                               GError **error)
{
	CamelService *service;
	CamelFolder *folder = NULL;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);
	g_return_val_if_fail (service_uid != NULL, NULL);

	service = camel_session_ref_service (
		CAMEL_SESSION (session), service_uid);

	if (service == NULL)
		return NULL;

	if (!CAMEL_IS_STORE (service))
		goto exit;

	if (!camel_service_connect_sync (service, cancellable, error))
		goto exit;

	folder = camel_store_get_trash_folder_sync (
		CAMEL_STORE (service), cancellable, error);

exit:
	g_object_unref (service);

	return folder;
}

void
e_mail_session_get_trash (EMailSession *session,
                          const gchar *service_uid,
                          gint io_priority,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (service_uid != NULL);

	context = g_slice_new0 (AsyncContext);
	context->uid = g_strdup (service_uid);

	simple = g_simple_async_result_new (
		G_OBJECT (session), callback,
		user_data, e_mail_session_get_trash);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) async_context_free);

	g_simple_async_result_run_in_thread (
		simple, (GSimpleAsyncThreadFunc)
		mail_session_get_trash_thread,
		io_priority, cancellable);

	g_object_unref (simple);
}

CamelFolder *
e_mail_session_get_trash_finish (EMailSession *session,
                                 GAsyncResult *result,
                                 GError **error)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (session),
		e_mail_session_get_trash), NULL);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	context = g_simple_async_result_get_op_res_gpointer (simple);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	g_return_val_if_fail (CAMEL_IS_FOLDER (context->folder), NULL);

	return g_object_ref (context->folder);
}

static void
mail_session_uri_to_folder_thread (GSimpleAsyncResult *simple,
                                   EMailSession *session,
                                   GCancellable *cancellable)
{
	AsyncContext *context;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	context->folder = e_mail_session_uri_to_folder_sync (
		session, context->uri, context->flags,
		cancellable, &error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);
}

CamelFolder *
e_mail_session_uri_to_folder_sync (EMailSession *session,
                                   const gchar *folder_uri,
                                   CamelStoreGetFolderFlags flags,
                                   GCancellable *cancellable,
                                   GError **error)
{
	CamelStore *store;
	CamelFolder *folder;
	gchar *folder_name;
	gboolean success;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);
	g_return_val_if_fail (folder_uri != NULL, NULL);

	success = e_mail_folder_uri_parse (
		CAMEL_SESSION (session), folder_uri,
		&store, &folder_name, error);

	if (!success)
		return NULL;

	folder = camel_store_get_folder_sync (
		store, folder_name, flags, cancellable, error);

	if (folder != NULL) {
		MailFolderCache *folder_cache;
		folder_cache = e_mail_session_get_folder_cache (session);
		mail_folder_cache_note_folder (folder_cache, folder);
	}

	g_free (folder_name);
	g_object_unref (store);

	return folder;
}

void
e_mail_session_uri_to_folder (EMailSession *session,
                              const gchar *folder_uri,
                              CamelStoreGetFolderFlags flags,
                              gint io_priority,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (folder_uri != NULL);

	context = g_slice_new0 (AsyncContext);
	context->uri = g_strdup (folder_uri);
	context->flags = flags;

	simple = g_simple_async_result_new (
		G_OBJECT (session), callback,
		user_data, e_mail_session_uri_to_folder);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) async_context_free);

	g_simple_async_result_run_in_thread (
		simple, (GSimpleAsyncThreadFunc)
		mail_session_uri_to_folder_thread,
		io_priority, cancellable);

	g_object_unref (simple);
}

CamelFolder *
e_mail_session_uri_to_folder_finish (EMailSession *session,
                                     GAsyncResult *result,
                                     GError **error)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (session),
		e_mail_session_uri_to_folder), NULL);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	context = g_simple_async_result_get_op_res_gpointer (simple);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	g_return_val_if_fail (CAMEL_IS_FOLDER (context->folder), NULL);

	return g_object_ref (context->folder);
}

gboolean
e_binding_transform_service_to_source (GBinding *binding,
                                       const GValue *source_value,
                                       GValue *target_value,
                                       gpointer session)
{
	CamelService *service;
	ESourceRegistry *registry;
	ESource *source;
	const gchar *uid;
	gboolean success = FALSE;

	g_return_val_if_fail (G_IS_BINDING (binding), FALSE);
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), FALSE);

	service = g_value_get_object (source_value);

	if (!CAMEL_IS_SERVICE (service))
		return FALSE;

	uid = camel_service_get_uid (service);
	registry = e_mail_session_get_registry (session);
	source = e_source_registry_ref_source (registry, uid);

	if (source != NULL) {
		g_value_take_object (target_value, source);
		success = TRUE;
	}

	return success;
}

gboolean
e_binding_transform_source_to_service (GBinding *binding,
                                       const GValue *source_value,
                                       GValue *target_value,
                                       gpointer session)
{
	CamelService *service;
	ESource *source;
	const gchar *uid;

	g_return_val_if_fail (G_IS_BINDING (binding), FALSE);
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), FALSE);

	source = g_value_get_object (source_value);

	if (!E_IS_SOURCE (source))
		return FALSE;

	uid = e_source_get_uid (source);
	service = camel_session_ref_service (session, uid);

	if (service == NULL)
		return FALSE;

	g_value_take_object (target_value, service);

	return TRUE;
}

/******************************** Legacy API *********************************/

void
mail_session_flush_filter_log (EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));

	if (session->priv->filter_logfile)
		fflush (session->priv->filter_logfile);
}

const gchar *
mail_session_get_data_dir (void)
{
	if (G_UNLIKELY (mail_data_dir == NULL)) {
		mail_data_dir = g_build_filename (e_get_user_data_dir (), "mail", NULL);
		g_mkdir_with_parents (mail_data_dir, 0700);
	}

	return mail_data_dir;
}

const gchar *
mail_session_get_cache_dir (void)
{
	if (G_UNLIKELY (mail_cache_dir == NULL)) {
		mail_cache_dir = g_build_filename (e_get_user_cache_dir (), "mail", NULL);
		g_mkdir_with_parents (mail_cache_dir, 0700);
	}

	return mail_cache_dir;
}

const gchar *
mail_session_get_config_dir (void)
{
	if (G_UNLIKELY (mail_config_dir == NULL)) {
		mail_config_dir = g_build_filename (e_get_user_config_dir (), "mail", NULL);
		g_mkdir_with_parents (mail_config_dir, 0700);
	}

	return mail_config_dir;
}

CamelStore *
e_mail_session_get_vfolder_store (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return CAMEL_STORE (session->priv->vfolder_store);
}

EMVFolderContext *
e_mail_session_create_vfolder_context (EMailSession *session)
{
	EMailSessionClass *class;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	class = E_MAIL_SESSION_GET_CLASS (session);
	g_return_val_if_fail (class != NULL, NULL);
	g_return_val_if_fail (class->create_vfolder_context != NULL, NULL);

	return class->create_vfolder_context (session);
}

static gboolean
mail_session_flush_outbox_timeout_cb (gpointer user_data)
{
	EMailSession *session = user_data;

	if (g_source_is_destroyed (g_main_current_source ()))
		return FALSE;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), FALSE);

	g_mutex_lock (&session->priv->preparing_flush_lock);
	if (session->priv->outbox_flush_id == g_source_get_id (g_main_current_source ()))
		session->priv->outbox_flush_id = 0;
	g_mutex_unlock (&session->priv->preparing_flush_lock);

	e_mail_session_flush_outbox (session);

	return FALSE;
}

void
e_mail_session_flush_outbox (EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));

	g_mutex_lock (&session->priv->preparing_flush_lock);
	if (session->priv->outbox_flush_id > 0) {
		g_source_remove (session->priv->outbox_flush_id);
		session->priv->outbox_flush_id = 0;
	}
	g_mutex_unlock (&session->priv->preparing_flush_lock);

	/* Connect to this and call mail_send in the main email client.*/
	g_signal_emit (session, signals[FLUSH_OUTBOX], 0);
}

void
e_mail_session_schedule_outbox_flush (EMailSession *session,
				      gint delay_minutes)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (delay_minutes >= 0);

	if (delay_minutes == 0) {
		e_mail_session_flush_outbox (session);
		return;
	}

	g_mutex_lock (&session->priv->preparing_flush_lock);
	if (session->priv->outbox_flush_id > 0) {
		g_source_remove (session->priv->outbox_flush_id);
		session->priv->outbox_flush_id = 0;
	}

	session->priv->outbox_flush_id = e_named_timeout_add_seconds (60 * delay_minutes, mail_session_flush_outbox_timeout_cb, session);

	g_mutex_unlock (&session->priv->preparing_flush_lock);
}

void
e_mail_session_cancel_scheduled_outbox_flush (EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));

	g_mutex_lock (&session->priv->preparing_flush_lock);
	if (session->priv->outbox_flush_id > 0) {
		g_source_remove (session->priv->outbox_flush_id);
		session->priv->outbox_flush_id = 0;
	}
	g_mutex_unlock (&session->priv->preparing_flush_lock);
}

static void
mail_session_wakeup_used_services_cond (GCancellable *cancenllable,
					  EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));

	/* Use broadcast here, because it's not known which operation had been
	   cancelled, thus rather wake up all of them to retest. */
	g_cond_broadcast (&session->priv->used_services_cond);
}

/**
 * e_mail_session_mark_service_used_sync:
 * @session: an #EMailSession
 * @service: a #CamelService
 * @cancellable: (allow none): a #GCancellable, or NULL
 *
 * Marks the @service as being used. If it is already in use, then waits
 * for its release. The only reasons for a failure are either invalid
 * parameters being passed in the function or the wait being cancelled.
 * Use e_mail_session_unmark_service_used() to notice the @session that
 * that the @service is no longer being used by the caller.
 *
 * Returns: Whether successfully waited for the @service.
 *
 * Since: 3.16
 **/
gboolean
e_mail_session_mark_service_used_sync (EMailSession *session,
				       CamelService *service,
				       GCancellable *cancellable)
{
	gulong cancelled_id = 0;
	gboolean message_pushed = FALSE;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), FALSE);
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), FALSE);

	g_mutex_lock (&session->priv->used_services_lock);

	if (cancellable)
		cancelled_id = g_cancellable_connect (cancellable, G_CALLBACK (mail_session_wakeup_used_services_cond), session, NULL);

	while (!g_cancellable_is_cancelled (cancellable) &&
		g_hash_table_contains (session->priv->used_services, service)) {

		if (!message_pushed) {
			camel_operation_push_message (cancellable, _("Waiting for “%s”"), camel_service_get_display_name (service));
			message_pushed = TRUE;
		}

		g_cond_wait (&session->priv->used_services_cond, &session->priv->used_services_lock);
	}

	if (message_pushed)
		camel_operation_pop_message (cancellable);

	if (cancelled_id)
		g_cancellable_disconnect (cancellable, cancelled_id);

	if (!g_cancellable_is_cancelled (cancellable))
		g_hash_table_insert (session->priv->used_services, service, GINT_TO_POINTER (1));

	g_mutex_unlock (&session->priv->used_services_lock);

	return !g_cancellable_is_cancelled (cancellable);
}

/**
 * e_mail_session_unmark_service_used:
 * @session: an #EMailSession
 * @service: a #CamelService
 *
 * Frees a "use lock" on the @service, thus it can be used by others. If anything
 * is waiting for it in e_mail_session_mark_service_used_sync(), then it is woken up.
 *
 * Since: 3.16
 **/
void
e_mail_session_unmark_service_used (EMailSession *session,
				    CamelService *service)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	g_mutex_lock (&session->priv->used_services_lock);

	if (g_hash_table_remove (session->priv->used_services, service)) {
		g_cond_signal (&session->priv->used_services_cond);
	}

	g_mutex_unlock (&session->priv->used_services_lock);
}

/**
 * e_mail_session_emit_allow_auth_prompt:
 * @session: an #EMailSession
 * @source: an #ESource
 *
 * Emits 'allow-auth-prompt' on @session for @source. This lets
 * any listeners know to enable credentials prompt for this @source.
 *
 * Since: 3.16
 **/
void
e_mail_session_emit_allow_auth_prompt (EMailSession *session,
				       ESource *source)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (E_IS_SOURCE (source));

	g_signal_emit (session, signals[ALLOW_AUTH_PROMPT], 0, source);
}
