/*
  test-crlf.c

  Test the CamelMimeFilterCharset class
*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include "camel-test.h"

#include <camel/camel-stream-fs.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-mime-filter-charset.h>

#define d(x) x

#define CHUNK_SIZE 4096

int 
main (int argc, char **argv)
{
	ssize_t comp_progress, comp_correct_chunk, comp_filter_chunk;
	char comp_correct[CHUNK_SIZE], comp_filter[CHUNK_SIZE];
	CamelStream *source;
	CamelStream *correct;
	CamelStreamFilter *filter;
	CamelMimeFilter *f;
	struct dirent *dent;
	int i, test = 0;
	DIR *dir;
	
	camel_test_init(argc, argv);
	
	dir = opendir (SOURCEDIR);
	
	while ((dent = readdir (dir))) {
		char *outfile, *charset, *work;
		const char *ext;
		
		ext = strrchr (dent->d_name, '.');
		if (!(!strncmp (dent->d_name, "charset-", 8) && ext && !strcmp (ext, ".in")))
			continue;
		
		work = g_strdup_printf ("Charset filter, test case %d (%s)", test++, dent->d_name);
		camel_test_start (work);
		g_free (work);
		
		if (!(source = camel_stream_fs_new_with_name (dent->d_name, 0, O_RDONLY))) {
			camel_test_fail ("Failed to open input case in \"%s\"", dent->d_name);
			continue;
		}
		
		outfile = g_strdup_printf ("%.*s.out", ext - dent->d_name, dent->d_name);
		
		if (!(correct = camel_stream_fs_new_with_name (outfile, 0, O_RDONLY))) {
			camel_test_fail ("Failed to open correct output in \"%s\"", outfile);
			g_free (outfile);
			continue;
		}
		g_free (outfile);
		
		if (!(filter = camel_stream_filter_new_with_stream (CAMEL_STREAM (source)))) {
			camel_test_fail ("Couldn't create CamelStreamFilter??");
			continue;
		}
		
		charset = g_strdup (dent->d_name + 8);
		ext = strchr (charset, '.');
		*((char *) ext) = '\0';
		
		if (!(f = (CamelMimeFilter *) camel_mime_filter_charset_new_convert (charset, "UTF-8"))) {
			camel_test_fail ("Couldn't create CamelMimeFilterCharset??");
			g_free (charset);
			continue;
		}
		g_free (charset);
		
		camel_stream_filter_add (filter, f);
		camel_object_unref (f);
		
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
					camel_test_fail ("Chunks are different sizes: correct is %d, "
							 "filter is %d, %d bytes into stream",
							 comp_correct_chunk, comp_filter_chunk, comp_progress);
				}
				
				comp_filter_chunk += delta;
			}
			
			for (i = 0; i < comp_filter_chunk; i++) {
				if (comp_correct[i] != comp_filter[i]) {
					camel_test_fail ("Difference: correct is %c, filter is %c, "
							 "%d bytes into stream",
							 comp_correct[i], 
							 comp_filter[i],
							 comp_progress + i);
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
		camel_test_pull ();
		
		camel_test_end ();
	}
	
	closedir (dir);
	
	return 0;
}
