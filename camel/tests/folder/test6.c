/* folder testing */

#include "camel-test.h"
#include "camel-test-provider.h"
#include "folders.h"
#include "session.h"

#include <camel/camel-exception.h>
#include <camel/camel-service.h>
#include <camel/camel-store.h>

#include <camel/camel-folder.h>
#include <camel/camel-folder-summary.h>
#include <camel/camel-mime-message.h>

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

static const char *imap_drivers[] = { "imap" };
static char *remote_providers[] = {
	"IMAP_TEST_URL",
};

int main(int argc, char **argv)
{
	CamelSession *session;
	CamelException *ex;
	int i;
	char *path;

	camel_test_init(argc, argv);
	camel_test_provider_init(1, imap_drivers);

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
		/*camel_test_nonfatal("The IMAP code is just rooted");*/
		test_folder_message_ops(session, path, FALSE, "testbox");
		test_folder_message_ops(session, path, FALSE, "INBOX");
		/*camel_test_fatal();*/
	}

	check_unref(session, 1);
	camel_exception_free(ex);

	return 0;
}
