/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-extras.c - Set of hash table sort of thingies.
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "gal/util/e-util.h"

#include "e-cell-checkbox.h"
#include "e-cell-date.h"
#include "e-cell-number.h"
#include "e-cell-pixbuf.h"
#include "e-cell-size.h"
#include "e-cell-text.h"
#include "e-cell-tree.h"
#include "e-table-extras.h"

static GObjectClass *ete_parent_class;

static void
cell_hash_free(gchar	*key,
	       ECell    *cell,
	       gpointer	user_data)
{
	g_free(key);
	if (cell)
		g_object_unref(cell);
}

static void
pixbuf_hash_free(gchar	*key,
		 GdkPixbuf *pixbuf,
		 gpointer	user_data)
{
	g_free(key);
	if (pixbuf)
		gdk_pixbuf_unref(pixbuf);
}

static void
ete_finalize (GObject *object)
{
	ETableExtras *ete = E_TABLE_EXTRAS (object);

	if (ete->cells) {
		g_hash_table_foreach (ete->cells, (GHFunc) cell_hash_free, NULL);
		g_hash_table_destroy (ete->cells);
	}

	if (ete->compares) {
		g_hash_table_foreach (ete->compares, (GHFunc) g_free, NULL);
		g_hash_table_destroy (ete->compares);
	}

	if (ete->searches) {
		g_hash_table_foreach (ete->searches, (GHFunc) g_free, NULL);
		g_hash_table_destroy (ete->searches);
	}

	if (ete->pixbufs) {
		g_hash_table_foreach (ete->pixbufs, (GHFunc) pixbuf_hash_free, NULL);
		g_hash_table_destroy (ete->pixbufs);
	}

	ete->cells = NULL;
	ete->compares = NULL;
	ete->searches = NULL;
	ete->pixbufs = NULL;

	ete_parent_class->finalize (object);
}

static void
ete_class_init (GObjectClass *klass)
{
	ete_parent_class = g_type_class_peek_parent (klass);
	
	klass->finalize = ete_finalize;
}

static gint
e_strint_compare(gconstpointer data1, gconstpointer data2)
{
	int int1 = atoi(data1);
	int int2 = atoi(data2);

	return g_int_compare(GINT_TO_POINTER(int1), GINT_TO_POINTER(int2));
}

/* UTF-8 strncasecmp - not optimized */

static gint
g_utf8_strncasecmp (const gchar *s1,
		    const gchar *s2,
		    guint n)
{
	gunichar c1, c2;

	g_return_val_if_fail (s1 != NULL && g_utf8_validate (s1, -1, NULL), 0);
	g_return_val_if_fail (s2 != NULL && g_utf8_validate (s2, -1, NULL), 0);

	while (n && *s1 && *s2)
		{

			n -= 1;

			c1 = g_unichar_tolower (g_utf8_get_char (s1));
			c2 = g_unichar_tolower (g_utf8_get_char (s2));

			/* Collation is locale-dependent, so this totally fails to do the right thing. */
			if (c1 != c2)
				return c1 < c2 ? -1 : 1;

			s1 = g_utf8_next_char (s1);
			s2 = g_utf8_next_char (s2);
		}

	if (n == 0 || (*s1 == '\0' && *s2 == '\0'))
		return 0;

	return *s1 ? 1 : -1;
}

static gboolean
e_string_search(gconstpointer haystack, const char *needle)
{
	int length;
	if (haystack == NULL)
		return FALSE;

	length = g_utf8_strlen (needle, -1);
	if (g_utf8_strncasecmp (haystack, needle, length) == 0)
		return TRUE;
	else
		return FALSE;
}

