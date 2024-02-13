/*
 * e-table-extras.c - Set of hash table sort of thingies.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "e-cell-checkbox.h"
#include "e-cell-date.h"
#include "e-cell-date-int.h"
#include "e-cell-number.h"
#include "e-cell-pixbuf.h"
#include "e-cell-size.h"
#include "e-cell-text.h"
#include "e-cell-tree.h"
#include "e-table-extras.h"
#include "e-table-sorting-utils.h"

struct _ETableExtrasPrivate {
	GHashTable *cells;
	GHashTable *compares;
	GHashTable *icon_names;
	GHashTable *searches;
};

G_DEFINE_TYPE_WITH_PRIVATE (ETableExtras, e_table_extras, G_TYPE_OBJECT)

static void
ete_finalize (GObject *object)
{
	ETableExtras *self = E_TABLE_EXTRAS (object);

	g_clear_pointer (&self->priv->cells, g_hash_table_destroy);
	g_clear_pointer (&self->priv->compares, g_hash_table_destroy);
	g_clear_pointer (&self->priv->searches, g_hash_table_destroy);
	g_clear_pointer (&self->priv->icon_names, g_hash_table_destroy);

	G_OBJECT_CLASS (e_table_extras_parent_class)->finalize (object);
}

static void
e_table_extras_class_init (ETableExtrasClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = ete_finalize;
}

static gint
e_strint_compare (gconstpointer data1,
                  gconstpointer data2)
{
	gint int1 = atoi (data1);
	gint int2 = atoi (data2);

	return e_int_compare (GINT_TO_POINTER (int1), GINT_TO_POINTER (int2));
}

static gint
e_int64ptr_compare (gconstpointer data1,
		    gconstpointer data2)
{
	const gint64 *pa = data1, *pb = data2;

	if (pa && pb)
		return (*pa == *pb) ? 0 : (*pa < *pb) ? -1 : 1;

	/* sort unset values before set */
	return (!pa && !pb) ? 0 : (pa ? 1 : -1);
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

			/* Collation is locale-dependent, so this
			 * totally fails to do the right thing. */
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
e_string_search (gconstpointer haystack,
                 const gchar *needle)
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

static gint
e_table_str_case_compare (gconstpointer x,
                          gconstpointer y,
                          gpointer cmp_cache)
{
	const gchar *cx = NULL, *cy = NULL;

	if (!cmp_cache)
		return e_str_case_compare (x, y);

	if (x == NULL || y == NULL) {
		if (x == y)
			return 0;
		else
			return x ? -1 : 1;
	}

	#define prepare_value(_z, _cz) \
		_cz = e_table_sorting_utils_lookup_cmp_cache (cmp_cache, _z); \
		if (!_cz) { \
			gchar *tmp = g_utf8_casefold (_z, -1); \
			_cz = g_utf8_collate_key (tmp, -1); \
			g_free (tmp); \
 \
			e_table_sorting_utils_add_to_cmp_cache ( \
				cmp_cache, _z, (gchar *) _cz); \
		}

	prepare_value (x, cx);
	prepare_value (y, cy);

	#undef prepare_value

	return strcmp (cx, cy);
}

static gint
e_table_collate_compare (gconstpointer x,
                         gconstpointer y,
                         gpointer cmp_cache)
{
	const gchar *cx = NULL, *cy = NULL;

	if (!cmp_cache)
		return e_collate_compare (x, y);

	if (x == NULL || y == NULL) {
		if (x == y)
			return 0;
		else
			return x ? -1 : 1;
	}

	#define prepare_value(_z, _cz) \
		_cz = e_table_sorting_utils_lookup_cmp_cache (cmp_cache, _z); \
		if (!_cz) { \
			_cz = g_utf8_collate_key (_z, -1); \
 \
			e_table_sorting_utils_add_to_cmp_cache ( \
				cmp_cache, _z, (gchar *) _cz); \
		}

	prepare_value (x, cx);
	prepare_value (y, cy);

	#undef prepare_value

	return strcmp (cx, cy);
}

static void
safe_unref (gpointer object)
{
	if (object != NULL)
		g_object_unref (object);
}

