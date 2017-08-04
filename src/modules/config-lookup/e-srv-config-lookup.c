/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

G_DEFINE_DYNAMIC_TYPE (ESrvConfigLookup, e_srv_config_lookup, E_TYPE_EXTENSION)

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
		gint priority_base;
	} known_services[] = {
		{ "imaps",	E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE,  "imapx",   _("IMAP server"),    E_CONFIG_LOOKUP_RESULT_PRIORITY_IMAP },
		{ "imap",	E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE,  "imapx",   _("IMAP server"),    E_CONFIG_LOOKUP_RESULT_PRIORITY_IMAP + PRIORITY_OFFSET / 2 },
		{ "pop3s",	E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE,  "pop",     _("POP3 server"),    E_CONFIG_LOOKUP_RESULT_PRIORITY_POP3 },
		{ "pop3",	E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE,  "pop",     _("POP3 server"),    E_CONFIG_LOOKUP_RESULT_PRIORITY_POP3 + PRIORITY_OFFSET / 2 },
		{ "submission",	E_CONFIG_LOOKUP_RESULT_MAIL_SEND,     "smtp",    _("SMTP server"),    E_CONFIG_LOOKUP_RESULT_PRIORITY_SMTP },
		{ "caldavs",	E_CONFIG_LOOKUP_RESULT_COLLECTION,    "caldav",  _("CalDAV server"),  PRIORITY_DEFAULT },
		{ "caldav",	E_CONFIG_LOOKUP_RESULT_COLLECTION,    "caldav",  _("CalDAV server"),  PRIORITY_DEFAULT + PRIORITY_OFFSET / 2 },
		{ "carddavs",	E_CONFIG_LOOKUP_RESULT_COLLECTION,    "carddav", _("CardDAV server"), PRIORITY_DEFAULT },
		{ "carddav",	E_CONFIG_LOOKUP_RESULT_COLLECTION,    "carddav", _("CardDAV server"), PRIORITY_DEFAULT + PRIORITY_OFFSET / 2 },
		{ "ldaps",	E_CONFIG_LOOKUP_RESULT_ADDRESS_BOOK,  "ldap",    _("LDAP server"),    PRIORITY_DEFAULT },
		{ "ldap",	E_CONFIG_LOOKUP_RESULT_ADDRESS_BOOK,  "ldap",    _("LDAP server"),    PRIORITY_DEFAULT + PRIORITY_OFFSET / 2 }
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

		targets = g_resolver_lookup_service (resolver, known_services[ii].gio_protocol, "tcp", domain, cancellable, &local_error);

		if (local_error) {
			if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
				g_clear_error (&local_error);
				break;
			}
			g_clear_error (&local_error);
		} else {
			GList *link;
			gint index = 0;

			targets = g_srv_target_list_sort (targets);

			for (link = targets; link; index++, link = g_list_next (link)) {
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
					known_services[ii].priority_base - PRIORITY_OFFSET,
					FALSE,
					known_services[ii].evo_protocol,
					known_services[ii].display_name,
					description);

				g_free (description);

				if (known_services[ii].kind == E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE ||
				    known_services[ii].kind == E_CONFIG_LOOKUP_RESULT_MAIL_SEND) {
					CamelNetworkSecurityMethod security_method;
					const gchar *extension_name;

					extension_name = e_source_camel_get_extension_name (known_services[ii].evo_protocol);

					e_config_lookup_result_simple_add_string (lookup_result, extension_name, "host", hostname);
					e_config_lookup_result_simple_add_uint (lookup_result, extension_name, "port", g_srv_target_get_port (target));
					e_config_lookup_result_simple_add_string (lookup_result, extension_name, "user", email_address);

					if (g_str_has_suffix (known_services[ii].gio_protocol, "s"))
						security_method = CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT;
					else
						security_method = CAMEL_NETWORK_SECURITY_METHOD_STARTTLS_ON_STANDARD_PORT;

					e_config_lookup_result_simple_add_enum (lookup_result, extension_name, "security-method",
						CAMEL_TYPE_NETWORK_SECURITY_METHOD, security_method);

					if (known_services[ii].kind == E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE)
						extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
					else
						extension_name = E_SOURCE_EXTENSION_MAIL_TRANSPORT;

					e_config_lookup_result_simple_add_string (lookup_result, extension_name,
						"backend-name", known_services[ii].evo_protocol);
				} else if (known_services[ii].kind == E_CONFIG_LOOKUP_RESULT_COLLECTION) {
					gboolean is_calendar = g_str_equal (known_services[ii].evo_protocol, "caldav");
					gchar *url;

					url = g_strdup_printf ("%s://%s:%d",
						g_str_has_suffix (known_services[ii].gio_protocol, "s") ? "https" : "http",
						hostname, g_srv_target_get_port (target));

					e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_COLLECTION,
						"backend-name", "webdav");

					e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_COLLECTION,
						is_calendar ? "calendar-url" : "contacts-url", url);

					g_free (url);
				} else if (known_services[ii].kind == E_CONFIG_LOOKUP_RESULT_ADDRESS_BOOK) {
					ESourceLDAPSecurity security;

					e_config_lookup_result_simple_add_string (lookup_result, NULL, "parent", "ldap-stub");
					e_config_lookup_result_simple_add_string (lookup_result, E_SOURCE_EXTENSION_AUTHENTICATION, "host", hostname);
					e_config_lookup_result_simple_add_uint (lookup_result, E_SOURCE_EXTENSION_AUTHENTICATION, "port", g_srv_target_get_port (target));

					if (g_str_equal (known_services[ii].gio_protocol, "ldaps"))
						security = E_SOURCE_LDAP_SECURITY_LDAPS;
					else
						security = E_SOURCE_LDAP_SECURITY_NONE;

					e_config_lookup_result_simple_add_enum (lookup_result,
						E_SOURCE_EXTENSION_LDAP_BACKEND, "security-method",
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

static void
srv_config_lookup_thread (EConfigLookup *config_lookup,
			  const ENamedParameters *params,
			  gpointer user_data,
			  GCancellable *cancellable)
{
	const gchar *email_address;
	gchar *domain;

	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	g_return_if_fail (params != NULL);

	email_address = e_named_parameters_get (params, E_CONFIG_LOOKUP_PARAM_EMAIL_ADDRESS);

	if (!email_address || !*email_address)
		return;

	domain = strchr (email_address, '@');
	if (!domain)
		return;

	domain = g_strdup (domain + 1);

	srv_config_lookup_domain_sync (config_lookup, email_address, domain, cancellable);

	g_free (domain);
}

static void
srv_config_lookup_run_cb (EConfigLookup *config_lookup,
			  const ENamedParameters *params,
			  EActivity *activity,
			  gpointer user_data)
{
	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	g_return_if_fail (E_IS_SRV_CONFIG_LOOKUP (user_data));
	g_return_if_fail (E_IS_ACTIVITY (activity));

	e_config_lookup_create_thread (config_lookup, params, activity,
		srv_config_lookup_thread, NULL, NULL);
}

static void
srv_config_lookup_constructed (GObject *object)
{
	EConfigLookup *config_lookup;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_srv_config_lookup_parent_class)->constructed (object);

	config_lookup = E_CONFIG_LOOKUP (e_extension_get_extensible (E_EXTENSION (object)));

	g_signal_connect (config_lookup, "run",
		G_CALLBACK (srv_config_lookup_run_cb), object);
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
