/*
  test ... camelstreamfs */

#include "camel-test.h"
#include "streams.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "camel/camel-stream-fs.h"

int main(int argc, char **argv)
{
	CamelSeekableStream *ss = NULL;
	int i;
	int fd = -1;
	struct stat st;
	int size;
	char buffer[1024];

	camel_test_init(argc, argv);

	camel_test_start("CamelStream fs, open, seek, read, write, eos");
	for (i=0;i<2;i++) {

		(void)unlink("stream.txt");

		push("trying to open a nonexistant stream, method %d", i);
		switch(i) {
		case 0:
			ss = (CamelSeekableStream *)camel_stream_fs_new_with_name("stream.txt", O_RDWR, 0);
			break;
		case 1:
			fd = open("stream.txt", O_RDWR, 0);
			ss = (CamelSeekableStream *)camel_stream_fs_new_with_fd(fd);
			break;
		}
		check(ss == NULL && errno == ENOENT);
		check(stat("stream.txt", &st) == -1 && errno == ENOENT);
		pull();

		push("Creating stream using method %d", i);
		switch(i) {
		case 0:
			ss = (CamelSeekableStream *)camel_stream_fs_new_with_name("stream.txt", O_CREAT|O_RDWR|O_TRUNC, 0600);
			fd = ((CamelStreamFs *)ss)->fd;
			break;
		case 1:
			fd = open("stream.txt", O_CREAT|O_RDWR|O_TRUNC, 0600);
			ss = (CamelSeekableStream *)camel_stream_fs_new_with_fd(fd);
			break;
		}
		check(ss != NULL);
		check(stat("stream.txt", &st) == 0 && (st.st_mode&0777) == 0600 && S_ISREG(st.st_mode) && st.st_size == 0);
		pull();

		test_stream_seekable_writepart(ss);
		test_stream_seekable_readpart(ss);

		push("getting filesize");
		check(stat("stream.txt", &st) == 0 && (st.st_mode&0777) == 0600 && S_ISREG(st.st_mode));
		size = st.st_size;
		pull();

		push("checking close closes");
		check_unref(ss, 1);
		check(close(fd) == -1);
		pull();

		push("re-opening stream");
		switch(i) {
		case 0:
			ss = (CamelSeekableStream *)camel_stream_fs_new_with_name("stream.txt", O_RDWR, 0);
			fd = ((CamelStreamFs *)ss)->fd;
			break;
		case 1:
			fd = open("stream.txt", O_RDWR, 0);
			ss = (CamelSeekableStream *)camel_stream_fs_new_with_fd(fd);
			break;
		}
		check(ss != NULL);
		check(stat("stream.txt", &st) == 0 && (st.st_mode&0777) == 0600 && S_ISREG(st.st_mode) && st.st_size == size);

		test_stream_seekable_readpart(ss);

		check_unref(ss, 1);
		check(close(fd) == -1);
		pull();

		push("re-opening stream with truncate");
		switch(i) {
		case 0:
			ss = (CamelSeekableStream *)camel_stream_fs_new_with_name("stream.txt", O_RDWR|O_TRUNC, 0);
			fd = ((CamelStreamFs *)ss)->fd;
			break;
		case 1:
			fd = open("stream.txt", O_RDWR|O_TRUNC, 0);
			ss = (CamelSeekableStream *)camel_stream_fs_new_with_fd(fd);
			break;
		}
		check(ss != NULL);
		check(stat("stream.txt", &st) == 0 && (st.st_mode&0777) == 0600 && S_ISREG(st.st_mode) && st.st_size == 0);

		/* read has to return 0 before eos is set */
		check(camel_stream_read(CAMEL_STREAM(ss), buffer, 1) == 0);
		check(camel_stream_eos(CAMEL_STREAM(ss)));

		check_unref(ss, 1);
		check(close(fd) == -1);
		pull();
		
		(void)unlink("stream.txt");
	}

	camel_test_end();

	return 0;
}
