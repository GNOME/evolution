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

/* a disk based array storage class */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib.h>

#include "block.h"
#include "index.h"

#define d(x)
/*#define DEBUG*/

static struct _IBEXStore *disk_create(struct _memcache *bc);
static int disk_sync(struct _IBEXStore *store);
static int disk_close(struct _IBEXStore *store);

static blockid_t disk_add(struct _IBEXStore *store, blockid_t head, nameid_t data);
static blockid_t disk_add_list(struct _IBEXStore *store, blockid_t head, GArray *data);
static blockid_t disk_remove(struct _IBEXStore *store, blockid_t head, nameid_t data);
static void disk_free(struct _IBEXStore *store, blockid_t head);

static gboolean disk_find(struct _IBEXStore *store, blockid_t head, nameid_t data);
static GArray *disk_get(struct _IBEXStore *store, blockid_t head);

struct _IBEXStoreClass ibex_diskarray_class = {
	disk_create, disk_sync, disk_close,
	disk_add, disk_add_list,
	disk_remove, disk_free,
	disk_find, disk_get
};

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
disk_add(struct _IBEXStore *store, blockid_t head, nameid_t data)
{
	struct _block *block;
	struct _block *newblock;
	blockid_t new;

	if (head == 0) {
		head = ibex_block_get(store->blocks);
	}
	block = ibex_block_read(store->blocks, head);

	d(printf("adding record %d to block %d (next = %d)\n", data, head, block->next));

	if (block->used < sizeof(block->bl_data)/sizeof(block->bl_data[0])) {
		d(printf("adding record into block %d  %d\n", head, data));
		block->bl_data[block->used] = data;
		block->used++;
		ibex_block_dirty(block);
		return head;
	} else {
		new = ibex_block_get(store->blocks);
		newblock = ibex_block_read(store->blocks, new);
		newblock->next = head;
		newblock->bl_data[0] = data;
		newblock->used = 1;
		d(printf("adding record into new %d  %d, next =%d\n", new, data, newblock->next));
		ibex_block_dirty(newblock);
		return new;
	}
}

static blockid_t
disk_add_list(struct _IBEXStore *store, blockid_t head, GArray *data)
{
	struct _block *block;
	struct _block *newblock;
	blockid_t new;
	int copied = 0;
	int left, space, tocopy;

	if (head == 0) {
		head = ibex_block_get(store->blocks);
	}
	block = ibex_block_read(store->blocks, head);

	while (copied < data->len) {
		left = data->len - copied;
		space = sizeof(block->bl_data)/sizeof(block->bl_data[0]) - block->used;
		if (space) {
			tocopy = MIN(left, space);
			memcpy(block->bl_data+block->used, &g_array_index(data, blockid_t, copied), tocopy*sizeof(blockid_t));
			block->used += tocopy;
			ibex_block_dirty(block);
		} else {
			new = ibex_block_get(store->blocks);
			newblock = ibex_block_read(store->blocks, new);
			newblock->next = head;
			tocopy = MIN(left, sizeof(block->bl_data)/sizeof(block->bl_data[0]));
			memcpy(newblock->bl_data, &g_array_index(data, blockid_t, copied), tocopy*sizeof(blockid_t));
			newblock->used = tocopy;
			block = newblock;
			head = new;
			ibex_block_dirty(newblock);
		}
		copied += tocopy;
	}
	return head;
}

static blockid_t
disk_remove(struct _IBEXStore *store, blockid_t head, nameid_t data)
{
	blockid_t node = head;

	d(printf("removing %d from %d\n", data, head));
	while (node) {
		struct _block *block = ibex_block_read(store->blocks, node);
		int i;

		for (i=0;i<block->used;i++) {
			if (block->bl_data[i] == data) {
				struct _block *start = ibex_block_read(store->blocks, head);

				start->used--;
				block->bl_data[i] = start->bl_data[start->used];
				if (start->used == 0) {
					struct _root *root = (struct _root *)ibex_block_read(store->blocks, 0);
					blockid_t new;

					d(printf("dropping block %d, new head = %d\n", head, start->next));
					new = start->next;
					start->next = root->free;
					root->free = head;
					head = new;
					ibex_block_dirty((struct _block *)root);
				}
				ibex_block_dirty(block);
				ibex_block_dirty(start);
				return head;
			}
		}
		node = block->next;
	}
	return head;
}

static void disk_free(struct _IBEXStore *store, blockid_t head)
{
	blockid_t next;
	struct _block *block;

	while (head) {
		block = ibex_block_read(store->blocks, head);
		next = block->next;
		ibex_block_free(store->blocks, head);
		head = next;
	}
}

static gboolean
disk_find(struct _IBEXStore *store, blockid_t head, nameid_t data)
{
	blockid_t node = head;

	d(printf("finding %d from %d\n", data, head));
	while (node) {
		struct _block *block = ibex_block_read(store->blocks, node);
		int i;

		for (i=0;i<block->used;i++) {
			if (block->bl_data[i] == data) {
				return TRUE;
			}
		}
		node = block->next;
	}
	return FALSE;
}

static GArray *
disk_get(struct _IBEXStore *store, blockid_t head)
{
	GArray *result = g_array_new(0, 0, sizeof(nameid_t));

	while (head) {
		struct _block *block = ibex_block_read(store->blocks, head);

		d(printf("getting data from block %d\n", head));

		g_array_append_vals(result, block->bl_data, block->used);
		head = block->next;
		d(printf("next = %d\n", head));
	}
	return result;
}

void
ibex_diskarray_dump(struct _memcache *blocks, blockid_t head)
{
	blockid_t node = head;

	printf("dumping list %d\n", node);
	while (node) {
		struct _block *block = ibex_block_read(blocks, node);
		int i;

		printf(" block %d used %d\n ", node, block->used);
		for (i=0;i<block->used;i++) {
			printf(" %d", block->bl_data[i]);
		}
		printf("\n");
		node = block->next;
	}
}
