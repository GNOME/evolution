/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include "camel-sasl.h"
#include "camel-mime-utils.h"
#include "camel-service.h"

#include "camel-sasl-cram-md5.h"
#include "camel-sasl-digest-md5.h"
#include "camel-sasl-gssapi.h"
#include "camel-sasl-kerberos4.h"
#include "camel-sasl-login.h"
#include "camel-sasl-plain.h"
#include "camel-sasl-popb4smtp.h"
#include "camel-sasl-ntlm.h"

#define w(x)

static CamelObjectClass *parent_class = NULL;

/* Returns the class for a CamelSasl */
#define CS_CLASS(so) CAMEL_SASL_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static GByteArray *sasl_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex);

static void
camel_sasl_class_init (CamelSaslClass *camel_sasl_class)
{
	parent_class = camel_type_get_global_classfuncs (CAMEL_OBJECT_TYPE);
	
	/* virtual method definition */
	camel_sasl_class->challenge = sasl_challenge;
}

static void
camel_sasl_finalize (CamelSasl *sasl)
{
	g_free (sasl->service_name);
	g_free(sasl->mech);
	camel_object_unref (CAMEL_OBJECT (sasl->service));
}

CamelType
camel_sasl_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (CAMEL_OBJECT_TYPE,
					    "CamelSasl",
					    sizeof (CamelSasl),
					    sizeof (CamelSaslClass),
					    (CamelObjectClassInitFunc) camel_sasl_class_init,
					    NULL,
					    NULL,
					    (CamelObjectFinalizeFunc) camel_sasl_finalize);
	}
	
	return type;
}


static GByteArray *
sasl_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex)
{
	w(g_warning ("sasl_challenge: Using default implementation!"));
	return NULL;
}

/**
 * camel_sasl_challenge:
 * @sasl: a SASL object
 * @token: a token, or %NULL
 * @ex: exception
 *
 * If @token is %NULL, generate the initial SASL message to send to
 * the server. (This will be %NULL if the client doesn't initiate the
 * exchange.) Otherwise, @token is a challenge from the server, and
 * the return value is the response.
 *
 * Return value: The SASL response or %NULL. If an error occurred, @ex
 * will also be set.
 **/
GByteArray *
camel_sasl_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_SASL (sasl), NULL);
	
	return CS_CLASS (sasl)->challenge (sasl, token, ex);
}

/**
 * camel_sasl_challenge_base64:
 * @sasl: a SASL object
 * @token: a base64-encoded token
 * @ex: exception
 *
 * As with camel_sasl_challenge(), but the challenge @token and the
 * response are both base64-encoded.
 *
 * Return value: As with camel_sasl_challenge(), but base64-encoded.
 **/
char *
camel_sasl_challenge_base64 (CamelSasl *sasl, const char *token, CamelException *ex)
{
	GByteArray *token_binary, *ret_binary;
	char *ret;
	int len;
	
	g_return_val_if_fail (CAMEL_IS_SASL (sasl), NULL);
	
	if (token) {
		token_binary = g_byte_array_new ();
		len = strlen (token);
		g_byte_array_append (token_binary, token, len);
		token_binary->len = camel_base64_decode_simple (token_binary->data, len);
	} else
		token_binary = NULL;
	
	ret_binary = camel_sasl_challenge (sasl, token_binary, ex);
	if (token_binary)
		g_byte_array_free (token_binary, TRUE);
	if (!ret_binary)
		return NULL;
	
	ret = camel_base64_encode_simple (ret_binary->data, ret_binary->len);
	g_byte_array_free (ret_binary, TRUE);

	return ret;
}

/**
 * camel_sasl_authenticated:
 * @sasl: a SASL object
 *
 * Return value: whether or not @sasl has successfully authenticated
 * the user. This will be %TRUE after it returns the last needed response.
 * The caller must still pass that information on to the server and verify
 * that it has accepted it.
 **/
gboolean
camel_sasl_authenticated (CamelSasl *sasl)
{
	return sasl->authenticated;
}


/**
 * camel_sasl_new:
 * @service_name: the SASL service name
 * @mechanism: the SASL mechanism
 * @service: the CamelService that will be using this SASL
 *
 * Return value: a new CamelSasl for the given @service_name,
 * @mechanism, and @service, or %NULL if the mechanism is not
 * supported.
 **/
