/* folder testing */

#include "camel-test.h"
#include "messages.h"
#include "folders.h"

#include <camel/camel-exception.h>
#include <camel/camel-service.h>
#include <camel/camel-session.h>
#include <camel/camel-store.h>

#include <camel/camel-folder.h>
#include <camel/camel-folder-summary.h>
#include <camel/camel-mime-message.h>

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

static char *stores[] = {
	"mbox:///tmp/camel-test/mbox",
	"mh:///tmp/camel-test/mh",
	"maildir:///tmp/camel-test/maildir"
};

int main(int argc, char **argv)
{
	CamelSession *session;
	CamelException *ex;
	int i;

	camel_test_init(argc, argv);

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	ex = camel_exception_new();

	session = camel_test_session_new ("/tmp/camel-test");

	/* we iterate over all stores we want to test, with indexing or indexing turned on or off */
	for (i=0;i<ARRAY_LEN(stores);i++) {
		char *name = stores[i];

		test_folder_message_ops(session, name, TRUE);
	}

	check_unref(session, 1);
	camel_exception_free(ex);

	return 0;
}
