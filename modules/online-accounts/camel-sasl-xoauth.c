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

struct _CamelSaslXOAuthPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (CamelSaslXOAuth, camel_sasl_xoauth, CAMEL_TYPE_SASL)

/*****************************************************************************
 * This is based on an old revision of gnome-online-accounts
 * which demonstrated OAuth authentication with an IMAP server.
 *
 * See commit 5bcbe2a3eac4821892680e0655b27ab8c128ab15
 *****************************************************************************/

#include <libsoup/soup.h>

#define OAUTH_ENCODE_STRING(str) \
	(str ? soup_uri_encode ((str), "!$&'()*+,;=@") : g_strdup (""))

#define SHA1_BLOCK_SIZE 64
#define SHA1_LENGTH 20

/*
 * hmac_sha1:
 * @key: The key
 * @message: The message
 *
 * Given the key and message, compute the HMAC-SHA1 hash and return the base-64
 * encoding of it.  This is very geared towards OAuth, and as such both key and
 * message must be NULL-terminated strings, and the result is base-64 encoded.
 */
static gchar *
hmac_sha1 (const gchar *key,
           const gchar *message)
{
	GChecksum *checksum;
	gchar *real_key;
	guchar ipad[SHA1_BLOCK_SIZE];
	guchar opad[SHA1_BLOCK_SIZE];
	guchar inner[SHA1_LENGTH];
	guchar digest[SHA1_LENGTH];
	gsize key_length, inner_length, digest_length;
	gint i;

	g_return_val_if_fail (key, NULL);
	g_return_val_if_fail (message, NULL);

	checksum = g_checksum_new (G_CHECKSUM_SHA1);

	/* If the key is longer than the block size, hash it first */
	if (strlen (key) > SHA1_BLOCK_SIZE) {
		guchar new_key[SHA1_LENGTH];

		key_length = sizeof (new_key);

		g_checksum_update (checksum, (guchar *) key, strlen (key));
		g_checksum_get_digest (checksum, new_key, &key_length);
		g_checksum_reset (checksum);

		real_key = g_memdup (new_key, key_length);
	} else {
		real_key = g_strdup (key);
		key_length = strlen (key);
	}

	/* Sanity check the length */
	g_assert (key_length <= SHA1_BLOCK_SIZE);

	/* Protect against use of the provided key by NULLing it */
	key = NULL;

	/* Stage 1 */
	memset (ipad, 0, sizeof (ipad));
	memset (opad, 0, sizeof (opad));

	memcpy (ipad, real_key, key_length);
	memcpy (opad, real_key, key_length);

	/* Stage 2 and 5 */
	for (i = 0; i < sizeof (ipad); i++) {
		ipad[i] ^= 0x36;
		opad[i] ^= 0x5C;
	}

	/* Stage 3 and 4 */
	g_checksum_update (checksum, ipad, sizeof (ipad));
	g_checksum_update (checksum, (guchar *) message, strlen (message));
	inner_length = sizeof (inner);
	g_checksum_get_digest (checksum, inner, &inner_length);
	g_checksum_reset (checksum);

	/* Stage 6 and 7 */
	g_checksum_update (checksum, opad, sizeof (opad));
	g_checksum_update (checksum, inner, inner_length);

	digest_length = sizeof (digest);
	g_checksum_get_digest (checksum, digest, &digest_length);

	g_checksum_free (checksum);
	g_free (real_key);

	return g_base64_encode (digest, digest_length);
}

static gchar *
sign_plaintext (const gchar *consumer_secret,
                const gchar *token_secret)
{
	gchar *cs;
	gchar *ts;
	gchar *rv;

	cs = OAUTH_ENCODE_STRING (consumer_secret);
	ts = OAUTH_ENCODE_STRING (token_secret);
	rv = g_strconcat (cs, "&", ts, NULL);

	g_free (cs);
	g_free (ts);

	return rv;
}

static gchar *
sign_hmac (const gchar *consumer_secret,
           const gchar *token_secret,
           const gchar *http_method,
           const gchar *request_uri,
           const gchar *encoded_params)
{
	GString *text;
	gchar *signature;
	gchar *key;

	text = g_string_new (NULL);
	g_string_append (text, http_method);
	g_string_append_c (text, '&');
	g_string_append_uri_escaped (text, request_uri, NULL, FALSE);
	g_string_append_c (text, '&');
	g_string_append_uri_escaped (text, encoded_params, NULL, FALSE);

	/* PLAINTEXT signature value is the HMAC-SHA1 key value */
	key = sign_plaintext (consumer_secret, token_secret);
	signature = hmac_sha1 (key, text->str);
	g_free (key);

	g_string_free (text, TRUE);

	return signature;
}

