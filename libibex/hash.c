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

/* hash based index mechanism */

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "block.h"
#include "index.h"

#define d(x)

#define HASH_SIZE (1024)
#define KEY_THRESHOLD (sizeof(struct _hashkey) + 4) /* minimum number of free bytes we worry about
						      maintaining free blocks for */
#define ARRAY_LEN(a) (sizeof(a)/sizeof(a[0]))

typedef guint32 hashid_t;

struct _HASHCursor {
	struct _IBEXCursor cursor;

	hashid_t block;
	unsigned int index;
	unsigned int size;
};

static struct _IBEXIndex *hash_create(struct _memcache *bc, int size);
static struct _IBEXIndex *hash_open(struct _memcache *bc, blockid_t root);
static int hash_sync(struct _IBEXIndex *index);
static int hash_close(struct _IBEXIndex *index);

static hashid_t hash_find(struct _IBEXIndex *index, const char *key, int keylen);
static void hash_remove(struct _IBEXIndex *index, const char *key, int keylen);
static hashid_t hash_insert(struct _IBEXIndex *index, const char *key, int keylen);
static char *hash_get_key(struct _IBEXIndex *index, hashid_t hashbucket, int *len);
static void hash_set_data_block(struct _IBEXIndex *index, hashid_t keyid, blockid_t blockid, blockid_t tail);
static blockid_t hash_get_data_block(struct _IBEXIndex *index, hashid_t keyid, blockid_t *tail);
static struct _IBEXCursor *hash_get_cursor(struct _IBEXIndex *index);

static struct _IBEXCursor *hash_cursor_create(struct _IBEXIndex *);
static void hash_cursor_close(struct _IBEXCursor *);
static guint32 hash_cursor_next(struct _IBEXCursor *);
static char *hash_cursor_next_key(struct _IBEXCursor *, int *keylenptr);

struct _IBEXIndexClass ibex_hash_class = {
	hash_create, hash_open,
	hash_sync, hash_close,
	hash_find,
	hash_remove,
	hash_insert,
	hash_get_key,
	hash_set_data_block,
	hash_get_data_block,
	hash_get_cursor,
};

struct _IBEXCursorClass ibex_hash_cursor_class = {
	hash_cursor_close,
	hash_cursor_next,
	hash_cursor_next_key
};

/* the reason we have the tail here is that otherwise we need to
   have a 32 bit blockid for the root node; which would make this
   structure the same size anyway, with about 24 wasted bits */
struct _hashkey {
	blockid_t next;		/* next in hash chain */
	blockid_t tail;
	unsigned int root:32-BLOCK_BITS;
	unsigned int keyoffset:BLOCK_BITS;
};

struct _hashblock {
	/*blockid_t next;*/		/* all key blocks linked together? */
	guint32 used;		/* elements used */
	union {
		struct _hashkey keys[(BLOCK_SIZE-4)/sizeof(struct _hashkey)];
		char keydata[BLOCK_SIZE-4];
	} hashblock_u;
};
#define hb_keys hashblock_u.keys
#define hb_keydata hashblock_u.keydata

/* size of block overhead + 2 key block overhead */
#define MAX_KEYLEN (BLOCK_SIZE - 4 - 12 - 12)

/* root block for a hash index */
struct _hashroot {
	hashid_t free;		/* free list */
	guint32 size;		/* how big the hash table is */
	hashid_t table[(BLOCK_SIZE-8)/sizeof(hashid_t)]; /* pointers to blocks of pointers */
};

struct _hashtableblock {
	hashid_t buckets[BLOCK_SIZE/sizeof(hashid_t)];
};

/* map a hash index to a block index */
#define HASH_INDEX(b) ((b) & (BLOCK_SIZE-1))
/* map a hash index to a block number */
#define HASH_BLOCK(b) ((b) & ~(BLOCK_SIZE-1))
/* map a block + index to a hash key */
#define HASH_KEY(b, i) (((b) & ~(BLOCK_SIZE-1)) | ((i) & (BLOCK_SIZE-1)))

