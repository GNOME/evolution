/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2003 Ximian, Inc. (www.ximian.com)
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

#include <glib.h>
#include <pthread.h>

#include "camel-cipher-context.h"
#include "camel-stream.h"

#define CIPHER_LOCK(ctx)   g_mutex_lock (((CamelCipherContext *) ctx)->priv->lock)
#define CIPHER_UNLOCK(ctx) g_mutex_unlock (((CamelCipherContext *) ctx)->priv->lock);

#define d(x)

#define CCC_CLASS(o) CAMEL_CIPHER_CONTEXT_CLASS(CAMEL_OBJECT_GET_CLASS(o))

struct _CamelCipherContextPrivate {
	GMutex *lock;
};

static CamelObjectClass *parent_class = NULL;

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
	
	camel_object_ref (session);
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
	
	camel_object_ref (session);
	context->session = session;
}

static int
cipher_sign (CamelCipherContext *ctx, const char *userid, CamelCipherHash hash,
	     struct _CamelStream *istream, struct _CamelMimePart *sigpart, CamelException *ex)
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
 * @sigpart: output signature part.
 * @ex: exception
 *
 * Signs the input stream and writes the resulting signature to output @sigpart.
 *
 * Return value: 0 for success or -1 for failure.
 **/
int
camel_cipher_sign (CamelCipherContext *context, const char *userid, CamelCipherHash hash,
		   struct _CamelStream *istream, struct _CamelMimePart *sigpart, CamelException *ex)
{
	int retval;
	
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), -1);
	
	CIPHER_LOCK(context);
	
	retval = CCC_CLASS (context)->sign (context, userid, hash, istream, sigpart, ex);
	
	CIPHER_UNLOCK(context);
	
	return retval;
}

static CamelCipherValidity *
cipher_verify (CamelCipherContext *context, CamelCipherHash hash, struct _CamelStream *istream,
	       struct _CamelMimePart *sigpart, CamelException *ex)
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
camel_cipher_verify (CamelCipherContext *context, CamelCipherHash hash, struct _CamelStream *istream,
		     struct _CamelMimePart *sigpart, CamelException *ex)
{
	CamelCipherValidity *valid;
	
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), NULL);
	
	CIPHER_LOCK(context);
	
	valid = CCC_CLASS (context)->verify (context, hash, istream, sigpart, ex);
	
	CIPHER_UNLOCK(context);
	
	return valid;
}

static int
cipher_encrypt (CamelCipherContext *context, const char *userid, GPtrArray *recipients,
		struct _CamelMimePart *ipart, struct _CamelMimePart *opart, CamelException *ex)
{
	camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
			     _("Encryption is not supported by this cipher"));
	return -1;
}

/**
 * camel_cipher_encrypt:
 * @context: Cipher Context
 * @userid: key id (or email address) to use when signing, or NULL to not sign.
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
camel_cipher_encrypt (CamelCipherContext *context, const char *userid, GPtrArray *recipients,
		      struct _CamelMimePart *ipart, struct _CamelMimePart *opart, CamelException *ex)
{
	int retval;
	
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), -1);
	
	CIPHER_LOCK(context);
	
	retval = CCC_CLASS (context)->encrypt (context, userid, recipients, ipart, opart, ex);
	
	CIPHER_UNLOCK(context);
	
	return retval;
}

static struct _CamelMimePart *
cipher_decrypt(CamelCipherContext *context, struct _CamelMimePart *ipart, CamelException *ex)
{
	camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
			     _("Decryption is not supported by this cipher"));
	return NULL;
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
struct _CamelMimePart *
camel_cipher_decrypt (CamelCipherContext *context, struct _CamelMimePart *ipart, CamelException *ex)
{
	struct _CamelMimePart *opart;

	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), NULL);
	
	CIPHER_LOCK(context);
	
	opart = CCC_CLASS (context)->decrypt (context, ipart, ex);
	
	CIPHER_UNLOCK(context);
	
	return opart;
}

static int
cipher_import_keys (CamelCipherContext *context, struct _CamelStream *istream, CamelException *ex)
{
	camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
			     _("You may not import keys with this cipher"));
	
	return -1;
}

/**
 * camel_cipher_import_keys:
 * @ctx: Cipher Context
 * @istream: input stream (containing keys)
 * @ex: exception
 *
 * Imports a stream of keys/certificates contained within @istream
 * into the key/certificate database controlled by @ctx.
 *
 * Returns 0 on success or -1 on fail.
 **/
