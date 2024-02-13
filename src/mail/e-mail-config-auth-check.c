/*
 * e-mail-config-auth-check.c
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

#include <glib/gi18n-lib.h>

#include "e-util/e-util.h"
#include "mail/e-mail-config-service-page.h"

#include "e-mail-ui-session.h"

#include "e-mail-config-auth-check.h"

typedef struct _AsyncContext AsyncContext;

struct _EMailConfigAuthCheckPrivate {
	EMailConfigServiceBackend *backend;
	gchar *active_mechanism;

	GtkWidget *combo_box;  /* not referenced */
	gulong host_changed_id;
	CamelServiceAuthType *used_xoauth2;
};

struct _AsyncContext {
	EMailConfigAuthCheck *auth_check;
	CamelSession *temporary_session;
	EActivity *activity;
};

enum {
	PROP_0,
	PROP_ACTIVE_MECHANISM,
	PROP_BACKEND
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailConfigAuthCheck, e_mail_config_auth_check, GTK_TYPE_BOX)

static void
async_context_free (AsyncContext *async_context)
{
	if (async_context->auth_check != NULL)
		g_object_unref (async_context->auth_check);

	if (async_context->temporary_session != NULL)
		g_object_unref (async_context->temporary_session);

	if (async_context->activity != NULL)
		g_object_unref (async_context->activity);

	g_slice_free (AsyncContext, async_context);
}

static void
mail_config_auth_check_update_done_cb (GObject *source_object,
                                       GAsyncResult *result,
                                       gpointer user_data)
{
	AsyncContext *async_context = user_data;
	EMailConfigAuthCheck *auth_check;
	EAlertSink *alert_sink;
	GList *available_authtypes;
	GError *error = NULL;

	auth_check = async_context->auth_check;
	alert_sink = e_activity_get_alert_sink (async_context->activity);

	available_authtypes = camel_service_query_auth_types_finish (
		CAMEL_SERVICE (source_object), result, &error);

	if (e_activity_handle_cancellation (async_context->activity, error)) {
		g_warn_if_fail (available_authtypes == NULL);
		g_error_free (error);

	} else if (error != NULL) {
		g_warn_if_fail (available_authtypes == NULL);
		e_alert_submit (
			alert_sink,
			"mail:checking-service-error",
			error->message, NULL);
		g_error_free (error);

	} else {
		e_auth_combo_box_update_available (
			E_AUTH_COMBO_BOX (auth_check->priv->combo_box),
			available_authtypes);
		e_auth_combo_box_pick_highest_available (E_AUTH_COMBO_BOX (auth_check->priv->combo_box));

		g_list_free (available_authtypes);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (auth_check), TRUE);

	async_context_free (async_context);
}

