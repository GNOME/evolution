/*
 * camel-sasl-xoauth.c
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

#include "camel-sasl-xoauth.h"

#define CAMEL_SASL_XOAUTH_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), CAMEL_TYPE_SASL_XOAUTH, CamelSaslXOAuthPrivate))

#define HMAC_SHA1_LEN 20 /* bytes, raw */

struct _CamelSaslXOAuthPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (CamelSaslXOAuth, camel_sasl_xoauth, CAMEL_TYPE_SASL)

static void
sasl_xoauth_append_request (GByteArray *byte_array,
                            const gchar *request_uri,
                            const gchar *consumer_key,
                            const gchar *consumer_secret,
                            const gchar *access_token,
                            const gchar *access_token_secret)
{
	GString *query;
	GString *base_string;
	GString *signing_key;
	GString *request;
	GHashTable *parameters;
	GList *keys;
	GList *iter;
	GHmac *signature_hmac;
	guchar signature_digest[HMAC_SHA1_LEN];
	gsize signature_digest_len;
	gchar *string;
	gpointer key, val;
	guint ii;

	const gchar *oauth_keys[] = {
		"oauth_version",
		"oauth_nonce",
		"oauth_timestamp",
		"oauth_consumer_key",
		"oauth_token",
		"oauth_signature_method",
		"oauth_signature"
	};

	parameters = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) g_free);

	/* Add OAuth parameters. */

	key = (gpointer) "oauth_version";
	g_hash_table_insert (parameters, key, g_strdup ("1.0"));

	key = (gpointer) "oauth_nonce";
	string = g_strdup_printf ("%u", g_random_int ());
	g_hash_table_insert (parameters, key, string); /* takes ownership */

	key = (gpointer) "oauth_timestamp";
	string = g_strdup_printf ("%" G_GINT64_FORMAT, (gint64) time (NULL));
	g_hash_table_insert (parameters, key, string); /* takes ownership */

	key = (gpointer) "oauth_consumer_key";
	g_hash_table_insert (parameters, key, g_strdup (consumer_key));

	key = (gpointer) "oauth_token";
	g_hash_table_insert (parameters, key, g_strdup (access_token));

	key = (gpointer) "oauth_signature_method";
	g_hash_table_insert (parameters, key, g_strdup ("HMAC-SHA1"));

	/* Build the query part of the signature base string.
	 * Parameters in the query part must be sorted by name. */

	query = g_string_sized_new (512);
	keys = g_hash_table_get_keys (parameters);
	keys = g_list_sort (keys, (GCompareFunc) g_strcmp0);
	for (iter = keys; iter != NULL; iter = iter->next) {
		key = iter->data;
		val = g_hash_table_lookup (parameters, key);

		if (iter != keys)
			g_string_append_c (query, '&');

		g_string_append_uri_escaped (query, key, NULL, FALSE);
		g_string_append_c (query, '=');
		g_string_append_uri_escaped (query, val, NULL, FALSE);
	}
	g_list_free (keys);

	/* Build the signature base string. */

	base_string = g_string_new (NULL);
	g_string_append (base_string, "GET");
	g_string_append_c (base_string, '&');
	g_string_append_uri_escaped (base_string, request_uri, NULL, FALSE);
	g_string_append_c (base_string, '&');
	g_string_append_uri_escaped (base_string, query->str, NULL, FALSE);

	/* Build the HMAC-SHA1 signing key. */

	signing_key = g_string_sized_new (512);
	g_string_append_uri_escaped (
		signing_key, consumer_secret, NULL, FALSE);
	g_string_append_c (signing_key, '&');
	g_string_append_uri_escaped (
		signing_key, access_token_secret, NULL, FALSE);

	/* Sign the request. */

	signature_digest_len = sizeof (signature_digest);

	signature_hmac = g_hmac_new (
		G_CHECKSUM_SHA1,
		(guchar *) signing_key->str,
		signing_key->len);
	g_hmac_update (
		signature_hmac,
		(guchar *) base_string->str,
		base_string->len);
	g_hmac_get_digest (
		signature_hmac,
		signature_digest,
		&signature_digest_len);
	g_hmac_unref (signature_hmac);

	key = (gpointer) "oauth_signature";
	string = g_base64_encode (signature_digest, signature_digest_len);
	g_hash_table_insert (parameters, key, string); /* takes ownership */

	/* Build the formal request string. */

	/* The request is easier to assemble with a GString. */
	request = g_string_sized_new (512);

	g_string_append_printf (request, "GET %s ", request_uri);

	for (ii = 0; ii < G_N_ELEMENTS (oauth_keys); ii++) {
		key = (gpointer) oauth_keys[ii];
		val = g_hash_table_lookup (parameters, key);

		if (ii > 0)
			g_string_append_c (request, ',');

		g_string_append (request, key);
		g_string_append_c (request, '=');
		g_string_append_c (request, '"');
		g_string_append_uri_escaped (request, val, NULL, FALSE);
		g_string_append_c (request, '"');
	}

	/* Copy the GString content to the GByteArray. */
	g_byte_array_append (
		byte_array, (guint8 *) request->str, request->len + 1);

	g_string_free (request, TRUE);

	/* Clean up. */

	g_string_free (query, TRUE);
	g_string_free (base_string, TRUE);
	g_string_free (signing_key, TRUE);

	g_hash_table_unref (parameters);
}

