/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2003 Ximian Inc.
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "libedataserver/e-msgport.h"

#include "camel-block-file.h"
#include "camel-file-utils.h"

#define d(x) /*(printf("%s(%d):%s: ",  __FILE__, __LINE__, __PRETTY_FUNCTION__),(x))*/

/* Locks must be obtained in the order defined */

struct _CamelBlockFilePrivate {
	/* We use the private structure to form our lru list from */
	struct _CamelBlockFilePrivate *next;
	struct _CamelBlockFilePrivate *prev;

	struct _CamelBlockFile *base;
	
	pthread_mutex_t root_lock; /* for modifying the root block */
	pthread_mutex_t cache_lock; /* for refcounting, flag manip, cache manip */
	pthread_mutex_t io_lock; /* for all io ops */
	
	unsigned int deleted:1;
};


#define CAMEL_BLOCK_FILE_LOCK(kf, lock) (pthread_mutex_lock(&(kf)->priv->lock))
#define CAMEL_BLOCK_FILE_TRYLOCK(kf, lock) (pthread_mutex_trylock(&(kf)->priv->lock))
#define CAMEL_BLOCK_FILE_UNLOCK(kf, lock) (pthread_mutex_unlock(&(kf)->priv->lock))

#define LOCK(x) pthread_mutex_lock(&x)
#define UNLOCK(x) pthread_mutex_unlock(&x)

static pthread_mutex_t block_file_lock = PTHREAD_MUTEX_INITIALIZER;

/* lru cache of block files */
static EDList block_file_list = E_DLIST_INITIALISER(block_file_list);
/* list to store block files that are actually intialised */
static EDList block_file_active_list = E_DLIST_INITIALISER(block_file_active_list);
static int block_file_count = 0;
static int block_file_threshhold = 10;

#define CBF_CLASS(o) ((CamelBlockFileClass *)(((CamelObject *)o)->klass))

static int sync_nolock(CamelBlockFile *bs);
static int sync_block_nolock(CamelBlockFile *bs, CamelBlock *bl);

static int
block_file_validate_root(CamelBlockFile *bs)
{
	struct stat st;
	CamelBlockRoot *br;
	int s;

	br = bs->root;

	s = fstat(bs->fd, &st);

	d(printf("Validate root: '%s'\n", bs->path));
	d(printf("version: %.8s (%.8s)\n", bs->root->version, bs->version));
	d(printf("block size: %d (%d)%s\n", br->block_size, bs->block_size,
		br->block_size != bs->block_size ? " BAD":" OK"));
	d(printf("free: %ld (%d add size < %ld)%s\n", (long)br->free, br->free / bs->block_size * bs->block_size, (long)st.st_size,
		(br->free > st.st_size) || (br->free % bs->block_size) != 0 ? " BAD":" OK"));
	d(printf("last: %ld (%d and size: %ld)%s\n", (long)br->last, br->last / bs->block_size * bs->block_size, (long)st.st_size,
		(br->last != st.st_size) || ((br->last % bs->block_size) != 0) ? " BAD": " OK"));
	d(printf("flags: %s\n", (br->flags & CAMEL_BLOCK_FILE_SYNC)?"SYNC":"unSYNC"));

	if (br->last == 0
	    || memcmp(bs->root->version, bs->version, 8) != 0
	    || br->block_size != bs->block_size
	    || (br->free % bs->block_size) != 0
	    || (br->last % bs->block_size) != 0
	    || fstat(bs->fd, &st) == -1
	    || st.st_size != br->last
	    || br->free > st.st_size
	    || (br->flags & CAMEL_BLOCK_FILE_SYNC) == 0) {
		if (s != -1 && st.st_size > 0) {
			g_warning("Invalid root: '%s'", bs->path);
			g_warning("version: %.8s (%.8s)", bs->root->version, bs->version);
			g_warning("block size: %d (%d)%s", br->block_size, bs->block_size,
				  br->block_size != bs->block_size ? " BAD":" OK");
			g_warning("free: %ld (%d add size < %ld)%s", (long)br->free, br->free / bs->block_size * bs->block_size, (long)st.st_size,
				  (br->free > st.st_size) || (br->free % bs->block_size) != 0 ? " BAD":" OK");
			g_warning("last: %ld (%d and size: %ld)%s", (long)br->last, br->last / bs->block_size * bs->block_size, (long)st.st_size,
				  (br->last != st.st_size) || ((br->last % bs->block_size) != 0) ? " BAD": " OK");
			g_warning("flags: %s", (br->flags & CAMEL_BLOCK_FILE_SYNC)?"SYNC":"unSYNC");
		}
		return -1;
	}

	return 0;
}

