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

#ifndef _INDEX_H
#define _INDEX_H

/* an indexing 'class' maps a key to 1 piece of info */

#define INDEX_STAT

struct _IBEXCursor {
	struct _IBEXCursorClass *klass;
	struct _IBEXIndex *index;
};

struct _IBEXCursorClass {
	void (*close)(struct _IBEXCursor *);

	guint32 (*next)(struct _IBEXCursor *);
	char *(*next_key)(struct _IBEXCursor *, int *keylenptr);
};

struct _IBEXIndex {
	struct _IBEXIndexClass *klass;
	struct _memcache *blocks;
	blockid_t root;		/* root block of ondisk index data */
#ifdef INDEX_STAT
	int lookups;		/* how many lookups */
	int lookup_total;	/* how many blocks loaded for all lookups (hash chain depth) */
#endif
};

struct _IBEXIndexClass {

	struct _IBEXIndex *(*create)(struct _memcache *bc, int size);
	struct _IBEXIndex *(*open)(struct _memcache *bc, blockid_t root);

	int (*sync)(struct _IBEXIndex *);
	int (*close)(struct _IBEXIndex *);

	/* lookup a key in the index, returns the keyid of this item, or 0 if not found */
	guint32 (*find)(struct _IBEXIndex *, const char *key, int keylen);

	/* remove a key from the index */
	void (*remove)(struct _IBEXIndex *, const char *key, int keylen);

	/* insert a new key into the index, the keyid is returned */
	guint32 (*insert)(struct _IBEXIndex *, const char *key, int keylen);

	/* get the key contents/key length from the keyid */
	char *(*get_key)(struct _IBEXIndex *, guint32 keyid, int *keylenptr);

	/* set the key contents based on the keyid */
	void (*set_data)(struct _IBEXIndex *, guint32 keyid, blockid_t datablock, blockid_t tail);

	/* get the key contents based on the keyid */
	blockid_t (*get_data)(struct _IBEXIndex *, guint32 keyid, blockid_t *tail);

	/* get a cursor for iterating over all contents */
	struct _IBEXCursor *(*get_cursor)(struct _IBEXIndex *);
};

/* a storage class, stores lists of lists of id's */

struct _IBEXStore {
	struct _IBEXStoreClass *klass;
	struct _memcache *blocks;
};

struct _IBEXStoreClass {
	struct _IBEXStore *(*create)(struct _memcache *bc);
	int (*sync)(struct _IBEXStore *store);
	int (*close)(struct _IBEXStore *store);

	blockid_t (*add)(struct _IBEXStore *store, blockid_t *head, blockid_t *tail, nameid_t data);
	blockid_t (*add_list)(struct _IBEXStore *store, blockid_t *head, blockid_t *tail, GArray *data);
	blockid_t (*remove)(struct _IBEXStore *store, blockid_t *head, blockid_t *tail, nameid_t data);
	void (*free)(struct _IBEXStore *store, blockid_t head, blockid_t tail);

	gboolean (*find)(struct _IBEXStore *store, blockid_t head, blockid_t tail, nameid_t data);
	GArray *(*get)(struct _IBEXStore *store, blockid_t head, blockid_t tail);
};

#endif
