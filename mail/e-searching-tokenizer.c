/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-searching-tokenizer.c
 *
 * Copyright (C) 2002 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
 * Rewritten significantly to handle multiple strings and improve performance
 *  by Michael Zucchi <notzed@ximian.com>
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "e-searching-tokenizer.h"

#include "e-util/e-memory.h"
#include "e-util/e-msgport.h"

#define d(x) 

enum {
	MATCH_SIGNAL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

static void     e_searching_tokenizer_begin      (HTMLTokenizer *, char *);
static void     e_searching_tokenizer_end        (HTMLTokenizer *);
static char    *e_searching_tokenizer_peek_token (HTMLTokenizer *);
static char    *e_searching_tokenizer_next_token (HTMLTokenizer *);
static gboolean e_searching_tokenizer_has_more   (HTMLTokenizer *);

static HTMLTokenizer *e_searching_tokenizer_clone (HTMLTokenizer *);

/*
  static const gchar *space_tags[] = { "br", NULL };*/

static HTMLTokenizerClass *parent_class = NULL;

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

/* ???
typedef struct _SharedState SharedState;
struct _SharedState {
	gint refs;
	gchar *str_primary;
	gchar *str_secondary;
	gboolean case_sensitive_primary;
	gboolean case_sensitive_secondary;
};
*/

/* ********************************************************************** */


#if 0
static SharedState *
shared_state_new (void)
{
	SharedState *shared = g_new0 (SharedState, 1);
	shared->refs = 1;
	return shared;
}

static void
shared_state_ref (SharedState *shared)
{
	g_return_if_fail (shared != NULL);
	g_return_if_fail (shared->refs > 0);
	++shared->refs;
}

static void
shared_state_unref (SharedState *shared)
{
	if (shared) {
		g_return_if_fail (shared->refs > 0);
		--shared->refs;
		if (shared->refs == 0) {
			g_free (shared->str_primary);
			g_free (shared->str_secondary);
			g_free (shared);
		}
	}
}
#endif

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

/* ********************************************************************** */

/* Utility functions */

/* This is faster and safer than glib2's utf8 abomination, but isn't exported from camel as yet */
static inline guint32
camel_utf8_getc(const unsigned char **ptr)
{
	register unsigned char *p = (unsigned char *)*ptr;
	register unsigned char c, r;
	register guint32 v, m;

again:
	r = *p++;
loop:
	if (r < 0x80) {
		*ptr = p;
		v = r;
	} else if (r < 0xfe) { /* valid start char? */
		v = r;
		m = 0x7f80;	/* used to mask out the length bits */
		do {
			c = *p++;
			if ((c & 0xc0) != 0x80) {
				r = c;
				goto loop;
			}
			v = (v<<6) | (c & 0x3f);
			r<<=1;
			m<<=5;
		} while (r & 0x40);
		
		*ptr = p;

		v &= ~m;
	} else {
		goto again;
	}

	return v;
}


/* note: our tags of interest are 7 bit ascii, only, no need to do any fancy utf8 stuff */
/* tags should be upper case
   if this list gets longer than 10 entries, consider binary search */
static char *ignored_tags[] = { "B", "I", "FONT", "TT", "EM", /* and more? */};

static int
ignore_tag(const char *tag)
{
	char *t = alloca(strlen(tag)+1), c, *out;
	const char *in;
	int i;

	/* we could use a aho-corasick matcher here too ... but we wont */

	/* normalise tag into 't'.
	   Note we use the property that the only tags we're interested in
	   are 7 bit ascii to shortcut and simplify case insensitivity */
	in = tag+2;		/* skip: TAG_ESCAPE '<' */
	if (*in == '/')
		in++;
	out = t;
	while ((c = *in++)) {
		if (c >= 'A' && c <= 'Z')
			*out++ = c;
		else if (c >= 'a' && c <= 'z')
			*out++ = c & 0xdf; /* convert ASCII to upper case */
		else
			/* maybe should check for > or ' ' etc? */
			break;
	}
	*out = 0;

	for (i=0;i<sizeof(ignored_tags)/sizeof(ignored_tags[0]);i++) {
		if (strcmp(t, ignored_tags[i]) == 0)
			return 1;
	}

	return 0;
}

/* ********************************************************************** */

/* Aho-Corasick search tree implmeentation */

/* next state if we match a character */
struct _match {
	struct _match *next;
	guint32 ch;
	struct _state *match;
};

/* tree state node */
struct _state {
	struct _match *matches;
	unsigned int final;		/* max no of chars we just matched */
	struct _state *fail;		/* where to try next if we fail */
	struct _state *next;		/* next on this level? */
};

/* base tree structure */
struct _trie {
	struct _state root;
	int max_depth;

