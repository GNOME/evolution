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

/*
  block file/cache/utility functions
*/

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <glib.h>

#include "block.h"

/*#define MALLOC_CHECK*/

#ifdef MALLOC_CHECK
#include <mcheck.h>
#endif

#define d(x)
/*#define DEBUG*/

int block_log;

#ifdef IBEX_STATS
static void
init_stats(struct _memcache *index)
{
	index->stats = g_hash_table_new(g_direct_hash, g_direct_equal);
}

static void
dump_1_stat(int id, struct _stat_info *info, struct _memcache *index)
{
	printf("%d %d %d %d %d\n", id, info->read, info->write, info->cache_hit, info->cache_miss);
}

static void
dump_stats(struct _memcache *index)
{
	printf("Block reads writes hits misses\n");
	g_hash_table_foreach(index->stats, dump_1_stat, index);
}

static void
add_read(struct _memcache *index, int id)
{
	struct _stat_info *info;

	info = g_hash_table_lookup(index->stats, (void *)id);
	if (info == NULL) {
		info = g_malloc0(sizeof(*info));
		g_hash_table_insert(index->stats, (void *)id, info);
	}
	info->read++;
}

static void
add_write(struct _memcache *index, int id)
{
	struct _stat_info *info;

	info = g_hash_table_lookup(index->stats, (void *)id);
	if (info == NULL) {
		info = g_malloc0(sizeof(*info));
		g_hash_table_insert(index->stats, (void *)id, info);
	}
	info->write++;
}

static void
add_hit(struct _memcache *index, int id)
{
	struct _stat_info *info;

	info = g_hash_table_lookup(index->stats, (void *)id);
	if (info == NULL) {
		info = g_malloc0(sizeof(*info));
		g_hash_table_insert(index->stats, (void *)id, info);
	}
	info->cache_hit++;
}

static void
add_miss(struct _memcache *index, int id)
{
	struct _stat_info *info;

	info = g_hash_table_lookup(index->stats, (void *)id);
	if (info == NULL) {
		info = g_malloc0(sizeof(*info));
		g_hash_table_insert(index->stats, (void *)id, info);
	}
	info->cache_miss++;
}
#endif /* IBEX_STATS */

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

/* simple list routines (for simplified memory management of cache/lists) */

/**
 * ibex_list_new:
 * @v: 
 * 
 * Initialise a list header.  A list header must always be initialised
 * before use.
 **/
void ibex_list_new(struct _list *v)
{
	v->head = (struct _listnode *)&v->tail;
	v->tail = 0;
	v->tailpred = (struct _listnode *)&v->head;
}

/**
 * ibex_list_addhead:
 * @l: List.
 * @n: Node to append.
 * 
 * Prepend a listnode to the head of the list @l.
 * 
 * Return value: Always @n.
 **/
struct _listnode *ibex_list_addhead(struct _list *l, struct _listnode *n)
{
	n->next = l->head;
	n->prev = (struct _listnode *)&l->head;
	l->head->prev = n;
	l->head = n;
	return n;
}

/**
 * ibex_list_addtail:
 * @l: 
 * @n: 
 * 
 * Append a listnode to the end of the list @l.
 * 
 * Return value: Always the same as @n.
 **/
struct _listnode *ibex_list_addtail(struct _list *l, struct _listnode *n)
{
	n->next = (struct _listnode *)&l->tail;
	n->prev = l->tailpred;
	l->tailpred->next = n;
	l->tailpred = n;
	return n;
}

/**
 * ibex_list_remove:
 * @n: The node to remove.
 * 
 * Remove a listnode from a list.
 * 
 * Return value: Always the same as @n.
 **/
struct _listnode *ibex_list_remove(struct _listnode *n)
{
	n->next->prev = n->prev;
	n->prev->next = n->next;
	return n;
}

static struct _memblock *
memblock_addr(struct _block *block)
{
	return (struct _memblock *)(((char *)block) - G_STRUCT_OFFSET(struct _memblock, data));
}

/* read/sync the rootblock into the block_cache structure */
static int
ibex_block_read_root(struct _memcache *block_cache)
{
	lseek(block_cache->fd, 0, SEEK_SET);
	if (read(block_cache->fd, &block_cache->root, sizeof(block_cache->root)) != sizeof(block_cache->root)) {
		
		return -1;
	}
	return 0;
}

static int
ibex_block_sync_root(struct _memcache *block_cache)
{
	lseek(block_cache->fd, 0, SEEK_SET);
	if (write(block_cache->fd, &block_cache->root, sizeof(block_cache->root)) != sizeof(block_cache->root)) {
		return -1;
	}
	return fsync(block_cache->fd);
}


/**
 * ibex_block_dirty:
 * @block: 
 * 
 * Dirty a block.  This will cause it to be written to disk on
 * a cache sync, or when the block is flushed from the cache.
 **/
void
ibex_block_dirty(struct _block *block)
{
	memblock_addr(block)->flags |= BLOCK_DIRTY;
}

