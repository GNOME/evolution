/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <camel/camel-gpg-context.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-mime-part.h>

#include "camel-test.h"
#include "session.h"

#define CAMEL_PGP_SESSION_TYPE     (camel_pgp_session_get_type ())
#define CAMEL_PGP_SESSION(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_PGP_SESSION_TYPE, CamelPgpSession))
#define CAMEL_PGP_SESSION_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_PGP_SESSION_TYPE, CamelPgpSessionClass))
#define CAMEL_PGP_IS_SESSION(o)    (CAMEL_CHECK_TYPE((o), CAMEL_PGP_SESSION_TYPE))


typedef struct _CamelPgpSession {
	CamelSession parent_object;
	
} CamelPgpSession;

typedef struct _CamelPgpSessionClass {
	CamelSessionClass parent_class;
	
} CamelPgpSessionClass;


static char *get_password (CamelSession *session, const char *prompt,
			   guint32 flags,
			   CamelService *service, const char *item,
			   CamelException *ex);

static void
init (CamelPgpSession *session)
{
	;
}

static void
class_init (CamelPgpSessionClass *camel_pgp_session_class)
{
	CamelSessionClass *camel_session_class =
		CAMEL_SESSION_CLASS (camel_pgp_session_class);
	
	/* virtual method override */
	camel_session_class->get_password = get_password;
}

static CamelType
camel_pgp_session_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (
			camel_test_session_get_type (),
			"CamelPgpSession",
			sizeof (CamelPgpSession),
			sizeof (CamelPgpSessionClass),
			(CamelObjectClassInitFunc) class_init,
			NULL,
			(CamelObjectInitFunc) init,
			NULL);
	}
	
	return type;
}

static char *
get_password (CamelSession *session, const char *prompt, guint32 flags,
	      CamelService *service, const char *item, CamelException *ex)
{
	return g_strdup ("no.secret");
}

static CamelSession *
camel_pgp_session_new (const char *path)
{
	CamelSession *session;
	
	session = CAMEL_SESSION (camel_object_new (CAMEL_PGP_SESSION_TYPE));
	
	camel_session_construct (session, path);
	
	return session;
}


int main (int argc, char **argv)
{
	CamelSession *session;
	CamelCipherContext *ctx;
	CamelException *ex;
	CamelCipherValidity *valid;
	CamelStream *stream1, *stream2;
	struct _CamelMimePart *sigpart, *conpart, *encpart, *outpart;
	CamelDataWrapper *dw;
	GPtrArray *recipients;
	GByteArray *buf;
	char *before, *after;
	int ret;
	
	camel_test_init (argc, argv);
	
	/* clear out any camel-test data */
	system ("/bin/rm -rf /tmp/camel-test");
	system ("/bin/mkdir /tmp/camel-test");
	setenv ("GNUPGHOME", "/tmp/camel-test/.gnupg", 1);
	
	/* import the gpg keys */
	if ((ret = system ("gpg < /dev/null > /dev/null 2>&1")) == -1)
		return 77;
	else if (WEXITSTATUS (ret) == 127)
		return 77;
	
	system ("gpg --import ../data/camel-test.gpg.pub > /dev/null 2>&1");
	system ("gpg --import ../data/camel-test.gpg.sec > /dev/null 2>&1");
	
	session = camel_pgp_session_new ("/tmp/camel-test");
	
	ex = camel_exception_new ();
	
	ctx = camel_gpg_context_new (session);
	camel_gpg_context_set_always_trust (CAMEL_GPG_CONTEXT (ctx), TRUE);
	
	camel_test_start ("Test of PGP functions");
	
	stream1 = camel_stream_mem_new ();
	camel_stream_write (stream1, "Hello, I am a test stream.\n", 27);
	camel_stream_reset (stream1);

	conpart = camel_mime_part_new();
	dw = camel_data_wrapper_new();
	camel_data_wrapper_construct_from_stream(dw, stream1);
	camel_medium_set_content_object((CamelMedium *)conpart, dw);
	camel_object_unref(stream1);
	camel_object_unref(dw);

	sigpart = camel_mime_part_new();

	camel_test_push ("PGP signing");
	camel_cipher_sign (ctx, "no.user@no.domain", CAMEL_CIPHER_HASH_SHA1, conpart, sigpart, ex);
	if (camel_exception_is_set(ex)) {
		printf("PGP signing failed assuming non-functional environment\n%s", camel_exception_get_description (ex));
		camel_test_pull();
		return 77;
	}
	camel_test_pull ();
	
	camel_exception_clear (ex);
	
	camel_test_push ("PGP verify");
	valid = camel_cipher_verify (ctx, sigpart, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	check_msg (camel_cipher_validity_get_valid (valid), "%s", camel_cipher_validity_get_description (valid));
	camel_cipher_validity_free (valid);
	camel_test_pull ();
	
	camel_object_unref(conpart);
	camel_object_unref(sigpart);
	
	stream1 = camel_stream_mem_new ();
	camel_stream_write (stream1, "Hello, I am a test of encryption/decryption.", 44);
	camel_stream_reset (stream1);

	conpart = camel_mime_part_new();
	dw = camel_data_wrapper_new();
	camel_stream_reset(stream1);
	camel_data_wrapper_construct_from_stream(dw, stream1);
	camel_medium_set_content_object((CamelMedium *)conpart, dw);
	camel_object_unref(stream1);
	camel_object_unref(dw);

	encpart = camel_mime_part_new();
	
	camel_exception_clear (ex);
	
	camel_test_push ("PGP encrypt");
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, "no.user@no.domain");
	camel_cipher_encrypt (ctx, "no.user@no.domain", recipients, conpart, encpart, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	g_ptr_array_free (recipients, TRUE);
	camel_test_pull ();

	camel_exception_clear (ex);
	
	camel_test_push ("PGP decrypt");
	outpart = camel_mime_part_new();
	valid = camel_cipher_decrypt (ctx, encpart, outpart, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	check_msg (valid->encrypt.status == CAMEL_CIPHER_VALIDITY_ENCRYPT_ENCRYPTED, "%s", valid->encrypt.description);

	stream1 = camel_stream_mem_new();
	stream2 = camel_stream_mem_new();

	camel_data_wrapper_write_to_stream((CamelDataWrapper *)conpart, stream1);
	camel_data_wrapper_write_to_stream((CamelDataWrapper *)outpart, stream2);

	buf = CAMEL_STREAM_MEM (stream1)->buffer;
	before = g_strndup (buf->data, buf->len);
	buf = CAMEL_STREAM_MEM (stream2)->buffer;
	after = g_strndup (buf->data, buf->len);
	check_msg (string_equal (before, after), "before = '%s', after = '%s'", before, after);
	g_free (before);
	g_free (after);

	camel_object_unref(stream1);
	camel_object_unref(stream2);
	camel_object_unref(conpart);
	camel_object_unref(encpart);
	camel_object_unref(outpart);

	camel_test_pull ();
	
	camel_object_unref (CAMEL_OBJECT (ctx));
	camel_object_unref (CAMEL_OBJECT (session));
	
	camel_test_end ();
	
	return 0;
}
