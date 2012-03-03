/*
 * e-mail-session.c
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
 * Authors:
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 *
 */

/* mail-session.c: handles the session information and resource manipulation */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#ifdef HAVE_CANBERRA
#include <canberra-gtk.h>
#endif

#include <libedataserver/e-flag.h>
#include <libedataserver/e-proxy.h>
#include <libebackend/e-extensible.h>
#include <libedataserverui/e-passwords.h>
#include <libedataserver/e-data-server-util.h>

#include "libemail-utils/e-account-utils.h"
#include "libemail-utils/mail-mt.h"

/* This is our hack, not part of libcamel. */
#include "camel-null-store.h"

#include "e-mail-junk-filter.h"
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

struct _EMailSessionPrivate {
	MailFolderCache *folder_cache;

	EAccountList *account_list;
	gulong account_added_handler_id;

	CamelStore *local_store;
	CamelStore *vfolder_store;

	FILE *filter_logfile;
	GHashTable *junk_filters;
	EProxy *proxy;

	/* Local folder cache. */
	GPtrArray *local_folders;
	GPtrArray *local_folder_uris;
};

struct _AsyncContext {
	/* arguments */
	CamelStoreGetFolderFlags flags;
	gchar *uid;
	gchar *uri;

	/* results */
	CamelFolder *folder;
};

enum {
	PROP_0,
	PROP_FOLDER_CACHE,
	PROP_JUNK_FILTER_NAME,
	PROP_LOCAL_STORE,
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
	STORE_ADDED,
	STORE_REMOVED,
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

/* Support for CamelSession.alert_user() *************************************/

static GQueue user_message_queue = { NULL, NULL, 0 };

struct _user_message_msg {
	MailMsg base;

	CamelSessionAlertType type;
	gchar *prompt;
	GSList *button_captions;
	EFlag *done;

	gint result;
	guint ismain : 1;
};

static void user_message_exec (struct _user_message_msg *m,
                               GCancellable *cancellable,
                               GError **error);

static void
user_message_response_free (struct _user_message_msg *m)
{

	/* check for pendings */
	if (!g_queue_is_empty (&user_message_queue)) {
		GCancellable *cancellable;

		m = g_queue_pop_head (&user_message_queue);
		cancellable = m->base.cancellable;
		user_message_exec (m, cancellable, &m->base.error);
		mail_msg_unref (m);
	}
}

/* clicked, send back the reply */
static void
user_message_response (struct _user_message_msg *m)
{
	/* if !allow_cancel, then we've already replied */
	if (m->button_captions) {
		m->result = TRUE; //If Accepted
		e_flag_set (m->done);
	}

	user_message_response_free (m);
}

static void
user_message_exec (struct _user_message_msg *m,
                   GCancellable *cancellable,
                   GError **error)
{
	/* XXX This is a case where we need to be able to construct
	 *     custom EAlerts without a predefined XML definition. */
	if (m->ismain) {
		/* Use DBUS to raise dialogs in clients and reply back.
		 * For now say accept all. */
		user_message_response (m);
	} else
		g_queue_push_tail (&user_message_queue, mail_msg_ref (m));
}

static void
user_message_free (struct _user_message_msg *m)
{
	g_free (m->prompt);
	g_slist_free_full (m->button_captions, g_free);
	e_flag_free (m->done);
}

static MailMsgInfo user_message_info = {
	sizeof (struct _user_message_msg),
	(MailMsgDescFunc) NULL,
	(MailMsgExecFunc) user_message_exec,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) user_message_free
};

/* Support for CamelSession.get_filter_driver () *****************************/

static CamelFolder *
get_folder (CamelFilterDriver *d,
            const gchar *uri,
            gpointer user_data,
            GError **error)
{
	EMailSession *session = E_MAIL_SESSION (user_data);

	/* FIXME Not passing a GCancellable here. */
	/* FIXME Need a camel_filter_driver_get_session(). */
	return e_mail_session_uri_to_folder_sync (
		session, uri, 0, NULL, error);
}

