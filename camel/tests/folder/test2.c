/* folder testing */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "camel-test.h"
#include "camel-test-provider.h"
#include "messages.h"
#include "folders.h"
#include "session.h"

#include <camel/camel-exception.h>
#include <camel/camel-service.h>
#include <camel/camel-store.h>

#include <camel/camel-folder.h>
#include <camel/camel-folder-summary.h>
#include <camel/camel-mime-message.h>

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

static const char *local_drivers[] = { "local" };

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
	camel_test_provider_init(1, local_drivers);

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	ex = camel_exception_new();

	session = camel_test_session_new ("/tmp/camel-test");

	/* we iterate over all stores we want to test, with indexing or indexing turned on or off */
	for (i=0;i<ARRAY_LEN(stores);i++) {
		char *name = stores[i];

		test_folder_message_ops(session, name, TRUE, "testbox");
	}

	/* create a pseudo-spool file, and check that */
	creat("/tmp/camel-test/testbox", 0600);
	test_folder_message_ops(session, "spool:///tmp/camel-test/testbox", TRUE, "INBOX");

	check_unref(session, 1);
	camel_exception_free(ex);

	return 0;
}
