/*
	stream tests

	todo: do we need a seek test that seeks beyond the eos, writes,
		then reads and checks for 0's in the space?
*/

#include <string.h>

#include "camel/camel-stream.h"
#include "camel/camel-seekable-stream.h"
#include "camel/camel-seekable-substream.h"

#include "streams.h"

#include "camel-test.h"

static char teststring[] = "\xaa\x55\xc0\x0c\xff\x00";
static char testbuf[10240];

/* pass in an empty read/write stream */
void
test_stream_seekable_writepart(CamelSeekableStream *s)
{
	off_t end;
	int i;

	push("seekable stream test, writing ");

	check(camel_seekable_stream_tell(s) == 0);
	check(camel_seekable_stream_seek(s, 0, CAMEL_STREAM_SET) == 0);
	check(camel_seekable_stream_tell(s) == 0);

	check(camel_stream_write(CAMEL_STREAM(s), "", 0) == 0);
	check(camel_seekable_stream_tell(s) == 0);
	check(camel_stream_write(CAMEL_STREAM(s), "\n", 1) == 1);
	check(camel_seekable_stream_tell(s) == 1);

	for (i=0;i<10240;i++) {
		check(camel_stream_write(CAMEL_STREAM(s), teststring, sizeof(teststring)) == sizeof(teststring));
		check(camel_seekable_stream_tell(s) == 1 + (i+1)*sizeof(teststring));
	}
	end = 10240*sizeof(teststring)+1;

	check_msg(camel_seekable_stream_seek(s, 0, CAMEL_STREAM_END) == end, "seek =%d end = %d",
		  camel_seekable_stream_seek(s, 0, CAMEL_STREAM_END), end);

	check(camel_seekable_stream_seek(s, 0, CAMEL_STREAM_END) == end);
	check(camel_seekable_stream_tell(s) == end);
	/* need to read 0 first to set eos */
	check(camel_stream_read(CAMEL_STREAM(s), testbuf, 10240) == 0);
	check(camel_stream_eos(CAMEL_STREAM(s)));

	pull();
}

void
test_stream_seekable_readpart(CamelSeekableStream *s)
{
	off_t off, new, end;
	int i, j;

	push("seekable stream test, re-reading");

	end = 10240*sizeof(teststring)+1;

	check(camel_seekable_stream_seek(s, 0, CAMEL_STREAM_SET) == 0);
	check(camel_seekable_stream_tell(s) == 0);
	check(!camel_stream_eos(CAMEL_STREAM(s)));

	off = 0;
	for (i=0;i<1024;i++) {

		new = i*3;

		/* exercise all seek methods */
		switch(i % 3) {
		case 0:
			check(camel_seekable_stream_seek(s, new, CAMEL_STREAM_SET) == new);
			break;
		case 1:
			check(camel_seekable_stream_seek(s, new-off, CAMEL_STREAM_CUR) == new);
			break;
		case 2:
			check(camel_seekable_stream_seek(s, new-end, CAMEL_STREAM_END) == new);
			break;
		}
		check(camel_seekable_stream_tell(s) == new);

		check(camel_stream_read(CAMEL_STREAM(s), testbuf, i*3) == i*3);
		for (j=0;j<i*3;j++) {
			int k = new + j;

			if (k==0) {
				check(testbuf[j] == '\n');
			} else {
				check(testbuf[j] == teststring[(k-1) % sizeof(teststring)]);
			}
		}
		off = new+i*3;
	}

	/* verify end-of-file behaviour */
	check(camel_seekable_stream_seek(s, -1, CAMEL_STREAM_END) == end-1);
	check(camel_seekable_stream_tell(s) == end-1);

	check(camel_stream_read(CAMEL_STREAM(s), testbuf, 10240) == 1);
	check(testbuf[0] == teststring[sizeof(teststring)-1]);

	check(camel_stream_read(CAMEL_STREAM(s), testbuf, 10240) == 0);
	check(camel_seekable_stream_seek(s, 0, CAMEL_STREAM_CUR) == end);
	check(camel_seekable_stream_tell(s) == end);
	check(camel_stream_eos(CAMEL_STREAM(s)));

	pull();
}

