/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-search.c: IMAP folder search */

/*
 *  Authors:
 *    Dan Winship <danw@ximian.com>
 *    Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2000, 2001, 2002 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "camel-imap-command.h"
#include "camel-imap-folder.h"
#include "camel-imap-store.h"
#include "camel-imap-search.h"
#include "camel-imap-private.h"
#include "camel-imap-utils.h"
#include "camel-imap-summary.h"

#include "libedataserver/md5-utils.h"	/* md5 hash building */
#include "camel-mime-utils.h"	/* base64 encoding */

#include "camel-seekable-stream.h"
#include "camel-search-private.h"

#define d(x)

/*
  File is:
   BODY	 (as in body search)
   Last uid when search performed
   termcount: number of search terms
   matchcount: number of matches
   term0, term1 ...
   match0, match1, match2, ...
*/

/* size of in-memory cache */
#define MATCH_CACHE_SIZE (32)

/* Also takes care of 'endianness' file magic */
#define MATCH_MARK (('B' << 24) | ('O' << 16) | ('D' << 8) | 'Y')

/* on-disk header, in native endianness format, matches follow */
struct _match_header {
	guint32 mark;
	guint32 validity;	/* uidvalidity for this folder */
	guint32 lastuid;
	guint32 termcount;
	guint32 matchcount;
};

/* in-memory record */
struct _match_record {
	struct _match_record *next;
	struct _match_record *prev;

	char hash[17];

	guint32 lastuid;
	guint32 validity;

	unsigned int termcount;
	char **terms;
	GArray *matches;
};


static void free_match(CamelImapSearch *is, struct _match_record *mr);
static ESExpResult *imap_body_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);

static CamelFolderSearchClass *imap_search_parent_class;

static void
camel_imap_search_class_init (CamelImapSearchClass *camel_imap_search_class)
{
	/* virtual method overload */
	CamelFolderSearchClass *camel_folder_search_class =
		CAMEL_FOLDER_SEARCH_CLASS (camel_imap_search_class);

	imap_search_parent_class = (CamelFolderSearchClass *)camel_type_get_global_classfuncs (camel_folder_search_get_type ());
	
	/* virtual method overload */
	camel_folder_search_class->body_contains = imap_body_contains;
}

static void
camel_imap_search_init(CamelImapSearch *is)
{
	e_dlist_init(&is->matches);
	is->matches_hash = g_hash_table_new(g_str_hash, g_str_equal);
	is->matches_count = 0;
	is->lastuid = 0;
}

static void
camel_imap_search_finalise(CamelImapSearch *is)
{
	struct _match_record *mr;

	while ( (mr = (struct _match_record *)e_dlist_remtail(&is->matches)) )
		free_match(is, mr);
	g_hash_table_destroy(is->matches_hash);
	if (is->cache)
		camel_object_unref((CamelObject *)is->cache);
}

CamelType
camel_imap_search_get_type (void)
{
	static CamelType camel_imap_search_type = CAMEL_INVALID_TYPE;
	
	if (camel_imap_search_type == CAMEL_INVALID_TYPE) {
		camel_imap_search_type = camel_type_register (
			CAMEL_FOLDER_SEARCH_TYPE, "CamelImapSearch",
			sizeof (CamelImapSearch),
			sizeof (CamelImapSearchClass),
			(CamelObjectClassInitFunc) camel_imap_search_class_init, NULL,
			(CamelObjectInitFunc) camel_imap_search_init,
			(CamelObjectFinalizeFunc) camel_imap_search_finalise);
	}

	return camel_imap_search_type;
}

/**
 * camel_imap_search_new:
 *
 * Return value: A new CamelImapSearch widget.
 **/
CamelFolderSearch *
camel_imap_search_new (const char *cachedir)
{
	CamelFolderSearch *new = CAMEL_FOLDER_SEARCH (camel_object_new (camel_imap_search_get_type ()));
	CamelImapSearch *is = (CamelImapSearch *)new;

	camel_folder_search_construct (new);

	is->cache = camel_data_cache_new(cachedir, 0, NULL);
	if (is->cache) {
		/* Expire entries after 14 days of inactivity */
		camel_data_cache_set_expire_access(is->cache, 60*60*24*14);
	}

	return new;
}


