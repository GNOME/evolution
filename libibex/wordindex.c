/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Michael Zucchi <notzed@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* code to manage a word index */
/* includes a cache for word index writes,
   but not for name index writes (currently), or any reads.

Note the word cache is only needed during indexing of lots
of words, and could then be discarded (:flush()).

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib.h>

#include "block.h"
#include "index.h"
#include "wordindex.h"

#define d(x)

/*#define WORDCACHE_SIZE (256)*/
#define WORDCACHE_SIZE (4096)

extern struct _IBEXStoreClass ibex_diskarray_class;
extern struct _IBEXIndexClass ibex_hash_class;

/* need 2 types of hash key?
   one that just stores the wordid / wordblock
   and one that stores the filecount/files?
*/


#define CACHE_FILE_COUNT (62)

struct _wordcache {
	struct _wordcache *next;
	struct _wordcache *prev;
	nameid_t wordid;	/* disk wordid */
	blockid_t wordblock;	/* head of disk list */
	blockid_t wordtail;	/* and the tail data */
	short filecount;	/* how many valid items in files[] */
	short filealloc;	/* how much allocated space in files[] */
	union {
		nameid_t *files;	/* memory cache of files */
		nameid_t file0;	/* if filecount == 1 && filealloc == 0, store directly */
	} file;
	char word[1];		/* actual word follows */
};

static void unindex_name(struct _IBEXWord *, const char *name); 	/* unindex all entries for name */
static gboolean contains_name(struct _IBEXWord *, const char *name);	/* index contains data for name */
static GPtrArray *find(struct _IBEXWord *, const char *word);		/* returns all matches for word */
static gboolean find_name(struct _IBEXWord *, const char *name, const char *word);	/* find if name contains word */
static void add(struct _IBEXWord *, const char *name, const char *word);	/* adds a single word to name (slow) */
static void add_list(struct _IBEXWord *, const char *name, GPtrArray *words);/* adds a bunch of words to a given name */
static int word_sync(struct _IBEXWord *idx);
static int word_flush(struct _IBEXWord *idx);
static int word_close(struct _IBEXWord *idx);
static void word_index_pre(struct _IBEXWord *idx);
static void word_index_post(struct _IBEXWord *idx);

struct _IBEXWordClass ibex_word_index_class = {
	word_sync, word_flush, word_close,
	word_index_pre, word_index_post,
	unindex_name, contains_name,
	find, find_name,
	add, add_list
};

/* this interface isn't the best, but it'll do for now */
struct _IBEXWord *
ibex_create_word_index(struct _memcache *bc, blockid_t *wordroot, blockid_t *nameroot)
{
	struct _IBEXWord *idx;

	idx = g_malloc0(sizeof(*idx));
	idx->wordcache = g_hash_table_new(g_str_hash, g_str_equal);
	ibex_list_new(&idx->wordnodes);
	idx->wordcount = 0;
	idx->klass = &ibex_word_index_class;

	/* we use the same block array storage for both indexes at the moment */
	idx->wordstore = ibex_diskarray_class.create(bc);
	idx->namestore = idx->wordstore;

	/* but not the same indexes! */
	if (*wordroot) {
		printf("opening wordindex root = %d\n", *wordroot);
		idx->wordindex = ibex_hash_class.open(bc, *wordroot);
	} else {
		idx->wordindex = ibex_hash_class.create(bc, 2048);
		*wordroot = idx->wordindex->root;
		printf("creating wordindex root = %d\n", *wordroot);
	}
	if (*nameroot) {
		printf("opening nameindex root = %d\n", *nameroot);
		idx->nameindex = ibex_hash_class.open(bc, *nameroot);
	} else {
		idx->nameindex = ibex_hash_class.create(bc, 2048);
		*nameroot = idx->nameindex->root;
		printf("creating nameindex root = %d\n", *nameroot);
	}
	return idx;
}

#if 1
static void
cache_sanity(struct _wordcache *head)
{
	while (head->next) {
		g_assert(head->filecount <= head->filealloc);
		g_assert(strlen(head->word) != 0);
		head = head->next;
	}
}
#endif

static void word_index_pre(struct _IBEXWord *idx)
{
}

static void word_index_post(struct _IBEXWord *idx)
{
}

