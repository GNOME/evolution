/*
  Multipart.
*/

#include "camel-test.h"
#include "messages.h"

/* for stat */
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <camel/camel-mime-message.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-stream-mem.h>
#include "camel/camel-multipart.h"

int main(int argc, char **argv)
{
	CamelMimeMessage *msg, *msg2, *msg3;
	CamelMultipart *mp, *mp2;
	CamelMimePart *part, *part2, *part3;

	camel_test_init(argc, argv);

	camel_test_start("multipart message");

	push("building message");
	msg = test_message_create_simple();
	mp = camel_multipart_new();

	/* Hrm, this should be able to set its own boundary, no? */
	camel_multipart_set_boundary(mp, "_=,.XYZ_Kangaroo_Meat_is_!_ABADF00D");
	check(strcmp(camel_multipart_get_boundary(mp), "_=,.XYZ_Kangaroo_Meat_is_!_ABADF00D") == 0);

	camel_medium_set_content_object((CamelMedium *)msg, (CamelDataWrapper *)mp);
	check(camel_multipart_get_number(mp) == 0);
	check(camel_multipart_get_part(mp, 0) == NULL);
	check(camel_multipart_get_part(mp, 1) == NULL);

	push("adding/removing parts");
	part = camel_mime_part_new();
	test_message_set_content_simple(part, 0, "text/plain", "content part 1", strlen("content part 1"));
	camel_multipart_add_part(mp, part);
	check(CAMEL_OBJECT(part)->ref_count == 2);
	check(camel_multipart_get_number(mp) == 1);
	check(camel_multipart_get_part(mp, 0) == part);
	check(camel_multipart_get_part(mp, 1) == NULL);

	camel_multipart_remove_part(mp, part);
	check(CAMEL_OBJECT(part)->ref_count == 1);
	check(camel_multipart_get_number(mp) == 0);
	check(camel_multipart_get_part(mp, 0) == NULL);
	check(camel_multipart_get_part(mp, 1) == NULL);

	camel_multipart_add_part_at(mp, part, 0);
	check(CAMEL_OBJECT(part)->ref_count == 2);
	check(camel_multipart_get_number(mp) == 1);
	check(camel_multipart_get_part(mp, 0) == part);
	check(camel_multipart_get_part(mp, 1) == NULL);

	check(camel_multipart_remove_part_at(mp, 1) == NULL);
	check(CAMEL_OBJECT(part)->ref_count == 2);
	check(camel_multipart_get_number(mp) == 1);
	check(camel_multipart_get_part(mp, 0) == part);
	check(camel_multipart_get_part(mp, 1) == NULL);

	check(camel_multipart_remove_part_at(mp, 0) == part);
	check(CAMEL_OBJECT(part)->ref_count == 1);
	check(camel_multipart_get_number(mp) == 0);
	check(camel_multipart_get_part(mp, 0) == NULL);
	check(camel_multipart_get_part(mp, 1) == NULL);

	camel_multipart_add_part(mp, part);
	check(CAMEL_OBJECT(part)->ref_count == 2);
	check(camel_multipart_get_number(mp) == 1);
	check(camel_multipart_get_part(mp, 0) == part);
	check(camel_multipart_get_part(mp, 1) == NULL);

	part2 = camel_mime_part_new();
	test_message_set_content_simple(part2, 0, "text/plain", "content part 2", strlen("content part 2"));
	camel_multipart_add_part(mp, part2);
	check(CAMEL_OBJECT(part2)->ref_count == 2);
	check(camel_multipart_get_number(mp) == 2);
	check(camel_multipart_get_part(mp, 0) == part);
	check(camel_multipart_get_part(mp, 1) == part2);

	part3 = camel_mime_part_new();
	test_message_set_content_simple(part3, 0, "text/plain", "content part 3", strlen("content part 3"));
	camel_multipart_add_part_at(mp, part3, 1);
	check(CAMEL_OBJECT(part3)->ref_count == 2);
	check(camel_multipart_get_number(mp) == 3);
	check(camel_multipart_get_part(mp, 0) == part);
	check(camel_multipart_get_part(mp, 1) == part3);
	check(camel_multipart_get_part(mp, 2) == part2);
	pull();

	push("save message to test3.msg");
	unlink("test3.msg");
	test_message_write_file(msg, "test3.msg");
	pull();
	
	push("read from test3.msg");
	msg2 = test_message_read_file("test3.msg");
	pull();

	push("compre content of multipart");
	mp2 = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)msg2);
	check(mp2 != NULL);
	check(CAMEL_IS_MULTIPART(mp2));
	check(camel_multipart_get_number(mp2) == 3);

	check(strcmp(camel_multipart_get_boundary(mp2), "_=,.XYZ_Kangaroo_Meat_is_!_ABADF00D") == 0);
	check(mp2->preface == NULL || strlen(mp2->preface) == 0);

	/* FIXME */
	camel_test_nonfatal("postface may gain a single \\n?");
	check_msg(mp2->postface == NULL || strlen(mp2->postface) == 0, "postface: '%s'", mp2->postface);
	camel_test_fatal();

	test_message_compare_content(camel_medium_get_content_object(CAMEL_MEDIUM(camel_multipart_get_part(mp2, 0))),
				     "content part 1", strlen("content part 1"));
	test_message_compare_content(camel_medium_get_content_object(CAMEL_MEDIUM(camel_multipart_get_part(mp2, 1))),
				     "content part 3", strlen("content part 3"));
	test_message_compare_content(camel_medium_get_content_object(CAMEL_MEDIUM(camel_multipart_get_part(mp2, 2))),
				     "content part 2", strlen("content part 2"));
	pull();

	push("writing again, & re-reading");
	unlink("test3-2.msg");
	test_message_write_file(msg2, "test3-2.msg");
	msg3 = test_message_read_file("test3-2.msg");

	push("comparing again");
	mp2 = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)msg3);
	check(mp2 != NULL);
	check(CAMEL_IS_MULTIPART(mp2));
	check(camel_multipart_get_number(mp2) == 3);

	check(strcmp(camel_multipart_get_boundary(mp2), "_=,.XYZ_Kangaroo_Meat_is_!_ABADF00D") == 0);
	check(mp2->preface == NULL || strlen(mp2->preface) == 0);

	check_msg(mp2->postface == NULL || strlen(mp2->postface) == 0, "postface: '%s'", mp2->postface);

	test_message_compare_content(camel_medium_get_content_object(CAMEL_MEDIUM(camel_multipart_get_part(mp2, 0))),
				     "content part 1", strlen("content part 1"));
	test_message_compare_content(camel_medium_get_content_object(CAMEL_MEDIUM(camel_multipart_get_part(mp2, 1))),
				     "content part 3", strlen("content part 3"));
	test_message_compare_content(camel_medium_get_content_object(CAMEL_MEDIUM(camel_multipart_get_part(mp2, 2))),
				     "content part 2", strlen("content part 2"));
	pull();
	pull();

	check_unref(msg2, 1);
	check_unref(msg3, 1);

	push("testing pre/post text");
	camel_multipart_set_preface(mp, "pre-text\nLines.");
	camel_multipart_set_postface(mp, "post-text, no lines.\nOne line.\n");

	check(strcmp(mp->preface, "pre-text\nLines.") == 0);
	check(strcmp(mp->postface, "post-text, no lines.\nOne line.\n") == 0);

	push("writing /re-reading");
	unlink("test3-3.msg");
	test_message_write_file(msg, "test3-3.msg");
	msg2 = test_message_read_file("test3-3.msg");

	mp2 = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)msg2);
	check(mp2 != NULL);
	check(CAMEL_IS_MULTIPART(mp2));
	check(camel_multipart_get_number(mp2) == 3);

	check(strcmp(camel_multipart_get_boundary(mp2), "_=,.XYZ_Kangaroo_Meat_is_!_ABADF00D") == 0);
	check(mp2->preface && strcmp(mp2->preface, "pre-text\nLines.") == 0);
	check(mp2->postface && strcmp(mp2->postface, "post-text, no lines.\nOne line.\n") == 0);
	test_message_compare_content(camel_medium_get_content_object(CAMEL_MEDIUM(camel_multipart_get_part(mp2, 0))),
					   "content part 1", strlen("content part 1"));
	test_message_compare_content(camel_medium_get_content_object(CAMEL_MEDIUM(camel_multipart_get_part(mp2, 1))),
				     "content part 3", strlen("content part 3"));
	test_message_compare_content(camel_medium_get_content_object(CAMEL_MEDIUM(camel_multipart_get_part(mp2, 2))),
				     "content part 2", strlen("content part 2"));
	pull();
	check_unref(msg2, 1);
	pull();

	check_unref(msg, 1);
	check_unref(mp, 1);
	check_unref(part, 1);
	check_unref(part2, 1);
	check_unref(part3, 1);

	camel_test_end();

	return 0;
}
