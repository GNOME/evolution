/*
 * e-mail-autoconfig.c
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

/* XXX Thoughts on RFC 6186: Use of SRV Records for Locating Email
 *                           Submission/Access Services
 *
 *     RFC 6186 specifies using SRV DNS lookups to aid in automatic
 *     configuration of mail accounts.  While it may be tempting to
 *     implement the RFC here (I was tempted at least), upon closer
 *     examination I find the RFC to be insufficient.
 *
 *     An SRV DNS lookup only provides a host name and port number.
 *     The RFC assumes the account's user name can be derived from
 *     the email address, and suggests probing the mail server for
 *     a valid user name by actually attempting authentication,
 *     first with the user's full email address and then falling
 *     back to only the local part.
 *
 *     I'm uncomfortable with this for a number of reasons:
 *
 *     1) I would prefer the user have a chance to manually review
 *        the settings before transmitting credentials of any kind,
 *        since DNS responses can be spoofed.
 *
 *     2) Authentication at this phase would require asking for
 *        a password either before or during auto-configuration.
 *        Asking before assumes a password-based authentication
 *        mechanism is to be used, which is not always the case,
 *        and asking during may raise the user's suspicion about
 *        what's going on behind the scenes (it would mine).
 *
 *     3) For better or worse, our architecture isn't really built
 *        to handle authentication at this stage.  EMailSession is
 *        wired into too many other areas to be reused here without
 *        risking unwanted side-effects, therefore it would require
 *        a custom CamelSession subclass with an authenticate_sync()
 *        implementation similar to EMailSession.
 *
 * While the technical limitations of (3) could be overcome, my concerns
 * in (1) and (2) still stand.  I think for the time being a better solution
 * is to have an administrator script on api.gnome.org that compares the host
 * and port settings in each clientConfig file to the _imap._tcp, _pop3._tcp,
 * and _submission._tcp SRV records for that service provider (if available)
 * to help ensure the static XML content remains accurate.  It would also be
 * instructive to track how many service providers even implement RFC 6186.
 *
 * Recording my thoughts here for posterity. -- mbarnes
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n-lib.h>

/* For error codes. */
#include <libsoup/soup.h>

#include "e-mail-autoconfig.h"

#define AUTOCONFIG_BASE_URI "https://autoconfig.thunderbird.net/v1.1/"

#define ERROR_IS_NOT_FOUND(error) \
	(g_error_matches ((error), E_SOUP_SESSION_ERROR, SOUP_STATUS_NOT_FOUND))

typedef struct _EMailAutoconfigResult EMailAutoconfigResult;
typedef struct _ParserClosure ParserClosure;

struct _EMailAutoconfigResult {
	gboolean set;
	gchar *user;
	gchar *host;
	guint16 port;
	gchar *auth_mechanism;
	CamelNetworkSecurityMethod security_method;
};

struct _EMailAutoconfigPrivate {
	ESourceRegistry *registry;
	gchar *email_address;
	gchar *email_local_part;
	gchar *email_domain_part;
	gchar *use_domain;
	EMailAutoconfigResult imap_result;
	EMailAutoconfigResult pop3_result;
	EMailAutoconfigResult smtp_result;
	EMailAutoconfigResult custom_result;
	GHashTable *custom_types; /* gchar *type ~> ENamedParameters *params */
};

struct _ParserClosure {
	EMailAutoconfig *autoconfig;
	EMailAutoconfigResult *result;
	gchar *current_type;
	ENamedParameters *custom_params;
};

enum {
	PROP_0,
	PROP_EMAIL_ADDRESS,
	PROP_REGISTRY,
	PROP_USE_DOMAIN
};

enum {
	PROCESS_CUSTOM_TYPES,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Forward Declarations */
static void	e_mail_autoconfig_initable_init	(GInitableIface *iface);

/* By default, the GAsyncInitable interface calls GInitable.init()
 * from a separate thread, so we only have to override GInitable. */
G_DEFINE_TYPE_WITH_CODE (EMailAutoconfig, e_mail_autoconfig, G_TYPE_OBJECT,
	G_ADD_PRIVATE (EMailAutoconfig)
	G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, e_mail_autoconfig_initable_init)
	G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
e_mail_config_result_clear (EMailAutoconfigResult *result)
{
	if (!result)
		return;

	g_clear_pointer (&result->user, g_free);
	g_clear_pointer (&result->host, g_free);
	g_clear_pointer (&result->auth_mechanism, g_free);
}

static void
mail_autoconfig_parse_start_element (GMarkupParseContext *context,
                                     const gchar *element_name,
                                     const gchar **attribute_names,
                                     const gchar **attribute_values,
                                     gpointer user_data,
                                     GError **error)
{
	ParserClosure *closure = user_data;
	EMailAutoconfigPrivate *priv;
	gboolean is_incoming_server;
	gboolean is_outgoing_server;

	priv = closure->autoconfig->priv;

	is_incoming_server = g_str_equal (element_name, "incomingServer");
	is_outgoing_server = g_str_equal (element_name, "outgoingServer");

	if (is_incoming_server || is_outgoing_server) {
		const gchar *type = NULL;

		g_markup_collect_attributes (
			element_name,
			attribute_names,
			attribute_values,
			error,
			G_MARKUP_COLLECT_STRING,
			"type", &type,
			G_MARKUP_COLLECT_INVALID);

		if (g_strcmp0 (type, "imap") == 0)
			closure->result = &priv->imap_result;
		if (g_strcmp0 (type, "pop3") == 0)
			closure->result = &priv->pop3_result;
		if (g_strcmp0 (type, "smtp") == 0)
			closure->result = &priv->smtp_result;

		if (type != NULL && closure->result == NULL) {
			g_return_if_fail (closure->current_type == NULL);
			g_return_if_fail (closure->custom_params == NULL);

			closure->current_type = g_strdup (type);
			closure->custom_params = e_named_parameters_new ();

			e_named_parameters_set (closure->custom_params, "kind", element_name);
		}
	}
}

static void
mail_autoconfig_parse_end_element (GMarkupParseContext *context,
                                   const gchar *element_name,
                                   gpointer user_data,
                                   GError **error)
{
	ParserClosure *closure = user_data;
	gboolean is_incoming_server;
	gboolean is_outgoing_server;

	is_incoming_server = g_str_equal (element_name, "incomingServer");
	is_outgoing_server = g_str_equal (element_name, "outgoingServer");

	if (is_incoming_server || is_outgoing_server) {
		if (closure->custom_params && e_named_parameters_count (closure->custom_params) > 1) {
			if (!closure->autoconfig->priv->custom_types)
				closure->autoconfig->priv->custom_types =
					g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) e_named_parameters_free);

			g_hash_table_insert (closure->autoconfig->priv->custom_types, closure->current_type, closure->custom_params);

			closure->current_type = NULL;
			closure->custom_params = NULL;
		}