/* unindex all entries for name */
static void unindex_name(struct _IBEXWord *idx, const char *name)
{
	GArray *words;
	int i;
	nameid_t nameid, wordid;
	blockid_t nameblock, wordblock, newblock, nametail, wordtail, newtail;
	char *word;
	struct _wordcache *cache;

	d(printf("unindexing %s\n", name));

	/* lookup the hash key */
	nameid = idx->nameindex->klass->find(idx->nameindex, name, strlen(name));
	/* get the block for this key */
	nameblock = idx->nameindex->klass->get_data(idx->nameindex, nameid, &nametail);
	/* and the data for this block */
	words = idx->namestore->klass->get(idx->namestore, nameblock, nametail);
	/* walk it ... */
	for (i=0;i<words->len;i++) {
		/* get the word */
		wordid = g_array_index(words, nameid_t, i);
		d(printf(" word %d\n", wordid));
		/* get the data block */
		wordblock = idx->wordindex->klass->get_data(idx->wordindex, wordid, &wordtail);
		/* clear this name from it */
		newblock = wordblock;
		newtail = wordtail;
		idx->wordstore->klass->remove(idx->wordstore, &newblock, &newtail, nameid);
		if (newblock != wordblock || newtail != wordtail)
			idx->wordindex->klass->set_data(idx->wordindex, wordid, newblock, newtail);

		/* now check the cache as well */
		word = idx->nameindex->klass->get_key(idx->wordindex, wordid, NULL);
		if (word) {
			cache = g_hash_table_lookup(idx->wordcache, word);
			if (cache) {
				/* its there, update our head/tail pointers */
				cache->wordblock = newblock;
				cache->wordtail = newtail;

				/* now check that we have a data entry in it */
				if (cache->filealloc == 0 && cache->filecount == 1) {
					if (cache->file.file0 == nameid) {
						cache->filecount = 0;
					}
				} else {
					int j;

					for (j=0;j<cache->filecount;j++) {
						if (cache->file.files[j] == nameid) {
							cache->file.files[j] = cache->file.files[cache->filecount-1];
							cache->filecount--;
							break;
						}
					}
				}
			}
			g_free(word);
		}
	}
	g_array_free(words, TRUE);

	/* and remove name data and itself */
	idx->namestore->klass->free(idx->namestore, nameblock, nametail);
	idx->nameindex->klass->remove(idx->nameindex, name, strlen(name));
}

/* index contains (any) data for name */
static gboolean contains_name(struct _IBEXWord *idx, const char *name)
{
	return idx->nameindex->klass->find(idx->nameindex, name, strlen(name)) != 0;
}

/* returns all matches for word */
static GPtrArray *find(struct _IBEXWord *idx, const char *word)
{
	nameid_t wordid, nameid;
	GPtrArray *res;
	GArray *names;
	int i;
	char *new;
	struct _wordcache *cache;
	blockid_t wordblock, wordtail;

	res = g_ptr_array_new();

	cache = g_hash_table_lookup(idx->wordcache, word);
	if (cache) {
		/* freshen cache entry if we touch it */
		ibex_list_remove((struct _listnode *)cache);
		ibex_list_addtail(&idx->wordnodes, (struct _listnode *)cache);
		wordid = cache->wordid;
		wordblock = cache->wordblock;
		wordtail = cache->wordtail;
	} else {
		/* lookup the hash key */
		wordid = idx->wordindex->klass->find(idx->wordindex, word, strlen(word));
		/* get the block for this key */
		wordblock = idx->wordindex->klass->get_data(idx->wordindex, wordid, &wordtail);
	}
	/* and the data for this block */
	names = idx->wordstore->klass->get(idx->wordstore, wordblock, wordtail);
	/* .. including any memory-only data */
	if (cache) {
		if (cache->filealloc == 0 && cache->filecount == 1)
			g_array_append_val(names, cache->file.file0);
		else
			g_array_append_vals(names, cache->file.files, cache->filecount);
	}

	/* walk it ... converting id's back to strings */
	g_ptr_array_set_size(res, names->len);
	for (i=0;i<names->len;i++) {
		nameid = g_array_index(names, nameid_t, i);
		new = idx->nameindex->klass->get_key(idx->nameindex, nameid, NULL);
		res->pdata[i] = new;
	}
	g_array_free(names, TRUE);
	return res;
}