static int
block_file_init_root(CamelBlockFile *bs)
{
	CamelBlockRoot *br = bs->root;

	memset(br, 0, bs->block_size);
	memcpy(br->version, bs->version, 8);
	br->last = bs->block_size;
	br->flags = CAMEL_BLOCK_FILE_SYNC;
	br->free = 0;
	br->block_size = bs->block_size;

	return 0;
}

static void
camel_block_file_class_init(CamelBlockFileClass *klass)
{
	klass->validate_root = block_file_validate_root;
	klass->init_root = block_file_init_root;
}

static guint
block_hash_func(const void *v)
{
	return ((camel_block_t) GPOINTER_TO_UINT(v)) >> CAMEL_BLOCK_SIZE_BITS;
}

static void
camel_block_file_init(CamelBlockFile *bs)
{
	struct _CamelBlockFilePrivate *p;

	bs->fd = -1;
	bs->block_size = CAMEL_BLOCK_SIZE;
	e_dlist_init(&bs->block_cache);
	bs->blocks = g_hash_table_new((GHashFunc)block_hash_func, NULL);
	/* this cache size and the text index size have been tuned for about the best
	   with moderate memory usage.  Doubling the memory usage barely affects performance. */
	bs->block_cache_limit = 256;

	p = bs->priv = g_malloc0(sizeof(*bs->priv));
	p->base = bs;
	
	pthread_mutex_init(&p->root_lock, NULL);
	pthread_mutex_init(&p->cache_lock, NULL);
	pthread_mutex_init(&p->io_lock, NULL);
	
	/* link into lru list */
	LOCK(block_file_lock);
	e_dlist_addhead(&block_file_list, (EDListNode *)p);

#if 0
	{
		printf("dumping block list\n");
		printf(" head = %p p = %p\n", block_file_list.head, p);
		p = block_file_list.head;
		while (p->next) {
			printf(" '%s'\n", p->base->path);
			p = p->next;
		}
	}
#endif

	UNLOCK(block_file_lock);
}

static void
camel_block_file_finalise(CamelBlockFile *bs)
{
	CamelBlock *bl, *bn;
	struct _CamelBlockFilePrivate *p;

	p = bs->priv;

	if (bs->root_block)
		camel_block_file_sync(bs);

	/* remove from lru list */
	LOCK(block_file_lock);
	if (bs->fd != -1)
		block_file_count--;
	e_dlist_remove((EDListNode *)p);
	UNLOCK(block_file_lock);

	bl = (CamelBlock *)bs->block_cache.head;
	bn = bl->next;
	while (bn) {
		if (bl->refcount != 0)
			g_warning("Block '%d' still referenced", bl->id);
		g_free(bl);
		bl = bn;
		bn = bn->next;
	}
	
	g_hash_table_destroy (bs->blocks);
	
	if (bs->root_block)
		camel_block_file_unref_block(bs, bs->root_block);
	g_free(bs->path);
	if (bs->fd != -1)
		close(bs->fd);
	
	pthread_mutex_destroy(&p->io_lock);
	pthread_mutex_destroy(&p->cache_lock);
	pthread_mutex_destroy(&p->root_lock);
	
	g_free(p);
}

CamelType
camel_block_file_get_type(void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(camel_object_get_type(), "CamelBlockFile",
					   sizeof (CamelBlockFile),
					   sizeof (CamelBlockFileClass),
					   (CamelObjectClassInitFunc) camel_block_file_class_init,
					   NULL,
					   (CamelObjectInitFunc) camel_block_file_init,
					   (CamelObjectFinalizeFunc) camel_block_file_finalise);
	}
	
	return type;
}

