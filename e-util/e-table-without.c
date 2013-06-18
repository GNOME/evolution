/*
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "e-table-without.h"

#define E_TABLE_WITHOUT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TABLE_WITHOUT, ETableWithoutPrivate))

G_DEFINE_TYPE (ETableWithout, e_table_without, E_TYPE_TABLE_SUBSET)

#define INCREMENT_AMOUNT 10

struct _ETableWithoutPrivate {
	GHashTable *hash;

	GHashFunc hash_func;
	GCompareFunc compare_func;

	ETableWithoutGetKeyFunc get_key_func;
	ETableWithoutDuplicateKeyFunc duplicate_key_func;
	ETableWithoutFreeKeyFunc free_gotten_key_func;
	ETableWithoutFreeKeyFunc free_duplicated_key_func;

	gpointer closure;
};

static gboolean
check (ETableWithout *etw,
       gint model_row)
{
	gboolean ret_val;
	gpointer key;
	ETableSubset *etss = E_TABLE_SUBSET (etw);
	ETableModel *source_model;

	source_model = e_table_subset_get_source_model (etss);

	if (etw->priv->get_key_func)
		key = etw->priv->get_key_func (
			source_model, model_row, etw->priv->closure);
	else
		key = GINT_TO_POINTER (model_row);
	ret_val = (g_hash_table_lookup (etw->priv->hash, key) != NULL);
	if (etw->priv->free_gotten_key_func)
		etw->priv->free_gotten_key_func (key, etw->priv->closure);
	return ret_val;
}

static gboolean
check_with_key (ETableWithout *etw,
                gpointer key,
                gint model_row)
{
	gboolean ret_val;
	gpointer key2;
	ETableSubset *etss = E_TABLE_SUBSET (etw);
	ETableModel *source_model;

	source_model = e_table_subset_get_source_model (etss);

	if (etw->priv->get_key_func)
		key2 = etw->priv->get_key_func (
			source_model, model_row, etw->priv->closure);
	else
		key2 = GINT_TO_POINTER (model_row);
	if (etw->priv->compare_func)
		ret_val = (etw->priv->compare_func (key, key2));
	else
		ret_val = (key == key2);
	if (etw->priv->free_gotten_key_func)
		etw->priv->free_gotten_key_func (key2, etw->priv->closure);
	return ret_val;
}

static gint
etw_view_to_model_row (ETableWithout *etw,
                       gint view_row)
{
	ETableSubset *etss = E_TABLE_SUBSET (etw);
	return etss->map_table[view_row];
}

static void
add_row (ETableWithout *etw,
         gint model_row)
{
	ETableSubset *etss = E_TABLE_SUBSET (etw);

	e_table_model_pre_change (E_TABLE_MODEL (etw));

	etss->map_table = g_renew (int, etss->map_table, etss->n_map + 1);

	etss->map_table[etss->n_map++] = model_row;

	e_table_model_row_inserted (E_TABLE_MODEL (etw), etss->n_map - 1);
}

static void
remove_row (ETableWithout *etw,
            gint view_row)
{
	ETableSubset *etss = E_TABLE_SUBSET (etw);

	e_table_model_pre_change (E_TABLE_MODEL (etw));
	memmove (
		etss->map_table + view_row,
		etss->map_table + view_row + 1,
		(etss->n_map - view_row - 1) * sizeof (gint));
	etss->n_map--;
	e_table_model_row_deleted (E_TABLE_MODEL (etw), view_row);
}

static void
delete_hash_element (gpointer key,
                     gpointer value,
                     gpointer closure)
{
	ETableWithout *etw = closure;
	if (etw->priv->free_duplicated_key_func)
		etw->priv->free_duplicated_key_func (key, etw->priv->closure);
}

static void
etw_dispose (GObject *object)
{
	ETableWithoutPrivate *priv;

	priv = E_TABLE_WITHOUT_GET_PRIVATE (object);

	if (priv->hash != NULL) {
		g_hash_table_foreach (priv->hash, delete_hash_element, object);
		g_hash_table_destroy (priv->hash);
		priv->hash = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_table_without_parent_class)->dispose (object);
}

static void
etw_proxy_model_rows_inserted (ETableSubset *etss,
                               ETableModel *etm,
                               gint model_row,
                               gint count)
{
	gint i;
	ETableWithout *etw = E_TABLE_WITHOUT (etss);
	gboolean shift = FALSE;

	/* i is View row */
	if (model_row != etss->n_map) {
		for (i = 0; i < etss->n_map; i++) {
			if (etss->map_table[i] > model_row)
				etss->map_table[i] += count;
		}
		shift = TRUE;
	}

	/* i is Model row */
	for (i = model_row; i < model_row + count; i++) {
		if (!check (etw, i)) {
			add_row (etw, i);
		}
	}
	if (shift)
		e_table_model_changed (E_TABLE_MODEL (etw));
	else
		e_table_model_no_change (E_TABLE_MODEL (etw));
}