static void
hash_match(char hash[17], int argc, struct _ESExpResult **argv)
{
	MD5Context ctx;
	unsigned char digest[16];
	unsigned int state = 0, save = 0;
	int i;

	md5_init(&ctx);
	for (i=0;i<argc;i++) {
		if (argv[i]->type == ESEXP_RES_STRING)
			md5_update(&ctx, argv[i]->value.string, strlen(argv[i]->value.string));
	}
	md5_final(&ctx, digest);

	camel_base64_encode_close(digest, 12, FALSE, hash, &state, &save);

	for (i=0;i<16;i++) {
		if (hash[i] == '+')
			hash[i] = ',';
		if (hash[i] == '/')
			hash[i] = '_';
	}

	hash[16] = 0;
}

static int
save_match(CamelImapSearch *is, struct _match_record *mr)
{
	guint32 mark = MATCH_MARK;
	int ret = 0;
	struct _match_header header;
	CamelStream *stream;

	/* since its a cache, doesn't matter if it doesn't save, at least we have the in-memory cache
	   for this session */
	if (is->cache == NULL)
		return -1;

	stream = camel_data_cache_add(is->cache, "search/body-contains", mr->hash, NULL);
	if (stream == NULL)
		return -1;

	d(printf("Saving search cache entry to '%s': %s\n", mr->hash, mr->terms[0]));
	
	/* we write the whole thing, then re-write the header magic, saves fancy sync code */
	memcpy(&header.mark, "    ", 4);
	header.termcount = 0;
	header.matchcount = mr->matches->len;
	header.lastuid = mr->lastuid;
	header.validity = mr->validity;

	if (camel_stream_write(stream, (char *)&header, sizeof(header)) != sizeof(header)
	    || camel_stream_write(stream, mr->matches->data, mr->matches->len*sizeof(guint32)) != mr->matches->len*sizeof(guint32)
	    || camel_seekable_stream_seek((CamelSeekableStream *)stream, 0, CAMEL_STREAM_SET) == -1
	    || camel_stream_write(stream, (char *)&mark, sizeof(mark)) != sizeof(mark)) {
		d(printf(" saving failed, removing cache entry\n"));
		camel_data_cache_remove(is->cache, "search/body-contains", mr->hash, NULL);
		ret = -1;
	}

	camel_object_unref((CamelObject *)stream);
	return ret;
}

static void
free_match(CamelImapSearch *is, struct _match_record *mr)
{
	int i;

	for (i=0;i<mr->termcount;i++)
		g_free(mr->terms[i]);
	g_free(mr->terms);
	g_array_free(mr->matches, TRUE);
	g_free(mr);
}

static struct _match_record *
load_match(CamelImapSearch *is, char hash[17], int argc, struct _ESExpResult **argv)
{
	struct _match_record *mr;
	CamelStream *stream = NULL;
	struct _match_header header;
	int i;

	mr = g_malloc0(sizeof(*mr));
	mr->matches = g_array_new(0, 0, sizeof(guint32));
	g_assert(strlen(hash) == 16);
	strcpy(mr->hash, hash);
	mr->terms = g_malloc0(sizeof(mr->terms[0]) * argc);
	for (i=0;i<argc;i++) {
		if (argv[i]->type == ESEXP_RES_STRING) {
			mr->termcount++;
			mr->terms[i] = g_strdup(argv[i]->value.string);
		}
	}

	d(printf("Loading search cache entry to '%s': %s\n", mr->hash, mr->terms[0]));

	memset(&header, 0, sizeof(header));
	if (is->cache)
		stream = camel_data_cache_get(is->cache, "search/body-contains", mr->hash, NULL);
	if (stream != NULL) {
		/* 'cause i'm gonna be lazy, i'm going to have the termcount == 0 for now,
		   and not load or save them since i can't think of a nice way to do it, the hash
		   should be sufficient to key it */
		/* This check should also handle endianness changes, we just throw away
		   the data (its only a cache) */
		if (camel_stream_read(stream, (char *)&header, sizeof(header)) == sizeof(header)
		    && header.validity == is->validity
		    && header.mark == MATCH_MARK
		    && header.termcount == 0) {
			d(printf(" found %d matches\n", header.matchcount));
			g_array_set_size(mr->matches, header.matchcount);
			camel_stream_read(stream, mr->matches->data, sizeof(guint32)*header.matchcount);
		} else {
			d(printf(" file format invalid/validity changed\n"));
			memset(&header, 0, sizeof(header));
		}
		camel_object_unref((CamelObject *)stream);
	} else {
		d(printf(" no cache entry found\n"));
	}

	mr->validity = header.validity;
	if (mr->validity != is->validity)
		mr->lastuid = 0;
	else
		mr->lastuid = header.lastuid;

	return mr;
}

