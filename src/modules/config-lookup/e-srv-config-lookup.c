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

#include <string.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include "e-util/e-util.h"

#include "e-srv-config-lookup.h"

/*
 * This implements lookup as specified in RFC 6186 (for mail),
 * RFC 6764 (for CalDAV/CardDAV), RFC 8341 (for 'sumbissions')
 */

/* Standard GObject macros */
#define E_TYPE_SRV_CONFIG_LOOKUP \
	(e_srv_config_lookup_get_type ())
#define E_SRV_CONFIG_LOOKUP(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SRV_CONFIG_LOOKUP, ESrvConfigLookup))
#define E_SRV_CONFIG_LOOKUP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SRV_CONFIG_LOOKUP, ESrvConfigLookupClass))
#define E_IS_SRV_CONFIG_LOOKUP(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SRV_CONFIG_LOOKUP))
#define E_IS_SRV_CONFIG_LOOKUP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SRV_CONFIG_LOOKUP))
#define E_SRV_CONFIG_LOOKUP_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SRV_CONFIG_LOOKUP, ESrvConfigLookupClass))

#define PRIORITY_OFFSET 100
#define PRIORITY_DEFAULT E_CONFIG_LOOKUP_RESULT_PRIORITY_IMAP

typedef struct _ESrvConfigLookup ESrvConfigLookup;
typedef struct _ESrvConfigLookupClass ESrvConfigLookupClass;

struct _ESrvConfigLookup {
	EExtension parent;
};

struct _ESrvConfigLookupClass {
	EExtensionClass parent_class;
};

GType e_srv_config_lookup_get_type (void) G_GNUC_CONST;

static void srv_config_lookup_worker_iface_init (EConfigLookupWorkerInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ESrvConfigLookup, e_srv_config_lookup, E_TYPE_EXTENSION, 0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (E_TYPE_CONFIG_LOOKUP_WORKER, srv_config_lookup_worker_iface_init))

