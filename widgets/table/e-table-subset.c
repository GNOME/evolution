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

	if (etss->source)
		gtk_object_unref (GTK_OBJECT (etss->source));

	if (etss->map_table)
		free (etss->map_table);

	GTK_OBJECT_CLASS (etss_parent_class)->destroy (object);
}

static int
etss_column_count (ETableModel *etm)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_column_count (etss->source);
}

static const char *
etss_column_name (ETableModel *etm, int col)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_column_name (etss->source, col);
}

static int
etss_row_count (ETableModel *etm)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_row_count (etss->source);
}

static void *
etss_value_at (ETableModel *etm, int col, int row)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_value_at (etss->source, col, etss->map_table [row]);
}

static void
etss_set_value_at (ETableModel *etm, int col, int row, void *val)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_set_value_at (etss->source, col, etss->map_table [row], val);
}

static gboolean
etss_is_cell_editable (ETableModel *etm, int col, int row)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_is_cell_editable (etss->source, col, etss->map_table [row]);
}

static int
etss_row_height (ETableModel *etm, int row)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_row_height (etss->source, etss->map_table [row]);
}

static void
etss_class_init (GtkObjectClass *klass)
{
	ETableModelClass *table_class = (ETableModelClass *) klass;

	etss_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->destroy = etss_destroy;

	table_class->column_count     = etss_column_count;
	table_class->column_name      = etss_column_name;
	table_class->row_count        = etss_row_count;
	table_class->value_at         = etss_value_at;
	table_class->set_value_at     = etss_set_value_at;
	table_class->is_cell_editable = etss_is_cell_editable;
	table_class->row_height       = etss_row_height;

}

E_MAKE_TYPE(e_table_subset, "ETableSubset", ETableSubset, etss_class_init, NULL, PARENT_TYPE);

ETableModel *
e_table_subset_construct (ETableSubset *etss, ETableModel *source, int nvals)
{
	unsigned int *buffer;
	int i;

	buffer = (unsigned int *) malloc (sizeof (unsigned int *) * nvals);
	if (buffer = NULL)
		return NULL;
	etss->map_table = buffer;
	etss->n_map = nvals;
	etss->source = source;
	gtk_object_ref (GTK_OBJECT (source));
	
	/* Init */
	for (i = 0; i < nvals; i++)
		etss->map_table [i] = i;

}

ETableModel *
e_table_subset_new (ETableModel *source, const int nvals)
{
	ETableSubset *etss = gtk_type_new (E_TABLE_SUBSET_TYPE);

	if (e_table_subset_construct (etss, source, nvals) == NULL){
		gtk_object_destroy (GTK_OBJECT (etss));
		return NULL;
	}

	return (ETableModel *) etss;
}

ETableModel *
e_table_subset_get_toplevel (ETableSubset *table)
{
	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_SUBSET (table), NULL);

	if (E_IS_TABLE_SUBSET (table->source))
		return e_table_subset_get_toplevel (E_TABLE_SUBSET (table->source));
	else
		return table->subset;
}
