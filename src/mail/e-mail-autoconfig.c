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

#define E_MAIL_AUTOCONFIG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_AUTOCONFIG, EMailAutoconfigPrivate))

#define AUTOCONFIG_BASE_URI \
	"https://api.gnome.org/evolution/autoconfig/1.1/"

#define ERROR_IS_NOT_FOUND(error) \
	(g_error_matches ((error), SOUP_HTTP_ERROR, SOUP_STATUS_NOT_FOUND))

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
	EMailAutoconfigResult imap_result;
	EMailAutoconfigResult pop3_result;
	EMailAutoconfigResult smtp_result;
};

struct _ParserClosure {
	EMailAutoconfig *autoconfig;
	EMailAutoconfigResult *result;
};

enum {
	PROP_0,
	PROP_EMAIL_ADDRESS,
	PROP_REGISTRY
};

/* Forward Declarations */
static void	e_mail_autoconfig_initable_init	(GInitableIface *iface);

/* By default, the GAsyncInitable interface calls GInitable.init()
 * from a separate thread, so we only have to override GInitable. */
G_DEFINE_TYPE_WITH_CODE (
	EMailAutoconfig,
	e_mail_autoconfig,
	G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (
		G_TYPE_INITABLE, e_mail_autoconfig_initable_init)
	G_IMPLEMENT_INTERFACE (
		G_TYPE_ASYNC_INITABLE, NULL))

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

	if (is_incoming_server || is_outgoing_server)
		closure->result = NULL;
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

	if (closure->result == NULL)
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
			substitute = priv->email_domain_part;

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
		closure->result->host = g_strdup (string->str);
		closure->result->set = TRUE;

	} else if (g_str_equal (element_name, "username")) {
		closure->result->user = g_strdup (string->str);
		closure->result->set = TRUE;

	} else if (g_str_equal (element_name, "port")) {
		glong port = strtol (string->str, NULL, 10);
		if (port == CLAMP (port, 1, G_MAXUINT16)) {
			closure->result->port = (guint16) port;
			closure->result->set = TRUE;
		}

	} else if (g_str_equal (element_name, "socketType")) {
		if (g_str_equal (string->str, "plain")) {
			closure->result->security_method =
				CAMEL_NETWORK_SECURITY_METHOD_NONE;
			closure->result->set = TRUE;
		} else if (g_str_equal (string->str, "SSL")) {
			closure->result->security_method =
				CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT;
			closure->result->set = TRUE;
		} else if (g_str_equal (string->str, "STARTTLS")) {
			closure->result->security_method =
				CAMEL_NETWORK_SECURITY_METHOD_STARTTLS_ON_STANDARD_PORT;
			closure->result->set = TRUE;
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

			closure->result->auth_mechanism = auth_mechanism;
			closure->result->set = TRUE;
		}

		/* "password-encrypted" apparently maps to CRAM-MD5,
		 * or at least that's how Thunderbird interprets it. */

		if (g_str_equal (string->str, "password-encrypted")) {
			closure->result->auth_mechanism = g_strdup ("CRAM-MD5");
			closure->result->set = TRUE;
		}

		/* XXX Other <authentication> values not handled,
		 *     but they are corner cases for the most part. */
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
	GList *records;
	gchar *name_server = NULL;

	resolver = g_resolver_get_default ();

	records = g_resolver_lookup_records (
		resolver, domain, G_RESOLVER_RECORD_NS, cancellable, error);

	/* This list is sorted per RFC 2782, so use the first item. */
	if (records != NULL) {
		GVariant *variant = records->data;
		g_variant_get_child (variant, 0, "s", &name_server);
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
mail_autoconfig_lookup (EMailAutoconfig *autoconfig,
                        const gchar *domain,
                        GCancellable *cancellable,
                        GError **error)
{
	GMarkupParseContext *context;
	ESourceRegistry *registry;
	ESource *proxy_source;
	SoupMessage *soup_message;
	SoupSession *soup_session;
	ParserClosure closure;
	gulong cancel_id = 0;
	gboolean success;
	guint status;
	gchar *uri;

	registry = e_mail_autoconfig_get_registry (autoconfig);
	proxy_source = e_source_registry_ref_builtin_proxy (registry);

	soup_session = soup_session_new_with_options (
		SOUP_SESSION_PROXY_RESOLVER,
		G_PROXY_RESOLVER (proxy_source),
		NULL);

	g_object_unref (proxy_source);

	uri = g_strconcat (AUTOCONFIG_BASE_URI, domain, NULL);

	soup_message = soup_message_new (SOUP_METHOD_GET, uri);
	g_free (uri);

	if (G_IS_CANCELLABLE (cancellable))
		cancel_id = g_cancellable_connect (
			cancellable,
			G_CALLBACK (mail_autoconfig_abort_soup_session_cb),
			g_object_ref (soup_session),
			(GDestroyNotify) g_object_unref);

	status = soup_session_send_message (soup_session, soup_message);

	if (cancel_id > 0)
		g_cancellable_disconnect (cancellable, cancel_id);

	success = SOUP_STATUS_IS_SUCCESSFUL (status);

	if (!success) {
		g_set_error_literal (
			error, SOUP_HTTP_ERROR,
			soup_message->status_code,
			soup_message->reason_phrase);
		goto exit;
	}

	closure.autoconfig = autoconfig;
	closure.result = NULL;

	context = g_markup_parse_context_new (
		&mail_autoconfig_parser, 0,
		&closure, (GDestroyNotify) NULL);

	success = g_markup_parse_context_parse (
		context,
		soup_message->response_body->data,
		soup_message->response_body->length,
		error);

	if (success)
		success = g_markup_parse_context_end_parse (context, error);

	g_markup_parse_context_free (context);

exit:
	g_object_unref (soup_message);
	g_object_unref (soup_session);

	return success;
}

static gboolean
mail_autoconfig_set_details (EMailAutoconfigResult *result,
                             ESource *source,
                             const gchar *extension_name)
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
	extension_name = e_source_camel_get_extension_name (backend_name);
	camel_ext = e_source_get_extension (source, extension_name);

	settings = e_source_camel_get_settings (camel_ext);
	g_return_val_if_fail (CAMEL_IS_NETWORK_SETTINGS (settings), FALSE);

	g_object_set (
		settings,
		"user", result->user,
		"host", result->host,
		"port", result->port,
		"auth-mechanism", result->auth_mechanism,
		"security-method", result->security_method,
		NULL);

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
					    ESource *source)
{
	EMailConfigLookupResult *mail_result;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_LOOKUP_RESULT (lookup_result), FALSE);

	mail_result = E_MAIL_CONFIG_LOOKUP_RESULT (lookup_result);

	/* No chain up to parent method, not needed here, because not used */
	return mail_autoconfig_set_details (&mail_result->result, source, mail_result->extension_name);
}

