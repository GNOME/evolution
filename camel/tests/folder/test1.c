/* store testing */

#include "camel-test.h"

#include <camel/camel-exception.h>
#include <camel/camel-service.h>
#include <camel/camel-session.h>
#include <camel/camel-store.h>

/* god, who designed this horrid interface */
static char *auth_callback(CamelAuthCallbackMode mode,
			   char *data, gboolean secret,
			   CamelService *service, char *item,
			   CamelException *ex)
{
	return NULL;
}

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

static char *local_providers[] = {
	"mbox",
	"mh",
	"maildir"
};

int main(int argc, char **argv)
{
	CamelSession *session;
	CamelStore *store;
	CamelException *ex;
	CamelFolder *folder, *root;
	int i;
	char *path;

	camel_test_init(argc, argv);

	ex = camel_exception_new();

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	session = camel_session_new("/tmp/camel-test", auth_callback, NULL, NULL);

	/* todo: cross-check everything with folder_info checks as well */
	/* todo: subscriptions? */
	/* todo: work out how to do imap/pop/nntp tests */
	for (i=0;i<ARRAY_LEN(local_providers);i++) {
		char *what = g_strdup_printf("testing local store: %s", local_providers[i]);

		camel_test_start(what);
		g_free(what);

		push("getting store");
		path = g_strdup_printf("%s:///tmp/camel-test/%s", local_providers[i], local_providers[i]);
		store = camel_session_get_store(session, path, ex);
		check_msg(!camel_exception_is_set(ex), "getting store: %s", camel_exception_get_description(ex));
		check(store != NULL);
		pull();

		/* local providers == no root folder */
		push("getting root folder");
		root = camel_store_get_root_folder(store, ex);
		check(camel_exception_is_set(ex));
		check(root == NULL);
		camel_exception_clear(ex);
		pull();

		/* same for default folder */
		push("getting default folder");
		root = camel_store_get_root_folder(store, ex);
		check(camel_exception_is_set(ex));
		check(root == NULL);
		camel_exception_clear(ex);
		pull();

		push("getting a non-existant folder, no create");
		folder = camel_store_get_folder(store, "unknown", 0, ex);
		check(camel_exception_is_set(ex));
		check(folder == NULL);
		camel_exception_clear(ex);
		pull();

		push("getting a non-existant folder, with create");
		folder = camel_store_get_folder(store, "testbox", CAMEL_STORE_FOLDER_CREATE, ex);
		check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
		check(folder != NULL);
		camel_object_unref((CamelObject *)folder);
		pull();

		push("getting an existing folder");
		folder = camel_store_get_folder(store, "testbox", 0, ex);
		check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
		check(folder != NULL);
		camel_object_unref((CamelObject *)folder);
		pull();

		push("renaming a non-existant folder");
		camel_store_rename_folder(store, "unknown1", "unknown2", ex);
		check(camel_exception_is_set(ex));
		camel_exception_clear(ex);
		pull();

		push("renaming an existing folder");
		camel_store_rename_folder(store, "testbox", "testbox2", ex);
		check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
		pull();

		push("opening the old name of a renamed folder");
		folder = camel_store_get_folder(store, "testbox", 0, ex);
		check(camel_exception_is_set(ex));
		check(folder == NULL);
		camel_exception_clear(ex);
		pull();

		push("opening the new name of a renamed folder");
		folder = camel_store_get_folder(store, "testbox2", 0, ex);
		check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
		check(folder != NULL);
		camel_object_unref((CamelObject *)folder);
		pull();

		push("deleting a non-existant folder");
		camel_store_delete_folder(store, "unknown", ex);
		check(camel_exception_is_set(ex));
		camel_exception_clear(ex);
		pull();

		push("deleting an existing folder");
		camel_store_delete_folder(store, "testbox2", ex);
		check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
		pull();

		push("opening a folder that has been deleted");
		folder = camel_store_get_folder(store, "testbox2", 0, ex);
		check(camel_exception_is_set(ex));
		check(folder == NULL);
		camel_exception_clear(ex);
		pull();

		camel_object_unref((CamelObject *)store);

		g_free(path);
		
		camel_test_end();
	}

	camel_object_unref((CamelObject *)session);
	camel_exception_free(ex);

	return 0;
}
