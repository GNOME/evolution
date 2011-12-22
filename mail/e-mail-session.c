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

#include <gconf/gconf-client.h>

#ifdef HAVE_CANBERRA
#include <canberra-gtk.h>
#endif

#include <libedataserverui/e-passwords.h>
#include <libedataserver/e-flag.h>

#include "e-util/e-util.h"
#include "e-util/e-account-utils.h"
#include "e-util/e-alert-dialog.h"
#include "e-util/e-extensible.h"
#include "e-util/e-util-private.h"
#include "e-util/gconf-bridge.h"

#include "e-mail-folder-utils.h"
#include "e-mail-junk-filter.h"
#include "e-mail-local.h"
#include "e-mail-session.h"
#include "em-composer-utils.h"
#include "em-filter-context.h"
#include "em-filter-rule.h"
#include "em-utils.h"
#include "mail-config.h"
#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-send-recv.h"
#include "mail-tools.h"

#define E_MAIL_SESSION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SESSION, EMailSessionPrivate))

static guint session_check_junk_notify_id;
static guint session_gconf_proxy_id;

typedef struct _AsyncContext AsyncContext;

struct _EMailSessionPrivate {
	MailFolderCache *folder_cache;

	FILE *filter_logfile;
	GHashTable *junk_filters;
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
	PROP_JUNK_FILTER_NAME
};

static gchar *mail_data_dir;
static gchar *mail_config_dir;

#if 0
static MailMsgInfo ms_thread_info_dummy = { sizeof (MailMsg) };
#endif

