/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-subset.c: Implements a table that contains a subset of another table.
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
#include "e-table-subset-variable.h"

#define ETSSV_CLASS(e) ((ETableSubsetVariableClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE E_TABLE_SUBSET_TYPE

#define INCREMENT_AMOUNT 10

static ETableSubsetClass *etssv_parent_class;

static void
etssv_add       (ETableSubsetVariable *etssv,
		 gint                  row)
{
	ETableModel *etm = E_TABLE_MODEL(etssv);
	ETableSubset *etss = E_TABLE_SUBSET(etssv);
	
	if ( etss->n_map + 1 > etssv->n_vals_allocated ) {
		etss->map_table = g_realloc(etss->map_table, (etssv->n_vals_allocated + INCREMENT_AMOUNT) * sizeof(int));
		etssv->n_vals_allocated += INCREMENT_AMOUNT;
	}
	etss->map_table[etss->n_map++] = row;
	if ( !etm->frozen )
		e_table_model_changed(etm);
}

static gboolean
etssv_remove    (ETableSubsetVariable *etssv,
		 gint                  row)
{
	ETableModel *etm = E_TABLE_MODEL(etssv);
	ETableSubset *etss = E_TABLE_SUBSET(etssv);
	int i;
	
	for ( i = 0; i < etss->n_map; i++ ) {
		if (etss->map_table[i] == row) {
			memmove(etss->map_table + i, etss->map_table + i + 1, (etss->n_map - i - 1) * sizeof(int));
			etss->n_map --;
			if ( !etm->frozen )
				e_table_model_changed(etm);
			return TRUE;
		}
	}
	return FALSE;
}

static void
etssv_class_init (GtkObjectClass *object_class)
{
	ETableSubsetVariableClass *klass = E_TABLE_SUBSET_VARIABLE_CLASS(object_class);
	etssv_parent_class = gtk_type_class (PARENT_TYPE);

	klass->add = etssv_add;
	klass->remove = etssv_remove;
}

E_MAKE_TYPE(e_table_subset_variable, "ETableSubsetVariable", ETableSubsetVariable, etssv_class_init, NULL, PARENT_TYPE);

ETableModel *
e_table_subset_variable_construct (ETableSubsetVariable *etssv,
				   ETableModel          *source)
{
	if ( e_table_subset_construct(E_TABLE_SUBSET(etssv), source, 1) == NULL )
		return NULL;
	E_TABLE_SUBSET(etssv)->n_map = 0;

	return E_TABLE_MODEL (etssv);
}

ETableModel *
e_table_subset_variable_new (ETableModel *source)
{
	ETableSubsetVariable *etssv = gtk_type_new (E_TABLE_SUBSET_VARIABLE_TYPE);

	if (e_table_subset_variable_construct (etssv, source) == NULL){
		gtk_object_destroy (GTK_OBJECT (etssv));
		return NULL;
	}

	return (ETableModel *) etssv;
}

void
e_table_subset_variable_add       (ETableSubsetVariable *etssv,
				   gint                  row)
{
	g_return_if_fail (etssv != NULL);
	g_return_if_fail (E_IS_TABLE_SUBSET_VARIABLE(etssv));

	if (ETSSV_CLASS(etssv)->add)
		ETSSV_CLASS (etssv)->add (etssv, row);
}

gboolean
e_table_subset_variable_remove    (ETableSubsetVariable *etssv,
				   gint                  row)
{
	g_return_val_if_fail (etssv != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_SUBSET_VARIABLE(etssv), FALSE);

	if (ETSSV_CLASS(etssv)->remove)
		return ETSSV_CLASS (etssv)->remove (etssv, row);
	else
		return FALSE;
}

void
e_table_subset_variable_increment (ETableSubsetVariable *etssv,
				   gint                  position,
				   gint                  amount)
{
	int i;
	ETableSubset *etss = E_TABLE_SUBSET(etssv);
	for ( i = 0; i < etss->n_map; i++ ) {
		if ( etss->map_table[i] > position )
			etss->map_table[i] += amount;
	}
}

void
e_table_subset_variable_set_allocation (ETableSubsetVariable *etssv,
					gint total)
{
	ETableSubset *etss = E_TABLE_SUBSET(etssv);
	if ( total <= 0 )
		total = 1;
	if ( total > etss->n_map ) {
		etss->map_table = g_realloc(etss->map_table, total * sizeof(int));
	}
}
