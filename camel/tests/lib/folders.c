
#include "camel-test.h"
#include "folders.h"

#include "camel/camel-exception.h"

/* check the total/unread is what we think it should be */
void
test_folder_counts(CamelFolder *folder, int total, int unread)
{
	GPtrArray *s;
	int i, myunread;
	const CamelMessageInfo *info;

	push("test folder counts %d total %d unread", total, unread);

	/* first, use the standard functions */
	check(camel_folder_get_message_count(folder) == total);
	check(camel_folder_get_unread_message_count(folder) == total);

	/* next, use the summary */
	s = camel_folder_get_summary(folder);
	check(s != NULL);
	check(s->len == total);
	myunread = s->len;
	for (i=0;i<s->len;i++) {
		info = s->pdata[i];
		if (info->flags & CAMEL_MESSAGE_SEEN)
			myunread--;
	}
	check(unread == myunread);
	camel_folder_free_summary(folder, s);

	/* last, use the uid list */
	s = camel_folder_get_uids(folder);
	check(s != NULL);
	check(s->len == total);
	myunread = s->len;
	for (i=0;i<s->len;i++) {
		info = camel_folder_get_message_info(folder, s->pdata[i]);
		if (info->flags & CAMEL_MESSAGE_SEEN)
			myunread--;
	}
	check(unread == myunread);
	camel_folder_free_uids(folder, s);

	pull();
}

static int
safe_strcmp(const char *a, const char *b)
{
	if (a == NULL && b == NULL)
		return 0;
	if (a == NULL)
		return 1;
	if (b == NULL)
		return -1;
	return strcmp(a, b);
}

void
test_message_info(CamelMimeMessage *msg, const CamelMessageInfo *info)
{
	check_msg(safe_strcmp(info->subject, camel_mime_message_get_subject(msg)) == 0,
		  "info->subject = '%s', get_subject() = '%s'", info->subject, camel_mime_message_get_subject(msg));

	/* FIXME: testing from/cc/to, etc is more tricky */

	check(info->date_sent == camel_mime_message_get_date(msg, NULL));

	/* date received isn't set for messages that haven't been sent anywhere ... */
	/*check(info->date_received == camel_mime_message_get_date_received(msg, NULL));*/

	/* so is messageid/references, etc */
}

/* check a message is present */
void
test_folder_message(CamelFolder *folder, const char *uid)
{
	CamelMimeMessage *msg;
	const CamelMessageInfo *info;
	GPtrArray *s;
	int i;
	CamelException *ex = camel_exception_new();
	int found;

	push("uid %s is in folder", uid);

	/* first try getting info */
	info = camel_folder_get_message_info(folder, uid);
	check(info != NULL);
	check(strcmp(info->uid, uid) == 0);

	/* then, getting message */
	msg = camel_folder_get_message(folder, uid, ex);
	check_msg(!camel_exception_is_set(ex), "%s", camel_exception_get_description(ex));
	check(msg != NULL);

	/* cross check with info */
	test_message_info(msg, info);

	camel_object_unref((CamelObject *)msg);

	/* see if it is in the summary (only once) */
	s = camel_folder_get_summary(folder);
	check(s != NULL);
	found = 0;
	for (i=0;i<s->len;i++) {
		info = s->pdata[i];
		if (strcmp(info->uid, uid) == 0)
			found++;
	}
	check(found == 1);
	camel_folder_free_summary(folder, s);

	/* check it is in the uid list */
	s = camel_folder_get_uids(folder);
	check(s != NULL);
	found = 0;
	for (i=0;i<s->len;i++) {
		if (strcmp(s->pdata[i], uid) == 0)
			found++;
	}
	check(found == 1);
	camel_folder_free_uids(folder, s);

	camel_exception_free(ex);

	pull();
}

/* check message not present */
void
test_folder_not_message(CamelFolder *folder, const char *uid)
{
	CamelMimeMessage *msg;
	const CamelMessageInfo *info;
	GPtrArray *s;
	int i;
	CamelException *ex = camel_exception_new();
	int found;

	push("uid %s is not in folder", uid);

	/* first try getting info */
	info = camel_folder_get_message_info(folder, uid);
	check(info == NULL);

	/* then, getting message */
	msg = camel_folder_get_message(folder, uid, ex);
	check(camel_exception_is_set(ex));
	check(msg == NULL);
	camel_exception_clear(ex);

	/* see if it is not in the summary (only once) */
	s = camel_folder_get_summary(folder);
	check(s != NULL);
	found = 0;
	for (i=0;i<s->len;i++) {
		info = s->pdata[i];
		if (strcmp(info->uid, uid) == 0)
			found++;
	}
	check(found == 0);
	camel_folder_free_summary(folder, s);

	/* check it is not in the uid list */
	s = camel_folder_get_uids(folder);
	check(s != NULL);
	found = 0;
	for (i=0;i<s->len;i++) {
		if (strcmp(s->pdata[i], uid) == 0)
			found++;
	}
	check(found == 0);
	camel_folder_free_uids(folder, s);

	camel_exception_free(ex);

	pull();
}