static void
sync_block(struct _memcache *block_cache, struct _memblock *memblock)
{
	if (block_log)
		printf("writing block %d\n", memblock->block);

	lseek(block_cache->fd, memblock->block, SEEK_SET);
	if (write(block_cache->fd, &memblock->data, sizeof(memblock->data)) != -1) {
		memblock->flags &= ~BLOCK_DIRTY;
	}
#ifdef IBEX_STATS
	add_write(block_cache, memblock->block);
#endif
}

/**
 * ibex_block_cache_sync:
 * @block_cache: 
 * 
 * Ensure the block cache is fully synced to disk.
 **/
void
ibex_block_cache_sync(struct _memcache *block_cache)
{
	struct _memblock *memblock;

	memblock = (struct _memblock *)block_cache->nodes.head;
	while (memblock->next) {
#ifdef MALLOC_CHECK
		checkmem(memblock);
#endif
		if (memblock->flags & BLOCK_DIRTY) {
			sync_block(block_cache, memblock);
		}
		memblock = memblock->next;
	}

	block_cache->root.flags |= IBEX_ROOT_SYNCF;
	if (ibex_block_sync_root(block_cache) != 0) {
		block_cache->root.flags &= ~IBEX_ROOT_SYNCF;
	}

#ifdef IBEX_STATS
	dump_stats(block_cache);
#endif

}

#ifdef MALLOC_CHECK
static void
check_cache(struct _memcache *block_cache)
{
	struct _memblock *mw, *mn;

	checkmem(block_cache);
	checkmem(block_cache->index);

	mw = (struct _memblock *)block_cache->nodes.head;
	mn = mw->next;
	while (mn) {
		checkmem(mw);
		mw = mn;
		mn = mn->next;
	}
}
#endif

/**
 * ibex_block_cache_flush:
 * @block_cache: 
 * 
 * Ensure the block cache is fully synced to disk, and then flush
 * its contents from memory.
 **/
void
ibex_block_cache_flush(struct _memcache *block_cache)
{
	struct _memblock *mw, *mn;

	ibex_block_cache_sync(block_cache);

	mw = (struct _memblock *)block_cache->nodes.head;
	mn = mw->next;
	while (mn) {
		g_hash_table_remove(block_cache->index, (void *)mw->block);
		g_free(mw);
		mw = mn;
		mn = mn->next;
	}
	ibex_list_new(&block_cache->nodes);
}

/**
 * ibex_block_read:
 * @block_cache: 
 * @blockid: 
 * 
 * Read the data of a block by blockid.  The data contents is backed by
 * the block cache, and should be considered static.
 *
 * TODO; should this return a NULL block on error?
 *
 * Return value: The address of the block data (which may be cached).
 **/
struct _block *
ibex_block_read(struct _memcache *block_cache, blockid_t blockid)
{
	struct _memblock *memblock;

#ifdef MALLOC_CHECK
	check_cache(block_cache);
#endif

	/* nothing can read the root block directly */
	g_assert(blockid != 0);
	g_assert(blockid < block_cache->root.roof);

	memblock = g_hash_table_lookup(block_cache->index, (void *)blockid);

#ifdef MALLOC_CHECK
	check_cache(block_cache);
#endif

	if (memblock) {
		d(printf("foudn blockid in cache %d = %p\n", blockid, &memblock->data));

		/* 'access' page */
		ibex_list_remove((struct _listnode *)memblock);
		ibex_list_addtail(&block_cache->nodes, (struct _listnode *)memblock);

#ifdef MALLOC_CHECK
		check_cache(block_cache);
#endif

#ifdef IBEX_STATS
		add_hit(block_cache, memblock->block);
#endif

#ifdef MALLOC_CHECK
		check_cache(block_cache);
#endif

		return &memblock->data;
	}
#ifdef IBEX_STATS
	add_miss(block_cache, blockid);
	add_read(block_cache, blockid);
#endif
	if (block_log)
		printf("miss block %d\n", blockid);

	d(printf("loading blockid from disk %d\n", blockid));
	memblock = g_malloc(sizeof(*memblock));
	memblock->block = blockid;
	memblock->flags = 0;
	lseek(block_cache->fd, blockid, SEEK_SET);
	memset(&memblock->data, 0, sizeof(memblock->data));
	read(block_cache->fd, &memblock->data, sizeof(memblock->data));

	ibex_list_addtail(&block_cache->nodes, (struct _listnode *)memblock);
	g_hash_table_insert(block_cache->index, (void *)blockid, memblock);
	if (block_cache->count >= CACHE_SIZE) {
		struct _memblock *old = (struct _memblock *)block_cache->nodes.head;
		d(printf("discaring cache block %d\n", old->block));
		g_hash_table_remove(block_cache->index, (void *)old->block);
		ibex_list_remove((struct _listnode *)old);
		if (old->flags & BLOCK_DIRTY) {
			/* are we about to un-sync the file?  update root and sync it */
			if (block_cache->root.flags & IBEX_ROOT_SYNCF) {
				printf("Unsyncing root block\n");

				block_cache->root.flags &= ~IBEX_ROOT_SYNCF;
				if (ibex_block_sync_root(block_cache) != 0) {
					/* what do we do?  i dont know! */
					g_warning("Could not sync root block of index: %s", strerror(errno));
				}
			}
			sync_block(block_cache, old);
		}
		g_free(old);
	} else {
		block_cache->count++;
	}

	d(printf("  --- cached blocks : %d\n", block_cache->count));

#ifdef MALLOC_CHECK
	check_cache(block_cache);
#endif
	return &memblock->data;
}

