#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <camel/camel-pgp-mime.h>
#include <camel/camel-stream-mem.h>

#include "camel-test.h"

static char test_msg[] = "Since we need to make sure that\nFrom lines work okay, we should test that"
"as well as test 8bit chars and other fun stuff? 8bit chars: Dra¾en Kaèar\n\nOkay, I guess that covers"
"the basics at least...\n";

/* god, who designed this horrid interface */
static gpointer auth_callback (CamelAuthCallbackMode mode,
			       char *data, gboolean secret,
			       CamelService *service, char *item,
			       CamelException *ex)
{
	return g_strdup ("PGP/MIME is rfc2015, now go and read it.");
}

int main (int argc, char **argv)
{
	CamelSession *session;
	CamelPgpContext *ctx;
	CamelException *ex;
	CamelPgpValidity *valid;
	CamelMimePart *mime_part, *part;
	GPtrArray *recipients;
	
	camel_test_init (argc, argv);
	
	ex = camel_exception_new ();
	
	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");
	
	session = camel_session_new ("/tmp/camel-test",
				     auth_callback, NULL, NULL);
	
	ctx = camel_pgp_context_new (session, CAMEL_PGP_TYPE_GPG, "/usr/bin/gpg");
	
	camel_test_start ("Test of PGP/MIME functions");
	
	mime_part = camel_mime_part_new ();
	camel_mime_part_set_content (mime_part, test_msg, strlen (test_msg), "text/plain");
	camel_mime_part_set_description (mime_part, "Test of PGP/MIME multipart/signed stuff");
	
	camel_test_push ("PGP/MIME signing");
	camel_pgp_mime_part_sign (ctx, &mime_part, "pgp-mime@xtorshun.org", CAMEL_PGP_HASH_TYPE_SHA1, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	check_msg (camel_pgp_mime_is_rfc2015_signed (mime_part),
		   "Huh, the MIME part does not seem to be a valid multipart/signed part");
	camel_test_pull ();
	
	camel_exception_clear (ex);
	
	camel_test_push ("PGP/MIME verify");
	valid = camel_pgp_mime_part_verify (ctx, mime_part, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	check_msg (camel_pgp_validity_get_valid (valid), "%s", camel_pgp_validity_get_description (valid));
	camel_pgp_validity_free (valid);
	camel_test_pull ();
	
	camel_object_unref (CAMEL_OBJECT (mime_part));
	
	camel_exception_clear (ex);
	
	mime_part = camel_mime_part_new ();
	camel_mime_part_set_content (mime_part, test_msg, strlen (test_msg), "text/plain");
	camel_mime_part_set_description (mime_part, "Test of PGP/MIME multipart/encrypted stuff");
	
	camel_test_push ("PGP/MIME encrypt");
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, "pgp-mime@xtorshun.org");
	camel_pgp_mime_part_encrypt (ctx, &mime_part, recipients, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	check_msg (camel_pgp_mime_is_rfc2015_encrypted (mime_part),
		   "Huh, the MIME part does not seem to be a valid multipart/encrypted part");
	g_ptr_array_free (recipients, TRUE);
	camel_test_pull ();
	
	camel_exception_clear (ex);
	
	camel_test_push ("PGP/MIME decrypt");
	part = camel_pgp_mime_part_decrypt (ctx, mime_part, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	camel_object_unref (CAMEL_OBJECT (part));
	camel_test_pull ();
	
	camel_object_unref (CAMEL_OBJECT (mime_part));
	
	camel_object_unref (CAMEL_OBJECT (ctx));
	camel_object_unref (CAMEL_OBJECT (session));
	
	camel_test_end ();
	
	return 0;
}
