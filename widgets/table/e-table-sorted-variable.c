/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-sorted.c: Implements a table that sorts another table
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Helix Code, Inc.
 */
#include <config.h>
#include <stdlib.h>
#include <gtk/gtksignal.h>
#include <string.h>
#include "e-util/e-util.h"
#include "e-table-sorted-variable.h"

#define PARENT_TYPE E_TABLE_SUBSET_VARIABLE_TYPE

#define INCREMENT_AMOUNT 10

static ETableSubsetVariableClass *etsv_parent_class;

static void etsv_proxy_model_changed (ETableModel *etm, ETableSortedVariable *etsv);
static void etsv_proxy_model_row_changed (ETableModel *etm, int row, ETableSortedVariable *etsv);
static void etsv_proxy_model_cell_changed (ETableModel *etm, int col, int row, ETableSortedVariable *etsv);
static void etsv_add       (ETableSubsetVariable *etssv, gint                  row);

static void
etsv_destroy (GtkObject *object)
{
	ETableSortedVariable *etsv = E_TABLE_SORTED_VARIABLE (object);
	ETableSubset *etss = E_TABLE_SUBSET (object);

	gtk_signal_disconnect (GTK_OBJECT (etss->source),
			       etsv->table_model_changed_id);
	gtk_signal_disconnect (GTK_OBJECT (etss->source),
			       etsv->table_model_row_changed_id);
	gtk_signal_disconnect (GTK_OBJECT (etss->source),
			       etsv->table_model_cell_changed_id);

	etsv->table_model_changed_id = 0;
	etsv->table_model_row_changed_id = 0;
	etsv->table_model_cell_changed_id = 0;

	GTK_OBJECT_CLASS (etsv_parent_class)->destroy (object);
}

static void
etsv_class_init (GtkObjectClass *object_class)
{
	ETableSubsetVariableClass *etssv_class = E_TABLE_SUBSET_VARIABLE_CLASS(object_class);

	etsv_parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = etsv_destroy;

	etssv_class->add = etsv_add;
}

E_MAKE_TYPE(e_table_sorted_variable, "ETableSortedVariable", ETableSortedVariable, etsv_class_init, NULL, PARENT_TYPE);

static void
etsv_add       (ETableSubsetVariable *etssv,
		gint                  row)
{
	ETableModel *etm = E_TABLE_MODEL(etssv);
	ETableSubset *etss = E_TABLE_SUBSET(etssv);
	ETableSortedVariable *etsv = E_TABLE_SORTED_VARIABLE(etssv);
	int i;
	int col = etsv->sort_col;
	GCompareFunc comp = etsv->compare;
	gint ascending = etsv->ascending;

	void *val = e_table_model_value_at(etss->source, col, row);
	
	/* FIXME: binary search anyone? */
	for ( i = 0; i < etss->n_map; i++ ) {
		int comp_val = (*comp)(val, e_table_model_value_at(etss->source, col, etss->map_table[i]));
		if ( (ascending && comp_val < 0) || ((!ascending) && comp_val > 0) )
			break;
	}
	if ( etss->n_map + 1 > etssv->n_vals_allocated ) {
		etss->map_table = g_realloc(etss->map_table, (etssv->n_vals_allocated + INCREMENT_AMOUNT) * sizeof(int));
		etssv->n_vals_allocated += INCREMENT_AMOUNT;
	}
	if ( i < etss->n_map )
		memmove(etss->map_table + i + 1, etss->map_table + i, (etss->n_map - i) * sizeof(int));
	etss->map_table[i] = row;
	etss->n_map++;
	if ( !etm->frozen )
		e_table_model_changed(etm);
}

ETableModel *
e_table_sorted_variable_new (ETableModel *source, int col, int ascending, GCompareFunc compare)
{
	ETableSortedVariable *etsv = gtk_type_new (E_TABLE_SORTED_VARIABLE_TYPE);
	ETableSubsetVariable *etssv = E_TABLE_SUBSET_VARIABLE (etsv);

	if (e_table_subset_variable_construct (etssv, source) == NULL){
		gtk_object_destroy (GTK_OBJECT (etsv));
		return NULL;
	}
	
	etsv->sort_col = col;
	etsv->ascending = ascending;
	etsv->compare = compare;

	etsv->table_model_changed_id = gtk_signal_connect (GTK_OBJECT (source), "model_changed",
							   GTK_SIGNAL_FUNC (etsv_proxy_model_changed), etsv);
	etsv->table_model_row_changed_id = gtk_signal_connect (GTK_OBJECT (source), "model_row_changed",
							       GTK_SIGNAL_FUNC (etsv_proxy_model_row_changed), etsv);
	etsv->table_model_cell_changed_id = gtk_signal_connect (GTK_OBJECT (source), "model_cell_changed",
								GTK_SIGNAL_FUNC (etsv_proxy_model_cell_changed), etsv);
	
	return E_TABLE_MODEL(etsv);
}

static void
etsv_proxy_model_changed (ETableModel *etm, ETableSortedVariable *etsv)
{
	if ( !E_TABLE_MODEL(etsv)->frozen ) {
		/*		FIXME: do_resort(); */
	}
}

static void
etsv_proxy_model_row_changed (ETableModel *etm, int row, ETableSortedVariable *etsv)
{
	ETableSubsetVariable *etssv = E_TABLE_SUBSET_VARIABLE(etsv);
	if ( !E_TABLE_MODEL(etsv)->frozen ) {
		if(e_table_subset_variable_remove(etssv, row))
			e_table_subset_variable_add(etssv, row);
	}
}

static void
etsv_proxy_model_cell_changed (ETableModel *etm, int col, int row, ETableSortedVariable *etsv)
{
	ETableSubsetVariable *etssv = E_TABLE_SUBSET_VARIABLE(etsv);
	if ( !E_TABLE_MODEL(etsv)->frozen ) {
		if ( col == etsv->sort_col ) {
			if(e_table_subset_variable_remove(etssv, row))
				e_table_subset_variable_add(etssv, row);
		}
	}
}