/* find if name contains word */
static gboolean find_name(struct _IBEXWord *idx, const char *name, const char *word)
{
	nameid_t wordid, nameid;
	blockid_t nameblock, nametail;
	struct _wordcache *cache;
	int i;

	/* lookup the hash key for the name */
	nameid = idx->nameindex->klass->find(idx->nameindex, name, strlen(name));
	/* get the block for this name */
	nameblock = idx->nameindex->klass->get_data(idx->nameindex, nameid, &nametail);

	/* check if there is an in-memory cache for this word, check its file there first */
	cache = g_hash_table_lookup(idx->wordcache, word);
	if (cache) {
		/* freshen cache entry if we touch it */
		ibex_list_remove((struct _listnode *)cache);
		ibex_list_addtail(&idx->wordnodes, (struct _listnode *)cache);
		if (cache->filecount == 1 && cache->filealloc == 0) {
			if (cache->file.file0 == nameid)
				return TRUE;
		} else {
			for (i=0;i<cache->filecount;i++) {
				if (cache->file.files[i] == nameid)
					return TRUE;
			}
		}
		/* not there?  well we can use the wordid anyway */
		wordid = cache->wordid;
	} else {
		/* lookup the hash key for word */
		wordid = idx->wordindex->klass->find(idx->wordindex, word, strlen(word));
	}

	/* see if wordid is in nameblock */
	return idx->namestore->klass->find(idx->namestore, nameblock, nametail, wordid);
}

/* cache helper functions */
/* flush a cache entry to disk, and empty it out */
static void
sync_cache_entry(struct _IBEXWord *idx, struct _wordcache *cache)
{
	GArray array; /* just use this as a header */
	blockid_t oldblock, oldtail;
	
	d(printf("syncing cache entry '%s' used %d\n", cache->word, cache->filecount));
	if (cache->filecount == 1 && cache->filealloc == 0)
		array.data = (char *)&cache->file.file0;
	else
		array.data = (char *)cache->file.files;
	array.len = cache->filecount;
	oldblock = cache->wordblock;
	oldtail = cache->wordtail;
	idx->wordstore->klass->add_list(idx->wordstore, &cache->wordblock, &cache->wordtail, &array);
	if (oldblock != cache->wordblock || oldtail != cache->wordtail) {
		idx->wordindex->klass->set_data(idx->wordindex, cache->wordid, cache->wordblock, cache->wordtail);
	}
	cache->filecount = 0;
}

/* create a new key in an index, returning its id and head block */
static void
add_index_key(struct _IBEXIndex *wordindex, const char *word, nameid_t *wordid, blockid_t *wordblock, blockid_t *wordtail)
{
	/* initialise cache entry - id of word entry and head block */
	*wordid = wordindex->klass->find(wordindex, word, strlen(word));
	if (*wordid == 0) {
		*wordid = wordindex->klass->insert(wordindex, word, strlen(word));
		*wordblock = 0;
		*wordtail = 0;
	} else {
		*wordblock = wordindex->klass->get_data(wordindex, *wordid, wordtail);
	}
}

/* create a new key in a cached index (only word cache so far), flushing old keys
   if too much space is being used */
static struct _wordcache *
add_index_cache(struct _IBEXWord *idx, const char *word)
{
	struct _wordcache *cache;

	d(printf("adding %s to cache\n", word));

	cache = g_hash_table_lookup(idx->wordcache, word);
	if (cache == 0) {
		/* see if we have to flush off the last entry */
		if (idx->wordcount >= WORDCACHE_SIZE) {
			struct _wordcache *mincache;
			int min, count=0;
			/* remove last entry, and flush it */
			cache = (struct _wordcache *)idx->wordnodes.tailpred;
			mincache = cache;
			min = mincache->filecount;

			d(printf("flushing word from cache %s\n", cache->word));
			/* instead of just using the last entry, we try and find an entry with
			   with only 1 item (failing that, the smallest in the range we look at) */
			/* this could probably benefit greatly from a more sophisticated aging algorithm */
			while (cache->next && count < 100) {
				if (cache->filecount == 1) {
					mincache = cache;
					break;
				}
				if (cache->filecount > 0 && cache->filecount < min) {
					mincache = cache;
					min = cache->filecount;
				}
				cache = cache->next;
				count++;
			}
			ibex_list_remove((struct _listnode *)mincache);
			g_hash_table_remove(idx->wordcache, mincache->word);
			sync_cache_entry(idx, mincache);
			if (mincache->filealloc)
				g_free(mincache->file.files);
			g_free(mincache);
			idx->wordcount--;
		}
		cache = g_malloc0(sizeof(*cache)+strlen(word));
		/* initialise cache entry - id of word entry and head block */
		add_index_key(idx->wordindex, word, &cache->wordid, &cache->wordblock, &cache->wordtail);
		/* other fields */
		strcpy(cache->word, word);
		cache->filecount = 0;
		g_hash_table_insert(idx->wordcache, cache->word, cache);
		ibex_list_addhead(&idx->wordnodes, (struct _listnode *)cache);
		idx->wordcount++;
	} else {
		/* move cache bucket ot the head of the cache list */
		ibex_list_remove((struct _listnode *)cache);
		ibex_list_addhead(&idx->wordnodes, (struct _listnode *)cache);
	}
	return cache;
}

