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

#include "camel-pkcs7-context.h"

#include "camel-stream-fs.h"
#include "camel-stream-mem.h"

#include <cert.h>
#include <secpkcs7.h>

#include <gtk/gtk.h> /* for _() macro */

#define d(x)

struct _CamelPkcs7ContextPrivate {
	CERTCertDBHandle *certdb;
};


static int                  pkcs7_sign (CamelCipherContext *ctx, const char *userid, CamelCipherHash hash,
					CamelStream *istream, CamelStream *ostream, CamelException *ex);
static int                  pkcs7_clearsign (CamelCipherContext *context, const char *userid,
					     CamelCipherHash hash, CamelStream *istream,
					     CamelStream *ostream, CamelException *ex);
static CamelCipherValidity *pkcs7_verify (CamelCipherContext *context, CamelStream *istream,
					  CamelStream *sigstream, CamelException *ex);
static int                  pkcs7_encrypt (CamelCipherContext *context, gboolean sign, const char *userid,
					   GPtrArray *recipients, CamelStream *istream, CamelStream *ostream,
					   CamelException *ex);
static int                  pkcs7_decrypt (CamelCipherContext *context, CamelStream *istream,
					   CamelStream *ostream, CamelException *ex);


static CamelCipherContextClass *parent_class;

static void
camel_pkcs7_context_init (CamelPkcs7Context *context)
{
	context->priv = g_new0 (struct _CamelPkcs7ContextPrivate, 1);
}

static void
camel_pkcs7_context_finalise (CamelObject *o)
{
	CamelPkcs7Context *context = (CamelPkcs7Context *)o;
	
	CERT_ClosePermCertDB (context->priv->certdb);
	g_free (context->priv->certdb);
	
	g_free (context->priv);
}

static void
camel_pkcs7_context_class_init (CamelPkcs7ContextClass *camel_pkcs7_context_class)
{
	CamelCipherContextClass *camel_cipher_context_class =
		CAMEL_CIPHER_CONTEXT_CLASS (camel_pkcs7_context_class);
	
	parent_class = CAMEL_CIPHER_CONTEXT_CLASS (camel_type_get_global_classfuncs (camel_cipher_context_get_type ()));
	
	camel_cipher_context_class->sign = pkcs7_sign;
	camel_cipher_context_class->clearsign = pkcs7_clearsign;
	camel_cipher_context_class->verify = pkcs7_verify;
	camel_cipher_context_class->encrypt = pkcs7_encrypt;
	camel_cipher_context_class->decrypt = pkcs7_decrypt;
}

CamelType
camel_pkcs7_context_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_cipher_context_get_type (),
					    "CamelPkcs7Context",
					    sizeof (CamelPkcs7Context),
					    sizeof (CamelPkcs7ContextClass),
					    (CamelObjectClassInitFunc) camel_pkcs7_context_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_pkcs7_context_init,
					    (CamelObjectFinalizeFunc) camel_pkcs7_context_finalise);
	}
	
	return type;
}


/**
 * camel_pkcs7_context_new:
 * @session: CamelSession
 * @certdb: certificate db
 *
 * This creates a new CamelPkcs7Context object which is used to sign,
 * verify, encrypt and decrypt streams.
 *
 * Return value: the new CamelPkcs7Context
 **/
CamelPkcs7Context *
camel_pkcs7_context_new (CamelSession *session, const char *certdb)
{
	CamelPkcs7Context *context;
	CERTCertDBHandle *handle;
	
	g_return_val_if_fail (session != NULL, NULL);
	
	context = CAMEL_PKCS7_CONTEXT (camel_object_new (CAMEL_PKCS7_CONTEXT_TYPE));
	
	camel_cipher_construct (CAMEL_CIPHER_CONTEXT (context), session);
	
	handle = g_new0 (CERTCertDBHandle, 1);
	if (certdb) {
		if (!CERT_OpenCertDBFilename (handle, certdb, FALSE)) {
			g_free (handle);
			return NULL;
		}
	} else {
		if (!CERT_OpenVolatileCertDB (handle)) {
			g_free (handle);
			return NULL;
		}
	}
	
	context->priv->certdb = handle;
	
	return context;
}

/*----------------------------------------------------------------------*
 *                     Public crypto functions
 *----------------------------------------------------------------------*/

static int
pkcs7_sign (CamelCipherContext *ctx, const char *userid, CamelCipherHash hash,
	    CamelStream *istream, CamelStream *ostream, CamelException *ex)
{
	CamelPkcs7Context *context = CAMEL_PKCS7_CONTEXT (ctx);
	
	
	
	return -1;
}


static int
pkcs7_clearsign (CamelCipherContext *ctx, const char *userid, CamelCipherHash hash,
		 CamelStream *istream, CamelStream *ostream, CamelException *ex)
{
	CamelPkcs7Context *context = CAMEL_PKCS7_CONTEXT (ctx);
	
	return -1;
}

static CamelCipherValidity *
pkcs7_verify (CamelCipherContext *ctx, CamelCipherHash hash, CamelStream *istream,
	      CamelStream *sigstream, CamelException *ex)
{
	CamelPkcs7Context *context = CAMEL_PKCS7_CONTEXT (ctx);
	CamelCipherValidity *valid = NULL;
	SEC_PKCS7ContentInfo *cinfo;
	SECCertUsage certusage;
	GByteArray *plaintext;
	CamelStream *stream;
	
	/* create our ContentInfo object */
	stream = camel_stream_mem_new ();
	camel_stream_write_to_stream (istream, stream);
	plaintext = CAMEL_STREAM_MEM (stream)->buffer;
	cinfo = SEC_PKCS7CreateData ();
	SEC_PKCS7SetContent (cinfo, plaintext->data, plaintext->len);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	certusage = 0;
	
	valid = camel_cipher_validity_new ();
	
	if (sigstream) {
		HASH_HashType digest_type;
		GByteArray *signature;
		SECItem *digest;
		
		switch (hash) {
		default:
		case CAMEL_CIPHER_HASH_DEFAULT:
			digest_type = HASH_AlgNULL;
			break;
		case CAMEL_CIPHER_HASH_MD2:
			digest_type = HASH_AlgMD2;
			break;
		case CAMEL_CIPHER_HASH_MD5:
			digest_type = HASH_AlgMD5;
			break;
		case CAMEL_CIPHER_HASH_SHA1:
			digest_type = HASH_AlgSHA1;
			break;
		}
		
		valid->valid = SEC_PKCS7VerifyDetachedSignature (cinfo, certusage, digest, digest_type, TRUE);
	} else {
		valid->valid = SEC_PKCS7VerifySignature (cinfo, certusage, TRUE);
	}
	
	SEC_PKCS7DestroyContentInfo (cinfo);
	
	return valid;
}


static int
pkcs7_encrypt (CamelCipherContext *ctx, gboolean sign, const char *userid, GPtrArray *recipients,
	       CamelStream *istream, CamelStream *ostream, CamelException *ex)
{
	CamelPkcs7Context *context = CAMEL_PKCS7_CONTEXT (ctx);
	
	return -1;
}


static int
pkcs7_decrypt (CamelCipherContext *ctx, CamelStream *istream,
	       CamelStream *ostream, CamelException *ex)
{
	CamelPkcs7Context *context = CAMEL_PKCS7_CONTEXT (ctx);
	
	return -1;
}
