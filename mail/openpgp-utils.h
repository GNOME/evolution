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


#ifndef __OPENPGP_UTILS_H__
#define __OPENPGP_UTILS_H__

#include <glib.h>
#include <camel/camel-exception.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef enum {
	PGP_TYPE_NONE,
	PGP_TYPE_PGP2,
	PGP_TYPE_PGP5,
	PGP_TYPE_GPG
} PgpType;

typedef enum {
	PGP_HASH_TYPE_NONE,
	PGP_HASH_TYPE_MD5,
	PGP_HASH_TYPE_SHA1
} PgpHashType;

typedef struct _PgpValidity PgpValidity;

void openpgp_init (const gchar *path, PgpType type);

gboolean openpgp_detect (const gchar *text);

gboolean openpgp_sign_detect (const gchar *text);

gchar *openpgp_decrypt (const gchar *ciphertext, gint cipherlen, gint *outlen, CamelException *ex);

gchar *openpgp_encrypt (const gchar *in, gint inlen, const GPtrArray *recipients,
			gboolean sign, const gchar *userid, CamelException *ex);

gchar *openpgp_clearsign (const gchar *plaintext, const gchar *userid,
			  PgpHashType hash, CamelException *ex);

gchar *openpgp_sign (const gchar *in, gint inlen, const gchar *userid,
		     PgpHashType hash, CamelException *ex);

PgpValidity *openpgp_verify (const gchar *in, gint inlen, const gchar *sigin,
			     gint siglen, CamelException *ex);

PgpValidity *openpgp_validity_new (void);

void openpgp_validity_init (PgpValidity *validity);

gboolean openpgp_validity_get_valid (PgpValidity *validity);

void openpgp_validity_set_valid (PgpValidity *validity, gboolean valid);

gchar *openpgp_validity_get_description (PgpValidity *validity);

void openpgp_validity_set_description (PgpValidity *validity, const gchar *description);

void openpgp_validity_clear (PgpValidity *validity);

void openpgp_validity_free (PgpValidity *validity);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __OPENPGP_UTILS_H__ */
