/* threaded folder testing */

#include <string.h>
#include <pthread.h>

#include "camel-test.h"
#include "camel-test-provider.h"
#include "folders.h"
#include "messages.h"
#include "session.h"

#include <camel/camel-exception.h>
#include <camel/camel-service.h>
#include <camel/camel-store.h>

#define MAX_LOOP (10000)
#define MAX_THREADS (5)

#define d(x) 

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

static const char *local_drivers[] = { "local" };
static char *local_providers[] = {
	"mbox",
	"mh",
	"maildir"
};

static char *path;
static CamelSession *session;
static int testid;

static void *
worker(void *d)
{
	int i;
	CamelException *ex = camel_exception_new();
	CamelStore *store;
	CamelFolder *folder;

	for (i=0;i<MAX_LOOP;i++) {
		store = camel_session_get_store(session, path, ex);
		camel_exception_clear(ex);
		folder = camel_store_get_folder(store, "testbox", CAMEL_STORE_FOLDER_CREATE, ex);
		camel_exception_clear(ex);
		if (testid == 0) {
			camel_object_unref(folder);
			camel_object_unref(store);
		} else {
			camel_object_unref(store);
			camel_object_unref(folder);
		}
	}

	camel_exception_free(ex);

	return NULL;
}

int main(int argc, char **argv)
{
	CamelException *ex;
	int i, j;
	pthread_t threads[MAX_THREADS];

	camel_test_init(argc, argv);
	camel_test_provider_init(1, local_drivers);

	ex = camel_exception_new();

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	session = camel_test_session_new ("/tmp/camel-test");

	for (testid=0;testid<2;testid++) {
		if (testid == 0)
			camel_test_start("store and folder bag torture test, stacked references");
		else
			camel_test_start("store and folder bag torture test, unstacked references");

		for (j=0;j<ARRAY_LEN(local_providers);j++) {

			camel_test_push("provider %s", local_providers[j]);
			path = g_strdup_printf("%s:///tmp/camel-test/%s", local_providers[j], local_providers[j]);
		
			for (i=0;i<MAX_THREADS;i++)
				pthread_create(&threads[i], 0, worker, NULL);
			
			for (i=0;i<MAX_THREADS;i++) 
				pthread_join(threads[i], NULL);
			
			test_free(path);

			camel_test_pull();
		}

		camel_test_end();
	}

	camel_object_unref((CamelObject *)session);
	camel_exception_free(ex);

	return 0;
}
