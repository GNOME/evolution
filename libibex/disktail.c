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

/* a disk based array storage class that stores the tails of data lists
   in common blocks */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib.h>
#include <string.h>

#include <stdio.h>

#include "block.h"
#include "index.h"

#define d(x)
/*#define DEBUG*/

/* marker to define which root keys indicate a single length key */
#define BLOCK_ONE (1<<BLOCK_BITS)

/* tail blocks only contain tail data ... */
/* and we pack it in, similar to the key data, only more compact */
struct _tailblock {
	unsigned int next:32-BLOCK_BITS; /* only needs to point to block numbers */
	unsigned int used:BLOCK_BITS; /* how many entries are used */
	union {
		unsigned char offset[BLOCK_SIZE-4]; /* works upto blocksize of 1024 bytes */
		nameid_t data[(BLOCK_SIZE-4)/4];
	} tailblock_u;
};
#define tb_offset tailblock_u.offset
#define tb_data tailblock_u.data

/* map a tail index to a block index */
#define TAIL_INDEX(b) ((b) & (BLOCK_SIZE-1))
/* map a tail index to a block number */
#define TAIL_BLOCK(b) ((b) & ~(BLOCK_SIZE-1))
/* map a block + index to a tailid */
#define TAIL_KEY(b, i) (((b) & ~(BLOCK_SIZE-1)) | ((i) & (BLOCK_SIZE-1)))

#define TAIL_THRESHOLD ((BLOCK_SIZE-8)/6)

static struct _IBEXStore *disk_create(struct _memcache *bc);
static int disk_sync(struct _IBEXStore *store);
static int disk_close(struct _IBEXStore *store);

static blockid_t disk_add(struct _IBEXStore *store, blockid_t *head, blockid_t *tail, nameid_t data);
static blockid_t disk_add_list(struct _IBEXStore *store, blockid_t *head, blockid_t *tail, GArray *data);
static blockid_t disk_remove(struct _IBEXStore *store, blockid_t *head, blockid_t *tail, nameid_t data);
static void disk_free(struct _IBEXStore *store, blockid_t head, blockid_t tail);

static gboolean disk_find(struct _IBEXStore *store, blockid_t head, blockid_t tail, nameid_t data);
static GArray *disk_get(struct _IBEXStore *store, blockid_t head, blockid_t tail);

struct _IBEXStoreClass ibex_diskarray_class = {
	disk_create, disk_sync, disk_close,
	disk_add, disk_add_list,
	disk_remove, disk_free,
	disk_find, disk_get
};


static int
tail_info(struct _tailblock *bucket, nameid_t tailid, blockid_t **startptr)
{
	blockid_t *start, *end;
	int index;

	/* get start/end of area to zap */
	index = TAIL_INDEX(tailid);
	start = &bucket->tb_data[bucket->tb_offset[index]];
	if (index == 0) {
		end = &bucket->tb_data[sizeof(bucket->tb_data)/sizeof(bucket->tb_data[0])];
	} else {
		end = &bucket->tb_data[bucket->tb_offset[index-1]];
	}
	if (startptr)
		*startptr = start;
	return end-start;
}