static void
etw_proxy_model_rows_deleted (ETableSubset *etss,
                              ETableModel *etm,
                              gint model_row,
                              gint count)
{
	gint i; /* View row */
	ETableWithout *etw = E_TABLE_WITHOUT (etss);
	gboolean shift = FALSE;

	for (i = 0; i < etss->n_map; i++) {
		if (etss->map_table[i] >= model_row &&
		    etss->map_table[i] < model_row + count) {
			remove_row (etw, i);
			i--;
		} else if (etss->map_table[i] >= model_row + count) {
			etss->map_table[i] -= count;
			shift = TRUE;
		}
	}
	if (shift)
		e_table_model_changed (E_TABLE_MODEL (etw));
	else
		e_table_model_no_change (E_TABLE_MODEL (etw));
}

static void
etw_proxy_model_changed (ETableSubset *etss,
                         ETableModel *etm)
{
	gint i; /* Model row */
	gint j; /* View row */
	gint row_count;
	ETableWithout *etw = E_TABLE_WITHOUT (etss);

	g_free (etss->map_table);
	row_count = e_table_model_row_count (etm);
	etss->map_table = g_new (int, row_count);

	for (i = 0, j = 0; i < row_count; i++) {
		if (!check (etw, i)) {
			etss->map_table[j++] = i;
		}
	}
	etss->n_map = j;

	if (E_TABLE_SUBSET_CLASS (e_table_without_parent_class)->proxy_model_changed)
		E_TABLE_SUBSET_CLASS (e_table_without_parent_class)->proxy_model_changed (etss, etm);
}

