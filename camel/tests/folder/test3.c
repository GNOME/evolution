/* folder/index testing */

#include <string.h>

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

static void
test_folder_search_sub(CamelFolder *folder, const char *expr, int expected)
{
	CamelException *ex = camel_exception_new();
	GPtrArray *uids;
	GHashTable *hash;
	int i;

	uids = camel_folder_search_by_expression(folder, expr, ex);
	check(uids != NULL);
	check_msg(uids->len == expected, "search %s expected %d got %d", expr, expected, uids->len);
	check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));

	/* check the uid's are actually unique, too */
	hash = g_hash_table_new(g_str_hash, g_str_equal);
	for (i=0;i<uids->len;i++) {
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

#if 0
	/* FIXME: ??? */
	camel_test_nonfatal("most searches require match-all construct");
	push("Testing search: %s", expr);
	test_folder_search_sub(folder, expr, expected);
	pull();
	camel_test_fatal();
#endif

	matchall = g_strdup_printf("(match-all %s)", expr);
	push("Testing search: %s", matchall);
	test_folder_search_sub(folder, matchall, expected);
	test_free(matchall);
	pull();
}

static struct {
	int counts[3];
	char *expr;
} searches[] = {
	{ { 1, 1, 0 }, "(header-matches \"subject\" \"Test1 message99 subject\")" },
	
	{ { 100, 50, 0 }, "(header-contains \"subject\" \"subject\")" },
	{ { 100, 50, 0 }, "(header-contains \"subject\" \"Subject\")" },

	{ { 100, 50, 0 }, "(body-contains \"content\")" },
	{ { 100, 50, 0 }, "(body-contains \"Content\")" },

	{ { 0, 0, 0 }, "(user-flag \"every7\")" },
	{ { 100/13+1, 50/13+1, 0 }, "(user-flag \"every13\")" },
	{ { 1, 1, 0 }, "(= \"7tag1\" (user-tag \"every7\"))" },
	{ { 100/11+1, 50/11+1, 0 }, "(= \"11tag\" (user-tag \"every11\"))" },
	
	{ { 100/13 + 100/17 + 1, 50/13 + 50/17 + 2, 0 }, "(user-flag \"every13\" \"every17\")" },
	{ { 100/13 + 100/17 + 1, 50/13 + 50/17 + 2, 0 }, "(or (user-flag \"every13\") (user-flag \"every17\"))" },
	{ { 1, 0, 0 }, "(and (user-flag \"every13\") (user-flag \"every17\"))" },
	
	{ { 0, 0, 0 }, "(and (header-contains \"subject\" \"Test1\") (header-contains \"subject\" \"Test2\"))" },
	/* we get 11 here as the header-contains is a substring match */
	{ { 11, 6, 0 }, "(and (header-contains \"subject\" \"Test1\") (header-contains \"subject\" \"subject\"))" },
	{ { 1, 1, 0 }, "(and (header-contains \"subject\" \"Test19\") (header-contains \"subject\" \"subject\"))" },
	{ { 0, 0, 0 }, "(and (header-contains \"subject\" \"Test191\") (header-contains \"subject\" \"subject\"))" },
	{ { 1, 1, 0 }, "(and (header-contains \"subject\" \"Test1\") (header-contains \"subject\" \"message99\"))" },
	
	{ { 22, 11, 0 }, "(or (header-contains \"subject\" \"Test1\") (header-contains \"subject\" \"Test2\"))" },
	{ { 2, 1, 0 }, "(or (header-contains \"subject\" \"Test16\") (header-contains \"subject\" \"Test99\"))" },
	{ { 1, 1, 0 }, "(or (header-contains \"subject\" \"Test123\") (header-contains \"subject\" \"Test99\"))" },
	{ { 100, 50, 0 }, "(or (header-contains \"subject\" \"Test1\") (header-contains \"subject\" \"subject\"))" },
	{ { 11, 6, 0 }, "(or (header-contains \"subject\" \"Test1\") (header-contains \"subject\" \"message99\"))" },
	
	/* 72000 is 24*60*100 == half the 'sent date' of the messages */
	{ { 100/2, 50/2, 0 }, "(> 72000 (get-sent-date))" },
	{ { 100/2-1, 50/2, 0 }, "(< 72000 (get-sent-date))" },
	{ { 1, 0, 0 }, "(= 72000 (get-sent-date))" },
	{ { 0, 0, 0 }, "(= 72001 (get-sent-date))" },
	
 	{ { (100/2-1)/17+1, (50/2-1)/17+1, 0 }, "(and (user-flag \"every17\") (< 72000 (get-sent-date)))" },
	{ { (100/2-1)/17+1, (50/2-1)/17, 0 }, "(and (user-flag \"every17\") (> 72000 (get-sent-date)))" },
	{ { (100/2-1)/13+1, (50/2-1)/13+1, 0 }, "(and (user-flag \"every13\") (< 72000 (get-sent-date)))" },
	{ { (100/2-1)/13+1, (50/2-1)/13+1, 0 }, "(and (user-flag \"every13\") (> 72000 (get-sent-date)))" },
	
	{ { 100/2+100/2/17, 50/2+50/2/17, 0 }, "(or (user-flag \"every17\") (< 72000 (get-sent-date)))" },
	{ { 100/2+100/2/17+1, 50/2+50/2/17+1, 0 }, "(or (user-flag \"every17\") (> 72000 (get-sent-date)))" },
	{ { 100/2+100/2/13, 50/2+50/2/13+1, 0 }, "(or (user-flag \"every13\") (< 72000 (get-sent-date)))" },
	{ { 100/2+100/2/13+1, 50/2+50/2/13+1, 0 }, "(or (user-flag \"every13\") (> 72000 (get-sent-date)))" },
};

