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


#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "mail-crypto.h"
#include "mail-session.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <signal.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#include <camel/camel-mime-filter-from.h>

/** rfc2015 stuff *******************************/

gboolean
mail_crypto_is_rfc2015_signed (CamelMimePart *mime_part)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *mp;
	CamelMimePart *part;
	CamelContentType *type;
	const gchar *param;
	int nparts;
	
	/* check that we have a multipart/signed */
	type = camel_mime_part_get_content_type (mime_part);
	if (!header_content_type_is (type, "multipart", "signed"))
		return FALSE;
	
	/* check that we have a protocol param with the value: "application/pgp-signed" */
	param = header_content_type_param (type, "protocol");
	if (!param || g_strcasecmp (param, "application/pgp-signed"))
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
	if (!header_content_type_is (type, "application", "pgp-siganture"))
		return FALSE;
	
	/* FIXME: Implement multisig stuff */	
	
	return TRUE;
}

gboolean
mail_crypto_is_rfc2015_encrypted (CamelMimePart *mime_part)
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

/**
 * pgp_mime_part_sign:
 * @mime_part: a MIME part that will be replaced by a pgp signed part
 * @userid: userid to sign with
 * @hash: one of PGP_HASH_TYPE_MD5 or PGP_HASH_TYPE_SHA1
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a PGP/MIME multipart in compliance with rfc2015 and
 * replaces #part with the generated multipart/signed. On failure,
 * #ex will be set and #part will remain untouched.
 **/
void
pgp_mime_part_sign (CamelMimePart **mime_part, const gchar *userid, PgpHashType hash, CamelException *ex)
{
	CamelMimePart *part, *signed_part;
	CamelMultipart *multipart;
	CamelMimePartEncodingType encoding;
	CamelContentType *mime_type;
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *crlf_filter, *from_filter;
	CamelStream *stream;
	GByteArray *array;
	gchar *cleartext, *signature;
	gchar *hash_type = NULL;
	gint clearlen;
	
	g_return_if_fail (*mime_part != NULL);
	g_return_if_fail (CAMEL_IS_MIME_PART (*mime_part));
	g_return_if_fail (userid != NULL);
	g_return_if_fail (hash != PGP_HASH_TYPE_NONE);
	
	part = *mime_part;
	encoding = camel_mime_part_get_encoding (part);
	
	/* the encoding should really be QP or Base64 */
	if (encoding != CAMEL_MIME_PART_ENCODING_BASE64)
		camel_mime_part_set_encoding (part, CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE);
	
	/* get the cleartext */
	array = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (array);
	crlf_filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_ENCODE,
						  CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	from_filter = CAMEL_MIME_FILTER (camel_mime_filter_from_new ());
	filtered_stream = camel_stream_filter_new_with_stream (stream);
	camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (crlf_filter));
	camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (from_filter));
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (part), CAMEL_STREAM (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (stream));
	cleartext = array->data;
	clearlen = array->len;
	
	/* get the signature */
	signature = openpgp_sign (cleartext, clearlen, userid, hash, ex);
	g_byte_array_free (array, TRUE);
	if (camel_exception_is_set (ex)) {
		/* restore the original encoding */
		camel_mime_part_set_encoding (part, encoding);
		return;
	}
	
	/* construct the pgp-signature mime part */
	fprintf (stderr, "signature:\n%s\n", signature);
	signed_part = camel_mime_part_new ();
	camel_mime_part_set_content (signed_part, signature, strlen (signature),
				     "application/pgp-signature");
	g_free (signature);
	camel_mime_part_set_encoding (signed_part, CAMEL_MIME_PART_ENCODING_DEFAULT);
	
	/* construct the container multipart/signed */
	multipart = camel_multipart_new ();
	camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (multipart),
					  "multipart/signed");
	switch (hash) {
	case PGP_HASH_TYPE_MD5:
		hash_type = "pgp-md5";
		break;
	case PGP_HASH_TYPE_SHA1:
		hash_type = "pgp-sha1";
		break;
	default:
		g_assert_not_reached ();
	}
	
	mime_type = camel_data_wrapper_get_mime_type_field (CAMEL_DATA_WRAPPER (multipart));
	header_content_type_set_param (mime_type, "micalg", hash_type);
	header_content_type_set_param (mime_type, "protocol", "application/pgp-signature");
	camel_multipart_set_boundary (multipart, NULL);
	
	/* add the parts to the multipart */
	camel_multipart_add_part (multipart, part);
	camel_object_unref (CAMEL_OBJECT (part));
	camel_multipart_add_part (multipart, signed_part);
	camel_object_unref (CAMEL_OBJECT (signed_part));
	
	/* replace the input part with the output part */
	camel_object_unref (CAMEL_OBJECT (*mime_part));
	*mime_part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (*mime_part),
					 CAMEL_DATA_WRAPPER (multipart));
	camel_object_unref (CAMEL_OBJECT (multipart));
}


/**
 * pgp_mime_part_verify:
 * @mime_part: a multipart/signed MIME Part
 * @ex: exception
 *
 * Returns TRUE if the signature is valid otherwise returns
 * FALSE. Note: you may want to check the exception if it fails as
 * there may be useful information to give to the user; example:
 * verification may have failed merely because the user doesn't have
 * the sender's key on her system.
 **/
