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
#include "e-util/e-util.h"
#include "e-table-sorted.h"

#define PARENT_TYPE E_TABLE_SUBSET_TYPE

static ETableSubsetClass *ets_parent_class;

static void
ets_class_init (GtkObjectClass *klass)
{
	ets_parent_class = gtk_type_class (PARENT_TYPE);
}

E_MAKE_TYPE(e_table_sorted, "ETableSorted", ETableSorted, ets_class_init, NULL, PARENT_TYPE);

static ETableSorted *sort_ets;

static int
my_sort (const void *a, const void *b)
{
	ETableModel *source = E_TABLE_SUBSET (sort_ets)->source;
	const int *ia = (const int *) a;
	const int *ib = (const int *) b;
	void *va, *vb;
	
	va = e_table_model_value_at (source, sort_ets->sort_col, *ia);
	vb = e_table_model_value_at (source, sort_ets->sort_col, *ib);

	return (*sort_ets->compare) (va, vb);
}

static void
do_sort (ETableSorted *ets)
{
	ETableSubset *etss = E_TABLE_SUBSET (ets);
	g_assert (sort_ets == NULL);
	
	sort_ets = ets;
	qsort (etss->map_table, etss->n_map, sizeof (unsigned int), my_sort);
	sort_ets = NULL;
}

ETableModel *
e_table_sorted_new (ETableModel *source, int col, GCompareFunc compare)
{
	ETableSorted *ets = gtk_type_new (E_TABLE_SORTED_TYPE);
	ETableSubset *etss = E_TABLE_SUBSET (ets);
	const int nvals = e_table_model_row_count (source);
	int i;

	if (e_table_subset_construct (etss, source, nvals) == NULL){
		gtk_object_destroy (GTK_OBJECT (ets));
		return NULL;
	}
	
	ets->compare = compare;
	ets->sort_col = col;
	
	/* Init */
	for (i = 0; i < nvals; i++)
		etss->map_table [i] = i;

	do_sort (ets);
	
	return (ETableModel *) ets;
}

void
e_table_sorted_resort (ETableSorted *ets, int col, GCompareFunc compare)
{
	if (col == -1 || compare == NULL)
		do_sort (ets);
	else {
		ets->sort_col = col;
		ets->compare = compare;
		do_sort (ets);
	}
}