static void
mail_config_auth_check_update (EMailConfigAuthCheck *auth_check)
{
	EActivity *activity;
	ESource *source;
	EMailConfigServicePage *page;
	EMailConfigServiceBackend *backend;
	EMailConfigServicePageClass *page_class;
	EMailConfigServiceBackendClass *backend_class;
	CamelService *service;
	CamelSession *session;
	CamelSettings *settings;
	GCancellable *cancellable;
	AsyncContext *async_context;
	gchar *temp_dir;
	GError *error = NULL;

	backend = e_mail_config_auth_check_get_backend (auth_check);
	page = e_mail_config_service_backend_get_page (backend);
	settings = e_mail_config_service_backend_get_settings (backend);
	source = e_mail_config_service_backend_get_source (backend);

	page_class = E_MAIL_CONFIG_SERVICE_PAGE_GET_CLASS (page);
	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_GET_CLASS (backend);

	temp_dir = e_mkdtemp ("evolution-auth-check-XXXXXX");

	/* Create a temporary session for our temporary service.
	 * Use the same temporary directory for "user-data-dir" and
	 * "user-cache-dir".  For our purposes it shouldn't matter. */
	session = g_object_new (
		CAMEL_TYPE_SESSION,
		"user-data-dir", temp_dir,
		"user-cache-dir", temp_dir,
		NULL);

	/* to be able to answer for invalid/self-signed server certificates */
	CAMEL_SESSION_GET_CLASS (session)->trust_prompt = e_mail_ui_session_trust_prompt;

	service = camel_session_add_service (
		session, "fake-uid",
		backend_class->backend_name,
		page_class->provider_type, &error);

	g_free (temp_dir);

	if (error != NULL) {
		g_warn_if_fail (service == NULL);
		e_alert_submit (
			E_ALERT_SINK (page),
			"mail:checking-service-error",
			error->message, NULL);
		g_error_free (error);
		return;
	}

	g_return_if_fail (CAMEL_IS_SERVICE (service));

	camel_service_set_settings (service, settings);

	/* Setup Proxy */
	if (source) {
		ESourceRegistry *registry;
		ESource *authentication_source;

		registry = e_mail_config_service_page_get_registry (e_mail_config_service_backend_get_page (backend));
		authentication_source = e_source_registry_find_extension (registry, source, E_SOURCE_EXTENSION_AUTHENTICATION);

		if (authentication_source) {
			GProxyResolver *proxy_resolver = NULL;
			ESourceAuthentication *extension;
			ESource *proxy_source = NULL;
			gchar *uid;

			extension = e_source_get_extension (authentication_source, E_SOURCE_EXTENSION_AUTHENTICATION);

			uid = e_source_authentication_dup_proxy_uid (extension);
			if (uid) {
				proxy_source = e_source_registry_ref_source (registry, uid);
				g_free (uid);
			}

			if (proxy_source) {
				proxy_resolver = G_PROXY_RESOLVER (proxy_source);
				if (!g_proxy_resolver_is_supported (proxy_resolver))
					proxy_resolver = NULL;
			}

			camel_service_set_proxy_resolver (service, proxy_resolver);

			g_clear_object (&authentication_source);
			g_clear_object (&proxy_source);
		}
	}

	activity = e_mail_config_activity_page_new_activity (
		E_MAIL_CONFIG_ACTIVITY_PAGE (page));
	cancellable = e_activity_get_cancellable (activity);
	e_activity_set_text (activity, _("Querying authentication types…"));

	gtk_widget_set_sensitive (GTK_WIDGET (auth_check), FALSE);

	async_context = g_slice_new (AsyncContext);
	async_context->auth_check = g_object_ref (auth_check);
	async_context->temporary_session = session;  /* takes ownership */
	async_context->activity = activity;          /* takes ownership */

	camel_service_query_auth_types (
		service, G_PRIORITY_DEFAULT, cancellable,
		mail_config_auth_check_update_done_cb, async_context);

	g_object_unref (service);
}

static void
mail_config_auth_check_clicked_cb (GtkButton *button,
                                   EMailConfigAuthCheck *auth_check)
{
	mail_config_auth_check_update (auth_check);
}

static void
mail_config_auth_check_init_mechanism (EMailConfigAuthCheck *auth_check)
{
	EMailConfigServiceBackend *backend;
	CamelProvider *provider;
	CamelSettings *settings;
	const gchar *auth_mechanism = NULL;

	/* Pick an initial active mechanism name by examining both
	 * the corresponding CamelNetworkSettings and CamelProvider. */

	backend = e_mail_config_auth_check_get_backend (auth_check);
	provider = e_mail_config_service_backend_get_provider (backend);
	settings = e_mail_config_service_backend_get_settings (backend);
	g_return_if_fail (CAMEL_IS_NETWORK_SETTINGS (settings));

	auth_mechanism =
		camel_network_settings_get_auth_mechanism (
		CAMEL_NETWORK_SETTINGS (settings));

	/* If CamelNetworkSettings does not have a mechanism name set,
	 * choose from the CamelProvider's list of supported mechanisms. */
	if (auth_mechanism == NULL && provider != NULL) {
		if (provider->authtypes != NULL) {
			CamelServiceAuthType *auth_type;
			auth_type = provider->authtypes->data;
			auth_mechanism = auth_type->authproto;
		}
	}

	if (auth_mechanism != NULL)
		e_mail_config_auth_check_set_active_mechanism (
			auth_check, auth_mechanism);
}

