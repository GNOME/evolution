/*
 * e-table-extras.c - Set of hash table sort of thingies.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "e-util/e-util.h"

#include "e-cell-checkbox.h"
#include "e-cell-date.h"
#include "e-cell-number.h"
#include "e-cell-pixbuf.h"
#include "e-cell-size.h"
#include "e-cell-text.h"
#include "e-cell-tree.h"
#include "e-table-extras.h"

/* workaround for avoiding API breakage */
#define ete_get_type e_table_extras_get_type
G_DEFINE_TYPE (ETableExtras, ete, G_TYPE_OBJECT)

static void
ete_finalize (GObject *object)
{
	ETableExtras *ete = E_TABLE_EXTRAS (object);

	if (ete->cells) {
		g_hash_table_destroy (ete->cells);
		ete->cells = NULL;
	}

	if (ete->compares) {
		g_hash_table_destroy (ete->compares);
		ete->compares = NULL;
	}

	if (ete->searches) {
		g_hash_table_destroy (ete->searches);
		ete->searches = NULL;
	}

	if (ete->pixbufs) {
		g_hash_table_destroy (ete->pixbufs);
		ete->pixbufs = NULL;
	}

	G_OBJECT_CLASS (ete_parent_class)->finalize (object);
}

static void
ete_class_init (ETableExtrasClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ete_finalize;
}

static gint
e_strint_compare(gconstpointer data1, gconstpointer data2)
{
	gint int1 = atoi(data1);
	gint int2 = atoi(data2);

	return e_int_compare(GINT_TO_POINTER(int1), GINT_TO_POINTER(int2));
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
e_string_search(gconstpointer haystack, const gchar *needle)
{
	gint length;
	if (haystack == NULL)
		return FALSE;

	length = g_utf8_strlen (needle, -1);
	if (g_utf8_strncasecmp (haystack, needle, length) == 0)
		return TRUE;
	else
		return FALSE;
}

static void
safe_unref (gpointer object)
{
	if (object != NULL)
		g_object_unref (object);
}

static void
ete_init (ETableExtras *extras)
{
	extras->cells = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) safe_unref);

	extras->compares = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

	extras->searches = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

	extras->pixbufs = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) safe_unref);

	e_table_extras_add_compare(extras, "string", e_str_compare);
	e_table_extras_add_compare(extras, "stringcase", e_str_case_compare);
	e_table_extras_add_compare(extras, "collate", e_collate_compare);
	e_table_extras_add_compare(extras, "integer", e_int_compare);
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

ETableExtras *
e_table_extras_new (void)
{
	ETableExtras *ete = g_object_new (E_TABLE_EXTRAS_TYPE, NULL);

	return (ETableExtras *) ete;
}

void
e_table_extras_add_cell     (ETableExtras *extras,
			     const gchar  *id,
			     ECell        *cell)
{
	if (cell)
		g_object_ref_sink (cell);
	g_hash_table_insert (extras->cells, g_strdup(id), cell);
}

ECell *
e_table_extras_get_cell     (ETableExtras *extras,
			     const gchar  *id)
{
	return g_hash_table_lookup(extras->cells, id);
}

void
e_table_extras_add_compare  (ETableExtras *extras,
			     const gchar  *id,
			     GCompareFunc  compare)
{
	g_hash_table_insert(extras->compares, g_strdup(id), (gpointer) compare);
}

GCompareFunc
e_table_extras_get_compare  (ETableExtras *extras,
			     const gchar  *id)
{
	return (GCompareFunc) g_hash_table_lookup(extras->compares, id);
}

void
e_table_extras_add_search  (ETableExtras     *extras,
			    const gchar      *id,
			    ETableSearchFunc  search)
{
	g_hash_table_insert(extras->searches, g_strdup(id), search);
}

ETableSearchFunc
e_table_extras_get_search  (ETableExtras *extras,
			    const gchar  *id)
{
	return g_hash_table_lookup(extras->searches, id);
}

void
e_table_extras_add_pixbuf     (ETableExtras *extras,
			       const gchar  *id,
			       GdkPixbuf    *pixbuf)
{
	if (pixbuf)
		g_object_ref(pixbuf);
	g_hash_table_insert (extras->pixbufs, g_strdup(id), pixbuf);
}

GdkPixbuf *
e_table_extras_get_pixbuf     (ETableExtras *extras,
			       const gchar  *id)
{
	return g_hash_table_lookup(extras->pixbufs, id);
}