/* compresses (or expand) the bucket entry, to the new size */
static void
tail_compress(struct _tailblock *bucket, int index, int newsize)
{
	int i;
	blockid_t *start, *end, *newstart;

	/* get start/end of area to zap */
	start = &bucket->tb_data[bucket->tb_offset[index]];
	if (index == 0) {
		end = &bucket->tb_data[sizeof(bucket->tb_data)/sizeof(bucket->tb_data[0])];
	} else {
		end = &bucket->tb_data[bucket->tb_offset[index-1]];
	}

	if (end-start == newsize)
		return;

	/*
	     XXXXXXXXXXXXXXXXXXXXXXXXXXXXXyyyyy
	     0                           20   25

	     newsize = 0
	     end = 25
	     newstart = 0
	     start = 20

	     newstart+(end-start)-newsize = 5
	     i = start-newstart

	     XXXXXXXXXXXXXXXXXXXXXXXXXXXXXyyyyy
	     0                           20   25

	     newsize = 2
	     end = 25
	     newstart = 0
	     start = 20

	     newstart+(end-start)-newsize = 3
	     i = start-newstart + MIN(end-start, newsize)) = 22

	     XXXXXXXXXXXXXXXXXXXXXXXXXXXXX
	     5 	                         25
	     newsize = 5
	     end = 25
	     start = 25
	     newstart = 5

	     newstart+(end-start)-newsize = 0
	     i = start-newstart = 20

	     XXXXXXXXXXXXXXXXXXXXXXXXXXXXXyy
	     3 	                         23 25
	     newsize = 5
	     end = 25
	     start = 23
	     newstart = 3

	     newstart+(end-start)-newsize = 0
	     i = start-newstart + MIN(end-start, newsize) = 22

	*/


	/* fixup data */
	newstart = &bucket->tb_data[bucket->tb_offset[bucket->used-1]];

	g_assert(newstart+(end-start)-newsize <= &bucket->tb_data[sizeof(bucket->tb_data)/sizeof(bucket->tb_data[0])]);
	g_assert(newstart + (start-newstart) + MIN(end-start, newsize) <= &bucket->tb_data[sizeof(bucket->tb_data)/sizeof(bucket->tb_data[0])]);
	g_assert(newstart+(end-start)-newsize >= &bucket->tb_offset[bucket->used]);
	g_assert(newstart + (start-newstart) + MIN(end-start, newsize) >= &bucket->tb_offset[bucket->used]);

	memmove(newstart+(end-start)-newsize, newstart, ((start-newstart)+MIN(end-start, newsize)) * sizeof(blockid_t));

	/* fixup key pointers */
	for (i=index;i<bucket->used;i++) {
		bucket->tb_offset[i] += (end-start)-newsize;
	}
	ibex_block_dirty((struct _block *)bucket);
}

/*
  returns the number of blockid's free
*/
static int
tail_space(struct _tailblock *tail)
{
	if (tail->used == 0)
		return sizeof(tail->tb_data)/sizeof(tail->tb_data[0])-1;

	return  &tail->tb_data[tail->tb_offset[tail->used-1]]
		- (blockid_t *)&tail->tb_offset[tail->used];
}

static void
tail_dump(struct _memcache *blocks, blockid_t tailid)
{
	int i;
	struct _tailblock *tail = (struct _tailblock *)ibex_block_read(blocks, TAIL_BLOCK(tailid));

	printf("Block %d, used %d\n", tailid, tail->used);
	for (i=0;i<sizeof(struct _tailblock)/sizeof(unsigned int);i++) {
		printf(" %08x", ((unsigned int *)tail)[i]);
	}
	printf("\n");
}

