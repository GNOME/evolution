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
#include <camel/camel-multipart-signed.h>
#include <camel/camel-multipart-encrypted.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-stream-mem.h>

#include "camel-test.h"
#include "session.h"

static char test_msg[] = "Since we need to make sure that\nFrom lines work okay, we should test that"
"as well as test 8bit chars and other fun stuff? 8bit chars: Dra¾en Kaèar\n\nOkay, I guess that covers"
"the basics at least...\n";


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
			   gboolean reprompt, gboolean secret,
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
get_password (CamelSession *session, const char *prompt, gboolean reprompt, gboolean secret,
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
	CamelMimePart *mime_part;
	CamelMultipartSigned *mps;
	CamelMultipartEncrypted *mpe;
	GPtrArray *recipients;
	
	camel_test_init (argc, argv);
	
	/* clear out any camel-test data */
	system ("/bin/rm -rf /tmp/camel-test");
	system ("/bin/mkdir /tmp/camel-test");
	setenv ("GNUPGHOME", "/tmp/camel-test/.gnupg", 1);
	
	/* import the gpg keys */
	if ((ret = system ("gpg > /dev/null 2>&1")) == -1)
		return 77;
	else if (WEXITSTATUS (ret) == 127)
		return 127;
	
	system ("gpg --import camel-test.gpg.pub > /dev/null 2>&1");
	system ("gpg --import camel-test.gpg.sec > /dev/null 2>&1");
	
	session = camel_pgp_session_new ("/tmp/camel-test");
	
	ex = camel_exception_new ();
	
	ctx = camel_gpg_context_new (session);
	camel_gpg_context_set_always_trust (CAMEL_GPG_CONTEXT (ctx), TRUE);
	
	camel_test_start ("Test of PGP/MIME functions");
	
	mime_part = camel_mime_part_new ();
	camel_mime_part_set_content (mime_part, test_msg, strlen (test_msg), "text/plain");
	camel_mime_part_set_description (mime_part, "Test of PGP/MIME multipart/signed stuff");
	
	camel_test_push ("PGP/MIME signing");
	mps = camel_multipart_signed_new ();
	camel_multipart_signed_sign (mps, ctx, mime_part, "no.user@no.domain", CAMEL_CIPHER_HASH_SHA1, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	camel_test_pull ();
	
	camel_object_unref (mime_part);
	camel_exception_clear (ex);
	
	camel_test_push ("PGP/MIME verify");
	valid = camel_multipart_signed_verify (mps, ctx, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	check_msg (camel_cipher_validity_get_valid (valid), "%s", camel_cipher_validity_get_description (valid));
	camel_cipher_validity_free (valid);
	camel_test_pull ();
	
	camel_object_unref (mps);
	camel_exception_clear (ex);
	
	mime_part = camel_mime_part_new ();
	camel_mime_part_set_content (mime_part, test_msg, strlen (test_msg), "text/plain");
	camel_mime_part_set_description (mime_part, "Test of PGP/MIME multipart/encrypted stuff");
	
	camel_test_push ("PGP/MIME encrypt");
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, "no.user@no.domain");
	
	mpe = camel_multipart_encrypted_new ();
	camel_multipart_encrypted_encrypt (mpe, mime_part, ctx, "no.user@no.domain", recipients, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	g_ptr_array_free (recipients, TRUE);
	camel_test_pull ();
	
	camel_exception_clear (ex);
	camel_object_unref (mime_part);
	
	camel_test_push ("PGP/MIME decrypt");
	mime_part = camel_multipart_encrypted_decrypt (mpe, ctx, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	camel_object_unref (mime_part);
	camel_object_unref (mpe);
	camel_test_pull ();
	
	camel_object_unref (CAMEL_OBJECT (ctx));
	camel_object_unref (CAMEL_OBJECT (session));
	
	camel_test_end ();
	
	return 0;
}
