/*
  test1.c

  Create a message, save it.

  Retrieve message, compare content.


  Operations:
	writing/loading from different types of streams
	reading/writing different content
	reading/writing different encodings
	reading/writing different charsets

  Just testing streams:
  	different stream types
	different file ops
	seek, eof, etc.
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

struct _text {
	char *text;
	int len;
};

#define MAX_TEXTS (14)
struct _text texts[MAX_TEXTS];

static void
setup(void)
{
	int i, j;
	char *p;

	/* setup various edge and other general cases */
	texts[0].text = "";
	texts[0].len = 0;
	texts[1].text = "";
	texts[1].len = 1;
	texts[2].text = "\n";
	texts[2].len = 1;
	texts[3].text = "A";
	texts[3].len = 1;
	texts[4].text = "This is a test.\n.";
	texts[4].len = strlen(texts[4].text);
	texts[5].text = "This is a test.\n\n.\n";
	texts[5].len = strlen(texts[5].text);
	texts[6].text = g_malloc0(1024);
	texts[6].len = 1024;
	texts[7].text = g_malloc0(102400);
	texts[7].len = 102400;
	texts[8].text = g_malloc(1024);
	memset(texts[8].text, '\n', 1024);
	texts[8].len = 1024;
	texts[9].text = g_malloc(102400);
	memset(texts[9].text, '\n', 102400);
	texts[9].len = 102400;
	texts[10].text = g_malloc(1024);
	memset(texts[10].text, ' ', 1024);
	texts[10].len = 1024;
	texts[11].text = g_malloc(102400);
	memset(texts[11].text, ' ', 102400);
	texts[11].len = 102400;

	srand(42);
	p = texts[12].text = g_malloc(1024);
	for (i=0;i<1024;i++) {
		j = rand();
		if (j<RAND_MAX/120)
			*p++ = '\n';
		else
			*p++ = (j % 95) + 32;
	}
	texts[12].len = 1024;
	p = texts[13].text = g_malloc(102400);
	for (i=0;i<102400;i++) {
		j = rand();
		if (j<RAND_MAX/120)
			*p++ = '\n';
		else
			*p++ = (j % 95) + 32;
	}
	texts[13].len = 102400;
}

static void cleanup(void)
{
	int i;

	for (i=6;i<14;i++) {
		g_free(texts[i].text);
	}
}

int main(int argc, char **argv)
{
	CamelMimeMessage *msg, *msg2;
	int i, j;
	char *text;
	int len;

	camel_test_init(argc, argv);

	setup();

	camel_test_start("Simple memory-based content creation");

	/* test all ways of setting simple content for a message (i.e. memory based) */
	for (j=0;j<MAX_TEXTS;j++) {
		push("testing text number %d", j);
		text = texts[j].text;
		len = texts[j].len;
		for (i=0;i<SET_CONTENT_WAYS;i++) {
			push("create simple message %d", i);
			msg = test_message_create_simple();

			push("set simple content");
			test_message_set_content_simple((CamelMimePart *)msg, i, "text/plain", text, len);
			pull();

			push("compare original content");
			test_message_compare_content(camel_medium_get_content_object((CamelMedium *)msg), text, len);
			pull();

			push("save message to test1.msg");
			unlink("test1.msg");
			test_message_write_file(msg, "test1.msg");
			check_unref(msg, 1);
			pull();
	
			push("read from test1.msg");
			msg2 = test_message_read_file("test1.msg");
			pull();

			push("compare read with original content");
			test_message_compare_content(camel_medium_get_content_object((CamelMedium *)msg2), text, len);
			check_unref(msg2, 1);
			pull();

			unlink("test1.msg");
			pull();
		}
		pull();
	}

	camel_test_end();

	camel_test_start("Different encodings");
	for (j=0;j<MAX_TEXTS;j++) {
		push("testing text number %d", j);
		text = texts[j].text;
		len = texts[j].len;
		for (i=0;i<CAMEL_TRANSFER_NUM_ENCODINGS;i++) {
			
			push("test simple message, encoding %s", camel_transfer_encoding_to_string(i));
			msg = test_message_create_simple();

			push("set simple content");
			test_message_set_content_simple((CamelMimePart *)msg, 0, "text/plain", text, len);
			pull();

			camel_mime_part_set_encoding((CamelMimePart *)msg, i);

			push("save message to test1.msg");
			unlink("test1.msg");
			test_message_write_file(msg, "test1.msg");
			check_unref(msg, 1);
			pull();
	
			push("read from test1.msg");
			msg2 = test_message_read_file("test1.msg");
			pull();

			push("compare read with original content");
			test_message_compare_content(camel_medium_get_content_object((CamelMedium *)msg2), text, len);
			check_unref(msg2, 1);
			pull();

			unlink("test1.msg");
			pull();
		}
		pull();
	}
	camel_test_end();

	cleanup();

	return 0;
}