static blockid_t
tail_get(struct _memcache *blocks, int size)
{
	blockid_t tailid;
	struct _tailblock *tail;
	int freeindex;
	blockid_t *end;
	int i, count = 0;

	d(printf("looking for a tail node with %d items in it\n", size));

	/* look for a node with enough space, if we dont find it fairly
	   quickly, just quit.  needs a better free algorithm i think ... */
	tailid = blocks->root.tail;
	while (tailid && count<5) {
		int space;

		d(printf(" checking tail node %d\n", tailid));

		tail = (struct _tailblock *)ibex_block_read(blocks, tailid);

		if (tail->used == 0) {
			/* assume its big enough ... */
			tail->used = 1;
			tail->tb_offset[0] = sizeof(tail->tb_data)/sizeof(tail->tb_data[0]) - size;
			d(printf("allocated %d (%d), used %d\n", tailid, tailid, tail->used));
			ibex_block_dirty((struct _block *)tail);

			g_assert(&tail->tb_offset[tail->used-1]
				 < &tail->tb_data[tail->tb_offset[tail->used-1]]);

			return tailid;
		}

		g_assert(&tail->tb_offset[tail->used-1]
			 < &tail->tb_data[tail->tb_offset[tail->used-1]]);

		/* see if we have a free slot first */
		freeindex = -1;
		end = &tail->tb_data[sizeof(tail->tb_data)/sizeof(tail->tb_data[0])];
		for (i=0;i<tail->used;i++) {
			if (end == &tail->tb_data[tail->tb_offset[i]]) {
				freeindex = i;
				break;
			}
			end = &tail->tb_data[tail->tb_offset[i]];
		}

		/* determine how much space we have available - incl any extra header we might need */
		space =  ((char *)&tail->tb_data[tail->tb_offset[tail->used-1]])
			- ((char *)&tail->tb_offset[tail->used])
			/* FIXMEL work out why this is out a little bit */
			- 8;
		if (freeindex == -1)
			space -= sizeof(tail->tb_offset[0]);

		/* if we have enough, set it up, creating a new entry if necessary */
		/* for some really odd reason the compiler promotes this expression to unsigned, hence
		   the requirement for the space>0 check ... */
		if (space>0 && space > size*sizeof(blockid_t)) {
			d(printf("space = %d, size = %d size*sizeof() = %d truth = %d\n", space, size, size*sizeof(blockid_t), space>size*sizeof(blockid_t)));
			if (freeindex == -1) {
				freeindex = tail->used;
				tail->tb_offset[tail->used] = tail->tb_offset[tail->used-1];
				tail->used++;
			}
			tail_compress(tail, freeindex, size);
			ibex_block_dirty((struct _block *)tail);
			d(printf("allocated %d (%d), used %d\n", tailid, TAIL_KEY(tailid, freeindex), tail->used));
			return TAIL_KEY(tailid, freeindex);
		}
		count++;
		tailid = block_location(tail->next);
	}

	d(printf("allocating new data node for tail data\n"));
	tailid = ibex_block_get(blocks);
	tail = (struct _tailblock *)ibex_block_read(blocks, tailid);
	tail->next = block_number(blocks->root.tail);
	blocks->root.tail = tailid;
	tail->used = 1;
	tail->tb_offset[0] = sizeof(tail->tb_data)/sizeof(tail->tb_data[0]) - size;
	ibex_block_dirty((struct _block *)tail);
	d(printf("allocated %d (%d), used %d\n", tailid, TAIL_KEY(tailid, 0), tail->used));

	g_assert(&tail->tb_offset[tail->used-1]
		 < &tail->tb_data[tail->tb_offset[tail->used-1]]);

	return TAIL_KEY(tailid, 0);
}

static void
tail_free(struct _memcache *blocks, blockid_t tailid)
{
	struct _tailblock *tail;

	d(printf("freeing tail id %d\n", tailid));

	if (tailid == 0)
		return;

	tail = (struct _tailblock *)ibex_block_read(blocks, TAIL_BLOCK(tailid));
	d(printf("  tail %d used %d\n", tailid, tail->used));
	g_assert(tail->used);
	if (TAIL_INDEX(tailid)  == tail->used - 1) {
		tail->used --;
	} else {
		tail_compress(tail, TAIL_INDEX(tailid), 0);
	}
	ibex_block_dirty((struct _block *)tail);
}

static struct _IBEXStore *disk_create(struct _memcache *bc)
{
	struct _IBEXStore *store;

	store = g_malloc0(sizeof(*store));
	store->klass = &ibex_diskarray_class;
	store->blocks = bc;

	return store;
}

static int disk_sync(struct _IBEXStore *store)
{
	/* no cache, nop */
	return 0;
}

static int disk_close(struct _IBEXStore *store)
{
	g_free(store);
	return 0;
}

