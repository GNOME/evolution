/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2000 Helix Code, Inc.
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

/* find.c: index file searching ops */

#include <string.h>

#include "ibex_internal.h"

/**
 * ibex_find: search an ibex for a word
 * @ib: an ibex
 * @word: the word
 *
 * This routine searches an ibex for a word and returns a GPtrArray
 * containing the names of the files in the ibex that contain the word.
 * If no matches are found, it will return an empty array (not NULL).
 * The caller must free the array, but MUST NOT free or alter its
 * elements.
 *
 * Return value: the array of filenames containing @word
 **/
GPtrArray *
ibex_find (ibex *ib, char *word)
{
	GPtrArray *refs, *ret;
	ibex_file *ibf;
	int i;

	ret = g_ptr_array_new ();
	refs = g_hash_table_lookup (ib->words, word);
	if (refs) {
		for (i = 0; i < refs->len; i++)	{
			ibf = g_ptr_array_index (refs, i);
			g_ptr_array_add (ret, ibf->name);
		}
	}
	return ret;
}

/**
 * ibex_find_name: Check if a word occurs in a given file
 * @ib: an ibex
 * @name: a filename
 * @word: a word
 *
 * This checks if the given word occurs in the given file.
 *
 * Return value: TRUE or FALSE
 **/
gboolean
ibex_find_name (ibex *ib, char *name, char *word)
{
	GPtrArray *refs;
	ibex_file *ibf;
	int i;

	refs = g_hash_table_lookup (ib->words, word);
	if (refs) {
		for (i = 0; i < refs->len; i++)	{
			ibf = g_ptr_array_index (refs, i);
			if (!strcmp (ibf->name, name))
				return TRUE;
		}
	}
	return FALSE;
}

static gint
build_array (gpointer key, gpointer value, gpointer data)
{
	char *name = key;
	unsigned int count = GPOINTER_TO_UINT (value);
	GPtrArray *ret = data;

	if (count == 1)
		g_ptr_array_add (ret, name);
	return FALSE;
}

/**
 * ibex_find_all: Find files containing multiple words
 * @ib: an ibex
 * @words: a GPtrArray of words
 *
 * This works like ibex_find(), but returns an array of filenames
 * which contain all of the words in @words.
 *
 * Return value: an array of matches
 **/
GPtrArray *
ibex_find_all (ibex *ib, GPtrArray *words)
{
	GTree *work;
	GPtrArray *wrefs, *ret;
	int i, j, count;
	char *word;
	ibex_file *ibf;

	if (words->len == 0)
		return g_ptr_array_new ();
	else if (words->len == 1)
		return ibex_find (ib, g_ptr_array_index (words, 0));

	work = g_tree_new (strcmp);
	for (i = 0; i < words->len; i++) {
		word = g_ptr_array_index (words, i);
		wrefs = g_hash_table_lookup (ib->words, word);
		if (!wrefs) {
			/* One of the words isn't even in the index. */
			g_tree_destroy (work);
			return g_ptr_array_new ();
		}

		if (i == 0) {
			/* Copy the references into a tree, using the
			 * filenames as keys and the size of words as
			 * the value.
			 */
			for (j = 0; j < wrefs->len; j++) {
				ibf = g_ptr_array_index (wrefs, j);
				g_tree_insert (work, ibf->name,
					       GUINT_TO_POINTER (words->len));
			}
		} else {
			/* Increment the counts in the working tree
			 * for the references for this word.
			 */
			for (j = 0; j < wrefs->len; j++) {
				ibf = g_ptr_array_index (wrefs, j);
				count = GPOINTER_TO_UINT (g_tree_lookup (work, ibf->name));
				if (count) {
					g_tree_insert (work, ibf->name,
						       GUINT_TO_POINTER (count - 1));
				}
			}
		}
	}

	/* Build an array with the refs that contain all the words. */
	ret = g_ptr_array_new ();
	g_tree_traverse (work, build_array, G_IN_ORDER, ret);
	g_tree_destroy (work);
	return ret;
}

static void
ibex_dump_foo(char *key, GPtrArray *refs, void *data)
{
	int i;

	printf("%s: ", key);
	for (i=0;i<refs->len;i++) {
		ibex_file *ibf = g_ptr_array_index (refs, i);
		printf("%c%s", ibf->index==-1?'-':' ', ibf->name);
	}
	printf("\n");
}

/* debug function to dump the tree, in key order */
void
ibex_dump_all (ibex *ib)
{
	g_hash_table_foreach(ib->words, ibex_dump_foo, 0);
}