static void
run_search(CamelFolder *folder, int m)
{
	int i, j = 0;

	check(m == 50 || m == 100 || m == 0);

	/* *shrug* messy, but it'll do */
	if (m==50)
		j = 1;
	else if (m==0)
		j = 2;

	push("performing searches, expected %d", m);
	for (i=0;i<ARRAY_LEN(searches);i++) {
		push("running search %d: %s", i, searches[i].expr);
		test_folder_search(folder, searches[i].expr, searches[i].counts[j]);
		pull();
	}
	pull();
}

static const char *local_drivers[] = { "local" };

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

	camel_test_init(argc, argv);
	camel_test_provider_init(1, local_drivers);

	ex = camel_exception_new();

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	session = camel_test_session_new ("/tmp/camel-test");

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
				camel_folder_append_message(folder, msg, NULL, NULL, ex);
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

			camel_test_nonfatal("Index not guaranteed to be accurate before sync: should be fixed eventually");
			push("Search before sync");
			run_search(folder, 100);
			pull();
			camel_test_fatal();

			push("syncing folder, searching");
			camel_folder_sync(folder, FALSE, ex);
			run_search(folder, 100);
			pull();

			push("syncing wiht expunge, search");
			camel_folder_sync(folder, TRUE, ex);
			run_search(folder, 100);
			pull();

			push("deleting every 2nd message");
			uids = camel_folder_get_uids(folder);
			check(uids->len == 100);
			for (j=0;j<uids->len;j+=2) {
				camel_folder_delete_message(folder, uids->pdata[j]);
			}
			camel_folder_free_uids(folder, uids);
			run_search(folder, 100);

			push("syncing");
			camel_folder_sync(folder, FALSE, ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			run_search(folder, 100);
			pull();

			push("expunging");
			camel_folder_expunge(folder, ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			run_search(folder, 50);
			pull();

			pull();

			push("closing and re-opening folder");
			check_unref(folder, 1);
			folder = camel_store_get_folder(store, "testbox", flags&~(CAMEL_STORE_FOLDER_CREATE), ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			check(folder != NULL);

			push("deleting remaining messages");
			uids = camel_folder_get_uids(folder);
			check(uids->len == 50);
			for (j=0;j<uids->len;j++) {
				camel_folder_delete_message(folder, uids->pdata[j]);
			}
			camel_folder_free_uids(folder, uids);
			run_search(folder, 50);

			push("syncing");
			camel_folder_sync(folder, FALSE, ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			run_search(folder, 50);
			pull();

			push("expunging");
			camel_folder_expunge(folder, ex);
			check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
			run_search(folder, 0);
			pull();

			pull();

			check_unref(folder, 1);
			pull();

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
