
/* threaded folder testing */

#include <string.h>
#include <pthread.h>

#include "camel-test.h"
#include "session.h"

#include <camel/camel-exception.h>
#include <camel/camel-service.h>
#include <camel/camel-store.h>

#define MAX_LOOP (10000)
#define MAX_THREADS (5)

#define d(x) 

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

static CamelSession *session;

/* FIXME: flags aren't really right yet */
/* ASCII sorted on full_name */
static CamelFolderInfo fi_list_1[] = {
	{ NULL, NULL, NULL, "maildir:/tmp/camel-test/maildir#.", "Inbox", ".", CAMEL_FOLDER_SYSTEM|CAMEL_FOLDER_NOCHILDREN, -1, -1 },
	{ NULL, NULL, NULL, "maildir:/tmp/camel-test/maildir#Junk", "Junk", "Junk", CAMEL_FOLDER_SYSTEM|CAMEL_FOLDER_NOCHILDREN, -1, -1 },
	{ NULL, NULL, NULL, "maildir:/tmp/camel-test/maildir#Trash", "Trash", "Trash", CAMEL_FOLDER_SYSTEM|CAMEL_FOLDER_NOCHILDREN, -1, -1 },
	{ NULL, NULL, NULL, "maildir:/tmp/camel-test/maildir#testbox", "testbox", "testbox", CAMEL_FOLDER_CHILDREN, -1, -1 },
	{ NULL, NULL, NULL, "maildir:/tmp/camel-test/maildir#testbox/foo", "foo", "testbox/foo", CAMEL_FOLDER_NOCHILDREN, -1, -1 },
	{ NULL, NULL, NULL, "maildir:/tmp/camel-test/maildir#testbox2", "testbox2", "testbox2", CAMEL_FOLDER_NOCHILDREN, -1, -1 },
};

static CamelFolderInfo fi_list_2[] = {
	{ NULL, NULL, NULL, "maildir:/tmp/camel-test/maildir#.", "Inbox", ".", CAMEL_FOLDER_SYSTEM|CAMEL_FOLDER_NOCHILDREN, -1, -1 },
	{ NULL, NULL, NULL, "maildir:/tmp/camel-test/maildir#Junk", "Junk", "Junk", CAMEL_FOLDER_SYSTEM|CAMEL_FOLDER_NOCHILDREN, -1, -1 },
	{ NULL, NULL, NULL, "maildir:/tmp/camel-test/maildir#Trash", "Trash", "Trash", CAMEL_FOLDER_SYSTEM|CAMEL_FOLDER_NOCHILDREN, -1, -1 },
	{ NULL, NULL, NULL, "maildir:/tmp/camel-test/maildir#testbox", "testbox", "testbox", CAMEL_FOLDER_NOCHILDREN, -1, -1 },
	{ NULL, NULL, NULL, "maildir:/tmp/camel-test/maildir#testbox2", "testbox2", "testbox2", CAMEL_FOLDER_NOCHILDREN, -1, -1 },
};

static CamelFolderInfo fi_list_3[] = {
	{ NULL, NULL, NULL, "maildir:/tmp/camel-test/maildir#testbox", "testbox", "testbox", CAMEL_FOLDER_CHILDREN, -1, -1 },
	{ NULL, NULL, NULL, "maildir:/tmp/camel-test/maildir#testbox/foo", "foo", "testbox/foo", CAMEL_FOLDER_NOCHILDREN, -1, -1 },
};

static int
cmp_fi(const void *a, const void *b)
{
	const CamelFolderInfo *fa = ((const CamelFolderInfo **)a)[0];
	const CamelFolderInfo *fb = ((const CamelFolderInfo **)b)[0];

	return strcmp(fa->full_name, fb->full_name);
}

static void
add_fi(GPtrArray *folders, CamelFolderInfo *fi)
{
	while (fi) {
		g_ptr_array_add(folders, fi);
		if (fi->child)
			add_fi(folders, fi->child);
		fi = fi->next;
	}
}