gboolean
pgp_mime_part_verify (CamelMimePart *mime_part, CamelException *ex)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *multipart;
	CamelMimePart *part, *sigpart;
	GByteArray *content, *sig;
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *crlf_filter;
	CamelStream *stream;
	gboolean valid = FALSE;
	
	g_return_val_if_fail (mime_part != NULL, FALSE);
	g_return_val_if_fail (CAMEL_IS_MIME_PART (mime_part), FALSE);
	
	if (!mail_crypto_is_rfc2015_signed (mime_part))
		return FALSE;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	multipart = CAMEL_MULTIPART (wrapper);
	
	/* get the plain part */
	part = camel_multipart_get_part (multipart, 0);
	content = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (content);
	crlf_filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_ENCODE,
						  CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	filtered_stream = camel_stream_filter_new_with_stream (stream);
	camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (crlf_filter));
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (part), CAMEL_STREAM (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (stream));
	
	/* get the signed part */
	sigpart = camel_multipart_get_part (multipart, 1);
	sig = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (sig);
	camel_data_wrapper_write_to_stream (camel_medium_get_content_object (CAMEL_MEDIUM (sigpart)), stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	/* verify */
	valid = openpgp_verify (content->data, content->len,
				sig->data, sig->len, ex);
	
	g_byte_array_free (content, TRUE);
	g_byte_array_free (sig, TRUE);
	
	return valid;
}


/**
 * pgp_mime_part_encrypt:
 * @mime_part: a MIME part that will be replaced by a pgp encrypted part
 * @recipients: list of recipient PGP Key IDs
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a PGP/MIME multipart in compliance with rfc2015 and
 * replaces #mime_part with the generated multipart/signed. On failure,
 * #ex will be set and #part will remain untouched.
 **/
void
pgp_mime_part_encrypt (CamelMimePart **mime_part, const GPtrArray *recipients, CamelException *ex)
{
	CamelMultipart *multipart;
	CamelMimePart *part, *version_part, *encrypted_part;
	CamelContentType *mime_type;
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *crlf_filter;
	CamelStream *stream;
	GByteArray *contents;
	gchar *ciphertext;
	
	g_return_if_fail (*mime_part != NULL);
	g_return_if_fail (CAMEL_IS_MIME_PART (*mime_part));
	g_return_if_fail (recipients != NULL);
	
	part = *mime_part;
	
	/* get the contents */
        contents = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (contents);
	crlf_filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_ENCODE,
						  CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	filtered_stream = camel_stream_filter_new_with_stream (stream);
	camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (crlf_filter));
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (part), CAMEL_STREAM (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (stream));
	
	/* pgp encrypt */
	ciphertext = openpgp_encrypt (contents->data,
				      contents->len,
				      recipients, FALSE, NULL, ex);
	if (camel_exception_is_set (ex))
		return;
	
	/* construct the version part */
	version_part = camel_mime_part_new ();
	camel_mime_part_set_encoding (version_part, CAMEL_MIME_PART_ENCODING_7BIT);
	camel_mime_part_set_content (version_part, "Version: 1", strlen ("Version: 1"),
				     "application/pgp-encrypted");
	
	/* construct the pgp-encrypted mime part */
	encrypted_part = camel_mime_part_new ();
	camel_mime_part_set_encoding (encrypted_part, CAMEL_MIME_PART_ENCODING_7BIT);
	camel_mime_part_set_content (encrypted_part, ciphertext, strlen (ciphertext),
				     "application/octet-stream");
	g_free (ciphertext);
	
	/* construct the container multipart/signed */
	multipart = camel_multipart_new ();
	camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (multipart),
					  "multipart/encrypted");
	camel_multipart_set_boundary (multipart, NULL);
	mime_type = camel_data_wrapper_get_mime_type_field (CAMEL_DATA_WRAPPER (multipart));
	header_content_type_set_param (mime_type, "protocol", "application/pgp-encrypted");
	
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
 * pgp_mime_part_decrypt:
 * @mime_part: a multipart/encrypted MIME Part
 * @ex: exception
 *
 * Returns the decrypted MIME Part on success or NULL on fail.
 **/
CamelMimePart *
pgp_mime_part_decrypt (CamelMimePart *mime_part, CamelException *ex)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *multipart;
	CamelMimeParser *parser;
	CamelMimePart *encrypted_part, *part;
	CamelContentType *mime_type;
	CamelStream *stream;
	GByteArray *content;
	gchar *cleartext, *ciphertext = NULL;
	int cipherlen, clearlen;
	
	g_return_val_if_fail (mime_part != NULL, NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_PART (mime_part), NULL);
	
	/* make sure the mime part is a multipart/encrypted */
	if (!mail_crypto_is_rfc2015_encrypted (mime_part))
		return NULL;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	multipart = CAMEL_MULTIPART (wrapper);
	
	/* get the encrypted part (second part) */
	encrypted_part = camel_multipart_get_part (multipart, 1 /* second part starting at 0 */);
	mime_type = camel_mime_part_get_content_type (encrypted_part);
	if (!header_content_type_is (mime_type, "application", "octet-stream"))
		return NULL;
	
	/* get the ciphertext */
	content = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (content);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (encrypted_part), stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	ciphertext = content->data;
	cipherlen = content->len;
	
	/* get the cleartext */
	cleartext = openpgp_decrypt (ciphertext, cipherlen, &clearlen, ex);
	g_byte_array_free (content, TRUE);
	if (camel_exception_is_set (ex))
		return NULL;
	
	/* create a stream based on the returned cleartext */
	stream = camel_stream_mem_new ();
	camel_stream_write (stream, cleartext, clearlen);
	camel_stream_reset (stream);
	g_free (cleartext);
	
	/* construct the new decrypted mime part from the stream */
	part = camel_mime_part_new ();
	parser = camel_mime_parser_new ();
	camel_mime_parser_init_with_stream (parser, stream);
	camel_mime_part_construct_from_parser (part, parser);
	camel_object_unref (CAMEL_OBJECT (parser));
	camel_object_unref (CAMEL_OBJECT (stream));
	
	return part;
}
