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

#include "camel-pgp-mime.h"
#include "camel-mime-filter-from.h"
#include "camel-mime-filter-crlf.h"
#include "camel-stream-filter.h"
#include "camel-stream-mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define d(x) x

/** rfc2015 stuff (aka PGP/MIME) *******************************/

gboolean
camel_pgp_mime_is_rfc2015_signed (CamelMimePart *mime_part)
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
	
	/* check that we have a protocol param with the value: "application/pgp-signed" */
	param = header_content_type_param (type, "protocol");
	if (!param || g_strcasecmp (param, "application/pgp-signature"))
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
	 * application/pgp-signature - check it. */
	part = camel_multipart_get_part (mp, 0);
	type = camel_mime_part_get_content_type (part);
	if (header_content_type_is (type, "application", "pgp-signature"))
		return FALSE;
	
	/* The second part should be application/pgp-signature. */
	part = camel_multipart_get_part (mp, 1);
	type = camel_mime_part_get_content_type (part);
	if (!header_content_type_is (type, "application", "pgp-signature"))
		return FALSE;
	
	return TRUE;
}

gboolean
camel_pgp_mime_is_rfc2015_encrypted (CamelMimePart *mime_part)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *mp;
	CamelMimePart *part;
	CamelContentType *type;
	const gchar *param;
	int nparts;
	
	/* check that we have a multipart/encrypted */
	type = camel_mime_part_get_content_type (mime_part);
	if (!header_content_type_is (type, "multipart", "encrypted"))
		return FALSE;
	
	/* check that we have a protocol param with the value: "application/pgp-encrypted" */
	param = header_content_type_param (type, "protocol");
	if (!param || g_strcasecmp (param, "application/pgp-encrypted"))
		return FALSE;
	
	/* check that we have at least 2 subparts */
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	mp = CAMEL_MULTIPART (wrapper);
	nparts = camel_multipart_get_number (mp);
	if (nparts < 2)
		return FALSE;
	
	/* The first part should be application/pgp-encrypted */
	part = camel_multipart_get_part (mp, 0);
	type = camel_mime_part_get_content_type (part);
	if (!header_content_type_is (type, "application", "pgp-encrypted"))
		return FALSE;
	
	/* The second part should be application/octet-stream - this
           is the one we care most about */
	part = camel_multipart_get_part (mp, 1);
	type = camel_mime_part_get_content_type (part);
	if (!header_content_type_is (type, "application", "octet-stream"))
		return FALSE;
	
	return TRUE;
}


static void
pgp_mime_part_sign_restore_part (CamelMimePart *mime_part, GSList *encodings)
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
			
			pgp_mime_part_sign_restore_part (part, encodings);
			encodings = encodings->next;
		}
	} else {
		CamelMimePartEncodingType encoding;
		
		encoding = GPOINTER_TO_INT (encodings->data);
		
		camel_mime_part_set_encoding (mime_part, encoding);
	}
}

static void
pgp_mime_part_sign_prepare_part (CamelMimePart *mime_part, GSList **encodings)
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
			
			pgp_mime_part_sign_prepare_part (part, encodings);
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
 * camel_pgp_mime_part_sign:
 * @context: PGP Context
 * @mime_part: a MIME part that will be replaced by a pgp signed part
 * @userid: userid to sign with
 * @hash: one of CAMEL_PGP_HASH_TYPE_MD5 or CAMEL_PGP_HASH_TYPE_SHA1
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a PGP/MIME multipart in compliance with rfc2015 and
 * replaces @part with the generated multipart/signed. On failure,
 * @ex will be set and #part will remain untouched.
 **/
void
camel_pgp_mime_part_sign (CamelPgpContext *context, CamelMimePart **mime_part, const char *userid,
			  CamelPgpHashType hash, CamelException *ex)
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
	pgp_mime_part_sign_prepare_part (part, &encodings);
	
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
	
	/* get the signature */
	if (camel_pgp_sign (context, userid, hash, stream, sigstream, ex) == -1) {
		camel_object_unref (CAMEL_OBJECT (stream));
		camel_object_unref (CAMEL_OBJECT (sigstream));
		
		/* restore the original encoding */
		pgp_mime_part_sign_restore_part (part, encodings);
		g_slist_free (encodings);
		return;
	}
	
	camel_object_unref (CAMEL_OBJECT (stream));
	camel_stream_reset (sigstream);
	
	/* we don't need these anymore... */
	g_slist_free (encodings);
	
	/* construct the pgp-signature mime part */
	signed_part = camel_mime_part_new ();
	camel_mime_part_set_content (signed_part, CAMEL_STREAM_MEM (sigstream)->buffer->data,
				     CAMEL_STREAM_MEM (sigstream)->buffer->len,
				     "application/pgp-signature");
	camel_object_unref (CAMEL_OBJECT (sigstream));
	
	/* construct the container multipart/signed */
	switch (hash) {
	case CAMEL_PGP_HASH_TYPE_MD5:
		hash_type = "pgp-md5";
		break;
	case CAMEL_PGP_HASH_TYPE_SHA1:
		hash_type = "pgp-sha1";
		break;
	default:
		/* set a reasonable default */
		hash = CAMEL_PGP_HASH_TYPE_SHA1;
		hash_type = "pgp-sha1";
		break;
	}
	
	multipart = camel_multipart_new ();
	
	mime_type = header_content_type_new ("multipart", "signed");
	header_content_type_set_param (mime_type, "micalg", hash_type);
	header_content_type_set_param (mime_type, "protocol", "application/pgp-signature");
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


