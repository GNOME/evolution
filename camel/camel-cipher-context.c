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

#include <pthread.h>
#include <string.h>

#include <glib.h>

#include "camel-i18n.h"
#include "camel-cipher-context.h"
#include "camel-stream.h"
#include "camel-operation.h"

#include "camel-mime-utils.h"
#include "camel-medium.h"
#include "camel-multipart.h"
#include "camel-mime-message.h"
#include "camel-mime-filter-canon.h"
#include "camel-stream-filter.h"

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
	     struct _CamelMimePart *ipart, struct _CamelMimePart *opart, CamelException *ex)
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
 * @ipart: Input part.
 * @opart: output part.
 * @ex: exception
 *
 * Converts the (unsigned) part @ipart into a new self-contained mime part @opart.
 * This may be a multipart/signed part, or a simple part for enveloped types.
 *
 * Return value: 0 for success or -1 for failure.
 **/
int
camel_cipher_sign (CamelCipherContext *context, const char *userid, CamelCipherHash hash,
		   struct _CamelMimePart *ipart, struct _CamelMimePart *opart, CamelException *ex)
{
	int retval;
	
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), -1);

	camel_operation_start(NULL, _("Signing message"));

	CIPHER_LOCK(context);
	
	retval = CCC_CLASS (context)->sign (context, userid, hash, ipart, opart, ex);
	
	CIPHER_UNLOCK(context);

	camel_operation_end(NULL);

	return retval;
}

static CamelCipherValidity *
cipher_verify (CamelCipherContext *context, struct _CamelMimePart *sigpart, CamelException *ex)
{
	camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
			     _("Verifying is not supported by this cipher"));
	return NULL;
}

/**
 * camel_cipher_verify:
 * @context: Cipher Context
 * @ipart: part to verify
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
camel_cipher_verify (CamelCipherContext *context, struct _CamelMimePart *ipart, CamelException *ex)
{
	CamelCipherValidity *valid;
	
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), NULL);
	
	camel_operation_start(NULL, _("Verifying message"));

	CIPHER_LOCK(context);
	
	valid = CCC_CLASS (context)->verify (context, ipart, ex);
	
	CIPHER_UNLOCK(context);

	camel_operation_end(NULL);
	
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

	camel_operation_start(NULL, _("Encrypting message"));
	
	CIPHER_LOCK(context);
	
	retval = CCC_CLASS (context)->encrypt (context, userid, recipients, ipart, opart, ex);
	
	CIPHER_UNLOCK(context);

	camel_operation_end(NULL);
	
	return retval;
}

static CamelCipherValidity *
cipher_decrypt(CamelCipherContext *context, struct _CamelMimePart *ipart, struct _CamelMimePart *opart, CamelException *ex)
{
	camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
			     _("Decryption is not supported by this cipher"));
	return NULL;
}

/**
 * camel_cipher_decrypt:
 * @context: 
 * @ipart: 
 * @opart: 
 * @ex: 
 * 
 * Decrypts @ipart into @opart.
 * 
 * Return value: A validity/encryption status.
 **/
CamelCipherValidity *
camel_cipher_decrypt(CamelCipherContext *context, struct _CamelMimePart *ipart, struct _CamelMimePart *opart, CamelException *ex)
{
	CamelCipherValidity *valid;

	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (context), NULL);

	camel_operation_start(NULL, _("Decrypting message"));
	
	CIPHER_LOCK(context);
	
	valid = CCC_CLASS (context)->decrypt (context, ipart, opart, ex);
	
	CIPHER_UNLOCK(context);

	camel_operation_end(NULL);
	
	return valid;
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
static void
ccv_certinfo_free(CamelCipherCertInfo *info)
{
	g_free(info->name);
	g_free(info->email);
	g_free(info);
}

CamelCipherValidity *
camel_cipher_validity_new (void)
{
	CamelCipherValidity *validity;
	
	validity = g_malloc(sizeof(*validity));
	camel_cipher_validity_init(validity);

	return validity;
}

