/*
 * e-mail-ui-session.c
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

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#ifdef HAVE_CANBERRA
#include <canberra-gtk.h>
#endif

#include <libebackend/libebackend.h>

#include "e-mail-account-store.h"

#include "e-util/e-util.h"
#include "e-util/e-util-private.h"

#include "shell/e-shell.h"
#include "shell/e-shell-view.h"
#include "shell/e-shell-content.h"
#include "shell/e-shell-window.h"

#include "e-mail-ui-session.h"
#include "em-composer-utils.h"
#include "em-filter-context.h"
#include "em-vfolder-editor-context.h"
#include "em-filter-rule.h"
#include "em-utils.h"
#include "mail-send-recv.h"

#ifdef HAVE_CANBERRA
static ca_context *cactx = NULL;
#endif

static gint eca_debug = -1;

typedef struct _SourceContext SourceContext;

struct _EMailUISessionPrivate {
	FILE *filter_logfile;
	ESourceRegistry *registry;
	EMailAccountStore *account_store;
	EMailLabelListStore *label_store;
	EPhotoCache *photo_cache;
	gboolean check_junk;

	GSList *address_cache; /* data is AddressCacheData struct */
	GMutex address_cache_mutex;
};

enum {
	PROP_0,
	PROP_ACCOUNT_STORE,
	PROP_CHECK_JUNK,
	PROP_LABEL_STORE,
	PROP_PHOTO_CACHE
};

enum {
	ACTIVITY_ADDED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (EMailUISession, e_mail_ui_session, E_TYPE_MAIL_SESSION,
	G_ADD_PRIVATE (EMailUISession)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

struct _SourceContext {
	EMailUISession *session;
	CamelService *service;
};

typedef struct _AddressCacheData {
	gchar *email_address;
	gint64 stamp; /* when it was added to cache, in microseconds */
	gboolean is_known;
} AddressCacheData;

static void
address_cache_data_free (gpointer pdata)
{
	AddressCacheData *data = pdata;

	if (data) {
		g_free (data->email_address);
		g_free (data);
	}
}

static GSList *
address_cache_data_remove_old_and_test (GSList *items,
					const gchar *email_address,
					gboolean *found,
					gboolean *is_known)
{
	gint64 old_when;
	GSList *iter, *prev = NULL;

	if (!items)
		return NULL;

	/* let the cache value live for 5 minutes */
	old_when = g_get_real_time () - 5 * 60 * 1000 * 1000;

	for (iter = items; iter; prev = iter, iter = iter->next) {
		AddressCacheData *data = iter->data;

		if (!data || data->stamp <= old_when || !data->email_address)
			break;

		if (g_ascii_strcasecmp (email_address, data->email_address) == 0) {
			*found = TRUE;
			*is_known = data->is_known;

			/* a match was found, shorten the list later */
			return items;
		}
	}

	g_slist_free_full (iter, address_cache_data_free);
	if (prev)
		prev->next = NULL;
	else
		items = NULL;

	return items;
}

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
	if (eca_debug == -1)
		eca_debug = g_strcmp0 (g_getenv ("ECA_DEBUG"), "1") == 0 ? 1 : 0;

#ifdef HAVE_CANBERRA
	if (filename && *filename) {
		gint err;

		if (!cactx) {
			ca_context_create (&cactx);
			ca_context_change_props (cactx,
				CA_PROP_APPLICATION_NAME, "Evolution",
				NULL);
		}

		err = ca_context_play (
			cactx, 0,
			CA_PROP_MEDIA_FILENAME, filename,
			NULL);

		if (eca_debug) {
			if (err != 0)
				e_util_debug_print ("ECA", "Session Play Sound: Failed to play '%s': %s\n", filename, ca_strerror (err));
			else
				e_util_debug_print ("ECA", "Session Play Sound: Played file '%s'\n", filename);
		}
	} else
#else
	if (eca_debug)
		e_util_debug_print ("ECA", "Session Play Sound: Cannot play sound, not compiled with libcanberra\n");
#endif
		gdk_display_beep (gdk_display_get_default ());

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

static gboolean
session_folder_can_filter_junk (CamelFolder *folder)
{
	if (!folder)
		return TRUE;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), TRUE);

	return (camel_folder_get_flags (folder) & CAMEL_FOLDER_FILTER_JUNK) != 0;
}