/*
  0 = write to the substream
  1 = write to the parent stream at the right spot
*/
void
test_seekable_substream_writepart(CamelStream *s, int type)
{
	CamelSeekableStream *ss = (CamelSeekableStream *)s;
	CamelSeekableSubstream *sus = (CamelSeekableSubstream *)s;
	CamelSeekableStream *sp = sus->parent_stream;
	int i, len;

	push("writing substream, type %d", type);

	if (type == 1) {
		check(camel_seekable_stream_seek(sp, ss->bound_start, CAMEL_STREAM_SET) == ss->bound_start);
		s = (CamelStream *)sp;
	} else {
		check(camel_seekable_stream_tell(ss) == ss->bound_start);
		check(camel_seekable_stream_seek(ss, 0, CAMEL_STREAM_SET) == ss->bound_start);
	}

	check(camel_seekable_stream_tell(CAMEL_SEEKABLE_STREAM(s)) == ss->bound_start);

	check(camel_stream_write(s, "", 0) == 0);
	check(camel_seekable_stream_tell(CAMEL_SEEKABLE_STREAM(s)) == ss->bound_start);

	/* fill up the bounds with writes */
	if (ss->bound_end != CAMEL_STREAM_UNBOUND) {
		for (i=0;i<(ss->bound_end-ss->bound_start)/sizeof(teststring);i++) {
			check(camel_stream_write(s, teststring, sizeof(teststring)) == sizeof(teststring));
			check(camel_seekable_stream_tell(CAMEL_SEEKABLE_STREAM(s)) == ss->bound_start + (i+1)*sizeof(teststring));
		}
		len = (ss->bound_end-ss->bound_start) % sizeof(teststring);
		check(camel_stream_write(s, teststring, len) == len);
		check(camel_seekable_stream_tell(CAMEL_SEEKABLE_STREAM(s)) == ss->bound_end);
		if (type == 0) {
			check(camel_stream_write(s, teststring, sizeof(teststring)) == 0);
			check(camel_stream_eos(s));
			check(camel_seekable_stream_tell(CAMEL_SEEKABLE_STREAM(s)) == ss->bound_end);
		}
	} else {
		/* just 10K */
		for (i=0;i<10240;i++) {
			check(camel_stream_write(CAMEL_STREAM(s), teststring, sizeof(teststring)) == sizeof(teststring));
			check(camel_seekable_stream_tell(CAMEL_SEEKABLE_STREAM(s)) == ss->bound_start + (i+1)*sizeof(teststring));
		}

		/* we can't really verify any end length here */
	}

	pull();
}

void
test_seekable_substream_readpart(CamelStream *s)
{
	CamelSeekableStream *ss = (CamelSeekableStream *)s;
	CamelSeekableSubstream *sus = (CamelSeekableSubstream *)s;
	CamelSeekableStream *sp = sus->parent_stream;
	int i, len;

	push("reading substream");

	check(camel_seekable_stream_seek(ss, 0, CAMEL_STREAM_SET) == ss->bound_start);
	check(camel_seekable_stream_tell(ss) == ss->bound_start);

	check(camel_seekable_stream_seek(sp, ss->bound_start, CAMEL_STREAM_SET) == ss->bound_start);
	check(camel_seekable_stream_tell(sp) == ss->bound_start);

	/* check writes, cross check with parent stream */
	if (ss->bound_end != CAMEL_STREAM_UNBOUND) {
		for (i=0;i<(ss->bound_end-ss->bound_start)/sizeof(teststring);i++) {
			check(camel_stream_read(s, testbuf, sizeof(teststring)) == sizeof(teststring));
			check(memcmp(testbuf, teststring, sizeof(teststring)) == 0);
			check(camel_seekable_stream_tell(ss) == ss->bound_start + (i+1)*sizeof(teststring));

			/* yeah great, the substreams affect the seek ... */
			check(camel_seekable_stream_seek(sp, ss->bound_start + (i)*sizeof(teststring), CAMEL_STREAM_SET) == ss->bound_start + i*sizeof(teststring));
			check(camel_stream_read(CAMEL_STREAM(sp), testbuf, sizeof(teststring)) == sizeof(teststring));
			check(memcmp(testbuf, teststring, sizeof(teststring)) == 0);
			check(camel_seekable_stream_tell(sp) == ss->bound_start + (i+1)*sizeof(teststring));
		}
		len = (ss->bound_end-ss->bound_start) % sizeof(teststring);
		check(camel_stream_read(s, testbuf, len) == len);
		check(memcmp(testbuf, teststring, len) == 0);

		check(camel_seekable_stream_seek(sp, ss->bound_end - len, CAMEL_STREAM_SET) == ss->bound_end - len);
		check(camel_stream_read(CAMEL_STREAM(sp), testbuf, len) == len);
		check(memcmp(testbuf, teststring, len) == 0);

		check(camel_stream_eos(s));
		check(camel_seekable_stream_tell(ss) == ss->bound_end);
		check(camel_seekable_stream_tell(sp) == ss->bound_end);
		check(camel_stream_read(s, testbuf, 1024) == 0);
		check(camel_seekable_stream_tell(ss) == ss->bound_end);
		check(camel_seekable_stream_tell(sp) == ss->bound_end);
		check(camel_stream_eos(s));
	} else {
		/* just 10K */
		for (i=0;i<10240;i++) {
			check(camel_stream_read(s, testbuf, sizeof(teststring)) == sizeof(teststring));
			check(memcmp(testbuf, teststring, sizeof(teststring)) == 0);
			check(camel_seekable_stream_tell(ss) == ss->bound_start + (i+1)*sizeof(teststring));

			check(camel_seekable_stream_seek(sp, ss->bound_start + (i)*sizeof(teststring), CAMEL_STREAM_SET) == ss->bound_start + i*sizeof(teststring));
			check(camel_stream_read(CAMEL_STREAM(sp), testbuf, sizeof(teststring)) == sizeof(teststring));
			check(memcmp(testbuf, teststring, sizeof(teststring)) == 0);
			check(camel_seekable_stream_tell(sp) == ss->bound_start + (i+1)*sizeof(teststring));
		}

		/* unbound - we dont know the real length */
#if 0
		end = 10240*sizeof(teststring)+ss->bound_start;

		check(camel_seekable_stream_seek(ss, 0, CAMEL_STREAM_END) == end);
		check(camel_seekable_stream_tell(ss) == end);
		/* need to read 0 first to set eos */
		check(camel_stream_read(s, testbuf, 10240) == 0);
		check(camel_stream_eos(s));
#endif
	}

	pull();
}
