
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include <stdio.h>
#include <gal/unicode/gunicode.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "ibex_internal.h"

#define d(x)
#define o(x) 

static void ibex_reset(ibex *ib);
static int close_backend(ibex *ib);

static struct _list ibex_list = { (struct _listnode *)&ibex_list.tail, 0, (struct _listnode *)&ibex_list.head };
static int ibex_open_init = 0;
static int ibex_open_threshold = IBEX_OPEN_THRESHOLD;

#ifdef ENABLE_THREADS
#include <pthread.h>
static pthread_mutex_t ibex_list_lock = PTHREAD_MUTEX_INITIALIZER;
int ibex_opened;		/* count of actually opened ibexe's */
#define IBEX_LIST_LOCK(ib) (pthread_mutex_lock(&ibex_list_lock))
#define IBEX_LIST_UNLOCK(ib) (pthread_mutex_unlock(&ibex_list_lock))
#else
#define IBEX_LIST_LOCK(ib) 
#define IBEX_LIST_UNLOCK(ib) 
#endif

/* check we are open properly */

/* TODO: return errors? */
static void ibex_use(ibex *ib)
{
	ibex *wb, *wn;

	/* always lock list then ibex */
	IBEX_LIST_LOCK(ib);
	IBEX_LOCK(ib);

	if (ib->blocks == NULL) {
		o(printf("Delayed opening ibex '%s', total = %d\n", ib->name, ibex_opened+1));

		ib->blocks = ibex_block_cache_open(ib->name, ib->flags, ib->mode);
		if (ib->blocks) {
			/* FIXME: the blockcache or the wordindex needs to manage the other one */
			ib->words = ib->blocks->words;		
		} else {
			ib->words = NULL;
			g_warning("ibex_use:open(): Error occured?: %s\n", strerror(errno));
		}
		if (ib->blocks != NULL)
			ibex_opened++;
	}

	ib->usecount++;

	/* this makes an 'lru' cache of used files */
	ibex_list_remove((struct _listnode *)ib);
	ibex_list_addtail(&ibex_list, (struct _listnode *)ib);

	IBEX_UNLOCK(ib);

	/* check env variable override for open threshold */
	if (!ibex_open_init) {
		char *limit;

		ibex_open_init = TRUE;
		limit = getenv("IBEX_OPEN_THRESHOLD");
		if (limit) {
			ibex_open_threshold = atoi(limit);
			if (ibex_open_threshold < IBEX_OPEN_THRESHOLD)
				ibex_open_threshold = IBEX_OPEN_THRESHOLD;
		}
	}

	/* check for other ibex's we can close now to not over-use fd's.
	   we can't do this first for locking issues */
	if (ibex_opened > ibex_open_threshold) {
		wb = (ibex *)ibex_list.head;
		wn = wb->next;
		while (wn) {
			IBEX_LOCK(wb);
			if (wb->usecount == 0 && wb->blocks != NULL) {
				o(printf("Forcing close of obex '%s', total = %d\n", wb->name, ibex_opened-1));
				close_backend(wb);
				IBEX_UNLOCK(wb);
				/* optimise the next scan? */
				/*ibex_list_remove((struct _listnode *)wb);
				  ibex_list_addtail(&ibex_list, (struct _listnode *)wb);*/
				ibex_opened--;
				break;
			}
			IBEX_UNLOCK(wb);
			wb = wn;
			wn = wn->next;
		}
	}

	IBEX_LIST_UNLOCK(ib);
}

static void ibex_unuse(ibex *ib)
{
	IBEX_LOCK(ib);
	ib->usecount--;
	IBEX_UNLOCK(ib);
}


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
	gunichar uc;

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
			char *next = g_utf8_next_char (s);
			uc = g_utf8_get_char (s);
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