static CamelFilterDriver *
main_get_filter_driver (CamelSession *session,
			const gchar *type,
			CamelFolder *for_folder,
			GError **error)
{
	EMailSession *ms = E_MAIL_SESSION (session);
	EMailUISession *self = E_MAIL_UI_SESSION (session);
	CamelFilterDriver *driver;
	EFilterRule *rule = NULL;
	const gchar *config_dir;
	gchar *user, *system;
	GSettings *settings;
	ERuleContext *fc;
	gboolean add_junk_test;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	config_dir = mail_session_get_config_dir ();
	user = g_build_filename (config_dir, "filters.xml", NULL);
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	fc = (ERuleContext *) em_filter_context_new (ms);
	e_rule_context_load (fc, system, user);
	g_free (system);
	g_free (user);

	driver = camel_filter_driver_new (session);
	camel_filter_driver_set_folder_func (driver, get_folder, session);

	if (g_settings_get_boolean (settings, "filters-log-actions") ||
	    camel_debug ("filters")) {
		if (!self->priv->filter_logfile &&
		    g_settings_get_boolean (settings, "filters-log-actions")) {
			gchar *filename;

			filename = g_settings_get_string (settings, "filters-log-file");
			if (filename) {
				if (!*filename || g_strcmp0 (filename, "stdout") == 0)
					self->priv->filter_logfile = stdout;
				else
					self->priv->filter_logfile = g_fopen (filename, "a+");

				g_free (filename);
			}
		} else if (!self->priv->filter_logfile) {
			self->priv->filter_logfile = stdout;
		}

		if (self->priv->filter_logfile)
			camel_filter_driver_set_logfile (driver, self->priv->filter_logfile);
	}

	camel_filter_driver_set_shell_func (driver, mail_execute_shell_command, NULL);
	camel_filter_driver_set_play_sound_func (driver, session_play_sound, NULL);
	camel_filter_driver_set_system_beep_func (driver, session_system_beep, NULL);

	add_junk_test = g_str_equal (type, E_FILTER_SOURCE_JUNKTEST) || (
		self->priv->check_junk &&
		g_str_equal (type, E_FILTER_SOURCE_INCOMING) &&
		session_folder_can_filter_junk (for_folder));

	if (add_junk_test) {
		/* implicit junk check as 1st rule */
		camel_filter_driver_add_rule (
			driver, "Junk check", "(= (junk-test) 1)",
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
	g_object_unref (settings);

	return driver;
}

static void
source_context_free (SourceContext *context)
{
	if (context->session != NULL)
		g_object_unref (context->session);

	if (context->service != NULL)
		g_object_unref (context->service);

	g_slice_free (SourceContext, context);
}

static gboolean
mail_ui_session_add_service_cb (SourceContext *context)
{
	EMailAccountStore *store;

	/* The CamelService should be fully initialized by now. */
	store = e_mail_ui_session_get_account_store (context->session);
	e_mail_account_store_add_service (store, context->service);

	return FALSE;
}

static void
mail_ui_session_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHECK_JUNK:
			e_mail_ui_session_set_check_junk (
				E_MAIL_UI_SESSION (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_ui_session_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_STORE:
			g_value_set_object (
				value,
				e_mail_ui_session_get_account_store (
				E_MAIL_UI_SESSION (object)));
			return;

		case PROP_CHECK_JUNK:
			g_value_set_boolean (
				value,
				e_mail_ui_session_get_check_junk (
				E_MAIL_UI_SESSION (object)));
			return;

		case PROP_LABEL_STORE:
			g_value_set_object (
				value,
				e_mail_ui_session_get_label_store (
				E_MAIL_UI_SESSION (object)));
			return;

		case PROP_PHOTO_CACHE:
			g_value_set_object (
				value,
				e_mail_ui_session_get_photo_cache (
				E_MAIL_UI_SESSION (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_ui_session_dispose (GObject *object)
{
	EMailUISession *self = E_MAIL_UI_SESSION (object);

	g_clear_object (&self->priv->registry);

	if (self->priv->account_store != NULL) {
		e_mail_account_store_clear (self->priv->account_store);
		g_clear_object (&self->priv->account_store);
	}

	g_clear_object (&self->priv->label_store);
	g_clear_object (&self->priv->photo_cache);

	g_mutex_lock (&self->priv->address_cache_mutex);
	g_slist_free_full (self->priv->address_cache, address_cache_data_free);
	self->priv->address_cache = NULL;
	g_mutex_unlock (&self->priv->address_cache_mutex);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_ui_session_parent_class)->dispose (object);
}

static void
mail_ui_session_finalize (GObject *object)
{
	EMailUISession *self = E_MAIL_UI_SESSION (object);

	g_mutex_clear (&self->priv->address_cache_mutex);

#ifdef HAVE_CANBERRA
	g_clear_pointer (&cactx, ca_context_destroy);
#endif

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_ui_session_parent_class)->finalize (object);
}

static void
mail_ui_session_constructed (GObject *object)
{
	EMailUISession *self;
	EMFolderTreeModel *folder_tree_model;
	ESourceRegistry *registry;
	EClientCache *client_cache;
	EMailSession *session;
	EShell *shell;

	session = E_MAIL_SESSION (object);
	shell = e_shell_get_default ();

	/* synchronize online state first, before any CamelService is created */
	e_binding_bind_property (
		shell, "online",
		session, "online",
		G_BINDING_SYNC_CREATE);

	self = E_MAIL_UI_SESSION (object);
	self->priv->account_store = e_mail_account_store_new (session);

	/* Keep our own reference to the ESourceRegistry so we
	 * can easily disconnect signal handlers in dispose(). */
	registry = e_mail_session_get_registry (session);
	self->priv->registry = g_object_ref (registry);

	client_cache = e_shell_get_client_cache (shell);
	self->priv->photo_cache = e_photo_cache_new (client_cache);

	/* XXX Make sure the folder tree model is created before we
	 *     add built-in CamelStores so it gets signals from the
	 *     EMailAccountStore.
	 *
	 * XXX This is creating a circular reference.  Perhaps the
	 *     model should only hold a weak pointer to EMailSession?
	 *
	 * FIXME EMailSession should just own the default instance.
	 */
	folder_tree_model = em_folder_tree_model_get_default ();
	em_folder_tree_model_set_session (folder_tree_model, session);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_ui_session_parent_class)->constructed (object);
}

static CamelService *
mail_ui_session_add_service (CamelSession *session,
                             const gchar *uid,
                             const gchar *protocol,
                             CamelProviderType type,
                             GError **error)
{
	CamelService *service;

	/* Chain up to parent's constructed() method. */
	service = CAMEL_SESSION_CLASS (e_mail_ui_session_parent_class)->
		add_service (session, uid, protocol, type, error);

	/* Inform the EMailAccountStore of the new CamelService
	 * from an idle callback so the service has a chance to
	 * fully initialize first. */
	if (CAMEL_IS_STORE (service)) {
		SourceContext *context;

		context = g_slice_new0 (SourceContext);
		context->session = E_MAIL_UI_SESSION (g_object_ref (session));
		context->service = g_object_ref (service);

		/* Prioritize ahead of GTK+ redraws. */
		g_idle_add_full (
			G_PRIORITY_HIGH_IDLE,
			(GSourceFunc) mail_ui_session_add_service_cb,
			context, (GDestroyNotify) source_context_free);
	}

	return service;
}

static void
mail_ui_session_remove_service (CamelSession *session,
                                CamelService *service)
{
	EMailAccountStore *store;
	EMailUISession *ui_session;

	/* Passing a NULL parent window skips confirmation prompts. */
	ui_session = E_MAIL_UI_SESSION (session);
	store = e_mail_ui_session_get_account_store (ui_session);
	e_mail_account_store_remove_service (store, NULL, service);
}

static CamelFilterDriver *
mail_ui_session_get_filter_driver (CamelSession *session,
				   const gchar *type,
				   CamelFolder *for_folder,
				   GError **error)
{
	return (CamelFilterDriver *) mail_call_main (
		MAIL_CALL_p_pppp, G_CALLBACK (main_get_filter_driver),
		session, type, for_folder, error);
}

static gboolean
mail_ui_session_lookup_addressbook (CamelSession *session,
                                    const gchar *name)
{
	CamelInternetAddress *cia;
	gboolean known_address = FALSE;

	/* FIXME CamelSession's lookup_addressbook() method needs redone.
	 *       No GCancellable provided, no means of reporting an error. */

	if (!mail_config_get_lookup_book ())
		return FALSE;

	cia = camel_internet_address_new ();

	if (camel_address_decode (CAMEL_ADDRESS (cia), name) > 0) {
		GError *error = NULL;

		e_mail_ui_session_check_known_address_sync (
			E_MAIL_UI_SESSION (session), cia,
			mail_config_get_lookup_book_local_only (),
			NULL, &known_address, &error);

		if (error != NULL) {
			g_warning ("%s: %s", G_STRFUNC, error->message);
			g_error_free (error);
		}
	} else {
		g_warning (
			"%s: Failed to decode internet "
			"address '%s'", G_STRFUNC, name);
	}

	g_object_unref (cia);

	return known_address;
}

static gboolean
mail_ui_session_check_book_contains_sync (EMailUISession *ui_session,
					  ESource *source,
					  const gchar *email_address,
					  GCancellable *cancellable,
					  GError **error)
{
	EPhotoCache *photo_cache;
	EClientCache *client_cache;
	EClient *client;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_MAIL_UI_SESSION (ui_session), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);
	g_return_val_if_fail (email_address != NULL, FALSE);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	if (!e_source_get_enabled (source))
		return FALSE;

	/* XXX EPhotoCache holds a reference on EClientCache, which
	 *     we need.  EMailUISession should probably hold its own
	 *     EClientCache reference, but this will do for now. */
	photo_cache = e_mail_ui_session_get_photo_cache (ui_session);
	client_cache = e_photo_cache_ref_client_cache (photo_cache);

	client = e_client_cache_get_client_sync (
		client_cache, source,
		E_SOURCE_EXTENSION_ADDRESS_BOOK, (guint32) -1,
		cancellable, error);

	if (client != NULL) {
		success = e_book_client_contains_email_sync (
			E_BOOK_CLIENT (client), email_address,
			cancellable, error);

		g_object_unref (client);
	}

	g_object_unref (client_cache);

	return success;
}

static gboolean
mail_ui_session_addressbook_contains_sync (CamelSession *session,
					   const gchar *book_uid,
					   const gchar *email_address,
					   GCancellable *cancellable,
					   GError **error)
{
	EMailUISession *ui_session = E_MAIL_UI_SESSION (session);
	GError *local_error = NULL;
	GList *books = NULL, *link;
	gboolean found = FALSE;

	if (g_strcmp0 (book_uid, CAMEL_SESSION_BOOK_UID_ANY) == 0) {
		books = e_source_registry_list_enabled (ui_session->priv->registry, E_SOURCE_EXTENSION_ADDRESS_BOOK);
	} else if (g_strcmp0 (book_uid, CAMEL_SESSION_BOOK_UID_COMPLETION) == 0) {
		GList *next = NULL;

		books = e_source_registry_list_enabled (ui_session->priv->registry, E_SOURCE_EXTENSION_ADDRESS_BOOK);

		for (link = books; link; link = next) {
			ESource *source = link->data;

			next = g_list_next (link);

			if (e_source_has_extension (source, E_SOURCE_EXTENSION_AUTOCOMPLETE) &&
			    !e_source_autocomplete_get_include_me (E_SOURCE_AUTOCOMPLETE (e_source_get_extension (source, E_SOURCE_EXTENSION_AUTOCOMPLETE)))) {
				g_object_unref (source);
				books = g_list_delete_link (books, link);
			}
		}
	} else {
		ESource *source;

		source = e_source_registry_ref_source (ui_session->priv->registry, book_uid);
		if (source) {
			found = mail_ui_session_check_book_contains_sync (ui_session, source, email_address, cancellable, error);
			g_object_unref (source);
		} else {
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, _("Book '%s' not found"), book_uid);
		}
	}

	for (link = books; link && !found && !g_cancellable_is_cancelled (cancellable); link = g_list_next (link)) {
		ESource *source = link->data;

		/* use the error from the first book, if looking in more books */
		found = mail_ui_session_check_book_contains_sync (ui_session, source, email_address, cancellable, local_error ? NULL : &local_error);
	}

	g_list_free_full (books, g_object_unref);

	if (!found && local_error)
		g_propagate_error (error, local_error);
	else
		g_clear_error (&local_error);

	return found;
}

static void
mail_ui_session_user_alert (CamelSession *session,
                            CamelService *service,
                            CamelSessionAlertType type,
                            const gchar *alert_message)
{
	EAlert *alert;
	EShell *shell;
	const gchar *alert_tag;
	gchar *display_name;

	shell = e_shell_get_default ();

	switch (type) {
		case CAMEL_SESSION_ALERT_INFO:
			alert_tag = "mail:user-alert-info";
			break;
		case CAMEL_SESSION_ALERT_WARNING:
			alert_tag = "mail:user-alert-warning";
			break;
		case CAMEL_SESSION_ALERT_ERROR:
			alert_tag = "mail:user-alert-error";
			break;
		default:
			g_return_if_reached ();
	}

	display_name = camel_service_dup_display_name (service);

	/* Just submit the alert to the EShell rather than hunting for
	 * a suitable window.  This will post it to all shell windows in
	 * all views, but if it's coming from the server then it must be
	 * important... right? */
	alert = e_alert_new (alert_tag, display_name, alert_message, NULL);
	e_shell_submit_alert (shell, alert);
	g_object_unref (alert);

	g_free (display_name);
}

static gpointer
mail_ui_session_call_trust_prompt_in_main_thread_cb (const gchar *source_extension,
						     const gchar *source_display_name,
						     const gchar *host,
						     const gchar *certificate_pem,
						     gconstpointer pcertificate_errors)
{
	EShell *shell;
	ETrustPromptResponse prompt_response;

	shell = e_shell_get_default ();

	prompt_response = e_trust_prompt_run_modal (gtk_application_get_active_window (GTK_APPLICATION (shell)),
		source_extension, source_display_name, host, certificate_pem, GPOINTER_TO_UINT (pcertificate_errors), NULL);

	return GINT_TO_POINTER (prompt_response);
}

static CamelCertTrust
mail_ui_session_trust_prompt (CamelSession *session,
			      CamelService *service,
			      GTlsCertificate *certificate,
			      GTlsCertificateFlags errors)
{
	CamelSettings *settings;
	CamelCertTrust response;
	gchar *host, *certificate_pem = NULL;
	ETrustPromptResponse prompt_response;
	const gchar *source_extension;

	settings = camel_service_ref_settings (service);
	g_return_val_if_fail (CAMEL_IS_NETWORK_SETTINGS (settings), 0);
	host = camel_network_settings_dup_host (
		CAMEL_NETWORK_SETTINGS (settings));
	g_object_unref (settings);

	/* XXX No accessor function for this property. */
	g_object_get (certificate, "certificate-pem", &certificate_pem, NULL);
	g_return_val_if_fail (certificate_pem != NULL, 0);

	if (CAMEL_IS_TRANSPORT (service))
		source_extension = E_SOURCE_EXTENSION_MAIL_TRANSPORT;
	else
		source_extension = E_SOURCE_EXTENSION_MAIL_ACCOUNT;

	prompt_response = GPOINTER_TO_INT (mail_call_main (MAIL_CALL_p_ppppp,
                G_CALLBACK (mail_ui_session_call_trust_prompt_in_main_thread_cb),
                source_extension, camel_service_get_display_name (service), host, certificate_pem, GUINT_TO_POINTER (errors)));

	g_free (certificate_pem);
	g_free (host);

	switch (prompt_response) {
		case E_TRUST_PROMPT_RESPONSE_REJECT:
			response = CAMEL_CERT_TRUST_NEVER;
			break;
		case E_TRUST_PROMPT_RESPONSE_ACCEPT:
			response = CAMEL_CERT_TRUST_FULLY;
			break;
		case E_TRUST_PROMPT_RESPONSE_ACCEPT_TEMPORARILY:
			response = CAMEL_CERT_TRUST_TEMPORARY;
			break;
		default:
			response = CAMEL_CERT_TRUST_UNKNOWN;
			break;
	}

	return response;
}

CamelCertTrust
e_mail_ui_session_trust_prompt (CamelSession *session,
                                CamelService *service,
                                GTlsCertificate *certificate,
                                GTlsCertificateFlags errors)
{
	return mail_ui_session_trust_prompt (session, service, certificate, errors);
}

typedef struct _TryCredentialsData {
	CamelService *service;
	const gchar *mechanism;
} TryCredentialsData;

static gboolean
mail_ui_session_try_credentials_sync (ECredentialsPrompter *prompter,
				      ESource *source,
				      const ENamedParameters *credentials,
				      gboolean *out_authenticated,
				      gpointer user_data,
				      GCancellable *cancellable,
				      GError **error)
{
	TryCredentialsData *data = user_data;
	gchar *credential_name = NULL;
	CamelAuthenticationResult result;

	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);
	g_return_val_if_fail (credentials != NULL, FALSE);
	g_return_val_if_fail (out_authenticated != NULL, FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (CAMEL_IS_SERVICE (data->service), FALSE);

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION)) {
		ESourceAuthentication *auth_extension;

		auth_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);
		credential_name = e_source_authentication_dup_credential_name (auth_extension);

		if (!credential_name || !*credential_name) {
			g_free (credential_name);
			credential_name = NULL;
		}
	}

	camel_service_set_password (data->service, e_named_parameters_get (credentials,
		credential_name ? credential_name : E_SOURCE_CREDENTIAL_PASSWORD));

	g_free (credential_name);

	result = camel_service_authenticate_sync (data->service, data->mechanism, cancellable, error);

	*out_authenticated = result == CAMEL_AUTHENTICATION_ACCEPTED;

	if (*out_authenticated) {
		ESourceCredentialsProvider *credentials_provider;
		ESource *cred_source;

		credentials_provider = e_credentials_prompter_get_provider (prompter);
		cred_source = e_source_credentials_provider_ref_credentials_source (credentials_provider, source);

		if (cred_source)
			e_source_invoke_authenticate_sync (cred_source, credentials, cancellable, NULL);

		g_clear_object (&cred_source);
	}

	return result == CAMEL_AUTHENTICATION_REJECTED;
}

