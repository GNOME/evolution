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

/* Once I figure out a nice API for S/MIME, it may be ideal to make a
 * new klass `CamelSecurityContext' or somesuch and have
 * CamelPgpContext (and the future CamelSMIMEContext) subclass it.
 *
 * The virtual functions could be sign, (clearsign maybe?,) verify, encrypt, and decrypt
 *
 * I could also make CamelPgpValidity more generic, maybe call it
 * CamelSignatureValidity or somesuch.
 */

#ifndef CAMEL_PGP_CONTEXT_H
#define CAMEL_PGP_CONTEXT_H

#include <camel/camel-session.h>
#include <camel/camel-stream.h>
#include <camel/camel-exception.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define CAMEL_PGP_CONTEXT_TYPE     (camel_pgp_context_get_type ())
#define CAMEL_PGP_CONTEXT(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_PGP_CONTEXT_TYPE, CamelPgpContext))
#define CAMEL_PGP_CONTEXT_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_PGP_CONTEXT_TYPE, CamelPgpContextClass))
#define CAMEL_IS_PGP_CONTEXT(o)    (CAMEL_CHECK_TYPE((o), CAMEL_PGP_CONTEXT_TYPE))

typedef enum {
	CAMEL_PGP_TYPE_NONE,
	CAMEL_PGP_TYPE_PGP2,
	CAMEL_PGP_TYPE_PGP5,
	CAMEL_PGP_TYPE_PGP6,
	CAMEL_PGP_TYPE_GPG
} CamelPgpType;

typedef enum {
	CAMEL_PGP_HASH_TYPE_DEFAULT,
	CAMEL_PGP_HASH_TYPE_MD5,
	CAMEL_PGP_HASH_TYPE_SHA1
} CamelPgpHashType;

typedef struct _CamelPgpContext {
	CamelObject parent_object;
	
	struct _CamelPgpContextPrivate *priv;
	
} CamelPgpContext;

typedef struct _CamelPgpContextClass {
	CamelObjectClass parent_class;
	
} CamelPgpContextClass;

typedef struct _CamelPgpValidity CamelPgpValidity;

CamelType         camel_pgp_context_get_type (void);

CamelPgpContext  *camel_pgp_context_new (CamelSession *session, CamelPgpType type, const char *path);

/* PGP routines */
int               camel_pgp_sign (CamelPgpContext *context, const char *userid, CamelPgpHashType hash,
				  CamelStream *istream, CamelStream *ostream, CamelException *ex);

int               camel_pgp_clearsign (CamelPgpContext *context, const char *userid, CamelPgpHashType hash,
				       CamelStream *istream, CamelStream *ostream, CamelException *ex);

CamelPgpValidity *camel_pgp_verify (CamelPgpContext *context, CamelStream *istream, CamelStream *sigstream,
				    CamelException *ex);

int               camel_pgp_encrypt (CamelPgpContext *context, gboolean sign, const char *userid,
				     GPtrArray *recipients, CamelStream *istream, CamelStream *ostream,
				     CamelException *ex);

int               camel_pgp_decrypt (CamelPgpContext *context, CamelStream *istream, CamelStream *ostream,
				     CamelException *ex);

/* CamelPgpValidity utility functions */
CamelPgpValidity *camel_pgp_validity_new (void);

void              camel_pgp_validity_init (CamelPgpValidity *validity);

gboolean          camel_pgp_validity_get_valid (CamelPgpValidity *validity);

void              camel_pgp_validity_set_valid (CamelPgpValidity *validity, gboolean valid);

gchar            *camel_pgp_validity_get_description (CamelPgpValidity *validity);

void              camel_pgp_validity_set_description (CamelPgpValidity *validity, const gchar *description);

void              camel_pgp_validity_clear (CamelPgpValidity *validity);

void              camel_pgp_validity_free (CamelPgpValidity *validity);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_PGP_CONTEXT_H */