static void
srv_config_lookup_domain_sync (EConfigLookup *config_lookup,
			       const gchar *email_address,
			       const gchar *domain,
			       GCancellable *cancellable)
{
	struct _services {
		const gchar *gio_protocol;
		EConfigLookupResultKind kind;
		const gchar *evo_protocol;
		const gchar *display_name;
		const gchar *lookup_info;
		gint priority_base;
	} known_services[] = {
		{ "imaps",	E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE,  "imapx",   N_("IMAP server"),    N_("Looking up IMAP server…"),    E_CONFIG_LOOKUP_RESULT_PRIORITY_IMAP },
		{ "imap",	E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE,  "imapx",   N_("IMAP server"),    N_("Looking up IMAP server…"),    E_CONFIG_LOOKUP_RESULT_PRIORITY_IMAP + PRIORITY_OFFSET / 2 },
		{ "pop3s",	E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE,  "pop",     N_("POP3 server"),    N_("Looking up POP3 server…"),    E_CONFIG_LOOKUP_RESULT_PRIORITY_POP3 },
		{ "pop3",	E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE,  "pop",     N_("POP3 server"),    N_("Looking up POP3 server…"),    E_CONFIG_LOOKUP_RESULT_PRIORITY_POP3 + PRIORITY_OFFSET / 2 },
		{ "submissions",E_CONFIG_LOOKUP_RESULT_MAIL_SEND,     "smtp",    N_("SMTP server"),    N_("Looking up SMTP server…"),    E_CONFIG_LOOKUP_RESULT_PRIORITY_SMTP },
		{ "submission",	E_CONFIG_LOOKUP_RESULT_MAIL_SEND,     "smtp",    N_("SMTP server"),    N_("Looking up SMTP server…"),    E_CONFIG_LOOKUP_RESULT_PRIORITY_SMTP + PRIORITY_OFFSET / 2 },
		{ "caldavs",	E_CONFIG_LOOKUP_RESULT_COLLECTION,    "caldav",  N_("CalDAV server"),  N_("Looking up CalDAV server…"),  PRIORITY_DEFAULT },
		{ "caldav",	E_CONFIG_LOOKUP_RESULT_COLLECTION,    "caldav",  N_("CalDAV server"),  N_("Looking up CalDAV server…"),  PRIORITY_DEFAULT + PRIORITY_OFFSET / 2 },
		{ "carddavs",	E_CONFIG_LOOKUP_RESULT_COLLECTION,    "carddav", N_("CardDAV server"), N_("Looking up CardDAV server…"), PRIORITY_DEFAULT },
		{ "carddav",	E_CONFIG_LOOKUP_RESULT_COLLECTION,    "carddav", N_("CardDAV server"), N_("Looking up CardDAV server…"), PRIORITY_DEFAULT + PRIORITY_OFFSET / 2 },
		{ "ldaps",	E_CONFIG_LOOKUP_RESULT_ADDRESS_BOOK,  "ldap",    N_("LDAP server"),    N_("Looking up LDAP server…"),    PRIORITY_DEFAULT },
		{ "ldap",	E_CONFIG_LOOKUP_RESULT_ADDRESS_BOOK,  "ldap",    N_("LDAP server"),    N_("Looking up LDAP server…"),    PRIORITY_DEFAULT + PRIORITY_OFFSET / 2 }
	};

	GResolver *resolver;
	gint ii;
	gboolean success = TRUE;

	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));

	if (!domain || !*domain)
		return;

	resolver = g_resolver_get_default ();

	for (ii = 0; success && ii < G_N_ELEMENTS (known_services); ii++) {
		GList *targets;
		GError *local_error = NULL;

		camel_operation_push_message (cancellable, "%s", _(known_services[ii].lookup_info));
		targets = g_resolver_lookup_service (resolver, known_services[ii].gio_protocol, "tcp", domain, cancellable, &local_error);
		camel_operation_pop_message (cancellable);

		if (local_error) {
			if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
				g_clear_error (&local_error);
				break;
			}
			g_clear_error (&local_error);
		} else {
			GList *link;

			for (link = targets; link; link = g_list_next (link)) {
				EConfigLookupResult *lookup_result;
				GSrvTarget *target = link->data;
				const gchar *hostname;
				gchar *description;

				if (!target)
					continue;

				hostname = g_srv_target_get_hostname (target);
				if (!hostname || !*hostname)
					continue;

				description = g_strdup_printf ("%s:%d", hostname, g_srv_target_get_port (target));

				lookup_result = e_config_lookup_result_simple_new (known_services[ii].kind,
					known_services[ii].priority_base + PRIORITY_OFFSET,
					/* consider mail configs complete */
					known_services[ii].kind == E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE ||
					known_services[ii].kind == E_CONFIG_LOOKUP_RESULT_MAIL_SEND,
					known_services[ii].evo_protocol,
					_(known_services[ii].display_name),
					description,
					NULL);

				g_free (description);

				if (known_services[ii].kind == E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE ||
				    known_services[ii].kind == E_CONFIG_LOOKUP_RESULT_MAIL_SEND) {
					CamelNetworkSecurityMethod security_method;
					const gchar *extension_name;

					if (known_services[ii].kind == E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE)
						extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
					else
						extension_name = E_SOURCE_EXTENSION_MAIL_TRANSPORT;

					e_config_lookup_result_simple_add_string (lookup_result, extension_name,
						"backend-name", known_services[ii].evo_protocol);

					if (known_services[ii].kind == E_CONFIG_LOOKUP_RESULT_MAIL_SEND) {
						/* Preset authentication method for SMTP, thus it authenticates by default */
						e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_AUTHENTICATION,
							"method", "PLAIN");
					}

					extension_name = e_source_camel_get_extension_name (known_services[ii].evo_protocol);

					if (g_str_has_suffix (known_services[ii].gio_protocol, "s"))
						security_method = CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT;
					else
						security_method = CAMEL_NETWORK_SECURITY_METHOD_STARTTLS_ON_STANDARD_PORT;

					e_config_lookup_result_simple_add_enum (lookup_result, extension_name, "security-method",
						CAMEL_TYPE_NETWORK_SECURITY_METHOD, security_method);

					/* Set the security method before the port, to not have it overwritten
					   in New Mail Account wizard (binding callback). */
					e_config_lookup_result_simple_add_string (lookup_result, extension_name, "host", hostname);
					e_config_lookup_result_simple_add_uint (lookup_result, extension_name, "port", g_srv_target_get_port (target));
					e_config_lookup_result_simple_add_string (lookup_result, extension_name, "user", email_address);
				} else if (known_services[ii].kind == E_CONFIG_LOOKUP_RESULT_COLLECTION) {
					gboolean is_calendar = g_str_equal (known_services[ii].evo_protocol, "caldav");
					gboolean is_secure = g_str_has_suffix (known_services[ii].gio_protocol, "s");
					guint16 port = g_srv_target_get_port (target);
					const gchar *txt_path = is_calendar ? ".well-known/caldav" : ".well-known/carddav";
					GList *txt_records;
					gchar *url, *tmp;

					tmp = g_strconcat (is_calendar ? "_caldav" : "_carddav", is_secure ? "s" : "", "._tcp.", domain, NULL);
					txt_records = g_resolver_lookup_records (resolver, tmp, G_RESOLVER_RECORD_TXT, cancellable, NULL);
					g_clear_pointer (&tmp, g_free);

					if (txt_records) {
						GList *txt_record;

						for (txt_record = txt_records; txt_record; txt_record = g_list_next (txt_record)) {
							gchar **contents = NULL;
							gint jj;

							g_variant_get (txt_record->data, "(^a&s)", &contents);

							for (jj = 0; contents && contents[jj]; jj++) {
								const gchar *txt_value = contents[jj];

								/* Compare case-insensitively, according to section 6.4 of RFC 6763 */
								if (!g_ascii_strncasecmp ("path=/", txt_value, 6)) {
									tmp = g_strdup (txt_value + 6);
									txt_path = tmp;
									break;
								}
							}

							g_free (contents);

							if (tmp)
								break;
						}

						g_list_free_full (txt_records, (GDestroyNotify) g_variant_unref);
					}

					if ((!is_secure && port == 80) || (is_secure && port == 443))
						url = g_strdup_printf ("http%s://%s/%s", is_secure ? "s" : "", hostname, txt_path);
					else
						url = g_strdup_printf ("http%s://%s:%d/%s", is_secure ? "s" : "", hostname, port, txt_path);

					g_free (tmp);

					e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_COLLECTION,
						"backend-name", "webdav");

					e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_COLLECTION,
						"identity", email_address);

					e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_COLLECTION,
						is_calendar ? "calendar-url" : "contacts-url", url);

					g_free (url);
				} else if (known_services[ii].kind == E_CONFIG_LOOKUP_RESULT_ADDRESS_BOOK) {
					ESourceLDAPSecurity security;

					e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_ADDRESS_BOOK,
						"backend-name", "ldap");

					e_config_lookup_result_simple_add_string (lookup_result, NULL, "parent", "ldap-stub");
					e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_AUTHENTICATION, "host", hostname);
					e_config_lookup_result_simple_add_uint (lookup_result, E_SOURCE_EXTENSION_AUTHENTICATION, "port", g_srv_target_get_port (target));

					e_config_lookup_result_simple_add_enum (lookup_result,
						E_SOURCE_EXTENSION_LDAP_BACKEND, "scope",
						E_TYPE_SOURCE_LDAP_SCOPE, E_SOURCE_LDAP_SCOPE_SUBTREE);

					if (g_str_equal (known_services[ii].gio_protocol, "ldaps"))
						security = E_SOURCE_LDAP_SECURITY_LDAPS;
					else
						security = E_SOURCE_LDAP_SECURITY_STARTTLS;

					e_config_lookup_result_simple_add_enum (lookup_result,
						E_SOURCE_EXTENSION_LDAP_BACKEND, "security",
						E_TYPE_SOURCE_LDAP_SECURITY, security);
				} else {
					g_warn_if_reached ();
				}

				e_config_lookup_add_result (config_lookup, lookup_result);
			}

			g_list_free_full (targets, (GDestroyNotify) g_srv_target_free);
		}
	}

	g_object_unref (resolver);
}

