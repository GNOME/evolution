/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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

#include <stdio.h>
#include <string.h>

#include "camel-multipart-encrypted.h"
#include "camel-mime-filter-crlf.h"
#include "camel-stream-filter.h"
#include "camel-stream-mem.h"
#include "camel-stream-fs.h"
#include "camel-mime-utils.h"
#include "camel-mime-part.h"

static void camel_multipart_encrypted_class_init (CamelMultipartEncryptedClass *klass);
static void camel_multipart_encrypted_init (gpointer object, gpointer klass);
static void camel_multipart_encrypted_finalize (CamelObject *object);

static void set_mime_type_field (CamelDataWrapper *data_wrapper, CamelContentType *mime_type);


static CamelMultipartClass *parent_class = NULL;


CamelType
camel_multipart_encrypted_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_multipart_get_type (),
					    "CamelMultipartEncrypted",
					    sizeof (CamelMultipartEncrypted),
					    sizeof (CamelMultipartEncryptedClass),
					    (CamelObjectClassInitFunc) camel_multipart_encrypted_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_multipart_encrypted_init,
					    (CamelObjectFinalizeFunc) camel_multipart_encrypted_finalize);
	}
	
	return type;
}


static void
camel_multipart_encrypted_class_init (CamelMultipartEncryptedClass *klass)
{
	CamelDataWrapperClass *camel_data_wrapper_class = CAMEL_DATA_WRAPPER_CLASS (klass);
	
	parent_class = (CamelMultipartClass *) camel_multipart_get_type ();
	
	/* virtual method overload */
	camel_data_wrapper_class->set_mime_type_field = set_mime_type_field;
}

static void
camel_multipart_encrypted_init (gpointer object, gpointer klass)
{
	CamelMultipartEncrypted *multipart = (CamelMultipartEncrypted *) object;
	
	camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (multipart), "multipart/encrypted");
	
	multipart->decrypted = NULL;
}

static void
camel_multipart_encrypted_finalize (CamelObject *object)
{
	CamelMultipartEncrypted *mpe = (CamelMultipartEncrypted *) object;
	
	g_free (mpe->protocol);
	
	if (mpe->decrypted)
		camel_object_unref (mpe->decrypted);
}

/* we snoop the mime type to get the protocol */
static void
set_mime_type_field (CamelDataWrapper *data_wrapper, CamelContentType *mime_type)
{
	CamelMultipartEncrypted *mpe = (CamelMultipartEncrypted *) data_wrapper;
	
	if (mime_type) {
		const char *protocol;
		
		protocol = header_content_type_param (mime_type, "protocol");
		g_free (mpe->protocol);
		mpe->protocol = g_strdup (protocol);
	}
	
	((CamelDataWrapperClass *) parent_class)->set_mime_type_field (data_wrapper, mime_type);
}


/**
 * camel_multipart_encrypted_new:
 *
 * Create a new CamelMultipartEncrypted object.
 *
 * A MultipartEncrypted should be used to store and create parts of
 * type "multipart/encrypted".
 *
 * Returns a new CamelMultipartEncrypted
 **/
CamelMultipartEncrypted *
camel_multipart_encrypted_new (void)
{
	CamelMultipartEncrypted *multipart;
	
	multipart = (CamelMultipartEncrypted *) camel_object_new (CAMEL_MULTIPART_ENCRYPTED_TYPE);
	
	return multipart;
}


int
camel_multipart_encrypted_encrypt (CamelMultipartEncrypted *mpe, CamelMimePart *content,
				   CamelCipherContext *cipher, const char *userid,
				   GPtrArray *recipients, CamelException *ex)
{
	CamelMimePart *version_part, *encrypted_part;
	CamelContentType *mime_type;
	CamelDataWrapper *wrapper;
	CamelStream *filtered_stream;
	CamelStream *stream, *ciphertext;
	CamelMimeFilter *crlf_filter;
	
	g_return_val_if_fail (CAMEL_IS_MULTIPART_ENCRYPTED (mpe), -1);
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (cipher), -1);
	g_return_val_if_fail (cipher->encrypt_protocol != NULL, -1);
	g_return_val_if_fail (CAMEL_IS_MIME_PART (content), -1);
	
	/* get the cleartext */
	stream = camel_stream_mem_new ();
	filtered_stream = (CamelStream *) camel_stream_filter_new_with_stream (stream);
	
	crlf_filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_ENCODE,
						  CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	camel_stream_filter_add (CAMEL_STREAM_FILTER (filtered_stream), crlf_filter);
	camel_object_unref (crlf_filter);
	
	camel_data_wrapper_write_to_stream ((CamelDataWrapper *) content, filtered_stream);
	camel_stream_flush (filtered_stream);
	camel_object_unref (filtered_stream);
	
	/* reset the content stream */
	camel_stream_reset (stream);
	
	/* encrypt the content stream */
	ciphertext = camel_stream_mem_new ();
	if (camel_cipher_encrypt (cipher, FALSE, userid, recipients, stream, ciphertext, ex) == -1) {
		camel_object_unref (ciphertext);
		camel_object_unref (stream);
		return -1;
	}
	
	camel_object_unref (stream);
	camel_stream_reset (ciphertext);
	
	/* construct the version part */
	stream = camel_stream_mem_new ();
	camel_stream_write_string (stream, "Version: 1\n");
	camel_stream_reset (stream);
	
	version_part = camel_mime_part_new ();
	wrapper = camel_data_wrapper_new ();
	camel_data_wrapper_set_mime_type (wrapper, cipher->encrypt_protocol);
	camel_data_wrapper_construct_from_stream (wrapper, stream);
	camel_object_unref (stream);
	camel_medium_set_content_object ((CamelMedium *) version_part, wrapper);
	camel_object_unref (wrapper);
	
	/* construct the encrypted mime part */
	encrypted_part = camel_mime_part_new ();
	wrapper = camel_data_wrapper_new ();
	camel_data_wrapper_set_mime_type (wrapper, "application/octet-stream; name=encrypted.asc");
	camel_data_wrapper_construct_from_stream (wrapper, ciphertext);
	camel_object_unref (ciphertext);
	camel_medium_set_content_object ((CamelMedium *) encrypted_part, wrapper);
	camel_object_unref (wrapper);
	
	/* save the version and encrypted parts */
	/* FIXME: make sure there aren't any other parts?? */
	camel_multipart_add_part (CAMEL_MULTIPART (mpe), version_part);
	camel_object_unref (version_part);
	camel_multipart_add_part (CAMEL_MULTIPART (mpe), encrypted_part);
	camel_object_unref (encrypted_part);
	
	/* cache the decrypted content */
	camel_object_ref (content);
	mpe->decrypted = content;
	
	/* set the content-type params for this multipart/encrypted part */
	mime_type = header_content_type_new ("multipart", "encrypted");
	header_content_type_set_param (mime_type, "protocol", cipher->encrypt_protocol);
	camel_data_wrapper_set_mime_type_field (CAMEL_DATA_WRAPPER (mpe), mime_type);
	header_content_type_unref (mime_type);
	camel_multipart_set_boundary ((CamelMultipart *) mpe, NULL);
	
	return 0;
}


