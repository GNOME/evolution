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

#include "camel-smime.h"
#include "camel-mime-filter-from.h"
#include "camel-mime-filter-crlf.h"
#include "camel-stream-filter.h"
#include "camel-stream-mem.h"
#include "camel-multipart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define d(x) x

/** rfc2633 stuff (aka S/MIME v3) ********************************/

gboolean
camel_smime_is_smime_v3_signed (CamelMimePart *mime_part)
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
camel_smime_is_smime_v3_encrypted (CamelMimePart *mime_part)
{
	char *types[] = { "p7m", "p7c", "p7s", NULL };
	const gchar *param, *filename;
	CamelContentType *type;
	int i;
	
	/* check that we have a application/pkcs7-mime part */
	type = camel_mime_part_get_content_type (mime_part);
	if (header_content_type_is (type, "application", "pkcs7-mime")) {
		/* check to make sure it's an encrypted pkcs7-mime part? */
		return TRUE;
	}
	
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


static void
smime_part_sign_restore_part (CamelMimePart *mime_part, GSList *encodings)
{
	CamelDataWrapper *wrapper;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	if (!wrapper)
		return;
	
	if (CAMEL_IS_MULTIPART (wrapper)) {
		int parts, i;
		
		parts = camel_multipart_get_number (CAMEL_MULTIPART (wrapper));
		for (i = 0; i < parts; i++) {
			CamelMimePart *part = camel_multipart_get_part (CAMEL_MULTIPART (wrapper), i);
			
			smime_part_sign_restore_part (part, encodings);
			encodings = encodings->next;
		}
	} else {
		CamelMimePartEncodingType encoding;
		
		encoding = GPOINTER_TO_INT (encodings->data);
		
		camel_mime_part_set_encoding (mime_part, encoding);
	}
}

static void
smime_part_sign_prepare_part (CamelMimePart *mime_part, GSList **encodings)
{
	CamelDataWrapper *wrapper;
	int parts, i;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	if (!wrapper)
		return;
	
	if (CAMEL_IS_MULTIPART (wrapper)) {
		parts = camel_multipart_get_number (CAMEL_MULTIPART (wrapper));
		for (i = 0; i < parts; i++) {
			CamelMimePart *part = camel_multipart_get_part (CAMEL_MULTIPART (wrapper), i);
			
			smime_part_sign_prepare_part (part, encodings);
		}
	} else {
		CamelMimePartEncodingType encoding;
		
		encoding = camel_mime_part_get_encoding (mime_part);
		
		/* FIXME: find the best encoding for this part and use that instead?? */
		/* the encoding should really be QP or Base64 */
		if (encoding != CAMEL_MIME_PART_ENCODING_BASE64)
			camel_mime_part_set_encoding (mime_part, CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE);
		
		*encodings = g_slist_append (*encodings, GINT_TO_POINTER (encoding));
	}
}


/**
 * camel_smime_part_sign:
 * @context: S/MIME Context
 * @mime_part: a MIME part that will be replaced by an S/MIME signed part
 * @userid: userid to sign with
 * @hash: one of CAMEL_CIPHER_HASH_TYPE_MD5 or CAMEL_CIPHER_HASH_TYPE_SHA1
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a S/MIME multipart in compliance with rfc2015/rfc2633 and
 * replaces @part with the generated multipart/signed. On failure,
 * @ex will be set and #part will remain untouched.
 **/
void
camel_smime_part_sign (CamelSMimeContext *context, CamelMimePart **mime_part, const char *userid,
		       CamelCipherHash hash, CamelException *ex)
{
	CamelMimePart *part, *signed_part;
	CamelMultipart *multipart;
	CamelContentType *mime_type;
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *crlf_filter, *from_filter;
	CamelStream *stream, *sigstream;
	gchar *hash_type = NULL;
	GSList *encodings = NULL;
	
	g_return_if_fail (*mime_part != NULL);
	g_return_if_fail (CAMEL_IS_MIME_PART (*mime_part));
	g_return_if_fail (userid != NULL);
	
	part = *mime_part;
	
	/* Prepare all the parts for signing... */
	smime_part_sign_prepare_part (part, &encodings);
	
	/* get the cleartext */
	stream = camel_stream_mem_new ();
	crlf_filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_ENCODE,
						  CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	from_filter = CAMEL_MIME_FILTER (camel_mime_filter_from_new ());
	filtered_stream = camel_stream_filter_new_with_stream (stream);
	camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (crlf_filter));
	camel_object_unref (CAMEL_OBJECT (crlf_filter));
	camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (from_filter));
	camel_object_unref (CAMEL_OBJECT (from_filter));
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (part), CAMEL_STREAM (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (filtered_stream));
	
	/* reset the stream */
	camel_stream_reset (stream);
	
	/* construct the signature stream */
	sigstream = camel_stream_mem_new ();
	
	switch (hash) {
	case CAMEL_CIPHER_HASH_MD5:
		hash_type = "md5";
		break;
	case CAMEL_CIPHER_HASH_SHA1:
		hash_type = "sha1";
		break;
	default:
		/* set a reasonable default */
		hash = CAMEL_CIPHER_HASH_SHA1;
		hash_type = "sha1";
		break;
	}
	
	/* get the signature */
	if (camel_smime_sign (context, userid, hash, stream, sigstream, ex) == -1) {
		camel_object_unref (CAMEL_OBJECT (stream));
		camel_object_unref (CAMEL_OBJECT (sigstream));
		
		/* restore the original encoding */
		smime_part_sign_restore_part (part, encodings);
		g_slist_free (encodings);
		return;
	}
	
	camel_object_unref (CAMEL_OBJECT (stream));
	camel_stream_reset (sigstream);
	
	/* we don't need these anymore... */
	g_slist_free (encodings);
	
	/* construct the pkcs7-signature mime part */
	signed_part = camel_mime_part_new ();
	camel_mime_part_set_content (signed_part, CAMEL_STREAM_MEM (sigstream)->buffer->data,
				     CAMEL_STREAM_MEM (sigstream)->buffer->len,
				     "application/pkcs7-signature");
	camel_object_unref (CAMEL_OBJECT (sigstream));
	camel_mime_part_set_encoding (signed_part, CAMEL_MIME_PART_ENCODING_BASE64);
	camel_mime_part_set_filename (signed_part, "smime.p7s");
	
	/* construct the container multipart/signed */
	multipart = camel_multipart_new ();
	
	mime_type = header_content_type_new ("multipart", "signed");
	header_content_type_set_param (mime_type, "micalg", hash_type);
	header_content_type_set_param (mime_type, "protocol", "application/pkcs7-signature");
	camel_data_wrapper_set_mime_type_field (CAMEL_DATA_WRAPPER (multipart), mime_type);
	header_content_type_unref (mime_type);
	
	camel_multipart_set_boundary (multipart, NULL);
	
	/* add the parts to the multipart */
	camel_multipart_add_part (multipart, part);
	camel_object_unref (CAMEL_OBJECT (part));
	camel_multipart_add_part (multipart, signed_part);
	camel_object_unref (CAMEL_OBJECT (signed_part));
	
	/* replace the input part with the output part */
	*mime_part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (*mime_part),
					 CAMEL_DATA_WRAPPER (multipart));
	camel_object_unref (CAMEL_OBJECT (multipart));
}