static void
mail_config_auth_check_host_changed_cb (CamelNetworkSettings *network_settings,
					GParamSpec *param,
					EMailConfigAuthCheck *auth_check)
{
	EMailConfigServiceBackend *backend;
	ESourceRegistry *registry;
	EOAuth2Service *oauth2_service;
	CamelProvider *provider;
	CamelServiceAuthType *change_authtype = NULL;

	g_return_if_fail (CAMEL_IS_NETWORK_SETTINGS (network_settings));
	g_return_if_fail (E_IS_MAIL_CONFIG_AUTH_CHECK (auth_check));

	backend = e_mail_config_auth_check_get_backend (auth_check);
	provider = e_mail_config_service_backend_get_provider (backend);

	registry = e_mail_config_service_page_get_registry (e_mail_config_service_backend_get_page (backend));
	oauth2_service = e_oauth2_services_find (e_source_registry_get_oauth2_services (registry),
		e_mail_config_service_backend_get_source (backend));
	if (!oauth2_service) {
		oauth2_service = e_oauth2_services_guess (e_source_registry_get_oauth2_services (registry),
			provider ? provider->protocol : NULL, camel_network_settings_get_host (network_settings));
	}

	if (oauth2_service)
		change_authtype = camel_sasl_authtype (e_oauth2_service_get_name (oauth2_service));

	g_clear_object (&oauth2_service);

	if (change_authtype != auth_check->priv->used_xoauth2) {
		if (auth_check->priv->used_xoauth2)
			e_auth_combo_box_remove_auth_type (E_AUTH_COMBO_BOX (auth_check->priv->combo_box), auth_check->priv->used_xoauth2);

		auth_check->priv->used_xoauth2 = change_authtype;

		if (auth_check->priv->used_xoauth2)
			e_auth_combo_box_add_auth_type (E_AUTH_COMBO_BOX (auth_check->priv->combo_box), auth_check->priv->used_xoauth2);
	}
}

static void
mail_config_auth_check_set_backend (EMailConfigAuthCheck *auth_check,
                                    EMailConfigServiceBackend *backend)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend));
	g_return_if_fail (auth_check->priv->backend == NULL);

	auth_check->priv->backend = g_object_ref (backend);
}

