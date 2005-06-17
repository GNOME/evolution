/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-without.c
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

#include "e-util/e-util.h"

#include "e-table-without.h"

#define PARENT_TYPE E_TABLE_SUBSET_TYPE

#define INCREMENT_AMOUNT 10

static ETableSubsetClass *parent_class;

struct _ETableWithoutPrivate {
	GHashTable *hash;

	GHashFunc hash_func;
	GCompareFunc compare_func;

	ETableWithoutGetKeyFunc get_key_func;
	ETableWithoutDuplicateKeyFunc duplicate_key_func;
	ETableWithoutFreeKeyFunc free_gotten_key_func;
	ETableWithoutFreeKeyFunc free_duplicated_key_func;

	void *closure;
};

static gboolean 
check (ETableWithout *etw, int model_row)
{
	gboolean ret_val;
	void *key;
	ETableSubset *etss = E_TABLE_SUBSET (etw);

	if (etw->priv->get_key_func)
		key = etw->priv->get_key_func (etss->source, model_row, etw->priv->closure);
	else
		key = GINT_TO_POINTER (model_row);
	ret_val = (g_hash_table_lookup (etw->priv->hash, key) != NULL);
	if (etw->priv->free_gotten_key_func)
		etw->priv->free_gotten_key_func (key, etw->priv->closure);
	return ret_val;
}

