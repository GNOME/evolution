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
#include "e-util.h"
#include "e-table-sorted.h"

#define PARENT_TYPE E_TABLE_MODEL_TYPE

static ETableModelClass *ets_parent_class;

static void
ets_destroy (GtkObject *object)
{
	ETableSorted *ets = E_TABLE_SORTED (object);

	gtk_object_unref (GTK_OBJECT (ets->source));
	gtk_object_unref (GTK_OBJECT (ets->header));
	free (ets->map_table);

	GTK_OBJECT_CLASS (ets_parent_class)->destroy (object);
}

static void
ets_class_init (GtkObjectClass *klass)
{
	ets_parent_class = gtk_type_class (PARENT_TYPE);
	klass->destroy = ets_destroy;
}

E_MAKE_TYPE(e_table_sorted, "ETableSorted", ETableSorted, ets_class_init, NULL, PARENT_TYPE);

static ETableSorted *sort_ets;

static int
my_sort (const void *a, const void *b)
{
	GCompareFunc comp;
	const int *ia = (const int *) a;
	const int *ib = (const int *) b;
	void *va, *vb;
	
	va = e_table_model_value_at (sort_ets->source, sort_ets->sort_idx, *ia);
	vb = e_table_model_value_at (sort_ets->source, sort_ets->sort_idx, *ib);

	comp = sort_ets->sort_col->compare;

	return (*comp) (va, vb);
}

ETableModel *
e_table_sorted_new (ETableModel *source, ETableHeader *header, short sort_field)
{
	ETableSorted *ets = gtk_type_new (E_TABLE_SORTED_TYPE);
	const int nvals = e_table_model_row_count (source);
	unsigned int *buffer;
	int i;

	buffer = malloc (sizeof (unsigned int *) * nvals);
	if (buffer = NULL)
		return NULL;
	ets->map_table = buffer;
	ets->n_map = nvals;
	ets->source = source;
	ets->header = header;
	ets->sort_col = e_table_header_get_column (header, sort_field);
	ets->sort_idx = sort_field;
	gtk_object_ref (GTK_OBJECT (source));
	gtk_object_ref (GTK_OBJECT (header));
	
	/* Init */
	for (i = 0; i < nvals; i++)
		ets->map_table [i] = i;

	/* Sort */
	g_assert (sort_ets == NULL);
	sort_ets = ets;
	qsort (ets->map_table, nvals, sizeof (unsigned int), my_sort);
	sort_ets = NULL;
	
	return (ETableModel *) ets;
}

	