static blockid_t
disk_add(struct _IBEXStore *store, blockid_t *headptr, blockid_t *tailptr, nameid_t data)
{
	struct _block *block;
	struct _block *newblock;
	blockid_t new, head = *headptr /*, tail = *tailptr*/;

	g_error("Inbimplemented");

	if (head == 0) {
		head = ibex_block_get(store->blocks);
	}
	block = ibex_block_read(store->blocks, head);

	d(printf("adding record %d to block %d (next = %d)\n", data, head, block->next));

	if (block->used < sizeof(block->bl_data)/sizeof(block->bl_data[0])) {
		(printf("adding record into block %d  %d\n", head, data));
		block->bl_data[block->used] = data;
		block->used++;
		ibex_block_dirty(block);
		return head;
	} else {
		new = ibex_block_get(store->blocks);
		newblock = ibex_block_read(store->blocks, new);
		newblock->next = block_number(head);
		newblock->bl_data[0] = data;
		newblock->used = 1;
		d(printf("adding record into new %d  %d, next =%d\n", new, data, newblock->next));
		ibex_block_dirty(newblock);
		return new;
	}
}

static blockid_t
disk_add_blocks_internal(struct _IBEXStore *store, blockid_t *headptr, blockid_t *tailptr, GArray *data)
{
	blockid_t head = *headptr, tail = *tailptr, new;
	int tocopy;
	struct _tailblock *tailblock;
	struct _block *block, *newblock;
	int space, copied = 0, left;

	/* assumes this funciton is in control of any tail creation */ 
	g_assert(tail == 0);

	d(printf("Adding %d items to block list\n", data->len));

	if (head == 0) {
		head = ibex_block_get(store->blocks);
		d(printf("allocating new head %d\n", head));
	}
	block = ibex_block_read(store->blocks, head);

	/* ensure the first block is full before proceeding */
	space = sizeof(block->bl_data)/sizeof(block->bl_data[0]) - block->used;
	if (space) {
		tocopy = MIN(data->len, space);
		memcpy(block->bl_data+block->used, &g_array_index(data, blockid_t, copied), tocopy*sizeof(blockid_t));
		block->used += tocopy;
		ibex_block_dirty(block);
		copied = tocopy;
		d(printf("copied %d items to left over last node\n", tocopy));
	}

	while (copied < data->len) {
		left = data->len - copied;
		/* do we drop the rest in a tail? */
		if (left < TAIL_THRESHOLD) {
			d(printf("Storing remaining %d items in tail\n", left));
			tocopy = left;
			new = tail_get(store->blocks, tocopy);
			tailblock = (struct _tailblock *)ibex_block_read(store->blocks, TAIL_BLOCK(new));
			memcpy(&tailblock->tb_data[tailblock->tb_offset[TAIL_INDEX(new)]],
			       &g_array_index(data, blockid_t, copied), tocopy*sizeof(blockid_t));
			*tailptr = new;
		} else {
			new = ibex_block_get(store->blocks);
			newblock = (struct _block *)ibex_block_read(store->blocks, new);
			newblock->next = block_number(head);
			tocopy = MIN(left, sizeof(block->bl_data)/sizeof(block->bl_data[0]));
			d(printf("storing %d items in own block\n", tocopy));
			memcpy(newblock->bl_data, &g_array_index(data, blockid_t, copied), tocopy*sizeof(blockid_t));
			newblock->used = tocopy;
			block = newblock;
			head = new;
			ibex_block_dirty(newblock);
		}
		copied += tocopy;
	}

	*headptr = head;
	return head;
}
/*
  case 1:
    no head, no tail, adding a lot of data
      add blocks until the last, which goes in a tail node
  case 2:
    no head, no tail, adding a little data
      just add a tail node
  case 3:
    no head, tail, total exceeds a block
      merge as much as possible into new full blocks, then the remainder in the tail
  case 4:
    no head, tail, room in existing tail for data
      add new data to tail node
  case 5:
    no head, tail, no room in existing tail for data
      add a new tail node, copy data across, free old tail node
*/

