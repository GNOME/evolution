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

/* god, who designed this horrid interface */
static char *auth_callback(CamelAuthCallbackMode mode,
			   char *data, gboolean secret,
			   CamelService *service, char *item,
			   CamelException *ex)
{
	return NULL;
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
	CamelStore *store;
	CamelException *ex;
	CamelFolder *folder;
	CamelMimeMessage *msg;
	int i, j;
	int indexed;
	GPtrArray *uids;
	const CamelMessageInfo *info;

	camel_test_init(argc, argv);

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	ex = camel_exception_new();

	session = camel_session_new("/tmp/camel-test", auth_callback, NULL, NULL);

	/* todo: cross-check everything with folder_info checks as well */
	/* todo: work out how to do imap/pop/nntp tests */

	/* we iterate over all stores we want to test, with indexing or indexing turned on or off */
	for (i=0;i<ARRAY_LEN(stores);i++) {
		char *name = stores[i];
		for (indexed = 0;indexed<2;indexed++) {
			char *what = g_strdup_printf("folder ops: %s (%sindexed)", name, indexed?"":"non-");
			int flags;

			camel_test_start(what);
			test_free(what);

			push("getting store");
			store = camel_session_get_store(session, stores[i], ex);
			check_msg(!camel_exception_is_set(ex), "getting store: %s", camel_exception_get_description(ex));
			check(store != NULL);
			pull();

			push("creating %sindexed folder", indexed?"":"non-");
			if (indexed)
				flags = CAMEL_STORE_FOLDER_CREATE|CAMEL_STORE_FOLDER_BODY_INDEX;
			else
				flags = CAMEL_STORE_FOLDER_CREATE;
			folder = camel_store_get_folder(store, "testbox", flags, ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			check(folder != NULL);

			/* verify empty/can't get nonexistant stuff */
			test_folder_counts(folder, 0, 0);
			test_folder_not_message(folder, "0");
			test_folder_not_message(folder, "");

			for (j=0;j<10;j++) {
				char *content, *subject;

				push("creating test message");
				msg = test_message_create_simple();
				content = g_strdup_printf("Test message %d contents\n\n", j);
				test_message_set_content_simple((CamelMimePart *)msg, 0, "text/plain",
								content, strlen(content));
				test_free(content);
				subject = g_strdup_printf("Test message %d", j);
				camel_mime_message_set_subject(msg, subject);
				pull();

				push("appending simple message %d", j);
				camel_folder_append_message(folder, msg, NULL, ex);
				check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
				test_folder_counts(folder, j+1, j+1);

				push("checking it is in the right uid slot & exists");
				uids = camel_folder_get_uids(folder);
				check(uids != NULL);
				check(uids->len == j+1);
				test_folder_message(folder, uids->pdata[j]);
				pull();

				push("checking it is the right message (subject): %s", subject);
				info = camel_folder_get_message_info(folder, uids->pdata[j]);
				check_msg(strcmp(camel_message_info_subject(info), subject)==0,
					  "info->subject %s", camel_message_info_subject(info));
				camel_folder_free_uids(folder, uids);
				pull();

				test_free(subject);

				check_unref(msg, 1);
				pull();
			}

			check_unref(folder, 1);
			pull();

			push("deleting test folder, with messages in it");
			camel_store_delete_folder(store, "testbox", ex);
			check(camel_exception_is_set(ex));
			camel_exception_clear(ex);
			pull();

			push("re-opening folder");
			folder = camel_store_get_folder(store, "testbox", flags, ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			check(folder != NULL);

			/* verify counts */
			test_folder_counts(folder, 10, 10);

			/* re-check uid's, after a reload */
			uids = camel_folder_get_uids(folder);
			check(uids != NULL);
			check(uids->len == 10);
			for (j=0;j<10;j++) {
				char *subject = g_strdup_printf("Test message %d", j);

				push("verify reload of %s", subject);
				test_folder_message(folder, uids->pdata[j]);

				info = camel_folder_get_message_info(folder, uids->pdata[j]);
				check_msg(strcmp(camel_message_info_subject(info), subject)==0,
					  "info->subject %s", camel_message_info_subject(info));
				test_free(subject);
				pull();
			}

			push("deleting first message & expunging");
			camel_folder_delete_message(folder, uids->pdata[0]);
			test_folder_counts(folder, 10, 10);
			camel_folder_expunge(folder, ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			test_folder_not_message(folder, uids->pdata[0]);
			test_folder_counts(folder, 9, 9);

			camel_folder_free_uids(folder, uids);

			uids = camel_folder_get_uids(folder);
			check(uids != NULL);
			check(uids->len == 9);
			for (j=0;j<9;j++) {
				char *subject = g_strdup_printf("Test message %d", j+1);

				push("verify after expunge of %s", subject);
				test_folder_message(folder, uids->pdata[j]);

				info = camel_folder_get_message_info(folder, uids->pdata[j]);
				check_msg(strcmp(camel_message_info_subject(info), subject)==0,
					  "info->subject %s", camel_message_info_subject(info));
				test_free(subject);
				pull();
			}
			pull();

			push("deleting last message & expunging");
			camel_folder_delete_message(folder, uids->pdata[8]);
			/* sync? */
			test_folder_counts(folder, 9, 9);
			camel_folder_expunge(folder, ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			test_folder_not_message(folder, uids->pdata[8]);
			test_folder_counts(folder, 8, 8);

			camel_folder_free_uids(folder, uids);

			uids = camel_folder_get_uids(folder);
			check(uids != NULL);
			check(uids->len == 8);
			for (j=0;j<8;j++) {
				char *subject = g_strdup_printf("Test message %d", j+1);

				push("verify after expunge of %s", subject);
				test_folder_message(folder, uids->pdata[j]);

				info = camel_folder_get_message_info(folder, uids->pdata[j]);
				check_msg(strcmp(camel_message_info_subject(info), subject)==0,
					  "info->subject %s", camel_message_info_subject(info));
				test_free(subject);
				pull();
			}
			pull();

			push("deleting all messages & expunging");
			for (j=0;j<8;j++) {
				camel_folder_delete_message(folder, uids->pdata[j]);
			}
			/* sync? */
			test_folder_counts(folder, 8, 8);
			camel_folder_expunge(folder, ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			for (j=0;j<8;j++) {
				test_folder_not_message(folder, uids->pdata[j]);
			}
			test_folder_counts(folder, 0, 0);

			camel_folder_free_uids(folder, uids);
			pull();

			check_unref(folder, 1);
			pull(); /* re-opening folder */

			push("deleting test folder, with no messages in it");
			camel_store_delete_folder(store, "testbox", ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			pull();

			check_unref(store, 1);
			camel_test_end();
		}
	}

	check_unref(session, 1);
	camel_exception_free(ex);

	return 0;
}