/* taken from tdb/gdbm */
static unsigned int hash_key(const unsigned char *key, int keylen)
{
	char *newkey;
	newkey = alloca(keylen+1);
	memcpy(newkey, key, keylen);
	newkey[keylen]=0;
	return g_str_hash(newkey);
#if 0
	unsigned int value;	/* Used to compute the hash value.  */
	unsigned int  i;	/* Used to cycle through random values. */

	/* Set the initial value from the key size. */
	value = 0x238F13AF * keylen;
	for (i=0; i < keylen; i++) {
		value = (value + (key[i] << (i*5 % 24)));
	}

	value = (1103515243 * value + 12345);  

	return value;
#endif
}

/* create a new hash table, return a pointer to its root block */
static struct _IBEXIndex *
hash_create(struct _memcache *bc, int size)
{
	blockid_t root, block;
	struct _hashroot *hashroot;
	int i;
	struct _hashtableblock *table;
	struct _IBEXIndex *index;

	g_assert(size<=10240);

	d(printf("initialising hash table, size = %d\n", size));

	index = g_malloc0(sizeof(*index));
	index->blocks = bc;
	index->klass = &ibex_hash_class;
	root = ibex_block_get(bc);
	index->root = root;
	d(printf(" root = %d\n", root));
	hashroot = (struct _hashroot *)ibex_block_read(bc, root);
	hashroot->free = 0;
	hashroot->size = size;
	ibex_block_dirty((struct _block *)hashroot);
	for (i=0;i<size/(BLOCK_SIZE/sizeof(blockid_t));i++) {
		d(printf("initialising hash table index block %d\n", i));
		block = hashroot->table[i] = ibex_block_get(bc);
		table = (struct _hashtableblock *)ibex_block_read(bc, block);
		memset(table, 0, sizeof(table));
		ibex_block_dirty((struct _block *)table);
	}

	return index;
}

static struct _IBEXIndex *
hash_open(struct _memcache *bc, blockid_t root)
{
	struct _IBEXIndex *index;

	/* FIXME: check a 'magic', and the root for validity */

	index = g_malloc0(sizeof(*index));
	index->blocks = bc;
	index->root = root;
	index->klass = &ibex_hash_class;

	return index;
}


static int hash_sync(struct _IBEXIndex *index)
{
	/* nop, index always synced on disk (at least, to blocks) */
	return 0;
}

static int hash_close(struct _IBEXIndex *index)
{
#ifdef INDEX_STAT
	printf("Performed %d lookups, average %f depth\n", index->lookups, (double)index->lookup_total/index->lookups);
#endif
	g_free(index);
	return 0;
}

/* get an iterator class */
static struct _IBEXCursor *hash_get_cursor(struct _IBEXIndex *index)
{
	return hash_cursor_create(index);
}

/* convert a hashbucket id into a name */
static char *
hash_get_key(struct _IBEXIndex *index, hashid_t hashbucket, int *len)
{
	struct _hashblock *bucket;
	int ind;
	char *ret, *start, *end;

	if (hashbucket == 0) {
		if (len)
			*len = 0;
		return g_strdup("");
	}

	bucket = (struct _hashblock *)ibex_block_read(index->blocks, HASH_BLOCK(hashbucket));
	ind = HASH_INDEX(hashbucket);

	g_assert(ind < bucket->used);

	start = &bucket->hb_keydata[bucket->hb_keys[ind].keyoffset];
	if (ind == 0) {
		end = &bucket->hb_keydata[sizeof(bucket->hb_keydata)/sizeof(bucket->hb_keydata[0])];
	} else {
		end = &bucket->hb_keydata[bucket->hb_keys[ind-1].keyoffset];
	}
	
	ret = g_malloc(end-start+1);
	memcpy(ret, start, end-start);
	ret[end-start]=0;
	if (len)
		*len = end-start;
	return ret;
}