static void
e_table_extras_init (ETableExtras *extras)
{
	ECell *cell, *sub_cell;

	extras->priv = e_table_extras_get_instance_private (extras);

	extras->priv->cells = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) safe_unref);

	extras->priv->compares = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

	extras->priv->icon_names = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	extras->priv->searches = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

	e_table_extras_add_compare (
		extras, "string",
		(GCompareDataFunc) e_str_compare);
	e_table_extras_add_compare (
		extras, "stringcase",
		(GCompareDataFunc) e_table_str_case_compare);
	e_table_extras_add_compare (
		extras, "collate",
		(GCompareDataFunc) e_table_collate_compare);
	e_table_extras_add_compare (
		extras, "integer",
		(GCompareDataFunc) e_int_compare);
	e_table_extras_add_compare (
		extras, "string-integer",
		(GCompareDataFunc) e_strint_compare);
	e_table_extras_add_compare (
		extras, "pointer-integer64",
		(GCompareDataFunc) e_int64ptr_compare);

	e_table_extras_add_search (extras, "string", e_string_search);

	cell = e_cell_checkbox_new ();
	e_table_extras_add_cell (extras, "checkbox", cell);
	g_object_unref (cell);

	cell = e_cell_date_new (NULL, GTK_JUSTIFY_LEFT);
	e_table_extras_add_cell (extras, "date", cell);
	g_object_unref (cell);

	cell = e_cell_date_int_new (NULL, GTK_JUSTIFY_LEFT);
	e_table_extras_add_cell (extras, "date-int", cell);
	g_object_unref (cell);

	cell = e_cell_number_new (NULL, GTK_JUSTIFY_RIGHT);
	e_table_extras_add_cell (extras, "number", cell);
	g_object_unref (cell);

	cell = e_cell_pixbuf_new ();
	e_table_extras_add_cell (extras, "pixbuf", cell);
	g_object_unref (cell);

	cell = e_cell_size_new (NULL, GTK_JUSTIFY_RIGHT);
	e_table_extras_add_cell (extras, "size", cell);
	g_object_unref (cell);

	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	e_table_extras_add_cell (extras, "string", cell);
	g_object_unref (cell);

	sub_cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	cell = e_cell_tree_new (TRUE, TRUE, sub_cell);
	e_table_extras_add_cell (extras, "tree-string", cell);
	g_object_unref (sub_cell);
	g_object_unref (cell);
}

ETableExtras *
e_table_extras_new (void)
{
	return g_object_new (E_TYPE_TABLE_EXTRAS, NULL);
}

void
e_table_extras_add_cell (ETableExtras *extras,
                         const gchar *id,
                         ECell *cell)
{
	g_return_if_fail (E_IS_TABLE_EXTRAS (extras));
	g_return_if_fail (id != NULL);

	if (cell != NULL)
		g_object_ref_sink (cell);

	g_hash_table_insert (extras->priv->cells, g_strdup (id), cell);
}

ECell *
e_table_extras_get_cell (ETableExtras *extras,
                         const gchar *id)
{
	g_return_val_if_fail (E_IS_TABLE_EXTRAS (extras), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	return g_hash_table_lookup (extras->priv->cells, id);
}

void
e_table_extras_add_compare (ETableExtras *extras,
                            const gchar *id,
                            GCompareDataFunc compare)
{
	g_return_if_fail (E_IS_TABLE_EXTRAS (extras));
	g_return_if_fail (id != NULL);

	g_hash_table_insert (
		extras->priv->compares,
		g_strdup (id), (gpointer) compare);
}

GCompareDataFunc
e_table_extras_get_compare (ETableExtras *extras,
                            const gchar *id)
{
	g_return_val_if_fail (E_IS_TABLE_EXTRAS (extras), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	return g_hash_table_lookup (extras->priv->compares, id);
}

void
e_table_extras_add_search (ETableExtras *extras,
                           const gchar *id,
                           ETableSearchFunc search)
{
	g_return_if_fail (E_IS_TABLE_EXTRAS (extras));
	g_return_if_fail (id != NULL);

	g_hash_table_insert (
		extras->priv->searches,
		g_strdup (id), (gpointer) search);
}

ETableSearchFunc
e_table_extras_get_search (ETableExtras *extras,
                           const gchar *id)
{
	g_return_val_if_fail (E_IS_TABLE_EXTRAS (extras), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	return g_hash_table_lookup (extras->priv->searches, id);
}

void
e_table_extras_add_icon_name (ETableExtras *extras,
                              const gchar *id,
                              const gchar *icon_name)
{
	g_return_if_fail (E_IS_TABLE_EXTRAS (extras));
	g_return_if_fail (id != NULL);

	g_hash_table_insert (
		extras->priv->icon_names,
		g_strdup (id), g_strdup (icon_name));
}

const gchar *
e_table_extras_get_icon_name (ETableExtras *extras,
                              const gchar *id)
{
	g_return_val_if_fail (E_IS_TABLE_EXTRAS (extras), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	return g_hash_table_lookup (extras->priv->icon_names, id);
}