static int
utf8_category (char *p, char **np, char *end)
{
	if (isascii ((unsigned char)*p)) {
		*np = p + 1;
		if (isalpha ((unsigned char)*p) || *p == '\'')
			return IBEX_ALPHA;
		return IBEX_NONALPHA;
	} else {
		gunichar uc;

		*np = g_utf8_find_next_char (p, end);
		if (!*np)
			return IBEX_INCOMPLETE;
		uc = g_utf8_get_char (p);
		if (uc == (gunichar) -1)
			return IBEX_INVALID;
		else if (g_unichar_isalpha (uc))
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
	char *p, *q, *nq, *end;
	char *word;
	int wordsiz, cat = 0;
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
	/* this weird word=NULL shit is to get rid of compiler warnings about clobbering with longjmp */
	g_free(word);
	word = NULL;

	ibex_use(ib);
	IBEX_LOCK(ib);

	if (ibex_block_cache_setjmp(ib->blocks) != 0) {
		printf("Error in indexing\n");
		ret = -1;
		ibex_reset(ib);
		word = NULL;	/* here too */
	} else {
		word = NULL;	/* ... and here */
		d(printf("name %s count %d size %d\n", name, wordlist->len, len));
		if (!ib->predone) {
			ib->words->klass->index_pre(ib->words);
			ib->predone = TRUE;
		}
		ib->words->klass->add_list(ib->words, name, wordlist);
		ret = 0;
	}

	IBEX_UNLOCK(ib);
	ibex_unuse(ib);

error:
	for (i=0;i<wordlist->len;i++)
		g_free(wordlist->pdata[i]);
	g_ptr_array_free(wordlist, TRUE);
	g_hash_table_destroy(words);
	g_free(word);
	return ret;
}


/**
 * ibex_open:
 * @file: 
 * @flags: 
 * @mode: 
 * 
 * Open a new ibex file.  file, flags, and mode as for open(2)
 * 
 * Return value: A new ibex, or NULL on failure.
 **/
ibex *ibex_open (char *file, int flags, int mode)
{
	ibex *ib;

	ib = g_malloc0(sizeof(*ib));
	ib->blocks = NULL;
	ib->usecount = 0;
#if 0
	ib->blocks = ibex_block_cache_open(file, flags, mode);
	if (ib->blocks == 0) {
		g_warning("create: Error occured?: %s\n", strerror(errno));
		g_free(ib);
		return NULL;
	}
	/* FIXME: the blockcache or the wordindex needs to manage the other one */
	ib->words = ib->blocks->words;
#endif
	ib->name = g_strdup(file);
	ib->flags = flags;
	ib->mode = mode;

#ifdef ENABLE_THREADS
	ib->lock = g_mutex_new();
#endif

	IBEX_LIST_LOCK(ib);
	ibex_list_addtail(&ibex_list, (struct _listnode *)ib);
	IBEX_LIST_UNLOCK(ib);

	return ib;
}

/**
 * ibex_save:
 * @ib: 
 * 
 * Save (sync) an ibex file to disk.
 * 
 * Return value: -1 on failure.
 **/
int ibex_save (ibex *ib)
{
	int ret;

	d(printf("syncing database\n"));

	ibex_use(ib);
	IBEX_LOCK(ib);

	if (ibex_block_cache_setjmp(ib->blocks) != 0) {
		ibex_reset(ib);
		printf("Error saving\n");
		ret = -1;
	} else {
		if (ib->predone) {
			ib->words->klass->index_post(ib->words);
			ib->predone = FALSE;
		}
		ib->words->klass->sync(ib->words);
		/* FIXME: some return */
		ibex_block_cache_sync(ib->blocks);
		ret = 0;
	}

	IBEX_UNLOCK(ib);
	ibex_unuse(ib);

	return ret;
}

static int
close_backend(ibex *ib)
{
	int ret;

	if (ib->blocks == 0)
		return 0;

	if (ibex_block_cache_setjmp(ib->blocks) != 0) {
		printf("Error closing!\n");
		ret = -1;
	} else {
		if (ib->predone) {
			ib->words->klass->index_post(ib->words);
			ib->predone = FALSE;
		}

		ib->words->klass->close(ib->words);
		/* FIXME: return */
		ibex_block_cache_close(ib->blocks);
		ib->blocks = NULL;
		ret = 0;
	}

	return ret;
}

/* close/reopen the ibex file, assumes we have lock */
static void
ibex_reset(ibex *ib)
{
	g_warning("resetting ibex file");

	close_backend(ib);

	ib->blocks = ibex_block_cache_open(ib->name, ib->flags, ib->mode);
	if (ib->blocks == 0) {
		g_warning("ibex_reset create: Error occured?: %s\n", strerror(errno));
	}
	/* FIXME: the blockcache or the wordindex needs to manage the other one */
	ib->words = ib->blocks->words;
}

/**
 * ibex_close:
 * @ib: 
 * 
 * Close (and save) an ibex file, restoring all resources used.
 * 
 * Return value: -1 on error.  In either case, ibex is no longer
 * defined afterwards.
 **/
int ibex_close (ibex *ib)
{
	int ret;

	d(printf("closing database\n"));

	g_assert(ib->usecount == 0);

	IBEX_LIST_LOCK(ib);
	ibex_list_remove((struct _listnode *)ib);
	IBEX_LIST_UNLOCK(ib);

	if (ib->blocks != NULL)
		ret = close_backend(ib);
	else
		ret = 0;

	g_free(ib->name);

#ifdef ENABLE_THREADS
	g_mutex_free(ib->lock);
#endif
	g_free(ib);

	return ret;
}

/**
 * ibex_unindex:
 * @ib: 
 * @name: 
 * 
 * Remove a name from the index.
 **/
void ibex_unindex (ibex *ib, char *name)
{
	d(printf("trying to unindex '%s'\n", name));

	ibex_use(ib);
	IBEX_LOCK(ib);

	if (ibex_block_cache_setjmp(ib->blocks) != 0) {
		printf("Error unindexing!\n");
		ibex_reset(ib);
	} else {
		ib->words->klass->unindex_name(ib->words, name);
	}

	IBEX_UNLOCK(ib);
	ibex_unuse(ib);
}

/**
 * ibex_find:
 * @ib: 
 * @word: 
 * 
 * Find all names containing @word.
 * 
 * Return value: 
 **/
GPtrArray *ibex_find (ibex *ib, char *word)
{
	char *normal;
	int len;
	GPtrArray *ret;

	len = strlen(word);
	normal = alloca(len+1);
	ibex_normalise_word(word, word+len, normal);

	ibex_use(ib);
	IBEX_LOCK(ib);

	if (ibex_block_cache_setjmp(ib->blocks) != 0) {
		ibex_reset(ib);
		ret = NULL;
	} else {
		ret = ib->words->klass->find(ib->words, normal);
	}

	IBEX_UNLOCK(ib);
	ibex_unuse(ib);

	return ret;
}

/**
 * ibex_find_name:
 * @ib: 
 * @name: 
 * @word: 
 * 
 * Return #TRUE if the specific @word is contained in @name.
 * 
 * Return value: 
 **/
gboolean ibex_find_name (ibex *ib, char *name, char *word)
{
	char *normal;
	int len;
	gboolean ret;

	len = strlen(word);
	normal = alloca(len+1);
	ibex_normalise_word(word, word+len, normal);

	ibex_use(ib);
	IBEX_LOCK(ib);

	if (ibex_block_cache_setjmp(ib->blocks) != 0) {
		ibex_reset(ib);
		ret = FALSE;
	} else {
		ret = ib->words->klass->find_name(ib->words, name, normal);
	}

	IBEX_UNLOCK(ib);
	ibex_unuse(ib);

	return ret;
}

/**
 * ibex_contains_name:
 * @ib: 
 * @name: 
 * 
 * Returns true if the name @name is somewhere in the database.
 * 
 * Return value: 
 **/
gboolean ibex_contains_name(ibex *ib, char *name)
{
	gboolean ret;

	ibex_use(ib);
	IBEX_LOCK(ib);

	if (ibex_block_cache_setjmp(ib->blocks) != 0) {
		ibex_reset(ib);
		ret = FALSE;
	} else {
		ret = ib->words->klass->contains_name(ib->words, name);
	}

	IBEX_UNLOCK(ib);
	ibex_unuse(ib);

	return ret;
}