static void
mail_config_auth_check_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_MECHANISM:
			e_mail_config_auth_check_set_active_mechanism (
				E_MAIL_CONFIG_AUTH_CHECK (object),
				g_value_get_string (value));
			return;

		case PROP_BACKEND:
			mail_config_auth_check_set_backend (
				E_MAIL_CONFIG_AUTH_CHECK (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_auth_check_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE_MECHANISM:
			g_value_set_string (
				value,
				e_mail_config_auth_check_get_active_mechanism (
				E_MAIL_CONFIG_AUTH_CHECK (object)));
			return;

		case PROP_BACKEND:
			g_value_set_object (
				value,
				e_mail_config_auth_check_get_backend (
				E_MAIL_CONFIG_AUTH_CHECK (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_auth_check_dispose (GObject *object)
{
	EMailConfigAuthCheck *self = E_MAIL_CONFIG_AUTH_CHECK (object);

	if (self->priv->backend != NULL) {
		if (self->priv->host_changed_id) {
			CamelSettings *settings;

			settings = e_mail_config_service_backend_get_settings (self->priv->backend);
			if (settings)
				e_signal_disconnect_notify_handler (settings, &self->priv->host_changed_id);
		}

		g_clear_object (&self->priv->backend);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_auth_check_parent_class)->dispose (object);
}

static void
mail_config_auth_check_finalize (GObject *object)
{
	EMailConfigAuthCheck *self = E_MAIL_CONFIG_AUTH_CHECK (object);

	g_free (self->priv->active_mechanism);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_config_auth_check_parent_class)->finalize (object);
}

static void
mail_config_auth_check_constructed (GObject *object)
{
	EMailConfigAuthCheck *auth_check;
	EMailConfigServiceBackend *backend;
	CamelProvider *provider;
	CamelSettings *settings;
	GtkWidget *widget;
	const gchar *text;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_auth_check_parent_class)->constructed (object);

	auth_check = E_MAIL_CONFIG_AUTH_CHECK (object);
	backend = e_mail_config_auth_check_get_backend (auth_check);
	provider = e_mail_config_service_backend_get_provider (backend);

	text = _("Check for Supported Types");
	widget = gtk_button_new_with_label (text);
	gtk_box_pack_start (GTK_BOX (object), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (mail_config_auth_check_clicked_cb),
		auth_check);

	widget = e_auth_combo_box_new ();
	e_auth_combo_box_set_provider (E_AUTH_COMBO_BOX (widget), provider);
	gtk_box_pack_start (GTK_BOX (object), widget, FALSE, FALSE, 0);
	auth_check->priv->combo_box = widget;  /* do not reference */
	gtk_widget_show (widget);

	settings = e_mail_config_service_backend_get_settings (backend);
	if (CAMEL_IS_NETWORK_SETTINGS (settings)) {
		ESourceRegistry *registry;
		EOAuth2Service *oauth2_service;

		auth_check->priv->host_changed_id = e_signal_connect_notify (
			settings, "notify::host", G_CALLBACK (mail_config_auth_check_host_changed_cb), auth_check);

		registry = e_mail_config_service_page_get_registry (e_mail_config_service_backend_get_page (backend));
		oauth2_service = e_oauth2_services_find (e_source_registry_get_oauth2_services (registry),
			e_mail_config_service_backend_get_source (backend));
		if (!oauth2_service) {
			oauth2_service = e_oauth2_services_guess (e_source_registry_get_oauth2_services (registry),
				provider ? provider->protocol : NULL, camel_network_settings_get_host (CAMEL_NETWORK_SETTINGS (settings)));
		}

		if (oauth2_service)
			auth_check->priv->used_xoauth2 = camel_sasl_authtype (e_oauth2_service_get_name (oauth2_service));

		g_clear_object (&oauth2_service);

		if (auth_check->priv->used_xoauth2)
			e_auth_combo_box_add_auth_type (E_AUTH_COMBO_BOX (auth_check->priv->combo_box), auth_check->priv->used_xoauth2);
	}

	e_binding_bind_property (
		widget, "active-id",
		auth_check, "active-mechanism",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	mail_config_auth_check_init_mechanism (auth_check);
}

static void
e_mail_config_auth_check_class_init (EMailConfigAuthCheckClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_auth_check_set_property;
	object_class->get_property = mail_config_auth_check_get_property;
	object_class->dispose = mail_config_auth_check_dispose;
	object_class->finalize = mail_config_auth_check_finalize;
	object_class->constructed = mail_config_auth_check_constructed;

	g_object_class_install_property (
		object_class,
		PROP_ACTIVE_MECHANISM,
		g_param_spec_string (
			"active-mechanism",
			"Active Mechanism",
			"Active authentication mechanism",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_BACKEND,
		g_param_spec_object (
			"backend",
			"Backend",
			"Mail configuration backend",
			E_TYPE_MAIL_CONFIG_SERVICE_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_config_auth_check_init (EMailConfigAuthCheck *auth_check)
{
	auth_check->priv = e_mail_config_auth_check_get_instance_private (auth_check);
	auth_check->priv->host_changed_id = 0;
	auth_check->priv->used_xoauth2 = NULL;

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (auth_check),
		GTK_ORIENTATION_HORIZONTAL);

	gtk_box_set_spacing (GTK_BOX (auth_check), 6);
}

GtkWidget *
e_mail_config_auth_check_new (EMailConfigServiceBackend *backend)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_SERVICE_BACKEND (backend), NULL);

	return g_object_new (
		E_TYPE_MAIL_CONFIG_AUTH_CHECK,
		"backend", backend, NULL);
}

EMailConfigServiceBackend *
e_mail_config_auth_check_get_backend (EMailConfigAuthCheck *auth_check)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_AUTH_CHECK (auth_check), NULL);

	return auth_check->priv->backend;
}

const gchar *
e_mail_config_auth_check_get_active_mechanism (EMailConfigAuthCheck *auth_check)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_AUTH_CHECK (auth_check), NULL);

	return auth_check->priv->active_mechanism;
}

void
e_mail_config_auth_check_set_active_mechanism (EMailConfigAuthCheck *auth_check,
                                               const gchar *active_mechanism)
{
	g_return_if_fail (E_IS_MAIL_CONFIG_AUTH_CHECK (auth_check));

	if (g_strcmp0 (auth_check->priv->active_mechanism, active_mechanism) == 0)
		return;

	g_free (auth_check->priv->active_mechanism);
	/* Do not allow NULL here, thus Password auth, which uses
	   empty string as ID, will match in the combo */
	auth_check->priv->active_mechanism = g_strdup (active_mechanism ? active_mechanism : "");

	g_object_notify (G_OBJECT (auth_check), "active-mechanism");
}