/**
 * ibex_block_cache_open:
 * @name: 
 * @flags: Flags as to open(2), should use O_RDWR and optionally O_CREAT.
 * @mode: Mose as to open(2)
 * 
 * Open a block file.
 * 
 * FIXME; this currently also initialises the word and name indexes
 * because their pointers are stored in the root block.  Should be
 * upto the caller to manage these pointers/data.
 *
 * Return value: NULL if the backing file could not be opened.
 **/
struct _memcache *
ibex_block_cache_open(const char *name, int flags, int mode)
{
	struct _memcache *block_cache = g_malloc0(sizeof(*block_cache));

	d(printf("opening ibex file: %s", name));

	/* setup cache */
	ibex_list_new(&block_cache->nodes);
	block_cache->count = 0;
	block_cache->index = g_hash_table_new(g_direct_hash, g_direct_equal);
	block_cache->fd = open(name, flags, mode);

	if (block_cache->fd == -1) {
		g_hash_table_destroy(block_cache->index);
		g_free(block_cache);
		return NULL;
	}

	ibex_block_read_root(block_cache);
	if (block_cache->root.roof == 0
	    || memcmp(block_cache->root.version, IBEX_VERSION, 4)
	    || ((block_cache->root.flags & IBEX_ROOT_SYNCF) == 0)) {
		(printf("Initialising superblock\n"));
		/* reset root data */
		memcpy(block_cache->root.version, IBEX_VERSION, 4);
		block_cache->root.roof = 1024;
		block_cache->root.free = 0;
		block_cache->root.words = 0;
		block_cache->root.names = 0;
		block_cache->root.tail = 0;	/* list of tail blocks */
		block_cache->root.flags = 0;
		ibex_block_sync_root(block_cache);
		/* reset the file contents */
		ftruncate(block_cache->fd, 1024);
	} else {
		(printf("superblock already initialised:\n"
			" roof = %d\n free = %d\n words = %d\n names = %d\n tail = %d\n",
			block_cache->root.roof, block_cache->root.free,
			block_cache->root.words, block_cache->root.names, block_cache->root.tail));
	}
	/* FIXME: this should be moved higher up in the object tree */
	{
		struct _IBEXWord *ibex_create_word_index_mem(struct _memcache *bc, blockid_t *wordroot, blockid_t *nameroot);

		block_cache->words = ibex_create_word_index_mem(block_cache, &block_cache->root.words,&block_cache->root.names);
	}

#ifdef IBEX_STATS
	init_stats(block_cache);
#endif

	return block_cache;
}

/**
 * ibex_block_cache_close:
 * @block_cache: 
 * 
 * Close the block file, sync any remaining cached data
 * to disk, and free all resources.
 **/
void
ibex_block_cache_close(struct _memcache *block_cache)
{
	struct _memblock *mw, *mn;

	ibex_block_cache_sync(block_cache);
	close(block_cache->fd);

	mw = (struct _memblock *)block_cache->nodes.head;
	mn = mw->next;
	while (mn) {
		g_free(mw);
		mw = mn;
		mn = mw->next;
	}

	g_hash_table_destroy(block_cache->index);

	g_free(block_cache);
}

/**
 * ibex_block_free:
 * @block_cache: 
 * @blockid: 
 * 
 * Return a block to the free pool.
 **/
void
ibex_block_free(struct _memcache *block_cache, blockid_t blockid)
{
	struct _block *block = ibex_block_read(block_cache, blockid);

	block->next = block_number(block_cache->root.free);
	block_cache->root.free = blockid;
	ibex_block_dirty((struct _block *)block);
}

/**
 * ibex_block_get:
 * @block_cache: 
 * 
 * Allocate a new block, or access a previously freed block and return
 * its block id.  The block will have zeroed contents.
 * 
 * Return value: 0 if there are no blocks left (disk full/read only
 * file, etc).
 **/
blockid_t
ibex_block_get(struct _memcache *block_cache)
{
	struct _block *block;
	blockid_t head;

	if (block_cache->root.free) {
		head = block_cache->root.free;
		block = ibex_block_read(block_cache, head);
		block_cache->root.free = block_location(block->next);
	} else {
		/* TODO: check the block will fit first */
		/* TODO: no need to read this block, can allocate it manually (saves a syscall/read) */
		head = block_cache->root.roof;
		block_cache->root.roof += BLOCK_SIZE;
		block = ibex_block_read(block_cache, head);
	}

	g_assert(head != 0);

	d(printf("new block = %d\n", head));
	block->next = 0;
	block->used = 0;
	ibex_block_dirty(block);
	return head;
}
