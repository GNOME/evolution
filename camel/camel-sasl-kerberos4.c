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

#ifdef HAVE_KRB4

#include <krb.h>
/* MIT krb4 des.h #defines _. Sigh. We don't need it. #undef it here
 * so we get the gettexty _ definition later.
 */
#undef _

#include <string.h>
#include "camel-i18n.h"
#include "camel-string-utils.h"
#include "camel-sasl-kerberos4.h"
#include "camel-service.h"

CamelServiceAuthType camel_sasl_kerberos4_authtype = {
	N_("Kerberos 4"),

	N_("This option will connect to the server using "
	   "Kerberos 4 authentication."),

	"KERBEROS_V4",
	FALSE
};

#define KERBEROS_V4_PROTECTION_NONE      1
#define KERBEROS_V4_PROTECTION_INTEGRITY 2
#define KERBEROS_V4_PROTECTION_PRIVACY   4

static CamelSaslClass *parent_class = NULL;

/* Returns the class for a CamelSaslKerberos4 */
#define CSK4_CLASS(so) CAMEL_SASL_KERBEROS4_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static GByteArray *krb4_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex);

struct _CamelSaslKerberos4Private {
	int state;
	
	guint32 nonce_n;
	guint32 nonce_h;
	
	des_cblock session;
	des_key_schedule schedule;
};

static void
camel_sasl_kerberos4_class_init (CamelSaslKerberos4Class *camel_sasl_kerberos4_class)
{
	CamelSaslClass *camel_sasl_class = CAMEL_SASL_CLASS (camel_sasl_kerberos4_class);
	
	parent_class = CAMEL_SASL_CLASS (camel_type_get_global_classfuncs (camel_sasl_get_type ()));
	
	/* virtual method overload */
	camel_sasl_class->challenge = krb4_challenge;
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

	if (sasl->priv) {
		memset (sasl->priv, 0, sizeof (sasl->priv));
		g_free (sasl->priv);
	}
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

static GByteArray *
krb4_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex)
{
	struct _CamelSaslKerberos4Private *priv = CAMEL_SASL_KERBEROS4 (sasl)->priv;
	GByteArray *ret = NULL;
	char *inst, *realm, *username;
	struct hostent *h;
	int status, len;
	KTEXT_ST authenticator;
	CREDENTIALS credentials;
	guint32 plus1;
	struct addrinfo *ai, hints;

	/* Need to wait for the server */
	if (!token)
		return NULL;

	switch (priv->state) {
	case 0:
		if (token->len != 4)
			goto lose;

		memcpy (&priv->nonce_n, token->data, 4);
		priv->nonce_h = ntohl (priv->nonce_n);

		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AI_CANONNAME;
		ai = camel_getaddrinfo(sasl->service->url->host?sasl->service->url->host:"localhost", NULL, &hints, ex);
		if (ai == NULL)
			goto lose;

		/* Our response is an authenticator including that number. */
		inst = g_strndup (ai->ai_canonname, strcspn (ai->ai_canonname, "."));
		camel_strdown (inst);
		realm = g_strdup (krb_realmofhost (ai->ai_canonname));
		camel_freeaddrinfo(ai);
		status = krb_mk_req (&authenticator, sasl->service_name, inst, realm, priv->nonce_h);
		if (status == KSUCCESS) {
			status = krb_get_cred (sasl->service_name, inst, realm, &credentials);
			memcpy (priv->session, credentials.session, sizeof (priv->session));
			memset (&credentials, 0, sizeof (credentials));
		}
		g_free (inst);
		g_free (realm);

		if (status != KSUCCESS) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      _("Could not get Kerberos ticket:\n%s"),
					      krb_err_txt[status]);
			goto lose;
		}
		des_key_sched (&priv->session, priv->schedule);

		ret = g_byte_array_new ();
		g_byte_array_append (ret, (const guint8 *)authenticator.dat, authenticator.length);
		break;

	case 1:
		if (token->len != 8)
			goto lose;

		/* This one is encrypted. */
		des_ecb_encrypt ((des_cblock *)token->data, (des_cblock *)token->data, priv->schedule, 0);

		/* Check that the returned value is the original nonce plus one. */
		memcpy (&plus1, token->data, 4);
		if (ntohl (plus1) != priv->nonce_h + 1)
			goto lose;

		/* "the fifth octet contain[s] a bit-mask specifying the
		 * protection mechanisms supported by the server"
		 */
		if (!(token->data[4] & KERBEROS_V4_PROTECTION_NONE)) {
			g_warning ("Server does not support `no protection' :-(");
			goto lose;
		}

		username = sasl->service->url->user;
		len = strlen (username) + 9;
		len += 8 - len % 8;
		ret = g_byte_array_new ();
		g_byte_array_set_size (ret, len);
		memset (ret->data, 0, len);
		memcpy (ret->data, &priv->nonce_n, 4);
		ret->data[4] = KERBEROS_V4_PROTECTION_NONE;
		ret->data[5] = ret->data[6] = ret->data[7] = 0;
		strcpy (ret->data + 8, username);

		des_pcbc_encrypt ((void *)ret->data, (void *)ret->data, len,
				  priv->schedule, &priv->session, 1);
		memset (&priv->session, 0, sizeof (priv->session));

		sasl->authenticated = TRUE;
		break;
	}

	priv->state++;
	return ret;

 lose:
	memset (&priv->session, 0, sizeof (priv->session));

	if (!camel_exception_is_set (ex)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				     _("Bad authentication response from server."));
	}
	return NULL;
}

#endif /* HAVE_KRB4 */
