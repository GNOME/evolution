/*
  test ... camelseekablesubstream */

#include "camel-test.h"
#include "streams.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "camel/camel-stream-mem.h"
#include "camel/camel-stream-fs.h"
#include "camel/camel-seekable-substream.h"

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

struct {
	off_t lower, upper;
} ranges[] = {
	{ 3, 10241 },
	{ 0, 1024 },
	{ 0, 0 },
	{ 0, 1 },
	{ 0, 2 },
	{ 0, 3 },
	{ 0, 7 },
	{ 1, 8 },
	{ 1, 9 },
	{ 10245, 10300 },
	{ 0, CAMEL_STREAM_UNBOUND },
/*	{ 1, CAMEL_STREAM_UNBOUND },
	{ 2, CAMEL_STREAM_UNBOUND },
	{ 3, CAMEL_STREAM_UNBOUND },  these take too long to run
	{ 7, CAMEL_STREAM_UNBOUND },*/
	{ 10245, CAMEL_STREAM_UNBOUND },
};

int main(int argc, char **argv)
{
	CamelSeekableStream *ss = NULL;
	int i, j;
	CamelSeekableSubstream *sus, *sus2;

	camel_test_init(argc, argv);

	camel_test_start("CamelSeekableSubstream, mem backing");
	for (j=0;j<SEEKABLE_SUBSTREAM_WAYS;j++) {
		push("testing writing method %d", j);
		ss = (CamelSeekableStream *)camel_stream_mem_new();
		check(ss != NULL);
		for (i=0;i<ARRAY_LEN(ranges);i++) {
			push("stream subrange %d-%d", ranges[i].lower, ranges[i].upper);
			sus = (CamelSeekableSubstream *)camel_seekable_substream_new_with_seekable_stream_and_bounds(ss, ranges[i].lower, ranges[i].upper);
			check(sus != NULL);
			
			test_seekable_substream_writepart((CamelStream *)sus, j);
			test_seekable_substream_readpart((CamelStream *)sus);
			
			sus2 = (CamelSeekableSubstream *)camel_seekable_substream_new_with_seekable_stream_and_bounds(ss, ranges[i].lower, ranges[i].upper);
			check(sus2 != NULL);
			test_seekable_substream_readpart((CamelStream *)sus2);
			
			check_unref(sus, 1);
			check_unref(sus2, 1);
			pull();
		}
		check_unref(ss, 1);
		pull();
	}

	camel_test_end();

	(void)unlink("stream.txt");

	camel_test_start("CamelSeekableSubstream, file backing");
	for (j=0;j<SEEKABLE_SUBSTREAM_WAYS;j++) {
		push("testing writing method %d", j);
		ss = (CamelSeekableStream *)camel_stream_fs_new_with_name("stream.txt", O_RDWR|O_CREAT|O_TRUNC, 0600);
		check(ss != NULL);
		for (i=0;i<ARRAY_LEN(ranges);i++) {
			push("stream subrange %d-%d", ranges[i].lower, ranges[i].upper);
			sus = (CamelSeekableSubstream *)camel_seekable_substream_new_with_seekable_stream_and_bounds(ss, ranges[i].lower, ranges[i].upper);
			check(sus != NULL);

			test_seekable_substream_writepart((CamelStream *)sus, j);
			test_seekable_substream_readpart((CamelStream *)sus);

			sus2 = (CamelSeekableSubstream *)camel_seekable_substream_new_with_seekable_stream_and_bounds(ss, ranges[i].lower, ranges[i].upper);
			check(sus2 != NULL);
			test_seekable_substream_readpart((CamelStream *)sus2);
			
			check_unref(sus, 1);
			check_unref(sus2, 1);
			pull();
		}
		check_unref(ss, 1);
		(void)unlink("stream.txt");
		pull();
	}

	camel_test_end();

	return 0;
}
