
#include <glib.h>

#include <db.h>
#include <stdio.h>
#include <unicode.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include "ibex_internal.h"

#define d(x)

static signed char utf8_trans[] = {
	'A', 'A', 'A', 'A', 'A', 'A', -1, 'C', 'E', 'E', 'E', 'E', 'I', 'I',
	'I', 'I', -2, 'N', 'O', 'O', 'O', 'O', 'O', '*', 'O', 'U', 'U', 'U',
	'U', 'Y', -3, -4, 'a', 'a', 'a', 'a', 'a', 'a', -5, 'c', 'e', 'e',
	'e', 'e', 'i', 'i', 'i', 'i', -6, 'n', 'o', 'o', 'o', 'o', 'o', '/',
	'o', 'u', 'u', 'u', 'u', 'y', -7, 'y', 'A', 'a', 'A', 'a', 'A', 'a',
	'C', 'c', 'C', 'c', 'C', 'c', 'C', 'c', 'D', 'd', 'D', 'd', 'E', 'e',
	'E', 'e', 'E', 'e', 'E', 'e', 'E', 'e', 'G', 'g', 'G', 'g', 'G', 'g',
	'G', 'g', 'H', 'h', 'H', 'h', 'I', 'i', 'I', 'i', 'I', 'i', 'I', 'i',
	'I', 'i', -8, -9, 'J', 'j', 'K', 'k', 'k', 'L', 'l', 'L', 'l', 'L',
	'l', 'L', 'l', 'L', 'l', 'N', 'n', 'N', 'n', 'N', 'n', 'n', -10, -11,
	'O', 'o', 'O', 'o', 'O', 'o', -12, -13, 'R', 'r', 'R', 'r', 'R', 'r',
	'S', 'r', 'S', 's', 'S', 's', 'S', 's', 'T', 't', 'T', 't', 'T', 't',
	'U', 'u', 'U', 'u', 'U', 'u', 'U', 'u', 'U', 'u', 'U', 'u', 'W', 'w',
	'Y', 'y', 'Y', 'Z', 'z', 'Z', 'z', 'Z', 'z', 's'
};

static char *utf8_long_trans[] = {
	"AE", "TH", "TH", "ss", "ae", "th", "th", "IJ", "ij",
	"NG", "ng", "OE", "oe"
};

/* This is a bit weird. It takes pointers to the start and end (actually
 * just past the end) of a UTF-8-encoded word, and a buffer at least 1
 * byte longer than the length of the word. It copies the word into the
 * buffer in all lowercase without accents, and splits up ligatures.
 * (Since any ligature would be a multi-byte character in UTF-8, splitting
 * them into two US-ASCII characters won't overrun the buffer.)
 *
 * It is not safe to call this routine with bad UTF-8.
 */
static void
ibex_normalise_word(char *start, char *end, char *buf)
{
	unsigned char *s, *d;
	unicode_char_t uc;

	s = (unsigned char *)start;
	d = (unsigned char *)buf;
	while (s < (unsigned char *)end) {
		if (*s < 0x80) {
			/* US-ASCII character: copy unless it's
			 * an apostrophe.
			 */
			if (*s != '\'')
				*d++ = tolower (*s);
			s++;
		} else {
			char *next = unicode_get_utf8 (s, &uc);
			if (uc >= 0xc0 && uc < 0xc0 + sizeof (utf8_trans)) {
				signed char ch = utf8_trans[uc - 0xc0];
				if (ch > 0)
					*d++ = tolower (ch);
				else {
					*d++ = tolower (utf8_long_trans[-ch - 1][0]);
					*d++ = tolower (utf8_long_trans[-ch - 1][1]);
				}
				s = next;
			} else {
				while (s < (unsigned char *)next)
					*d++ = *s++;
			}
		}
	}
	*d = '\0';
}

enum { IBEX_ALPHA, IBEX_NONALPHA, IBEX_INVALID, IBEX_INCOMPLETE };

/* This incorporates parts of libunicode, because there's no way to
 * force libunicode to not read past a certain point.
 */
static int
utf8_category (char *sp, char **snp, char *send)
{
	unsigned char *p = (unsigned char *)sp, **np = (unsigned char **)snp;
	unsigned char *end = (unsigned char *)send;

	if (isascii (*p)) {
		*np = p + 1;
		if (isalpha (*p) || *p == '\'')
			return IBEX_ALPHA;
		return IBEX_NONALPHA;
	} else {
		unicode_char_t uc;
		int more;

		if ((*p & 0xe0) == 0xc0) {
			more = 1;
			uc = *p & 0x1f;
		} else if ((*p & 0xf0) == 0xe0) {
			more = 2;
			uc = *p & 0x0f;
		} else if ((*p & 0xf8) == 0xf0) {
			more = 3;
			uc = *p & 0x07;
		} else if ((*p & 0xfc) == 0xf8) {
			more = 4;
			uc = *p & 0x03;
		} else if ((*p & 0xfe) == 0xfc) {
			more = 5;
			uc = *p & 0x01;
		} else
			return IBEX_INVALID;

		if (p + more > end)
			return IBEX_INCOMPLETE;

		while (more--) {
			if ((*++p & 0xc0) != 0x80)
				return IBEX_INVALID;
			uc <<= 6;
			uc |= *p & 0x3f;
		}

		*np = p + 1;
		if (unicode_isalpha (uc))
			return IBEX_ALPHA;
		else
			return IBEX_NONALPHA;
	}
}