/* 'use' a block file for io */
static int
block_file_use(CamelBlockFile *bs)
{
	struct _CamelBlockFilePrivate *nw, *nn, *p = bs->priv;
	CamelBlockFile *bf;
	int err;

	/* We want to:
	    remove file from active list
	    lock it

	   Then when done:
	    unlock it
	    add it back to end of active list
	*/

	CAMEL_BLOCK_FILE_LOCK(bs, io_lock);

	if (bs->fd != -1)
		return 0;
	else if (p->deleted) {
		CAMEL_BLOCK_FILE_UNLOCK(bs, io_lock);
		errno = ENOENT;
		return -1;
	} else
		d(printf("Turning block file online: %s\n", bs->path));

	if ((bs->fd = open(bs->path, bs->flags, 0600)) == -1) {
		err = errno;
		CAMEL_BLOCK_FILE_UNLOCK(bs, io_lock);
		errno = err;
		return -1;
	}

	LOCK(block_file_lock);
	e_dlist_remove((EDListNode *)p);
	e_dlist_addtail(&block_file_active_list, (EDListNode *)p);

	block_file_count++;

	nw = (struct _CamelBlockFilePrivate *)block_file_list.head;
	nn = nw->next;
	while (block_file_count > block_file_threshhold && nn) {
		/* We never hit the current blockfile here, as its removed from the list first */
		bf = nw->base;
		if (bf->fd != -1) {
			/* Need to trylock, as any of these lock levels might be trying
			   to lock the block_file_lock, so we need to check and abort if so */
			if (CAMEL_BLOCK_FILE_TRYLOCK(bf, root_lock) == 0) {
				if (CAMEL_BLOCK_FILE_TRYLOCK(bf, cache_lock) == 0) {
					if (CAMEL_BLOCK_FILE_TRYLOCK(bf, io_lock) == 0) {
						d(printf("[%d] Turning block file offline: %s\n", block_file_count-1, bf->path));
						sync_nolock(bf);
						close(bf->fd);
						bf->fd = -1;
						block_file_count--;
						CAMEL_BLOCK_FILE_UNLOCK(bf, io_lock);
					}
					CAMEL_BLOCK_FILE_UNLOCK(bf, cache_lock);
				}
				CAMEL_BLOCK_FILE_UNLOCK(bf, root_lock);
			}
		}
		nw = nn;
		nn = nw->next;
	}

	UNLOCK(block_file_lock);

	return 0;
}

static void
block_file_unuse(CamelBlockFile *bs)
{
	LOCK(block_file_lock);
	e_dlist_remove((EDListNode *)bs->priv);
	e_dlist_addtail(&block_file_list, (EDListNode *)bs->priv);
	UNLOCK(block_file_lock);

	CAMEL_BLOCK_FILE_UNLOCK(bs, io_lock);
}

/*
o = camel_cache_get(c, key);
camel_cache_unref(c, key);
camel_cache_add(c, key, o);
camel_cache_remove(c, key);
*/

/**
 * camel_block_file_new:
 * @path: 
 * @: 
 * @block_size: 
 * 
 * Allocate a new block file, stored at @path.  @version contains an 8 character
 * version string which must match the head of the file, or the file will be
 * intitialised.
 * 
 * @block_size is currently ignored and is set to CAMEL_BLOCK_SIZE.
 *
 * Return value: The new block file, or NULL if it could not be created.
 **/
CamelBlockFile *camel_block_file_new(const char *path, int flags, const char version[8], size_t block_size)
{
	CamelBlockFile *bs;

	bs = (CamelBlockFile *)camel_object_new(camel_block_file_get_type());
	memcpy(bs->version, version, 8);
	bs->path = g_strdup(path);
	bs->flags = flags;

	bs->root_block = camel_block_file_get_block(bs, 0);
	if (bs->root_block == NULL) {
		camel_object_unref((CamelObject *)bs);
		return NULL;
	}
	camel_block_file_detach_block(bs, bs->root_block);
	bs->root = (CamelBlockRoot *)&bs->root_block->data;

	/* we only need these flags on first open */
	bs->flags &= ~(O_CREAT|O_EXCL|O_TRUNC);

	/* Do we need to init the root block? */
	if (CBF_CLASS(bs)->validate_root(bs) == -1) {
		d(printf("Initialise root block: %.8s\n", version));

		CBF_CLASS(bs)->init_root(bs);
		camel_block_file_touch_block(bs, bs->root_block);
		if (block_file_use(bs) == -1) {
			camel_object_unref((CamelObject *)bs);
			return NULL;
		}
		if (sync_block_nolock(bs, bs->root_block) == -1
		    || ftruncate(bs->fd, bs->root->last) == -1) {
			block_file_unuse(bs);
			camel_object_unref((CamelObject *)bs);
			return NULL;
		}
		block_file_unuse(bs);
	}

	return bs;
}