		g_clear_pointer (&closure->current_type, g_free);
		g_clear_pointer (&closure->custom_params, e_named_parameters_free);
		closure->result = NULL;
	}
}

static void
mail_autoconfig_parse_text (GMarkupParseContext *context,
                            const gchar *text,
                            gsize text_length,
                            gpointer user_data,
                            GError **error)
{
	ParserClosure *closure = user_data;
	EMailAutoconfigPrivate *priv;
	const gchar *element_name;
	GString *string;

	priv = closure->autoconfig->priv;

	if (closure->result == NULL && closure->custom_params == NULL)
		return;

	/* Perform the following text substitutions:
	 *
	 * %EMAILADDRESS%    :  closure->email_address
	 * %EMAILLOCALPART%  :  closure->email_local_part
	 * %EMAILDOMAIN%     :  closure->email_domain_part
	 */
	if (strchr (text, '%') == NULL)
		string = g_string_new (text);
	else {
		const gchar *cp = text;

		string = g_string_sized_new (256);
		while (*cp != '\0') {
			const gchar *variable;
			const gchar *substitute;

			if (*cp != '%') {
				g_string_append_c (string, *cp++);
				continue;
			}

			variable = "%EMAILADDRESS%";
			substitute = priv->email_address;

			if (strncmp (cp, variable, strlen (variable)) == 0) {
				g_string_append (string, substitute);
				cp += strlen (variable);
				continue;
			}

			variable = "%EMAILLOCALPART%";
			substitute = priv->email_local_part;

			if (strncmp (cp, variable, strlen (variable)) == 0) {
				g_string_append (string, substitute);
				cp += strlen (variable);
				continue;
			}

			variable = "%EMAILDOMAIN%";
			substitute = priv->use_domain;

			if (strncmp (cp, variable, strlen (variable)) == 0) {
				g_string_append (string, substitute);
				cp += strlen (variable);
				continue;
			}

			g_string_append_c (string, *cp++);
		}
	}

	element_name = g_markup_parse_context_get_element (context);

	if (g_str_equal (element_name, "hostname")) {
		if (closure->custom_params) {
			e_named_parameters_set (closure->custom_params, "host", string->str);
		} else {
			closure->result->host = g_strdup (string->str);
			closure->result->set = TRUE;
		}

	} else if (g_str_equal (element_name, "username")) {
		if (closure->custom_params) {
			e_named_parameters_set (closure->custom_params, "user", string->str);
		} else {
			closure->result->user = g_strdup (string->str);
			closure->result->set = TRUE;
		}

	} else if (g_str_equal (element_name, "port")) {
		if (closure->custom_params) {
			e_named_parameters_set (closure->custom_params, "port", string->str);
		} else {
			glong port = strtol (string->str, NULL, 10);
			if (port == CLAMP (port, 1, G_MAXUINT16)) {
				closure->result->port = (guint16) port;
				closure->result->set = TRUE;
			}
		}

	} else if (g_str_equal (element_name, "socketType")) {
		CamelNetworkSecurityMethod security_method = CAMEL_NETWORK_SECURITY_METHOD_NONE;
		gboolean set = FALSE;

		if (g_str_equal (string->str, "plain")) {
			security_method = CAMEL_NETWORK_SECURITY_METHOD_NONE;
			set = TRUE;
		} else if (g_str_equal (string->str, "SSL")) {
			security_method = CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT;
			set = TRUE;
		} else if (g_str_equal (string->str, "STARTTLS")) {
			security_method = CAMEL_NETWORK_SECURITY_METHOD_STARTTLS_ON_STANDARD_PORT;
			set = TRUE;
		}

		if (set) {
			if (closure->custom_params) {
				const gchar *enum_str = e_enum_to_string (CAMEL_TYPE_NETWORK_SECURITY_METHOD, security_method);
				g_warn_if_fail (enum_str != NULL);
				if (enum_str)
					e_named_parameters_set (closure->custom_params, "security-method", enum_str);
			} else {
				closure->result->set = set;
				closure->result->security_method = security_method;
			}
		}
	} else if (g_str_equal (element_name, "authentication")) {
		gboolean use_plain_auth = FALSE;

		/* "password-cleartext" and "plain" are synonymous. */

		if (g_str_equal (string->str, "password-cleartext"))
			use_plain_auth = TRUE;

		if (g_str_equal (string->str, "plain"))
			use_plain_auth = TRUE;

		if (use_plain_auth) {
			gchar *auth_mechanism = NULL;

			/* The exact auth name depends on the protocol. */

			/* Leave this NULL for IMAP so Camel
			 * will issue an IMAP LOGIN command. */
			if (closure->result == &priv->imap_result)
				auth_mechanism = NULL;

			/* Leave this NULL for POP3 so Camel
			 * will issue POP3 USER/PASS commands. */
			if (closure->result == &priv->pop3_result)
				auth_mechanism = NULL;

			if (closure->result == &priv->smtp_result)
				auth_mechanism = g_strdup ("LOGIN");

			if (closure->custom_params) {
				e_named_parameters_set (closure->custom_params, "auth-mechanism", auth_mechanism);
				g_free (auth_mechanism);
			} else {
				closure->result->auth_mechanism = auth_mechanism;
				closure->result->set = TRUE;
			}
		} else if (g_str_equal (string->str, "password-encrypted")) {
			/* "password-encrypted" apparently maps to CRAM-MD5,
			 * or at least that's how Thunderbird interprets it. */
			if (closure->custom_params) {
				e_named_parameters_set (closure->custom_params, "auth-mechanism", "CRAM-MD5");
			} else {
				closure->result->auth_mechanism = g_strdup ("CRAM-MD5");
				closure->result->set = TRUE;
			}
		} else if (closure->custom_params) {
			e_named_parameters_set (closure->custom_params, "auth-mechanism", string->str);
		}

		/* XXX Other <authentication> values not handled,
		 *     but they are corner cases for the most part. */
	} else if (closure->custom_params) {
		e_named_parameters_set (closure->custom_params, element_name, string->str);
	}

	g_string_free (string, TRUE);
}