static CamelFilterDriver *
main_get_filter_driver (CamelSession *session,
                        const gchar *type,
                        GError **error)
{
	CamelFilterDriver *driver;
	EMailSession *ms = (EMailSession *) session;
	GSettings *settings;

	settings = g_settings_new ("org.gnome.evolution.mail");

	driver = camel_filter_driver_new (session);
	camel_filter_driver_set_folder_func (driver, get_folder, session);

	if (g_settings_get_boolean (settings, "filters-log-actions")) {
		if (ms->priv->filter_logfile == NULL) {
			gchar *filename;

			filename = g_settings_get_string (settings, "filters-log-file");
			if (filename) {
				ms->priv->filter_logfile = g_fopen (filename, "a+");
				g_free (filename);
			}
		}

		if (ms->priv->filter_logfile)
			camel_filter_driver_set_logfile (driver, ms->priv->filter_logfile);
	}

	g_object_unref (settings);

	return driver;
}

/* Support for CamelSession.forward_to () ************************************/

static guint preparing_flush = 0;

static gboolean
forward_to_flush_outbox_cb (EMailSession *session)
{

	preparing_flush = 0;

	/* Connect to this and call mail_send in the main email client.*/
	g_signal_emit (session, signals[FLUSH_OUTBOX], 0);

	return FALSE;
}

static void
ms_forward_to_cb (CamelFolder *folder,
                  GAsyncResult *result,
                  EMailSession *session)
{
	GSettings *settings;

	/* FIXME Poor error handling. */
	if (!e_mail_folder_append_message_finish (folder, result, NULL, NULL))
		return;

	settings = g_settings_new ("org.gnome.evolution.mail");

	/* do not call mail send immediately, just pile them all in the outbox */
	if (preparing_flush || g_settings_get_boolean (
		settings, "flush-outbox")) {
		if (preparing_flush)
			g_source_remove (preparing_flush);

		preparing_flush = g_timeout_add_seconds (
			60, (GSourceFunc)
			forward_to_flush_outbox_cb, session);
	}

	g_object_unref (settings);
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

static gchar *
mail_session_make_key (CamelService *service,
                       const gchar *item)
{
	gchar *key;

	if (service != NULL) {
		CamelURL *url;

		url = camel_service_new_camel_url (service);
		key = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
		camel_url_free (url);
	} else
		key = g_strdup (item);

	return key;
}

static void
mail_session_check_junk_notify (GSettings *settings,
                                const gchar *key,
                                CamelSession *session)
{
	if (strcmp (key, "junk-check-incoming") == 0)
		camel_session_set_check_junk (
			session, g_settings_get_boolean (settings, key));
}

static const gchar *
mail_session_get_junk_filter_name (EMailSession *session)
{
	CamelJunkFilter *junk_filter;
	GHashTableIter iter;
	gpointer key, value;

	/* XXX This property can be removed once Evolution moves to
	 *     GSettings and can use transform functions when binding
	 *     properties to settings.  That's why this is private. */

	g_hash_table_iter_init (&iter, session->priv->junk_filters);
	junk_filter = camel_session_get_junk_filter (CAMEL_SESSION (session));

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		if (junk_filter == CAMEL_JUNK_FILTER (value))
			return (const gchar *) key;
	}

	if (junk_filter != NULL)
		g_warning (
			"Camel is using a junk filter "
			"unknown to Evolution of type %s",
			G_OBJECT_TYPE_NAME (junk_filter));

	return "";
}

static void
mail_session_set_junk_filter_name (EMailSession *session,
                                   const gchar *junk_filter_name)
{
	CamelJunkFilter *junk_filter = NULL;

	/* XXX This property can be removed once Evolution moves to
	 *     GSettings and can use transform functions when binding
	 *     properties to settings.  That's why this is private. */

	/* An empty string is equivalent to a NULL string. */
	if (junk_filter_name != NULL && *junk_filter_name == '\0')
		junk_filter_name = NULL;

	if (junk_filter_name != NULL) {
		junk_filter = g_hash_table_lookup (
			session->priv->junk_filters, junk_filter_name);
		if (junk_filter != NULL) {
			if (!e_mail_junk_filter_available (
				E_MAIL_JUNK_FILTER (junk_filter)))
				junk_filter = NULL;
		} else {
			g_warning (
				"Unrecognized junk filter name "
				"'%s' in GSettings", junk_filter_name);
		}
	}

	camel_session_set_junk_filter (CAMEL_SESSION (session), junk_filter);

	/* XXX We emit the "notify" signal in mail_session_notify(). */
}