G_DEFINE_TYPE_WITH_CODE (
	EMailSession,
	e_mail_session,
	CAMEL_TYPE_SESSION,
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

/* Support for CamelSession.alert_user() *************************************/

static gpointer user_message_dialog;
static GQueue user_message_queue = { NULL, NULL, 0 };

struct _user_message_msg {
	MailMsg base;

	CamelSessionAlertType type;
	gchar *prompt;
	EFlag *done;

	guint allow_cancel : 1;
	guint result : 1;
	guint ismain : 1;
};

static void user_message_exec (struct _user_message_msg *m,
                               GCancellable *cancellable,
                               GError **error);

static void
user_message_response_free (GtkDialog *dialog,
                            gint button)
{
	struct _user_message_msg *m = NULL;

	gtk_widget_destroy ((GtkWidget *) dialog);

	user_message_dialog = NULL;

	/* check for pendings */
	if (!g_queue_is_empty (&user_message_queue)) {
		GCancellable *cancellable;

		m = g_queue_pop_head (&user_message_queue);
		cancellable = e_activity_get_cancellable (m->base.activity);
		user_message_exec (m, cancellable, &m->base.error);
		mail_msg_unref (m);
	}
}

/* clicked, send back the reply */
static void
user_message_response (GtkDialog *dialog,
                       gint button,
                       struct _user_message_msg *m)
{
	/* if !allow_cancel, then we've already replied */
	if (m && m->allow_cancel) {
		m->result = button == GTK_RESPONSE_OK;
		e_flag_set (m->done);
	}

	user_message_response_free (dialog, button);
}

static void
user_message_exec (struct _user_message_msg *m,
                   GCancellable *cancellable,
                   GError **error)
{
	GtkWindow *parent;
	const gchar *error_type;

	if (!m->ismain && user_message_dialog != NULL) {
		g_queue_push_tail (&user_message_queue, mail_msg_ref (m));
		return;
	}

	switch (m->type) {
		case CAMEL_SESSION_ALERT_INFO:
			error_type = m->allow_cancel ?
				"mail:session-message-info-cancel" :
				"mail:session-message-info";
			break;
		case CAMEL_SESSION_ALERT_WARNING:
			error_type = m->allow_cancel ?
				"mail:session-message-warning-cancel" :
				"mail:session-message-warning";
			break;
		case CAMEL_SESSION_ALERT_ERROR:
			error_type = m->allow_cancel ?
				"mail:session-message-error-cancel" :
				"mail:session-message-error";
			break;
		default:
			error_type = NULL;
			g_return_if_reached ();
	}

	/* Pull in the active window from the shell to get a parent window */
	parent = e_shell_get_active_window (e_shell_get_default ());
	user_message_dialog = e_alert_dialog_new_for_args (
		parent, error_type, m->prompt, NULL);
	g_object_set (user_message_dialog, "resizable", TRUE, NULL);

	/* XXX This is a case where we need to be able to construct
	 *     custom EAlerts without a predefined XML definition. */
	if (m->ismain) {
		gint response;

		response = gtk_dialog_run (user_message_dialog);
		user_message_response (
			user_message_dialog, response, m);
	} else {
		gpointer user_data = m;

		if (!m->allow_cancel)
			user_data = NULL;

		g_signal_connect (
			user_message_dialog, "response",
			G_CALLBACK (user_message_response), user_data);
		gtk_widget_show (user_message_dialog);
	}
}

static void
user_message_free (struct _user_message_msg *m)
{
	g_free (m->prompt);
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

static gboolean
session_play_sound_cb (const gchar *filename)
{
#ifdef HAVE_CANBERRA
	if (filename != NULL && *filename != '\0')
		ca_context_play (
			ca_gtk_context_get (), 0,
			CA_PROP_MEDIA_FILENAME, filename,
			NULL);
	else
#endif
		gdk_beep ();

	return FALSE;
}

static void
session_play_sound (CamelFilterDriver *driver,
                    const gchar *filename,
                    gpointer user_data)
{
	g_idle_add_full (
		G_PRIORITY_DEFAULT_IDLE,
		(GSourceFunc) session_play_sound_cb,
		g_strdup (filename), (GDestroyNotify) g_free);
}

static void
session_system_beep (CamelFilterDriver *driver,
                     gpointer user_data)
{
	g_idle_add ((GSourceFunc) session_play_sound_cb, NULL);
}

static CamelFilterDriver *
main_get_filter_driver (CamelSession *session,
                        const gchar *type,
                        GError **error)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailSession *ms = E_MAIL_SESSION (session);
	CamelFilterDriver *driver;
	EFilterRule *rule = NULL;
	const gchar *config_dir;
	gchar *user, *system;
	GConfClient *client;
	ERuleContext *fc;

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	g_return_val_if_fail (E_IS_MAIL_BACKEND (shell_backend), NULL);

	client = gconf_client_get_default ();

	config_dir = mail_session_get_config_dir ();
	user = g_build_filename (config_dir, "filters.xml", NULL);
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	fc = (ERuleContext *) em_filter_context_new (
		E_MAIL_BACKEND (shell_backend));
	e_rule_context_load (fc, system, user);
	g_free (system);
	g_free (user);

	driver = camel_filter_driver_new (session);
	camel_filter_driver_set_folder_func (driver, get_folder, session);

	if (gconf_client_get_bool (client, "/apps/evolution/mail/filters/log", NULL)) {
		if (ms->priv->filter_logfile == NULL) {
			gchar *filename;

			filename = gconf_client_get_string (
				client, "/apps/evolution/mail/filters/logfile", NULL);
			if (filename) {
				ms->priv->filter_logfile = g_fopen (filename, "a+");
				g_free (filename);
			}
		}

		if (ms->priv->filter_logfile)
			camel_filter_driver_set_logfile (driver, ms->priv->filter_logfile);
	}

	camel_filter_driver_set_shell_func (driver, mail_execute_shell_command, NULL);
	camel_filter_driver_set_play_sound_func (driver, session_play_sound, NULL);
	camel_filter_driver_set_system_beep_func (driver, session_system_beep, NULL);

	if ((!strcmp (type, E_FILTER_SOURCE_INCOMING) ||
		!strcmp (type, E_FILTER_SOURCE_JUNKTEST))
		&& camel_session_get_check_junk (session)) {

		/* implicit junk check as 1st rule */
		camel_filter_driver_add_rule (
			driver, "Junk check", "(junk-test)",
			"(begin (set-system-flag \"junk\"))");
	}

	if (strcmp (type, E_FILTER_SOURCE_JUNKTEST) != 0) {
		GString *fsearch, *faction;

		fsearch = g_string_new ("");
		faction = g_string_new ("");

		if (!strcmp (type, E_FILTER_SOURCE_DEMAND))
			type = E_FILTER_SOURCE_INCOMING;

		/* add the user-defined rules next */
		while ((rule = e_rule_context_next_rule (fc, rule, type))) {
			g_string_truncate (fsearch, 0);
			g_string_truncate (faction, 0);

			/* skip disabled rules */
			if (!rule->enabled)
				continue;

			e_filter_rule_build_code (rule, fsearch);
			em_filter_rule_build_action (
				EM_FILTER_RULE (rule), faction);
			camel_filter_driver_add_rule (
				driver, rule->name,
				fsearch->str, faction->str);
		}

		g_string_free (fsearch, TRUE);
		g_string_free (faction, TRUE);
	}

	g_object_unref (fc);

	g_object_unref (client);

	return driver;
}

/* Support for CamelSession.forward_to () ************************************/

static guint preparing_flush = 0;

static gboolean
forward_to_flush_outbox_cb (EMailSession *session)
{
	g_return_val_if_fail (preparing_flush != 0, FALSE);

	preparing_flush = 0;
	mail_send (session);

	return FALSE;
}

static void
ms_forward_to_cb (CamelFolder *folder,
                  GAsyncResult *result,
                  EMailSession *session)
{
	GConfClient *client;

	/* FIXME Poor error handling. */
	if (!e_mail_folder_append_message_finish (folder, result, NULL, NULL))
		return;

	client = gconf_client_get_default ();

	/* do not call mail send immediately, just pile them all in the outbox */
	if (preparing_flush || gconf_client_get_bool (
		client, "/apps/evolution/mail/filters/flush-outbox", NULL)) {
		if (preparing_flush)
			g_source_remove (preparing_flush);

		preparing_flush = g_timeout_add_seconds (
			60, (GSourceFunc)
			forward_to_flush_outbox_cb, session);
	}

	g_object_unref (client);
}

/* Support for SOCKS proxy ***************************************************/

static GSettings *proxy_settings = NULL, *proxy_socks_settings = NULL;

static void
set_socks_proxy_from_gsettings (CamelSession *session)
{
	gchar *mode, *host;
	gint port;

	g_return_if_fail (proxy_settings != NULL);
	g_return_if_fail (proxy_socks_settings != NULL);

	mode = g_settings_get_string (proxy_settings, "mode");
	if (g_strcmp0 (mode, "manual") == 0) {
		host = g_settings_get_string (proxy_socks_settings, "host");
		port = g_settings_get_int (proxy_socks_settings, "port");
		camel_session_set_socks_proxy (session, host, port);
		g_free (host);
	}
	g_free (mode);
}

static void
proxy_gsettings_changed_cb (GSettings *settings,
			    const gchar *key,
			    CamelSession *session)
{
	set_socks_proxy_from_gsettings (session);
}

static void
set_socks_proxy_gsettings_watch (CamelSession *session)
{
	g_return_if_fail (proxy_settings != NULL);
	g_return_if_fail (proxy_socks_settings != NULL);

	g_signal_connect (
		proxy_settings, "changed::mode",
		G_CALLBACK (proxy_gsettings_changed_cb), session);

	g_signal_connect (
		proxy_socks_settings, "changed",
		G_CALLBACK (proxy_gsettings_changed_cb), session);
}

static void
init_socks_proxy (CamelSession *session)
{
	g_return_if_fail (CAMEL_IS_SESSION (session));

	if (!proxy_settings) {
		proxy_settings = g_settings_new ("org.gnome.system.proxy");
		proxy_socks_settings = g_settings_get_child (proxy_settings, "socks");
		g_object_weak_ref (G_OBJECT (proxy_settings), (GWeakNotify) g_nullify_pointer, &proxy_settings);
		g_object_weak_ref (G_OBJECT (proxy_socks_settings), (GWeakNotify) g_nullify_pointer, &proxy_socks_settings);
	} else {
		g_object_ref (proxy_settings);
		g_object_ref (proxy_socks_settings);
	}

	set_socks_proxy_gsettings_watch (session);
	set_socks_proxy_from_gsettings (session);
}

/*****************************************************************************/

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

	if (service != NULL)
		key = camel_url_to_string (
			camel_service_get_camel_url (service),
			CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
	else
		key = g_strdup (item);

	return key;
}

static void
mail_session_check_junk_notify (GConfClient *gconf,
                                guint id,
                                GConfEntry *entry,
                                CamelSession *session)
{
	gchar *key;

	g_return_if_fail (gconf_entry_get_key (entry) != NULL);
	g_return_if_fail (gconf_entry_get_value (entry) != NULL);

	key = strrchr (gconf_entry_get_key (entry), '/');
	if (key) {
		key++;
		if (strcmp (key, "check_incoming") == 0)
			camel_session_set_check_junk (
				session, gconf_value_get_bool (
				gconf_entry_get_value (entry)));
	}
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

	return "";  /* GConfBridge doesn't like NULL strings */
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
				"'%s' in GConf", junk_filter_name);
		}
	}

	camel_session_set_junk_filter (CAMEL_SESSION (session), junk_filter);

	/* XXX We emit the "notify" signal in mail_session_notify(). */
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

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_session_parent_class)->dispose (object);
}