int
camel_block_file_rename(CamelBlockFile *bs, const char *path)
{
	int ret;
	struct stat st;
	int err;

	CAMEL_BLOCK_FILE_LOCK(bs, io_lock);

	ret = rename(bs->path, path);
	if (ret == -1) {
		/* Maybe the rename actually worked */
		err = errno;
		if (stat(path, &st) == 0
		    && stat(bs->path, &st) == -1
		    && errno == ENOENT)
			ret = 0;
		errno = err;
	}

	if (ret != -1) {
		g_free(bs->path);
		bs->path = g_strdup(path);
	}

	CAMEL_BLOCK_FILE_UNLOCK(bs, io_lock);

	return ret;
}

int
camel_block_file_delete(CamelBlockFile *bs)
{
	int ret;
	struct _CamelBlockFilePrivate *p = bs->priv;

	CAMEL_BLOCK_FILE_LOCK(bs, io_lock);

	if (bs->fd != -1) {
		LOCK(block_file_lock);
		block_file_count--;
		UNLOCK(block_file_lock);
		close(bs->fd);
		bs->fd = -1;
	}

	p->deleted = TRUE;
	ret = unlink(bs->path);

	CAMEL_BLOCK_FILE_UNLOCK(bs, io_lock);

	return ret;
	
}

/**
 * camel_block_file_new_block:
 * @bs: 
 * 
 * Allocate a new block, return a pointer to it.  Old blocks
 * may be flushed to disk during this call.
 * 
 * Return value: The block, or NULL if an error occured.
 **/
CamelBlock *camel_block_file_new_block(CamelBlockFile *bs)
{
	CamelBlock *bl;

	CAMEL_BLOCK_FILE_LOCK(bs, root_lock);

	if (bs->root->free) {
		bl = camel_block_file_get_block(bs, bs->root->free);
		if (bl == NULL)
			goto fail;
		bs->root->free = ((camel_block_t *)bl->data)[0];
	} else {
		bl = camel_block_file_get_block(bs, bs->root->last);
		if (bl == NULL)
			goto fail;
		bs->root->last += CAMEL_BLOCK_SIZE;
	}

	bs->root_block->flags |= CAMEL_BLOCK_DIRTY;

	bl->flags |= CAMEL_BLOCK_DIRTY;
	memset(bl->data, 0, CAMEL_BLOCK_SIZE);
fail:
	CAMEL_BLOCK_FILE_UNLOCK(bs, root_lock);

	return bl;
}

/**
 * camel_block_file_free_block:
 * @bs: 
 * @id: 
 * 
 * 
 **/
int camel_block_file_free_block(CamelBlockFile *bs, camel_block_t id)
{
	CamelBlock *bl;

	bl = camel_block_file_get_block(bs, id);
	if (bl == NULL)
		return -1;

	CAMEL_BLOCK_FILE_LOCK(bs, root_lock);

	((camel_block_t *)bl->data)[0] = bs->root->free;
	bs->root->free = bl->id;
	bs->root_block->flags |= CAMEL_BLOCK_DIRTY;
	bl->flags |= CAMEL_BLOCK_DIRTY;
	camel_block_file_unref_block(bs, bl);

	CAMEL_BLOCK_FILE_UNLOCK(bs, root_lock);

	return 0;
}

/**
 * camel_block_file_get_block:
 * @bs: 
 * @id: 
 * 
 * Retreive a block @id.
 * 
 * Return value: The block, or NULL if blockid is invalid or a file error
 * occured.
 **/