static void
mail_session_add_by_account (EMailSession *session,
                             EAccount *account)
{
	CamelService *service = NULL;
	CamelProvider *provider;
	CamelURL *url = NULL;
	const gchar *protocol = NULL;
	gboolean have_source_url;
	GError *error = NULL;

	have_source_url =
		(account->source != NULL) &&
		(account->source->url != NULL);

	if (have_source_url)
		url = camel_url_new (account->source->url, NULL);

	protocol = (url != NULL) ? url->protocol : "none";
	provider = camel_provider_get (protocol, &error);

	if (url != NULL)
		camel_url_free (url);

	if (error != NULL) {
		g_warn_if_fail (provider == NULL);
		g_warning ("%s", error->message);
		g_error_free (error);
		return;
	}

	g_return_if_fail (provider != NULL);

	/* Load the service, but don't connect.  Check its provider,
	 * and if this belongs in the folder tree model, add it. */

	service = camel_session_add_service (
		CAMEL_SESSION (session),
		account->uid, provider->protocol,
		CAMEL_PROVIDER_STORE, &error);

	if (error != NULL) {
		g_warning (
			"Failed to add service: %s: %s",
			account->name, error->message);
		g_error_free (error);
		return;
	}

	camel_service_set_display_name (service, account->name);

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
}

static void
mail_session_account_added_cb (EAccountList *account_list,
                               EAccount *account,
                               EMailSession *session)
{
	mail_session_add_by_account (session, account);
}

static void
mail_session_add_local_store (EMailSession *session)
{
	CamelLocalSettings *local_settings;
	CamelSession *camel_session;
	CamelSettings *settings;
	CamelService *service;
	const gchar *data_dir;
	gchar *path;
	gint ii;
	GError *error = NULL;

	camel_session = CAMEL_SESSION (session);

	service = camel_session_add_service (
		camel_session, E_MAIL_SESSION_LOCAL_UID,
		"maildir", CAMEL_PROVIDER_STORE, &error);

	/* XXX One could argue this is a fatal error
	 *     since we depend on it in so many places. */
	if (error != NULL) {
		g_critical ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		return;
	}

	g_return_if_fail (CAMEL_IS_SERVICE (service));

	camel_service_set_display_name (service, _("On This Computer"));

	settings = camel_service_get_settings (service);
	local_settings = CAMEL_LOCAL_SETTINGS (settings);
	data_dir = camel_session_get_user_data_dir (camel_session);

	path = g_build_filename (data_dir, E_MAIL_SESSION_LOCAL_UID, NULL);
	camel_local_settings_set_path (local_settings, path);
	g_free (path);

	/* Shouldn't need to worry about other mail applications
	 * altering files in our local mail store. */
	g_object_set (service, "need-summary-check", FALSE, NULL);

	/* Populate the local folder cache. */
	for (ii = 0; ii < E_MAIL_NUM_LOCAL_FOLDERS; ii++) {
		CamelFolder *folder;
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

	session->priv->local_store = g_object_ref (service);
}

static void
mail_session_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_JUNK_FILTER_NAME:
			mail_session_set_junk_filter_name (
				E_MAIL_SESSION (object),
				g_value_get_string (value));
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

		case PROP_JUNK_FILTER_NAME:
			g_value_set_string (
				value,
				mail_session_get_junk_filter_name (
				E_MAIL_SESSION (object)));
			return;

		case PROP_LOCAL_STORE:
			g_value_set_object (
				value,
				e_mail_session_get_local_store (
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

	if (priv->folder_cache != NULL) {
		g_object_unref (priv->folder_cache);
		priv->folder_cache = NULL;
	}

	if (priv->account_list != NULL) {
		g_signal_handler_disconnect (
			priv->account_list,
			priv->account_added_handler_id);
		g_object_unref (priv->account_list);
		priv->account_list = NULL;
	}

	if (priv->local_store != NULL) {
		g_object_unref (priv->local_store);
		priv->local_store = NULL;
	}

	if (priv->vfolder_store != NULL) {
		g_object_unref (priv->vfolder_store);
		priv->vfolder_store = NULL;
	}
	g_ptr_array_set_size (priv->local_folders, 0);
	g_ptr_array_set_size (priv->local_folder_uris, 0);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_session_parent_class)->dispose (object);
}

static void
mail_session_add_vfolder_store (EMailSession *session)
{
	CamelSession *camel_session;
	CamelService *service;
	GError *error = NULL;

	camel_session = CAMEL_SESSION (session);

	service = camel_session_add_service (
		camel_session, E_MAIL_SESSION_VFOLDER_UID,
		"vfolder", CAMEL_PROVIDER_STORE, &error);

	if (error != NULL) {
		g_critical ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		return;
	}

	g_return_if_fail (CAMEL_IS_SERVICE (service));

	camel_service_set_display_name (service, _("Search Folders"));
	em_utils_connect_service_sync (service, NULL, NULL);

	/* XXX There's more configuration to do in vfolder_load_storage()
	 *     but it requires an EMailBackend, which we don't have access
	 *     to from here, so it has to be called from elsewhere.  Kinda
	 *     thinking about reworking that... */

	session->priv->vfolder_store = g_object_ref (service);
}

static void
mail_session_finalize (GObject *object)
{
	EMailSessionPrivate *priv;

	priv = E_MAIL_SESSION_GET_PRIVATE (object);

	g_hash_table_destroy (priv->junk_filters);
	g_object_unref (priv->proxy);

	g_ptr_array_free (priv->local_folders, TRUE);
	g_ptr_array_free (priv->local_folder_uris, TRUE);

	g_free (mail_data_dir);
	g_free (mail_config_dir);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_session_parent_class)->finalize (object);
}

