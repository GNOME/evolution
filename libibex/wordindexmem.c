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

/* this is the same as wordindex.c, but it doesn't have an LRU cache
   for word names.  it has a lookup tab le that is only loaded if
   index-pre is called, otherwise it always hits disk */

/* code to manage a word index */
/* includes a cache for word index writes,
   but not for name index writes (currently), or any reads.

Note the word cache is only needed during indexing of lots
of words, and could then be discarded (:flush()).

*/

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib.h>

#include "block.h"
#include "index.h"
#include "wordindex.h"

/*#define MALLOC_CHECK*/

#ifdef MALLOC_CHECK
#include <mcheck.h>
#endif

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

static void sync_cache_entry(struct _IBEXWord *idx, struct _wordcache *cache);

struct _IBEXWordClass ibex_word_index_mem_class = {
	word_sync, word_flush, word_close,
	word_index_pre, word_index_post,
	unindex_name, contains_name,
	find, find_name,
	add, add_list
};

#ifdef MALLOC_CHECK
static void
checkmem(void *p)
{
	if (p) {
		int status = mprobe(p);

		switch (status) {
		case MCHECK_HEAD:
			printf("Memory underrun at %p\n", p);
			abort();
		case MCHECK_TAIL:
			printf("Memory overrun at %p\n", p);
			abort();
		case MCHECK_FREE:
			printf("Double free %p\n", p);
			abort();
		}
	}
}
#endif

/* this interface isn't the best, but it'll do for now */
struct _IBEXWord *
ibex_create_word_index_mem(struct _memcache *bc, blockid_t *wordroot, blockid_t *nameroot)
{
	struct _IBEXWord *idx;

	idx = g_malloc0(sizeof(*idx));
	idx->wordcache = g_hash_table_new(g_str_hash, g_str_equal);
	ibex_list_new(&idx->wordnodes);
	idx->wordcount = 0;
	idx->precount = 0;
	idx->namecache = g_hash_table_new(g_str_hash, g_str_equal);
	idx->nameinit = 0;
	idx->klass = &ibex_word_index_mem_class;

	/* we use the same block array storage for both indexes at the moment */
	idx->wordstore = ibex_diskarray_class.create(bc);
	idx->namestore = idx->wordstore;

	/* but not the same indexes! */
	if (*wordroot) {
		d(printf("opening wordindex root = %d\n", *wordroot));
		idx->wordindex = ibex_hash_class.open(bc, *wordroot);
	} else {
		idx->wordindex = ibex_hash_class.create(bc, 2048);
		*wordroot = idx->wordindex->root;
		d(printf("creating wordindex root = %d\n", *wordroot));
	}
	if (*nameroot) {
		d(printf("opening nameindex root = %d\n", *nameroot));
		idx->nameindex = ibex_hash_class.open(bc, *nameroot);
	} else {
		idx->nameindex = ibex_hash_class.create(bc, 2048);
		*nameroot = idx->nameindex->root;
		d(printf("creating nameindex root = %d\n", *nameroot));
	}
	return idx;
}

static void
node_sanity(char *key, struct _wordcache *node, void *data)
{
	g_assert(node->filecount <= node->filealloc || (node->filecount == 1 && node->filealloc == 0));
	g_assert(strlen(node->word) != 0);
#ifdef MALLOC_CHECK
	checkmem(node);
	if (node->filealloc)
		checkmem(node->file.files);
#endif
}

static void
cache_sanity(struct _IBEXWord *idx)
{
#ifdef MALLOC_CHECK
	checkmem(idx);
#endif
	g_hash_table_foreach(idx->wordcache, (GHFunc)node_sanity, idx);
}

static void word_index_pre(struct _IBEXWord *idx)
{
	struct _IBEXCursor *idc;
	struct _wordcache *cache;
	nameid_t wordid;
	char *key;
	int len;

	idx->precount ++;
	if (idx->precount > 1)
		return;

	/* want to load all words into the cache lookup table */
	d(printf("pre-loading all word info into memory\n"));
	idc = idx->wordindex->klass->get_cursor(idx->wordindex);
	while ( (wordid = idc->klass->next(idc)) ) {
		key = idc->index->klass->get_key(idc->index, wordid, &len);
		/*d(printf("Adding word %s\n", key));*/
		cache = g_malloc0(sizeof(*cache) + strlen(key));
		strcpy(cache->word, key);
		g_free(key);
		cache->wordid = wordid;
		cache->wordblock = idc->index->klass->get_data(idc->index, wordid, &cache->wordtail);
		cache->filecount = 0;
		cache->filealloc = 0;
		g_hash_table_insert(idx->wordcache, cache->word, cache);
		idx->wordcount++;
	}

#ifdef MALLOC_CHECK
	cache_sanity(idx);
#endif

	idc->klass->close(idc);

	d(printf("done\n"));
}

static gboolean
sync_free_value(void *key, void *value, void *data)
{
	struct _wordcache *cache = (struct _wordcache *)value;
	struct _IBEXWord *idx = (struct _IBEXWord *)data;

	sync_cache_entry(idx, cache);
	if (cache->filealloc)
		g_free(cache->file.files);
	g_free(cache);

	return TRUE;
}