static void
ete_init (ETableExtras *extras)
{
	extras->cells = g_hash_table_new(g_str_hash, g_str_equal);
	extras->compares = g_hash_table_new(g_str_hash, g_str_equal);
	extras->searches = g_hash_table_new(g_str_hash, g_str_equal);
	extras->pixbufs = g_hash_table_new(g_str_hash, g_str_equal);

	e_table_extras_add_compare(extras, "string", g_str_compare);
	e_table_extras_add_compare(extras, "collate", g_collate_compare);
	e_table_extras_add_compare(extras, "integer", g_int_compare);
	e_table_extras_add_compare(extras, "string-integer", e_strint_compare);

	e_table_extras_add_search(extras, "string", e_string_search);

	e_table_extras_add_cell(extras, "checkbox", e_cell_checkbox_new());
	e_table_extras_add_cell(extras, "date", e_cell_date_new (NULL, GTK_JUSTIFY_LEFT));
	e_table_extras_add_cell(extras, "number", e_cell_number_new (NULL, GTK_JUSTIFY_RIGHT));
	e_table_extras_add_cell(extras, "pixbuf", e_cell_pixbuf_new ());
	e_table_extras_add_cell(extras, "size", e_cell_size_new (NULL, GTK_JUSTIFY_RIGHT));
	e_table_extras_add_cell(extras, "string", e_cell_text_new (NULL, GTK_JUSTIFY_LEFT));
	e_table_extras_add_cell(extras, "tree-string", e_cell_tree_new (NULL, NULL, TRUE, e_cell_text_new (NULL, GTK_JUSTIFY_LEFT)));
}

E_MAKE_TYPE(e_table_extras, "ETableExtras", ETableExtras, ete_class_init, ete_init, G_TYPE_OBJECT)

ETableExtras *
e_table_extras_new (void)
{
	ETableExtras *ete = g_object_new (E_TABLE_EXTRAS_TYPE, NULL);

	return (ETableExtras *) ete;
}

void
e_table_extras_add_cell     (ETableExtras *extras,
			     char         *id,
			     ECell        *cell)
{
	gchar *old_key;
	ECell *old_cell;

	if (g_hash_table_lookup_extended (extras->cells, id, (gpointer *)&old_key, (gpointer *)&old_cell)) {
		g_hash_table_remove (extras->cells, old_key);
		g_free (old_key);
		if (old_cell)
			g_object_unref (old_cell);
	}

	if (cell) {
		g_object_ref (cell);
		gtk_object_sink (GTK_OBJECT (cell));
	}
	g_hash_table_insert (extras->cells, g_strdup(id), cell);
}

ECell *
e_table_extras_get_cell     (ETableExtras *extras,
			     char         *id)
{
	return g_hash_table_lookup(extras->cells, id);
}

void
e_table_extras_add_compare  (ETableExtras *extras,
			     char         *id,
			     GCompareFunc  compare)
{
	gchar *old_key;
	GCompareFunc old_compare;

	if (g_hash_table_lookup_extended (extras->compares, id, (gpointer *)&old_key, (gpointer *)&old_compare)) {
		g_hash_table_remove (extras->compares, old_key);
		g_free (old_key);
	}

	g_hash_table_insert(extras->compares, g_strdup(id), (gpointer) compare);
}

GCompareFunc
e_table_extras_get_compare  (ETableExtras *extras,
			     char         *id)
{
	return (GCompareFunc) g_hash_table_lookup(extras->compares, id);
}

void
e_table_extras_add_search  (ETableExtras     *extras,
			    char             *id,
			    ETableSearchFunc  search)
{
	gchar *old_key;
	ETableSearchFunc old_search;

	if (g_hash_table_lookup_extended (extras->searches, id, (gpointer *)&old_key, (gpointer *)&old_search)) {
		g_hash_table_remove (extras->searches, old_key);
		g_free (old_key);
	}

	g_hash_table_insert(extras->searches, g_strdup(id), search);
}

ETableSearchFunc
e_table_extras_get_search  (ETableExtras *extras,
			    char         *id)
{
	return g_hash_table_lookup(extras->searches, id);
}

void
e_table_extras_add_pixbuf     (ETableExtras *extras,
			       char         *id,
			       GdkPixbuf    *pixbuf)
{
	gchar *old_key;
	GdkPixbuf *old_pixbuf;

	if (g_hash_table_lookup_extended (extras->pixbufs, id, (gpointer *)&old_key, (gpointer *)&old_pixbuf)) {
		g_hash_table_remove (extras->cells, old_key);
		g_free (old_key);
		if (old_pixbuf)
			gdk_pixbuf_unref (old_pixbuf);
	}

	if (pixbuf)
		gdk_pixbuf_ref(pixbuf);
	g_hash_table_insert (extras->pixbufs, g_strdup(id), pixbuf);
}

GdkPixbuf *
e_table_extras_get_pixbuf     (ETableExtras *extras,
			       char         *id)
{
	return g_hash_table_lookup(extras->pixbufs, id);
}