void
camel_cipher_validity_init (CamelCipherValidity *validity)
{
	g_assert (validity != NULL);

	memset(validity, 0, sizeof(*validity));
	e_dlist_init(&validity->children);
	e_dlist_init(&validity->sign.signers);
	e_dlist_init(&validity->encrypt.encrypters);
}

gboolean
camel_cipher_validity_get_valid (CamelCipherValidity *validity)
{
	return validity != NULL
		&& validity->sign.status == CAMEL_CIPHER_VALIDITY_SIGN_GOOD;
}

void
camel_cipher_validity_set_valid (CamelCipherValidity *validity, gboolean valid)
{
	g_assert (validity != NULL);
	
	validity->sign.status = valid?CAMEL_CIPHER_VALIDITY_SIGN_GOOD:CAMEL_CIPHER_VALIDITY_SIGN_BAD;
}

gchar *
camel_cipher_validity_get_description (CamelCipherValidity *validity)
{
	if (validity == NULL)
		return NULL;
	
	return validity->sign.description;
}

void
camel_cipher_validity_set_description (CamelCipherValidity *validity, const gchar *description)
{
	g_assert (validity != NULL);
	
	g_free(validity->sign.description);
	validity->sign.description = g_strdup(description);
}

void
camel_cipher_validity_clear (CamelCipherValidity *validity)
{
	g_assert (validity != NULL);

	/* TODO: this doesn't free children/clear key lists */
	g_free(validity->sign.description);
	g_free(validity->encrypt.description);
	camel_cipher_validity_init(validity);
}

CamelCipherValidity *
camel_cipher_validity_clone(CamelCipherValidity *vin)
{
	CamelCipherValidity *vo;
	CamelCipherCertInfo *info;

	vo = camel_cipher_validity_new();
	vo->sign.status = vin->sign.status;
	vo->sign.description = g_strdup(vin->sign.description);
	vo->encrypt.status = vin->encrypt.status;
	vo->encrypt.description = g_strdup(vin->encrypt.description);

	info = (CamelCipherCertInfo *)vin->sign.signers.head;
	while (info->next) {
		camel_cipher_validity_add_certinfo(vo, CAMEL_CIPHER_VALIDITY_SIGN, info->name, info->email);
		info = info->next;
	}

	info = (CamelCipherCertInfo *)vin->encrypt.encrypters.head;
	while (info->next) {
		camel_cipher_validity_add_certinfo(vo, CAMEL_CIPHER_VALIDITY_ENCRYPT, info->name, info->email);
		info = info->next;
	}

	return vo;
}

/**
 * camel_cipher_validity_add_certinfo:
 * @vin: 
 * @mode: 
 * @name: 
 * @email: 
 * 
 * Add a cert info to the signer or encrypter info.
 **/
void
camel_cipher_validity_add_certinfo(CamelCipherValidity *vin, enum _camel_cipher_validity_mode_t mode, const char *name, const char *email)
{
	CamelCipherCertInfo *info;
	EDList *list;

	info = g_malloc0(sizeof(*info));
	info->name = g_strdup(name);
	info->email = g_strdup(email);

	list = (mode==CAMEL_CIPHER_VALIDITY_SIGN)?&vin->sign.signers:&vin->encrypt.encrypters;
	e_dlist_addtail(list, (EDListNode *)info);

	printf("adding certinfo %s <%s>\n", name?name:"unset", email?email:"unset");
}

/**
 * camel_cipher_validity_envelope:
 * @validity: 
 * @outer: 
 * 
 * Calculate a conglomerate validity based on wrapping one secure part inside
 * another one.
 **/
