/* folder testing */

#include "camel-test.h"
#include "messages.h"

#include <camel/camel-exception.h>
#include <camel/camel-service.h>
#include <camel/camel-session.h>
#include <camel/camel-store.h>

#include <camel/camel-folder.h>
#include <camel/camel-folder-summary.h>
#include <camel/camel-mime-message.h>

static int regtimeout()
{
	return 1;
}

static int unregtimeout()
{
	return 1;
}

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
	camel_session_class->register_timeout = regtimeout;
	camel_session_class->remove_timeout = unregtimeout;
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

static CamelSession *
camel_test_session_new (const char *path)
{
	CamelSession *session;
	
	session = CAMEL_SESSION (camel_object_new (CAMEL_TEST_SESSION_TYPE));
	
	camel_session_construct (session, path);
	
	return session;
}

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

static char *remote_providers[] = {
	"NNTP_TEST_URL",
};

int main(int argc, char **argv)
{
	CamelSession *session;
	CamelException *ex;
	int i;
	char *path;

	camel_test_init(argc, argv);

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	ex = camel_exception_new();

	session = camel_test_session_new ("/tmp/camel-test");

	for (i=0;i<ARRAY_LEN(remote_providers);i++) {
		path = getenv(remote_providers[i]);

		if (path == NULL) {
			printf("Aborted (ignored).\n");
			printf("Set '%s', to re-run test.\n", remote_providers[i]);
			/* tells make check to ignore us in the total count */
			_exit(77);
		}
		camel_test_nonfatal("Dont know how many tests apply to NNTP");
		test_folder_message_ops(session, path, FALSE);
		camel_test_fatal();
	}

	check_unref(session, 1);
	camel_exception_free(ex);

	return 0;
}
