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

#include "camel-cipher-context.h"

#include <glib.h>

#include <iconv.h>

#ifdef ENABLE_THREADS
#include <pthread.h>
#define CIPHER_LOCK(ctx)   g_mutex_lock (((CamelCipherContext *) ctx)->priv->lock)
#define CIPHER_UNLOCK(ctx) g_mutex_unlock (((CamelCipherContext *) ctx)->priv->lock);
#else
#define CIPHER_LOCK(ctx)
#define CIPHER_UNLOCK(ctx)
#endif

#define d(x)

#define CCC_CLASS(o) CAMEL_CIPHER_CONTEXT_CLASS(CAMEL_OBJECT_GET_CLASS(o))

struct _CamelCipherContextPrivate {
#ifdef ENABLE_THREADS
	GMutex *lock;
#endif
};

static int                  cipher_sign (CamelCipherContext *ctx, const char *userid, CamelCipherHash hash,
					 CamelStream *istream, CamelStream *ostream, CamelException *ex);
static int                  cipher_clearsign (CamelCipherContext *context, const char *userid, CamelCipherHash hash,
					      CamelStream *istream, CamelStream *ostream, CamelException *ex);
static CamelCipherValidity *cipher_verify (CamelCipherContext *context, CamelStream *istream,
					   CamelStream *sigstream, CamelException *ex);
static int                  cipher_encrypt (CamelCipherContext *context, gboolean sign, const char *userid,
					    GPtrArray *recipients, CamelStream *istream, CamelStream *ostream,
					    CamelException *ex);
static int                  cipher_decrypt (CamelCipherContext *context, CamelStream *istream, CamelStream *ostream,
					    CamelException *ex);

static CamelObjectClass *parent_class;

static void
camel_cipher_context_init (CamelCipherContext *context)
{
	context->priv = g_new0 (struct _CamelCipherContextPrivate, 1);
#ifdef ENABLE_THREADS
	context->priv->lock = g_mutex_new ();
#endif
}

static void
camel_cipher_context_finalise (CamelObject *o)
{
	CamelCipherContext *context = (CamelCipherContext *)o;
	
	camel_object_unref (CAMEL_OBJECT (context->session));
	
#ifdef ENABLE_THREADS
	g_mutex_free (context->priv->lock);
#endif
	
	g_free (context->priv);
}

static void
camel_cipher_context_class_init (CamelCipherContextClass *camel_cipher_context_class)
{
	parent_class = camel_type_get_global_classfuncs (camel_object_get_type ());
	
	camel_cipher_context_class->sign = cipher_sign;
	camel_cipher_context_class->clearsign = cipher_clearsign;
	camel_cipher_context_class->verify = cipher_verify;
	camel_cipher_context_class->encrypt = cipher_encrypt;
	camel_cipher_context_class->decrypt = cipher_decrypt;
}

CamelType
camel_cipher_context_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_object_get_type (),
					    "CamelCipherContext",
					    sizeof (CamelCipherContext),
					    sizeof (CamelCipherContextClass),
					    (CamelObjectClassInitFunc) camel_cipher_context_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_cipher_context_init,
					    (CamelObjectFinalizeFunc) camel_cipher_context_finalise);
	}
	
	return type;
}


/**
 * camel_cipher_context_new:
 * @session: CamelSession
 *
 * This creates a new CamelCipherContext object which is used to sign,
 * verify, encrypt and decrypt streams.
 *
 * Return value: the new CamelCipherContext
 **/
CamelCipherContext *
camel_cipher_context_new (CamelSession *session)
{
	CamelCipherContext *context;
	
	g_return_val_if_fail (session != NULL, NULL);
	
	context = CAMEL_CIPHER_CONTEXT (camel_object_new (CAMEL_CIPHER_CONTEXT_TYPE));
	
	camel_object_ref (CAMEL_OBJECT (session));
	context->session = session;
	
	return context;
}