static GMarkupParser mail_autoconfig_parser = {
	mail_autoconfig_parse_start_element,
	mail_autoconfig_parse_end_element,
	mail_autoconfig_parse_text
};

static gchar *
mail_autoconfig_resolve_name_server (const gchar *domain,
                                     GCancellable *cancellable,
                                     GError **error)
{
	GResolver *resolver;
	GList *records, *link;
	gchar *name_server = NULL;
	guint16 best_pref = G_MAXUINT16;

	resolver = g_resolver_get_default ();
	records = g_resolver_lookup_records (resolver, domain, G_RESOLVER_RECORD_MX, cancellable, error);

	for (link = records; link; link = g_list_next (link)) {
		GVariant *var = link->data;
		gchar *val = NULL;
		guint16 pref = G_MAXUINT16;

		g_variant_get (var, "(qs)", &pref, &val);

		if (!name_server || best_pref > pref) {
			g_clear_pointer (&name_server, g_free);
			name_server = val;
			best_pref = pref;
		} else {
			g_free (val);
		}
	}

	g_list_free_full (records, (GDestroyNotify) g_variant_unref);
	g_object_unref (resolver);

	return name_server;
}

static void
mail_autoconfig_abort_soup_session_cb (GCancellable *cancellable,
                                       SoupSession *soup_session)
{
	soup_session_abort (soup_session);
}

static gboolean
mail_autoconfig_lookup_uri_sync (EMailAutoconfig *autoconfig,
				 const gchar *uri,
				 SoupSession *soup_session,
				 GCancellable *cancellable,
				 GError **error)
{
	SoupMessage *soup_message;
	GBytes *data;
	gboolean success;
	GError *local_error = NULL;

	soup_message = soup_message_new (SOUP_METHOD_GET, uri);

	if (!soup_message) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
			_("Invalid URI: “%s”"), uri);
		return FALSE;
	}

	soup_message_headers_append (
		soup_message_get_request_headers (soup_message),
		"User-Agent", "Evolution/" VERSION VERSION_SUBSTRING " " VERSION_COMMENT);

	data = soup_session_send_and_read (soup_session, soup_message, cancellable, &local_error);

	success = SOUP_STATUS_IS_SUCCESSFUL (soup_message_get_status (soup_message));

	if (camel_debug ("autoconfig")) {
		printf ("mail-autoconfig: URI:'%s' success:%d n-bytes-returned:%" G_GUINT64_FORMAT "\n",
			uri, success, data ? (guint64) g_bytes_get_size (data) : (guint64) 0);
	}

	if (success && data) {
		GMarkupParseContext *context;
		ParserClosure closure;

		closure.autoconfig = autoconfig;
		closure.result = NULL;
		closure.current_type = NULL;
		closure.custom_params = NULL;

		context = g_markup_parse_context_new (
			&mail_autoconfig_parser, 0,
			&closure, (GDestroyNotify) NULL);

		success = g_markup_parse_context_parse (
			context,
			g_bytes_get_data (data, NULL),
			g_bytes_get_size (data),
			error);

		if (success)
			success = g_markup_parse_context_end_parse (context, error);

		g_clear_pointer (&closure.custom_params, e_named_parameters_free);
		g_clear_pointer (&closure.current_type, g_free);

		g_markup_parse_context_free (context);
	} else if (local_error) {
		g_propagate_error (error, local_error);
		local_error = NULL;
	} else {
		g_set_error_literal (
			error, E_SOUP_SESSION_ERROR,
			soup_message_get_status (soup_message),
			soup_message_get_reason_phrase (soup_message));
	}

	if (data)
		g_bytes_unref (data);
	g_object_unref (soup_message);
	g_clear_error (&local_error);

	return success;
}