/****************************************************************************/

static gchar *
sasl_xoauth_find_account_id (ESourceRegistry *registry,
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
sasl_xoauth_get_account_by_id (GoaClient *client,
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
sasl_xoauth_challenge_sync (CamelSasl *sasl,
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
	account_id = sasl_xoauth_find_account_id (registry, uid);
	goa_object = sasl_xoauth_get_account_by_id (goa_client, account_id);

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
		GoaOAuthBased *goa_oauth_based;
		const gchar *identity;
		const gchar *consumer_key;
		const gchar *consumer_secret;
		const gchar *service_type;
		gchar *access_token = NULL;
		gchar *access_token_secret = NULL;
		gchar *request_uri;

		goa_oauth_based = goa_object_get_oauth_based (goa_object);

		identity = goa_account_get_identity (goa_account);
		service_type = CAMEL_IS_STORE (service) ? "imap" : "smtp";

		/* FIXME This should probably be generalized. */
		request_uri = g_strdup_printf (
			"https://mail.google.com/mail/b/%s/%s/",
			identity, service_type);

		consumer_key =
			goa_oauth_based_get_consumer_key (goa_oauth_based);
		consumer_secret =
			goa_oauth_based_get_consumer_secret (goa_oauth_based);

		success = goa_oauth_based_call_get_access_token_sync (
			goa_oauth_based,
			&access_token,
			&access_token_secret,
			NULL,
			cancellable,
			error);

		if (success) {
			byte_array = g_byte_array_new ();
			sasl_xoauth_append_request (
				byte_array,
				request_uri,
				consumer_key,
				consumer_secret,
				access_token,
				access_token_secret);
		}

		g_free (access_token);
		g_free (access_token_secret);
		g_free (request_uri);

		g_object_unref (goa_oauth_based);
	}

	g_object_unref (goa_account);
	g_object_unref (goa_object);
	g_object_unref (goa_client);

	/* IMAP and SMTP services will Base64-encode the request. */

	return byte_array;
}

static gpointer
camel_sasl_xoauth_auth_type_init (gpointer unused)
{
	CamelServiceAuthType *auth_type;

	/* This is a one-time allocation, never freed. */
	auth_type = g_malloc0 (sizeof (CamelServiceAuthType));
	auth_type->name = _("OAuth");
	auth_type->description =
		_("This option will connect to the server by "
		  "way of the GNOME Online Accounts service");
	auth_type->authproto = "XOAUTH";
	auth_type->need_password = FALSE;

	return auth_type;
}

static void
camel_sasl_xoauth_class_init (CamelSaslXOAuthClass *class)
{
	static GOnce auth_type_once = G_ONCE_INIT;
	CamelSaslClass *sasl_class;

	g_once (&auth_type_once, camel_sasl_xoauth_auth_type_init, NULL);

	g_type_class_add_private (class, sizeof (CamelSaslXOAuthPrivate));

	sasl_class = CAMEL_SASL_CLASS (class);
	sasl_class->auth_type = auth_type_once.retval;
	sasl_class->challenge_sync = sasl_xoauth_challenge_sync;
}

static void
camel_sasl_xoauth_class_finalize (CamelSaslXOAuthClass *class)
{
}

static void
camel_sasl_xoauth_init (CamelSaslXOAuth *sasl)
{
	sasl->priv = CAMEL_SASL_XOAUTH_GET_PRIVATE (sasl);
}

void
camel_sasl_xoauth_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	camel_sasl_xoauth_register_type (type_module);
}