static void
mail_session_notify (GObject *object,
                     GParamSpec *pspec)
{
	/* GObject does not implement this method; do not chain up. */

	/* XXX Delete this once Evolution moves to GSettings and
	 *     we're able to get rid of PROP_JUNK_FILTER_NAME. */
	if (g_strcmp0 (pspec->name, "junk-filter") == 0)
		g_object_notify (object, "junk-filter-name");
}

static gboolean
mail_session_initialize_stores_idle (gpointer user_data)
{
	EMailSession *session = user_data;
	EAccountList *account_list;
	EAccount *account;
	EIterator *iter;

	g_return_val_if_fail (session != NULL, FALSE);

	account_list = e_get_account_list ();
	iter = e_list_get_iterator (E_LIST (account_list));

	while (e_iterator_is_valid (iter)) {
		/* XXX EIterator misuses const. */
		account = (EAccount *) e_iterator_get (iter);

		mail_session_add_by_account (session, account);

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	return FALSE;
}

static void
mail_session_constructed (GObject *object)
{
	EMailSession *session;
	EExtensible *extensible;
	GType extension_type;
	GList *list, *link;
	GSettings *settings;
	EAccountList *account_list;
	gulong handler_id;

	session = E_MAIL_SESSION (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_session_parent_class)->constructed (object);

	account_list = e_get_account_list ();
	session->priv->account_list = g_object_ref (account_list);

	/* This must be created after the account store. */
	session->priv->folder_cache = mail_folder_cache_new (session);

	/* Add built-in CamelStores. */
	mail_session_add_local_store (session);
	mail_session_add_vfolder_store (session);

	/* Give it a chance to load user settings, they are not loaded yet.
	 *
	 * XXX Is this the case where hiding such natural things like loading
	 *     user setting into an EExtension strikes back and proves itself
	 *     being suboptimal?
	 */
	g_idle_add (mail_session_initialize_stores_idle, object);

	/* Listen for account list updates. */

	handler_id = g_signal_connect (
		account_list, "account-added",
		G_CALLBACK (mail_session_account_added_cb), session);
	session->priv->account_added_handler_id = handler_id;

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

	/* Bind the "junk-default-plugin" GSettings
	 * key to our "junk-filter-name" property. */

	settings = g_settings_new ("org.gnome.evolution.mail");
	g_settings_bind (
		settings, "junk-default-plugin",
		object, "junk-filter-name",
		G_SETTINGS_BIND_DEFAULT);
	g_object_unref (settings);
}

static CamelService *
mail_session_add_service (CamelSession *session,
                          const gchar *uid,
                          const gchar *protocol,
                          CamelProviderType type,
                          GError **error)
{
	CamelService *service;

	/* Chain up to parents add_service() method. */
	service = CAMEL_SESSION_CLASS (e_mail_session_parent_class)->
		add_service (session, uid, protocol, type, error);

	/* Initialize the CamelSettings object from CamelURL parameters.
	 * This is temporary; soon we'll read settings from key files. */

	if (CAMEL_IS_SERVICE (service)) {
		EAccount *account;
		CamelURL *url = NULL;

		account = e_get_account_by_uid (uid);
		if (account != NULL) {
			const gchar *url_string = NULL;

			switch (type) {
				case CAMEL_PROVIDER_STORE:
					url_string = account->source->url;
					break;
				case CAMEL_PROVIDER_TRANSPORT:
					url_string = account->transport->url;
					break;
				default:
					break;
			}

			/* Be lenient about malformed URLs. */
			if (url_string != NULL)
				url = camel_url_new (url_string, NULL);
		}

		if (url != NULL) {
			CamelSettings *settings;

			settings = camel_service_get_settings (service);
			camel_settings_load_from_url (settings, url);
			camel_url_free (url);

			g_object_notify (G_OBJECT (service), "settings");

			/* Migrate files for this service from its old
			 * URL-based directory to a UID-based directory
			 * if necessary. */
			camel_service_migrate_files (service);
		}
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
	EAccount *account = NULL;
	const gchar *display_name = NULL;
	const gchar *uid = NULL;
	gchar *ret = NULL;

	if (CAMEL_IS_SERVICE (service)) {
		display_name = camel_service_get_display_name (service);
		uid = camel_service_get_uid (service);
		account = e_get_account_by_uid (uid);
	}

	if (!strcmp(item, "popb4smtp_uid")) {
		/* not 100% mt safe, but should be ok */
		ret = g_strdup ((account != NULL) ? account->uid : uid);
	} else {
		gchar *key = mail_session_make_key (service, item);
		EAccountService *config_service = NULL;

		ret = e_passwords_get_password (NULL, key);
		if (ret == NULL || (flags & CAMEL_SESSION_PASSWORD_REPROMPT)) {
			gboolean remember;

			g_free (ret);
			ret = NULL;

			if (account != NULL) {
				if (CAMEL_IS_STORE (service))
					config_service = account->source;
				if (CAMEL_IS_TRANSPORT (service))
					config_service = account->transport;
			}

			remember = config_service ? config_service->save_passwd : FALSE;

			if (!config_service || (config_service &&
				!config_service->get_password_canceled)) {
				guint32 eflags;
				gchar *title;

				if (flags & CAMEL_SESSION_PASSPHRASE) {
					if (display_name != NULL)
						title = g_strdup_printf (
							_("Enter Passphrase for %s"),
							display_name);
					else
						title = g_strdup (
							_("Enter Passphrase"));
				} else {
					if (display_name != NULL)
						title = g_strdup_printf (
							_("Enter Password for %s"),
							display_name);
					else
						title = g_strdup (
							_("Enter Password"));
				}
				if ((flags & CAMEL_SESSION_PASSWORD_STATIC) != 0)
					eflags = E_PASSWORDS_REMEMBER_NEVER;
				else if (config_service == NULL)
					eflags = E_PASSWORDS_REMEMBER_SESSION;
				else
					eflags = E_PASSWORDS_REMEMBER_FOREVER;

				if (flags & CAMEL_SESSION_PASSWORD_REPROMPT)
					eflags |= E_PASSWORDS_REPROMPT;

				if (flags & CAMEL_SESSION_PASSWORD_SECRET)
					eflags |= E_PASSWORDS_SECRET;

				if (flags & CAMEL_SESSION_PASSPHRASE)
					eflags |= E_PASSWORDS_PASSPHRASE;

				/* HACK: breaks abstraction ...
				 * e_account_writable() doesn't use the
				 * EAccount, it also uses the same writable
				 * key for source and transport. */
				if (!e_account_writable (NULL, E_ACCOUNT_SOURCE_SAVE_PASSWD))
					eflags |= E_PASSWORDS_DISABLE_REMEMBER;

				ret = e_passwords_ask_password (
					title, NULL, key, prompt,
					eflags, &remember, NULL);

				if (!ret)
					e_passwords_forget_password (NULL, key);

				g_free (title);

				if (ret && config_service) {
					config_service->save_passwd = remember;
					e_account_list_save (e_get_account_list ());
				}

				if (config_service)
					config_service->get_password_canceled = ret == NULL;
			}
		}

		g_free (key);
	}

	if (ret == NULL)
		g_set_error (
			error, G_IO_ERROR,
			G_IO_ERROR_CANCELLED,
			_("User canceled operation."));

	return ret;
}

static gboolean
mail_session_forget_password (CamelSession *session,
                              CamelService *service,
                              const gchar *item,
                              GError **error)
{
	gchar *key;

	key = mail_session_make_key (service, item);

	e_passwords_forget_password (NULL, key);

	g_free (key);

	return TRUE;
}

static gint
mail_session_alert_user (CamelSession *session,
                         CamelSessionAlertType type,
                         const gchar *prompt,
                         GSList *button_captions)
{
	struct _user_message_msg *m;
	GCancellable *cancellable;
	gint result = -1;
	GSList *iter;

	m = mail_msg_new (&user_message_info);
	m->ismain = mail_in_main_thread ();
	m->type = type;
	m->prompt = g_strdup (prompt);
	m->done = e_flag_new ();
	m->button_captions = g_slist_copy (button_captions);

	for (iter = m->button_captions; iter; iter = iter->next)
		iter->data = g_strdup (iter->data);

	if (g_slist_length (button_captions) > 1)
		mail_msg_ref (m);

	cancellable = m->base.cancellable;

	if (m->ismain)
		user_message_exec (m, cancellable, &m->base.error);
	else
		mail_msg_main_loop_push (m);

	if (g_slist_length (button_captions) > 1) {
		e_flag_wait (m->done);
		result = m->result;
		mail_msg_unref (m);
	} else if (m->ismain)
		mail_msg_unref (m);

	return result;
}

static CamelFilterDriver *
mail_session_get_filter_driver (CamelSession *session,
                                const gchar *type,
                                GError **error)
{
	return (CamelFilterDriver *) mail_call_main (
		MAIL_CALL_p_ppp, (MailMainFunc) main_get_filter_driver,
		session, type, error);
}

static gboolean
mail_session_lookup_addressbook (CamelSession *session,
                                 const gchar *name)
{
	CamelInternetAddress *addr;
	gboolean ret;

	if (!mail_config_get_lookup_book ())
		return FALSE;

	addr = camel_internet_address_new ();
	camel_address_decode ((CamelAddress *) addr, name);
	ret = em_utils_in_addressbook (
		addr, mail_config_get_lookup_book_local_only ());
	g_object_unref (addr);

	return ret;
}

static gboolean
mail_session_forward_to (CamelSession *session,
                         CamelFolder *folder,
                         CamelMimeMessage *message,
                         const gchar *address,
                         GError **error)
{
	EAccount *account;
	CamelMimeMessage *forward;
	CamelStream *mem;
	CamelInternetAddress *addr;
	CamelFolder *out_folder;
	CamelMessageInfo *info;
	CamelMedium *medium;
	const gchar *from_address;
	const gchar *from_name;
	const gchar *header_name;
	struct _camel_header_raw *xev;
	gchar *subject;

	g_return_val_if_fail (folder != NULL, FALSE);
	g_return_val_if_fail (message != NULL, FALSE);
	g_return_val_if_fail (address != NULL, FALSE);

	if (!*address) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("No destination address provided, forward "
			  "of the message has been cancelled."));
		return FALSE;
	}

	account = em_utils_guess_account_with_recipients (message, folder);
	if (!account) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("No account found to use, forward of the "
			  "message has been cancelled."));
		return FALSE;
	}

	from_address = account->id->address;
	from_name = account->id->name;

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
	xev = mail_tool_remove_xevolution_headers (forward);
	camel_header_raw_clear (&xev);

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

	/* FIXME Pass a GCancellable. */
	e_mail_folder_append_message (
		out_folder, forward, info, G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) ms_forward_to_cb, session);

	camel_message_info_free (info);

	return TRUE;
}