struct {
	char *name;
	CamelCipherHash hash;
} known_hash_types[] = {
	{ "md5", CAMEL_CIPHER_HASH_MD5 },
	{ "rsa-md5", CAMEL_CIPHER_HASH_MD5 },
	{ "sha1", CAMEL_CIPHER_HASH_SHA1 },
	{ "rsa-sha1", CAMEL_CIPHER_HASH_SHA1 },
	{ NULL, CAMEL_CIPHER_HASH_DEFAULT }
};

static CamelCipherHash
get_hash_type (const char *string)
{
	int i;
	
	for (i = 0; known_hash_types[i].name; i++)
		if (!g_strcasecmp (known_hash_types[i].name, string))
			return known_hash_types[i].hash;
	
	return CAMEL_CIPHER_HASH_DEFAULT;
}

/**
 * camel_smime_part_verify:
 * @context: S/MIME Context
 * @mime_part: a multipart/signed MIME Part
 * @ex: exception
 *
 * Returns a CamelCipherValidity on success or NULL on fail.
 **/
CamelCipherValidity *
camel_smime_part_verify (CamelSMimeContext *context, CamelMimePart *mime_part, CamelException *ex)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *multipart;
	CamelMimePart *part, *sigpart;
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *crlf_filter, *from_filter;
	CamelStream *stream, *sigstream;
	CamelContentType *type;
	CamelCipherValidity *valid;
	CamelCipherHash hash;
	const char *hash_str;
	
	g_return_val_if_fail (mime_part != NULL, NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_PART (mime_part), NULL);
	
	if (!camel_smime_is_smime_v3_signed (mime_part))
		return NULL;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	multipart = CAMEL_MULTIPART (wrapper);
	
	/* get the plain part */
	part = camel_multipart_get_part (multipart, 0);
	stream = camel_stream_mem_new ();
	crlf_filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_ENCODE,
						  CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	from_filter = CAMEL_MIME_FILTER (camel_mime_filter_from_new ());
	filtered_stream = camel_stream_filter_new_with_stream (stream);
	camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (crlf_filter));
	camel_object_unref (CAMEL_OBJECT (crlf_filter));
	camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (from_filter));
	camel_object_unref (CAMEL_OBJECT (from_filter));
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (part), CAMEL_STREAM (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (filtered_stream));
	camel_stream_reset (stream);
	
	/* get the signed part */
	sigpart = camel_multipart_get_part (multipart, 1);
	sigstream = camel_stream_mem_new ();
	camel_data_wrapper_write_to_stream (camel_medium_get_content_object (CAMEL_MEDIUM (sigpart)),
					    sigstream);
	camel_stream_reset (sigstream);
	
	/* verify */
	type = camel_mime_part_get_content_type (sigpart);
	hash_str = header_content_type_param (type, "micalg");
	hash = get_hash_type (hash_str);
	valid = camel_smime_verify (context, hash, stream, sigstream, ex);
	
	camel_object_unref (CAMEL_OBJECT (sigstream));
	camel_object_unref (CAMEL_OBJECT (stream));
	
	return valid;
}


