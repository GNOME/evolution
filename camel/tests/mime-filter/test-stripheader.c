/*
  test-stripheader.c

  Test the CamelMimeFilterStripHeader class
*/

#include <stdio.h>
#include <string.h>

#include "camel-test.h"

#include <camel/camel-stream-fs.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-mime-filter-stripheader.h>

#define d(x) x

#define NUM_CASES 6
#define CHUNK_SIZE 32

int 
main(int argc, char **argv)
{
	CamelStream *source;
	CamelStream *correct;
	CamelStreamFilter *filter;
	CamelMimeFilter *sh;
	gchar *work;
	int i;
	ssize_t comp_progress, comp_correct_chunk, comp_filter_chunk;
	int comp_i;
	char comp_correct[CHUNK_SIZE], comp_filter[CHUNK_SIZE];

	camel_test_init(argc, argv);

	for (i = 0; i < NUM_CASES; i++) {
		work = g_strdup_printf ("Header stripping filter, test case %d", i);
		camel_test_start (work);
		g_free (work);

		camel_test_push ("Initializing objects");
		work = g_strdup_printf ("%s/stripheader-%d.in", SOURCEDIR, i + 1);
		source = camel_stream_fs_new_with_name (work, 0, O_RDONLY);
		if (!source) {
			camel_test_fail ("Failed to open input case in \"%s\"", work);
			g_free (work);
			continue;
		}
		g_free (work);

		work = g_strdup_printf ("%s/stripheader-%d.out", SOURCEDIR, i + 1);
		correct = camel_stream_fs_new_with_name (work, 0, O_RDONLY);
		if (!correct) {
			camel_test_fail ("Failed to open correct output in \"%s\"", work);
			g_free (work);
			continue;
		}
		g_free (work);

		filter = camel_stream_filter_new_with_stream (CAMEL_STREAM (source));
		if (!filter) {
			camel_test_fail ("Couldn't create CamelStreamFilter??");
			continue;
		}

		sh = camel_mime_filter_stripheader_new ("Stripped");
		if (!sh) {
			camel_test_fail ("Couldn't create CamelMimeFilterStripHeader??");
			continue;
		}

		camel_stream_filter_add (filter, sh);
		camel_test_pull ();

		camel_test_push ("Running filter and comparing to correct result");

		comp_progress = 0;

		while (1) {
			comp_correct_chunk = camel_stream_read (correct, comp_correct, CHUNK_SIZE);
			comp_filter_chunk = 0;

			if (comp_correct_chunk == 0)
				break;

			while (comp_filter_chunk < comp_correct_chunk) {
				ssize_t delta;

				delta = camel_stream_read (CAMEL_STREAM (filter), 
							   comp_filter + comp_filter_chunk, 
							   CHUNK_SIZE - comp_filter_chunk);

				if (delta == 0) {
					camel_test_fail ("Chunks are different sizes: correct is %d, filter is %d, %d bytes into stream",
							 comp_correct_chunk, comp_filter_chunk, comp_progress);
				}

				comp_filter_chunk += delta;
			}

			d(printf ("\n\nCORRECT: >>%.*s<<", comp_correct_chunk, comp_correct);)
			d(printf ("\nFILTER : >>%.*s<<\n", comp_filter_chunk, comp_filter);)

			for (comp_i = 0; comp_i < comp_filter_chunk; comp_i++) {
				if (comp_correct[comp_i] != comp_filter[comp_i]) {
					camel_test_fail ("Difference: correct is %c, filter is %c, %d bytes into stream",
							 comp_correct[comp_i], 
							 comp_filter[comp_i],
							 comp_progress + comp_i);
				}
			}

			comp_progress += comp_filter_chunk;
		}

		camel_test_pull ();

		/* inefficient */
		camel_test_push ("Cleaning up");
		camel_object_unref (CAMEL_OBJECT (filter));
		camel_object_unref (CAMEL_OBJECT (correct));
		camel_object_unref (CAMEL_OBJECT (source));
		camel_object_unref (CAMEL_OBJECT (sh));
		camel_test_pull ();

		camel_test_end();
	}

	return 0;
}
