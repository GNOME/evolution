/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <libedataserver/libedataserver.h>
#include <libedataserverui/libedataserverui.h>

#include "e-util/e-util.h"

#include "e-webdav-config-lookup.h"

/* Standard GObject macros */
#define E_TYPE_WEBDAV_CONFIG_LOOKUP \
	(e_webdav_config_lookup_get_type ())
#define E_WEBDAV_CONFIG_LOOKUP(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEBDAV_CONFIG_LOOKUP, EWebDAVConfigLookup))
#define E_WEBDAV_CONFIG_LOOKUP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_WEBDAV_CONFIG_LOOKUP, EWebDAVConfigLookupClass))
#define E_IS_WEBDAV_CONFIG_LOOKUP(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_WEBDAV_CONFIG_LOOKUP))
#define E_IS_WEBDAV_CONFIG_LOOKUP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_WEBDAV_CONFIG_LOOKUP))
#define E_WEBDAV_CONFIG_LOOKUP_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_WEBDAV_CONFIG_LOOKUP, EWebDAVConfigLookupClass))

typedef struct _EWebDAVConfigLookup EWebDAVConfigLookup;
typedef struct _EWebDAVConfigLookupClass EWebDAVConfigLookupClass;

struct _EWebDAVConfigLookup {
	EExtension parent;
};

struct _EWebDAVConfigLookupClass {
	EExtensionClass parent_class;
};

GType e_webdav_config_lookup_get_type (void) G_GNUC_CONST;

static void webdav_config_lookup_worker_iface_init (EConfigLookupWorkerInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EWebDAVConfigLookup, e_webdav_config_lookup, E_TYPE_EXTENSION, 0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (E_TYPE_CONFIG_LOOKUP_WORKER, webdav_config_lookup_worker_iface_init))

static void
webdav_config_lookup_to_result (EConfigLookup *config_lookup,
				const gchar *url,
				const ENamedParameters *params,
				const gchar *user,
				const gchar *certificate_trust,
				const GSList *discovered_sources)
{
	EConfigLookupResult *lookup_result;
	GSList *link;
	gboolean has_calendar = FALSE, has_contacts = FALSE;
	GString *description;

	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));

	for (link = (GSList *) discovered_sources; link && (!has_calendar || !has_contacts); link = g_slist_next (link)) {
		EWebDAVDiscoveredSource *discovered = link->data;

		if (!discovered)
			continue;

		has_calendar = has_calendar ||
			(discovered->supports & E_WEBDAV_DISCOVER_SUPPORTS_EVENTS) != 0 ||
			(discovered->supports & E_WEBDAV_DISCOVER_SUPPORTS_MEMOS) != 0 ||
			(discovered->supports & E_WEBDAV_DISCOVER_SUPPORTS_TASKS) != 0;

		has_contacts = has_contacts ||
			(discovered->supports & E_WEBDAV_DISCOVER_SUPPORTS_CONTACTS) != 0;
	}

	if (!has_calendar && !has_contacts)
		return;

	description = g_string_new ("");

	if (has_calendar) {
		if (description->len)
			g_string_append_c (description, '\n');

		g_string_append_printf (description, _("CalDAV: %s"), url);
	}

	if (has_contacts) {
		if (description->len)
			g_string_append_c (description, '\n');

		g_string_append_printf (description, _("CardDAV: %s"), url);
	}

	lookup_result = e_config_lookup_result_simple_new (E_CONFIG_LOOKUP_RESULT_COLLECTION,
		E_CONFIG_LOOKUP_RESULT_PRIORITY_POP3,
		TRUE, "webdav",
		has_calendar && has_contacts ? _("CalDAV and CardDAV server") :
		has_calendar ? _("CalDAV server") : _("CardDAV server"),
		description->str,
		params && e_named_parameters_exists (params, E_CONFIG_LOOKUP_PARAM_PASSWORD) &&
		e_named_parameters_exists (params, E_CONFIG_LOOKUP_PARAM_REMEMBER_PASSWORD) ?
		e_named_parameters_get (params, E_CONFIG_LOOKUP_PARAM_PASSWORD) : NULL);

	g_string_free (description, TRUE);

	e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_COLLECTION,
		"backend-name", "webdav");

	e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_COLLECTION,
		"identity", user);

	e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_AUTHENTICATION,
		"user", user);

	e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_AUTHENTICATION,
		"method", "plain/password");

	if (has_calendar) {
		e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_COLLECTION,
			"calendar-url", url);
	}

	if (has_contacts) {
		e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_COLLECTION,
			"contacts-url", url);
	}

	if (certificate_trust) {
		e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_WEBDAV_BACKEND,
			"ssl-trust", certificate_trust);
	}

	e_config_lookup_add_result (config_lookup, lookup_result);
}