/* adds a single word to name (slow) */
static void add(struct _IBEXWord *idx, const char *name, const char *word)
{
	nameid_t nameid;
	blockid_t nameblock, newblock, nametail, newtail;
	struct _wordcache *cache;

	g_error("Dont use wordindex::add()");
	abort();

	cache = add_index_cache(idx, word);

	/* get the nameid and block start for this name */
	add_index_key(idx->nameindex, name, &nameid, &nameblock, &nametail);

	/* check for repeats of the last name - dont add anything */
	if (cache->filecount == 1 && cache->filealloc == 0) {
		if (cache->file.file0 == nameid)
			return;
	} else {
		if (cache->file.files[cache->filecount] == nameid)
			return;
	}

	/* see if we are setting the first, drop it in the union */
	if (cache->filecount == 0 && cache->filealloc == 0) {
		cache->file.file0 = nameid;
	} else if (cache->filecount == 1 && cache->filealloc == 0) {
		nameid_t saveid;
		/* we need to allocate space for words */
		saveid = cache->file.file0;
		cache->file.files = g_malloc(sizeof(cache->file.files[0]) * CACHE_FILE_COUNT);
		/* this could possibly grow as needed, but i wont for now */
		cache->filealloc = CACHE_FILE_COUNT;
		cache->file.files[0] = saveid;
		cache->file.files[1] = nameid;
	} else {
		cache->file.files[cache->filecount] = nameid;
	}

	cache->filecount++;

	/* if we are full, force a flush now */
	if (cache->filealloc && cache->filecount >= cache->filealloc) {
		sync_cache_entry(idx, cache);
	}

	newtail = nametail;
	newblock = nameblock;
	idx->namestore->klass->add(idx->namestore, &newblock, &newtail, cache->wordid);
	if (newblock != nameblock || newtail != nametail) {
		idx->nameindex->klass->set_data(idx->nameindex, nameid, newblock, newtail);
	}
}

/* adds a bunch of words to a given name */
static void add_list(struct _IBEXWord *idx, const char *name, GPtrArray *words)
{
	int i;
	GArray *data = g_array_new(0, 0, sizeof(nameid_t));
	blockid_t nameblock, newblock, nametail, newtail;
	nameid_t nameid;
	struct _wordcache *cache;

	d(printf("Adding words to name %s\n", name));

	d(cache_sanity((struct _wordcache *)idx->wordnodes.head));

	/* get the nameid and block start for this name */
	add_index_key(idx->nameindex, name, &nameid, &nameblock, &nametail);

	d(cache_sanity((struct _wordcache *)idx->wordnodes.head));

	for (i=0;i<words->len;i++) {
		char *word = words->pdata[i];

		cache = add_index_cache(idx, word);

		/*d(cache_sanity((struct _wordcache *)idx->wordnodes.head));*/

		/* check for duplicates; doesn't catch duplicates over an overflow boundary.  Watch me care. */
		if (cache->filecount == 0
		    /* the 1 item case */
		    || (cache->filecount == 1 && cache->filealloc == 0 && cache->file.file0 != nameid)
		    /* the normal case */
		    || (cache->filealloc > 0 && cache->file.files[cache->filecount-1] != nameid)) {

			/* see if we are setting the first, drop it in the union */
			if (cache->filecount == 0 && cache->filealloc == 0) {
				cache->file.file0 = nameid;
			} else if (cache->filecount == 1 && cache->filealloc == 0) {
				nameid_t saveid;
				/* we need to allocate space for words */
				saveid = cache->file.file0;
				cache->file.files = g_malloc(sizeof(cache->file.files[0]) * CACHE_FILE_COUNT);
				/* this could possibly grow as needed, but i wont for now */
				cache->filealloc = CACHE_FILE_COUNT;
				cache->file.files[0] = saveid;
				cache->file.files[1] = nameid;
			} else {
				cache->file.files[cache->filecount] = nameid;
			}

			cache->filecount++;

			/* if we are full, force a flush now */
			if (cache->filealloc && cache->filecount >= cache->filealloc) {
				sync_cache_entry(idx, cache);
			}

			/*d(cache_sanity((struct _wordcache *)idx->wordnodes.head));*/

			/* and append this wordid for this name in memory */
			g_array_append_val(data, cache->wordid);
		}

		/*d(cache_sanity((struct _wordcache *)idx->wordnodes.head));*/
	}

	d(cache_sanity((struct _wordcache *)idx->wordnodes.head));

	/* and append these word id's in one go */
	newblock = nameblock;
	newtail = nametail;
	idx->namestore->klass->add_list(idx->namestore, &newblock, &newtail, data);
	if (newblock != nameblock || newtail != nametail) {
		idx->nameindex->klass->set_data(idx->nameindex, nameid, newblock, newtail);
	}

	d(cache_sanity((struct _wordcache *)idx->wordnodes.head));

	g_array_free(data, TRUE);
}