static gboolean
mail_autoconfig_lookup (EMailAutoconfig *autoconfig,
                        const gchar *domain,
			const gchar *email,
			const gchar *emailmd5,
                        GCancellable *cancellable,
                        GError **error)
{
	ESourceRegistry *registry;
	ESource *proxy_source;
	SoupSession *soup_session;
	gulong cancel_id = 0;
	gchar *uri;
	gchar *email_escaped;
	gboolean success = FALSE;

	registry = e_mail_autoconfig_get_registry (autoconfig);
	proxy_source = e_source_registry_ref_builtin_proxy (registry);

	soup_session = soup_session_new_with_options (
		"proxy-resolver", G_PROXY_RESOLVER (proxy_source),
		"timeout", 15,
		NULL);

	g_object_unref (proxy_source);

	email_escaped = g_uri_escape_string (email, NULL, FALSE);

	if (G_IS_CANCELLABLE (cancellable))
		cancel_id = g_cancellable_connect (
			cancellable,
			G_CALLBACK (mail_autoconfig_abort_soup_session_cb),
			g_object_ref (soup_session),
			(GDestroyNotify) g_object_unref);

	/* First try user configuration in autoconfig.$DOMAIN URL and ignore error */
	if (!success && ((error && !*error && !g_cancellable_set_error_if_cancelled (cancellable, error)) || !g_cancellable_is_cancelled (cancellable))) {
		uri = g_strconcat ("https://autoconfig.", domain, "/mail/config-v1.1.xml?emailaddress=", email_escaped, "&emailmd5=", emailmd5, NULL);
		success = mail_autoconfig_lookup_uri_sync (autoconfig, uri, soup_session, cancellable, NULL);
		g_free (uri);
	}

	if (!success && ((error && !*error && !g_cancellable_set_error_if_cancelled (cancellable, error)) || !g_cancellable_is_cancelled (cancellable))) {
		uri = g_strconcat ("http://autoconfig.", domain, "/mail/config-v1.1.xml?emailaddress=", email_escaped, "&emailmd5=", emailmd5, NULL);
		success = mail_autoconfig_lookup_uri_sync (autoconfig, uri, soup_session, cancellable, NULL);
		g_free (uri);
	}

	/* Then with $DOMAIN/.well-known/autoconfig/ and ignore error */
	if (!success && ((error && !*error && !g_cancellable_set_error_if_cancelled (cancellable, error)) || !g_cancellable_is_cancelled (cancellable))) {
		uri = g_strconcat ("https://", domain, "/.well-known/autoconfig/mail/config-v1.1.xml?emailaddress=", email_escaped, "&emailmd5=", emailmd5, NULL);
		success = mail_autoconfig_lookup_uri_sync (autoconfig, uri, soup_session, cancellable, NULL);
		g_free (uri);
	}

	if (!success && ((error && !*error && !g_cancellable_set_error_if_cancelled (cancellable, error)) || !g_cancellable_is_cancelled (cancellable))) {
		uri = g_strconcat ("http://", domain, "/.well-known/autoconfig/mail/config-v1.1.xml?emailaddress=", email_escaped, "&emailmd5=", emailmd5, NULL);
		success = mail_autoconfig_lookup_uri_sync (autoconfig, uri, soup_session, cancellable, NULL);
		g_free (uri);
	}

	/* Final, try the upstream ISPDB and propagate error */
	if (!success && ((error && !*error && !g_cancellable_set_error_if_cancelled (cancellable, error)) || !g_cancellable_is_cancelled (cancellable))) {
		uri = g_strconcat (AUTOCONFIG_BASE_URI, domain, NULL);
		success = mail_autoconfig_lookup_uri_sync (autoconfig, uri, soup_session, cancellable, error);
		g_free (uri);
	}

	if (cancel_id > 0)
		g_cancellable_disconnect (cancellable, cancel_id);

	g_object_unref (soup_session);
	g_free (email_escaped);

	return success;
}

static gboolean
mail_autoconfig_set_details (ESourceRegistry *registry,
			     EMailAutoconfigResult *result,
			     ESource *source,
			     const gchar *extension_name,
			     const gchar *default_backend_name)
{
	ESourceCamel *camel_ext;
	ESourceBackend *backend_ext;
	CamelSettings *settings;
	const gchar *backend_name;

	g_return_val_if_fail (result != NULL, FALSE);

	if (!result->set)
		return FALSE;

	if (!e_source_has_extension (source, extension_name))
		return FALSE;

	backend_ext = e_source_get_extension (source, extension_name);
	backend_name = e_source_backend_get_backend_name (backend_ext);
	if (!backend_name || !*backend_name) {
		e_source_backend_set_backend_name (backend_ext, default_backend_name);
		backend_name = default_backend_name;
	}

	if (!backend_name)
		return FALSE;

	extension_name = e_source_camel_get_extension_name (backend_name);
	camel_ext = e_source_get_extension (source, extension_name);

	settings = e_source_camel_get_settings (camel_ext);
	g_return_val_if_fail (CAMEL_IS_NETWORK_SETTINGS (settings), FALSE);

	/* Set the security method before the port, to not have it overwritten
	   in New Mail Account wizard (binding callback). */
	g_object_set (settings,
		"auth-mechanism", result->auth_mechanism,
		"security-method", result->security_method,
		"user", result->user,
		"host", result->host,
		"port", result->port,
		NULL);

	if (result->host && registry) {
		EOAuth2Service *oauth2_service;

		/* Prefer OAuth2, if available */
		oauth2_service = e_oauth2_services_find (e_source_registry_get_oauth2_services (registry), source);
		if (!oauth2_service) {
			oauth2_service = e_oauth2_services_guess (e_source_registry_get_oauth2_services (registry),
				backend_name, result->host);
		}

		if (oauth2_service) {
			g_object_set (settings,
				"auth-mechanism", e_oauth2_service_get_name (oauth2_service),
				NULL);
		}

		g_clear_object (&oauth2_service);
	}

	return TRUE;
}