	EMemChunk *state_chunks;
	EMemChunk *match_chunks;
};

static void 
dump_trie(struct _state *s, int d)
{
	char *p = alloca(d*2+1);
	struct _match *m;

	memset(p, ' ', d*2);
	p[d*2]=0;
	
	printf("%s[state] %p: %d  fail->%p\n", p, s, s->final, s->fail);
	m = s->matches;
	while (m) {
		printf(" %s'%c' -> %p\n", p, m->ch, m->match);
		if (m->match)
			dump_trie(m->match, d+1);
		m = m->next;
	}
}

/* This builds an Aho-Corasick search trie for a set of utf8 words */
/* See
    http://www-sr.informatik.uni-tuebingen.de/~buehler/AC/AC.html
   for a neat demo */

static inline struct _match *
g(struct _state *q, guint32 c)
{
	struct _match *m = q->matches;

	while (m && m->ch != c)
		m = m->next;

	return m;
}

static struct _trie *
build_trie(int nocase, int len, char **words)
{
	struct _state *q, *qt, *r;
	char *word;
	struct _match *m, *n = NULL;
	int i, depth;
	guint32 c;
	struct _trie *trie;
	int state_depth_max, state_depth_size;
	struct _state **state_depth;

	trie = g_malloc(sizeof(*trie));
	trie->root.matches = NULL;
	trie->root.final = 0;
	trie->root.fail = NULL;
	trie->root.next = NULL;

	trie->state_chunks = e_memchunk_new(8, sizeof(struct _state));
	trie->match_chunks = e_memchunk_new(8, sizeof(struct _match));

	/* This will correspond to the length of the longest pattern */
	state_depth_size = 0;
	state_depth_max = 64;
	state_depth = g_malloc(sizeof(*state_depth[0])*64);
	state_depth[0] = NULL;

	/* Step 1: Build trie */

	/* This just builds a tree that merges all common prefixes into the same branch */

	for (i=0;i<len;i++) {
		word = words[i];
		q = &trie->root;
		depth = 0;
		while ((c = camel_utf8_getc((const unsigned char **)&word))) {
			if (nocase)
				c = g_unichar_tolower(c);
			m = g(q, c);
			if (m == NULL) {
				m = e_memchunk_alloc(trie->match_chunks);
				m->ch = c;
				m->next = q->matches;
				q->matches = m;
				q = m->match = e_memchunk_alloc(trie->state_chunks);
				q->matches = NULL;
				q->fail = &trie->root;
				q->final = 0;
				if (state_depth_max < depth) {
					state_depth_max += 64;
					state_depth = g_realloc(state_depth, sizeof(*state_depth[0])*state_depth_max);
				}
				if (state_depth_size < depth) {
					state_depth[depth] = 0;
					state_depth_size = depth;
				}
				q->next = state_depth[depth];
				state_depth[depth] = q;
			} else {
				q = m->match;
			}
			depth++;
		}
		q->final = depth;
	}

	d(printf("Dumping trie:\n"));
	d(dump_trie(&trie->root, 0));

	/* Step 2: Build failure graph */

	/* This searches for the longest substring which is a prefix of another string and
	   builds a graph of failure links so you can find multiple substrings concurrently,
	   using aho-corasick's algorithm */

	for (i=0;i<state_depth_size;i++) {
		q = state_depth[i];
		while (q) {
			m = q->matches;
			while (m) {
				c = m->ch;
				qt = m->match;
				r = q->fail;
				while (r && (n = g(r, c)) == NULL)
					r = r->fail;
				if (r != NULL) {
					qt->fail = n->match;
					if (qt->fail->final > qt->final)
						qt->final = qt->fail->final;
				} else {
					if ((n = g(&trie->root, c)))
						qt->fail = n->match;
					else
						qt->fail = &trie->root;
				}
				m = m->next;
			}
			q = q->next;
		}
	}
	
	d(printf("After failure analysis\n"));
	d(dump_trie(&trie->root, 0));

	g_free(state_depth);

	trie->max_depth = state_depth_size;

	return trie;
}

static void
free_trie(struct _trie *t)
{
	e_memchunk_destroy(t->match_chunks);
	e_memchunk_destroy(t->state_chunks);

	g_free(t);
}

/* ********************************************************************** */

/* html token searcher */

struct _token {
	struct _token *next;
	struct _token *prev;
	unsigned int offset;
	/* we need to copy the token for memory management, so why not copy it whole */
	char tok[1];
};

/* stack of submatches currently being scanned, used for merging */
struct _submatch {
	unsigned int offstart, offend; /* in bytes */
};

/* flags for new func */
#define SEARCH_CASE (1)
#define SEARCH_BOLD (2)

struct _searcher {
	struct _trie *t;