/**
 * camel_smime_part_encrypt:
 * @context: S/MIME Context
 * @mime_part: a MIME part that will be replaced by a pgp encrypted part
 * @recipients: list of recipient PGP Key IDs
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a PGP/MIME multipart in compliance with rfc2015 and
 * replaces #mime_part with the generated multipart/signed. On failure,
 * #ex will be set and #part will remain untouched.
 **/
void
camel_smime_part_encrypt (CamelSMimeContext *context, CamelMimePart **mime_part,
			  GPtrArray *recipients, CamelException *ex)
{
	CamelMimePart *part, *encrypted_part;
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *crlf_filter;
	CamelStream *stream, *ciphertext;
	
	g_return_if_fail (*mime_part != NULL);
	g_return_if_fail (CAMEL_IS_MIME_PART (*mime_part));
	g_return_if_fail (recipients != NULL);
	
	part = *mime_part;
	
	/* get the contents */
	stream = camel_stream_mem_new ();
	crlf_filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_ENCODE,
						  CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	filtered_stream = camel_stream_filter_new_with_stream (stream);
	camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (crlf_filter));
	camel_object_unref (CAMEL_OBJECT (crlf_filter));
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (part), CAMEL_STREAM (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (filtered_stream));
	camel_stream_reset (stream);
	
	/* smime encrypt */
	ciphertext = camel_stream_mem_new ();
	if (camel_smime_encrypt (context, FALSE, NULL, recipients, stream, ciphertext, ex) == -1) {
		camel_object_unref (CAMEL_OBJECT (stream));
		camel_object_unref (CAMEL_OBJECT (ciphertext));
		return;
	}
	
	camel_object_unref (CAMEL_OBJECT (stream));
	camel_stream_reset (ciphertext);
	
	/* construct the encrypted mime part */
	encrypted_part = camel_mime_part_new ();
	camel_mime_part_set_content (encrypted_part, CAMEL_STREAM_MEM (ciphertext)->buffer->data,
				     CAMEL_STREAM_MEM (ciphertext)->buffer->len,
				     "application/pkcs7-mime; smime-type=enveloped-data");
	camel_mime_part_set_encoding (encrypted_part, CAMEL_MIME_PART_ENCODING_BASE64);
	camel_object_unref (CAMEL_OBJECT (ciphertext));
	
	/* replace the input part with the output part */
	camel_object_unref (CAMEL_OBJECT (*mime_part));
	*mime_part = encrypted_part;
}


/**
 * camel_smime_part_decrypt:
 * @context: S/MIME Context
 * @mime_part: a S/MIME encrypted MIME Part
 * @ex: exception
 *
 * Returns the decrypted MIME Part on success or NULL on fail.
 **/
CamelMimePart *
camel_smime_part_decrypt (CamelSMimeContext *context, CamelMimePart *mime_part, CamelException *ex)
{
	CamelMimePart *part;
	CamelStream *stream, *ciphertext;
	
	g_return_val_if_fail (mime_part != NULL, NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_PART (mime_part), NULL);
	
	/* make sure the mime part is a S/MIME encrypted */
	if (!camel_smime_is_smime_v3_encrypted (mime_part))
		return NULL;
	
	/* get the ciphertext */
	ciphertext = camel_stream_mem_new ();
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (mime_part), ciphertext);
	camel_stream_reset (ciphertext);
	
	/* get the cleartext */
	stream = camel_stream_mem_new ();
	if (camel_smime_decrypt (context, ciphertext, stream, ex) == -1) {
		camel_object_unref (CAMEL_OBJECT (ciphertext));
		camel_object_unref (CAMEL_OBJECT (stream));
		return NULL;
	}
	
	camel_object_unref (CAMEL_OBJECT (ciphertext));
	camel_stream_reset (stream);
	
	/* construct the new decrypted mime part from the stream */
	part = camel_mime_part_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (part), stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	return part;
}