#define E_TYPE_MAIL_CONFIG_LOOKUP_RESULT \
	(e_mail_config_lookup_result_get_type ())
#define E_MAIL_CONFIG_LOOKUP_RESULT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_LOOKUP_RESULT, EMailConfigLookupResult))
#define E_IS_MAIL_CONFIG_LOOKUP_RESULT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_LOOKUP_RESULT))

typedef struct _EMailConfigLookupResult EMailConfigLookupResult;
typedef struct _EMailConfigLookupResultClass EMailConfigLookupResultClass;

struct _EMailConfigLookupResult {
	/*< private >*/
	EConfigLookupResultSimple parent;

	EMailAutoconfigResult result;
	gchar *extension_name;
};

struct _EMailConfigLookupResultClass {
	/*< private >*/
	EConfigLookupResultSimpleClass parent_class;
};

GType e_mail_config_lookup_result_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (EMailConfigLookupResult, e_mail_config_lookup_result, E_TYPE_CONFIG_LOOKUP_RESULT_SIMPLE)

static gboolean
mail_config_lookup_result_configure_source (EConfigLookupResult *lookup_result,
					    EConfigLookup *config_lookup,
					    ESource *source)
{
	EMailConfigLookupResult *mail_result;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_LOOKUP_RESULT (lookup_result), FALSE);

	mail_result = E_MAIL_CONFIG_LOOKUP_RESULT (lookup_result);

	/* No chain up to parent method, not needed here, because not used */
	return mail_autoconfig_set_details (
		e_config_lookup_get_registry (config_lookup),
		&mail_result->result, source,
		mail_result->extension_name,
		e_config_lookup_result_get_protocol (lookup_result));
}

static void
mail_config_lookup_result_finalize (GObject *object)
{
	EMailConfigLookupResult *mail_result = E_MAIL_CONFIG_LOOKUP_RESULT (object);

	e_mail_config_result_clear (&mail_result->result);
	g_free (mail_result->extension_name);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_config_lookup_result_parent_class)->finalize (object);
}

static void
e_mail_config_lookup_result_class_init (EMailConfigLookupResultClass *klass)
{
	EConfigLookupResultSimpleClass *simple_result_class;
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mail_config_lookup_result_finalize;

	simple_result_class = E_CONFIG_LOOKUP_RESULT_SIMPLE_CLASS (klass);
	simple_result_class->configure_source = mail_config_lookup_result_configure_source;
}

static void
e_mail_config_lookup_result_init (EMailConfigLookupResult *mail_result)
{
}

static EConfigLookupResult *
e_mail_config_lookup_result_new (EConfigLookupResultKind kind,
				 gint priority,
				 const gchar *protocol,
				 const gchar *display_name,
				 const gchar *description,
				 const EMailAutoconfigResult *result,
				 const gchar *extension_name)
{
	EMailConfigLookupResult *mail_result;

	g_return_val_if_fail (protocol != NULL, NULL);
	g_return_val_if_fail (display_name != NULL, NULL);
	g_return_val_if_fail (description != NULL, NULL);
	g_return_val_if_fail (result != NULL, NULL);
	g_return_val_if_fail (extension_name != NULL, NULL);

	mail_result = g_object_new (E_TYPE_MAIL_CONFIG_LOOKUP_RESULT,
		"kind", kind,
		"priority", priority,
		"is-complete", TRUE,
		"protocol", protocol,
		"display-name", display_name,
		"description", description,
		"password", NULL,
		NULL);

	mail_result->result.set = result->set;
	mail_result->result.user = g_strdup (result->user);
	mail_result->result.host = g_strdup (result->host);
	mail_result->result.port = result->port;
	mail_result->result.auth_mechanism = g_strdup (result->auth_mechanism);
	mail_result->result.security_method = result->security_method;
	mail_result->extension_name = g_strdup (extension_name);

	return E_CONFIG_LOOKUP_RESULT (mail_result);
}

