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

#define MAX_MESSAGES (100)
#define MAX_THREADS (10)

#define d(x) 

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

static const char *local_drivers[] = { "local" };

static char *local_providers[] = {
	"mbox",
	"mh",
	"maildir"
};

static void
test_add_message(CamelFolder *folder, int j)
{
	CamelMimeMessage *msg;
	char *content;
	char *subject;
	CamelException ex;

	camel_exception_init(&ex);

	push("creating message %d\n", j);
	msg = test_message_create_simple();
	content = g_strdup_printf("Test message %08x contents\n\n", j);
	test_message_set_content_simple((CamelMimePart *)msg, 0, "text/plain",
							content, strlen(content));
	test_free(content);
	subject = g_strdup_printf("Test message %08x subject", j);
	camel_mime_message_set_subject(msg, subject);
	pull();

	push("appending simple message %d", j);
	camel_folder_append_message(folder, msg, NULL, NULL, &ex);
	check_msg(!camel_exception_is_set(&ex), "%s", camel_exception_get_description(&ex));
	pull();

	check_unref(msg, 1);
}

struct _threadinfo {
	int id;
	CamelFolder *folder;
};

static void *
worker(void *d)
{
	struct _threadinfo *info = d;
	int i, j, id = info->id;
	char *sub, *content;
	GPtrArray *res;
	CamelException *ex = camel_exception_new();
	CamelMimeMessage *msg;

	/* we add a message, search for it, twiddle some flags, delete it */
	/* and flat out */
	for (i=0;i<MAX_MESSAGES;i++) {
		d(printf("Thread %ld message %i\n", pthread_self(), i));
		test_add_message(info->folder, id+i);

		sub = g_strdup_printf("(match-all (header-contains \"subject\" \"message %08x subject\"))", id+i);

		push("searching for message %d\n\tusing: %s", id+i, sub);
		res = camel_folder_search_by_expression(info->folder, sub, ex);
		check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
		check_msg(res->len == 1, "res->len = %d", res->len);
		pull();

		push("getting message '%s'", res->pdata[0]);
		msg = camel_folder_get_message(info->folder, (char *)res->pdata[0], ex);
		check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
		pull();

		content = g_strdup_printf("Test message %08x contents\n\n", id+i);
		push("comparing content '%s': '%s'", res->pdata[0], content);
		test_message_compare_content(camel_medium_get_content_object((CamelMedium *)msg), content, strlen(content));
		test_free(content);
		pull();

		push("deleting message, cleanup");
		j=(100.0*rand()/(RAND_MAX+1.0));
		if (j<=70) {
			camel_folder_delete_message(info->folder, res->pdata[0]);
		}

		camel_folder_search_free(info->folder, res);
		res = NULL;
		test_free(sub);

		check_unref(msg, 1);
		pull();

		/* about 1-in 100 calls will expunge */
		j=(200.0*rand()/(RAND_MAX+1.0));
		if (j<=2) {
			d(printf("Forcing an expuge\n"));
			push("expunging folder");
			camel_folder_expunge(info->folder, ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			pull();
		}
	}

	camel_exception_free(ex);

	return info;
}

int main(int argc, char **argv)
{
	CamelSession *session;
	CamelException *ex;
	int i, j, index;
	char *path;
	CamelStore *store;
	pthread_t threads[MAX_THREADS];
	struct _threadinfo *info;
	CamelFolder *folder;
	GPtrArray *uids;

	camel_test_init(argc, argv);
	camel_test_provider_init(1, local_drivers);

	ex = camel_exception_new();

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	session = camel_test_session_new ("/tmp/camel-test");

	for (j=0;j<ARRAY_LEN(local_providers);j++) {
		for (index=0;index<2;index++) {
			path = g_strdup_printf("method %s %s", local_providers[j], index?"indexed":"nonindexed");
			camel_test_start(path);
			test_free(path);

			push("trying %s index %d", local_providers[j], index);
			path = g_strdup_printf("%s:///tmp/camel-test/%s", local_providers[j], local_providers[j]);
			store = camel_session_get_store(session, path, ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			test_free(path);
			
			if (index == 0)
				folder = camel_store_get_folder(store, "testbox", CAMEL_STORE_FOLDER_CREATE, ex);
			else
				folder = camel_store_get_folder(store, "testbox",
								CAMEL_STORE_FOLDER_CREATE|CAMEL_STORE_FOLDER_BODY_INDEX, ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));

			for (i=0;i<MAX_THREADS;i++) {
				info = g_malloc(sizeof(*info));
				info->id = i*MAX_MESSAGES;
				info->folder = folder;
				pthread_create(&threads[i], 0, worker, info);
			}

			for (i=0;i<MAX_THREADS;i++) {
				pthread_join(threads[i], (void **)&info);
				g_free(info);
			}
			pull();

			push("deleting remaining messages");
			uids = camel_folder_get_uids(folder);
			for (i=0;i<uids->len;i++) {
				camel_folder_delete_message(folder, uids->pdata[i]);
			}
			camel_folder_free_uids(folder, uids);

			camel_folder_expunge(folder, ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));

			check_unref(folder, 1);

			camel_store_delete_folder(store, "testbox", ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));

			check_unref(store, 1);

			pull();

			camel_test_end();
		}
	}

	camel_object_unref((CamelObject *)session);
	camel_exception_free(ex);

	return 0;
}