static gboolean 
check_with_key (ETableWithout *etw, void *key, int model_row)
{
	gboolean ret_val;
	void *key2;
	ETableSubset *etss = E_TABLE_SUBSET (etw);

	if (etw->priv->get_key_func)
		key2 = etw->priv->get_key_func (etss->source, model_row, etw->priv->closure);
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
etw_view_to_model_row (ETableWithout *etw, int view_row)
{
	ETableSubset *etss = E_TABLE_SUBSET (etw);
	return etss->map_table[view_row];
}

static void
add_row (ETableWithout *etw, int model_row)
{
	ETableSubset *etss = E_TABLE_SUBSET (etw);

	e_table_model_pre_change (E_TABLE_MODEL (etw));

	etss->map_table = g_renew (int, etss->map_table, etss->n_map + 1);

	etss->map_table[etss->n_map++] = model_row;

	e_table_model_row_inserted (E_TABLE_MODEL (etw), etss->n_map - 1);
}

static void
remove_row (ETableWithout *etw, int view_row)
{
	ETableSubset *etss = E_TABLE_SUBSET (etw);

	e_table_model_pre_change (E_TABLE_MODEL (etw));
	memmove (etss->map_table + view_row, etss->map_table + view_row + 1, (etss->n_map - view_row - 1) * sizeof (int));
	etss->n_map --;
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
	ETableWithout *etw = E_TABLE_WITHOUT (object);

	if (etw->priv) {
		if (etw->priv->hash) {
			g_hash_table_foreach (etw->priv->hash, delete_hash_element, etw);
			g_hash_table_destroy (etw->priv->hash);
			etw->priv->hash = NULL;
		}
		g_free (etw->priv);
		etw->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
etw_proxy_model_rows_inserted (ETableSubset *etss, ETableModel *etm, int model_row, int count)
{
	int i;
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
etw_proxy_model_rows_deleted (ETableSubset *etss, ETableModel *etm, int model_row, int count)
{
	int i; /* View row */
	ETableWithout *etw = E_TABLE_WITHOUT (etss);
	gboolean shift = FALSE;

	for (i = 0; i < etss->n_map; i++) {
		if (etss->map_table[i] >= model_row && etss->map_table[i] < model_row + count) {
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
etw_proxy_model_changed (ETableSubset *etss, ETableModel *etm)
{
	int i; /* Model row */
	int j; /* View row */
	int row_count;
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

	if (parent_class->proxy_model_changed)
		parent_class->proxy_model_changed (etss, etm);
}

static void
etw_class_init (ETableWithoutClass *klass)
{
	ETableSubsetClass *etss_class         = E_TABLE_SUBSET_CLASS (klass);
	GObjectClass *object_class            = G_OBJECT_CLASS (klass);

	parent_class                          = g_type_class_ref (PARENT_TYPE);

	object_class->dispose                 = etw_dispose;

	etss_class->proxy_model_rows_inserted = etw_proxy_model_rows_inserted;
	etss_class->proxy_model_rows_deleted  = etw_proxy_model_rows_deleted;
	etss_class->proxy_model_changed       = etw_proxy_model_changed;
}

static void
etw_init (ETableWithout *etw)
{
	etw->priv                           = g_new (ETableWithoutPrivate, 1);
	etw->priv->hash_func                = NULL;
	etw->priv->compare_func             = NULL;
	etw->priv->get_key_func             = NULL;
	etw->priv->duplicate_key_func       = NULL;
	etw->priv->free_gotten_key_func     = NULL;
	etw->priv->free_duplicated_key_func = NULL;
}

E_MAKE_TYPE(e_table_without, "ETableWithout", ETableWithout, etw_class_init, etw_init, PARENT_TYPE)

ETableModel *
e_table_without_construct (ETableWithout                 *etw,
			   ETableModel                   *source,
			   GHashFunc                      hash_func,
			   GCompareFunc                   compare_func,
			   ETableWithoutGetKeyFunc        get_key_func,
			   ETableWithoutDuplicateKeyFunc  duplicate_key_func,
			   ETableWithoutFreeKeyFunc       free_gotten_key_func,
			   ETableWithoutFreeKeyFunc       free_duplicated_key_func,
			   void                          *closure)
{
	if (e_table_subset_construct (E_TABLE_SUBSET(etw), source, 1) == NULL)
		return NULL;
	E_TABLE_SUBSET(etw)->n_map = 0;

	etw->priv->hash_func                = hash_func;
	etw->priv->compare_func 	    = compare_func;
	etw->priv->get_key_func 	    = get_key_func;
	etw->priv->duplicate_key_func 	    = duplicate_key_func;
	etw->priv->free_gotten_key_func     = free_gotten_key_func;
	etw->priv->free_duplicated_key_func = free_duplicated_key_func;
	etw->priv->closure                  = closure;

	etw->priv->hash = g_hash_table_new (etw->priv->hash_func, etw->priv->compare_func);

	return E_TABLE_MODEL (etw);
}

ETableModel *
e_table_without_new        (ETableModel                   *source,
			    GHashFunc                      hash_func,
			    GCompareFunc                   compare_func,
			    ETableWithoutGetKeyFunc        get_key_func,
			    ETableWithoutDuplicateKeyFunc  duplicate_key_func,
			    ETableWithoutFreeKeyFunc       free_gotten_key_func,
			    ETableWithoutFreeKeyFunc       free_duplicated_key_func,
			    void                          *closure)
{
	ETableWithout *etw = g_object_new (E_TABLE_WITHOUT_TYPE, NULL);

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

void         e_table_without_hide       (ETableWithout *etw,
					 void          *key)
{
	int i; /* View row */
	ETableSubset *etss = E_TABLE_SUBSET (etw);

	if (etw->priv->duplicate_key_func)
		key = etw->priv->duplicate_key_func (key, etw->priv->closure);

	g_hash_table_insert (etw->priv->hash, key, key);
	for (i = 0; i < etss->n_map; i++) {
		if (check_with_key (etw, key, etw_view_to_model_row (etw, i))) {
			remove_row (etw, i);
			i --;
		}
	}
}

/* An adopted key will later be freed using the free_duplicated_key function. */
void         e_table_without_hide_adopt (ETableWithout *etw,
					 void          *key)
{
	int i; /* View row */
	ETableSubset *etss = E_TABLE_SUBSET (etw);

	g_hash_table_insert (etw->priv->hash, key, key);
	for (i = 0; i < etss->n_map; i++) {
		if (check_with_key (etw, key, etw_view_to_model_row (etw, i))) {
			remove_row (etw, i);
			i --;
		}
	}
}

void
e_table_without_show       (ETableWithout *etw,
			    void          *key)
{
	int i; /* Model row */
	ETableSubset *etss = E_TABLE_SUBSET (etw);
	int count;
	void *old_key;

	count = e_table_model_row_count (etss->source);

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
e_table_without_show_all   (ETableWithout *etw)
{
	int i; /* Model row */
	int row_count;
	ETableSubset *etss = E_TABLE_SUBSET (etw);

	e_table_model_pre_change (E_TABLE_MODEL (etw));

	if (etw->priv->hash) {
		g_hash_table_foreach (etw->priv->hash, delete_hash_element, etw);
		g_hash_table_destroy (etw->priv->hash);
		etw->priv->hash = NULL;
	}
	etw->priv->hash = g_hash_table_new (etw->priv->hash_func, etw->priv->compare_func);

	row_count = e_table_model_row_count (E_TABLE_MODEL(etss->source));
	g_free (etss->map_table);
	etss->map_table = g_new (int, row_count);

	for (i = 0; i < row_count; i++) {
		etss->map_table[i] = i;
	}
	etss->n_map = row_count;

	e_table_model_changed (E_TABLE_MODEL (etw));
}
