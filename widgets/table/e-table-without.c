/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-subset.c: Implements a table that contains a subset of another table.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Ximian, Inc.
 */
#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtksignal.h>
#include "gal/util/e-util.h"
#include "e-table-without.h"

#define ETW_CLASS(e) ((ETableWithoutClass *)((GtkObject *)e)->klass)

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

	key = etw->priv->get_key_func (etss->source, model_row, etw->priv->closure);
	ret_val = (g_hash_table_lookup (etw->priv->hash, key) != NULL);
	etw->priv->free_gotten_key_func (key, etw->priv->closure);
	return ret_val;
}

static gboolean 
check_with_key (ETableWithout *etw, void *key, int model_row)
{
	gboolean ret_val;
	void *key2;
	ETableSubset *etss = E_TABLE_SUBSET (etw);

	key2 = etw->priv->get_key_func (etss->source, model_row, etw->priv->closure);
	ret_val = (etw->priv->compare_func (key, key2));
	etw->priv->free_gotten_key_func (key, etw->priv->closure);
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

	etss->map_table = g_renew (int, etss->map_table, etss->n_map + 1);

	etss->map_table[etss->n_map++] = model_row;

	e_table_model_row_inserted (E_TABLE_MODEL (etw), etss->n_map - 1);
}

static void
remove_row (ETableWithout *etw, int view_row)
{
	ETableSubset *etss = E_TABLE_SUBSET (etw);

	memmove (etss->map_table + view_row, etss->map_table + view_row + 1, (etss->n_map - view_row - 1) * sizeof (int));
	etss->n_map --;
	e_table_model_row_deleted (E_TABLE_MODEL (etw), view_row);
}

static void
etw_proxy_model_rows_inserted (ETableSubset *etss, ETableModel *etm, int model_row, int count)
{
	int i;
	ETableWithout *etw = E_TABLE_WITHOUT (etss);

	/* i is View row */
	for (i = 0; i < etss->n_map; i++) {
		if (etss->map_table[i] > model_row)
			etss->map_table[i] += count;
	}

	/* i is Model row */
	for (i = model_row; i < model_row + count; i++) {
		if (check (etw, i)) {
			add_row (etw, i);
		}
	}
}

static void
etw_proxy_model_rows_deleted (ETableSubset *etss, ETableModel *etm, int model_row, int count)
{
	int i; /* View row */
	ETableWithout *etw = E_TABLE_WITHOUT (etss);

	for (i = 0; i < etss->n_map; i++) {
		if (etss->map_table[i] >= model_row && etss->map_table[i] < model_row + count) {
			remove_row (etw, i);
			i--;
		} else if (etss->map_table[i] >= model_row + count)
			etss->map_table[i] -= count;
	}
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
		if (check (etw, i)) {
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
	ETableSubsetClass *etss_class = E_TABLE_SUBSET_CLASS (klass);
	parent_class = gtk_type_class (PARENT_TYPE);

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

E_MAKE_TYPE(e_table_without, "ETableWithout", ETableWithout, etw_class_init, etw_init, PARENT_TYPE);

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
	ETableWithout *etw = gtk_type_new (E_TABLE_WITHOUT_TYPE);

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
		gtk_object_unref (GTK_OBJECT (etw));
		return NULL;
	}

	return (ETableModel *) etw;
}

void         e_table_without_add        (ETableWithout *etw,
					 void          *key)
{
	int i; /* View row */
	ETableSubset *etss = E_TABLE_SUBSET (etw);

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
void         e_table_without_add_adopt  (ETableWithout *etw,
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

void         e_table_without_remove     (ETableWithout *etw,
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
		etw->priv->free_duplicated_key_func (key, etw->priv->closure);
		g_hash_table_remove (etw->priv->hash, key);
	}
}
