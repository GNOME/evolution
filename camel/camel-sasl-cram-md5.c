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

#include <stdio.h>
#include <string.h>
#include "camel-sasl-cram-md5.h"
#include "camel-mime-utils.h"
#include "camel-service.h"
#include <e-util/md5-utils.h>
#include "camel-i18n.h"

CamelServiceAuthType camel_sasl_cram_md5_authtype = {
	N_("CRAM-MD5"),

	N_("This option will connect to the server using a "
	   "secure CRAM-MD5 password, if the server supports it."),

	"CRAM-MD5",
	TRUE
};

static CamelSaslClass *parent_class = NULL;

/* Returns the class for a CamelSaslCramMd5 */
#define CSCM_CLASS(so) CAMEL_SASL_CRAM_MD5_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static GByteArray *cram_md5_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex);

static void
camel_sasl_cram_md5_class_init (CamelSaslCramMd5Class *camel_sasl_cram_md5_class)
{
	CamelSaslClass *camel_sasl_class = CAMEL_SASL_CLASS (camel_sasl_cram_md5_class);
	
	parent_class = CAMEL_SASL_CLASS (camel_type_get_global_classfuncs (camel_sasl_get_type ()));
	
	/* virtual method overload */
	camel_sasl_class->challenge = cram_md5_challenge;
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
					    NULL,
					    NULL);
	}
	
	return type;
}

/* CRAM-MD5 algorithm:
 * MD5 ((passwd XOR opad), MD5 ((passwd XOR ipad), timestamp))
 */

static GByteArray *
cram_md5_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex)
{
	char *passwd;
	guchar digest[16], md5asc[33], *s, *p;
	GByteArray *ret = NULL;
	guchar ipad[64];
	guchar opad[64];
	MD5Context ctx;
	int i, pw_len;
	
	/* Need to wait for the server */
	if (!token)
		return NULL;
	
	g_return_val_if_fail (sasl->service->url->passwd != NULL, NULL);
	
	memset (ipad, 0, sizeof (ipad));
	memset (opad, 0, sizeof (opad));
	
	passwd = sasl->service->url->passwd;
	pw_len = strlen (passwd);
	if (pw_len <= 64) {
		memcpy (ipad, passwd, pw_len);
		memcpy (opad, passwd, pw_len);
	} else {
		md5_get_digest (passwd, pw_len, ipad);
		memcpy (opad, ipad, 16);
	}
	
	for (i = 0; i < 64; i++) {
		ipad[i] ^= 0x36;
		opad[i] ^= 0x5c;
	}
	
	md5_init (&ctx);
	md5_update (&ctx, ipad, 64);
	md5_update (&ctx, token->data, token->len);
	md5_final (&ctx, digest);
	
	md5_init (&ctx);
	md5_update (&ctx, opad, 64);
	md5_update (&ctx, digest, 16);
	md5_final (&ctx, digest);
	
	/* lowercase hexify that bad-boy... */
	for (s = digest, p = md5asc; p < md5asc + 32; s++, p += 2)
		sprintf (p, "%.2x", *s);
	
	ret = g_byte_array_new ();
	g_byte_array_append (ret, sasl->service->url->user, strlen (sasl->service->url->user));
	g_byte_array_append (ret, " ", 1);
	g_byte_array_append (ret, md5asc, 32);
	
	sasl->authenticated = TRUE;
	
	return ret;
}