static void
mail_session_finalize (GObject *object)
{
	EMailSessionPrivate *priv;
	GConfClient *client;

	priv = E_MAIL_SESSION_GET_PRIVATE (object);

	g_hash_table_destroy (priv->junk_filters);

	client = gconf_client_get_default ();

	if (session_check_junk_notify_id != 0) {
		gconf_client_notify_remove (client, session_check_junk_notify_id);
		session_check_junk_notify_id = 0;
	}

	if (session_gconf_proxy_id != 0) {
		gconf_client_notify_remove (client, session_gconf_proxy_id);
		session_gconf_proxy_id = 0;
	}

	g_object_unref (client);

	g_free (mail_data_dir);
	g_free (mail_config_dir);

	if (proxy_settings) {
		g_signal_handlers_disconnect_by_func (proxy_settings, proxy_gsettings_changed_cb, CAMEL_SESSION (object));
		g_object_unref (proxy_settings);
	}

	if (proxy_socks_settings) {
		g_signal_handlers_disconnect_by_func (proxy_socks_settings, proxy_gsettings_changed_cb, CAMEL_SESSION (object));
		g_object_unref (proxy_socks_settings);
	}

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

static void
mail_session_constructed (GObject *object)
{
	EMailSessionPrivate *priv;
	EExtensible *extensible;
	GType extension_type;
	GList *list, *iter;

	priv = E_MAIL_SESSION_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_session_parent_class)->constructed (object);

	extensible = E_EXTENSIBLE (object);
	e_extensible_load_extensions (extensible);

	/* Add junk filter extensions to an internal hash table. */

	extension_type = E_TYPE_MAIL_JUNK_FILTER;
	list = e_extensible_list_extensions (extensible, extension_type);

	for (iter = list; iter != NULL; iter = g_list_next (iter)) {
		EMailJunkFilter *junk_filter;
		EMailJunkFilterClass *class;

		junk_filter = E_MAIL_JUNK_FILTER (iter->data);
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
			priv->junk_filters,
			(gpointer) class->filter_name,
			junk_filter);
	}

	g_list_free (list);

	/* Bind the "/apps/evolution/mail/junk/default_plugin"
	 * GConf key to our "junk-filter-name" property. */

	gconf_bridge_bind_property (
		gconf_bridge_get (),
		"/apps/evolution/mail/junk/default_plugin",
		object, "junk-filter-name");
}