static gboolean
mail_ui_session_authenticate_sync (CamelSession *session,
				   CamelService *service,
				   const gchar *mechanism,
				   GCancellable *cancellable,
				   GError **error)
{
	ESource *source;
	ESourceRegistry *registry;
	CamelServiceAuthType *authtype = NULL;
	CamelAuthenticationResult result;
	const gchar *uid;
	gboolean authenticated;
	gboolean try_empty_password = FALSE;
	GError *local_error = NULL;

	/* Do not chain up.  Camel's default method is only an example for
	 * subclasses to follow.  Instead we mimic most of its logic here. */

	registry = e_mail_session_get_registry (E_MAIL_SESSION (session));

	/* Treat a mechanism name of "none" as NULL. */
	if (g_strcmp0 (mechanism, "none") == 0)
		mechanism = NULL;

	/* APOP is one case where a non-SASL mechanism name is passed, so
	 * don't bail if the CamelServiceAuthType struct comes back NULL. */
	if (mechanism != NULL)
		authtype = camel_sasl_authtype (mechanism);

	/* If the SASL mechanism does not involve a user
	 * password, then it gets one shot to authenticate. */
	if (authtype != NULL && !authtype->need_password) {
		result = camel_service_authenticate_sync (service, mechanism, cancellable, &local_error);

		if ((result == CAMEL_AUTHENTICATION_REJECTED ||
		    g_error_matches (local_error, CAMEL_SERVICE_ERROR, CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE)) &&
		    e_oauth2_services_is_oauth2_alias (e_source_registry_get_oauth2_services (registry), mechanism)) {
			EShell *shell;
			ECredentialsPrompter *credentials_prompter;
			TryCredentialsData data;

			g_clear_error (&local_error);

			shell = e_shell_get_default ();
			credentials_prompter = e_shell_get_credentials_prompter (shell);

			/* Find a matching ESource for this CamelService. */
			uid = camel_service_get_uid (service);
			source = e_source_registry_ref_source (registry, uid);

			if (!source) {
				g_set_error (
					error, CAMEL_SERVICE_ERROR,
					CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
					_("No data source found for UID “%s”"), uid);
				return FALSE;
			}

			data.service = service;
			data.mechanism = mechanism;

			if (e_credentials_prompter_loop_prompt_sync (credentials_prompter,
				source, E_CREDENTIALS_PROMPTER_PROMPT_FLAG_ALLOW_SOURCE_SAVE,
				mail_ui_session_try_credentials_sync, &data, cancellable, &local_error))
				result = CAMEL_AUTHENTICATION_ACCEPTED;
			else
				result = CAMEL_AUTHENTICATION_ERROR;
		}

		if (local_error)
			g_propagate_error (error, local_error);

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

		provider = camel_service_get_provider (service);
		service_name = provider->protocol;

		/* XXX Would be nice if camel_sasl_try_empty_password_sync()
		 *     returned the result in an "out" parameter so it's
		 *     easier to distinguish errors from a "no" answer.
		 * YYY There are precisely two states. Either we appear to
		 *     have credentials (although we don't yet know if the
		 *     server would *accept* them, of course). Or we don't
		 *     have any credentials, and we can't even try. There
		 *     is no middle ground.
		 *     N.B. For 'have credentials', read 'the ntlm_auth
		 *          helper exists and at first glance seems to
		 *          be responding sanely'. */
		sasl = camel_sasl_new (service_name, mechanism, service);
		if (sasl != NULL) {
			try_empty_password =
				camel_sasl_try_empty_password_sync (
				sasl, cancellable, &local_error);
			g_object_unref (sasl);
		}
	}

	/* Abort authentication if we got cancelled.
	 * Otherwise clear any errors and press on. */
	if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		return FALSE;

	g_clear_error (&local_error);

	/* Find a matching ESource for this CamelService. */
	uid = camel_service_get_uid (service);
	source = e_source_registry_ref_source (registry, uid);

	if (source == NULL) {
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
			_("No data source found for UID “%s”"), uid);
		return FALSE;
	}

	result = CAMEL_AUTHENTICATION_REJECTED;

	if (try_empty_password) {
		result = camel_service_authenticate_sync (
			service, mechanism, cancellable, error);
	}

	if (result == CAMEL_AUTHENTICATION_REJECTED) {
		/* We need a password, preferrably one cached in
		 * the keyring or else by interactive user prompt. */
		EShell *shell;
		ECredentialsPrompter *credentials_prompter;
		TryCredentialsData data;

		shell = e_shell_get_default ();
		credentials_prompter = e_shell_get_credentials_prompter (shell);

		data.service = service;
		data.mechanism = mechanism;

		authenticated = e_credentials_prompter_loop_prompt_sync (credentials_prompter,
			source, E_CREDENTIALS_PROMPTER_PROMPT_FLAG_ALLOW_SOURCE_SAVE,
			mail_ui_session_try_credentials_sync, &data, cancellable, error);
	} else {
		authenticated = (result == CAMEL_AUTHENTICATION_ACCEPTED);
	}

	g_object_unref (source);

	return authenticated;
}

