#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "messages.h"
#include "camel-test.h"

#include <camel/camel-multipart.h>
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

static void
content_finalise(CamelObject *folder, void *crap, void *ba)
{
	g_byte_array_free(ba, TRUE);
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

		camel_object_hook_event((CamelObject *)content, "finalize", content_finalise, ba);
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
	camel_object_unref((CamelObject *)file);

	return msg2;
}

static void
hexdump (const unsigned char *in, int inlen)
{
	const unsigned char *inptr = in, *start = inptr;
	const unsigned char *inend = in + inlen;
	int octets;
	
	while (inptr < inend) {
		octets = 0;
		while (inptr < inend && octets < 16) {
			printf ("%.2X ", *inptr++);
			octets++;
		}
		
		while (octets < 16) {
			printf ("   ");
			octets++;
		}
		
		printf ("       ");
		
		while (start < inptr) {
			fputc (isprint ((int) *start) ? *start : '.', stdout);
			start++;
		}
		
		fputc ('\n', stdout);
	}
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
	camel_data_wrapper_decode_to_stream(dw, (CamelStream *)content);
	
	if (content->buffer->len != len) {
		printf ("original text:\n");
		hexdump (text, len);
		
		printf ("new text:\n");
		hexdump (content->buffer->data, content->buffer->len);
	}
	
	check_msg(content->buffer->len == len, "buffer->len = %d, len = %d", content->buffer->len, len);
	check_msg(memcmp(content->buffer->data, text, content->buffer->len) == 0, "len = %d", len);

	check_unref(content, 1);

	return 0;
}

int
test_message_compare (CamelMimeMessage *msg)
{
	CamelMimeMessage *msg2;
	CamelStreamMem *mem1, *mem2;
	
	mem1 = (CamelStreamMem *) camel_stream_mem_new ();
	check_msg(camel_data_wrapper_write_to_stream ((CamelDataWrapper *) msg, (CamelStream *) mem1) != -1, "write_to_stream 1 failed");
	camel_stream_reset ((CamelStream *) mem1);
	
	msg2 = camel_mime_message_new ();
	check_msg(camel_data_wrapper_construct_from_stream ((CamelDataWrapper *) msg2, (CamelStream *) mem1) != -1, "construct_from_stream 1 failed");
	camel_stream_reset ((CamelStream *) mem1);
	
	mem2 = (CamelStreamMem *) camel_stream_mem_new ();
	check_msg(camel_data_wrapper_write_to_stream ((CamelDataWrapper *) msg2, (CamelStream *) mem2) != -1, "write_to_stream 2 failed");
	camel_stream_reset ((CamelStream *) mem2);
	
	if (mem1->buffer->len != mem2->buffer->len) {
		CamelDataWrapper *content;
		
		printf ("mem1 stream:\n%.*s\n", mem1->buffer->len, mem1->buffer->data);
		printf ("mem2 stream:\n%.*s\n\n", mem2->buffer->len, mem2->buffer->data);

		printf("msg1:\n");
		test_message_dump_structure(msg);
		printf("msg2:\n");
		test_message_dump_structure(msg2);

		content = camel_medium_get_content_object ((CamelMedium *) msg);
	}

	check_unref(msg2, 1);
	
	check_msg (mem1->buffer->len == mem2->buffer->len,
		   "mem1->buffer->len = %d, mem2->buffer->len = %d",
		   mem1->buffer->len, mem2->buffer->len);
	
	check_msg (memcmp (mem1->buffer->data, mem2->buffer->data, mem1->buffer->len) == 0, "msg/stream compare");
	
	camel_object_unref (mem1);
	camel_object_unref (mem2);
	
	return 0;
}

int
test_message_compare_header(CamelMimeMessage *m1, CamelMimeMessage *m2)
{
	return 0;
}

int
test_message_compare_messages(CamelMimeMessage *m1, CamelMimeMessage *m2)
{
	return 0;
}

static void
message_dump_rec(CamelMimeMessage *msg, CamelMimePart *part, int depth)
{
	CamelDataWrapper *containee;
	int parts, i;
	char *s;
	char *mime_type;

	s = alloca(depth+1);
	memset(s, ' ', depth);
	s[depth] = 0;

	mime_type = camel_data_wrapper_get_mime_type((CamelDataWrapper *)part);
	printf("%sPart <%s>\n", s, ((CamelObject *)part)->klass->name);
	printf("%sContent-Type: %s\n", s, mime_type);
	g_free(mime_type);
	printf("%s encoding: %s\n", s, camel_transfer_encoding_to_string(((CamelDataWrapper *)part)->encoding));
	printf("%s part encoding: %s\n", s, camel_transfer_encoding_to_string(part->encoding));

	containee = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	
	if (containee == NULL)
		return;

	mime_type = camel_data_wrapper_get_mime_type(containee);
	printf("%sContent <%s>\n", s, ((CamelObject *)containee)->klass->name);
	printf ("%sContent-Type: %s\n", s, mime_type);
	g_free (mime_type);
	printf("%s encoding: %s\n", s, camel_transfer_encoding_to_string(((CamelDataWrapper *)containee)->encoding));
	
	/* using the object types is more accurate than using the mime/types */
	if (CAMEL_IS_MULTIPART (containee)) {
		parts = camel_multipart_get_number (CAMEL_MULTIPART (containee));
		for (i = 0; i < parts; i++) {
			CamelMimePart *part = camel_multipart_get_part (CAMEL_MULTIPART (containee), i);
			
			message_dump_rec(msg, part, depth+1);
		}
	} else if (CAMEL_IS_MIME_MESSAGE (containee)) {
		message_dump_rec(msg, (CamelMimePart *)containee, depth+1);
	}
}

void
test_message_dump_structure(CamelMimeMessage *m)
{
	message_dump_rec(m, (CamelMimePart *)m, 0);
}
