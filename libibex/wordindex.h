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

#ifndef _WORDINDEX_H
#define _WORDINDEX_H

#include <glib.h>

#include "block.h"
#include "index.h"

struct _IBEXWord;

/* not used yet */
typedef void (*IBEXNormaliseFunc)(char *source, int len, char *dest);

struct _IBEXWordClass {
	int (*sync)(struct _IBEXWord *);
	int (*flush)(struct _IBEXWord *);
	int (*close)(struct _IBEXWord *);

	void (*index_pre)(struct _IBEXWord *); /* get ready for doing a lot of indexing.  may be a nop */
	void (*index_post)(struct _IBEXWord *);

	void (*unindex_name)(struct _IBEXWord *, const char *name);		/* unindex all entries for name */
	gboolean (*contains_name)(struct _IBEXWord *, const char *name);	/* index contains data for name */
	GPtrArray *(*find)(struct _IBEXWord *, const char *word);		/* returns all matches for word */
	gboolean (*find_name)(struct _IBEXWord *, const char *name, const char *word);	/* find if name contains word */
	void (*add)(struct _IBEXWord *, const char *name, const char *word);	/* adds a single word to name */
	void (*add_list)(struct _IBEXWord *, const char *name, GPtrArray *words);/* adds a bunch of words to a given name */
};

struct _IBEXWord {
	struct _IBEXWordClass *klass;
	struct _IBEXStore *wordstore;
	struct _IBEXIndex *wordindex;
	struct _IBEXStore *namestore;
	struct _IBEXIndex *nameindex;

	/* word caching info (should probably be modularised) */
	GHashTable *wordcache;	/* word->struct _wordcache mapping */
	struct _list wordnodes;	/* LRU list of wordcache structures */
	int wordcount;		/* how much space used in cache */
	int precount;
};


struct _IBEXWord *ibex_create_word_index(struct _memcache *bc, blockid_t *wordroot, blockid_t *nameroot);

/* alternate implemenation */
struct _IBEXWord *ibex_create_word_index_mem(struct _memcache *bc, blockid_t *wordroot, blockid_t *nameroot);

#endif /* !_WORDINDEX_H */