static gboolean
webdav_config_lookup_propagate_error (GError **error,
				      GError *local_error,
				      const gchar *certificate_pem,
				      GTlsCertificateFlags certificate_errors,
				      gboolean *out_authentication_failed)
{
	if (g_error_matches (local_error, E_SOUP_SESSION_ERROR, SOUP_STATUS_UNAUTHORIZED)) {
		g_set_error_literal (error, E_CONFIG_LOOKUP_WORKER_ERROR, E_CONFIG_LOOKUP_WORKER_ERROR_REQUIRES_PASSWORD,
			_("Requires password to continue."));

		g_clear_error (&local_error);

		if (out_authentication_failed)
			*out_authentication_failed = TRUE;

		return TRUE;
	}

	if (g_error_matches (local_error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE) &&
	    certificate_pem && *certificate_pem && certificate_errors) {
		gchar *description = e_trust_prompt_describe_certificate_errors (certificate_errors);

		if (description) {
			g_set_error_literal (error, E_CONFIG_LOOKUP_WORKER_ERROR,
				E_CONFIG_LOOKUP_WORKER_ERROR_CERTIFICATE, description);

			g_clear_error (&local_error);
			g_free (description);
			return TRUE;
		}
	}

	return FALSE;
}

static void
webdav_config_lookup_set_host_from_url (ESourceAuthentication *authentication_extension,
					const gchar *url)
{
	GUri *guri = NULL;
	const gchar *host = NULL;

	g_return_if_fail (E_IS_SOURCE_AUTHENTICATION (authentication_extension));

	if (url) {
		guri = g_uri_parse (url, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
		if (guri)
			host = g_uri_get_host (guri);
	}

	e_source_authentication_set_host (authentication_extension, host);

	if (guri)
		g_uri_unref (guri);
}

static const gchar *
webdav_config_lookup_worker_get_display_name (EConfigLookupWorker *lookup_worker)
{
	return _("Look up for a CalDAV/CardDAV server");
}

static gboolean
webdav_config_lookup_discover (ESource *dummy_source,
			       const gchar *url,
			       ETrustPromptResponse trust_response,
			       GTlsCertificate *certificate,
			       EConfigLookup *config_lookup,
			       const ENamedParameters *params,
			       ENamedParameters **out_restart_params,
			       gboolean *out_authentication_failed,
			       GCancellable *cancellable,
			       GError **error)
{
	ESourceAuthentication *authentication_extension;
	ESourceWebdav *webdav_extension;
	ENamedParameters *credentials = NULL;
	GSList *discovered_sources = NULL;
	gchar *certificate_pem = NULL;
	GTlsCertificateFlags certificate_errors = 0;
	GError *local_error = NULL;
	gboolean should_stop = FALSE;

	if (out_authentication_failed)
		*out_authentication_failed = FALSE;

	if (e_named_parameters_exists (params, E_CONFIG_LOOKUP_PARAM_PASSWORD)) {
		credentials = e_named_parameters_new ();

		e_named_parameters_set (credentials, E_SOURCE_CREDENTIAL_PASSWORD,
			e_named_parameters_get (params, E_CONFIG_LOOKUP_PARAM_PASSWORD));
	}

	authentication_extension = e_source_get_extension (dummy_source, E_SOURCE_EXTENSION_AUTHENTICATION);
	webdav_extension = e_source_get_extension (dummy_source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);

	webdav_config_lookup_set_host_from_url (authentication_extension, url);

	if (certificate && trust_response != E_TRUST_PROMPT_RESPONSE_UNKNOWN)
		e_source_webdav_update_ssl_trust (webdav_extension, e_source_authentication_get_host (authentication_extension), certificate, trust_response);

	if (e_webdav_discover_sources_sync (dummy_source, url,
		E_WEBDAV_DISCOVER_SUPPORTS_NONE, credentials, &certificate_pem, &certificate_errors,
		&discovered_sources, NULL, cancellable, &local_error)) {
		webdav_config_lookup_to_result (config_lookup, url, params,
			e_source_authentication_get_user (authentication_extension),
			e_source_webdav_get_ssl_trust (webdav_extension), discovered_sources);
		e_webdav_discover_free_discovered_sources (discovered_sources);
		discovered_sources = NULL;
	} else if (webdav_config_lookup_propagate_error (error, local_error, certificate_pem, certificate_errors, out_authentication_failed)) {
		if (certificate_pem) {
			e_named_parameters_set (*out_restart_params, E_CONFIG_LOOKUP_PARAM_CERTIFICATE_PEM, certificate_pem);
			e_named_parameters_set (*out_restart_params, E_CONFIG_LOOKUP_PARAM_CERTIFICATE_HOST,
				e_source_authentication_get_host (authentication_extension));
		}

		should_stop = TRUE;
	} else {
		g_clear_error (&local_error);
	}

	g_clear_pointer (&certificate_pem, g_free);
	e_named_parameters_free (credentials);

	return should_stop;
}

static void
webdav_config_lookup_worker_run (EConfigLookupWorker *lookup_worker,
				 EConfigLookup *config_lookup,
				 const ENamedParameters *params,
				 ENamedParameters **out_restart_params,
				 GCancellable *cancellable,
				 GError **error)
{
	ESource *dummy_source;
	ESourceAuthentication *authentication_extension;
	ESourceWebdav *webdav_extension;
	ETrustPromptResponse trust_response = E_TRUST_PROMPT_RESPONSE_UNKNOWN;
	GTlsCertificate *certificate = NULL;
	gchar *email_address, *at_pos;
	const gchar *servers;
	gboolean should_stop = FALSE, authentication_failed = FALSE;

	g_return_if_fail (E_IS_WEBDAV_CONFIG_LOOKUP (lookup_worker));
	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	g_return_if_fail (params != NULL);
	g_return_if_fail (out_restart_params != NULL);

	email_address = g_strdup (e_named_parameters_get (params, E_CONFIG_LOOKUP_PARAM_EMAIL_ADDRESS));

	*out_restart_params = e_named_parameters_new_clone (params);

	dummy_source = e_source_new (NULL, NULL, NULL);
	e_source_set_display_name (dummy_source, "Dummy Source");

	webdav_extension = e_source_get_extension (dummy_source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
	e_source_webdav_set_display_name (webdav_extension, "Dummy Source");

	if (e_named_parameters_exists (params, E_CONFIG_LOOKUP_PARAM_CERTIFICATE_PEM) &&
	    e_named_parameters_exists (params, E_CONFIG_LOOKUP_PARAM_CERTIFICATE_TRUST)) {
		const gchar *certificate_pem;

		certificate_pem = e_named_parameters_get (params, E_CONFIG_LOOKUP_PARAM_CERTIFICATE_PEM);
		certificate = g_tls_certificate_new_from_pem (certificate_pem, -1, NULL);

		if (certificate) {
			trust_response = e_config_lookup_decode_certificate_trust (
				e_named_parameters_get (params, E_CONFIG_LOOKUP_PARAM_CERTIFICATE_TRUST));
		}
	}

	at_pos = strchr (email_address, '@');

	authentication_extension = e_source_get_extension (dummy_source, E_SOURCE_EXTENSION_AUTHENTICATION);
	if (e_named_parameters_exists (params, E_CONFIG_LOOKUP_PARAM_USER))
		e_source_authentication_set_user (authentication_extension, e_named_parameters_get (params, E_CONFIG_LOOKUP_PARAM_USER));
	else
		e_source_authentication_set_user (authentication_extension, email_address);

	/* Try to guess from an email domain only if no servers were given (because it is almost always wrong). */
	servers = e_named_parameters_get (params, E_CONFIG_LOOKUP_PARAM_SERVERS);

	if (at_pos && at_pos[1] && (!servers || !*servers)) {
		const gchar *host = at_pos + 1;
		gchar *url;

		/* Intentionally use the secure HTTP; users can override it with the Servers value */
		url = g_strconcat ("https://", host, NULL);

		should_stop = webdav_config_lookup_discover (dummy_source, url, trust_response, certificate, config_lookup,
			params, out_restart_params, &authentication_failed, cancellable, error);

		if (authentication_failed && at_pos &&
		    !e_named_parameters_exists (params, E_CONFIG_LOOKUP_PARAM_USER) &&
		    e_named_parameters_exists (params, E_CONFIG_LOOKUP_PARAM_PASSWORD)) {
			/* Use only user name portion */
			*at_pos = '\0';
			e_source_authentication_set_user (authentication_extension, email_address);

			g_clear_error (error);

			should_stop = webdav_config_lookup_discover (dummy_source, url, trust_response, certificate, config_lookup,
				params, out_restart_params, NULL, cancellable, error);

			/* Restore back to full email address */
			*at_pos = '@';
			e_source_authentication_set_user (authentication_extension, email_address);
		}

		g_free (url);
	}

	if (!should_stop && servers && *servers) {
		gchar **servers_strv;
		gint ii;

		servers_strv = g_strsplit (servers, ";", 0);

		for (ii = 0; servers_strv && servers_strv[ii] && !g_cancellable_is_cancelled (cancellable); ii++) {
			gchar *url;

			if (strstr (servers_strv[ii], "://")) {
				url = g_strdup (servers_strv[ii]);
			} else {
				/* Intentionally use secure HTTP; users can override it, if needed */
				url = g_strconcat ("https://", servers_strv[ii], NULL);
			}

			g_clear_error (error);

			webdav_config_lookup_discover (dummy_source, url, trust_response, certificate, config_lookup,
				params, out_restart_params, &authentication_failed, cancellable, error);

			if (authentication_failed && at_pos &&
			    !e_named_parameters_exists (params, E_CONFIG_LOOKUP_PARAM_USER) &&
			    e_named_parameters_exists (params, E_CONFIG_LOOKUP_PARAM_PASSWORD)) {
				/* Use only user name portion */
				*at_pos = '\0';
				e_source_authentication_set_user (authentication_extension, email_address);

				g_clear_error (error);

				webdav_config_lookup_discover (dummy_source, url, trust_response, certificate, config_lookup,
					params, out_restart_params, NULL, cancellable, error);

				/* Restore back to full email address */
				*at_pos = '@';
				e_source_authentication_set_user (authentication_extension, email_address);
			}

			g_free (url);
		}

		g_strfreev (servers_strv);
	}

	g_clear_object (&dummy_source);
	g_clear_object (&certificate);
	g_free (email_address);
}

static void
webdav_config_lookup_constructed (GObject *object)
{
	EConfigLookup *config_lookup;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_webdav_config_lookup_parent_class)->constructed (object);

	config_lookup = E_CONFIG_LOOKUP (e_extension_get_extensible (E_EXTENSION (object)));

	e_config_lookup_register_worker (config_lookup, E_CONFIG_LOOKUP_WORKER (object));
}

static void
e_webdav_config_lookup_class_init (EWebDAVConfigLookupClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = webdav_config_lookup_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CONFIG_LOOKUP;
}

static void
e_webdav_config_lookup_class_finalize (EWebDAVConfigLookupClass *class)
{
}

static void
webdav_config_lookup_worker_iface_init (EConfigLookupWorkerInterface *iface)
{
	iface->get_display_name = webdav_config_lookup_worker_get_display_name;
	iface->run = webdav_config_lookup_worker_run;
}

static void
e_webdav_config_lookup_init (EWebDAVConfigLookup *extension)
{
}

void
e_webdav_config_lookup_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_webdav_config_lookup_register_type (type_module);
}