static int
sync_match(CamelImapSearch *is, struct _match_record *mr)
{
	char *p, *result, *lasts = NULL;
	CamelImapResponse *response = NULL;
	guint32 uid;
	CamelFolder *folder = ((CamelFolderSearch *)is)->folder;
	CamelImapStore *store = (CamelImapStore *)folder->parent_store;
	struct _camel_search_words *words;
	GString *search;
	int i;
	
	if (mr->lastuid >= is->lastuid && mr->validity == is->validity)
		return 0;
	
	d(printf ("updating match record for uid's %d:%d\n", mr->lastuid+1, is->lastuid));
	
	/* TODO: Handle multiple search terms */
	
	/* This handles multiple search words within a single term */
	words = camel_search_words_split (mr->terms[0]);
	search = g_string_new ("");
	g_string_append_printf (search, "UID %d:%d", mr->lastuid + 1, is->lastuid);
	for (i = 0; i < words->len; i++) {
		char *w = words->words[i]->word, c;
		
		g_string_append_printf (search, " BODY \"");
		while ((c = *w++)) {
			if (c == '\\' || c == '"')
				g_string_append_c (search, '\\');
			g_string_append_c (search, c);
		}
		g_string_append_c (search, '"');
	}
	camel_search_words_free (words);
	
	/* We only try search using utf8 if its non us-ascii text? */
	if ((words->type & CAMEL_SEARCH_WORD_8BIT) &&  (store->capabilities & IMAP_CAPABILITY_utf8_search)) {
		response = camel_imap_command (store, folder, NULL,
					       "UID SEARCH CHARSET UTF-8 %s", search->str);
		/* We can't actually tell if we got a NO response, so assume always */
		if (response == NULL)
			store->capabilities &= ~IMAP_CAPABILITY_utf8_search;
	}
	if (response == NULL)
		response = camel_imap_command (store, folder, NULL,
					       "UID SEARCH %s", search->str);
	g_string_free(search, TRUE);

	if (!response)
		return -1;
	result = camel_imap_response_extract (store, response, "SEARCH", NULL);
	if (!result)
		return -1;
	
	p = result + sizeof ("* SEARCH");
	for (p = strtok_r (p, " ", &lasts); p; p = strtok_r (NULL, " ", &lasts)) {
		uid = strtoul(p, NULL, 10);
		g_array_append_vals(mr->matches, &uid, 1);
	}
	g_free(result);

	mr->validity = is->validity;
	mr->lastuid = is->lastuid;
	save_match(is, mr);

	return 0;
}

