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
#include "camel-sasl-plain.h"
#include <string.h>

static CamelSaslClass *parent_class = NULL;

/* Returns the class for a CamelSaslPlain */
#define CSP_CLASS(so) CAMEL_SASL_PLAIN_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static GByteArray *plain_challenge (CamelSasl *sasl, const char *token, CamelException *ex);

enum {
	STATE_LOGIN,
	STATE_FINAL
};

struct _CamelSaslPlainPrivate {
	int state;
};

static void
camel_sasl_plain_class_init (CamelSaslPlainClass *camel_sasl_plain_class)
{
	CamelSaslClass *camel_sasl_class = CAMEL_SASL_CLASS (camel_sasl_plain_class);
	
	parent_class = CAMEL_SASL_CLASS (camel_type_get_global_classfuncs (camel_sasl_get_type ()));
	
	/* virtual method overload */
	camel_sasl_class->challenge = plain_challenge;
}

static void
camel_sasl_plain_init (gpointer object, gpointer klass)
{
	CamelSaslPlain *sasl_plain = CAMEL_SASL_PLAIN (object);
	
	sasl_plain->priv = g_new0 (struct _CamelSaslPlainPrivate, 1);
}

static void
camel_sasl_plain_finalize (CamelObject *object)
{
	CamelSaslPlain *sasl = CAMEL_SASL_PLAIN (object);
	
	g_free (sasl->login);
	g_free (sasl->auth_id);
	g_free (sasl->passwd);
	g_free (sasl->priv);
}


CamelType
camel_sasl_plain_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_sasl_get_type (),
					    "CamelSaslPlain",
					    sizeof (CamelSaslPlain),
					    sizeof (CamelSaslPlainClass),
					    (CamelObjectClassInitFunc) camel_sasl_plain_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_sasl_plain_init,
					    (CamelObjectFinalizeFunc) camel_sasl_plain_finalize);
	}
	
	return type;
}

CamelSasl *
camel_sasl_plain_new (const char *login, const char *auth_id, const char *passwd)
{
	CamelSaslPlain *sasl_plain;
	
	if (!auth_id) return NULL;
	if (!passwd) return NULL;
	
	sasl_plain = CAMEL_SASL_PLAIN (camel_object_new (camel_sasl_plain_get_type ()));
	sasl_plain->login = g_strdup (login);
	sasl_plain->auth_id = g_strdup (auth_id);
	sasl_plain->passwd = g_strdup (passwd);
	
	return CAMEL_SASL (sasl_plain);
}

static GByteArray *
plain_challenge (CamelSasl *sasl, const char *token, CamelException *ex)
{
	CamelSaslPlain *sasl_plain = CAMEL_SASL_PLAIN (sasl);
	struct _CamelSaslPlainPrivate *priv = sasl_plain->priv;
	GByteArray *buf = NULL;
	
	switch (priv->state) {
	case STATE_LOGIN:
		/* FIXME: make sure these are "UTF8-SAFE" */
		buf = g_byte_array_new ();
		if (sasl_plain->login)
			g_byte_array_append (buf, sasl_plain->login, strlen (sasl_plain->login));
		g_byte_array_append (buf, "", 1);
		g_byte_array_append (buf, sasl_plain->auth_id, strlen (sasl_plain->auth_id));
		g_byte_array_append (buf, "", 1);
		g_byte_array_append (buf, sasl_plain->passwd, strlen (sasl_plain->passwd));
		break;
	case STATE_FINAL:
		sasl->authenticated = TRUE;
		break;
	default:
		break;
	}
	
	priv->state++;
	
	return buf;
}
