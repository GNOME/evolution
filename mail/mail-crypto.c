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


/**
 * mail_crypto_pgp_mime_part_sign:
 * @mime_part: a MIME part that will be replaced by a pgp signed part
 * @userid: userid to sign with
 * @hash: one of CAMEL_CIPHER_HASH_MD5 or CAMEL_CIPHER_HASH_SHA1
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
	
	if (context) {
		camel_pgp_mime_part_sign (context, mime_part, userid, hash, ex);
		camel_object_unref (CAMEL_OBJECT (context));
	} else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create a PGP signature context."));
}


/**
 * mail_crypto_pgp_mime_part_verify:
 * @mime_part: a multipart/signed MIME Part
 * @ex: exception
 *
 * Returns a CamelCipherValidity on success or NULL on fail.
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
	} else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create a PGP verification context."));
	
	return valid;
}


/**
 * mail_crypto_pgp_mime_part_encrypt:
 * @mime_part: a MIME part that will be replaced by a pgp encrypted part
 * @recipients: list of recipient PGP Key IDs
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a PGP/MIME multipart in compliance with rfc2015 and
 * replaces #mime_part with the generated multipart/encrypted. On failure,
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
	} else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create a PGP encryption context."));
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
	} else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create a PGP decryption context."));
	
	return part;
}


/**
 * mail_crypto_smime_part_sign:
 * @mime_part: a MIME part that will be replaced by a S/MIME signed part
 * @userid: userid to sign with
 * @hash: one of CAMEL_CIPHER_HASH_MD5 or CAMEL_CIPHER_HASH_SHA1
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a S/MIME multipart in compliance with rfc2633 and
 * replaces #part with the generated multipart/signed. On failure,
 * #ex will be set and #part will remain untouched.
 **/
void
mail_crypto_smime_part_sign (CamelMimePart **mime_part, const char *userid, CamelCipherHash hash, CamelException *ex)
{
	CamelSMimeContext *context = NULL;
	
#ifdef HAVE_NSS
	context = camel_smime_context_new (session);
#endif
	
	if (context) {
		camel_smime_part_sign (context, mime_part, userid, hash, ex);
		camel_object_unref (CAMEL_OBJECT (context));
	} else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create a S/MIME signature context."));
}


/**
 * mail_crypto_smime_part_verify:
 * @mime_part: a multipart/signed S/MIME Part
 * @ex: exception
 *
 * Returns a CamelCipherValidity on success or NULL on fail.
 **/
CamelCipherValidity *
mail_crypto_smime_part_verify (CamelMimePart *mime_part, CamelException *ex)
{
	CamelCipherValidity *valid = NULL;
	CamelSMimeContext *context = NULL;
	
#ifdef HAVE_NSS
	context = camel_smime_context_new (session);
#endif
	
	if (context) {
		valid = camel_smime_part_verify (context, mime_part, ex);
		camel_object_unref (CAMEL_OBJECT (context));
	} else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create a S/MIME verification context."));
	
	return valid;
}


/**
 * mail_crypto_smime_part_encrypt:
 * @mime_part: a MIME part that will be replaced by a S/MIME encrypted part
 * @recipients: list of recipient S/MIME encryption certificates
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a S/MIME multipart in compliance with rfc2633 and
 * replaces #mime_part with the generated part. On failure,
 * #ex will be set and #part will remain untouched.
 **/
void
mail_crypto_smime_part_encrypt (CamelMimePart **mime_part, GPtrArray *recipients, CamelException *ex)
{
	CamelSMimeContext *context = NULL;
	
#ifdef HAVE_NSS
	context = camel_smime_context_new (session);
#endif
	
	if (context) {
		camel_smime_part_encrypt (context, mime_part, recipients, ex);
		camel_object_unref (CAMEL_OBJECT (context));
	} else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create a S/MIME encryption context."));
}


/**
 * mail_crypto_smime_part_decrypt:
 * @mime_part: an S/MIME encrypted Part
 * @ex: exception
 *
 * Returns the decrypted MIME Part on success or NULL on fail.
 **/
CamelMimePart *
mail_crypto_smime_part_decrypt (CamelMimePart *mime_part, CamelException *ex)
{
	CamelSMimeContext *context = NULL;
	CamelMimePart *part = NULL;
	
#ifdef HAVE_NSS
	context = camel_smime_context_new (session);
#endif
	
	if (context) {
		part = camel_smime_part_decrypt (context, mime_part, ex);
		camel_object_unref (CAMEL_OBJECT (context));
	} else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create a S/MIME decryption context."));
	
	return part;
}