/* sigh, this is fnugly code ... */
static hashid_t
hash_find(struct _IBEXIndex *index, const char *key, int keylen)
{
	struct _hashroot *hashroot;
	guint32 hash;
	int hashentry;
	blockid_t hashtable;
	hashid_t hashbucket;
	struct _hashtableblock *table;

	g_assert(index != 0);
	g_assert(index->root != 0);

	d(printf("finding hash %.*s\n", keylen, key));

	/* truncate the key to the maximum size */
	if (keylen > MAX_KEYLEN)
		keylen = MAX_KEYLEN;

	hashroot = (struct _hashroot *)ibex_block_read(index->blocks, index->root);

	/* find the table containing this entry */
	hash = hash_key(key, keylen) % hashroot->size;
	hashtable = hashroot->table[hash / (BLOCK_SIZE/sizeof(blockid_t))];
	g_assert(hashtable != 0);
	table = (struct _hashtableblock *)ibex_block_read(index->blocks, hashtable);
	hashentry = hash % (BLOCK_SIZE/sizeof(blockid_t));
	/* and its bucket */
	hashbucket = table->buckets[hashentry];

#ifdef INDEX_STAT
	index->lookups++;
#endif
	/* go down the bucket chain, reading each entry till we are done ... */
	while (hashbucket != 0) {
		struct _hashblock *bucket;
		char *start, *end;
		int ind;

#ifdef INDEX_STAT
		index->lookup_total++;
#endif

		d(printf(" checking bucket %d\n", hashbucket));

		/* get the real bucket id from the hashbucket id */
		bucket = (struct _hashblock *)ibex_block_read(index->blocks, HASH_BLOCK(hashbucket));
		/* and get the key number within the block */
		ind = HASH_INDEX(hashbucket);

		g_assert(ind < bucket->used);

		start = &bucket->hb_keydata[bucket->hb_keys[ind].keyoffset];
		if (ind == 0) {
			end = &bucket->hb_keydata[sizeof(bucket->hb_keydata)/sizeof(bucket->hb_keydata[0])];
		} else {
			end = &bucket->hb_keydata[bucket->hb_keys[ind-1].keyoffset];
		}
		if ( (end-start) == keylen
		     && memcmp(start, key, keylen) == 0) {
			return hashbucket;
		}
		hashbucket = bucket->hb_keys[ind].next;
	}
	return 0;
}

/* compresses the bucket 'bucket', removing data
   at index 'index' */
static void
hash_compress(struct _hashblock *bucket, int index)
{
	int i;
	char *start, *end, *newstart;

	/* get start/end of area to zap */
	start = &bucket->hb_keydata[bucket->hb_keys[index].keyoffset];
	if (index == 0) {
		end = &bucket->hb_keydata[sizeof(bucket->hb_keydata)/sizeof(bucket->hb_keydata[0])];
	} else {
		end = &bucket->hb_keydata[bucket->hb_keys[index-1].keyoffset];
	}

	if (start == end)
		return;

	/* fixup data */
	newstart = &bucket->hb_keydata[bucket->hb_keys[bucket->used-1].keyoffset];
	memmove(newstart+(end-start), newstart, start-newstart);

	/* fixup key pointers */
	for (i=index;i<bucket->used;i++) {
		bucket->hb_keys[i].keyoffset += (end-start);
	}
	ibex_block_dirty((struct _block *)bucket);
}

/* make room 'len' for the key 'index' */
/* assumes key 'index' is already empty (0 length) */
static void
hash_expand(struct _hashblock *bucket, int index, int len)
{
	int i;
	char *end, *newstart;

	/* get start/end of area to zap */
	if (index == 0) {
		end = &bucket->hb_keydata[sizeof(bucket->hb_keydata)/sizeof(bucket->hb_keydata[0])];
	} else {
		end = &bucket->hb_keydata[bucket->hb_keys[index-1].keyoffset];
	}

	/* fixup data */
	newstart = &bucket->hb_keydata[bucket->hb_keys[bucket->used-1].keyoffset];
	memmove(newstart-len, newstart, end-newstart);

	/* fixup key pointers */
	for (i=index;i<bucket->used;i++) {
		bucket->hb_keys[i].keyoffset -= len;
	}
	ibex_block_dirty((struct _block *)bucket);
}

