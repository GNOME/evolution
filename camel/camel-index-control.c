
/* Command to manually force a compression/dump of an index file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "libedataserver/e-msgport.h"
#include "libedataserver/e-memory.h"

#include "camel/camel-object.h"

#include "camel-text-index.h"

#include <stdio.h>

static void
do_usage(char *argv0)
{
	fprintf(stderr, "Usage: %s [ compress | dump | info ] file(s) ...\n", argv0);
	fprintf(stderr, " compress - compress (an) index file(s)\n");
	fprintf(stderr, " dump - dump (an) index file's content to stdout\n");
	fprintf(stderr, " info - dump summary info to stdout\n");
	exit(1);
}

static int
do_compress(int argc, char **argv)
{
	int i;
	CamelIndex *idx;

	for (i=2;i<argc;i++) {
		printf("Opening index file: %s\n", argv[i]);
		idx = (CamelIndex *)camel_text_index_new(argv[i], O_RDWR);
		if (idx) {
			printf(" Compressing ...\n");
			if (camel_index_compress(idx) == -1) {
				camel_object_unref((CamelObject *)idx);
				return 1;
			}
			camel_object_unref((CamelObject *)idx);
		} else {
			printf(" Failed: %s\n", strerror (errno));
			return 1;
		}
	}
	return 0;
}

static int
do_dump(int argc, char **argv)
{
	int i;
	CamelIndex *idx;

	for (i=2;i<argc;i++) {
		printf("Opening index file: %s\n", argv[i]);
		idx = (CamelIndex *)camel_text_index_new(argv[i], O_RDONLY);
		if (idx) {
			printf(" Dumping ...\n");
			camel_text_index_dump((CamelTextIndex *)idx);
			camel_object_unref((CamelObject *)idx);
		} else {
			printf(" Failed: %s\n", strerror (errno));
			return 1;
		}
	}
	return 0;
}

static int
do_info(int argc, char **argv)
{
	int i;
	CamelIndex *idx;

	for (i=2;i<argc;i++) {
		printf("Opening index file: %s\n", argv[i]);
		idx = (CamelIndex *)camel_text_index_new(argv[i], O_RDONLY);
		if (idx) {
			camel_text_index_info((CamelTextIndex *)idx);
			camel_object_unref((CamelObject *)idx);
		} else {
			printf(" Failed: %s\n", strerror (errno));
			return 0;
		}
	}
	return 1;
}

static int
do_check(int argc, char **argv)
{
	int i;
	CamelIndex *idx;

	for (i=2;i<argc;i++) {
		printf("Opening index file: %s\n", argv[i]);
		idx = (CamelIndex *)camel_text_index_new(argv[i], O_RDONLY);
		if (idx) {
			camel_text_index_validate((CamelTextIndex *)idx);
			camel_object_unref((CamelObject *)idx);
		} else {
			printf(" Failed: %s\n", strerror (errno));
			return 0;
		}
	}
	return 1;
}

static int do_perf(int argc, char **argv);

int main(int argc, char **argv)
{
	extern int camel_init(const char *certdb_dir, gboolean nss_init);

	if (argc<2)
		do_usage(argv[0]);

	g_thread_init(NULL);
	camel_init(NULL, 0);

	if (!strcmp(argv[1], "compress"))
		return do_compress(argc, argv);
	else if (!strcmp(argv[1], "dump"))
		return do_dump(argc, argv);
	else if (!strcmp(argv[1], "info"))
		return do_info(argc, argv);
	else if (!strcmp(argv[1], "check"))
		return do_check(argc, argv);
	else if (!strcmp(argv[1], "perf"))
		return do_perf(argc, argv);

	do_usage(argv[0]);
	return 1;
}

#include <sys/types.h>
#include <dirent.h>
#include "camel-stream-null.h"
#include "camel-stream-filter.h"
#include "camel-mime-filter-index.h"
#include "camel-stream-fs.h"

static int
do_perf(int argc, char **argv)
{
	CamelIndex *idx;
	DIR *dir;
	char *path = "/home/notzed/evolution/local/Inbox/mbox/cur";
	struct dirent *d;
	CamelStream *null, *filter, *stream;
	CamelMimeFilterIndex *filter_index;
	char *name;
	CamelIndexName *idn;

	dir = opendir(path);
	if (dir == NULL) {
		perror("open dir");
		return 1;
	}

	idx = (CamelIndex *)camel_text_index_new("/tmp/index", O_TRUNC|O_CREAT|O_RDWR);
	if (idx == NULL) {
		perror("open index");
		return 1;
	}

	null = camel_stream_null_new();
	filter = (CamelStream *)camel_stream_filter_new_with_stream(null);
	camel_object_unref((CamelObject *)null);
	filter_index = camel_mime_filter_index_new_index(idx);
	camel_stream_filter_add((CamelStreamFilter *)filter, (CamelMimeFilter *)filter_index);

	while ((d = readdir(dir))) {
		printf("indexing '%s'\n", d->d_name);

		idn = camel_index_add_name(idx, d->d_name);
		camel_mime_filter_index_set_name(filter_index, idn);
		name = g_strdup_printf("%s/%s", path, d->d_name);
		stream = camel_stream_fs_new_with_name(name, O_RDONLY, 0);
		camel_stream_write_to_stream(stream, filter);
		camel_object_unref((CamelObject *)stream);
		g_free(name);

		camel_index_write_name(idx, idn);
		camel_object_unref((CamelObject *)idn);
		camel_mime_filter_index_set_name(filter_index, NULL);
	}

	closedir(dir);

	camel_index_sync(idx);
	camel_object_unref((CamelObject *)idx);

	camel_object_unref((CamelObject *)filter);
	camel_object_unref((CamelObject *)filter_index);

	return 0;
}