CamelBlock *camel_block_file_get_block(CamelBlockFile *bs, camel_block_t id)
{
	CamelBlock *bl, *flush, *prev;

	/* Sanity check: Dont allow reading of root block (except before its been read)
	   or blocks with invalid block id's */
	if ((bs->root == NULL && id != 0)
	    || (bs->root != NULL && (id > bs->root->last || id == 0))
	    || (id % bs->block_size) != 0) {
		errno = EINVAL;
		return NULL;
	}

	CAMEL_BLOCK_FILE_LOCK(bs, cache_lock);

	bl = g_hash_table_lookup(bs->blocks, GUINT_TO_POINTER(id));

	d(printf("Get  block %08x: %s\n", id, bl?"cached":"must read"));

	if (bl == NULL) {
		/* LOCK io_lock */
		if (block_file_use(bs) == -1) {
			CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);
			return NULL;
		}

		bl = g_malloc0(sizeof(*bl));
		bl->id = id;
		if (lseek(bs->fd, id, SEEK_SET) == -1 ||
		    camel_read (bs->fd, bl->data, CAMEL_BLOCK_SIZE) == -1) {
			block_file_unuse(bs);
			CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);
			g_free(bl);
			return NULL;
		}

		bs->block_cache_count++;
		g_hash_table_insert(bs->blocks, GUINT_TO_POINTER(bl->id), bl);

		/* flush old blocks */
		flush = (CamelBlock *)bs->block_cache.tailpred;
		prev = flush->prev;
		while (bs->block_cache_count > bs->block_cache_limit && prev) {
			if (flush->refcount == 0) {
				if (sync_block_nolock(bs, flush) != -1) {
					g_hash_table_remove(bs->blocks, GUINT_TO_POINTER(flush->id));
					e_dlist_remove((EDListNode *)flush);
					g_free(flush);
					bs->block_cache_count--;
				}
			}
			flush = prev;
			prev = prev->prev;
		}
		/* UNLOCK io_lock */
		block_file_unuse(bs);
	} else {
		e_dlist_remove((EDListNode *)bl);
	}

	e_dlist_addhead(&bs->block_cache, (EDListNode *)bl);
	bl->refcount++;

	CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);

	d(printf("Got  block %08x\n", id));

	return bl;
}

/**
 * camel_block_file_detach_block:
 * @bs: 
 * @bl: 
 * 
 * Detatch a block from the block file's cache.  The block should
 * be unref'd or attached when finished with.  The block file will
 * perform no writes of this block or flushing of it if the cache
 * fills.
 **/
void camel_block_file_detach_block(CamelBlockFile *bs, CamelBlock *bl)
{
	CAMEL_BLOCK_FILE_LOCK(bs, cache_lock);

	g_hash_table_remove(bs->blocks, GUINT_TO_POINTER(bl->id));
	e_dlist_remove((EDListNode *)bl);
	bl->flags |= CAMEL_BLOCK_DETACHED;

	CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);
}

/**
 * camel_block_file_attach_block:
 * @bs: 
 * @bl: 
 * 
 * Reattach a block that has been detached.
 **/
void camel_block_file_attach_block(CamelBlockFile *bs, CamelBlock *bl)
{
	CAMEL_BLOCK_FILE_LOCK(bs, cache_lock);

	g_hash_table_insert(bs->blocks, GUINT_TO_POINTER(bl->id), bl);
	e_dlist_addtail(&bs->block_cache, (EDListNode *)bl);
	bl->flags &= ~CAMEL_BLOCK_DETACHED;

	CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);
}

/**
 * camel_block_file_touch_block:
 * @bs: 
 * @bl: 
 * 
 * Mark a block as dirty.  The block will be written to disk if
 * it ever expires from the cache.
 **/
void camel_block_file_touch_block(CamelBlockFile *bs, CamelBlock *bl)
{
	CAMEL_BLOCK_FILE_LOCK(bs, root_lock);
	CAMEL_BLOCK_FILE_LOCK(bs, cache_lock);

	bl->flags |= CAMEL_BLOCK_DIRTY;

	if ((bs->root->flags & CAMEL_BLOCK_FILE_SYNC) && bl != bs->root_block) {
		d(printf("turning off sync flag\n"));
		bs->root->flags &= ~CAMEL_BLOCK_FILE_SYNC;
		bs->root_block->flags |= CAMEL_BLOCK_DIRTY;
		camel_block_file_sync_block(bs, bs->root_block);
	}

	CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);
	CAMEL_BLOCK_FILE_UNLOCK(bs, root_lock);
}

/**
 * camel_block_file_unref_block:
 * @bs: 
 * @bl: 
 * 
 * Mark a block as unused.  If a block is used it will not be
 * written to disk, or flushed from memory.
 *
 * If a block is detatched and this is the last reference, the
 * block will be freed.
 **/
void camel_block_file_unref_block(CamelBlockFile *bs, CamelBlock *bl)
{
	CAMEL_BLOCK_FILE_LOCK(bs, cache_lock);

	if (bl->refcount == 1 && (bl->flags & CAMEL_BLOCK_DETACHED))
		g_free(bl);
	else
		bl->refcount--;

	CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);
}

