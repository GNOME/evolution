/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include <config.h>

#ifdef HAVE_KRB4
#include <krb.h>
/* MIT krb4 des.h #defines _. Sigh. We don't need it. #undef it here
 * so we get the gettexty _ definition later.
 */
#undef _
#endif

#include "camel-sasl-kerberos4.h"
#include "camel-mime-utils.h"
#include <string.h>

#define KERBEROS_V4_PROTECTION_NONE      1
#define KERBEROS_V4_PROTECTION_INTEGRITY 2
#define KERBEROS_V4_PROTECTION_PRIVACY   4

static CamelSaslClass *parent_class = NULL;

/* Returns the class for a CamelSaslKerberos4 */
#define CSK4_CLASS(so) CAMEL_SASL_KERBEROS4_CLASS (CAMEL_OBJECT_GET_CLASS (so))

#ifdef HAVE_KRB4
static GByteArray *krb4_challenge (CamelSasl *sasl, const char *token, CamelException *ex);
#endif

enum {
	STATE_NONCE,
	STATE_NONCE_PLUS_ONE,
	STATE_FINAL
};

struct _CamelSaslKerberos4Private {
	int state;
	
	guint32 nonce_n;
	guint32 nonce_h;
	guint32 plus1;
	
#ifdef HAVE_KRB4
	KTEXT_ST authenticator;
	CREDENTIALS credentials;
	des_cblock session;
	des_key_schedule schedule;
#endif /* HAVE_KRB4 */
};

static void
camel_sasl_kerberos4_class_init (CamelSaslKerberos4Class *camel_sasl_kerberos4_class)
{
#ifdef HAVE_KRB4
	CamelSaslClass *camel_sasl_class = CAMEL_SASL_CLASS (camel_sasl_kerberos4_class);
#endif
	
	parent_class = CAMEL_SASL_CLASS (camel_type_get_global_classfuncs (camel_sasl_get_type ()));
	
	/* virtual method overload */
#ifdef HAVE_KRB4
	camel_sasl_class->challenge = krb4_challenge;
#endif
}

static void
camel_sasl_kerberos4_init (gpointer object, gpointer klass)
{
	CamelSaslKerberos4 *sasl_krb4 = CAMEL_SASL_KERBEROS4 (object);
	
	sasl_krb4->priv = g_new0 (struct _CamelSaslKerberos4Private, 1);
}

static void
camel_sasl_kerberos4_finalize (CamelObject *object)
{
	CamelSaslKerberos4 *sasl = CAMEL_SASL_KERBEROS4 (object);
	
	g_free (sasl->protocol);
	g_free (sasl->username);
	g_free (sasl->priv);
}


CamelType
camel_sasl_kerberos4_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_sasl_get_type (),
					    "CamelSaslKerberos4",
					    sizeof (CamelSaslKerberos4),
					    sizeof (CamelSaslKerberos4Class),
					    (CamelObjectClassInitFunc) camel_sasl_kerberos4_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_sasl_kerberos4_init,
					    (CamelObjectFinalizeFunc) camel_sasl_kerberos4_finalize);
	}
	
	return type;
}

CamelSasl *
camel_sasl_kerberos4_new (const char *protocol, const char *username, struct hostent *host)
{
	CamelSaslKerberos4 *sasl_krb4;
	
	if (!protocol) return NULL;
	if (!username) return NULL;
	if (!host) return NULL;
	
#ifdef HAVE_KRB4
	sasl_krb4 = CAMEL_SASL_KERBEROS4 (camel_object_new (camel_sasl_kerberos4_get_type ()));
	sasl_krb4->protocol = g_strdup (protocol);
	g_strdown (sasl_krb4->protocol);
	sasl_krb4->username = g_strdup (username);
	sasl_krb4->host = host;
	
	return CAMEL_SASL (sasl_krb4);
#else
	return NULL;
#endif /* HAVE_KRB4 */
}