	char *(*next_token)();	/* callbacks for more tokens */
	void *next_data;

	int words;		/* how many words */
	char *tags, *tage;	/* the tag we used to highlight */

	int flags;		/* case sensitive or not */

	struct _state *state;	/* state is the current trie state */

	int matchcount;

	EDList input;		/* pending 'input' tokens, processed but might match */
	EDList output;		/* output tokens ready for source */

	struct _token *current;	/* for token output memory management */

	guint32 offset;		/* current offset through searchable stream? */
	guint32 offout;		/* last output position */

	unsigned int lastp;	/* current position in rotating last buffer */
	guint32 *last;		/* buffer that goes back last 'n' positions */
	guint32 last_mask;	/* bitmask for efficient rotation calculation */

	unsigned int submatchp;	/* submatch stack */
	struct _submatch *submatches;
};

static void
searcher_set_tokenfunc(struct _searcher *s, char *(*next)(), void *data)
{
	s->next_token = next;
	s->next_data = data;
}

static struct _searcher *
searcher_new(int flags, int argc, char **argv, const char *tags, const char *tage)
{
	int i, m;
	struct _searcher *s;

	s = g_malloc(sizeof(*s));

	s->t = build_trie((flags&SEARCH_CASE) == 0, argc, argv);
	s->words = argc;
	s->tags = g_strdup(tags);
	s->tage = g_strdup(tage);
	s->flags = flags;
	s->state = &s->t->root;
	s->matchcount = 0;

	e_dlist_init(&s->input);
	e_dlist_init(&s->output);
	s->current = 0;

	s->offset = 0;
	s->offout = 0;

	/* rotating queue of previous character positions */
	m = s->t->max_depth+1;
	i = 2;
	while (i<m)
		i<<=2;
	s->last = g_malloc(sizeof(s->last[0])*i);
	s->last_mask = i-1;
	s->lastp = 0;

	/* a stack of possible submatches */
	s->submatchp = 0;
	s->submatches = g_malloc(sizeof(s->submatches[0])*argc+1);

	return s;
}

static void
searcher_free(struct _searcher *s)
{
	struct _token *t;

	while ((t = (struct _token *)e_dlist_remhead(&s->input)))
		g_free(t);
	while ((t = (struct _token *)e_dlist_remhead(&s->output)))
		g_free(t);
	g_free(s->tags);
	g_free(s->tage);
	g_free(s->last);
	g_free(s->submatches);
	free_trie(s->t);
	g_free(s);
}
static struct _token *
append_token(EDList *list, const char *tok, int len)
{
	struct _token *token;

	if (len == -1)
		len = strlen(tok);
	token = g_malloc(sizeof(*token) + len+1);
	token->offset = 0;	/* set by caller when required */
	memcpy(token->tok, tok, len);
	token->tok[len] = 0;
	e_dlist_addtail(list, (EDListNode *)token);

	return token;
}

#define free_token(x) (g_free(x))

static void
output_token(struct _searcher *s, struct _token *token)
{
	int offend;
	int left, pre;

	if (token->tok[0] == TAG_ESCAPE) {
		if (token->offset >= s->offout) {
			d(printf("moving tag token '%s' from input to output\n", token->tok[0]==TAG_ESCAPE?token->tok+1:token->tok));
			e_dlist_addtail(&s->output, (EDListNode *)token);
		} else {
			d(printf("discarding tag token '%s' from input\n", token->tok[0]==TAG_ESCAPE?token->tok+1:token->tok));
			free_token(token);
		}
	} else {
		offend = token->offset + strlen(token->tok);
		left = offend-s->offout;
		if (left > 0) {
			pre = s->offout - token->offset;
			if (pre>0)
				memmove(token->tok, token->tok+pre, left+1);
			d(printf("adding partial remaining/failed '%s'\n", token->tok[0]==TAG_ESCAPE?token->tok+1:token->tok));
			s->offout = offend;
			e_dlist_addtail(&s->output, (EDListNode *)token);
		} else {
			d(printf("discarding whole token '%s'\n", token->tok[0]==TAG_ESCAPE?token->tok+1:token->tok));
			free_token(token);
		}
	}
}

static struct _token *
find_token(struct _searcher *s, int start)
{
	register struct _token *token;

	/* find token which is start token, from end of list back */
	token = (struct _token *)s->input.tailpred;
	while (token->prev) {
		if (token->offset <= start)
			return token;
		token = token->prev;
	}

	return NULL;
}

static void
output_match(struct _searcher *s, unsigned int start, unsigned int end)
{
	register struct _token *token;
	struct _token *starttoken, *endtoken;
	char b[8];

	d(printf("output match: %d-%d  at %d\n", start, end, s->offout));

	starttoken = find_token(s, start);
	endtoken = find_token(s, end);

	if (starttoken == NULL || endtoken == NULL) {
		printf("Cannot find match history for match %d-%d\n", start, end);
		return;
	}

	d(printf("start in token '%s'\n", starttoken->tok[0]==TAG_ESCAPE?starttoken->tok+1:starttoken->tok));
	d(printf("end in token   '%s'\n", endtoken->tok[0]==TAG_ESCAPE?endtoken->tok+1:endtoken->tok));

	/* output pending stuff that didn't match afterall */
	while ((struct _token *)s->input.head != starttoken) {
		token = (struct _token *)e_dlist_remhead(&s->input);
		d(printf("appending failed match '%s'\n", token->tok[0]==TAG_ESCAPE?token->tok+1:token->tok));
		output_token(s, token);
	}

	/* output any pre-match text */
	if (s->offout < start) {
		token = append_token(&s->output, starttoken->tok + (s->offout-starttoken->offset), start-s->offout);
		d(printf("adding pre-match text '%s'\n", token->tok[0]==TAG_ESCAPE?token->tok+1:token->tok));
		s->offout = start;
	}

	/* output highlight/bold */
	if (s->flags & SEARCH_BOLD) {
		sprintf(b, "%c<b>", (char)TAG_ESCAPE);
		append_token(&s->output, b, -1);
	}
	if (s->tags)
		append_token(&s->output, s->tags, -1);

	/* output match node(s) */
	if (starttoken != endtoken) {
		while ((struct _token *)s->input.head != endtoken) {
			token = (struct _token *)e_dlist_remhead(&s->input);
			d(printf("appending (partial) match node (head) '%s'\n", token->tok[0]==TAG_ESCAPE?token->tok+1:token->tok));
			output_token(s, token);
		}
	}

	/* any remaining partial content */
	if (s->offout < end) {
		token = append_token(&s->output, endtoken->tok+(s->offout-endtoken->offset), end-s->offout);
		d(printf("appending (partial) match node (tail) '%s'\n", token->tok[0]==TAG_ESCAPE?token->tok+1:token->tok));
		s->offout = end;
	}

	/* end highlight */
	if (s->tage)
		append_token(&s->output, s->tage, -1);

	/* and close bold if we need to */
	if (s->flags & SEARCH_BOLD) {
		sprintf(b, "%c</b>", (char)TAG_ESCAPE);
		append_token(&s->output, b, -1);
	}
}

/* output any sub-pending blocks */
static void
output_subpending(struct _searcher *s)
{
	int i;

	for (i=s->submatchp-1;i>=0;i--)
		output_match(s, s->submatches[i].offstart, s->submatches[i].offend);
	s->submatchp = 0;
}

/* returns true if a merge took place */
static int
merge_subpending(struct _searcher *s, int offstart, int offend)
{
	int i;

	/* merges overlapping or abutting match strings */
	if (s->submatchp &&
	    s->submatches[s->submatchp-1].offend >= offstart) {

		/* go from end, any that match 'invalidate' follow-on ones too */
		for (i=s->submatchp-1;i>=0;i--) {
			if (s->submatches[i].offend >= offstart) {
				if (offstart < s->submatches[i].offstart)
					s->submatches[i].offstart = offstart;
				s->submatches[i].offend = offend;
				if (s->submatchp > i)
					s->submatchp = i+1;
			}
		}
		return 1;
	}

	return 0;
}

static void
push_subpending(struct _searcher *s, int offstart, int offend)
{
	/* This is really an assertion, we just ignore the last pending match instead of crashing though */
	if (s->submatchp >= s->words) {
		printf("ERROR: submatch pending stack overflow\n");
		s->submatchp = s->words-1;
	}

	s->submatches[s->submatchp].offstart = offstart;
	s->submatches[s->submatchp].offend = offend;
	s->submatchp++;
}

/* move any (partial) tokens from input to output if they are beyond the current output position */
static void
output_pending(struct _searcher *s)
{
	struct _token *token;

	while ( (token = (struct _token *)e_dlist_remhead(&s->input)) )
		output_token(s, token);
}

/* flushes any nodes we cannot possibly match anymore */
static void
flush_extra(struct _searcher *s)
{
	unsigned int start;
	int i;
	struct _token *starttoken, *token;

	/* find earliest char that can be in contention */
	start = s->offset - s->t->max_depth;
	for (i=0;i<s->submatchp;i++)
		if (s->submatches[i].offstart < start)
			start = s->submatches[i].offstart;

	/* now, flush out any tokens which are before this point */
	starttoken = find_token(s, start);
	if (starttoken == NULL)
		return;

	while ((struct _token *)s->input.head != starttoken) {
		token = (struct _token *)e_dlist_remhead(&s->input);
		output_token(s, token);
	}
}

static char *
searcher_next_token(struct _searcher *s)
{
	struct _token *token;
	char *tok, *stok, *pre_tok;
	struct _trie *t = s->t;
	struct _state *q = s->state;
	struct _match *m = NULL;
	int offstart, offend;
	guint32 c;

	while (e_dlist_empty(&s->output)) {
		/* get next token */
		tok = s->next_token(s->next_data);
		if (tok == NULL) {
			output_subpending(s);
			output_pending(s);
			break;
		}

		/* we dont always have to copy each token, e.g. if we dont match anything */
		token = append_token(&s->input, tok, -1);
		token->offset = s->offset;
		tok = token->tok;

		d(printf("new token %d '%s'\n", token->offset, token->tok[0]==TAG_ESCAPE?token->tok+1:token->tok));

		/* tag test, reset state on unknown tags */
		if (tok[0] == TAG_ESCAPE) {
			if (!ignore_tag(tok)) {
				/* force reset */
				output_subpending(s);
				output_pending(s);
				q = &t->root;
			}

			continue;
		}

		/* process whole token */
		pre_tok = stok = tok;
		while ((c = camel_utf8_getc((const unsigned char **)&tok))) {
			if ((s->flags & SEARCH_CASE) == 0)
				c = g_unichar_tolower(c);
			while (q && (m = g(q, c)) == NULL)
				q = q->fail;
			if (q == NULL) {
				/* mismatch ... reset state */
				output_subpending(s);
				q = &t->root;
			} else if (m != NULL) {
				/* keep track of previous offsets of utf8 chars, rotating buffer */
				s->last[s->lastp] = s->offset + (pre_tok-stok);
				s->lastp = (s->lastp+1)&s->last_mask;

				q = m->match;
				/* we have a match of q->final characters for a matching word */
				if (q->final) {
					s->matchcount++;

					/* use the last buffer to find the real offset of this char */
					offstart = s->last[(s->lastp - q->final)&s->last_mask];
					offend = s->offset + (tok - stok);

					if (q->matches == NULL) {
						if (s->submatchp == 0) {
							/* nothing pending, always put something in so we can try merge */
							push_subpending(s, offstart, offend);
						} else if (!merge_subpending(s, offstart, offend)) {
							/* can't merge, output what we have, and start againt */
							output_subpending(s);
							push_subpending(s, offstart, offend);
							/*output_match(s, offstart, offend);*/
						} else if (e_dlist_length(&s->input) > 8) {
							/* we're continuing to match and merge, but we have a lot of stuff
							   waiting, so flush it out now since this is a safe point to do it */
							output_subpending(s);
						}
					} else {
						/* merge/add subpending */
						if (!merge_subpending(s, offstart, offend))
							push_subpending(s, offstart, offend);
					}
				}
			}
			pre_tok = tok;
		}

		s->offset += (pre_tok-stok);

		flush_extra(s);
	}

	s->state = q;

	if (s->current)
		free_token(s->current);

	s->current = token = (struct _token *)e_dlist_remhead(&s->output);

	return token?token->tok:NULL;
}

static char *
searcher_peek_token(struct _searcher *s)
{
	char *tok;

	/* we just get it and then put it back, it's fast enuf */
	tok = searcher_next_token(s);
	if (tok) {
		/* need to clear this so we dont free it while its still active */
		e_dlist_addhead(&s->output, (EDListNode *)s->current);
		s->current = NULL;
	}

	return tok;
}

static int
searcher_pending(struct _searcher *s)
{
	return !(e_dlist_empty(&s->input) && e_dlist_empty(&s->output));
}

/* ********************************************************************** */

struct _search_info {
	GPtrArray *strv;
	char *colour;
	unsigned int size:8;
	unsigned int flags:8;
};

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

static struct _search_info *
search_info_new(void)
{
	struct _search_info *s;

	s = g_malloc0(sizeof(struct _search_info));
	s->strv = g_ptr_array_new();

	return s;
}

static void
search_info_set_flags(struct _search_info *si, unsigned int flags, unsigned int mask)
{
	si->flags = (si->flags & ~mask) | (flags & mask);
}

static void
search_info_set_colour(struct _search_info *si, const char *colour)
{
	g_free(si->colour);
	si->colour = g_strdup(colour);
}

static void
search_info_add_string(struct _search_info *si, const char *s)
{
	const char *start;
	guint32 c;

	if (s && s[0]) {
		/* strip leading whitespace */
		start = s;
		while ((c = camel_utf8_getc((const unsigned char **)&s))) {
			if (!g_unichar_isspace(c)) {
				break;
			}
			start = s;
		}
		/* should probably also strip trailing, but i'm lazy today */
		if (start[0])
			g_ptr_array_add(si->strv, g_strdup(start));
	}
}

static void
search_info_clear(struct _search_info *si)
{
	int i;

	for (i=0;i<si->strv->len;i++)
		g_free(si->strv->pdata[i]);

	g_ptr_array_set_size(si->strv, 0);
}

static void
search_info_free(struct _search_info *si)
{
	int i;

	for (i=0;i<si->strv->len;i++)
		g_free(si->strv->pdata[i]);

	g_ptr_array_free(si->strv, TRUE);
	g_free(si->colour);
	g_free(si);
}

static struct _search_info *
search_info_clone(struct _search_info *si)
{
	struct _search_info *out;
	int i;

	out = search_info_new();
	for (i=0;i<si->strv->len;i++)
		g_ptr_array_add(out->strv, g_strdup(si->strv->pdata[i]));
	out->colour = g_strdup(si->colour);
	out->flags = si->flags;
	out->size = si->size;

	return out;
}

static struct _searcher *
search_info_to_searcher(struct _search_info *si)
{
	char *tags, *tage;
	char *col;

	if (si->strv->len == 0)
		return NULL;

	if (si->colour == NULL)
		col = "red";
	else
		col = si->colour;

	tags = alloca(20+strlen(col));
	sprintf(tags, "%c<font color=\"%s\">", TAG_ESCAPE, col);
	tage = alloca(20);
	sprintf(tage, "%c</font>", TAG_ESCAPE);

	return searcher_new(si->flags, si->strv->len, (char **)si->strv->pdata, tags, tage);
}

/* ********************************************************************** */

struct _ESearchingTokenizerPrivate {
	struct _search_info *primary, *secondary;
	struct _searcher *engine;
};

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

static void
e_searching_tokenizer_finalise (GObject *obj)
{
	ESearchingTokenizer *st = E_SEARCHING_TOKENIZER (obj);
	struct _ESearchingTokenizerPrivate *p = st->priv;
	
	search_info_free (p->primary);
	search_info_free (p->secondary);
	if (p->engine)
		searcher_free(p->engine);
	
	/* again wtf? 
	shared_state_unref (st->priv->shared);
	*/
	
	g_free (p);
	
	if (G_OBJECT_CLASS (parent_class)->finalize)
		G_OBJECT_CLASS (parent_class)->finalize(obj);
}

static void
e_searching_tokenizer_class_init (ESearchingTokenizerClass *klass)
{
	GObjectClass *obj_class = (GObjectClass *) klass;
	HTMLTokenizerClass *tok_class = HTML_TOKENIZER_CLASS (klass);
	
	parent_class = g_type_class_ref (HTML_TYPE_TOKENIZER);
	
	signals[MATCH_SIGNAL] =
		g_signal_new ("match",
			      E_TYPE_SEARCHING_TOKENIZER,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESearchingTokenizerClass, match),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	obj_class->finalize = e_searching_tokenizer_finalise;
	
	tok_class->begin = e_searching_tokenizer_begin;
	tok_class->end = e_searching_tokenizer_end;
	
	tok_class->peek_token = e_searching_tokenizer_peek_token;
	tok_class->next_token = e_searching_tokenizer_next_token;
	tok_class->has_more = e_searching_tokenizer_has_more;
	tok_class->clone = e_searching_tokenizer_clone;
}

static void
e_searching_tokenizer_init (ESearchingTokenizer *st)
{
	struct _ESearchingTokenizerPrivate *p;

	p = st->priv = g_new0 (struct _ESearchingTokenizerPrivate, 1);

	p->primary = search_info_new();
	search_info_set_flags(p->primary, SEARCH_BOLD, SEARCH_CASE|SEARCH_BOLD);
	search_info_set_colour(p->primary, "red");
	
	p->secondary = search_info_new();
	search_info_set_flags(p->secondary, SEARCH_BOLD, SEARCH_CASE|SEARCH_BOLD);
	search_info_set_colour(p->secondary, "purple");
}

GType
e_searching_tokenizer_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (ESearchingTokenizerClass),
			NULL, NULL,
			(GClassInitFunc) e_searching_tokenizer_class_init,
			NULL, NULL,
			sizeof (ESearchingTokenizer),
			0,
			(GInstanceInitFunc) e_searching_tokenizer_init,
		};
		
		type = g_type_register_static (HTML_TYPE_TOKENIZER, "ESearchingTokenizer", &info, 0);
	}
	
	return type;
}

