/*
  test-crlf.c

  Test the CamelMimeFilterCanon class
*/

#include <stdio.h>
#include <string.h>

#include "camel-test.h"

#include <camel/camel-stream-fs.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-mime-filter-canon.h>

#define d(x) x

#define NUM_CASES 1
#define CHUNK_SIZE 4096

struct {
	int flags;
	char *in;
	char *out;
} tests[] = {
	{ CAMEL_MIME_FILTER_CANON_FROM|CAMEL_MIME_FILTER_CANON_CRLF,
	  "From \nRussia - with love.\n\n",
	  "=46rom \r\nRussia - with love.\r\n\r\n" },
	{ CAMEL_MIME_FILTER_CANON_FROM|CAMEL_MIME_FILTER_CANON_CRLF,
	  "From \r\nRussia - with love.\r\n\n",
	  "=46rom \r\nRussia - with love.\r\n\r\n" },
	{ CAMEL_MIME_FILTER_CANON_FROM|CAMEL_MIME_FILTER_CANON_CRLF,
	  "Tasmiania with fur    \nFrom",
	  "Tasmiania with fur    \r\nFrom" },
	{ CAMEL_MIME_FILTER_CANON_FROM,
	  "Tasmiania with fur    \nFrom",
	  "Tasmiania with fur    \nFrom" },
	{ CAMEL_MIME_FILTER_CANON_CRLF,
	  "Tasmiania with fur    \nFrom",
	  "Tasmiania with fur    \r\nFrom" },
	{ CAMEL_MIME_FILTER_CANON_FROM|CAMEL_MIME_FILTER_CANON_CRLF,
	  "Tasmiania with fur    \nFrom here",
	  "Tasmiania with fur    \r\n=46rom here" },
	{ CAMEL_MIME_FILTER_CANON_FROM|CAMEL_MIME_FILTER_CANON_CRLF|CAMEL_MIME_FILTER_CANON_STRIP,
	  "Tasmiania with fur    \nFrom here",
	  "Tasmiania with fur\r\n=46rom here" },
	{ CAMEL_MIME_FILTER_CANON_FROM|CAMEL_MIME_FILTER_CANON_CRLF|CAMEL_MIME_FILTER_CANON_STRIP,
	  "Tasmiania with fur    \nFrom here\n",
	  "Tasmiania with fur\r\n=46rom here\r\n" },
	{ CAMEL_MIME_FILTER_CANON_FROM|CAMEL_MIME_FILTER_CANON_CRLF|CAMEL_MIME_FILTER_CANON_STRIP,
	  "Tasmiania with fur    \nFrom here or there ? \n",
	  "Tasmiania with fur\r\n=46rom here or there ?\r\n" },
};

int 
main (int argc, char **argv)
{
	CamelStreamFilter *filter;
	CamelMimeFilter *sh;
	int i;
	
	camel_test_init(argc, argv);

	camel_test_start("canonicalisation filter tests");

	for (i=0;i<sizeof(tests)/sizeof(tests[0]);i++) {
		int step;

		camel_test_push("Data test %d '%s'\n", i, tests[i].in);

		/* try all write sizes */
		for (step=1;step<20;step++) {
			CamelStreamMem *out;
			char *p;

			camel_test_push("Chunk size %d\n", step);

			out = (CamelStreamMem *)camel_stream_mem_new();
			filter = camel_stream_filter_new_with_stream((CamelStream *)out);
			sh = camel_mime_filter_canon_new(tests[i].flags);
			check(camel_stream_filter_add(filter, sh) != -1);
			check_unref(sh, 2);

			p = tests[i].in;
			while (*p) {
				int w = MIN(strlen(p), step);

				check(camel_stream_write((CamelStream *)filter, p, w) == w);
				p += w;
			}
			camel_stream_flush((CamelStream *)filter);

			check_msg(out->buffer->len == strlen(tests[i].out), "Buffer length mismatch: expected %d got %d\n or '%s' got '%.*s'", strlen(tests[i].out), out->buffer->len, tests[i].out, out->buffer->len, out->buffer->data);
			check_msg(0 == memcmp(out->buffer->data, tests[i].out, out->buffer->len), "Buffer mismatch: expected '%s' got '%.*s'", tests[i].out, out->buffer->len, out->buffer->data);
			check_unref(filter, 1);
			check_unref(out, 1);

			camel_test_pull();
		}

		camel_test_pull();
	}

	camel_test_end();
	
	return 0;
}
