/*
	Copyright 2000 Helix Code Inc.
*/

/* find.c: index file searching ops */

#include <string.h>

#include "ibex_internal.h"

GPtrArray *
ibex_find(ibex *ib, char *word)
{
  GPtrArray *refs, *ret;
  ibex_file *ibf;
  int i;

  ret = g_ptr_array_new();
  refs = g_hash_table_lookup(ib->words, word);
  if (refs)
    {
      for (i = 0; i < refs->len; i++)
	{
	  ibf = g_ptr_array_index(refs, i);
	  g_ptr_array_add(ret, ibf->name);
	}
    }
  return ret;
}

gboolean
ibex_find_name(ibex *ib, char *name, char *word)
{
  GPtrArray *refs;
  ibex_file *ibf;
  int i;

  refs = g_hash_table_lookup(ib->words, word);
  if (refs)
    {
      for (i = 0; i < refs->len; i++)
	{
	  ibf = g_ptr_array_index(refs, i);
	  if (!strcmp(ibf->name, name))
		  return TRUE;
	}
    }
  return FALSE;
}

static gint
build_array(gpointer key, gpointer value, gpointer data)
{
  char *name = key;
  unsigned int count = GPOINTER_TO_UINT(value);
  GPtrArray *ret = data;

  if (count == 1)
    g_ptr_array_add(ret, name);
  return FALSE;
}

GPtrArray *
ibex_find_all(ibex *ib, GPtrArray *words)
{
  GTree *work;
  GPtrArray *wrefs, *ret;
  int i, j, count;
  char *word;
  ibex_file *ibf;

  if (words->len == 0)
    return g_ptr_array_new();
  else if (words->len == 1)
    return ibex_find(ib, g_ptr_array_index(words, 0));

  work = g_tree_new(strcmp);
  for (i = 0; i < words->len; i++)
    {
      word = g_ptr_array_index(words, i);
      wrefs = g_hash_table_lookup(ib->words, word);
      if (!wrefs)
	{
	  /* One of the words isn't even in the index. */
	  g_tree_destroy(work);
	  return g_ptr_array_new();
	}

      if (i == 0)
	{
	  /* Copy the references into a tree, using the filenames as
	   * keys and the size of words as the value.
	   */
	  for (j = 0; j < wrefs->len; j++)
	    {
	      ibf = g_ptr_array_index(wrefs, j);
	      g_tree_insert(work, ibf->name, GUINT_TO_POINTER(words->len));
	    }
	}
      else
	{
	  /* Increment the counts in the working tree for the references
	   * for this word.
	   */
	  for (j = 0; j < wrefs->len; j++)
	    {
	      ibf = g_ptr_array_index(wrefs, j);
	      count = GPOINTER_TO_UINT(g_tree_lookup(work, ibf->name));
	      if (count)
		g_tree_insert(work, ibf->name, GUINT_TO_POINTER(count - 1));
	    }
	}
    }

  /* Build an array with the refs that contain all the words. */
  ret = g_ptr_array_new();
  g_tree_traverse(work, build_array, G_IN_ORDER, ret);
  g_tree_destroy(work);
  return ret;
}