static int
sync_block_nolock(CamelBlockFile *bs, CamelBlock *bl)
{
	d(printf("Sync block %08x: %s\n", bl->id, (bl->flags & CAMEL_BLOCK_DIRTY)?"dirty":"clean"));

	if (bl->flags & CAMEL_BLOCK_DIRTY) {
		if (lseek(bs->fd, bl->id, SEEK_SET) == -1
		    || write(bs->fd, bl->data, CAMEL_BLOCK_SIZE) != CAMEL_BLOCK_SIZE) {
			return -1;
		}
		bl->flags &= ~CAMEL_BLOCK_DIRTY;
	}

	return 0;
}

static int
sync_nolock(CamelBlockFile *bs)
{
	CamelBlock *bl, *bn;
	int work = FALSE;

	bl = (CamelBlock *)bs->block_cache.head;
	bn = bl->next;
	while (bn) {
		if (bl->flags & CAMEL_BLOCK_DIRTY) {
			work = TRUE;
			if (sync_block_nolock(bs, bl) == -1)
				return -1;
		}
		bl = bn;
		bn = bn->next;
	}

	if (!work
	    && (bs->root_block->flags & CAMEL_BLOCK_DIRTY) == 0
	    && (bs->root->flags & CAMEL_BLOCK_FILE_SYNC) != 0)
		return 0;

	d(printf("turning on sync flag\n"));

	bs->root->flags |= CAMEL_BLOCK_FILE_SYNC;
	bs->root_block->flags |= CAMEL_BLOCK_DIRTY;

	return sync_block_nolock(bs, bs->root_block);
}

/**
 * camel_block_file_sync_block:
 * @bs: 
 * @bl: 
 * 
 * Flush a block to disk immediately.  The block will only
 * be flushed to disk if it is marked as dirty (touched).
 * 
 * Return value: -1 on io error.
 **/
int camel_block_file_sync_block(CamelBlockFile *bs, CamelBlock *bl)
{
	int ret;

	/* LOCK io_lock */
	if (block_file_use(bs) == -1)
		return -1;

	ret = sync_block_nolock(bs, bl);

	block_file_unuse(bs);

	return ret;
}

/**
 * camel_block_file_sync:
 * @bs: 
 * 
 * Sync all dirty blocks to disk, including the root block.
 * 
 * Return value: -1 on io error.
 **/
int camel_block_file_sync(CamelBlockFile *bs)
{
	int ret;

	CAMEL_BLOCK_FILE_LOCK(bs, root_lock);
	CAMEL_BLOCK_FILE_LOCK(bs, cache_lock);

	/* LOCK io_lock */
	if (block_file_use(bs) == -1)
		ret = -1;
	else {
		ret = sync_nolock(bs);
		block_file_unuse(bs);
	}

	CAMEL_BLOCK_FILE_UNLOCK(bs, cache_lock);
	CAMEL_BLOCK_FILE_UNLOCK(bs, root_lock);

	return ret;
}

/* ********************************************************************** */

struct _CamelKeyFilePrivate {
	struct _CamelKeyFilePrivate *next;
	struct _CamelKeyFilePrivate *prev;

	struct _CamelKeyFile *base;
	pthread_mutex_t lock;
	unsigned int deleted:1;
};

#define CAMEL_KEY_FILE_LOCK(kf, lock) (pthread_mutex_lock(&(kf)->priv->lock))
#define CAMEL_KEY_FILE_TRYLOCK(kf, lock) (pthread_mutex_trylock(&(kf)->priv->lock))
#define CAMEL_KEY_FILE_UNLOCK(kf, lock) (pthread_mutex_unlock(&(kf)->priv->lock))

static pthread_mutex_t key_file_lock = PTHREAD_MUTEX_INITIALIZER;

/* lru cache of block files */
static EDList key_file_list = E_DLIST_INITIALISER(key_file_list);
static EDList key_file_active_list = E_DLIST_INITIALISER(key_file_active_list);
static int key_file_count = 0;
static int key_file_threshhold = 10;

static void
camel_key_file_class_init(CamelKeyFileClass *klass)
{
}

static void
camel_key_file_init(CamelKeyFile *bs)
{
	struct _CamelKeyFilePrivate *p;

	p = bs->priv = g_malloc0(sizeof(*bs->priv));
	p->base = bs;
	
	pthread_mutex_init(&p->lock, NULL);
	
	LOCK(key_file_lock);
	e_dlist_addhead(&key_file_list, (EDListNode *)p);
	UNLOCK(key_file_lock);
}