/**
 * ibex_index_buffer: the lowest-level ibex indexing interface
 * @ib: an ibex
 * @name: the name of the file being indexed
 * @buffer: a buffer containing data from the file
 * @len: the length of @buffer
 * @unread: an output argument containing the number of unread bytes
 *
 * This routine indexes up to @len bytes from @buffer into @ib.
 * If @unread is NULL, the indexer assumes that the buffer ends on a
 * word boundary, and will index all the way to the end of the
 * buffer. If @unread is not NULL, and the buffer ends with an
 * alphabetic character, the indexer will assume that the buffer has
 * been cut off in the middle of a word, and return the number of
 * un-indexed bytes at the end of the buffer in *@unread. The caller
 * should then read in more data through whatever means it has
 * and pass in the unread bytes from the original buffer, followed
 * by the new data, on its next call.
 *
 * Return value: 0 on success, -1 on failure.
 **/
int
ibex_index_buffer (ibex *ib, char *name, char *buffer, size_t len, size_t *unread)
{
	char *p, *q, *nq, *end, *word;
	int wordsiz, cat;
	GHashTable *words = g_hash_table_new(g_str_hash, g_str_equal);
	GPtrArray *wordlist = g_ptr_array_new();
	int i, ret=-1;

	if (unread)
		*unread = 0;

	end = buffer + len;
	wordsiz = 20;
	word = g_malloc (wordsiz);

	p = buffer;
	while (p < end) {
		while (p < end) {
			cat = utf8_category (p, &q, end);
			if (cat != IBEX_NONALPHA)
				break;
			p = q;
		}
		if (p == end) {
			goto done;
		} else if (cat == IBEX_INVALID) {
			goto error;
		} else if (cat == IBEX_INCOMPLETE)
			q = end;

		while (q < end) {
			cat = utf8_category (q, &nq, end);
			if (cat != IBEX_ALPHA)
				break;
			q = nq;
		}
		if (cat == IBEX_INVALID ||
		    (cat == IBEX_INCOMPLETE && !unread)) {
			goto error;
		} else if (cat == IBEX_INCOMPLETE || (q == end && unread)) {
			*unread = end - p;
			goto done;
		}

		if (wordsiz < q - p + 1) {
			wordsiz = q - p + 1;
			word = g_realloc (word, wordsiz);
		}
		ibex_normalise_word (p, q, word);
		if (word[0]) {
			if (g_hash_table_lookup(words, word) == 0) {
				char *newword = g_strdup(word);
				g_ptr_array_add(wordlist, newword);
				g_hash_table_insert(words, newword, name);
			}
		}
		p = q;
	}
done:
	d(printf("name %s count %d size %d\n", name, wordlist->len, len));
	ib->words->klass->add_list(ib->words, name, wordlist);
	ret = 0;
error:
	for (i=0;i<wordlist->len;i++)
		g_free(wordlist->pdata[i]);
	g_ptr_array_free(wordlist, TRUE);
	g_hash_table_destroy(words);
	g_free (word);
	return ret;
}


ibex *ibex_open (char *file, int flags, int mode)
{
	ibex *ib;

	ib = g_malloc0(sizeof(*ib));
	ib->blocks = ibex_block_cache_open(file, flags, mode);
	if (ib->blocks == 0) {
		g_warning("create: Error occured?: %s\n", strerror(errno));
		g_free(ib);
		return NULL;
	}
	/* FIXME: the blockcache or the wordindex needs to manage the other one */
	ib->words = ib->blocks->words;

	return ib;
}

int ibex_save (ibex *ib)
{
	printf("syncing database\n");
	ib->words->klass->sync(ib->words);
	/* FIXME: some return */
	ibex_block_cache_sync(ib->blocks);
	return 0;
}

int ibex_close (ibex *ib)
{
	int ret = 0;

	printf("closing database\n");

	ib->words->klass->close(ib->words);
	ibex_block_cache_close(ib->blocks);
	g_free(ib);
	return ret;
}

void ibex_unindex (ibex *ib, char *name)
{
	d(printf("trying to unindex '%s'\n", name));
	ib->words->klass->unindex_name(ib->words, name);
}

GPtrArray *ibex_find (ibex *ib, char *word)
{
	char *normal;
	int len;

	len = strlen(word);
	normal = alloca(len+1);
	ibex_normalise_word(word, word+len, normal);
	return ib->words->klass->find(ib->words, normal);
}

gboolean ibex_find_name (ibex *ib, char *name, char *word)
{
	char *normal;
	int len;

	len = strlen(word);
	normal = alloca(len+1);
	ibex_normalise_word(word, word+len, normal);
	return ib->words->klass->find_name(ib->words, name, normal);
}

gboolean ibex_contains_name(ibex *ib, char *name)
{
	return ib->words->klass->contains_name(ib->words, name);
}