static void
hash_remove(struct _IBEXIndex *index, const char *key, int keylen)
{
	struct _hashroot *hashroot;
	guint32 hash;
	int hashentry;
	blockid_t hashtable;
	hashid_t hashbucket, hashprev;
	struct _hashtableblock *table;

	g_assert(index != 0);
	g_assert(index->root != 0);

	d(printf("removing hash %.*s\n", keylen, key));

	/* truncate the key to the maximum size */
	if (keylen > MAX_KEYLEN)
		keylen = MAX_KEYLEN;

	hashroot = (struct _hashroot *)ibex_block_read(index->blocks, index->root);

	/* find the table containing this entry */
	hash = hash_key(key, keylen) % hashroot->size;
	hashtable = hashroot->table[hash / (BLOCK_SIZE/sizeof(blockid_t))];
	table = (struct _hashtableblock *)ibex_block_read(index->blocks, hashtable);
	hashentry = hash % (BLOCK_SIZE/sizeof(blockid_t));
	/* and its bucket */
	hashbucket = table->buckets[hashentry];

	/* go down the bucket chain, reading each entry till we are done ... */
	hashprev = 0;
	while (hashbucket != 0) {
		struct _hashblock *bucket;
		char *start, *end;
		int ind;

		d(printf(" checking bucket %d\n", hashbucket));

		/* get the real bucket id from the hashbucket id */
		bucket = (struct _hashblock *)ibex_block_read(index->blocks, HASH_BLOCK(hashbucket));
		/* and get the key number within the block */
		ind = HASH_INDEX(hashbucket);

		g_assert(ind < bucket->used);

		start = &bucket->hb_keydata[bucket->hb_keys[ind].keyoffset];
		if (ind == 0) {
			end = &bucket->hb_keydata[sizeof(bucket->hb_keydata)/sizeof(bucket->hb_keydata[0])];
		} else {
			end = &bucket->hb_keydata[bucket->hb_keys[ind-1].keyoffset];
		}
		if ( (end-start) == keylen
		     && memcmp(start, key, keylen) == 0) {
			struct _hashblock *prevbucket;

			if (hashprev == 0) {
				/* unlink from hash chain */
				table->buckets[hashentry] = bucket->hb_keys[HASH_INDEX(hashbucket)].next;
				/* link into free list */
				bucket->hb_keys[HASH_INDEX(hashbucket)].next = hashroot->free;
				hashroot->free = hashbucket;
				/* compress away data */
				hash_compress(bucket, HASH_INDEX(hashbucket));
				ibex_block_dirty((struct _block *)bucket);
				ibex_block_dirty((struct _block *)table);
				ibex_block_dirty((struct _block *)hashroot);
			} else {
				prevbucket = (struct _hashblock *)ibex_block_read(index->blocks, HASH_BLOCK(hashprev));
				prevbucket->hb_keys[HASH_INDEX(hashprev)].next =
					bucket->hb_keys[ind].next;
				/* link into free list */
				bucket->hb_keys[ind].next = hashroot->free;
				hashroot->free = hashbucket;
				/* compress entry */
				hash_compress(bucket, ind);
				ibex_block_dirty((struct _block *)bucket);
				ibex_block_dirty((struct _block *)prevbucket);
				ibex_block_dirty((struct _block *)hashroot);
			}
			return;
		}
		hashprev = hashbucket;
		hashbucket = bucket->hb_keys[ind].next;
	}
}

/* set where the datablock is located */
static void
hash_set_data_block(struct _IBEXIndex *index, hashid_t keyid, blockid_t blockid, blockid_t tail)
{
	struct _hashblock *bucket;

	d(printf("setting data block hash %d to %d tail %d\n", keyid, blockid, tail));

	/* map to a block number */
	g_assert((blockid & (BLOCK_SIZE-1)) == 0);
	blockid >>= BLOCK_BITS;

	bucket = (struct _hashblock *)ibex_block_read(index->blocks, HASH_BLOCK(keyid));
	if (bucket->hb_keys[HASH_INDEX(keyid)].root != blockid
	    || bucket->hb_keys[HASH_INDEX(keyid)].tail != tail) {
		bucket->hb_keys[HASH_INDEX(keyid)].tail = tail;
		bucket->hb_keys[HASH_INDEX(keyid)].root = blockid;
		ibex_block_dirty((struct _block *)bucket);
	}
}

static blockid_t
hash_get_data_block(struct _IBEXIndex *index, hashid_t keyid, blockid_t *tail)
{
	struct _hashblock *bucket;

	d(printf("getting data block hash %d\n", keyid));

	if (keyid == 0) {
		if (tail)
			*tail = 0;
		return 0;
	}

	bucket = (struct _hashblock *)ibex_block_read(index->blocks, HASH_BLOCK(keyid));
	if (tail)
		*tail = bucket->hb_keys[HASH_INDEX(keyid)].tail;
	return bucket->hb_keys[HASH_INDEX(keyid)].root << BLOCK_BITS;
}