static void
mail_config_lookup_result_finalize (GObject *object)
{
	EMailConfigLookupResult *mail_result = E_MAIL_CONFIG_LOOKUP_RESULT (object);

	g_free (mail_result->result.user);
	g_free (mail_result->result.host);
	g_free (mail_result->result.auth_mechanism);
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

static void
mail_autoconfig_set_email_address (EMailAutoconfig *autoconfig,
                                   const gchar *email_address)
{
	g_return_if_fail (email_address != NULL);
	g_return_if_fail (autoconfig->priv->email_address == NULL);

	autoconfig->priv->email_address = g_strdup (email_address);
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
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_autoconfig_dispose (GObject *object)
{
	EMailAutoconfigPrivate *priv;

	priv = E_MAIL_AUTOCONFIG_GET_PRIVATE (object);

	g_clear_object (&priv->registry);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_autoconfig_parent_class)->dispose (object);
}

static void
mail_autoconfig_finalize (GObject *object)
{
	EMailAutoconfigPrivate *priv;

	priv = E_MAIL_AUTOCONFIG_GET_PRIVATE (object);

	g_free (priv->email_address);
	g_free (priv->email_local_part);
	g_free (priv->email_domain_part);

	g_free (priv->imap_result.user);
	g_free (priv->imap_result.host);
	g_free (priv->imap_result.auth_mechanism);
	g_free (priv->pop3_result.user);
	g_free (priv->pop3_result.host);
	g_free (priv->pop3_result.auth_mechanism);
	g_free (priv->smtp_result.user);
	g_free (priv->smtp_result.host);
	g_free (priv->smtp_result.auth_mechanism);

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
	gchar *name_server;
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

	/* First try the email address domain verbatim. */
	success = mail_autoconfig_lookup (
		autoconfig, domain, cancellable, &local_error);

	g_warn_if_fail (
		(success && local_error == NULL) ||
		(!success && local_error != NULL));

	if (success)
		return TRUE;

	/* "404 Not Found" errors are non-fatal this time around. */
	if (ERROR_IS_NOT_FOUND (local_error)) {
		g_clear_error (&local_error);
	} else {
		g_propagate_error (error, local_error);
		return FALSE;
	}

	/* Look up an authoritative name server for the email address
	 * domain according to its "name server" (NS) DNS record. */
	name_server = mail_autoconfig_resolve_name_server (
		domain, cancellable, error);

	if (name_server == NULL)
		return FALSE;

	/* Widdle away segments of the name server domain until
	 * we find a match, or until we widdle down to nothing. */

	cp = name_server;
	while (cp != NULL && strchr (cp, '.') != NULL) {
		g_clear_error (&local_error);

		success = mail_autoconfig_lookup (
			autoconfig, cp, cancellable, &local_error);

		g_warn_if_fail (
			(success && local_error == NULL) ||
			(!success && local_error != NULL));

		if (success || !ERROR_IS_NOT_FOUND (local_error))
			break;

		cp = strchr (cp, '.');
		if (cp != NULL)
			cp++;
	}

	if (local_error != NULL)
		g_propagate_error (error, local_error);

	g_free (name_server);

	return success;
}

static void
e_mail_autoconfig_class_init (EMailAutoconfigClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EMailAutoconfigPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_autoconfig_set_property;
	object_class->get_property = mail_autoconfig_get_property;
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
}

static void
e_mail_autoconfig_initable_init (GInitableIface *iface)
{
	iface->init = mail_autoconfig_initable_init;
}

static void
e_mail_autoconfig_init (EMailAutoconfig *autoconfig)
{
	autoconfig->priv = E_MAIL_AUTOCONFIG_GET_PRIVATE (autoconfig);
}

EMailAutoconfig *
e_mail_autoconfig_new_sync (ESourceRegistry *registry,
                            const gchar *email_address,
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
		NULL);
}

