/*
 * E-table-subset.c: Implements a table that contains a subset of another table.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Helix Code, Inc.
 */
#include <config.h>
#include "e-util.h"
#include "e-table-subset.h"

#define PARENT_TYPE E_TABLE_MODEL_TYPE

static ETableModelClass *etss_parent_class;

static void
etss_destroy (GtkObject *object)
{
	ETableSubset *etss = E_TABLE_SUBSET (object);

	gtk_object_unref (GTK_OBJECT (etss->source));
	free (ets->subset_table);

	GTK_OBJECT_CLASS (ets_parent_class)->destroy (object);
}

static void
etss_class_init (GtkObjectClass *klass)
{
	etss_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->destroy = etss_destroy;
}

E_MAKE_TYPE(e_table_subset, "ETableSubset", ETableSubset, etss_class_init, NULL, PARENT_TYPE);

ETableModel *
e_table_subset_new (ETableModel *source, ETableHeader *header, short sort_field)
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