HTMLTokenizer *
e_searching_tokenizer_new (void)
{
	return (HTMLTokenizer *) g_object_new (E_TYPE_SEARCHING_TOKENIZER, NULL);
}

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

/* blah blah the htmltokeniser doesn't like being asked
   for a token if it doens't hvae any! */
static char *get_token(HTMLTokenizer *t)
{
	HTMLTokenizerClass *klass = HTML_TOKENIZER_CLASS (parent_class);
	
	return klass->has_more(t) ? klass->next_token(t) : NULL;
}

static void
e_searching_tokenizer_begin (HTMLTokenizer *t, char *content_type)
{
	ESearchingTokenizer *st = E_SEARCHING_TOKENIZER (t);
	struct _ESearchingTokenizerPrivate *p = st->priv;

	/* reset search */
	if (p->engine) {
		searcher_free(p->engine);
		p->engine = NULL;
	}

	if ((p->engine = search_info_to_searcher(p->primary))
	    || (p->engine = search_info_to_searcher(p->secondary))) {
		/*HTMLTokenizerClass *klass = HTML_TOKENIZER_CLASS (parent_class);*/

		/*searcher_set_tokenfunc(p->engine, klass->next_token, st);*/
		searcher_set_tokenfunc(p->engine, get_token, st);
	}
	/* else - no engine, no search active */

	HTML_TOKENIZER_CLASS (parent_class)->begin (t, content_type);
}