static void
sync_value(void *key, void *value, void *data)
{
	struct _wordcache *cache = (struct _wordcache *)value;
	struct _IBEXWord *idx = (struct _IBEXWord *)data;

	sync_cache_entry(idx, cache);
}

static void word_index_post(struct _IBEXWord *idx)
{
	idx->precount--;
	if (idx->precount > 0)
		return;
	idx->precount = 0;

#ifdef MALLOC_CHECK
	cache_sanity(idx);
#endif

	g_hash_table_foreach_remove(idx->wordcache, sync_free_value, idx);
	idx->wordcount = 0;
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

	/* if we have a namecache, check that to see if we need to remove that item, or there is no work here */
	if (idx->nameinit) {
		char *oldkey;
		gboolean oldval;

		if (g_hash_table_lookup_extended(idx->namecache, name, (void *)&oldkey, (void *)&oldval)) {
			g_hash_table_remove(idx->namecache, oldkey);
			g_free(oldkey);
		} else {
			return;
		}
	}

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
	struct _IBEXCursor *idc;
	nameid_t wordid;
	char *key;
	int len;

	/* load all the names into memory, since we're *usually* about to do a lot of these */

	/* Note that because of the (poor) hash algorithm, this is >> faster than
	   looking up every key in turn.  Basically because all keys are stored packed
	   in the same list, not in buckets of keys for the same hash (among other reasons) */

	if (!idx->nameinit) {
		d(printf("pre-loading all name info into memory\n"));
		idc = idx->nameindex->klass->get_cursor(idx->nameindex);
		while ( (wordid = idc->klass->next(idc)) ) {
			key = idc->index->klass->get_key(idc->index, wordid, &len);
			g_hash_table_insert(idx->namecache, key, (void *)TRUE);
		}
		idc->klass->close(idc);
		idx->nameinit = TRUE;
	}

	return (gboolean)g_hash_table_lookup(idx->namecache, name);
	/*return idx->nameindex->klass->find(idx->nameindex, name, strlen(name)) != 0;*/
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
#if 0
		/* freshen cache entry if we touch it */
		ibex_list_remove((struct _listnode *)cache);
		ibex_list_addtail(&idx->wordnodes, (struct _listnode *)cache);
#endif
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

	/* if we have the namelist in memory, quick-check that */
	if (idx->nameinit && g_hash_table_lookup(idx->namecache, name) == NULL)
		return FALSE;

	/* lookup the hash key for the name */
	nameid = idx->nameindex->klass->find(idx->nameindex, name, strlen(name));
	/* get the block for this name */
	nameblock = idx->nameindex->klass->get_data(idx->nameindex, nameid, &nametail);

	/* check if there is an in-memory cache for this word, check its file there first */
	cache = g_hash_table_lookup(idx->wordcache, word);
	if (cache) {
#if 0
		/* freshen cache entry if we touch it */
		ibex_list_remove((struct _listnode *)cache);
		ibex_list_addtail(&idx->wordnodes, (struct _listnode *)cache);
#endif
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
	
	if (cache->filecount == 0)
		return;

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

	cache = g_hash_table_lookup(idx->wordcache, word);
	if (cache == 0) {
		/*d(printf("adding %s to cache\n", word));*/

#if 0
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
#endif
		cache = g_malloc0(sizeof(*cache)+strlen(word));
		/* if we're in an index state, we can assume we dont have it if we dont have it in memory */
		if (idx->precount == 0) {
			/* initialise cache entry - id of word entry and head block */
			add_index_key(idx->wordindex, word, &cache->wordid, &cache->wordblock, &cache->wordtail);
		} else {
			cache->wordid = idx->wordindex->klass->insert(idx->wordindex, word, strlen(word));
		}
		/* other fields */
		strcpy(cache->word, word);
		cache->filecount = 0;
		g_hash_table_insert(idx->wordcache, cache->word, cache);
#if 0
		ibex_list_addhead(&idx->wordnodes, (struct _listnode *)cache);
#endif
		idx->wordcount++;
	} else {
		/*d(printf("already have %s in cache\n", word));*/
#if 0
		/* move cache bucket ot the head of the cache list */
		ibex_list_remove((struct _listnode *)cache);
		ibex_list_addhead(&idx->wordnodes, (struct _listnode *)cache);
#endif
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

	d(cache_sanity(idx));

	/* make sure we keep the namecache in sync, if it is active */
	if (idx->nameinit && g_hash_table_lookup(idx->namecache, name) == NULL) {
		g_hash_table_insert(idx->namecache, g_strdup(name), (void *)TRUE);
		/* we know we dont have it in the disk hash either, so we insert anew (saves a lookup) */
		nameid = idx->nameindex->klass->insert(idx->nameindex, name, strlen(name));
		nameblock = 0;
		nametail = 0;
	} else {
		/* get the nameid and block start for this name */
		add_index_key(idx->nameindex, name, &nameid, &nameblock, &nametail);
	}

	d(cache_sanity(idx));

	for (i=0;i<words->len;i++) {
		char *word = words->pdata[i];

		cache = add_index_cache(idx, word);

		/*d(cache_sanity(idx));*/

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

			/*d(cache_sanity(idx));*/

			/* and append this wordid for this name in memory */
			g_array_append_val(data, cache->wordid);
		}

		/*d(cache_sanity(idx));*/
	}

	d(cache_sanity(idx));

	/* and append these word id's in one go */
	newblock = nameblock;
	newtail = nametail;
	idx->namestore->klass->add_list(idx->namestore, &newblock, &newtail, data);
	if (newblock != nameblock || newtail != nametail) {
		idx->nameindex->klass->set_data(idx->nameindex, nameid, newblock, newtail);
	}

	d(cache_sanity(idx));

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

static gboolean
free_key(void *key, void *value, void *data)
{
	g_free(key);

	return TRUE;
}

/* sync and flush any in-memory data to disk and free it */
static int
word_flush(struct _IBEXWord *idx)
{
	d(cache_sanity(idx));

	g_hash_table_foreach_remove(idx->wordcache, sync_free_value, idx);
	idx->wordcount = 0;
	if (idx->nameinit) {
		g_hash_table_foreach_remove(idx->namecache, free_key, NULL);
		idx->nameinit = FALSE;
	}
	return 0;
}

static int word_close(struct _IBEXWord *idx)
{
	idx->klass->flush(idx);

	idx->namestore->klass->close(idx->namestore);
	idx->nameindex->klass->close(idx->nameindex);
	/*same as namestore:
	  idx->wordstore->klass->close(idx->wordstore);*/
	idx->wordindex->klass->close(idx->wordindex);
	g_hash_table_destroy(idx->wordcache);
	g_hash_table_destroy(idx->namecache);
	g_free(idx);

	return 0;
}

/* debugging/tuning function */

struct _stats {
	int memcache;		/* total memory used by cache entries */
	int memfile;		/* total mem ysed by file data */
	int memfileused;	/* actual memory used by file data */
	int memword;		/* total mem used by words */
	int file1;		/* total file entries with only 1 entry */
	int total;
};

static void
get_info(void *key, void *value, void *data)
{
	struct _wordcache *cache = (struct _wordcache *)value;
	struct _stats *stats = (struct _stats *)data;

	/* round up to probable alignment, + malloc overheads */
	stats->memcache += ((sizeof(struct _wordcache) + strlen(cache->word) + 4 + 3) & ~3);
	if (cache->filealloc > 0) {
		/* size of file array data */
		stats->memcache += sizeof(nameid_t) * cache->filealloc + 4;
		/* actual used memory */
		stats->memfile += sizeof(nameid_t) * cache->filealloc;
		stats->memfileused += sizeof(nameid_t) * cache->filecount;
	}
	if (cache->filecount == 1 && cache->filealloc == 0)
		stats->file1++;

	stats->memword += strlen(cache->word);
	stats->total++;
}

static char *
num(int num)
{
	int n;
	static char buf[256];
	char *p = buf;
	char type = 0;

	n = num;
	if (n>1000000) {
		p+= sprintf(p, "%d ", n/1000000);
		n -= (n/1000000)*1000000;
		type = 'M';
	}
	if (n>1000) {
		if (num>1000000)
			p+= sprintf(p, "%03d ", n/1000);
		else
			p+= sprintf(p, "%d ", n/1000);
		n -= (n/1000)*1000;
		if (type == 0)
			type = 'K';
	}
	if (num > 1000)
		p += sprintf(p, "%03d", n);
	else
		p += sprintf(p, "%d", n);

	n = num;
	switch (type) {
	case 'M':
		p += sprintf(p, ", %d.%02dM", n/1024/1024, n*100/1024/1024);
		break;
	case 'K':
		p += sprintf(p, ", %d.%02dK", n/1024, n*100/1024);
		break;
	case 0:
		break;
	}

	return buf;
}

void word_index_mem_dump_info(struct _IBEXWord *idx);

void word_index_mem_dump_info(struct _IBEXWord *idx)
{
	struct _stats stats = { 0 };
	int useful;

	g_hash_table_foreach(idx->wordcache, get_info, &stats);

	useful = stats.total * sizeof(struct _wordcache) + stats.memword + stats.memfile;

	printf("Word Index Stats:\n");
	printf("Total word count: %d\n", stats.total);
	printf("Total memory used: %s\n", num(stats.memcache));
	printf("Total useful memory: %s\n", num(useful));
	printf("Total malloc/alignment overhead: %s\n", num(stats.memcache - useful));
	printf("Total buffer overhead: %s\n", num(stats.memfile - stats.memfileused));
	printf("Space taken by words: %s\n", num(stats.memword + stats.total));
	printf("Number of 1-word entries: %s\n", num(stats.file1));
	if (stats.memcache > 0)
		printf("%% unused space: %d %%\n", (stats.memfile - stats.memfileused) * 100 / stats.memcache);
}