static blockid_t
disk_add_list(struct _IBEXStore *store, blockid_t *headptr, blockid_t *tailptr, GArray *data)
{
	blockid_t new, head = *headptr, tail = *tailptr, *start;
	struct _tailblock *tailblock, *tailnew;
	int len;
	GArray *tmpdata = NULL;

	d(printf("adding %d items head=%d tail=%d\n", data->len, head, tail));
	if (data->len == 0)
		return head;

	/* store length=1 data in the tail pointer */
	if (head == 0 && tail == 0 && data->len == 1) {
		*headptr = BLOCK_ONE;
		*tailptr = g_array_index(data, blockid_t, 0);
		return BLOCK_ONE;
	}
	/* if we got length=1 data to append to, upgrade it to a real block list */
	if (head == BLOCK_ONE) {
		tmpdata = g_array_new(0, 0, sizeof(blockid_t));
		g_array_append_vals(tmpdata, data->data, data->len);
		g_array_append_val(tmpdata, tail);
		head = *headptr = 0;
		tail = *tailptr = 0;
	}

	/* if we have no head, then check the tail */
	if (head == 0) {
		if (tail == 0) {
			if (data->len >= TAIL_THRESHOLD) {
				/* add normally */
				head = disk_add_blocks_internal(store, headptr, tailptr, data);
			} else {
				/* else add to a tail block */
				new = tail_get(store->blocks, data->len);
				d(printf("adding %d items to a tail block %d\n", data->len, new));
				tailnew = (struct _tailblock *)ibex_block_read(store->blocks, TAIL_BLOCK(new));
				memcpy(&tailnew->tb_data[tailnew->tb_offset[TAIL_INDEX(new)]],
				       data->data, data->len*sizeof(blockid_t));
				*tailptr = new;
				ibex_block_dirty((struct _block *)tailnew);
			}
		} else {
			tailblock = (struct _tailblock *)ibex_block_read(store->blocks, TAIL_BLOCK(tail));
			len = tail_info(tailblock, tail, &start);
			/* case 3 */
			if (len + data->len >= TAIL_THRESHOLD) {
				/* this is suboptimal, but should work - merge the tail data with
				   our new data, and write it out */
				if (tmpdata == NULL) {
					tmpdata = g_array_new(0, 0, sizeof(blockid_t));
					g_array_append_vals(tmpdata, data->data, data->len);
				}
				g_array_append_vals(tmpdata, start, len);
				*tailptr = 0;
				tail_free(store->blocks, tail);
				head = disk_add_blocks_internal(store, headptr, tailptr, tmpdata);
			} else if (tail_space(tailblock) >= data->len) {
				/* can we just expand this in our node, or do we need a new one? */
				tail_compress(tailblock, TAIL_INDEX(tail), data->len + len);
				memcpy(&tailblock->tb_data[tailblock->tb_offset[TAIL_INDEX(tail)] + len],
				       data->data, data->len * sizeof(blockid_t));
				ibex_block_dirty((struct _block *)tailblock);
			} else {
				/* we need to allocate a new tail node */
				new = tail_get(store->blocks, data->len + len);
				/* and copy the data across */
				tailnew = (struct _tailblock *)ibex_block_read(store->blocks, TAIL_BLOCK(new));
				memcpy(&tailnew->tb_data[tailnew->tb_offset[TAIL_INDEX(new)]],
				       start, len*sizeof(blockid_t));
				memcpy(&tailnew->tb_data[tailnew->tb_offset[TAIL_INDEX(new)]+len],
				       data->data, data->len*sizeof(blockid_t));
				tail_free(store->blocks, tail);
				ibex_block_dirty((struct _block *)tailnew);
				*tailptr = new;
			}
		}
	} else {
		if (tail) {
			/* read/merge the tail with the new data, rewrite out.
			   suboptimal, but it should be 'ok' ? */
			tailblock = (struct _tailblock *)ibex_block_read(store->blocks, TAIL_BLOCK(tail));
			len = tail_info(tailblock, tail, &start);
			tmpdata = g_array_new(0, 0, sizeof(blockid_t));
			g_array_append_vals(tmpdata, start, len);
			g_array_append_vals(tmpdata, data->data, data->len);
			*tailptr = 0;
			tail_free(store->blocks, tail);
			head = disk_add_blocks_internal(store, headptr, tailptr, tmpdata);
		} else {
			head = disk_add_blocks_internal(store, headptr, tailptr, data);
		}
	}
	if (tmpdata)
		g_array_free(tmpdata, TRUE);

	return head;
}

