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
#include "camel-sasl-cram-md5.h"
#include "camel-mime-utils.h"
#include <e-util/md5-utils.h>
#include <stdio.h>
#include <string.h>

static CamelSaslClass *parent_class = NULL;

/* Returns the class for a CamelSaslCramMd5 */
#define CSCM_CLASS(so) CAMEL_SASL_CRAM_MD5_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static GByteArray *cram_md5_challenge (CamelSasl *sasl, const char *token, CamelException *ex);

enum {
	STATE_AUTH,
	STATE_FINAL
};

struct _CamelSaslCramMd5Private {
	int state;
};

static void
camel_sasl_cram_md5_class_init (CamelSaslCramMd5Class *camel_sasl_cram_md5_class)
{
	CamelSaslClass *camel_sasl_class = CAMEL_SASL_CLASS (camel_sasl_cram_md5_class);
	
	parent_class = CAMEL_SASL_CLASS (camel_type_get_global_classfuncs (camel_sasl_get_type ()));
	
	/* virtual method overload */
	camel_sasl_class->challenge = cram_md5_challenge;
}

static void
camel_sasl_cram_md5_init (gpointer object, gpointer klass)
{
	CamelSaslCramMd5 *sasl_cram = CAMEL_SASL_CRAM_MD5 (object);
	
	sasl_cram->priv = g_new0 (struct _CamelSaslCramMd5Private, 1);
}

static void
camel_sasl_cram_md5_finalize (CamelObject *object)
{
	CamelSaslCramMd5 *sasl = CAMEL_SASL_CRAM_MD5 (object);
	
	g_free (sasl->username);
	g_free (sasl->priv);
}


CamelType
camel_sasl_cram_md5_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_sasl_get_type (),
					    "CamelSaslCramMd5",
					    sizeof (CamelSaslCramMd5),
					    sizeof (CamelSaslCramMd5Class),
					    (CamelObjectClassInitFunc) camel_sasl_cram_md5_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_sasl_cram_md5_init,
					    (CamelObjectFinalizeFunc) camel_sasl_cram_md5_finalize);
	}
	
	return type;
}

CamelSasl *
camel_sasl_cram_md5_new (const char *username, const char *passwd)
{
	CamelSaslCramMd5 *sasl_cram;
	
	if (!username) return NULL;
	if (!passwd) return NULL;
	
	sasl_cram = CAMEL_SASL_CRAM_MD5 (camel_object_new (camel_sasl_cram_md5_get_type ()));
	sasl_cram->username = g_strdup (username);
	sasl_cram->passwd = g_strdup (passwd);
	
	return CAMEL_SASL (sasl_cram);
}

/* CRAM-MD5 algorithm:
 * MD5 ((passwd XOR opad), MD5 ((passwd XOR ipad), timestamp))
 */

static GByteArray *
cram_md5_challenge (CamelSasl *sasl, const char *token, CamelException *ex)
{
	CamelSaslCramMd5 *sasl_cram = CAMEL_SASL_CRAM_MD5 (sasl);
	struct _CamelSaslCramMd5Private *priv = sasl_cram->priv;
	guchar digest[16], md5asc[33], *s, *p;
	char *timestamp, *passwd, *buf;
	GByteArray *ret = NULL;
	guchar ipad[64];
	guchar opad[64];
	MD5Context ctx;
	int i, pw_len;
	
	switch (priv->state) {
	case STATE_AUTH:
		timestamp = g_strdup (token);
		base64_decode_simple (timestamp, strlen (timestamp));
		
		passwd = sasl_cram->passwd;
		pw_len = strlen (sasl_cram->passwd);
		if (pw_len > 64) {
			md5_init (&ctx);
			md5_update (&ctx, passwd, pw_len);
			md5_final (&ctx, digest);
			passwd = g_strdup (digest);
			pw_len = 16;
		}
		
		memset (ipad, 0, sizeof (ipad));
		memset (opad, 0, sizeof (opad));
		memcpy (ipad, passwd, pw_len);
		memcpy (opad, passwd, pw_len);
		
		for (i = 0; i < 64; i++) {
			ipad[i] ^= 0x36;
			opad[i] ^= 0x5c;
		}
		
		md5_init (&ctx);
		md5_update (&ctx, ipad, 64);
		md5_update (&ctx, timestamp, strlen (timestamp));
		md5_final (&ctx, digest);
		
		md5_init (&ctx);
		md5_update (&ctx, opad, 64);
		md5_update (&ctx, digest, 16);
		md5_final (&ctx, digest);
		
		/* lowercase hexify that bad-boy... */
		for (s = digest, p = md5asc; p < md5asc + 32; s++, p += 2)
			sprintf (p, "%.2x", *s);
		
		buf = g_strdup_printf ("%s %s", sasl_cram->username, md5asc);
		
		ret = g_byte_array_new ();
		g_byte_array_append (ret, buf, strlen (buf));
		g_free (buf);
		
		break;
	case STATE_FINAL:
		sasl->authenticated = TRUE;
	default:
		break;
	}
	
	priv->state++;
	
	return ret;
}