static struct _match_record *
get_match(CamelImapSearch *is, int argc, struct _ESExpResult **argv)
{
	char hash[17];
	struct _match_record *mr;

	hash_match(hash, argc, argv);

	mr = g_hash_table_lookup(is->matches_hash, hash);
	if (mr == NULL) {
		while (is->matches_count >= MATCH_CACHE_SIZE) {
			mr = (struct _match_record *)e_dlist_remtail(&is->matches);
			if (mr) {
				printf("expiring match '%s' (%s)\n", mr->hash, mr->terms[0]);
				g_hash_table_remove(is->matches_hash, mr->hash);
				free_match(is, mr);
				is->matches_count--;
			} else {
				is->matches_count = 0;
			}
		}
		mr = load_match(is, hash, argc, argv);
		g_hash_table_insert(is->matches_hash, mr->hash, mr);
		is->matches_count++;
	} else {
		e_dlist_remove((EDListNode *)mr);
	}

	e_dlist_addhead(&is->matches, (EDListNode *)mr);

	/* what about offline mode? */
	/* We could cache those results too, or should we cache them elsewhere? */
	sync_match(is, mr);

	return mr;
}

static ESExpResult *
imap_body_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (s->folder->parent_store);
	CamelImapSearch *is = (CamelImapSearch *)s;
	char *uid;
	ESExpResult *r;
	CamelMessageInfo *info;	
	GHashTable *uid_hash = NULL;
	GPtrArray *array;
	int i, j;
	struct _match_record *mr;
	guint32 uidn, *uidp;

	d(printf("Performing body search '%s'\n", argv[0]->value.string));

	/* TODO: Cache offline searches too? */

	/* If offline, search using the parent class, which can handle this manually */
	if (!camel_disco_store_check_online (CAMEL_DISCO_STORE (store), NULL))
		return imap_search_parent_class->body_contains(f, argc, argv, s);

	/* optimise the match "" case - match everything */
	if (argc == 1 && argv[0]->value.string[0] == '\0') {
		if (s->current) {
			r = e_sexp_result_new(f, ESEXP_RES_BOOL);
			r->value.bool = TRUE;
		} else {
			r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
			r->value.ptrarray = g_ptr_array_new ();
			for (i = 0; i < s->summary->len; i++) {
				info = g_ptr_array_index(s->summary, i);
				g_ptr_array_add(r->value.ptrarray, (char *)camel_message_info_uid(info));
			}
		}
	} else if (argc == 0 || s->summary->len == 0) {
		/* nothing to match case, do nothing (should be handled higher up?) */
		if (s->current) {
			r = e_sexp_result_new(f, ESEXP_RES_BOOL);
			r->value.bool = FALSE;
		} else {
			r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
			r->value.ptrarray = g_ptr_array_new ();
		}
	} else {
		int truth = FALSE;

		/* setup lastuid/validity for synchronising */
		info = g_ptr_array_index(s->summary, s->summary->len-1);
		is->lastuid = strtoul(camel_message_info_uid(info), NULL, 10);
		is->validity = ((CamelImapSummary *)(s->folder->summary))->validity;

		mr = get_match(is, argc, argv);

		if (s->current) {
			uidn = strtoul(camel_message_info_uid(s->current), NULL, 10);
			uidp = (guint32 *)mr->matches->data;
			j = mr->matches->len;
			for (i=0;i<j && !truth;i++)
				truth = *uidp++ == uidn;
			r = e_sexp_result_new(f, ESEXP_RES_BOOL);
			r->value.bool = truth;
		} else {
			r = e_sexp_result_new(f, ESEXP_RES_ARRAY_PTR);
			array = r->value.ptrarray = g_ptr_array_new();

			/* We use a hash to map the uid numbers to uid strings as required by the search api */
			/* We use the summary's strings so we dont need to alloc more */
			uid_hash = g_hash_table_new(NULL, NULL);
			for (i = 0; i < s->summary->len; i++) {
				info = s->summary->pdata[i];
				uid = (char *)camel_message_info_uid(info);
				uidn = strtoul(uid, NULL, 10);
				g_hash_table_insert(uid_hash, GUINT_TO_POINTER(uidn), uid);
			}

			uidp = (guint32 *)mr->matches->data;
			j = mr->matches->len;
			for (i=0;i<j && !truth;i++) {
				uid = g_hash_table_lookup(uid_hash, GUINT_TO_POINTER(*uidp++));
				if (uid)
					g_ptr_array_add(array, uid);
			}

			g_hash_table_destroy(uid_hash);
		}
	}

	return r;
}