static blockid_t
disk_remove(struct _IBEXStore *store, blockid_t *headptr, blockid_t *tailptr, nameid_t data)
{
	blockid_t head = *headptr, node = *headptr, tail = *tailptr;
	int i;

	/* special case for 1-item nodes */
	if (head == BLOCK_ONE) {
		if (tail == data) {
			*tailptr = 0;
			*headptr = 0;
			head = 0;
		}
		return head;
	}

	d(printf("removing %d from %d\n", data, head));
	while (node) {
		struct _block *block = ibex_block_read(store->blocks, node);

		for (i=0;i<block->used;i++) {
			if (block->bl_data[i] == data) {
				struct _block *start = ibex_block_read(store->blocks, head);

				d(printf("removing data from block %d\n ", head));

				start->used--;
				block->bl_data[i] = start->bl_data[start->used];
				if (start->used == 0) {
					blockid_t new;

					d(printf("dropping block %d, new head = %d\n", head, start->next));
					new = block_location(start->next);
					start->next = block_number(store->blocks->root.free);
					store->blocks->root.free = head;
					head = new;
				}
				ibex_block_dirty(block);
				ibex_block_dirty(start);
				*headptr = head;
				return head;
			}
		}
		node = block_location(block->next);
	}

	if (tail) {
		struct _tailblock *tailblock = (struct _tailblock *)ibex_block_read(store->blocks, TAIL_BLOCK(tail));
		int len;
		blockid_t *start;

		len = tail_info(tailblock, tail, &start);
		for (i=0;i<len;i++) {
			if (start[i] == data) {
				for (;i<len-1;i++)
					start[i] = start[i+1];
				if (len == 1)
					*tailptr = 0;
				if (len == 1 && (tailblock->used-1) == TAIL_INDEX(tail)) {
					d(printf("dropping/unlinking tailblock %d (%d) used = %d\n",
						 TAIL_BLOCK(tail), tail, tailblock->used));
					tailblock->used--;
					/* drop/unlink block? */
					ibex_block_dirty((struct _block *)tailblock);
					*tailptr = 0;
				} else {
					tail_compress(tailblock, TAIL_INDEX(tail), len-1);
					ibex_block_dirty((struct _block *)tailblock);
				}
			}
		}
		
	}

	return head;
}

static void disk_free(struct _IBEXStore *store, blockid_t head, blockid_t tail)
{
	blockid_t next;
	struct _block *block;

	if (head == BLOCK_ONE)
		return;

	while (head) {
		d(printf("freeing block %d\n", head));
		block = ibex_block_read(store->blocks, head);
		next = block_location(block->next);
		ibex_block_free(store->blocks, head);
		head = next;
	}
	if (tail) {
		struct _tailblock *tailblock = (struct _tailblock *)ibex_block_read(store->blocks, TAIL_BLOCK(tail));
		d(printf("freeing tail block %d (%d)\n", TAIL_BLOCK(tail), tail));
		tail_compress(tailblock, TAIL_INDEX(tail), 0);
		ibex_block_dirty((struct _block *)tailblock);
	}
}

