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

static int
ets_column_count (ETableModel *etm)
{
	ETableSorted *ets = (ETableSorted *)etm;

	return e_table_model_column_count (ets->source);
}

static const char *
ets_column_name (ETableModel *etm, int col)
{
	ETableSorted *ets = (ETableSorted *)etm;

	return e_table_model_column_name (ets->source, col);
}

static int
ets_row_count (ETableModel *etm)
{
	ETableSorted *ets = (ETableSorted *)etm;

	return e_table_model_row_count (ets->source);
}

static void *
ets_value_at (ETableModel *etm, int col, int row)
{
	ETableSorted *ets = (ETableSorted *)etm;

	return e_table_model_value_at (ets->source, col, ets->map_table [row]);
}

static void
ets_set_value_at (ETableModel *etm, int col, int row, void *val)
{
	ETableSorted *ets = (ETableSorted *)etm;

	return e_table_model_set_value_at (ets->source, col, ets->map_table [row], val);
}

static gboolean
ets_is_cell_editable (ETableModel *etm, int col, int row)
{
	ETableSorted *ets = (ETableSorted *)etm;

	return e_table_model_is_cell_editable (ets->source, col, ets->map_table [row]);
}

static int
ets_row_height (ETableModel *etm, int row)
{
	ETableSorted *ets = (ETableSorted *)etm;

	return e_table_model_row_height (ets->source, ets->map_table [row]);
}

static void
ets_class_init (GtkObjectClass *klass)
{
	ETableModelClass *table_class = (ETableModelClass *) klass;
	
	ets_parent_class = gtk_type_class (PARENT_TYPE);
	klass->destroy = ets_destroy;

	table_class->column_count     = ets_column_count;
	table_class->column_name      = ets_column_name;
	table_class->row_count        = ets_row_count;
	table_class->value_at         = ets_value_at;
	table_class->set_value_at     = ets_set_value_at;
	table_class->is_cell_editable = ets_is_cell_editable;
	table_class->row_height       = ets_row_height;
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

	