/**
 * camel_cipher_context_construct:
 * @context: CamelCipherContext
 * @session: CamelSession
 *
 * Constucts the CamelCipherContext
 **/
void
camel_cipher_context_construct (CamelCipherContext *context, CamelSession *session)
{
	g_return_if_fail (CAMEL_IS_CIPHER_CONTEXT (context));
	g_return_if_fail (CAMEL_IS_SESSION (session));
	
	camel_object_ref (CAMEL_OBJECT (session));
	context->session = session;
}


static int
cipher_sign (CamelCipherContext *ctx, const char *userid, CamelCipherHash hash,
	     CamelStream *istream, CamelStream *ostream, CamelException *ex)
{
	camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
			     _("Signing is not supported by this cipher"));
	return -1;
}

/**
 * camel_cipher_sign:
 * @context: Cipher Context
 * @userid: private key to use to sign the stream
 * @hash: preferred Message-Integrity-Check hash algorithm
 * @istream: input stream
 * @ostream: output stream
 * @ex: exception
 *
 * Signs the input stream and writes the resulting signature to the output stream.
 *
 * Return value: 0 for success or -1 for failure.
 **/
int
camel_cipher_sign (CamelCipherContext *context, const char *userid, CamelCipherHash hash,
		   CamelStream *istream, CamelStream *ostream, CamelException *ex)
{
	int retval;
	
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), -1);
	
	CIPHER_LOCK(context);
	
	retval = CCC_CLASS (context)->sign (context, userid, hash, istream, ostream, ex);
	
	CIPHER_UNLOCK(context);
	
	return retval;
}


static int
cipher_clearsign (CamelCipherContext *context, const char *userid, CamelCipherHash hash,
		  CamelStream *istream, CamelStream *ostream, CamelException *ex)
{
	camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
			     _("Clearsigning is not supported by this cipher"));
	return -1;
}

/**
 * camel_cipher_clearsign:
 * @context: Cipher Context
 * @userid: key id or email address of the private key to sign with
 * @hash: preferred Message-Integrity-Check hash algorithm
 * @istream: input stream
 * @ostream: output stream
 * @ex: exception
 *
 * Clearsigns the input stream and writes the resulting clearsign to the output stream.
 *
 * Return value: 0 for success or -1 for failure.
 **/
int
camel_cipher_clearsign (CamelCipherContext *context, const char *userid, CamelCipherHash hash,
			CamelStream *istream, CamelStream *ostream, CamelException *ex)
{
	int retval;
	
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), -1);
	
	CIPHER_LOCK(context);
	
	retval = CCC_CLASS (context)->clearsign (context, userid, hash, istream, ostream, ex);
	
	CIPHER_UNLOCK(context);
	
	return retval;
}


static CamelCipherValidity *
cipher_verify (CamelCipherContext *context, CamelStream *istream,
	       CamelStream *sigstream, CamelException *ex)
{
	camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
			     _("Verifying is not supported by this cipher"));
	return NULL;
}

/**
 * camel_cipher_verify:
 * @context: Cipher Context
 * @istream: input stream
 * @sigstream: optional detached-signature stream
 * @ex: exception
 *
 * Verifies the signature. If @istream is a clearsigned stream,
 * you should pass %NULL as the sigstream parameter. Otherwise
 * @sigstream is assumed to be the signature stream and is used to
 * verify the integirity of the @istream.
 *
 * Return value: a CamelCipherValidity structure containing information
 * about the integrity of the input stream or %NULL on failure to
 * execute at all.
 **/
CamelCipherValidity *
camel_cipher_verify (CamelCipherContext *context, CamelStream *istream,
		     CamelStream *sigstream, CamelException *ex)
{
	CamelCipherValidity *valid;
	
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), NULL);
	
	CIPHER_LOCK(context);
	
	valid = CCC_CLASS (context)->verify (context, istream, sigstream, ex);
	
	CIPHER_UNLOCK(context);
	
	return valid;
}