static void
check_fi(CamelFolderInfo *fi, CamelFolderInfo *list, int len)
{
	GPtrArray *folders = g_ptr_array_new();
	int i;

	add_fi(folders, fi);
	check_msg(folders->len == len, "unexpected number of folders returned from folderinfo");
	qsort(folders->pdata, folders->len, sizeof(folders->pdata[0]), cmp_fi);
	for (i=0;i<len;i++) {
		CamelFolderInfo *f = folders->pdata[i];

		camel_test_push("checking folder '%s'", list[i].uri);

		check_msg(!strcmp(f->uri, list[i].uri), "got '%s' expecting '%s'", f->uri, list[i].uri);
		check(!strcmp(f->full_name, list[i].full_name));

		/* this might be translated, but we can't know */
		camel_test_nonfatal("Inbox not english");
		check(!strcmp(f->name, list[i].name));
		camel_test_fatal();

		camel_test_nonfatal("Flags mismatch");
		check(f->flags == list[i].flags);
		camel_test_fatal();

		camel_test_pull();
	}

	g_ptr_array_free(folders, TRUE);
}

int main(int argc, char **argv)
{
	CamelException *ex;
	CamelFolder *f1, *f2;
	CamelStore *store;
	CamelFolderInfo *fi;

	camel_test_init(argc, argv);

	ex = camel_exception_new();

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	session = camel_test_session_new("/tmp/camel-test");
	store = camel_session_get_store(session, "maildir:///tmp/camel-test/maildir", ex);
	camel_exception_clear(ex);

	camel_test_start("Maildir backward compatability tests");

	camel_test_push("./ prefix path, one level");
	f1 = camel_store_get_folder(store, "testbox", CAMEL_STORE_FOLDER_CREATE, ex);
	check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
	f2 = camel_store_get_folder(store, "./testbox", CAMEL_STORE_FOLDER_CREATE, ex);
	check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
	check(f1 == f2);
	check_unref(f2, 2);
	check_unref(f1, 1);
	camel_test_pull();

	camel_test_push("./ prefix path, one level, no create");
	f1 = camel_store_get_folder(store, "testbox2", CAMEL_STORE_FOLDER_CREATE, ex);
	check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
	f2 = camel_store_get_folder(store, "./testbox2", 0, ex);
	check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
	check(f1 == f2);
	check_unref(f2, 2);
	check_unref(f1, 1);
	camel_test_pull();

	camel_test_push("./ prefix path, two levels");
	f1 = camel_store_get_folder(store, "testbox/foo", CAMEL_STORE_FOLDER_CREATE, ex);
	check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
	f2 = camel_store_get_folder(store, "./testbox/foo", CAMEL_STORE_FOLDER_CREATE, ex);
	check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
	check(f1 == f2);
	check_unref(f2, 2);
	check_unref(f1, 1);
	camel_test_pull();

	camel_test_push("'.' == Inbox");
	f2 = camel_store_get_inbox(store, ex);
	check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
	f1 = camel_store_get_folder(store, ".", 0, ex);
	check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
	check(f1 == f2);
	check_unref(f2, 2);
	check_unref(f1, 1);
	camel_test_pull();

	camel_test_push("folder info, recursive");
	fi = camel_store_get_folder_info(store, "", CAMEL_STORE_FOLDER_INFO_RECURSIVE, ex);
	check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
	check(fi != NULL);
	check_fi(fi, fi_list_1, ARRAY_LEN(fi_list_1));
	camel_test_pull();

	camel_test_push("folder info, flat");
	fi = camel_store_get_folder_info(store, "", 0, ex);
	check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
	check(fi != NULL);
	check_fi(fi, fi_list_2, ARRAY_LEN(fi_list_2));
	camel_test_pull();

	camel_test_push("folder info, recursive, non root");
	fi = camel_store_get_folder_info(store, "testbox", CAMEL_STORE_FOLDER_INFO_RECURSIVE, ex);
	check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
	check(fi != NULL);
	check_fi(fi, fi_list_3, ARRAY_LEN(fi_list_3));
	camel_test_pull();

	check_unref(store, 1);
	check_unref(session, 1);

	camel_exception_free(ex);

	camel_test_end();

	return 0;
}
