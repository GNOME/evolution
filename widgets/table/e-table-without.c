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
};

static void
etw_proxy_model_rows_inserted (ETableSubset *etss, ETableModel *etm, int row, int count)
{
	int i;

	for (i = 0; i < etss->n_map; i++) {
		if (etss->map_table[i] > row)
			etss->map_table[i] += count;
	}

	for (i = row; i < row + count; i++) {
		if (check ()) {
			add_row ();
		}
	}
}

static void
etw_proxy_model_rows_deleted (ETableSubset *etss, ETableModel *etm, int row, int count)
{
	for (i = 0; i < n_map; i++) {
		if (etss->map_table[i] >= row && etss->map_table[i] < row + count)
			remove_row ();
		else if (etss->map_table[i] >= row + count)
			etss->map_table[i] -= count;
	}
}

static void
etw_proxy_model_changed (ETableSubset *etss, ETableModel *etm)
{
	int i;
	int j;
	int row_count;
	g_free (etss->map_table);
	row_count = e_table_model_row_count (etm);
	etss->map_table = g_new (int, row_count);
	for (i = 0; i < row_count; i++) {
		if (check ()) {
			etss->map_table[j++] = i;
		}
	}
	etss->n_map = j;

	if (parent_class->proxy_model_changed)
		parent_class->proxy_model_changed (etss, etm);
}

static void
etw_class_init (GtkObjectClass *object_class)
{
	ETableWithoutClass *klass = E_TABLE_WITHOUT_CLASS (object_class);
	ETableSubsetClass *etss_class = E_TABLE_SUBSET_CLASS (object_class);
	parent_class = gtk_type_class (PARENT_TYPE);

	etss_class->proxy_model_rows_inserted = etw_proxy_model_rows_inserted;
	etss_class->proxy_model_rows_deleted  = etw_proxy_model_rows_deleted;
	etss_class->proxy_model_changed       = etw_proxy_model_changed;
}

E_MAKE_TYPE(e_table_without, "ETableWithout", ETableWithout, etw_class_init, NULL, PARENT_TYPE);

ETableModel *
e_table_without_construct (ETableWithout *etw,
				   ETableModel          *source)
{
	if (e_table_subset_construct (E_TABLE_SUBSET(etw), source, 1) == NULL)
		return NULL;
	E_TABLE_SUBSET(etw)->n_map = 0;

	return E_TABLE_MODEL (etw);
}

ETableModel *
e_table_without_new (ETableModel *source)
{
	ETableWithout *etw = gtk_type_new (E_TABLE_WITHOUT_TYPE);

	if (e_table_without_construct (etw, source) == NULL){
		gtk_object_unref (GTK_OBJECT (etw));
		return NULL;
	}

	return (ETableModel *) etw;
}

void         e_table_without_add        (ETableWithout *etw,
					 void          *key)
{
	ETableSubset *etss = E_TABLE_SUBSET (etw);
	g_hash_table_insert (etw->priv->hash, key, key);
	for (i = 0; i < etss->n_map; i++) {
		if (check_with_key (etss->source, i, key)) {
			remove_row ();
		}
	}
}

void         e_table_without_remove     (ETableWithout *etw,
					 void          *key)
{
	ETableSubset *etss = E_TABLE_SUBSET (etw);
	for (i = 0; i < count; i++) {
		if (check_with_key (etss)) {
			add_row ();
		}
	}
}
