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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "mail-crypto.h"
#include "mail-session.h"
#include "mail-config.h"

/** rfc2633 stuff (aka S/MIME v3) ********************************/

gboolean
mail_crypto_is_smime_v3_signed (CamelMimePart *mime_part)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *mp;
	CamelMimePart *part;
	CamelContentType *type;
	const gchar *param, *micalg;
	int nparts;
	
	/* check that we have a multipart/signed */
	type = camel_mime_part_get_content_type (mime_part);
	if (!header_content_type_is (type, "multipart", "signed"))
		return FALSE;
	
	/* check that we have a protocol param with the value: "application/pkcs7-signature" */
	param = header_content_type_param (type, "protocol");
	if (!param || g_strcasecmp (param, "application/pkcs7-signature"))
		return FALSE;
	
	/* check that we have a micalg parameter */
	micalg = header_content_type_param (type, "micalg");
	if (!micalg)
		return FALSE;
	
	/* check that we have exactly 2 subparts */
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	mp = CAMEL_MULTIPART (wrapper);
	nparts = camel_multipart_get_number (mp);
	if (nparts != 2)
		return FALSE;
	
	/* The first part may be of any type except for 
	 * application/pkcs7-signature - check it. */
	part = camel_multipart_get_part (mp, 0);
	type = camel_mime_part_get_content_type (part);
	if (header_content_type_is (type, "application", "pkcs7-signature"))
		return FALSE;
	
	/* The second part should be application/pkcs7-signature. */
	part = camel_multipart_get_part (mp, 1);
	type = camel_mime_part_get_content_type (part);
	if (!header_content_type_is (type, "application", "pkcs7-signature"))
		return FALSE;
	
	return TRUE;
}

gboolean
mail_crypto_is_pkcs7_mime (CamelMimePart *mime_part)
{
	char *types[] = { "p7m", "p7c", "p7s", NULL };
	const gchar *param, *filename;
	CamelContentType *type;
	int i;
	
	/* check that we have a application/pkcs7-mime part */
	type = camel_mime_part_get_content_type (mime_part);
	if (header_content_type_is (type, "application", "pkcs7-mime"))
		return TRUE;
	
	if (header_content_type_is (type, "application", "octent-stream")) {
		/* check to see if we have a paremeter called "smime-type" */
		param = header_content_type_param (type, "smime-type");
		if (param)
			return TRUE;
		
		/* check to see if there is a name param and if it has a smime extension */
		param = header_content_type_param (type, "smime-type");
		if (param && *param && strlen (param) > 4) {
			for (i = 0; types[i]; i++)
				if (!g_strcasecmp (param + strlen (param)-4, types[i]))
					return TRUE;
		}
		
		/* check to see if there is a name param and if it has a smime extension */
		filename = camel_mime_part_get_filename (mime_part);
		if (filename && *filename && strlen (filename) > 4) {
			for (i = 0; types[i]; i++)
				if (!g_strcasecmp (filename + strlen (filename)-4, types[i]))
					return TRUE;
		}
	}
	
	return FALSE;
}


/**
 * mail_crypto_pgp_mime_part_sign:
 * @mime_part: a MIME part that will be replaced by a pgp signed part
 * @userid: userid to sign with
 * @hash: one of PGP_HASH_TYPE_MD5 or PGP_HASH_TYPE_SHA1
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a PGP/MIME multipart in compliance with rfc2015 and
 * replaces #part with the generated multipart/signed. On failure,
 * #ex will be set and #part will remain untouched.
 **/
void
mail_crypto_pgp_mime_part_sign (CamelMimePart **mime_part, const char *userid, CamelCipherHash hash, CamelException *ex)
{
	CamelPgpContext *context;
	
	context = camel_pgp_context_new (session, mail_config_get_pgp_type (),
					 mail_config_get_pgp_path ());
	camel_pgp_mime_part_sign (context, mime_part, userid, hash, ex);
	camel_object_unref (CAMEL_OBJECT (context));
}


/**
 * mail_crypto_pgp_mime_part_verify:
 * @mime_part: a multipart/signed MIME Part
 * @ex: exception
 *
 * Returns a PgpValidity on success or NULL on fail.
 **/
CamelCipherValidity *
mail_crypto_pgp_mime_part_verify (CamelMimePart *mime_part, CamelException *ex)
{
	CamelCipherValidity *valid = NULL;
	CamelPgpContext *context;
	
	context = camel_pgp_context_new (session, mail_config_get_pgp_type (),
					 mail_config_get_pgp_path ());
	
	if (context) {
		valid = camel_pgp_mime_part_verify (context, mime_part, ex);
		camel_object_unref (CAMEL_OBJECT (context));
	}
	
	return valid;
}


/**
 * mail_crypto_pgp_mime_part_encrypt:
 * @mime_part: a MIME part that will be replaced by a pgp encrypted part
 * @recipients: list of recipient PGP Key IDs
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a PGP/MIME multipart in compliance with rfc2015 and
 * replaces #mime_part with the generated multipart/signed. On failure,
 * #ex will be set and #part will remain untouched.
 **/
void
mail_crypto_pgp_mime_part_encrypt (CamelMimePart **mime_part, GPtrArray *recipients, CamelException *ex)
{
	CamelPgpContext *context;
	
	context = camel_pgp_context_new (session, mail_config_get_pgp_type (),
					 mail_config_get_pgp_path ());
	
	if (context) {
		camel_pgp_mime_part_encrypt (context, mime_part, recipients, ex);
		camel_object_unref (CAMEL_OBJECT (context));
	}
}


/**
 * mail_crypto_pgp_mime_part_decrypt:
 * @mime_part: a multipart/encrypted MIME Part
 * @ex: exception
 *
 * Returns the decrypted MIME Part on success or NULL on fail.
 **/
CamelMimePart *
mail_crypto_pgp_mime_part_decrypt (CamelMimePart *mime_part, CamelException *ex)
{
	CamelPgpContext *context;
	CamelMimePart *part = NULL;
	
	context = camel_pgp_context_new (session, mail_config_get_pgp_type (),
					 mail_config_get_pgp_path ());
	
	if (context) {
		part = camel_pgp_mime_part_decrypt (context, mime_part, ex);
		camel_object_unref (CAMEL_OBJECT (context));
	}
	
	return part;
}