static hashid_t
hash_insert(struct _IBEXIndex *index, const char *key, int keylen)
{
	struct _hashroot *hashroot;
	guint32 hash;
	int hashentry;
	blockid_t hashtable;
	hashid_t hashbucket, keybucket, keyprev, keyfree;
	struct _hashtableblock *table;
	struct _hashblock *bucket;
	int count;

	g_assert(index != 0);
	g_assert(index->root != 0);

	/* truncate the key to the maximum size */
	if (keylen > MAX_KEYLEN)
		keylen = MAX_KEYLEN;

	d(printf("inserting hash %.*s\n", keylen, key));

	hashroot = (struct _hashroot *)ibex_block_read(index->blocks, index->root);

	/* find the table containing this entry */
	hash = hash_key(key, keylen) % hashroot->size;
	hashtable = hashroot->table[hash / (BLOCK_SIZE/sizeof(blockid_t))];
	table = (struct _hashtableblock *)ibex_block_read(index->blocks, hashtable);
	hashentry = hash % (BLOCK_SIZE/sizeof(blockid_t));
	/* and its bucket */
	hashbucket = table->buckets[hashentry];

	/* now look for a free slot, first try the free list */
	/* but dont try too hard if our key is just too long ... so just scan upto
	   4 blocks, but if we dont find a space, tough ... */
	keybucket = hashroot->free;
	keyprev = 0;
	count = 0;
	while (keybucket && count<4) {
		int space;
		
		d(printf(" checking free %d\n", keybucket));

		/* read the bucket containing this free key */
		bucket = (struct _hashblock *)ibex_block_read(index->blocks, HASH_BLOCK(keybucket));

		/* check if there is enough space for the key */
		space =  &bucket->hb_keydata[bucket->hb_keys[bucket->used-1].keyoffset]
			- (char *)&bucket->hb_keys[bucket->used];
		if (space >= keylen) {
			hash_expand(bucket, HASH_INDEX(keybucket), keylen);
			memcpy(&bucket->hb_keydata[bucket->hb_keys[HASH_INDEX(keybucket)].keyoffset],
			       key, keylen);

			/* check if there is free space still in this node, and there are no other empty blocks */
			keyfree = bucket->hb_keys[HASH_INDEX(keybucket)].next;
			if ((space-keylen) >= KEY_THRESHOLD) {
				int i;
				int head = ARRAY_LEN(bucket->hb_keydata);
				int found = FALSE;

				for (i=0;i<bucket->used;i++) {
					if (bucket->hb_keys[i].keyoffset == head) {
						/* already have a free slot in this block, leave it */
						found = TRUE;
						break;
					}
					head = bucket->hb_keys[i].keyoffset;
				}
				if (!found) {
					/* we should link in a new free slot for this node */
					bucket->hb_keys[bucket->used].next = bucket->hb_keys[HASH_INDEX(keybucket)].next;
					bucket->hb_keys[bucket->used].keyoffset = bucket->hb_keys[bucket->used-1].keyoffset;
					keyfree = HASH_KEY(HASH_BLOCK(keybucket), bucket->used);
					bucket->used++;
				}
			}
			/* link 'keyfree' back to the parent ... */
			if (keyprev == 0) {
				hashroot->free = keyfree;
				ibex_block_dirty((struct _block *)hashroot);
			} else {
				struct _hashblock *prevbucket;
				prevbucket = (struct _hashblock *)ibex_block_read(index->blocks, HASH_BLOCK(keyprev));
				prevbucket->hb_keys[HASH_INDEX(keyprev)].next = keyfree;
				ibex_block_dirty((struct _block *)prevbucket);
			}

			/* link into the hash chain */
			bucket->hb_keys[HASH_INDEX(keybucket)].next = hashbucket;
			bucket->hb_keys[HASH_INDEX(keybucket)].root = 0;
			bucket->hb_keys[HASH_INDEX(keybucket)].tail = 0;
			table->buckets[hashentry] = keybucket;
			ibex_block_dirty((struct _block *)table);
			ibex_block_dirty((struct _block *)bucket);

			d(printf(" new key id %d\n", keybucket));
			d(printf(" new free id %d\n", hashroot->free));

			return keybucket;
		}

		count++;
		keyprev = keybucket;
		keybucket = bucket->hb_keys[HASH_INDEX(keybucket)].next;
	}

	/* else create a new block ... */
	keybucket = ibex_block_get(index->blocks);
	bucket = (struct _hashblock *)ibex_block_read(index->blocks, keybucket);

	d(printf("creating new key bucket %d\n", keybucket));

	memset(bucket, 0, sizeof(*bucket));

	bucket->used = 2;
	/* first block, is the new key */
	bucket->hb_keys[0].keyoffset = ARRAY_LEN(bucket->hb_keydata) - keylen;
	memcpy(&bucket->hb_keydata[bucket->hb_keys[0].keyoffset], key, keylen);
	bucket->hb_keys[0].next = hashbucket;
	bucket->hb_keys[0].root = 0;
	bucket->hb_keys[0].tail = 0;

	table->buckets[hashentry] = HASH_KEY(keybucket, 0);

	/* next block is a free block, link into free list */
	bucket->hb_keys[1].keyoffset = bucket->hb_keys[0].keyoffset;
	bucket->hb_keys[1].next = hashroot->free;
	hashroot->free = HASH_KEY(keybucket, 1);

	ibex_block_dirty((struct _block *)hashroot);
	ibex_block_dirty((struct _block *)table);
	ibex_block_dirty((struct _block *)bucket);

	d(printf(" new key id %d\n", HASH_KEY(keybucket, 0)));
	d(printf(" new free id %d\n", hashroot->free));

	return HASH_KEY(keybucket, 0);
}

