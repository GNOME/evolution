/* folder/index testing */

#include "camel-test.h"
#include "messages.h"
#include "folders.h"

#include "camel/camel-exception.h"
#include "camel/camel-service.h"
#include "camel/camel-session.h"
#include "camel/camel-store.h"

#include "camel/camel-folder.h"
#include "camel/camel-folder-summary.h"
#include "camel/camel-mime-message.h"
#include "camel/camel-filter-driver.h"
#include "camel/camel-stream-fs.h"

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))


/* god, who designed this horrid interface */
static char *auth_callback(CamelAuthCallbackMode mode,
			   char *data, gboolean secret,
			   CamelService *service, char *item,
			   CamelException *ex)
{
	return NULL;
}

struct {
	char *name;
	CamelFolder *folder;
} mailboxes[] = {
	{ "INBOX", NULL },
	{ "folder1", NULL },
	{ "folder2", NULL },
	{ "folder3", NULL },
	{ "folder4", NULL },
};

struct {
	char *name, *match, *action;
} rules[] = {
	{ "empty1", "(match-all (header-contains \"Frobnitz\"))", "(copy-to \"folder1\")" },
	{ "empty2", "(header-contains \"Frobnitz\")", "(copy-to \"folder2\")" },
	{ "count11", "(and (header-contains \"subject\" \"Test1\") (header-contains \"subject\" \"subject\"))", "(move-to \"folder3\")" },
	{ "empty3", "(and (header-contains \"subject\" \"Test1\") (header-contains \"subject\" \"subject\"))", "(move-to \"folder4\")" },
	{ "count1", "(body-contains \"data50\")", "(copy-to \"folder1\")" },
	{ "stop", "(body-contains \"data2\")", "(stop)" },
	{ "notreached1", "(body-contains \"data2\")", "(move-to \"folder2\")" },
	{ "count1", "(body-contains \"data3\")", "(move-to \"folder2\")" },
};


static CamelFolder *get_folder(CamelFilterDriver *d, const char *uri, void *data, CamelException *ex)
{
	int i;

	for (i=0;i<ARRAY_LEN(mailboxes);i++)
		if (!strcmp(mailboxes[i].name, uri)) {
			camel_object_ref((CamelObject *)mailboxes[i].folder);
			return mailboxes[i].folder;
		}
	return NULL;
}

int main(int argc, char **argv)
{
	CamelSession *session;
	CamelStore *store;
	CamelException *ex;
	CamelFolder *folder;
	CamelMimeMessage *msg;
	int i, j;
	CamelStream *mbox;
	CamelFilterDriver *driver;

	/*gtk_init(&argc, &argv);*/

	camel_test_init(argc, argv);

	ex = camel_exception_new();

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	camel_test_start("Simple filtering of mbox");

	session = camel_session_new("/tmp/camel-test", auth_callback, NULL, NULL);

	/* todo: cross-check everything with folder_info checks as well */
	/* todo: work out how to do imap/pop/nntp tests */

	push("getting store");
	store = camel_session_get_store(session, "mbox:///tmp/camel-test/mbox", ex);
	check_msg(!camel_exception_is_set(ex), "getting store: %s", camel_exception_get_description(ex));
	check(store != NULL);
	pull();

	push("Creating output folders");
	for (i=0;i<ARRAY_LEN(mailboxes);i++) {
		push("creating %s", mailboxes[i].name);
		mailboxes[i].folder = folder = camel_store_get_folder(store, mailboxes[i].name, CAMEL_STORE_FOLDER_CREATE, ex);
		check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
		check(folder != NULL);
		
		/* we need an empty folder for this to work */
		test_folder_counts(folder, 0, 0);
		pull();
	}
	pull();

	/* append a bunch of messages with specific content */
	push("creating 100 test message mbox");
	mbox = camel_stream_fs_new_with_name("/tmp/camel-test/inbox", O_WRONLY|O_CREAT|O_EXCL, 0600);
	for (j=0;j<100;j++) {
		char *content, *subject;
			
		push("creating test message");
		msg = test_message_create_simple();
		content = g_strdup_printf("data%d content\n", j);
		test_message_set_content_simple((CamelMimePart *)msg, 0, "text/plain",
						content, strlen(content));
		test_free(content);
		subject = g_strdup_printf("Test%d message%d subject", j, 100-j);
		camel_mime_message_set_subject(msg, subject);
		
		camel_mime_message_set_date(msg, j*60*24, 0);
		pull();
		
		camel_stream_printf(mbox, "From \n");
		check(camel_data_wrapper_write_to_stream((CamelDataWrapper *)msg, mbox) != -1);
#if 0		
		push("appending simple message %d", j);
		camel_folder_append_message(folder, msg, NULL, ex);
		check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
		pull();
#endif				
		test_free(subject);
		
		check_unref(msg, 1);
	}
	check(camel_stream_close(mbox) != -1);
	check_unref(mbox, 1);
	pull();

	push("Building filters");
	driver = camel_filter_driver_new(get_folder, NULL);
	for (i=0;i<ARRAY_LEN(rules);i++) {
		camel_filter_driver_add_rule(driver, rules[i].name, rules[i].match, rules[i].action);
	}
	pull();

	push("Executing filters");
	camel_filter_driver_set_default_folder(driver, mailboxes[0].folder);
	camel_filter_driver_filter_mbox(driver, "/tmp/camel-test/inbox", ex);
	check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));

	/* now need to check the folder counts/etc */

	check_unref(driver, 1);
	pull();

	for (i=0;i<ARRAY_LEN(mailboxes);i++) {
		check_unref(mailboxes[i].folder, 1);
	}

	check_unref(store, 1);

	check_unref(session, 1);
	camel_exception_free(ex);

	camel_test_end();

	return 0;
}