static void
e_table_without_class_init (ETableWithoutClass *class)
{
	GObjectClass *object_class;
	ETableSubsetClass *etss_class;

	g_type_class_add_private (class, sizeof (ETableWithoutPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = etw_dispose;

	etss_class = E_TABLE_SUBSET_CLASS (class);
	etss_class->proxy_model_rows_inserted = etw_proxy_model_rows_inserted;
	etss_class->proxy_model_rows_deleted  = etw_proxy_model_rows_deleted;
	etss_class->proxy_model_changed = etw_proxy_model_changed;
}

static void
e_table_without_init (ETableWithout *etw)
{
	etw->priv = E_TABLE_WITHOUT_GET_PRIVATE (etw);
}

ETableModel *
e_table_without_construct (ETableWithout *etw,
                           ETableModel *source,
                           GHashFunc hash_func,
                           GCompareFunc compare_func,
                           ETableWithoutGetKeyFunc get_key_func,
                           ETableWithoutDuplicateKeyFunc duplicate_key_func,
                           ETableWithoutFreeKeyFunc free_gotten_key_func,
                           ETableWithoutFreeKeyFunc free_duplicated_key_func,
                           gpointer closure)
{
	if (e_table_subset_construct (E_TABLE_SUBSET (etw), source, 1) == NULL)
		return NULL;
	E_TABLE_SUBSET (etw)->n_map = 0;

	etw->priv->hash_func                = hash_func;
	etw->priv->compare_func	    = compare_func;
	etw->priv->get_key_func	    = get_key_func;
	etw->priv->duplicate_key_func	    = duplicate_key_func;
	etw->priv->free_gotten_key_func     = free_gotten_key_func;
	etw->priv->free_duplicated_key_func = free_duplicated_key_func;
	etw->priv->closure                  = closure;

	etw->priv->hash = g_hash_table_new (
		etw->priv->hash_func, etw->priv->compare_func);

	return E_TABLE_MODEL (etw);
}

ETableModel *
e_table_without_new (ETableModel *source,
                     GHashFunc hash_func,
                     GCompareFunc compare_func,
                     ETableWithoutGetKeyFunc get_key_func,
                     ETableWithoutDuplicateKeyFunc duplicate_key_func,
                     ETableWithoutFreeKeyFunc free_gotten_key_func,
                     ETableWithoutFreeKeyFunc free_duplicated_key_func,
                     gpointer closure)
{
	ETableWithout *etw = g_object_new (E_TYPE_TABLE_WITHOUT, NULL);

	if (e_table_without_construct (etw,
				       source,
				       hash_func,
				       compare_func,
				       get_key_func,
				       duplicate_key_func,
				       free_gotten_key_func,
				       free_duplicated_key_func,
				       closure)
	    == NULL) {
		g_object_unref (etw);
		return NULL;
	}

	return (ETableModel *) etw;
}

void
e_table_without_hide (ETableWithout *etw,
                      gpointer key)
{
	gint i; /* View row */
	ETableSubset *etss = E_TABLE_SUBSET (etw);

	if (etw->priv->duplicate_key_func)
		key = etw->priv->duplicate_key_func (key, etw->priv->closure);

	g_hash_table_insert (etw->priv->hash, key, key);
	for (i = 0; i < etss->n_map; i++) {
		if (check_with_key (etw, key, etw_view_to_model_row (etw, i))) {
			remove_row (etw, i);
			i--;
		}
	}
}

/* An adopted key will later be freed using the free_duplicated_key function. */
void
e_table_without_hide_adopt (ETableWithout *etw,
                            gpointer key)
{
	gint i; /* View row */
	ETableSubset *etss = E_TABLE_SUBSET (etw);

	g_hash_table_insert (etw->priv->hash, key, key);
	for (i = 0; i < etss->n_map; i++) {
		if (check_with_key (etw, key, etw_view_to_model_row (etw, i))) {
			remove_row (etw, i);
			i--;
		}
	}
}

void
e_table_without_show (ETableWithout *etw,
                      gpointer key)
{
	gint i; /* Model row */
	ETableSubset *etss = E_TABLE_SUBSET (etw);
	ETableModel *source_model;
	gint count;
	gpointer old_key;

	source_model = e_table_subset_get_source_model (etss);
	count = e_table_model_row_count (source_model);

	for (i = 0; i < count; i++) {
		if (check_with_key (etw, key, i)) {
			add_row (etw, i);
		}
	}
	if (g_hash_table_lookup_extended (etw->priv->hash, key, &old_key, NULL)) {
#if 0
		if (etw->priv->free_duplicated_key_func)
			etw->priv->free_duplicated_key_func (key, etw->priv->closure);
#endif
		g_hash_table_remove (etw->priv->hash, key);
	}
}

void
e_table_without_show_all (ETableWithout *etw)
{
	gint i; /* Model row */
	gint row_count;
	ETableSubset *etss = E_TABLE_SUBSET (etw);
	ETableModel *source_model;

	e_table_model_pre_change (E_TABLE_MODEL (etw));

	if (etw->priv->hash) {
		g_hash_table_foreach (etw->priv->hash, delete_hash_element, etw);
		g_hash_table_destroy (etw->priv->hash);
		etw->priv->hash = NULL;
	}
	etw->priv->hash = g_hash_table_new (
		etw->priv->hash_func, etw->priv->compare_func);

	source_model = e_table_subset_get_source_model (etss);
	row_count = e_table_model_row_count (source_model);

	g_free (etss->map_table);
	etss->map_table = g_new (int, row_count);

	for (i = 0; i < row_count; i++) {
		etss->map_table[i] = i;
	}
	etss->n_map = row_count;

	e_table_model_changed (E_TABLE_MODEL (etw));
}