static void
mail_autoconfig_result_to_config_lookup (EMailAutoconfig *mail_autoconfig,
					 EConfigLookup *config_lookup,
					 EMailAutoconfigResult *result,
					 gint priority,
					 const gchar *protocol,
					 const gchar *display_name,
					 const gchar *extension_name)
{
	EConfigLookupResult *lookup_result;
	EConfigLookupResultKind kind;
	GString *description;

	g_return_if_fail (E_IS_MAIL_AUTOCONFIG (mail_autoconfig));
	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	g_return_if_fail (result != NULL);
	g_return_if_fail (protocol != NULL);
	g_return_if_fail (display_name != NULL);
	g_return_if_fail (extension_name != NULL);

	if (!result->set)
		return;

	kind = E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE;
	if (g_strcmp0 (extension_name, E_SOURCE_EXTENSION_MAIL_TRANSPORT) == 0)
		kind = E_CONFIG_LOOKUP_RESULT_MAIL_SEND;

	description = g_string_new ("");

	g_string_append_printf (description, _("Host: %s:%d"), result->host, result->port);

	if (result->user && *result->user) {
		g_string_append_c (description, '\n');
		g_string_append_printf (description, _("User: %s"), result->user);
	}

	g_string_append_c (description, '\n');
	g_string_append_printf (description, _("Security method: %s"),
		result->security_method == CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT ?  _("TLS") :
		result->security_method == CAMEL_NETWORK_SECURITY_METHOD_STARTTLS_ON_STANDARD_PORT ? _("STARTTLS") : _("None"));

	if (result->auth_mechanism && *result->auth_mechanism) {
		g_string_append_c (description, '\n');
		g_string_append_printf (description, _("Authentication mechanism: %s"), result->auth_mechanism);
	}

	lookup_result = e_mail_config_lookup_result_new (kind, priority, protocol, display_name, description->str, result, extension_name);
	e_config_lookup_add_result (config_lookup, lookup_result);

	g_string_free (description, TRUE);
}

static gchar *
mail_autoconfig_calc_emailmd5 (const gchar *email_address)
{
	gchar *lowercase, *emailmd5;

	if (!email_address)
		return NULL;

	lowercase = g_ascii_strdown (email_address, -1);
	if (!lowercase || !*lowercase) {
		g_free (lowercase);
		return NULL;
	}

	emailmd5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, lowercase, -1);

	g_free (lowercase);

	return emailmd5;
}

static void
mail_autoconfig_set_email_address (EMailAutoconfig *autoconfig,
                                   const gchar *email_address)
{
	g_return_if_fail (email_address != NULL);
	g_return_if_fail (autoconfig->priv->email_address == NULL);

	autoconfig->priv->email_address = g_strdup (email_address);
}

static void
mail_autoconfig_set_use_domain (EMailAutoconfig *autoconfig,
				const gchar *use_domain)
{
	if (g_strcmp0 (autoconfig->priv->use_domain, use_domain) != 0) {
		g_free (autoconfig->priv->use_domain);
		autoconfig->priv->use_domain = g_strdup (use_domain);
	}
}

static void
mail_autoconfig_set_registry (EMailAutoconfig *autoconfig,
                              ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (autoconfig->priv->registry == NULL);

	autoconfig->priv->registry = g_object_ref (registry);
}

