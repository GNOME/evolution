#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libedataserver/e-sexp.h>
#include <camel/camel-exception.h>
#include <camel/camel-search-private.h>

#include "camel-test.h"

/* TODO: should put utf8 stuff here too */

static struct {
	char *word;
	int count;
	struct {
		char *word;
		int type;
	} splits[5];
} split_tests[] = {
	{ "simple", 1, { { "simple", CAMEL_SEARCH_WORD_SIMPLE } } },
	{ "two words", 2, { { "two", CAMEL_SEARCH_WORD_SIMPLE }, {"words" , CAMEL_SEARCH_WORD_SIMPLE } } },
	{ "compl;ex", 1, { { "compl;ex", CAMEL_SEARCH_WORD_COMPLEX } } },
	{ "compl;ex simple", 2, { { "compl;ex", CAMEL_SEARCH_WORD_COMPLEX} , {"simple", CAMEL_SEARCH_WORD_SIMPLE} } },
	{ "\"quoted\"", 1, { { "quoted", CAMEL_SEARCH_WORD_SIMPLE } } },
	{ "\"quoted double\"", 1, { { "quoted double", CAMEL_SEARCH_WORD_COMPLEX } } },
	{ "\"quoted double\" compl;ex", 2, { { "quoted double", CAMEL_SEARCH_WORD_COMPLEX }, { "compl;ex", CAMEL_SEARCH_WORD_COMPLEX } } },
	{ "\"quoted double \\\" escaped\"", 1, { { "quoted double \" escaped", CAMEL_SEARCH_WORD_COMPLEX } } },
	{ "\"quoted\\\"double\" \\\" escaped\\\"", 3, { { "quoted\"double", CAMEL_SEARCH_WORD_COMPLEX }, {"\"", CAMEL_SEARCH_WORD_COMPLEX}, { "escaped\"", CAMEL_SEARCH_WORD_COMPLEX } } },
	{ "\\\"escaped", 1, { { "\"escaped", CAMEL_SEARCH_WORD_COMPLEX } } },

};
#define SPLIT_LENGTH (sizeof(split_tests)/sizeof(split_tests[0]))

static struct {
	char *word;
	int count;
	struct {
		char *word;
		int type;
	} splits[5];
} simple_tests[] = {
	{ "simple", 1, { {"simple", CAMEL_SEARCH_WORD_SIMPLE } } },
	{ "simpleCaSe", 1, { { "simplecase", CAMEL_SEARCH_WORD_SIMPLE } } },
	{ "two words", 2, { { "two", CAMEL_SEARCH_WORD_SIMPLE }, { "words", CAMEL_SEARCH_WORD_SIMPLE } } },
	{ "two wordscAsE", 2, { { "two", CAMEL_SEARCH_WORD_SIMPLE} ,  { "wordscase", CAMEL_SEARCH_WORD_SIMPLE } } },
	{ "compl;ex", 2, { { "compl", CAMEL_SEARCH_WORD_SIMPLE }, { "ex", CAMEL_SEARCH_WORD_SIMPLE } } },
	{ "compl;ex simple", 3, { { "compl", CAMEL_SEARCH_WORD_SIMPLE }, { "ex", CAMEL_SEARCH_WORD_SIMPLE }, { "simple", CAMEL_SEARCH_WORD_SIMPLE } } },
	{ "\"quoted compl;ex\" simple", 4, { { "quoted", CAMEL_SEARCH_WORD_SIMPLE}, { "compl", CAMEL_SEARCH_WORD_SIMPLE }, { "ex", CAMEL_SEARCH_WORD_SIMPLE }, { "simple", CAMEL_SEARCH_WORD_SIMPLE } } },
	{ "\\\" \"quoted\"compl;ex\" simple", 4, { { "quoted", CAMEL_SEARCH_WORD_SIMPLE}, { "compl", CAMEL_SEARCH_WORD_SIMPLE }, { "ex", CAMEL_SEARCH_WORD_SIMPLE }, { "simple", CAMEL_SEARCH_WORD_SIMPLE } } },
};

#define SIMPLE_LENGTH (sizeof(simple_tests)/sizeof(simple_tests[0]))

int
main (int argc, char **argv)
{
	int i, j;
	struct _camel_search_words *words, *tmp;

	camel_test_init(argc, argv);

	camel_test_start("Search splitting");

	for (i=0; i<SPLIT_LENGTH; i++) {
		camel_test_push("split %d '%s'", i, split_tests[i].word);

		words = camel_search_words_split(split_tests[i].word);
		check(words != NULL);
		check_msg(words->len == split_tests[i].count, "words->len = %d, count = %d", words->len, split_tests[i].count);

		for (j=0;j<words->len;j++) {
			check_msg(strcmp(split_tests[i].splits[j].word, words->words[j]->word) == 0,
				  "'%s' != '%s'", split_tests[i].splits[j].word, words->words[j]->word);
			check(split_tests[i].splits[j].type == words->words[j]->type);
		}

		camel_search_words_free(words);
		camel_test_pull();
	}

	camel_test_end();

	camel_test_start("Search splitting - simple");

	for (i=0; i<SIMPLE_LENGTH; i++) {
		camel_test_push("simple split %d '%s'", i, simple_tests[i].word);

		tmp = camel_search_words_split(simple_tests[i].word);
		check(tmp != NULL);

		words = camel_search_words_simple(tmp);
		check(words != NULL);
		check_msg(words->len == simple_tests[i].count, "words->len = %d, count = %d", words->len, simple_tests[i].count);

		for (j=0;j<words->len;j++) {
			check_msg(strcmp(simple_tests[i].splits[j].word, words->words[j]->word) == 0,
				  "'%s' != '%s'", simple_tests[i].splits[j].word, words->words[j]->word);
			check(simple_tests[i].splits[j].type == words->words[j]->type);
		}

		camel_search_words_free(words);
		camel_search_words_free(tmp);
		camel_test_pull();
	}

	camel_test_end();

	return 0;
}