static void
mail_ui_session_refresh_service (EMailSession *session,
                                 CamelService *service)
{
	if (!camel_application_is_exiting &&
	    camel_session_get_online (CAMEL_SESSION (session))) {
		mail_receive_service (service);
	}
}

static EMVFolderContext *
mail_ui_session_create_vfolder_context (EMailSession *session)
{
	return (EMVFolderContext *) em_vfolder_editor_context_new (session);
}

static void
e_mail_ui_session_class_init (EMailUISessionClass *class)
{
	GObjectClass *object_class;
	CamelSessionClass *session_class;
	EMailSessionClass *mail_session_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_ui_session_set_property;
	object_class->get_property = mail_ui_session_get_property;
	object_class->dispose = mail_ui_session_dispose;
	object_class->finalize = mail_ui_session_finalize;
	object_class->constructed = mail_ui_session_constructed;

	session_class = CAMEL_SESSION_CLASS (class);
	session_class->add_service = mail_ui_session_add_service;
	session_class->remove_service = mail_ui_session_remove_service;
	session_class->get_filter_driver = mail_ui_session_get_filter_driver;
	session_class->lookup_addressbook = mail_ui_session_lookup_addressbook;
	session_class->addressbook_contains_sync = mail_ui_session_addressbook_contains_sync;
	session_class->user_alert = mail_ui_session_user_alert;
	session_class->trust_prompt = mail_ui_session_trust_prompt;
	session_class->authenticate_sync = mail_ui_session_authenticate_sync;

	mail_session_class = E_MAIL_SESSION_CLASS (class);
	mail_session_class->create_vfolder_context = mail_ui_session_create_vfolder_context;
	mail_session_class->refresh_service = mail_ui_session_refresh_service;

	g_object_class_install_property (
		object_class,
		PROP_CHECK_JUNK,
		g_param_spec_boolean (
			"check-junk",
			"Check Junk",
			"Check if incoming messages are junk",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_LABEL_STORE,
		g_param_spec_object (
			"label-store",
			"Label Store",
			"Mail label store",
			E_TYPE_MAIL_LABEL_LIST_STORE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_PHOTO_CACHE,
		g_param_spec_object (
			"photo-cache",
			"Photo Cache",
			"Contact photo cache",
			E_TYPE_PHOTO_CACHE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	signals[ACTIVITY_ADDED] = g_signal_new (
		"activity-added",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailUISessionClass, activity_added),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_ACTIVITY);
}

static void
e_mail_ui_session_init (EMailUISession *session)
{
	session->priv = e_mail_ui_session_get_instance_private (session);
	g_mutex_init (&session->priv->address_cache_mutex);
	session->priv->label_store = e_mail_label_list_store_new ();
}

EMailSession *
e_mail_ui_session_new (ESourceRegistry *registry)
{
	const gchar *user_data_dir;
	const gchar *user_cache_dir;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	user_data_dir = mail_session_get_data_dir ();
	user_cache_dir = mail_session_get_cache_dir ();

	return g_object_new (
		E_TYPE_MAIL_UI_SESSION,
		"registry", registry,
		"user-data-dir", user_data_dir,
		"user-cache-dir", user_cache_dir,
		NULL);
}

EMailAccountStore *
e_mail_ui_session_get_account_store (EMailUISession *session)
{
	g_return_val_if_fail (E_IS_MAIL_UI_SESSION (session), NULL);

	return session->priv->account_store;
}

/**
 * e_mail_ui_session_get_check_junk:
 * @session: an #EMailUISession
 *
 * Returns whether to automatically check incoming messages for junk content.
 *
 * Returns: whether to check for junk messages
 **/
gboolean
e_mail_ui_session_get_check_junk (EMailUISession *session)
{
	g_return_val_if_fail (E_IS_MAIL_UI_SESSION (session), FALSE);

	return session->priv->check_junk;
}

/**
 * e_mail_ui_session_set_check_junk:
 * @session: an #EMailUISession
 * @check_junk: whether to check for junk messages
 *
 * Sets whether to automatically check incoming messages for junk content.
 **/
void
e_mail_ui_session_set_check_junk (EMailUISession *session,
                                  gboolean check_junk)
{
	g_return_if_fail (E_IS_MAIL_UI_SESSION (session));

	if (check_junk == session->priv->check_junk)
		return;

	session->priv->check_junk = check_junk;

	g_object_notify (G_OBJECT (session), "check-junk");
}

EMailLabelListStore *
e_mail_ui_session_get_label_store (EMailUISession *session)
{
	g_return_val_if_fail (E_IS_MAIL_UI_SESSION (session), NULL);

	return session->priv->label_store;
}

EPhotoCache *
e_mail_ui_session_get_photo_cache (EMailUISession *session)
{
	g_return_val_if_fail (E_IS_MAIL_UI_SESSION (session), NULL);

	return session->priv->photo_cache;
}

void
e_mail_ui_session_add_activity (EMailUISession *session,
                                EActivity *activity)
{
	g_return_if_fail (E_IS_MAIL_UI_SESSION (session));
	g_return_if_fail (E_IS_ACTIVITY (activity));

	g_signal_emit (session, signals[ACTIVITY_ADDED], 0, activity);
}

/* let's test in local books first */
static gint
sort_local_books_first_cb (gconstpointer a,
			   gconstpointer b)
{
	ESource *asource = (ESource *) a;
	ESource *bsource = (ESource *) b;
	ESourceBackend *abackend, *bbackend;

	abackend = e_source_get_extension (asource, E_SOURCE_EXTENSION_ADDRESS_BOOK);
	bbackend = e_source_get_extension (bsource, E_SOURCE_EXTENSION_ADDRESS_BOOK);

	if (g_strcmp0 (e_source_backend_get_backend_name (abackend), "local") == 0) {
		if (g_strcmp0 (e_source_backend_get_backend_name (bbackend), "local") == 0)
			return 0;

		return -1;
	}

	if (g_strcmp0 (e_source_backend_get_backend_name (bbackend), "local") == 0)
		return 1;

	return g_strcmp0 (e_source_backend_get_backend_name (abackend),
			  e_source_backend_get_backend_name (bbackend));
}

/**
 * e_mail_ui_session_check_known_address_sync:
 * @session: an #EMailUISession
 * @addr: a #CamelInternetAddress
 * @check_local_only: only check the builtin address book
 * @cancellable: optional #GCancellable object, or %NULL
 * @out_known_address: return location for the determination of
 *                     whether @addr is a known address
 * @error: return location for a #GError, or %NULL
 *
 * Determines whether @addr is a known email address by querying address
 * books for contacts with a matching email address.  If @check_local_only
 * is %TRUE then only the builtin address book is checked, otherwise all
 * enabled address books are checked.
 *
 * The result of the query is returned through the @out_known_address
 * boolean pointer, not through the return value.  The return value only
 * indicates whether the address book queries were completed successfully.
 * If an error occurred, the function sets @error and returns %FALSE.
 *
 * Returns: whether address books were successfully queried
 **/
gboolean
e_mail_ui_session_check_known_address_sync (EMailUISession *session,
                                            CamelInternetAddress *addr,
                                            gboolean check_local_only,
                                            GCancellable *cancellable,
                                            gboolean *out_known_address,
                                            GError **error)
{
	EPhotoCache *photo_cache;
	EClientCache *client_cache;
	ESourceRegistry *registry;
	GList *list, *link;
	const gchar *email_address = NULL;
	gboolean known_address = FALSE;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_MAIL_UI_SESSION (session), FALSE);
	g_return_val_if_fail (CAMEL_IS_INTERNET_ADDRESS (addr), FALSE);
	g_return_val_if_fail (camel_internet_address_get (addr, 0, NULL, &email_address), FALSE);
	g_return_val_if_fail (email_address != NULL, FALSE);

	g_mutex_lock (&session->priv->address_cache_mutex);

	session->priv->address_cache = address_cache_data_remove_old_and_test (
		session->priv->address_cache,
		email_address, &success, &known_address);

	if (success) {
		g_mutex_unlock (&session->priv->address_cache_mutex);

		if (out_known_address)
			*out_known_address = known_address;

		return success;
	}

	/* XXX EPhotoCache holds a reference on EClientCache, which
	 *     we need.  EMailUISession should probably hold its own
	 *     EClientCache reference, but this will do for now. */
	photo_cache = e_mail_ui_session_get_photo_cache (session);
	client_cache = e_photo_cache_ref_client_cache (photo_cache);
	registry = e_client_cache_ref_registry (client_cache);

	if (check_local_only) {
		ESource *source;

		source = e_source_registry_ref_builtin_address_book (registry);
		list = g_list_prepend (NULL, g_object_ref (source));
		g_object_unref (source);
	} else {
		list = e_source_registry_list_enabled (
			registry, E_SOURCE_EXTENSION_ADDRESS_BOOK);
		list = g_list_sort (list, sort_local_books_first_cb);
	}

	for (link = list; link != NULL && !g_cancellable_is_cancelled (cancellable); link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		EClient *client;
		GError *local_error = NULL;

		/* Skip disabled sources. */
		if (!e_source_get_enabled (source))
			continue;

		client = e_client_cache_get_client_sync (
			client_cache, source,
			E_SOURCE_EXTENSION_ADDRESS_BOOK, (guint32) -1,
			cancellable, &local_error);

		if (client == NULL) {
			/* ignore E_CLIENT_ERROR-s, no need to stop searching if one
			   of the books is temporarily unreachable or any such issue */
			if (local_error && local_error->domain == E_CLIENT_ERROR) {
				g_clear_error (&local_error);
				continue;
			}

			if (local_error)
				g_propagate_error (error, local_error);

			success = FALSE;
			break;
		}

		success = e_book_client_contains_email_sync (
			E_BOOK_CLIENT (client), email_address,
			cancellable, &local_error);

		g_object_unref (client);

		if (!success) {
			/* ignore book-specific errors here and continue with the next */
			g_clear_error (&local_error);
			continue;
		}

		known_address = TRUE;
		break;
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	g_object_unref (registry);
	g_object_unref (client_cache);

	if (success && out_known_address != NULL)
		*out_known_address = known_address;

	if (!g_cancellable_is_cancelled (cancellable)) {
		AddressCacheData *data = g_new0 (AddressCacheData, 1);

		data->email_address = g_strdup (email_address);
		data->stamp = g_get_real_time ();
		data->is_known = known_address;

		/* this makes the list sorted by time, from newest to oldest */
		session->priv->address_cache = g_slist_prepend (
			session->priv->address_cache, data);
	}

	g_mutex_unlock (&session->priv->address_cache_mutex);

	return success;
}