int
camel_cipher_import_keys (CamelCipherContext *context, struct _CamelStream *istream, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), -1);
	g_return_val_if_fail (CAMEL_IS_STREAM (istream), -1);
	
	return CCC_CLASS (context)->import_keys (context, istream, ex);
}

static int
cipher_export_keys (CamelCipherContext *context, GPtrArray *keys,
		    struct _CamelStream *ostream, CamelException *ex)
{
	camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
			     _("You may not export keys with this cipher"));
	
	return -1;
}

/**
 * camel_cipher_export_keys:
 * @ctx: Cipher Context
 * @keys: an array of key ids
 * @ostream: output stream
 * @ex: exception
 *
 * Exports the keys/certificates in @keys to the stream @ostream from
 * the key/certificate database controlled by @ctx.
 *
 * Returns 0 on success or -1 on fail.
 **/
int
camel_cipher_export_keys (CamelCipherContext *context, GPtrArray *keys,
			  struct _CamelStream *ostream, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), -1);
	g_return_val_if_fail (CAMEL_IS_STREAM (ostream), -1);
	g_return_val_if_fail (keys != NULL, -1);
	
	return CCC_CLASS (context)->export_keys (context, keys, ostream, ex);
}

static CamelCipherHash
cipher_id_to_hash(CamelCipherContext *context, const char *id)
{
	return CAMEL_CIPHER_HASH_DEFAULT;
}

/* a couple of util functions */
CamelCipherHash
camel_cipher_id_to_hash(CamelCipherContext *context, const char *id)
{
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), CAMEL_CIPHER_HASH_DEFAULT);
	
	return CCC_CLASS (context)->id_to_hash (context, id);
}

static const char *
cipher_hash_to_id(CamelCipherContext *context, CamelCipherHash hash)
{
	return NULL;
}

const char *
camel_cipher_hash_to_id(CamelCipherContext *context, CamelCipherHash hash)
{
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), NULL);
	
	return CCC_CLASS (context)->hash_to_id (context, hash);
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

/* ********************************************************************** */

static void
camel_cipher_context_init (CamelCipherContext *context)
{
	context->priv = g_new0 (struct _CamelCipherContextPrivate, 1);
	context->priv->lock = g_mutex_new ();
}

static void
camel_cipher_context_finalise (CamelObject *o)
{
	CamelCipherContext *context = (CamelCipherContext *)o;
	
	camel_object_unref (CAMEL_OBJECT (context->session));
	
	g_mutex_free (context->priv->lock);
	
	g_free (context->priv);
}

static void
camel_cipher_context_class_init (CamelCipherContextClass *camel_cipher_context_class)
{
	parent_class = camel_type_get_global_classfuncs (camel_object_get_type ());
	
	camel_cipher_context_class->hash_to_id = cipher_hash_to_id;
	camel_cipher_context_class->id_to_hash = cipher_id_to_hash;
	camel_cipher_context_class->sign = cipher_sign;
	camel_cipher_context_class->verify = cipher_verify;
	camel_cipher_context_class->encrypt = cipher_encrypt;
	camel_cipher_context_class->decrypt = cipher_decrypt;
	camel_cipher_context_class->import_keys = cipher_import_keys;
	camel_cipher_context_class->export_keys = cipher_export_keys;
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