/* hash cursor functions */
static struct _IBEXCursor *
hash_cursor_create(struct _IBEXIndex *idx)
{
	struct _HASHCursor *idc;
	struct _hashroot *hashroot;

	idc = g_malloc(sizeof(*idc));
	idc->cursor.klass = &ibex_hash_cursor_class;
	idc->cursor.index = idx;
	idc->block = 0;
	idc->index = 0;

	hashroot = (struct _hashroot *)ibex_block_read(idx->blocks, idx->root);
	idc->size = hashroot->size;

	return &idc->cursor;
}

static void
hash_cursor_close(struct _IBEXCursor *idc)
{
	g_free(idc);
}

static guint32
hash_cursor_next(struct _IBEXCursor *idc)
{
	struct _HASHCursor *hc = (struct _HASHCursor *)idc;
	struct _hashroot *hashroot;
	struct _hashblock *bucket;
	struct _hashtableblock *table;

	/* get the next bucket chain */
	if (hc->block != 0) {
		int ind;

		bucket = (struct _hashblock *)ibex_block_read(idc->index->blocks, HASH_BLOCK(hc->block));
		ind = HASH_INDEX(hc->block);

		g_assert(ind < bucket->used);

		hc->block = bucket->hb_keys[ind].next;
	}

	if (hc->block == 0) {
		hashroot = (struct _hashroot *)ibex_block_read(idc->index->blocks, idc->index->root);
		while (hc->block == 0 && hc->index < hc->size) {
			table = (struct _hashtableblock *)
				ibex_block_read(idc->index->blocks,
						hashroot->table[hc->index / (BLOCK_SIZE/sizeof(blockid_t))]);
			hc->block = table->buckets[hc->index % (BLOCK_SIZE/sizeof(blockid_t))];

			hc->index++;
		}
	}

	return hc->block;
}

static char *
hash_cursor_next_key(struct _IBEXCursor *idc, int *keylenptr)
{
	/* TODO: this could be made slightly mroe efficient going to the structs direct.
	   but i'm lazy today */
	return idc->index->klass->get_key(idc->index, idc->klass->next(idc), keylenptr);
}

/* debug */
void ibex_hash_dump(struct _IBEXIndex *index);
static void ibex_hash_dump_rec(struct _IBEXIndex *index, int *words, int *wordslen);

void ibex_hash_dump(struct _IBEXIndex *index)
{
	int words = 0, wordslen=0;

	ibex_hash_dump_rec(index, &words, &wordslen);

	printf("Total words = %d, bytes = %d, ave length = %f\n", words, wordslen, (double)wordslen/(double)words);
}