#ifdef HAVE_KRB4
static GByteArray *
krb4_challenge (CamelSasl *sasl, const char *token, CamelException *ex)
{
	CamelSaslKerberos4 *sasl_krb4 = CAMEL_SASL_KERBEROS4 (sasl);
	struct _CamelSaslKerberos4Private *priv = sasl_krb4->priv;
	char *buf = NULL, *data = NULL;
	GByteArray *ret = NULL;
	char *inst, *realm;
	struct hostent *h;
	int status, len;
	KTEXT_ST authenticator;
	CREDENTIALS credentials;
	des_cblock session;
	des_key_schedule schedule;
	
	if (token)
		data = g_strdup (token);
	else
		goto fail;
	
	switch (priv->state) {
	case STATE_NONCE:
		if (strlen (data) != 8 || base64_decode_simple (data, 8) != 4)
			goto break_and_lose;
		
		memcpy (&priv->nonce_n, data, 4);
		priv->nonce_h = ntohl (priv->nonce_n);
		
		/* Our response is an authenticator including that number. */
		h = sasl_krb4->host;
		inst = g_strndup (h->h_name, strcspn (h->h_name, "."));
		g_strdown (inst);
		realm = g_strdup (krb_realmofhost (h->h_name));
		status = krb_mk_req (&authenticator, sasl_krb4->protocol, inst, realm, priv->nonce_h);
		if (status == KSUCCESS) {
			status = krb_get_cred (sasl_krb4->protocol, inst, realm, &credentials);
			memcpy (session, credentials.session, sizeof (session));
			memset (&credentials, 0, sizeof (credentials));
		}
		g_free (inst);
		g_free (realm);
		
		if (status != KSUCCESS) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      _("Could not get Kerberos ticket:\n%s"),
					      krb_err_txt[status]);
			goto break_and_lose;
		}
		des_key_sched (&session, schedule);
		
		buf = base64_encode_simple (authenticator.dat, authenticator.length);
		break;
	case STATE_NONCE_PLUS_ONE:
		len = strlen (data);
		base64_decode_simple (data, len);
		
		/* This one is encrypted. */
		des_ecb_encrypt ((des_cblock *)data, (des_cblock *)data, schedule, 0);
		
		/* Check that the returned value is the original nonce plus one. */
		memcpy (&priv->plus1, data, 4);
		if (ntohl (priv->plus1) != priv->nonce_h + 1)
			goto lose;
		
		/* "the fifth octet contain[s] a bit-mask specifying the
		 * protection mechanisms supported by the server"
		 */
		if (!(data[4] & KERBEROS_V4_PROTECTION_NONE)) {
			g_warning ("Server does not support `no protection' :-(");
			goto break_and_lose;
		}
		
		len = strlen (sasl_krb4->username) + 9;
		len += 8 - len % 8;
		data = g_malloc0 (len);
		memcpy (data, &priv->nonce_n, 4);
		data[4] = KERBEROS_V4_PROTECTION_NONE;
		data[5] = data[6] = data[7] = 0;
		strcpy (data + 8, sasl_krb4->username);
		
		des_pcbc_encrypt ((void *)data, (void *)data, len,
				  schedule, &session, 1);
		memset (&session, 0, sizeof (session));
		buf = base64_encode_simple (data, len);
		break;
	case STATE_FINAL:
		sasl->authenticated = TRUE;
		break;
	default:
		break;
	}
	
	g_free (data);
	priv->state++;
	
	if (buf) {
		ret = g_byte_array_new ();
		g_byte_array_append (ret, buf, strlen (buf));
		g_free (buf);
	}
	
	return ret;
	
 break_and_lose:
	/* Get the server out of "waiting for continuation data" mode. */
	g_free (data);
	ret = g_byte_array_new ();
	g_byte_array_append (ret, "*", 1);
	return ret;
	
 lose:
	memset (&session, 0, sizeof (session));
	
	if (!camel_exception_is_set (ex)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				     _("Bad authentication response from server."));
	}
 fail:
	g_free (data);
	return NULL;
}
#endif HAVE_KRB4