static CamelService *
mail_session_add_service (CamelSession *session,
                          const gchar *uid,
                          const gchar *url_string,
                          CamelProviderType type,
                          GError **error)
{
	CamelService *service;

	/* Chain up to parents add_service() method. */
	service = CAMEL_SESSION_CLASS (e_mail_session_parent_class)->
		add_service (session, uid, url_string, type, error);

	/* Initialize the CamelSettings object from CamelURL parameters.
	 * This is temporary; soon we'll read settings from key files. */

	if (CAMEL_IS_SERVICE (service)) {
		CamelSettings *settings;
		CamelURL *url;

		settings = camel_service_get_settings (service);
		url = camel_service_get_camel_url (service);
		camel_settings_load_from_url (settings, url);
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

static gboolean
mail_session_alert_user (CamelSession *session,
                         CamelSessionAlertType type,
                         const gchar *prompt,
                         gboolean cancel)
{
	struct _user_message_msg *m;
	GCancellable *cancellable;
	gboolean result = TRUE;

	m = mail_msg_new (&user_message_info);
	m->ismain = mail_in_main_thread ();
	m->type = type;
	m->prompt = g_strdup (prompt);
	m->done = e_flag_new ();
	m->allow_cancel = cancel;

	if (cancel)
		mail_msg_ref (m);

	cancellable = e_activity_get_cancellable (m->base.activity);

	if (m->ismain)
		user_message_exec (m, cancellable, &m->base.error);
	else
		mail_msg_main_loop_push (m);

	if (cancel) {
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
	camel_internet_address_add (
		addr, account->id->name, account->id->address);
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
	out_folder = e_mail_local_get_folder (E_MAIL_LOCAL_FOLDER_OUTBOX);
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
}

static void
e_mail_session_init (EMailSession *session)
{
	GConfClient *client;

	session->priv = E_MAIL_SESSION_GET_PRIVATE (session);
	session->priv->folder_cache = mail_folder_cache_new ();
	session->priv->junk_filters = g_hash_table_new (
		(GHashFunc) g_str_hash, (GEqualFunc) g_str_equal);

	/* Initialize the EAccount setup. */
	e_account_writable (NULL, E_ACCOUNT_SOURCE_SAVE_PASSWD);

	client = gconf_client_get_default ();

	gconf_client_add_dir (
		client, "/apps/evolution/mail/junk",
		GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	camel_session_set_check_junk (
		CAMEL_SESSION (session), gconf_client_get_bool (
		client, "/apps/evolution/mail/junk/check_incoming", NULL));
	session_check_junk_notify_id = gconf_client_notify_add (
		client, "/apps/evolution/mail/junk",
		(GConfClientNotifyFunc) mail_session_check_junk_notify,
		session, NULL, NULL);

	mail_config_reload_junk_headers (session);

	init_socks_proxy (CAMEL_SESSION (session));

	g_object_unref (client);
}

EMailSession *
e_mail_session_new (void)
{
	const gchar *user_data_dir;

	user_data_dir = mail_session_get_data_dir ();

	return g_object_new (
		E_TYPE_MAIL_SESSION,
		"user-data-dir", user_data_dir, NULL);
}

MailFolderCache *
e_mail_session_get_folder_cache (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return session->priv->folder_cache;
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

	if (error != NULL) {
		g_simple_async_result_set_from_error (simple, error);
		g_error_free (error);
	}
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

	if (error != NULL) {
		g_simple_async_result_set_from_error (simple, error);
		g_error_free (error);
	}
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

	if (error != NULL) {
		g_simple_async_result_set_from_error (simple, error);
		g_error_free (error);
	}
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
mail_session_get_config_dir (void)
{
	if (G_UNLIKELY (mail_config_dir == NULL))
		mail_config_dir = g_build_filename (
			e_get_user_config_dir (), "mail", NULL);

	return mail_config_dir;
}