CamelMimePart *
camel_multipart_encrypted_decrypt (CamelMultipartEncrypted *mpe,
				   CamelCipherContext *cipher,
				   CamelException *ex)
{
	CamelMimePart *version_part, *encrypted_part, *decrypted_part;
	CamelContentType *mime_type;
	CamelDataWrapper *wrapper;
	CamelStream *filtered_stream;
	CamelMimeFilter *crlf_filter;
	CamelStream *ciphertext;
	CamelStream *stream;
	const char *protocol;
	char *content_type;
	
	g_return_val_if_fail (CAMEL_IS_MULTIPART_ENCRYPTED (mpe), NULL);
	g_return_val_if_fail (CAMEL_IS_CIPHER_CONTEXT (cipher), NULL);
	g_return_val_if_fail (cipher->encrypt_protocol != NULL, NULL);
	
	if (mpe->decrypted) {
		/* we seem to have already decrypted the part */
		camel_object_ref (mpe->decrypted);
		return mpe->decrypted;
	}
	
	protocol = mpe->protocol;
	
	if (protocol) {
		/* make sure the protocol matches the cipher encrypt protocol */
		if (strcasecmp (cipher->encrypt_protocol, protocol) != 0) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Failed to decrypt MIME part: protocol error"));
			
			return NULL;
		}
	} else {
		/* *shrug* - I guess just go on as if they match? */
		protocol = cipher->encrypt_protocol;
	}
	
	/* make sure the protocol matches the version part's content-type */
	version_part = camel_multipart_get_part (CAMEL_MULTIPART (mpe), CAMEL_MULTIPART_ENCRYPTED_VERSION);
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (version_part));
	content_type = camel_data_wrapper_get_mime_type (wrapper);
	if (strcasecmp (content_type, protocol) != 0) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Failed to decrypt MIME part: protocol error"));
		
		g_free (content_type);
		
		return NULL;
	}
	g_free (content_type);
	
	/* get the encrypted part (second part) */
	encrypted_part = camel_multipart_get_part (CAMEL_MULTIPART (mpe), CAMEL_MULTIPART_ENCRYPTED_CONTENT);
	mime_type = camel_mime_part_get_content_type (encrypted_part);
	if (!header_content_type_is (mime_type, "application", "octet-stream")) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Failed to decrypt MIME part: invalid structure"));
		return NULL;
	}
	
	/* get the ciphertext stream */
	ciphertext = camel_stream_mem_new ();
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (encrypted_part));
	camel_data_wrapper_write_to_stream (wrapper, ciphertext);
	camel_stream_reset (ciphertext);
	
	stream = camel_stream_mem_new ();
	filtered_stream = (CamelStream *) camel_stream_filter_new_with_stream (stream);
	crlf_filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_DECODE,
						  CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	camel_stream_filter_add (CAMEL_STREAM_FILTER (filtered_stream), crlf_filter);
	camel_object_unref (crlf_filter);
	
	/* get the cleartext */
	if (camel_cipher_decrypt (cipher, ciphertext, filtered_stream, ex) == -1) {
		camel_object_unref (filtered_stream);
		camel_object_unref (ciphertext);
		camel_object_unref (stream);
		
		return NULL;
	}
	
	camel_stream_flush (filtered_stream);
	camel_object_unref (filtered_stream);
	camel_object_unref (ciphertext);
	camel_stream_reset (stream);
	
	decrypted_part = camel_mime_part_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (decrypted_part), stream);
	
	if (decrypted_part) {
		/* cache the decrypted part */
		camel_object_ref (decrypted_part);
		mpe->decrypted = decrypted_part;
	} else {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Failed to decrypt MIME part: parse error"));
	}
	
	return decrypted_part;
}