static gboolean
disk_find(struct _IBEXStore *store, blockid_t head, blockid_t tail, nameid_t data)
{
	blockid_t node = head;
	int i;

	d(printf("finding %d from %d\n", data, head));

	if (head == BLOCK_ONE)
		return data == tail;

	/* first check full blocks */
	while (node) {
		struct _block *block = ibex_block_read(store->blocks, node);

		for (i=0;i<block->used;i++) {
			if (block->bl_data[i] == data) {
				return TRUE;
			}
		}
		node = block_location(block->next);
	}

	/* then check tail */
	if (tail) {
		struct _tailblock *tailblock = (struct _tailblock *)ibex_block_read(store->blocks, TAIL_BLOCK(tail));
		int len;
		blockid_t *start;

		len = tail_info(tailblock, tail, &start);
		for (i=0;i<len;i++) {
			if (start[i] == data)
				return TRUE;
		}
	}

	return FALSE;
}

static GArray *
disk_get(struct _IBEXStore *store, blockid_t head, blockid_t tail)
{
	GArray *result = g_array_new(0, 0, sizeof(nameid_t));

	if (head == BLOCK_ONE) {
		g_array_append_val(result, tail);
		return result;
	}

	while (head) {
		struct _block *block = ibex_block_read(store->blocks, head);

		d(printf("getting data from block %d\n", head));

		g_array_append_vals(result, block->bl_data, block->used);
		head = block_location(block->next);
		d(printf("next = %d\n", head));
	}

	/* chuck on any tail data as well */
	if (tail) {
		struct _tailblock *tailblock;
		int len;
		blockid_t *start;
		
		tailblock = (struct _tailblock *)ibex_block_read(store->blocks, TAIL_BLOCK(tail));
		len = tail_info(tailblock, tail, &start);
		g_array_append_vals(result, start, len);
	}
	return result;
}

void
ibex_diskarray_dump(struct _memcache *blocks, blockid_t head, blockid_t tail)
{
	blockid_t node = head;

	printf("dumping list %d tail %d\n", node, tail);
	if (head == BLOCK_ONE) {
		printf(" 1 length index: %d\n", tail);
		return;
	}
       
	while (node) {
		struct _block *block = ibex_block_read(blocks, node);
		int i;

		printf(" block %d used %d\n ", node, block->used);
		for (i=0;i<block->used;i++) {
			printf(" %d", block->bl_data[i]);
		}
		printf("\n");
		node = block_location(block->next);
	}

	printf("tail: ");
	if (tail) {
		struct _tailblock *tailblock;
		int len;
		blockid_t *start;
		int i;

		tailblock = (struct _tailblock *)ibex_block_read(blocks, TAIL_BLOCK(tail));
		len = tail_info(tailblock, tail, &start);
		for (i=0;i<len;i++)
			printf(" %d", start[i]);
	}
	printf("\n");
}

#if 0
int main(int argc, char **argv)
{
	struct _memcache *bc;
	struct _IBEXStore *disk;
	int i;
	blockid_t data[100];
	GArray adata;
	blockid_t head=0, tail=0;

	for (i=0;i<100;i++) {
		data[i] = i;
	}

	bc = ibex_block_cache_open("index.db", O_CREAT|O_RDWR, 0600);
	disk = ibex_diskarray_class.create(bc);

	head = 0;
	tail = 0;
	adata.data = data;
	adata.len = 70;
	for (i=0;i<100;i++) {
		printf("Loop %d\n", i);
		ibex_diskarray_class.add_list(disk, &head, &tail, &adata);
		ibex_diskarray_dump(bc, head, tail);
	}
	
#if 0
	for (i=1;i<100;i++) {
		printf("inserting %d items\n", i);
		adata.data = data;
		adata.len = i;
		head=0;
		tail=0;
		ibex_diskarray_class.add_list(disk, &head, &tail, &adata);
		ibex_diskarray_dump(bc, head, tail);
	}
#endif
	ibex_diskarray_class.close(disk);
	ibex_block_cache_close(bc);
	return 0;
}

#endif
