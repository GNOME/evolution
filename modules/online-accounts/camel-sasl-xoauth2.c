/*
 * camel-sasl-xoauth2.c
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
 */

/* XXX Yeah, yeah... */
#define GOA_API_IS_SUBJECT_TO_CHANGE

#include <config.h>
#include <glib/gi18n-lib.h>

#include <goa/goa.h>

#include <libemail-engine/e-mail-session.h>

#include "camel-sasl-xoauth2.h"

#define CAMEL_SASL_XOAUTH2_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), CAMEL_TYPE_SASL_XOAUTH2, CamelSaslXOAuth2Private))

struct _CamelSaslXOAuth2Private {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (CamelSaslXOAuth2, camel_sasl_xoauth2, CAMEL_TYPE_SASL)

static void
sasl_xoauth2_append_request (GByteArray *byte_array,
                             const gchar *identity,
                             const gchar *access_token)
{
	GString *request;

	/* Compared to OAuth 1.0, this step is trivial. */

	/* The request is easier to assemble with a GString. */
	request = g_string_sized_new (512);

	g_string_append (request, "user=");
	g_string_append (request, identity);
	g_string_append_c (request, 1);
	g_string_append (request, "auth=Bearer ");
	g_string_append (request, access_token);
	g_string_append_c (request, 1);
	g_string_append_c (request, 1);

	/* Copy the GString content to the GByteArray. */
	g_byte_array_append (
		byte_array, (guint8 *) request->str, request->len);

	g_string_free (request, TRUE);
}

static gchar *
sasl_xoauth2_find_account_id (ESourceRegistry *registry,
                              const gchar *uid)
{
	ESource *source;
	ESource *ancestor;
	const gchar *extension_name;
	gchar *account_id = NULL;

	extension_name = E_SOURCE_EXTENSION_GOA;

	source = e_source_registry_ref_source (registry, uid);
	g_return_val_if_fail (source != NULL, NULL);

	ancestor = e_source_registry_find_extension (
		registry, source, extension_name);

	if (ancestor != NULL) {
		ESourceGoa *extension;

		extension = e_source_get_extension (ancestor, extension_name);
		account_id = e_source_goa_dup_account_id (extension);

		g_object_unref (ancestor);
	}

	g_object_unref (source);

	return account_id;
}

static GoaObject *
sasl_xoauth2_get_account_by_id (GoaClient *client,
                                const gchar *account_id)
{
	GoaObject *match = NULL;
	GList *list, *iter;

	list = goa_client_get_accounts (client);

	for (iter = list; iter != NULL; iter = g_list_next (iter)) {
		GoaObject *goa_object;
		GoaAccount *goa_account;
		const gchar *candidate_id;

		goa_object = GOA_OBJECT (iter->data);
		goa_account = goa_object_get_account (goa_object);
		candidate_id = goa_account_get_id (goa_account);

		if (g_strcmp0 (account_id, candidate_id) == 0)
			match = g_object_ref (goa_object);

		g_object_unref (goa_account);

		if (match != NULL)
			break;
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	return match;
}

static GByteArray *
sasl_xoauth2_challenge_sync (CamelSasl *sasl,
                             GByteArray *token,
                             GCancellable *cancellable,
                             GError **error)
{
	GoaClient *goa_client;
	GoaObject *goa_object;
	GoaAccount *goa_account;
	GByteArray *byte_array = NULL;
	CamelService *service;
	CamelSession *session;
	ESourceRegistry *registry;
	const gchar *uid;
	gchar *account_id;
	gboolean success;

	service = camel_sasl_get_service (sasl);
	session = camel_service_get_session (service);
	registry = e_mail_session_get_registry (E_MAIL_SESSION (session));

	goa_client = goa_client_new_sync (cancellable, error);
	if (goa_client == NULL)
		return NULL;

	uid = camel_service_get_uid (service);
	account_id = sasl_xoauth2_find_account_id (registry, uid);
	goa_object = sasl_xoauth2_get_account_by_id (goa_client, account_id);

	g_free (account_id);

	if (goa_object == NULL) {
		g_set_error_literal (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_CANT_AUTHENTICATE,
			_("Cannot find a corresponding account in "
			"the org.gnome.OnlineAccounts service from "
			"which to obtain an authentication token."));
		g_object_unref (goa_client);
		return NULL;
	}

	goa_account = goa_object_get_account (goa_object);

	success = goa_account_call_ensure_credentials_sync (
		goa_account, NULL, cancellable, error);

	if (success) {
		GoaOAuth2Based *goa_oauth2_based;
		gchar *access_token = NULL;

		goa_oauth2_based = goa_object_get_oauth2_based (goa_object);

		success = goa_oauth2_based_call_get_access_token_sync (
			goa_oauth2_based,
			&access_token, NULL,
			cancellable, error);

		if (success) {
			gchar *identity;

			identity = goa_account_dup_identity (goa_account);

			byte_array = g_byte_array_new ();
			sasl_xoauth2_append_request (
				byte_array, identity, access_token);

			g_free (identity);
		}

		g_free (access_token);

		g_object_unref (goa_oauth2_based);
	}

	g_object_unref (goa_account);
	g_object_unref (goa_object);
	g_object_unref (goa_client);

	/* IMAP and SMTP services will Base64-encode the request. */

	return byte_array;
}

static gpointer
camel_sasl_xoauth2_auth_type_init (gpointer unused)
{
	CamelServiceAuthType *auth_type;

	/* This is a one-time allocation, never freed. */
	auth_type = g_malloc0 (sizeof (CamelServiceAuthType));
	auth_type->name = _("OAuth2");
	auth_type->description =
		_("This option will connect to the server by "
		  "way of the GNOME Online Accounts service");
	auth_type->authproto = "XOAUTH2";
	auth_type->need_password = FALSE;

	return auth_type;
}

static void
camel_sasl_xoauth2_class_init (CamelSaslXOAuth2Class *class)
{
	static GOnce auth_type_once = G_ONCE_INIT;
	CamelSaslClass *sasl_class;

	g_once (&auth_type_once, camel_sasl_xoauth2_auth_type_init, NULL);

	g_type_class_add_private (class, sizeof (CamelSaslXOAuth2Private));

	sasl_class = CAMEL_SASL_CLASS (class);
	sasl_class->auth_type = auth_type_once.retval;
	sasl_class->challenge_sync = sasl_xoauth2_challenge_sync;
}

static void
camel_sasl_xoauth2_class_finalize (CamelSaslXOAuth2Class *class)
{
}

static void
camel_sasl_xoauth2_init (CamelSaslXOAuth2 *sasl)
{
	sasl->priv = CAMEL_SASL_XOAUTH2_GET_PRIVATE (sasl);
}

void
camel_sasl_xoauth2_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	camel_sasl_xoauth2_register_type (type_module);
}