void
e_mail_autoconfig_new (ESourceRegistry *registry,
                       const gchar *email_address,
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

gboolean
e_mail_autoconfig_set_imap_details (EMailAutoconfig *autoconfig,
                                    ESource *imap_source)
{
	g_return_val_if_fail (E_IS_MAIL_AUTOCONFIG (autoconfig), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (imap_source), FALSE);

	return mail_autoconfig_set_details (
		&autoconfig->priv->imap_result,
		imap_source, E_SOURCE_EXTENSION_MAIL_ACCOUNT);
}

gboolean
e_mail_autoconfig_set_pop3_details (EMailAutoconfig *autoconfig,
                                    ESource *pop3_source)
{
	g_return_val_if_fail (E_IS_MAIL_AUTOCONFIG (autoconfig), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (pop3_source), FALSE);

	return mail_autoconfig_set_details (
		&autoconfig->priv->pop3_result,
		pop3_source, E_SOURCE_EXTENSION_MAIL_ACCOUNT);
}

gboolean
e_mail_autoconfig_set_smtp_details (EMailAutoconfig *autoconfig,
                                    ESource *smtp_source)
{
	g_return_val_if_fail (E_IS_MAIL_AUTOCONFIG (autoconfig), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (smtp_source), FALSE);

	return mail_autoconfig_set_details (
		&autoconfig->priv->smtp_result,
		smtp_source, E_SOURCE_EXTENSION_MAIL_TRANSPORT);
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
}