static int
cipher_encrypt (CamelCipherContext *context, gboolean sign, const char *userid, GPtrArray *recipients,
		CamelStream *istream, CamelStream *ostream, CamelException *ex)
{
	camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
			     _("Encryption is not supported by this cipher"));
	return -1;
}

/**
 * camel_cipher_encrypt:
 * @context: Cipher Context
 * @sign: sign as well as encrypt
 * @userid: key id (or email address) to use when signing (assuming @sign is %TRUE)
 * @recipients: an array of recipient key ids and/or email addresses
 * @istream: cleartext input stream
 * @ostream: ciphertext output stream
 * @ex: exception
 *
 * Encrypts (and optionally signs) the cleartext input stream and
 * writes the resulting ciphertext to the output stream.
 *
 * Return value: 0 for success or -1 for failure.
 **/
int
camel_cipher_encrypt (CamelCipherContext *context, gboolean sign, const char *userid, GPtrArray *recipients,
		      CamelStream *istream, CamelStream *ostream, CamelException *ex)
{
	int retval;
	
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), -1);
	
	CIPHER_LOCK(context);
	
	retval = CCC_CLASS (context)->encrypt (context, sign, userid, recipients, istream, ostream, ex);
	
	CIPHER_UNLOCK(context);
	
	return retval;
}


static int
cipher_decrypt (CamelCipherContext *context, CamelStream *istream,
		CamelStream *ostream, CamelException *ex)
{
	camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
			     _("Decryption is not supported by this cipher"));
	return -1;
}

/**
 * camel_cipher_decrypt:
 * @context: Cipher Context
 * @ciphertext: ciphertext stream (ie input stream)
 * @cleartext: cleartext stream (ie output stream)
 * @ex: exception
 *
 * Decrypts the ciphertext input stream and writes the resulting
 * cleartext to the output stream.
 *
 * Return value: 0 for success or -1 for failure.
 **/
int
camel_cipher_decrypt (CamelCipherContext *context, CamelStream *istream,
		      CamelStream *ostream, CamelException *ex)
{
	int retval;
	
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), -1);
	
	CIPHER_LOCK(context);
	
	retval = CCC_CLASS (context)->decrypt (context, istream, ostream, ex);
	
	CIPHER_UNLOCK(context);
	
	return retval;
}


/* Cipher Validity stuff */
struct _CamelCipherValidity {
	gboolean valid;
	gchar *description;
};

CamelCipherValidity *
camel_cipher_validity_new (void)
{
	CamelCipherValidity *validity;
	
	validity = g_new (CamelCipherValidity, 1);
	validity->valid = FALSE;
	validity->description = NULL;
	
	return validity;
}

void
camel_cipher_validity_init (CamelCipherValidity *validity)
{
	g_assert (validity != NULL);
	
	validity->valid = FALSE;
	validity->description = NULL;
}

gboolean
camel_cipher_validity_get_valid (CamelCipherValidity *validity)
{
	if (validity == NULL)
		return FALSE;
	
	return validity->valid;
}

void
camel_cipher_validity_set_valid (CamelCipherValidity *validity, gboolean valid)
{
	g_assert (validity != NULL);
	
	validity->valid = valid;
}

gchar *
camel_cipher_validity_get_description (CamelCipherValidity *validity)
{
	if (validity == NULL)
		return NULL;
	
	return validity->description;
}

void
camel_cipher_validity_set_description (CamelCipherValidity *validity, const gchar *description)
{
	g_assert (validity != NULL);
	
	g_free (validity->description);
	validity->description = g_strdup (description);
}

void
camel_cipher_validity_clear (CamelCipherValidity *validity)
{
	g_assert (validity != NULL);
	
	validity->valid = FALSE;
	g_free (validity->description);
	validity->description = NULL;
}

void
camel_cipher_validity_free (CamelCipherValidity *validity)
{
	if (validity == NULL)
		return;
	
	g_free (validity->description);
	g_free (validity);
}
