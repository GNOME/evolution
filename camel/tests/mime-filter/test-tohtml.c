/*
  test-html.c

  Test the CamelMimeFilterToHTML class
*/

#include <sys/stat.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include "camel-test.h"

#include <camel/camel-stream-fs.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-mime-filter-tohtml.h>

#define d(x) x

#define CHUNK_SIZE 4096

static void
test_filter(CamelMimeFilter *f, const char *inname, const char *outname)
{
	CamelStreamMem *in, *out;
	CamelStream *indisk, *outdisk, *filter;
	int id;

	camel_test_push("Data file `%s'", inname);

	camel_test_push("setup");

	indisk = camel_stream_fs_new_with_name(inname, O_RDONLY, 0);
	check(indisk);
	outdisk = camel_stream_fs_new_with_name(outname, O_RDONLY, 0);
	check(outdisk);

	out = (CamelStreamMem *)camel_stream_mem_new();
	check(camel_stream_write_to_stream(outdisk, (CamelStream *)out) > 0);

	camel_test_pull();

	camel_test_push("reading through filter stream");

	in = (CamelStreamMem *)camel_stream_mem_new();

	filter = (CamelStream *)camel_stream_filter_new_with_stream(indisk);
	check_count(indisk, 2);
	id = camel_stream_filter_add((CamelStreamFilter *)filter, f);
	check_count(f, 2);
		
	check(camel_stream_write_to_stream(filter, (CamelStream *)in) > 0);
	check_msg(in->buffer->len == out->buffer->len
		  && memcmp(in->buffer->data, out->buffer->data, in->buffer->len) == 0,
		  "Buffer content mismatch, %d != %d, in = '%.*s' != out = '%.*s'", in->buffer->len, out->buffer->len,
		  in->buffer->len, in->buffer->data, out->buffer->len, out->buffer->data);

	camel_test_pull();

	camel_stream_filter_remove((CamelStreamFilter *)filter, id);
	check_count(f, 1);
	camel_mime_filter_reset(f);

	check_unref(filter, 1);
	check_count(indisk, 1);
	check_count(f, 1);
	check_unref(in, 1);

	check(camel_stream_reset(indisk) == 0);

	camel_test_push("writing through filter stream");

	in = (CamelStreamMem *)camel_stream_mem_new();
	filter = (CamelStream *)camel_stream_filter_new_with_stream((CamelStream *)in);
	check_count(in, 2);
	id = camel_stream_filter_add((CamelStreamFilter *)filter, f);
	check_count(f, 2);

	check(camel_stream_write_to_stream(indisk, filter) > 0);
	check(camel_stream_flush(filter) == 0);
	check_msg(in->buffer->len == out->buffer->len
		  && memcmp(in->buffer->data, out->buffer->data, in->buffer->len) == 0,
		  "Buffer content mismatch, %d != %d, in = '%.*s' != out = '%.*s'", in->buffer->len, out->buffer->len,
		  in->buffer->len, in->buffer->data, out->buffer->len, out->buffer->data);

	camel_stream_filter_remove((CamelStreamFilter *)filter, id);
	check_unref(filter, 1);
	check_unref(in, 1);
	check_unref(indisk, 1);
	check_unref(outdisk, 1);
	check_unref(out, 1);

	camel_test_pull();

	camel_test_pull();
}

int 
main (int argc, char **argv)
{
	int i;

	camel_test_init(argc, argv);
	
	camel_test_start("HTML Stream filtering");

	for (i=0;i<100;i++) {
		char inname[32], outname[32];
		CamelMimeFilter *f;
		struct stat st;

		sprintf(inname, "data/html.%d.in", i);
		sprintf(outname, "data/html.%d.out", i);

		if (stat(inname, &st) == -1)
			break;

		f = camel_mime_filter_tohtml_new(CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);

		test_filter(f, inname, outname);

		check_unref(f, 1);
	}

	camel_test_end();

	return 0;
}
