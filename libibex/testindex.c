/* Test code for libibex */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <glib.h>
#include "ibex_internal.h"

#ifdef ENABLE_THREADS
#include <pthread.h>
#endif

#define TIMEIT
/*#define DO_MCHECK*/

#ifdef TIMEIT
#include <sys/time.h>
#include <unistd.h>
#endif

#ifdef DO_MCHECK
#include <mcheck.h>
#endif

void word_index_mem_dump_info(struct _IBEXWord *idx);

/*
  The following is a routine to generate a Gaussian distribution
  of pseudo random numbers, to make the results a little more
  meaningful
*/

/* boxmuller.c           Implements the Polar form of the Box-Muller
                         Transformation

                      (c) Copyright 1994, Everett F. Carter Jr.
                          Permission is granted by the author to use
                          this software for any application provided this
                          copyright notice is preserved.

*/

#include <stdlib.h>
#include <math.h>

#define ranf() ((float)rand()/(float)RAND_MAX)

static float box_muller(float m, float s)      /* normal random variate generator */
{                                       /* mean m, standard deviation s */
        float x1, x2, w, y1;
        static float y2;
        static int use_last = 0;

        if (use_last)                   /* use value from previous call */
        {
                y1 = y2;
                use_last = 0;
        }
        else
        {
                do {
                        x1 = 2.0 * ranf() - 1.0;
                        x2 = 2.0 * ranf() - 1.0;
                        w = x1 * x1 + x2 * x2;
                } while ( w >= 1.0 );

                w = sqrt( (-2.0 * log( w ) ) / w );
                y1 = x1 * w;
                y2 = x2 * w;
                use_last = 1;
        }

        return( m + y1 * s );
}

/* gets a word from words, using m and s as distribution values */
static char *getword(GPtrArray *words, float m, float s)
{
	int index;

	do {
		index = (int)box_muller(m, s);
	} while (index<0 || index>=words->len);

	return words->pdata[index];
}

#ifdef ENABLE_THREADS
int do_read_words;

static void *
read_words(void *in)
{
	ibex *ib = in;
	GPtrArray *a;
	int lastlen = 0;
	int i;

	while (do_read_words) {
		a = ibex_find(ib, "joneses");
		if (a->len != lastlen) {
			printf("Found %d joneses!\n", a->len);
			lastlen = a->len;
		}
		for (i=0;i<a->len;i++)
			g_free(a->pdata[i]);
		g_ptr_array_free(a, TRUE);
	}
}
#endif



#ifdef DO_MCHECK
static int blowup(int status)
{
	switch(status) {
	case 1:
		printf("Double free failure\n");
		break;
	case 2:
		printf("Memory clobbered before block\n");
		break;
	case 3:
		printf("Memory clobbered after block\n");
		break;
	}
	abort();
	return status;
}
#endif

int main(int argc, char **argv)
{
	int i, j;
	GPtrArray *words;
	char line[256];
	int len;
	FILE *file;
	float m, s;
	ibex *ib;
	GString *buffer;
	int files;
	char *dict;
	int synccount;
#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;
#endif
#ifdef ENABLE_THREADS
	pthread_t id;
#endif

#ifdef DO_MCHECK
	mcheck(blowup);
#endif
	words = g_ptr_array_new();
	buffer = g_string_new("");

#ifdef ENABLE_THREADS
	g_thread_init(0);
#undef ENABLE_THREADS
#endif

#ifdef TIMEIT
	gettimeofday(&start, NULL);
#endif

	srand(0xABADF00D);

	synccount = 1000;
	files = 8000;
	dict = "/usr/dict/words";

	/* read words into an array */
	file = fopen(dict, "r");
	if (file == NULL) {
		fprintf(stderr, "Cannot open word file: %s: %s\n", dict, strerror(errno));
		return 1;
	}
	while (fgets(line, sizeof(line), file) != NULL) {
		len = strlen(line);
		if (len>0 && line[len-1]=='\n') {
			line[len-1]=0;
		}
		g_ptr_array_add(words, g_strdup(line));
	}
	fclose(file);
	
	fprintf(stderr, "Read %d words\n", words->len);

	/* *shrug* arbitrary values really */
	m = words->len/2;
	/* well, the average vocabulary of a mailbox is about 10K words */
	s = 1000.0;

	printf("mean is %f, s is %f\n", m, s);

	/* open ibex file */
	ib = ibex_open("test.ibex", O_RDWR|O_CREAT, 0600);
	if (ib == NULL) {
		perror("Creating ibex file\n");
		return 1;
	}

#ifdef ENABLE_THREADS
	do_read_words = 1;
	pthread_create(&id, 0, read_words, ib);
#endif
	printf("Adding %d files\n", files);

	
	/* simulate adding new words to a bunch of files */
	for (j=0;j<200000;j++) {
		/* always new name */
		char *name;
		/* something like 60 words in a typical message, say */
		int count = (int)box_muller(60.0, 20.0);
		int word = (int)box_muller(m, 4000);
		GPtrArray *a;
		static int lastlen = 0;

		/* random name */
		name = words->pdata[word % words->len];

		if (j%1000 == 0) {
			IBEX_LOCK(ib);
			word_index_mem_dump_info(ib->words);
			IBEX_UNLOCK(ib);
		}

		/* lookup word just to test lookup */
		a = ibex_find(ib, name);
		if (a) {
			for (i=0;i<a->len;i++)
				g_free(a->pdata[i]);
			g_ptr_array_free(a, TRUE);
		}

		/* half the time, remove items from the index */
		if (rand() < RAND_MAX/2) {
			ibex_unindex(ib, name);
		} else {
			/* cache the name info */
			ibex_contains_name(ib, name);

			/*printf("Adding %d words to '%s'\n", count, name);*/

			g_string_truncate(buffer, 0);

			/* build up the word buffer */
			for (i=0;i<count;i++) {
				if (i>0)
					g_string_append_c(buffer, ' ');
				g_string_append(buffer, getword(words, m, 2000));
			}

			/* and index it */
			ibex_index_buffer(ib, name, buffer->str, buffer->len, NULL);
		}


		a = ibex_find(ib, "joneses");
		if (a) {
			if (a->len != lastlen) {
				printf("Found %d joneses!\n", a->len);
				lastlen = a->len;
			}
			for (i=0;i<a->len;i++)
				g_free(a->pdata[i]);
			g_ptr_array_free(a, TRUE);
		}

		if (j%synccount == 0) {
			printf("Reloading index\n");
			IBEX_LOCK(ib);
			word_index_mem_dump_info(ib->words);
			IBEX_UNLOCK(ib);
#ifdef ENABLE_THREADS
			do_read_words = 0;
			pthread_join(id, 0);
#endif
			ibex_save(ib);
			ibex_close(ib);

			ib = ibex_open("test.ibex", O_RDWR|O_CREAT, 0600);
			IBEX_LOCK(ib);
			word_index_mem_dump_info(ib->words);
			IBEX_UNLOCK(ib);
#ifdef ENABLE_THREADS
			do_read_words = 1;
			pthread_create(&id, 0, read_words, ib);
#endif
		}

	}


	IBEX_LOCK(ib);
	word_index_mem_dump_info(ib->words);
	IBEX_UNLOCK(ib);

#ifdef ENABLE_THREADS
	do_read_words = 0;
	pthread_join(id, 0);
#endif

	ibex_close(ib);

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Total time taken %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

	return 0;
}