CamelSasl *
camel_sasl_new (const char *service_name, const char *mechanism, CamelService *service)
{
	CamelSasl *sasl;
	
	g_return_val_if_fail (service_name != NULL, NULL);
	g_return_val_if_fail (mechanism != NULL, NULL);
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), NULL);
	
	/* We don't do ANONYMOUS here, because it's a little bit weird. */
	
	if (!strcmp (mechanism, "CRAM-MD5"))
		sasl = (CamelSasl *)camel_object_new (CAMEL_SASL_CRAM_MD5_TYPE);
	else if (!strcmp (mechanism, "DIGEST-MD5"))
		sasl = (CamelSasl *)camel_object_new (CAMEL_SASL_DIGEST_MD5_TYPE);
#ifdef HAVE_KRB5
	else if (!strcmp (mechanism, "GSSAPI"))
		sasl = (CamelSasl *)camel_object_new (CAMEL_SASL_GSSAPI_TYPE);
#endif
#ifdef HAVE_KRB4
	else if (!strcmp (mechanism, "KERBEROS_V4"))
		sasl = (CamelSasl *)camel_object_new (CAMEL_SASL_KERBEROS4_TYPE);
#endif
	else if (!strcmp (mechanism, "PLAIN"))
		sasl = (CamelSasl *)camel_object_new (CAMEL_SASL_PLAIN_TYPE);
	else if (!strcmp (mechanism, "LOGIN"))
		sasl = (CamelSasl *)camel_object_new (CAMEL_SASL_LOGIN_TYPE);
	else if (!strcmp (mechanism, "POPB4SMTP"))
		sasl = (CamelSasl *)camel_object_new (CAMEL_SASL_POPB4SMTP_TYPE);
	else if (!strcmp (mechanism, "NTLM"))
		sasl = (CamelSasl *)camel_object_new (CAMEL_SASL_NTLM_TYPE);
	else
		return NULL;

	sasl->mech = g_strdup(mechanism);
	sasl->service_name = g_strdup (service_name);
	sasl->service = service;
	camel_object_ref (CAMEL_OBJECT (service));
	
	return sasl;
}

/**
 * camel_sasl_authtype_list:
 * @include_plain: whether or not to include the PLAIN mechanism
 *
 * Return value: a GList of SASL-supported authtypes. The caller must
 * free the list, but not the contents.
 **/
GList *
camel_sasl_authtype_list (gboolean include_plain)
{
	GList *types = NULL;
	
	types = g_list_prepend (types, &camel_sasl_cram_md5_authtype);
	types = g_list_prepend (types, &camel_sasl_digest_md5_authtype);
#ifdef HAVE_KRB5
	types = g_list_prepend (types, &camel_sasl_gssapi_authtype);
#endif
#ifdef HAVE_KRB4
	types = g_list_prepend (types, &camel_sasl_kerberos4_authtype);
#endif
	types = g_list_prepend (types, &camel_sasl_ntlm_authtype);
	if (include_plain)
		types = g_list_prepend (types, &camel_sasl_plain_authtype);
	
	return types;
}

/**
 * camel_sasl_authtype:
 * @mechanism: the SASL mechanism to get an authtype for
 *
 * Return value: a CamelServiceAuthType for the given mechanism, if
 * it is supported.
 **/
CamelServiceAuthType *
camel_sasl_authtype (const char *mechanism)
{
	if (!strcmp (mechanism, "CRAM-MD5"))
		return &camel_sasl_cram_md5_authtype;
	else if (!strcmp (mechanism, "DIGEST-MD5"))
		return &camel_sasl_digest_md5_authtype;
#ifdef HAVE_KRB5
	else if (!strcmp (mechanism, "GSSAPI"))
		return &camel_sasl_gssapi_authtype;
#endif
#ifdef HAVE_KRB4
	else if (!strcmp (mechanism, "KERBEROS_V4"))
		return &camel_sasl_kerberos4_authtype;
#endif
	else if (!strcmp (mechanism, "PLAIN"))
		return &camel_sasl_plain_authtype;
	else if (!strcmp (mechanism, "LOGIN"))
		return &camel_sasl_login_authtype;
	else if (!strcmp(mechanism, "POPB4SMTP"))
		return &camel_sasl_popb4smtp_authtype;
	else if (!strcmp (mechanism, "NTLM"))
		return &camel_sasl_ntlm_authtype;
	else
		return NULL;
}