static void
camel_key_file_finalise(CamelKeyFile *bs)
{
	struct _CamelKeyFilePrivate *p = bs->priv;

	LOCK(key_file_lock);
	e_dlist_remove((EDListNode *)p);

	if (bs-> fp) {
		key_file_count--;
		fclose(bs->fp);
	}

	UNLOCK(key_file_lock);

	g_free(bs->path);
	
	pthread_mutex_destroy(&p->lock);
	
	g_free(p);
}

CamelType
camel_key_file_get_type(void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(camel_object_get_type(), "CamelKeyFile",
					   sizeof (CamelKeyFile),
					   sizeof (CamelKeyFileClass),
					   (CamelObjectClassInitFunc) camel_key_file_class_init,
					   NULL,
					   (CamelObjectInitFunc) camel_key_file_init,
					   (CamelObjectFinalizeFunc) camel_key_file_finalise);
	}
	
	return type;
}

/* 'use' a key file for io */
static int
key_file_use(CamelKeyFile *bs)
{
	struct _CamelKeyFilePrivate *nw, *nn, *p = bs->priv;
	CamelKeyFile *bf;
	int err, fd;
	char *flag;

	/* We want to:
	    remove file from active list
	    lock it

	   Then when done:
	    unlock it
	    add it back to end of active list
	*/

	/* TODO: Check header on reset? */

	CAMEL_KEY_FILE_LOCK(bs, lock);

	if (bs->fp != NULL)
		return 0;
	else if (p->deleted) {
		CAMEL_KEY_FILE_UNLOCK(bs, lock);
		errno = ENOENT;
		return -1;
	} else
		d(printf("Turning key file online: '%s'\n", bs->path));

	if ((bs->flags & O_ACCMODE) == O_RDONLY)
		flag = "r";
	else
		flag = "a+";

	if ((fd = open(bs->path, bs->flags, 0600)) == -1
	    || (bs->fp = fdopen(fd, flag)) == NULL) {
		err = errno;
		close(fd);
		CAMEL_KEY_FILE_UNLOCK(bs, lock);
		errno = err;
		return -1;
	}

	LOCK(key_file_lock);
	e_dlist_remove((EDListNode *)p);
	e_dlist_addtail(&key_file_active_list, (EDListNode *)p);

	key_file_count++;

	nw = (struct _CamelKeyFilePrivate *)key_file_list.head;
	nn = nw->next;
	while (key_file_count > key_file_threshhold && nn) {
		/* We never hit the current keyfile here, as its removed from the list first */
		bf = nw->base;
		if (bf->fp != NULL) {
			/* Need to trylock, as any of these lock levels might be trying
			   to lock the key_file_lock, so we need to check and abort if so */
			if (CAMEL_BLOCK_FILE_TRYLOCK(bf, lock) == 0) {
				d(printf("Turning key file offline: %s\n", bf->path));
				fclose(bf->fp);
				bf->fp = NULL;
				key_file_count--;
				CAMEL_BLOCK_FILE_UNLOCK(bf, lock);
			}
		}
		nw = nn;
		nn = nw->next;
	}

	UNLOCK(key_file_lock);

	return 0;
}

static void
key_file_unuse(CamelKeyFile *bs)
{
	LOCK(key_file_lock);
	e_dlist_remove((EDListNode *)bs->priv);
	e_dlist_addtail(&key_file_list, (EDListNode *)bs->priv);
	UNLOCK(key_file_lock);

	CAMEL_KEY_FILE_UNLOCK(bs, lock);
}

/**
 * camel_key_file_new:
 * @path: 
 * @flags: open flags
 * @version[]: Version string (header) of file.  Currently
 * written but not checked.
 * 
 * Create a new key file.  A linked list of record blocks.
 * 
 * Return value: A new key file, or NULL if the file could not
 * be opened/created/initialised.
 **/
CamelKeyFile *
camel_key_file_new(const char *path, int flags, const char version[8])
{
	CamelKeyFile *kf;
	off_t last;
	int err;

	d(printf("New key file '%s'\n", path));

	kf = (CamelKeyFile *)camel_object_new(camel_key_file_get_type());
	kf->path = g_strdup(path);
	kf->fp = NULL;
	kf->flags = flags;
	kf->last = 8;

	if (key_file_use(kf) == -1) {
		camel_object_unref((CamelObject *)kf);
		kf = NULL;
	} else {
		fseek(kf->fp, 0, SEEK_END);
		last = ftell(kf->fp);
		if (last == 0) {
			fwrite(version, 8, 1, kf->fp);
			last += 8;
		}
		kf->last = last;

		err = ferror(kf->fp);
		key_file_unuse(kf);

		/* we only need these flags on first open */
		kf->flags &= ~(O_CREAT|O_EXCL|O_TRUNC);

		if (err) {
			camel_object_unref((CamelObject *)kf);
			kf = NULL;
		}
	}

	return kf;
}

