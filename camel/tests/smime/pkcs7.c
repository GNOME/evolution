#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <camel/camel-smime-context.h>
#include <camel/camel-stream-mem.h>

#include "camel-test.h"


#define CAMEL_TEST_SESSION_TYPE     (camel_test_session_get_type ())
#define CAMEL_TEST_SESSION(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_TEST_SESSION_TYPE, CamelTestSession))
#define CAMEL_TEST_SESSION_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_TEST_SESSION_TYPE, CamelTestSessionClass))
#define CAMEL_TEST_IS_SESSION(o)    (CAMEL_CHECK_TYPE((o), CAMEL_TEST_SESSION_TYPE))


typedef struct _CamelTestSession {
	CamelSession parent_object;
	
} CamelTestSession;

typedef struct _CamelTestSessionClass {
	CamelSessionClass parent_class;
	
} CamelTestSessionClass;


static char *get_password (CamelSession *session, const char *prompt,
			   guint32 flags, CamelService *service,
			   const char *item, CamelException *ex);

static void
init (CamelTestSession *session)
{
	;
}

static void
class_init (CamelTestSessionClass *camel_test_session_class)
{
	CamelSessionClass *camel_session_class =
		CAMEL_SESSION_CLASS (camel_test_session_class);
	
	/* virtual method override */
	camel_session_class->get_password = get_password;
}

static CamelType
camel_test_session_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (
			camel_test_session_get_type (),
			"CamelTestSession",
			sizeof (CamelTestSession),
			sizeof (CamelTestSessionClass),
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
	return g_strdup ("S/MIME v3 is rfc263x, now go and read them.");
}

static CamelSession *
camel_test_session_new (const char *path)
{
	CamelSession *session;
	
	session = CAMEL_SESSION (camel_object_new (CAMEL_TEST_SESSION_TYPE));
	
	camel_session_construct (session, path);
	
	return session;
}


int main (int argc, char **argv)
{
	CamelSession *session;
	CamelSMimeContext *ctx;
	CamelException *ex;
	CamelCipherValidity *valid;
	CamelStream *stream1, *stream2, *stream3;
	GPtrArray *recipients;
	GByteArray *buf;
	char *before, *after;
	
	camel_test_init (argc, argv);
	
	ex = camel_exception_new ();
	
	/* clear out any camel-test data */
	system ("/bin/rm -rf /tmp/camel-test");
	
	session = camel_test_session_new ("/tmp/camel-test");
	
	ctx = camel_smime_context_new (session);
	
	camel_test_start ("Test of S/MIME PKCS7 functions");
	
	stream1 = camel_stream_mem_new ();
	camel_stream_write (stream1, "Hello, I am a test stream.", 25);
	camel_stream_reset (stream1);
	
	stream2 = camel_stream_mem_new ();
	
	camel_test_push ("PKCS7 signing");
	camel_smime_sign (ctx, "smime@xtorshun.org", CAMEL_CIPHER_HASH_SHA1,
			  stream1, stream2, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	camel_test_pull ();
	
	camel_exception_clear (ex);
	
	camel_test_push ("PKCS7 verify");
	camel_stream_reset (stream1);
	camel_stream_reset (stream2);
	valid = camel_smime_verify (ctx, CAMEL_CIPHER_HASH_SHA1, stream1, stream2, ex);
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
	
	camel_test_push ("PKCS7 encrypt");
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, "smime@xtorshun.org");
	camel_smime_encrypt (ctx, FALSE, "smime@xtorshun.org", recipients,
			     stream1, stream2, ex);
	check_msg (!camel_exception_is_set (ex), "%s", camel_exception_get_description (ex));
	g_ptr_array_free (recipients, TRUE);
	camel_test_pull ();
	
	camel_stream_reset (stream2);
	camel_exception_clear (ex);
	
	camel_test_push ("PKCS7 decrypt");
	camel_smime_decrypt (ctx, stream2, stream3, ex);
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