static void
e_searching_tokenizer_end (HTMLTokenizer *t)
{
	/* so end gets called before any get/next tokens.
	   I dont get it.  */
#if 0
	ESearchingTokenizer *st = E_SEARCHING_TOKENIZER (t);
	struct _ESearchingTokenizerPrivate *p = st->priv;

	/* not sure if we should reset search every time ... *shrug* */
	if (p->engine) {
		searcher_free(p->engine);
		p->engine = NULL;
	}
#endif

	HTML_TOKENIZER_CLASS (parent_class)->end (t);
}

static char *
e_searching_tokenizer_peek_token (HTMLTokenizer *tok)
{
	ESearchingTokenizer *st = E_SEARCHING_TOKENIZER (tok);

	/* If no search is active, just use the default method. */
	if (st->priv->engine == NULL)
		return HTML_TOKENIZER_CLASS (parent_class)->peek_token (tok);

	return searcher_peek_token(st->priv->engine);
}

static char *
e_searching_tokenizer_next_token (HTMLTokenizer *tok)
{
	ESearchingTokenizer *st = E_SEARCHING_TOKENIZER (tok);
	int oldmatched;
	char *token;
	
	/* If no search is active, just use the default method. */
	if (st->priv->engine == NULL)
		return HTML_TOKENIZER_CLASS (parent_class)->next_token (tok);
	
	oldmatched = st->priv->engine->matchcount;
	
	token = searcher_next_token(st->priv->engine);

	/* not sure if this has to be accurate or just say we had some matches */
	if (oldmatched != st->priv->engine->matchcount)
		g_signal_emit (st, signals[MATCH_SIGNAL], 0);
	
	return token;
}