/**
 * camel_pgp_mime_part_verify:
 * @context: PGP Context
 * @mime_part: a multipart/signed MIME Part
 * @ex: exception
 *
 * Returns a CamelPgpValidity on success or NULL on fail.
 **/
CamelPgpValidity *
camel_pgp_mime_part_verify (CamelPgpContext *context, CamelMimePart *mime_part, CamelException *ex)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *multipart;
	CamelMimePart *part, *sigpart;
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *crlf_filter, *from_filter;
	CamelStream *stream, *sigstream;
	CamelPgpValidity *valid;
	
	g_return_val_if_fail (mime_part != NULL, NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_PART (mime_part), NULL);
	
	if (!camel_pgp_mime_is_rfc2015_signed (mime_part))
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
	valid = camel_pgp_verify (context, stream, sigstream, ex);
	
	camel_object_unref (CAMEL_OBJECT (sigstream));
	camel_object_unref (CAMEL_OBJECT (stream));
	
	return valid;
}


/**
 * camel_pgp_mime_part_encrypt:
 * @context: PGP Context
 * @mime_part: a MIME part that will be replaced by a pgp encrypted part
 * @recipients: list of recipient PGP Key IDs
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a PGP/MIME multipart in compliance with rfc2015 and
 * replaces #mime_part with the generated multipart/signed. On failure,
 * #ex will be set and #part will remain untouched.
 **/
void
camel_pgp_mime_part_encrypt (CamelPgpContext *context, CamelMimePart **mime_part,
			     GPtrArray *recipients, CamelException *ex)
{
	CamelMultipart *multipart;
	CamelMimePart *part, *version_part, *encrypted_part;
	CamelContentType *mime_type;
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
	
	/* pgp encrypt */
	ciphertext = camel_stream_mem_new ();
	if (camel_pgp_encrypt (context, FALSE, NULL, recipients, stream, ciphertext, ex) == -1) {
		camel_object_unref (CAMEL_OBJECT (stream));
		camel_object_unref (CAMEL_OBJECT (ciphertext));
		return;
	}
	
	camel_object_unref (CAMEL_OBJECT (stream));
	camel_stream_reset (ciphertext);
	
	/* construct the version part */
	version_part = camel_mime_part_new ();
	camel_mime_part_set_encoding (version_part, CAMEL_MIME_PART_ENCODING_7BIT);
	camel_mime_part_set_content (version_part, "Version: 1", strlen ("Version: 1"),
				     "application/pgp-encrypted");
	
	/* construct the pgp-encrypted mime part */
	encrypted_part = camel_mime_part_new ();
	camel_mime_part_set_content (encrypted_part, CAMEL_STREAM_MEM (ciphertext)->buffer->data,
				     CAMEL_STREAM_MEM (ciphertext)->buffer->len,
				     "application/octet-stream");
	camel_object_unref (CAMEL_OBJECT (ciphertext));
	camel_mime_part_set_encoding (encrypted_part, CAMEL_MIME_PART_ENCODING_7BIT);
	
	/* construct the container multipart/encrypted */
	multipart = camel_multipart_new ();
	
	mime_type = header_content_type_new ("multipart", "encrypted");
	header_content_type_set_param (mime_type, "protocol", "application/pgp-encrypted");
	camel_data_wrapper_set_mime_type_field (CAMEL_DATA_WRAPPER (multipart), mime_type);
	header_content_type_unref (mime_type);
	
	camel_multipart_set_boundary (multipart, NULL);
	
	/* add the parts to the multipart */
	camel_multipart_add_part (multipart, version_part);
	camel_object_unref (CAMEL_OBJECT (version_part));
	camel_multipart_add_part (multipart, encrypted_part);
	camel_object_unref (CAMEL_OBJECT (encrypted_part));
	
	/* replace the input part with the output part */
	camel_object_unref (CAMEL_OBJECT (*mime_part));
	*mime_part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (*mime_part),
					 CAMEL_DATA_WRAPPER (multipart));
	camel_object_unref (CAMEL_OBJECT (multipart));
}


/**
 * camel_pgp_mime_part_decrypt:
 * @context: PGP Context
 * @mime_part: a multipart/encrypted MIME Part
 * @ex: exception
 *
 * Returns the decrypted MIME Part on success or NULL on fail.
 **/
CamelMimePart *
camel_pgp_mime_part_decrypt (CamelPgpContext *context, CamelMimePart *mime_part, CamelException *ex)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *multipart;
	CamelMimePart *encrypted_part, *part;
	CamelContentType *mime_type;
	CamelStream *stream, *ciphertext;
	
	g_return_val_if_fail (mime_part != NULL, NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_PART (mime_part), NULL);
	
	/* make sure the mime part is a multipart/encrypted */
	if (!camel_pgp_mime_is_rfc2015_encrypted (mime_part))
		return NULL;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	multipart = CAMEL_MULTIPART (wrapper);
	
	/* get the encrypted part (second part) */
	encrypted_part = camel_multipart_get_part (multipart, 1 /* second part starting at 0 */);
	mime_type = camel_mime_part_get_content_type (encrypted_part);
	if (!header_content_type_is (mime_type, "application", "octet-stream"))
		return NULL;
	
	/* get the ciphertext */
	ciphertext = camel_stream_mem_new ();
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (encrypted_part), ciphertext);
	camel_stream_reset (ciphertext);
	
	/* get the cleartext */
	stream = camel_stream_mem_new ();
	if (camel_pgp_decrypt (context, ciphertext, stream, ex) == -1) {
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