static void
mail_session_get_socks_proxy (CamelSession *session,
                              const gchar *for_host,
                              gchar **host_ret,
                              gint *port_ret)
{
	EMailSession *mail_session;
	gchar *uri;

	g_return_if_fail (session != NULL);
	g_return_if_fail (for_host != NULL);
	g_return_if_fail (host_ret != NULL);
	g_return_if_fail (port_ret != NULL);

	mail_session = E_MAIL_SESSION (session);
	g_return_if_fail (mail_session != NULL);
	g_return_if_fail (mail_session->priv != NULL);

	*host_ret = NULL;
	*port_ret = 0;

	uri = g_strconcat ("socks://", for_host, NULL);

	if (e_proxy_require_proxy_for_uri (mail_session->priv->proxy, uri)) {
		SoupURI *suri;

		suri = e_proxy_peek_uri_for (mail_session->priv->proxy, uri);
		if (suri) {
			*host_ret = g_strdup (suri->host);
			*port_ret = suri->port;
		}
	}

	g_free (uri);
}

static gboolean
mail_session_authenticate_sync (CamelSession *session,
                                CamelService *service,
                                const gchar *mechanism,
                                GCancellable *cancellable,
                                GError **error)
{
	CamelServiceAuthType *authtype = NULL;
	CamelAuthenticationResult result;
	CamelProvider *provider;
	CamelSettings *settings;
	const gchar *password;
	guint32 password_flags;
	GError *local_error = NULL;

	/* Do not chain up.  Camel's default method is only an example for
	 * subclasses to follow.  Instead we mimic most of its logic here. */

	provider = camel_service_get_provider (service);
	settings = camel_service_get_settings (service);

	/* APOP is one case where a non-SASL mechanism name is passed, so
	 * don't bail if the CamelServiceAuthType struct comes back NULL. */
	if (mechanism != NULL)
		authtype = camel_sasl_authtype (mechanism);

	/* If the SASL mechanism does not involve a user
	 * password, then it gets one shot to authenticate. */
	if (authtype != NULL && !authtype->need_password) {
		result = camel_service_authenticate_sync (
			service, mechanism, cancellable, error);
		if (result == CAMEL_AUTHENTICATION_REJECTED)
			g_set_error (
				error, CAMEL_SERVICE_ERROR,
				CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
				_("%s authentication failed"), mechanism);
		return (result == CAMEL_AUTHENTICATION_ACCEPTED);
	}

	/* Some SASL mechanisms can attempt to authenticate without a
	 * user password being provided (e.g. single-sign-on credentials),
	 * but can fall back to a user password.  Handle that case next. */
	if (mechanism != NULL) {
		CamelProvider *provider;
		CamelSasl *sasl;
		const gchar *service_name;
		gboolean success = FALSE;

		provider = camel_service_get_provider (service);
		service_name = provider->protocol;

		/* XXX Would be nice if camel_sasl_try_empty_password_sync()
		 *     returned CamelAuthenticationResult so it's easier to
		 *     detect errors. */
		sasl = camel_sasl_new (service_name, mechanism, service);
		if (sasl != NULL) {
			success = camel_sasl_try_empty_password_sync (
				sasl, cancellable, &local_error);
			g_object_unref (sasl);
		}

		if (success)
			return TRUE;
	}

	/* Abort authentication if we got cancelled.
	 * Otherwise clear any errors and press on. */
	if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		return FALSE;

	g_clear_error (&local_error);

	password_flags = CAMEL_SESSION_PASSWORD_SECRET;

retry:
	password = camel_service_get_password (service);

	if (password == NULL) {
		CamelNetworkSettings *network_settings;
		const gchar *host;
		const gchar *user;
		gchar *prompt;
		gchar *new_passwd;

		network_settings = CAMEL_NETWORK_SETTINGS (settings);
		host = camel_network_settings_get_host (network_settings);
		user = camel_network_settings_get_user (network_settings);

		prompt = camel_session_build_password_prompt (
			provider->name, user, host);

		new_passwd = camel_session_get_password (
			session, service, prompt, "password",
			password_flags, &local_error);
		camel_service_set_password (service, new_passwd);
		password = camel_service_get_password (service);
		g_free (new_passwd);

		g_free (prompt);

		if (local_error != NULL) {
			g_propagate_error (error, local_error);
			return FALSE;
		}

		if (password == NULL) {
			g_set_error (
				error, CAMEL_SERVICE_ERROR,
				CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
				_("No password was provided"));
			return FALSE;
		}
	}

	result = camel_service_authenticate_sync (
		service, mechanism, cancellable, error);

	if (result == CAMEL_AUTHENTICATION_REJECTED) {
		password_flags |= CAMEL_SESSION_PASSWORD_REPROMPT;
		camel_service_set_password (service, NULL);
		goto retry;
	}

	return (result == CAMEL_AUTHENTICATION_ACCEPTED);
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
	object_class->notify = mail_session_notify;
	object_class->constructed = mail_session_constructed;

	session_class = CAMEL_SESSION_CLASS (class);
	session_class->add_service = mail_session_add_service;
	session_class->get_password = mail_session_get_password;
	session_class->forget_password = mail_session_forget_password;
	session_class->alert_user = mail_session_alert_user;
	session_class->get_filter_driver = mail_session_get_filter_driver;
	session_class->lookup_addressbook = mail_session_lookup_addressbook;
	session_class->forward_to = mail_session_forward_to;
	session_class->get_socks_proxy = mail_session_get_socks_proxy;
	session_class->authenticate_sync = mail_session_authenticate_sync;

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

	/* XXX This property can be removed once Evolution moves to
	 *     GSettings and can use transform functions when binding
	 *     properties to settings. */
	g_object_class_install_property (
		object_class,
		PROP_JUNK_FILTER_NAME,
		g_param_spec_string (
			"junk-filter-name",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
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
		0, /* struct offset */
		NULL, NULL, /* accumulator */
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/**
	 * EMailSession::store-added
	 * @session: the email session
	 * @store: the CamelStore
	 *
	 * Emitted when a store is added
	 **/
	signals[STORE_ADDED] = g_signal_new (
		"store-added",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		0, /* struct offset */
		NULL, NULL, /* accumulator */
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		CAMEL_TYPE_STORE);

	/**
	 * EMailSession::store-removed
	 * @session: the email session
	 * @store: the CamelStore
	 *
	 * Emitted when a store is removed 
	 **/
	signals[STORE_REMOVED] = g_signal_new (
		"store-removed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		0, /* struct offset */
		NULL, NULL, /* accumulator */
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		CAMEL_TYPE_STORE);

	camel_null_store_register_provider ();
}

static void
e_mail_session_init (EMailSession *session)
{
	GSettings *settings;
	GHashTable *junk_filters;

	junk_filters = g_hash_table_new (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal);

	session->priv = E_MAIL_SESSION_GET_PRIVATE (session);
	session->priv->junk_filters = junk_filters;
	session->priv->proxy = e_proxy_new ();

	session->priv->local_folders =
		g_ptr_array_new_with_free_func (
		(GDestroyNotify) g_object_unref);
	session->priv->local_folder_uris =
		g_ptr_array_new_with_free_func (
		(GDestroyNotify) g_free);

	/* Initialize the EAccount setup. */
	e_account_writable (NULL, E_ACCOUNT_SOURCE_SAVE_PASSWD);

	settings = g_settings_new ("org.gnome.evolution.mail");

	camel_session_set_check_junk (
		CAMEL_SESSION (session), g_settings_get_boolean (
		settings, "junk-check-incoming"));
	g_signal_connect (
		settings, "changed",
		G_CALLBACK (mail_session_check_junk_notify), session);

	mail_config_reload_junk_headers (session);

	e_proxy_setup_proxy (session->priv->proxy);

	g_object_unref (settings);
}

EMailSession *
e_mail_session_new (void)
{
	const gchar *user_data_dir;
	const gchar *user_cache_dir;

	user_data_dir = mail_session_get_data_dir ();
	user_cache_dir = mail_session_get_cache_dir ();

	return g_object_new (
		E_TYPE_MAIL_SESSION,
		"user-data-dir", user_data_dir,
		"user-cache-dir", user_cache_dir,
		NULL);
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

	return session->priv->local_store;
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

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);
	g_return_val_if_fail (service_uid != NULL, NULL);

	service = camel_session_get_service (
		CAMEL_SESSION (session), service_uid);

	if (!CAMEL_IS_STORE (service))
		return NULL;

	if (!em_utils_connect_service_sync (service, cancellable, error))
		return NULL;

	return camel_store_get_inbox_folder_sync (
		CAMEL_STORE (service), cancellable, error);
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

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);
	g_return_val_if_fail (service_uid != NULL, NULL);

	service = camel_session_get_service (
		CAMEL_SESSION (session), service_uid);

	if (!CAMEL_IS_STORE (service))
		return NULL;

	if (!em_utils_connect_service_sync (service, cancellable, error))
		return NULL;

	return camel_store_get_trash_folder_sync (
		CAMEL_STORE (service), cancellable, error);
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
	if (G_UNLIKELY (mail_data_dir == NULL))
		mail_data_dir = g_build_filename (
			e_get_user_data_dir (), "mail", NULL);

	return mail_data_dir;
}

const gchar *
mail_session_get_cache_dir (void)
{
	if (G_UNLIKELY (mail_cache_dir == NULL))
		mail_cache_dir = g_build_filename (
			e_get_user_cache_dir (), "mail", NULL);

	return mail_cache_dir;
}

const gchar *
mail_session_get_config_dir (void)
{
	if (G_UNLIKELY (mail_config_dir == NULL))
		mail_config_dir = g_build_filename (
			e_get_user_config_dir (), "mail", NULL);

	return mail_config_dir;
}

CamelStore *
e_mail_session_get_vfolder_store (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return session->priv->vfolder_store;
}

EMVFolderContext *
e_mail_session_create_vfolder_context (EMailSession *session)
{
	EMailSessionClass *class;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	class = E_MAIL_SESSION_GET_CLASS (session);
	g_return_val_if_fail (class->create_vfolder_context != NULL, NULL);

	return class->create_vfolder_context (session);
}

