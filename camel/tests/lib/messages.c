

#include "messages.h"
#include "camel-test.h"

#include <camel/camel-mime-message.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-stream-mem.h>

CamelMimeMessage *
test_message_create_simple(void)
{
	CamelMimeMessage *msg;
	CamelInternetAddress *addr;

	msg = camel_mime_message_new();

	addr = camel_internet_address_new();
	camel_internet_address_add(addr, "Michael Zucchi", "zed@nowhere.com");
	camel_mime_message_set_from(msg, addr);
	camel_address_remove((CamelAddress *)addr, -1);
	camel_internet_address_add(addr, "POSTMASTER", "POSTMASTER@somewhere.net");
	camel_mime_message_set_recipients(msg, CAMEL_RECIPIENT_TYPE_TO, addr);
	camel_address_remove((CamelAddress *)addr, -1);
	camel_internet_address_add(addr, "Michael Zucchi", "zed@nowhere.com");
	camel_mime_message_set_recipients(msg, CAMEL_RECIPIENT_TYPE_CC, addr);

	check_unref(addr, 1);

	camel_mime_message_set_subject(msg, "Simple message subject");
	camel_mime_message_set_date(msg, time(0), 930);

	return msg;
}

void
test_message_set_content_simple(CamelMimePart *part, int how, const char *type, const char *text, int len)
{
	CamelStreamMem *content = NULL;
	CamelDataWrapper *dw;
	static GByteArray *ba;

	switch (how) {
	case 0:
		camel_mime_part_set_content(part, text, len, type);
		break;
	case 1:
		content = (CamelStreamMem *)camel_stream_mem_new_with_buffer(text, len);
		break;
	case 2:
		content = (CamelStreamMem *)camel_stream_mem_new();
		camel_stream_mem_set_buffer(content, text, len);
		break;
	case 3:
		ba = g_byte_array_new();
		g_byte_array_append(ba, text, len);

		content = (CamelStreamMem *)camel_stream_mem_new_with_byte_array(ba);
		ba = NULL;
		break;
	case 4:
		ba = g_byte_array_new();
		g_byte_array_append(ba, text, len);

		content = (CamelStreamMem *)camel_stream_mem_new();
		camel_stream_mem_set_byte_array(content, ba);

		/* ba gets leaked here */
		break;
	}

	if (content != 0) {
		dw = camel_data_wrapper_new();
                camel_data_wrapper_set_mime_type (dw, type);

		camel_data_wrapper_construct_from_stream(dw, (CamelStream *)content);
		camel_medium_set_content_object((CamelMedium *)part, dw);

		check_unref(content, 2);
		check_unref(dw, 2);
	}
}

int
test_message_write_file(CamelMimeMessage *msg, const char *name)
{
	CamelStreamFs *file;
	int ret;

	file = (CamelStreamFs *)camel_stream_fs_new_with_name(name, O_CREAT|O_WRONLY, 0600);
	camel_data_wrapper_write_to_stream((CamelDataWrapper *)msg, (CamelStream *)file);
	ret = camel_stream_close((CamelStream *)file);

	check(((CamelObject *)file)->ref_count == 1);
	camel_object_unref((CamelObject *)file);

	return ret;
}

CamelMimeMessage *
test_message_read_file(const char *name)
{
	CamelStreamFs *file;
	CamelMimeMessage *msg2;

	file = (CamelStreamFs *)camel_stream_fs_new_with_name(name, O_RDONLY, 0);
	msg2 = camel_mime_message_new();

	camel_data_wrapper_construct_from_stream((CamelDataWrapper *)msg2, (CamelStream *)file);
	/* file's refcount may be > 1 if the message is real big */
	check(CAMEL_OBJECT(file)->ref_count >=1);

	return msg2;
}

int
test_message_compare_content(CamelDataWrapper *dw, const char *text, int len)
{
	CamelStreamMem *content;

	/* sigh, ok, so i len == 0, dw will probably be 0 too
	   camel_mime_part_set_content is weird like that */
	if (dw == 0 && len == 0)
		return 0;

	content = (CamelStreamMem *)camel_stream_mem_new();
	camel_data_wrapper_write_to_stream(dw, (CamelStream *)content);

	check_msg(content->buffer->len == len, "buffer->len = %d, len = %d", content->buffer->len, len);
	check_msg(memcmp(content->buffer->data, text, content->buffer->len) == 0, "len = %d", len);

	check_unref(content, 1);

	return 0;
}

int
test_message_compare_header(CamelMimeMessage *m1, CamelMimeMessage *m2)
{
}

int
test_message_compare_messages(CamelMimeMessage *m1, CamelMimeMessage *m2)
{
}