static gboolean
e_searching_tokenizer_has_more (HTMLTokenizer *tok)
{
	ESearchingTokenizer *st = E_SEARCHING_TOKENIZER (tok);

	return (st->priv->engine != NULL && searcher_pending(st->priv->engine))
		|| HTML_TOKENIZER_CLASS (parent_class)->has_more (tok);
}

/* proxy matched event, not sure what its for otherwise */
static void
matched (ESearchingTokenizer *st)
{
	/*++st->priv->match_count;*/
	g_signal_emit (st, signals[MATCH_SIGNAL], 0);
}

static HTMLTokenizer *
e_searching_tokenizer_clone (HTMLTokenizer *tok)
{
	ESearchingTokenizer *orig_st = E_SEARCHING_TOKENIZER (tok);
	ESearchingTokenizer *new_st = E_SEARCHING_TOKENIZER (e_searching_tokenizer_new ());
	
	search_info_free(new_st->priv->primary);
	search_info_free(new_st->priv->secondary);
	
	new_st->priv->primary = search_info_clone(orig_st->priv->primary);
	new_st->priv->secondary = search_info_clone(orig_st->priv->secondary);
	
	/* what the fucking what???? */
#if 0
	shared_state_ref (orig_st->priv->shared);
	shared_state_unref (new_st->priv->shared);
	new_st->priv->shared = orig_st->priv->shared;
#endif
	
	g_signal_connect_swapped (new_st, "match", G_CALLBACK(matched), orig_st);
	
	return HTML_TOKENIZER (new_st);
}
/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

