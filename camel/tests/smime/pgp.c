#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <camel/camel-pgp-context.h>
#include <camel/camel-stream-mem.h>

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
			   gboolean secret, CamelService *service,
			   const char *item, CamelException *ex);

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
get_password (CamelSession *session, const char *prompt, gboolean secret,
	      CamelService *service, const char *item, CamelException *ex)
{
	return g_strdup ("PGP/MIME is rfc2015, now go and read it.");
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
	CamelPgpContext *ctx;
	CamelException *ex;
	CamelCipherValidity *valid;
	CamelStream *stream1, *stream2, *stream3;
	GPtrArray *recipients;
	GByteArray *buf;
	char *before, *after;
	
	camel_test_init (argc, argv);
	
	ex = camel_exception_new ();
	
	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");
	
	session = camel_pgp_session_new ("/tmp/camel-test");
	
	ctx = camel_pgp_context_new (session, CAMEL_PGP_TYPE_GPG, "/usr/bin/gpg");
	
	camel_test_start ("Test of PGP functions");
	
	stream1 = camel_stream_mem_new ();
	camel_stream_write (stream1, "Hello, I am a test stream.\n", 27);
	camel_stream_reset (stream1);
	
	stream2 = camel_stream_mem_new ();
	
	camel_test_push ("PGP signing");
	camel_pgp_sign (ctx, "pgp-mime@xtorshun.org", CAMEL_CIPHER_HASH_SHA1,
			stream1, stream2, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	camel_test_pull ();
	
	camel_exception_clear (ex);
	
	camel_test_push ("PGP verify");
	camel_stream_reset (stream1);
	camel_stream_reset (stream2);
	valid = camel_pgp_verify (ctx, stream1, stream2, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	check_msg (camel_cipher_validity_get_valid (valid), "%s", camel_cipher_validity_get_description (valid));
	camel_cipher_validity_free (valid);
	camel_test_pull ();
	
	camel_object_unref (CAMEL_OBJECT (stream1));
	camel_object_unref (CAMEL_OBJECT (stream2));
	
	stream1 = camel_stream_mem_new ();
	stream2 = camel_stream_mem_new ();
	stream3 = camel_stream_mem_new ();
	
	camel_stream_write (stream1, "Hello, I am a test of encryption/decryption.", 44);
	camel_stream_reset (stream1);
	
	camel_exception_clear (ex);
	
	camel_test_push ("PGP encrypt");
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, "pgp-mime@xtorshun.org");
	camel_pgp_encrypt (ctx, FALSE, "pgp-mime@xtorshun.org", recipients,
			   stream1, stream2, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	g_ptr_array_free (recipients, TRUE);
	camel_test_pull ();
	
	camel_stream_reset (stream2);
	camel_exception_clear (ex);
	
	camel_test_push ("PGP decrypt");
	camel_pgp_decrypt (ctx, stream2, stream3, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	buf = CAMEL_STREAM_MEM (stream1)->buffer;
	before = g_strndup (buf->data, buf->len);
	buf = CAMEL_STREAM_MEM (stream3)->buffer;
	after = g_strndup (buf->data, buf->len);
	check_msg (string_equal (before, after), "before = '%s', after = '%s'", before, after);
	g_free (before);
	g_free (after);
	camel_test_pull ();
	
	camel_object_unref (CAMEL_OBJECT (ctx));
	camel_object_unref (CAMEL_OBJECT (session));
	
	camel_test_end ();
	
	return 0;
}