static const gchar *
srv_config_lookup_worker_get_display_name (EConfigLookupWorker *worker)
{
	return _("Look up in SRV records");
}

static void
srv_config_lookup_worker_run (EConfigLookupWorker *lookup_worker,
			      EConfigLookup *config_lookup,
			      const ENamedParameters *params,
			      ENamedParameters **out_restart_params,
			      GCancellable *cancellable,
			      GError **error)
{
	const gchar *email_address;
	const gchar *domain;
	const gchar *servers;

	g_return_if_fail (E_IS_SRV_CONFIG_LOOKUP (lookup_worker));
	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	g_return_if_fail (params != NULL);

	email_address = e_named_parameters_get (params, E_CONFIG_LOOKUP_PARAM_EMAIL_ADDRESS);

	if (email_address && *email_address) {
		domain = strchr (email_address, '@');
		if (domain && *domain)
			srv_config_lookup_domain_sync (config_lookup, email_address, domain + 1, cancellable);
	}

	if (!email_address)
		email_address = "";

	servers = e_named_parameters_get (params, E_CONFIG_LOOKUP_PARAM_SERVERS);
	if (servers && *servers) {
		gchar **servers_strv;
		gint ii;

		servers_strv = g_strsplit (servers, ";", 0);

		for (ii = 0; servers_strv && servers_strv[ii] && !g_cancellable_is_cancelled (cancellable); ii++) {
			domain = servers_strv[ii];

			if (domain && *domain)
				srv_config_lookup_domain_sync (config_lookup, email_address, domain, cancellable);
		}

		g_strfreev (servers_strv);
	}
}

static void
srv_config_lookup_constructed (GObject *object)
{
	EConfigLookup *config_lookup;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_srv_config_lookup_parent_class)->constructed (object);

	config_lookup = E_CONFIG_LOOKUP (e_extension_get_extensible (E_EXTENSION (object)));

	e_config_lookup_register_worker (config_lookup, E_CONFIG_LOOKUP_WORKER (object));
}

static void
e_srv_config_lookup_class_init (ESrvConfigLookupClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = srv_config_lookup_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CONFIG_LOOKUP;
}

static void
e_srv_config_lookup_class_finalize (ESrvConfigLookupClass *class)
{
}

static void
srv_config_lookup_worker_iface_init (EConfigLookupWorkerInterface *iface)
{
	iface->get_display_name = srv_config_lookup_worker_get_display_name;
	iface->run = srv_config_lookup_worker_run;
}

static void
e_srv_config_lookup_init (ESrvConfigLookup *extension)
{
}

void
e_srv_config_lookup_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_srv_config_lookup_register_type (type_module);
}