static void
ibex_hash_dump_rec(struct _IBEXIndex *index, int *words, int *wordslen)
{
	int i;
	struct _hashtableblock *table;
	struct _hashblock *bucket;
	struct _hashroot *hashroot;
	blockid_t hashtable;
	hashid_t hashbucket;

	printf("Walking hash tree:\n");
	hashroot = (struct _hashroot *)ibex_block_read(index->blocks, index->root);
	for (i=0;i<hashroot->size;i++) {
		printf("Hash table chain: %d\n", i);
		hashtable = hashroot->table[i / (BLOCK_SIZE/sizeof(blockid_t))];
		table = (struct _hashtableblock *)ibex_block_read(index->blocks, hashtable);
		hashbucket = table->buckets[i % (BLOCK_SIZE/sizeof(blockid_t))];
		while (hashbucket) {
			int len;

			*words = *words + 1;

			bucket = (struct _hashblock *)ibex_block_read(index->blocks, HASH_BLOCK(hashbucket));
			printf(" bucket %d: [used %d]", hashbucket, bucket->used);
			if (HASH_INDEX(hashbucket) == 0) {
				len = ARRAY_LEN(bucket->hb_keydata) -
					bucket->hb_keys[HASH_INDEX(hashbucket)].keyoffset;
			} else {
				len = bucket->hb_keys[HASH_INDEX(hashbucket)-1].keyoffset -
					bucket->hb_keys[HASH_INDEX(hashbucket)].keyoffset;
			}
			printf("'%.*s' = %d next=%d\n", len, &bucket->hb_keydata[bucket->hb_keys[HASH_INDEX(hashbucket)].keyoffset],
			       bucket->hb_keys[HASH_INDEX(hashbucket)].root,
			       bucket->hb_keys[HASH_INDEX(hashbucket)].next);

			*wordslen = *wordslen + len;

			ibex_diskarray_dump(index->blocks,
					    bucket->hb_keys[HASH_INDEX(hashbucket)].root << BLOCK_BITS,
					    bucket->hb_keys[HASH_INDEX(hashbucket)].tail);

			hashbucket = bucket->hb_keys[HASH_INDEX(hashbucket)].next;
		}
		/* make sure its still in the cache */
		hashroot = (struct _hashroot *)ibex_block_read(index->blocks, index->root);
	}

	hashbucket = hashroot->free;
	printf("Dumping free lists ..\n");
	while (hashbucket) {
		printf(" %d", hashbucket);
		bucket = (struct _hashblock *)ibex_block_read(index->blocks, HASH_BLOCK(hashbucket));
		hashbucket = bucket->hb_keys[HASH_INDEX(hashbucket)].next;
	}
	printf("\n");
}

#if 0
int main(int argc, char **argv)
{
	struct _memcache *bc;
	struct _IBEXIndex *hash;
	int i;

	bc = ibex_block_cache_open("index.db", O_CREAT|O_RDWR, 0600);
	hash = ibex_hash_class.create(bc, 1024);
	for (i=0;i<10000;i++) {
		char key[16];
		sprintf(key, "key %d", i);
		ibex_hash_class.insert(hash, key, strlen(key));
	}

	for (i=500;i<1000;i++) {
		char key[16];
		sprintf(key, "key %d", i);
		ibex_hash_class.remove(hash, key, strlen(key));
	}

	for (i=500;i<1000;i++) {
		char key[16];
		sprintf(key, "key %d", i);
		ibex_hash_class.insert(hash, key, strlen(key));
	}


	ibex_hash_dump(hash);

	for (i=0;i<2000;i++) {
		char key[16], *lookup;
		hashid_t keyid;
		blockid_t root, tail;

		sprintf(key, "key %d", i);
		keyid = ibex_hash_class.find(hash, key, strlen(key));
		lookup = ibex_hash_class.get_key(hash, keyid, 0);
		root = ibex_hash_class.get_data(hash, keyid, &tail);
		printf("key %s = %d = '%s' root:%d tail:%d \n", key, keyid, lookup, root, tail);
		g_free(lookup);
	}

	ibex_hash_class.close(hash);
	ibex_block_cache_close(bc);
	return 0;
}

#endif
