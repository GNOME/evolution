/* folder/index testing */

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


static void
test_folder_search_sub(CamelFolder *folder, const char *expr, int expected)
{
	CamelException *ex = camel_exception_new();
	GPtrArray *uids;
	GHashTable *hash;
	int i;

	uids = camel_folder_search_by_expression(folder, expr, ex);
	check(uids != NULL);
	check(uids->len == expected);
	check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));

	/* check the uid's are actually unique, too */
	hash = g_hash_table_new(g_str_hash, g_str_equal);
	for (i=0;uids->len;i++) {
		check(g_hash_table_lookup(hash, uids->pdata[i]) == NULL);
		g_hash_table_insert(hash, uids->pdata[i], uids->pdata[i]);
	}
	g_hash_table_destroy(hash);

	camel_folder_search_free(folder, uids);

	camel_exception_free(ex);
}

static void
test_folder_search(CamelFolder *folder, const char *expr, int expected)
{
	char *matchall;

	push("Testing search: %s", expr);
	test_folder_search_sub(folder, expr, expected);
	pull();

	matchall = g_strdup_printf("(match-all %s)", expr);
	push("Testing search: %s", matchall);
	test_folder_search_sub(folder, matchall, expected);
	test_free(matchall);
	pull();
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

	ex = camel_exception_new();

	camel_test_init(argc, argv);

	session = camel_session_new("/tmp/camel-test", auth_callback, NULL, NULL);

	/* todo: cross-check everything with folder_info checks as well */
	/* todo: work out how to do imap/pop/nntp tests */

	/* we iterate over all stores we want to test, with indexing or indexing turned on or off */
	for (i=0;i<ARRAY_LEN(stores);i++) {
		char *name = stores[i];
		for (indexed = 0;indexed<2;indexed++) {
			char *what = g_strdup_printf("folder search: %s (%sindexed)", name, indexed?"":"non-");
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

			/* we need an empty folder for this to work */
			test_folder_counts(folder, 0, 0);
			pull();

			/* append a bunch of messages with specific content */
			push("appending 100 test messages");
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

				push("appending simple message %d", j);
				camel_folder_append_message(folder, msg, NULL, ex);
				check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
				pull();

				test_free(subject);

				check_unref(msg, 1);
			}
			pull();

			push("Setting up some flags &c");
			uids = camel_folder_get_uids(folder);
			check(uids->len == 100);
			for (j=0;j<100;j++) {
				char *uid = uids->pdata[j];

				if ((j/13)*13 == j) {
					camel_folder_set_message_user_flag(folder, uid, "every13", TRUE);
				}
				if ((j/17)*17 == j) {
					camel_folder_set_message_user_flag(folder, uid, "every17", TRUE);
				}
				if ((j/7)*7 == j) {
					char *tag = g_strdup_printf("7tag%d", j/7);
					camel_folder_set_message_user_tag(folder, uid, "every7", tag);
					test_free(tag);
				}
				if ((j/11)*11 == j) {
					camel_folder_set_message_user_tag(folder, uid, "every11", "11tag");
				}
			}
			camel_folder_free_uids(folder, uids);
			pull();

			/* should try invalid search strings too */

			/* try some searches */
			push("performing searches");
			test_folder_search(folder, "(header-contains \"subject\" \"subject\")", 100);
			test_folder_search(folder, "(header-contains \"subject\" \"Subject\")", 100);

			test_folder_search(folder, "(body-contains \"content\")", 100);
			test_folder_search(folder, "(body-contains \"Content\")", 100);

			test_folder_search(folder, "(user-flag \"every7\")", 0);
			test_folder_search(folder, "(user-flag \"every13\")", 100/13);
			test_folder_search(folder, "(= \"7tag1\" (user-tag \"every7\"))", 1);
			test_folder_search(folder, "(= \"11tag\" (user-tag \"every11\"))", 100/11);

			test_folder_search(folder, "(user-flag \"every13\" \"every17\")", 100/13 + 100/17);
			test_folder_search(folder, "(or (user-flag \"every13\") (user-flag \"every17\"))", 100/13 + 100/17);
			test_folder_search(folder, "(and (user-flag \"every13\") (user-flag \"every17\"))", 0);

			test_folder_search(folder, "(and (header-contains \"subject\" \"Test1\")"
					   "(header-contains \"subject\" \"Test2\"))", 0);
			test_folder_search(folder, "(and (header-contains \"subject\" \"Test1\")"
					   "(header-contains \"subject\" \"subject\"))", 1);
			test_folder_search(folder, "(and (header-contains \"subject\" \"Test1\")"
					   "(header-contains \"subject\" \"message99\"))", 1);

			test_folder_search(folder, "(or (header-contains \"subject\" \"Test1\")"
					   "(header-contains \"subject\" \"Test2\"))", 2);
			test_folder_search(folder, "(or (header-contains \"subject\" \"Test1\")"
					   "(header-contains \"subject\" \"subject\"))", 100);
			test_folder_search(folder, "(or (header-contains \"subject\" \"Test1\")"
					   "(header-contains \"subject\" \"message99\"))", 1);

			/* 7200 is 24*60*50 == half the 'sent date' of the messages */
			test_folder_search(folder, "(> 7200 (get-sent-date))", 49);
			test_folder_search(folder, "(< 7200 (get-sent-date))", 49);
			test_folder_search(folder, "(= 7200 (get-sent-date))", 1);
			test_folder_search(folder, "(= 7201 (get-sent-date))", 0);

			test_folder_search(folder, "(and (user-flag \"every17\") (< 7200 (get-sent-date)))", 49/17);
			test_folder_search(folder, "(and (user-flag \"every17\") (> 7200 (get-sent-date)))", 49/17-1);
			test_folder_search(folder, "(and (user-flag \"every13\") (< 7200 (get-sent-date)))", 49/13);
			test_folder_search(folder, "(and (user-flag \"every13\") (> 7200 (get-sent-date)))", 49/13-1);

			test_folder_search(folder, "(or (user-flag \"every17\") (< 7200 (get-sent-date)))", 49);
			test_folder_search(folder, "(or (user-flag \"every17\") (> 7200 (get-sent-date)))", 49);
			test_folder_search(folder, "(or (user-flag \"every13\") (< 7200 (get-sent-date)))", 49);
			test_folder_search(folder, "(or (user-flag \"every13\") (> 7200 (get-sent-date)))", 49);

			push("deleting every 2nd message & expunging");
			uids = camel_folder_get_uids(folder);
			check(uids->len == 100);
			for (j=0;j<uids->len;j++) {
				camel_folder_delete_message(folder, uids->pdata[j]);
			}

			push("searches after deletions, before sync");
			test_folder_search(folder, "(header-contains \"subject\" \"subject\")", 100);
			test_folder_search(folder, "(body-contains \"content\")", 100);
			pull();

			camel_folder_sync(folder, FALSE, ex);

			push("searches after sync, before expunge");
			test_folder_search(folder, "(header-contains \"subject\" \"subject\")", 100);
			test_folder_search(folder, "(body-contains \"content\")", 100);
			pull();

			camel_folder_expunge(folder, ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			camel_folder_free_uids(folder, uids);
			pull();

			/* more searches */
			push("searches after deletions");
			test_folder_search(folder, "(header-contains \"subject\" \"subject\")", 50);
			test_folder_search(folder, "(body-contains \"content\")", 50);
			pull();

			push("deleting remaining messages & expunging");
			uids = camel_folder_get_uids(folder);
			check(uids->len == 100);
			for (j=0;j<uids->len;j++) {
				camel_folder_delete_message(folder, uids->pdata[j]);
			}
			camel_folder_expunge(folder, ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			camel_folder_free_uids(folder, uids);
			pull();

			push("searches wtih no messages");
			test_folder_search(folder, "(header-contains \"subject\" \"subject\")", 0);
			test_folder_search(folder, "(body-contains \"content\")", 0);
			pull();

			check_unref(folder, 1);
			pull();

			push("deleting test folder, with no messages in it");
			camel_store_delete_folder(store, "testbox", ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			pull();

			camel_object_unref((CamelObject *)store);
			camel_test_end();
		}
	}

	camel_object_unref((CamelObject *)session);
	camel_exception_free(ex);

	return 0;
}
