/*
  test ... camelstreammem */

#include "camel-test.h"
#include "streams.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "camel/camel-stream-mem.h"

int main(int argc, char **argv)
{
	CamelSeekableStream *ss = NULL;
	int i;
	int fd = -1;
	struct stat st;
	int size;
	char buffer[1024];
	GByteArray *ba;

	camel_test_init(argc, argv);

	camel_test_start("CamelStream mem, create, seek, read, write, eos");
	for (i=0;i<3;i++) {

		push("Creating stream using method %d", i);
		switch(i) {
		case 0:
			ss = (CamelSeekableStream *)camel_stream_mem_new();
			break;
		case 1:
			ba = g_byte_array_new();
			ss = (CamelSeekableStream *)camel_stream_mem_new_with_byte_array(ba);
			break;
		case 2:
			ss = (CamelSeekableStream *)camel_stream_mem_new_with_buffer("", 0);
			break;
		}
		check(ss != NULL);

		test_stream_seekable_writepart(ss);
		test_stream_seekable_readpart(ss);

		check_unref(ss, 1);
		pull();
	}

	camel_test_end();

	return 0;
}
