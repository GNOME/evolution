/*
 * camel-sasl-oauth2-google.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include "camel-sasl-oauth2-google.h"

#include <libemail-engine/e-mail-session.h>

static CamelServiceAuthType sasl_oauth2_google_auth_type = {
	N_("OAuth2 Google"),
	N_("This option will use an OAuth 2.0 "
	   "access token to connect to the Google server"),
	"Google",
	TRUE
};

G_DEFINE_TYPE (CamelSaslOAuth2Google, camel_sasl_oauth2_google, CAMEL_TYPE_SASL)

static void
sasl_oauth2_google_append_request (GByteArray *byte_array,
				   const gchar *user,
				   const gchar *access_token)
{
	GString *request;

	g_return_if_fail (user != NULL);
	g_return_if_fail (access_token != NULL);

	/* Compared to OAuth 1.0, this step is trivial. */

	/* The request is easier to assemble with a GString. */
	request = g_string_sized_new (512);

	g_string_append (request, "user=");
	g_string_append (request, user);
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

static GByteArray *
sasl_oauth2_google_challenge_sync (CamelSasl *sasl,
				   GByteArray *token,
				   GCancellable *cancellable,
				   GError **error)
{
	GByteArray *byte_array = NULL;
	CamelService *service;
	CamelSettings *settings;
	gchar *access_token = NULL;

	service = camel_sasl_get_service (sasl);
	settings = camel_service_ref_settings (service);
	access_token = camel_service_dup_password (service);

	if (access_token) {
		CamelNetworkSettings *network_settings;
		gchar *user;

		network_settings = CAMEL_NETWORK_SETTINGS (settings);
		user = camel_network_settings_dup_user (network_settings);

		byte_array = g_byte_array_new ();
		sasl_oauth2_google_append_request (byte_array, user, access_token);

		g_free (user);
	}

	g_free (access_token);

	g_object_unref (settings);

	/* IMAP and SMTP services will Base64-encode the request. */

	return byte_array;
}

static void
camel_sasl_oauth2_google_class_init (CamelSaslOAuth2GoogleClass *class)
{
	CamelSaslClass *sasl_class;

	sasl_class = CAMEL_SASL_CLASS (class);
	sasl_class->auth_type = &sasl_oauth2_google_auth_type;
	sasl_class->challenge_sync = sasl_oauth2_google_challenge_sync;
}

static void
camel_sasl_oauth2_google_init (CamelSaslOAuth2Google *sasl)
{
}