void
e_searching_tokenizer_set_primary_search_string (ESearchingTokenizer *st, const gchar *search_str)
{
	g_return_if_fail (st && E_IS_SEARCHING_TOKENIZER (st));
	
	search_info_clear(st->priv->primary);
	search_info_add_string(st->priv->primary, search_str);
}

void
e_searching_tokenizer_add_primary_search_string (ESearchingTokenizer *st, const gchar *search_str)
{
	g_return_if_fail (st && E_IS_SEARCHING_TOKENIZER (st));
	
	search_info_add_string(st->priv->primary, search_str);
}

void
e_searching_tokenizer_set_primary_case_sensitivity (ESearchingTokenizer *st, gboolean iscase)
{
	g_return_if_fail (st && E_IS_SEARCHING_TOKENIZER (st));
	
	search_info_set_flags(st->priv->primary, iscase?SEARCH_CASE:0, SEARCH_CASE);
}

void
e_searching_tokenizer_set_secondary_search_string (ESearchingTokenizer *st, const gchar *search_str)
{
	g_return_if_fail (st && E_IS_SEARCHING_TOKENIZER (st));
	
	search_info_clear(st->priv->secondary);
	search_info_add_string(st->priv->secondary, search_str);
}

void
e_searching_tokenizer_add_secondary_search_string (ESearchingTokenizer *st, const gchar *search_str)
{
	g_return_if_fail (st && E_IS_SEARCHING_TOKENIZER (st));
	
	search_info_add_string(st->priv->secondary, search_str);
}

void
e_searching_tokenizer_set_secondary_case_sensitivity (ESearchingTokenizer *st, gboolean iscase)
{
	g_return_if_fail (st && E_IS_SEARCHING_TOKENIZER (st));
	
	search_info_set_flags(st->priv->secondary, iscase?SEARCH_CASE:0, SEARCH_CASE);
}

/* Note: only returns the primary search string count */
gint
e_searching_tokenizer_match_count (ESearchingTokenizer *st)
{
	g_return_val_if_fail (E_IS_SEARCHING_TOKENIZER (st), -1);
	
	if (st->priv->engine && st->priv->primary->strv->len)
		return st->priv->engine->matchcount;
	
	return 0;
}