int
camel_key_file_rename(CamelKeyFile *kf, const char *path)
{
	int ret;
	struct stat st;
	int err;

	CAMEL_KEY_FILE_LOCK(kf, lock);

	ret = rename(kf->path, path);
	if (ret == -1) {
		/* Maybe the rename actually worked */
		err = errno;
		if (stat(path, &st) == 0
		    && stat(kf->path, &st) == -1
		    && errno == ENOENT)
			ret = 0;
		errno = err;
	}

	if (ret != -1) {
		g_free(kf->path);
		kf->path = g_strdup(path);
	}

	CAMEL_KEY_FILE_UNLOCK(kf, lock);

	return ret;
}

int
camel_key_file_delete(CamelKeyFile *kf)
{
	int ret;
	struct _CamelKeyFilePrivate *p = kf->priv;

	CAMEL_KEY_FILE_LOCK(kf, lock);

	if (kf->fp) {
		LOCK(key_file_lock);
		key_file_count--;
		UNLOCK(key_file_lock);
		fclose(kf->fp);
		kf->fp = NULL;
	}

	p->deleted = TRUE;
	ret = unlink(kf->path);

	CAMEL_KEY_FILE_UNLOCK(kf, lock);

	return ret;
	
}

/**
 * camel_key_file_write:
 * @kf: 
 * @parent: 
 * @len: 
 * @records: 
 * 
 * Write a new list of records to the key file.
 * 
 * Return value: -1 on io error.  The key file will remain unchanged.
 **/
int
camel_key_file_write(CamelKeyFile *kf, camel_block_t *parent, size_t len, camel_key_t *records)
{
	camel_block_t next;
	guint32 size;
	int ret = -1;

	d(printf("write key %08x len = %d\n", *parent, len));

	if (len == 0) {
		d(printf(" new parent = %08x\n", *parent));
		return 0;
	}

	/* LOCK */
	if (key_file_use(kf) == -1)
		return -1;

	size = len;

	/* FIXME: Use io util functions? */
	next = kf->last;
	fseek(kf->fp, kf->last, SEEK_SET);
	fwrite(parent, sizeof(*parent), 1, kf->fp);
	fwrite(&size, sizeof(size), 1, kf->fp);
	fwrite(records, sizeof(records[0]), len, kf->fp);

	if (ferror(kf->fp)) {
		clearerr(kf->fp);
	} else {
		kf->last = ftell(kf->fp);
		*parent = next;
		ret = len;
	}

	/* UNLOCK */
	key_file_unuse(kf);

	d(printf(" new parent = %08x\n", *parent));

	return ret;
}

/**
 * camel_key_file_read:
 * @kf: 
 * @start: The record pointer.  This will be set to the next record pointer on success.
 * @len: Number of records read, if != NULL.
 * @records: Records, allocated, must be freed with g_free, if != NULL.
 * 
 * Read the next block of data from the key file.  Returns the number of
 * records.
 * 
 * Return value: -1 on io error.
 **/
int
camel_key_file_read(CamelKeyFile *kf, camel_block_t *start, size_t *len, camel_key_t **records)
{
	guint32 size;
	long pos = *start;
	camel_block_t next;
	int ret = -1;

	if (pos == 0)
		return 0;

	/* LOCK */
	if (key_file_use(kf) == -1)
		return -1;

	if (fseek(kf->fp, pos, SEEK_SET) == -1
	    || fread(&next, sizeof(next), 1, kf->fp) != 1
	    || fread(&size, sizeof(size), 1, kf->fp) != 1
	    || size > 1024) {
		clearerr(kf->fp);
		goto fail;
	}

	if (len)
		*len = size;

	if (records) {
		camel_key_t *keys = g_malloc(size * sizeof(camel_key_t));

		if (fread(keys, sizeof(camel_key_t), size, kf->fp) != size) {
			g_free(keys);
			goto fail;
		}
		*records = keys;
	}

	*start = next;

	ret = 0;
fail:
	/* UNLOCK */
	key_file_unuse(kf);

	return ret;
}