static GHashTable *
calculate_xoauth_params (const gchar *request_uri,
                         const gchar *consumer_key,
                         const gchar *consumer_secret,
                         const gchar *access_token,
                         const gchar *access_token_secret)
{
	gchar *signature;
	GHashTable *params;
	gchar *nonce;
	gchar *timestamp;
	GList *keys;
	GList *iter;
	GString *normalized;
	gpointer key;

	nonce = g_strdup_printf ("%u", g_random_int ());
	timestamp = g_strdup_printf (
		"%" G_GINT64_FORMAT, (gint64) time (NULL));

	params = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) g_free);

	key = (gpointer) "oauth_consumer_key";
	g_hash_table_insert (params, key, g_strdup (consumer_key));

	key = (gpointer) "oauth_nonce";
	g_hash_table_insert (params, key, nonce); /* takes ownership */

	key = (gpointer) "oauth_timestamp";
	g_hash_table_insert (params, key, timestamp); /* takes ownership */

	key = (gpointer) "oauth_version";
	g_hash_table_insert (params, key, g_strdup ("1.0"));

	key = (gpointer) "oauth_signature_method";
	g_hash_table_insert (params, key, g_strdup ("HMAC-SHA1"));

	key = (gpointer) "oauth_token";
	g_hash_table_insert (params, key, g_strdup (access_token));

	normalized = g_string_new (NULL);
	keys = g_hash_table_get_keys (params);
	keys = g_list_sort (keys, (GCompareFunc) g_strcmp0);
	for (iter = keys; iter != NULL; iter = iter->next) {
		const gchar *key = iter->data;
		const gchar *value;
		gchar *k;
		gchar *v;

		value = g_hash_table_lookup (params, key);
		if (normalized->len > 0)
			g_string_append_c (normalized, '&');

		k = OAUTH_ENCODE_STRING (key);
		v = OAUTH_ENCODE_STRING (value);

		g_string_append_printf (normalized, "%s=%s", k, v);

		g_free (k);
		g_free (v);
	}
	g_list_free (keys);

	signature = sign_hmac (
		consumer_secret, access_token_secret,
		"GET", request_uri, normalized->str);

	key = (gpointer) "oauth_signature";
	g_hash_table_insert (params, key, signature); /* takes ownership */

	g_string_free (normalized, TRUE);

	return params;
}

static gchar *
calculate_xoauth_param (const gchar *request_uri,
                        const gchar *consumer_key,
                        const gchar *consumer_secret,
                        const gchar *access_token,
                        const gchar *access_token_secret)
{
	GString *str;
	GHashTable *params;
	GList *keys;
	GList *iter;

	params = calculate_xoauth_params (
		request_uri,
		consumer_key,
		consumer_secret,
		access_token,
		access_token_secret);

	str = g_string_new ("GET ");
	g_string_append (str, request_uri);
	g_string_append_c (str, ' ');
	keys = g_hash_table_get_keys (params);
	keys = g_list_sort (keys, (GCompareFunc) g_strcmp0);
	for (iter = keys; iter != NULL; iter = iter->next) {
		const gchar *key = iter->data;
		const gchar *value;
		gchar *k;
		gchar *v;

		value = g_hash_table_lookup (params, key);
		if (iter != keys)
			g_string_append_c (str, ',');

		k = OAUTH_ENCODE_STRING (key);
		v = OAUTH_ENCODE_STRING (value);
		g_string_append_printf (str, "%s=\"%s\"", k, v);
		g_free (k);
		g_free (v);
	}
	g_list_free (keys);

	g_hash_table_unref (params);

	return g_string_free (str, FALSE);
}

/****************************************************************************/

static gchar *
sasl_xoauth_find_account_id (ESourceRegistry *registry,
                             const gchar *uid)
{
	ESource *source;
	const gchar *extension_name;

	extension_name = E_SOURCE_EXTENSION_GOA;

	while (uid != NULL) {
		ESourceGoa *extension;
		gchar *account_id;

		source = e_source_registry_ref_source (registry, uid);
		g_return_val_if_fail (source != NULL, NULL);

		if (!e_source_has_extension (source, extension_name)) {
			uid = e_source_get_parent (source);
			g_object_unref (source);
			continue;
		}

		extension = e_source_get_extension (source, extension_name);
		account_id = e_source_goa_dup_account_id (extension);

		g_object_unref (source);

		return account_id;
	}

	return NULL;
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
	GByteArray *parameters = NULL;
	CamelService *service;
	CamelSession *session;
	ESourceRegistry *registry;
	const gchar *uid;
	gchar *account_id;
	gchar *xoauth_param = NULL;
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

		if (success)
			xoauth_param = calculate_xoauth_param (
				request_uri,
				consumer_key,
				consumer_secret,
				access_token,
				access_token_secret);

		g_free (access_token);
		g_free (access_token_secret);
		g_free (request_uri);

		g_object_unref (goa_oauth_based);
	}

	g_object_unref (goa_account);
	g_object_unref (goa_object);
	g_object_unref (goa_client);

	if (success) {
		/* Sanity check. */
		g_return_val_if_fail (xoauth_param != NULL, NULL);

		parameters = g_byte_array_new ();
		g_byte_array_append (
			parameters, (guint8 *) xoauth_param,
			strlen (xoauth_param) + 1);
		g_free (xoauth_param);
	}

	/* IMAP and SMTP services will Base64-encode the XOAUTH parameters. */

	return parameters;
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
