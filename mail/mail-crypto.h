/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#ifndef MAIL_CRYPTO_H
#define MAIL_CRYPTO_H

#include <gnome.h>
#include <camel/camel.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

typedef enum {
	PGP_HASH_TYPE_NONE,
	PGP_HASH_TYPE_MD5,
	PGP_HASH_TYPE_SHA1
} PgpHashType;

char *mail_crypto_openpgp_decrypt (const char *ciphertext,
				   int cipherlen,
				   int *outlen,
				   CamelException *ex);

char *mail_crypto_openpgp_encrypt (const char *in, int inlen,
				   const GPtrArray *recipients,
				   gboolean sign,
				   const char *userid,
				   CamelException *ex);

char *mail_crypto_openpgp_clearsign (const char *plaintext,
				     const char *userid,
				     PgpHashType hash,
				     CamelException *ex);

char *mail_crypto_openpgp_sign (const char *in, int inlen,
				const char *userid,
				PgpHashType hash,
				CamelException *ex);

gboolean mail_crypto_openpgp_verify (const char *in, int inlen,
				     const char *sigin, int siglen,
				     CamelException *ex);

gboolean is_rfc2015_signed (CamelMimePart *part);

gboolean is_rfc2015_encrypted (CamelMimePart *part);

void pgp_mime_part_sign (CamelMimePart **mime_part,
			 const gchar *userid,
			 PgpHashType hash,
			 CamelException *ex);

gboolean pgp_mime_part_verify (CamelMimePart *mime_part,
			       CamelException *ex);

void pgp_mime_part_encrypt (CamelMimePart **mime_part,
			    const GPtrArray *recipients,
			    CamelException *ex);

CamelMimePart *pgp_mime_part_decrypt (CamelMimePart *mime_part,
				      CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! MAIL_CRYPTO_H */