/* sync any in-memory data to disk */
static int
word_sync(struct _IBEXWord *idx)
{
	/* we just flush also, save memory */
	word_flush(idx);

#if 0
	struct _wordcache *cache = (struct _wordcache *)idx->wordnodes.head;

	while (cache->next) {
		sync_cache_entry(idx, cache);
		cache = cache->next;
	}

	/*ibex_hash_dump(idx->wordindex);*/
	/*ibex_hash_dump(idx->nameindex);*/
#endif
	return 0;
}

/* sync and flush any in-memory data to disk and free it */
static int
word_flush(struct _IBEXWord *idx)
{
	struct _wordcache *cache = (struct _wordcache *)idx->wordnodes.head, *next;
	extern int block_log;
	int count= 0;
	int used=0, wasted=0;

	block_log = 0;

	next = cache->next;
	while (next) {
		count++;
		used += sizeof(struct _wordcache) + (cache->filealloc * sizeof(nameid_t));
		if (cache->filealloc)
			wasted += (cache->filealloc-cache->filecount)*sizeof(nameid_t);
		else
			wasted += (1-cache->filecount) * sizeof(nameid_t);

		/*printf("syncing word %s\n", cache->word);*/
		sync_cache_entry(idx, cache);
		g_hash_table_remove(idx->wordcache, cache->word);
		if (cache->filealloc)
			g_free(cache->file.files);
		g_free(cache);
		cache = next;
		next = cache->next;
	}

	printf("sync cache entries = %d\n used memory = %d\n wasted memory = %d\n", count, used, wasted);

	block_log = 0;
	ibex_list_new(&idx->wordnodes);
	idx->wordcount = 0;
	return 0;
}

static int word_close(struct _IBEXWord *idx)
{
	struct _wordcache *cache = (struct _wordcache *)idx->wordnodes.head, *next;
	extern int block_log;
	int count= 0;
	int used=0, wasted=0;

	block_log = 0;

	next = cache->next;
	while (next) {
		count++;
		used += sizeof(struct _wordcache) + (cache->filealloc * sizeof(nameid_t));
		if (cache->filealloc)
			wasted += (cache->filealloc-cache->filecount)*sizeof(nameid_t);
		else
			wasted += (1-cache->filecount) * sizeof(nameid_t);

		/*printf("closing word %s\n", cache->word);*/
		sync_cache_entry(idx, cache);
		if (cache->filealloc)
			g_free(cache->file.files);
		g_free(cache);
		cache = next;
		next = cache->next;
	}
	block_log = 0;

	printf("cache entries = %d\n used memory = %d\n wasted memory = %d\n", count, used, wasted);

	idx->namestore->klass->close(idx->namestore);
	idx->nameindex->klass->close(idx->nameindex);
	/*same as namestore:
	  idx->wordstore->klass->close(idx->wordstore);*/
	idx->wordindex->klass->close(idx->wordindex);
	g_hash_table_destroy(idx->wordcache);
	g_free(idx);

	return 0;
}