static void
mail_autoconfig_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EMAIL_ADDRESS:
			mail_autoconfig_set_email_address (
				E_MAIL_AUTOCONFIG (object),
				g_value_get_string (value));
			return;

		case PROP_REGISTRY:
			mail_autoconfig_set_registry (
				E_MAIL_AUTOCONFIG (object),
				g_value_get_object (value));
			return;

		case PROP_USE_DOMAIN:
			mail_autoconfig_set_use_domain (
				E_MAIL_AUTOCONFIG (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_autoconfig_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EMAIL_ADDRESS:
			g_value_set_string (
				value,
				e_mail_autoconfig_get_email_address (
				E_MAIL_AUTOCONFIG (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_mail_autoconfig_get_registry (
				E_MAIL_AUTOCONFIG (object)));
			return;

		case PROP_USE_DOMAIN:
			g_value_set_string (
				value,
				e_mail_autoconfig_get_use_domain (
				E_MAIL_AUTOCONFIG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_autoconfig_constructed (GObject *object)
{
	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_autoconfig_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
mail_autoconfig_dispose (GObject *object)
{
	EMailAutoconfig *self = E_MAIL_AUTOCONFIG (object);

	g_clear_object (&self->priv->registry);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_autoconfig_parent_class)->dispose (object);
}

static void
mail_autoconfig_finalize (GObject *object)
{
	EMailAutoconfig *self = E_MAIL_AUTOCONFIG (object);

	g_free (self->priv->email_address);
	g_free (self->priv->email_local_part);
	g_free (self->priv->email_domain_part);
	g_free (self->priv->use_domain);

	e_mail_config_result_clear (&self->priv->imap_result);
	e_mail_config_result_clear (&self->priv->pop3_result);
	e_mail_config_result_clear (&self->priv->smtp_result);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_autoconfig_parent_class)->finalize (object);
}

static gboolean
mail_autoconfig_initable_init (GInitable *initable,
                               GCancellable *cancellable,
                               GError **error)
{
	EMailAutoconfig *autoconfig;
	const gchar *email_address;
	const gchar *domain;
	const gchar *cp;
	gchar *name_server, *emailmd5;
	gboolean success = FALSE;
	GError *local_error = NULL;

	autoconfig = E_MAIL_AUTOCONFIG (initable);
	email_address = e_mail_autoconfig_get_email_address (autoconfig);

	if (email_address == NULL) {
		g_set_error_literal (
			error, G_IO_ERROR,
			G_IO_ERROR_INVALID_ARGUMENT,
			_("No email address provided"));
		return FALSE;
	}

	cp = strchr (email_address, '@');
	if (cp == NULL) {
		g_set_error_literal (
			error, G_IO_ERROR,
			G_IO_ERROR_INVALID_ARGUMENT,
			_("Missing domain in email address"));
		return FALSE;
	}

	domain = cp + 1;

	autoconfig->priv->email_local_part =
		g_strndup (email_address, cp - email_address);
	autoconfig->priv->email_domain_part = g_strdup (domain);

	if (autoconfig->priv->use_domain && *autoconfig->priv->use_domain)
		domain = autoconfig->priv->use_domain;

	emailmd5 = mail_autoconfig_calc_emailmd5 (email_address);

	/* First try the email address domain verbatim. */
	success = mail_autoconfig_lookup (
		autoconfig, domain, email_address, emailmd5, cancellable, &local_error);

	g_warn_if_fail (
		(success && local_error == NULL) ||
		(!success && local_error != NULL));

	if (success) {
		g_free (emailmd5);
		return TRUE;
	}

	/* "404 Not Found" errors are non-fatal this time around. */
	if (ERROR_IS_NOT_FOUND (local_error)) {
		g_clear_error (&local_error);
	} else {
		g_propagate_error (error, local_error);
		g_free (emailmd5);
		return FALSE;
	}

	/* Look up an authoritative name server for the email address
	 * domain according to its "name server" (NS) DNS record. */
	name_server = mail_autoconfig_resolve_name_server (
		domain, cancellable, error);

	if (!name_server) {
		g_free (emailmd5);
		return FALSE;
	}

	/* Widdle away segments of the name server domain until
	 * we find a match, or until we widdle down to nothing. */

	cp = name_server;
	while (cp != NULL && strchr (cp, '.') != NULL) {
		g_clear_error (&local_error);

		success = mail_autoconfig_lookup (
			autoconfig, cp, email_address, emailmd5, cancellable, &local_error);

		g_warn_if_fail (
			(success && local_error == NULL) ||
			(!success && local_error != NULL));

		if (success || !ERROR_IS_NOT_FOUND (local_error))
			break;

		cp = strchr (cp, '.');
		if (cp != NULL)
			cp++;
	}

	if (!success && !local_error)
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Unknown error"));
	else if (local_error)
		g_propagate_error (error, local_error);

	g_free (name_server);
	g_free (emailmd5);

	return success;
}

static void
e_mail_autoconfig_class_init (EMailAutoconfigClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_autoconfig_set_property;
	object_class->get_property = mail_autoconfig_get_property;
	object_class->constructed = mail_autoconfig_constructed;
	object_class->dispose = mail_autoconfig_dispose;
	object_class->finalize = mail_autoconfig_finalize;

	g_object_class_install_property (
		object_class,
		PROP_EMAIL_ADDRESS,
		g_param_spec_string (
			"email-address",
			"Email Address",
			"The address from which to query config data",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
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
		PROP_USE_DOMAIN,
		g_param_spec_string (
			"use-domain",
			"Use Domain",
			"A domain to use, instead of the one from email-address",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	signals[PROCESS_CUSTOM_TYPES] = g_signal_new (
		"process-custom-types",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		E_TYPE_CONFIG_LOOKUP,
		G_TYPE_HASH_TABLE);
}

static void
e_mail_autoconfig_initable_init (GInitableIface *iface)
{
	iface->init = mail_autoconfig_initable_init;
}

static void
e_mail_autoconfig_init (EMailAutoconfig *autoconfig)
{
	autoconfig->priv = e_mail_autoconfig_get_instance_private (autoconfig);
}

EMailAutoconfig *
e_mail_autoconfig_new_sync (ESourceRegistry *registry,
                            const gchar *email_address,
			    const gchar *use_domain,
                            GCancellable *cancellable,
                            GError **error)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (email_address != NULL, NULL);

	return g_initable_new (
		E_TYPE_MAIL_AUTOCONFIG,
		cancellable, error,
		"registry", registry,
		"email-address", email_address,
		"use-domain", use_domain,
		NULL);
}

void
e_mail_autoconfig_new (ESourceRegistry *registry,
                       const gchar *email_address,
		       const gchar *use_domain,
                       gint io_priority,
                       GCancellable *cancellable,
                       GAsyncReadyCallback callback,
                       gpointer user_data)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (email_address != NULL);

	g_async_initable_new_async (
		E_TYPE_MAIL_AUTOCONFIG,
		io_priority, cancellable,
		callback, user_data,
		"registry", registry,
		"email-address", email_address,
		"use-domain", use_domain,
		NULL);
}

EMailAutoconfig *
e_mail_autoconfig_finish (GAsyncResult *result,
                          GError **error)
{
	GObject *source_object;
	GObject *autoconfig;

	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

	source_object = g_async_result_get_source_object (result);
	g_return_val_if_fail (source_object != NULL, NULL);

	autoconfig = g_async_initable_new_finish (
		G_ASYNC_INITABLE (source_object), result, error);

	g_object_unref (source_object);

	if (autoconfig == NULL)
		return NULL;

	return E_MAIL_AUTOCONFIG (autoconfig);
}

/**
 * e_mail_autoconfig_get_registry:
 * @autoconfig: an #EMailAutoconfig
 *
 * Returns the #ESourceRegistry passed to e_mail_autoconfig_new() or
 * e_mail_autoconfig_new_sync().
 *
 * Returns: an #ESourceRegistry
 **/
ESourceRegistry *
e_mail_autoconfig_get_registry (EMailAutoconfig *autoconfig)
{
	g_return_val_if_fail (E_IS_MAIL_AUTOCONFIG (autoconfig), NULL);

	return autoconfig->priv->registry;
}

const gchar *
e_mail_autoconfig_get_email_address (EMailAutoconfig *autoconfig)
{
	g_return_val_if_fail (E_IS_MAIL_AUTOCONFIG (autoconfig), NULL);

	return autoconfig->priv->email_address;
}

const gchar *
e_mail_autoconfig_get_use_domain (EMailAutoconfig *autoconfig)
{
	g_return_val_if_fail (E_IS_MAIL_AUTOCONFIG (autoconfig), NULL);

	return autoconfig->priv->use_domain;
}

gboolean
e_mail_autoconfig_set_imap_details (EMailAutoconfig *autoconfig,
                                    ESource *imap_source)
{
	g_return_val_if_fail (E_IS_MAIL_AUTOCONFIG (autoconfig), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (imap_source), FALSE);

	return mail_autoconfig_set_details (
		autoconfig->priv->registry,
		&autoconfig->priv->imap_result,
		imap_source, E_SOURCE_EXTENSION_MAIL_ACCOUNT, "imapx");
}

gboolean
e_mail_autoconfig_set_pop3_details (EMailAutoconfig *autoconfig,
                                    ESource *pop3_source)
{
	g_return_val_if_fail (E_IS_MAIL_AUTOCONFIG (autoconfig), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (pop3_source), FALSE);

	return mail_autoconfig_set_details (
		autoconfig->priv->registry,
		&autoconfig->priv->pop3_result,
		pop3_source, E_SOURCE_EXTENSION_MAIL_ACCOUNT, "pop3");
}

gboolean
e_mail_autoconfig_set_smtp_details (EMailAutoconfig *autoconfig,
                                    ESource *smtp_source)
{
	g_return_val_if_fail (E_IS_MAIL_AUTOCONFIG (autoconfig), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (smtp_source), FALSE);

	return mail_autoconfig_set_details (
		autoconfig->priv->registry,
		&autoconfig->priv->smtp_result,
		smtp_source, E_SOURCE_EXTENSION_MAIL_TRANSPORT, "smtp");
}

void
e_mail_autoconfig_dump_results (EMailAutoconfig *autoconfig)
{
	const gchar *email_address;
	gboolean have_results;

	g_return_if_fail (E_IS_MAIL_AUTOCONFIG (autoconfig));

	email_address = autoconfig->priv->email_address;

	have_results =
		autoconfig->priv->imap_result.set ||
		autoconfig->priv->pop3_result.set ||
		autoconfig->priv->smtp_result.set;

	if (have_results) {
		if (autoconfig->priv->use_domain && *autoconfig->priv->use_domain)
			g_print ("Results for <%s> and domain '%s'\n", email_address, autoconfig->priv->use_domain);
		else
			g_print ("Results for <%s>\n", email_address);

		if (autoconfig->priv->imap_result.set) {
			g_print (
				"IMAP: %s@%s:%u\n",
				autoconfig->priv->imap_result.user,
				autoconfig->priv->imap_result.host,
				autoconfig->priv->imap_result.port);
		}

		if (autoconfig->priv->pop3_result.set) {
			g_print (
				"POP3: %s@%s:%u\n",
				autoconfig->priv->pop3_result.user,
				autoconfig->priv->pop3_result.host,
				autoconfig->priv->pop3_result.port);
		}

		if (autoconfig->priv->smtp_result.set) {
			g_print (
				"SMTP: %s@%s:%u\n",
				autoconfig->priv->smtp_result.user,
				autoconfig->priv->smtp_result.host,
				autoconfig->priv->smtp_result.port);
		}

	} else if (autoconfig->priv->use_domain && *autoconfig->priv->use_domain) {
		g_print ("No results for <%s> and domain '%s'\n", email_address, autoconfig->priv->use_domain);
	} else {
		g_print ("No results for <%s>\n", email_address);
	}
}

/**
 * e_mail_autoconfig_copy_results_to_config_lookup:
 * @mail_autoconfig: an #EMailAutoconfig
 * @config_lookup: an #EConfigLookup
 *
 * Copies any valid result from @mail_autoconfig to @config_lookup.
 *
 * Since: 3.26
 **/
void
e_mail_autoconfig_copy_results_to_config_lookup (EMailAutoconfig *mail_autoconfig,
						 EConfigLookup *config_lookup)
{
	g_return_if_fail (E_IS_MAIL_AUTOCONFIG (mail_autoconfig));
	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));

	mail_autoconfig_result_to_config_lookup (mail_autoconfig, config_lookup,
		&mail_autoconfig->priv->imap_result,
		E_CONFIG_LOOKUP_RESULT_PRIORITY_IMAP,
		"imapx",
		_("IMAP server"),
		E_SOURCE_EXTENSION_MAIL_ACCOUNT);

	mail_autoconfig_result_to_config_lookup (mail_autoconfig, config_lookup,
		&mail_autoconfig->priv->pop3_result,
		E_CONFIG_LOOKUP_RESULT_PRIORITY_POP3,
		"pop",
		_("POP3 server"),
		E_SOURCE_EXTENSION_MAIL_ACCOUNT);

	mail_autoconfig_result_to_config_lookup (mail_autoconfig, config_lookup,
		&mail_autoconfig->priv->smtp_result,
		E_CONFIG_LOOKUP_RESULT_PRIORITY_SMTP,
		"smtp",
		_("SMTP server"),
		E_SOURCE_EXTENSION_MAIL_TRANSPORT);

	if (mail_autoconfig->priv->custom_types) {
		g_signal_emit (mail_autoconfig, signals[PROCESS_CUSTOM_TYPES], 0,
			config_lookup, mail_autoconfig->priv->custom_types);
	}
}