void
camel_cipher_validity_envelope(CamelCipherValidity *parent, CamelCipherValidity *valid)
{
	CamelCipherCertInfo *info;

	if (parent->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE
	    && parent->encrypt.status == CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE
	    && valid->sign.status == CAMEL_CIPHER_VALIDITY_SIGN_NONE
	    && valid->encrypt.status != CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE) {
		/* case 1: only signed inside only encrypted -> merge both */
		parent->encrypt.status = valid->encrypt.status;
		parent->encrypt.description = g_strdup(valid->encrypt.description);
		info = (CamelCipherCertInfo *)valid->encrypt.encrypters.head;
		while (info->next) {
			camel_cipher_validity_add_certinfo(parent, CAMEL_CIPHER_VALIDITY_ENCRYPT, info->name, info->email);
			info = info->next;
		}
	} else if (parent->sign.status == CAMEL_CIPHER_VALIDITY_SIGN_NONE
		   && parent->encrypt.status != CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE
		   && valid->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE
		   && valid->encrypt.status == CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE) {
		/* case 2: only encrypted inside only signed */
		parent->sign.status = valid->sign.status;
		parent->sign.description = g_strdup(valid->sign.description);
		info = (CamelCipherCertInfo *)valid->sign.signers.head;
		while (info->next) {
			camel_cipher_validity_add_certinfo(parent, CAMEL_CIPHER_VALIDITY_SIGN, info->name, info->email);
			info = info->next;
		}
	}
	/* Otherwise, I dunno - what do you do? */
}

void
camel_cipher_validity_free (CamelCipherValidity *validity)
{
	CamelCipherValidity *child;
	CamelCipherCertInfo *info;

	if (validity == NULL)
		return;

	while ((child = (CamelCipherValidity *)e_dlist_remhead(&validity->children)))
		camel_cipher_validity_free(child);

	while ((info = (CamelCipherCertInfo *)e_dlist_remhead(&validity->sign.signers)))
		ccv_certinfo_free(info);

	while ((info = (CamelCipherCertInfo *)e_dlist_remhead(&validity->encrypt.encrypters)))
		ccv_certinfo_free(info);

	camel_cipher_validity_clear(validity);
	g_free(validity);
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

/* See rfc3156, section 2 and others */
/* We do this simply: Anything not base64 must be qp
   This is so that we can safely translate any occurance of "From "
   into the quoted-printable escaped version safely. */
static void
cc_prepare_sign(CamelMimePart *part)
{
	CamelDataWrapper *dw;
	CamelTransferEncoding encoding;
	int parts, i;
	
	dw = camel_medium_get_content_object((CamelMedium *)part);
	if (!dw)
		return;
	
	if (CAMEL_IS_MULTIPART (dw)) {
		parts = camel_multipart_get_number((CamelMultipart *)dw);
		for (i = 0; i < parts; i++)
			cc_prepare_sign(camel_multipart_get_part((CamelMultipart *)dw, i));
	} else if (CAMEL_IS_MIME_MESSAGE (dw)) {
		cc_prepare_sign((CamelMimePart *)dw);
	} else {
		encoding = camel_mime_part_get_encoding(part);

		if (encoding != CAMEL_TRANSFER_ENCODING_BASE64
		    && encoding != CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE) {
			camel_mime_part_set_encoding(part, CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE);
		}
	}
}

/**
 * camel_cipher_canonical_to_stream:
 * @part: Part to write.
 * @flags: flags for the canonicalisation filter (CamelMimeFilterCanon)
 * @ostream: stream to write canonicalised output to.
 * 
 * Writes a part to a stream in a canonicalised format, suitable for signing/encrypting.
 *
 * The transfer encoding paramaters for the part may be changed by this function.
 * 
 * Return value: -1 on error;
 **/
int
camel_cipher_canonical_to_stream(CamelMimePart *part, guint32 flags, CamelStream *ostream)
{
	CamelStreamFilter *filter;
	CamelMimeFilter *canon;
	int res = -1;

	if (flags & (CAMEL_MIME_FILTER_CANON_FROM|CAMEL_MIME_FILTER_CANON_STRIP))
		cc_prepare_sign(part);

	filter = camel_stream_filter_new_with_stream(ostream);
	canon = camel_mime_filter_canon_new(flags);
	camel_stream_filter_add(filter, canon);
	camel_object_unref(canon);

	if (camel_data_wrapper_write_to_stream((CamelDataWrapper *)part, (CamelStream *)filter) != -1
	    && camel_stream_flush((CamelStream *)filter) != -1)
		res = 0;

	camel_object_unref(filter);
	camel_stream_reset(ostream);

	return res;
}
