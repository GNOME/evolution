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
#include <gtk/gtksignal.h>
#include "gal/util/e-util.h"
#include "e-table-subset.h"

static void etss_proxy_model_pre_change_real (ETableSubset *etss, ETableModel *etm);
static void etss_proxy_model_changed_real (ETableSubset *etss, ETableModel *etm);
static void etss_proxy_model_row_changed_real (ETableSubset *etss, ETableModel *etm, int row);
static void etss_proxy_model_cell_changed_real (ETableSubset *etss, ETableModel *etm, int col, int row);

#define PARENT_TYPE E_TABLE_MODEL_TYPE
#define d(x)

static ETableModelClass *etss_parent_class;

#define ETSS_CLASS(object) (E_TABLE_SUBSET_CLASS(GTK_OBJECT(object)->klass))

static void
etss_destroy (GtkObject *object)
{
	ETableSubset *etss = E_TABLE_SUBSET (object);

	if (etss->source) {
		gtk_signal_disconnect (GTK_OBJECT (etss->source),
				       etss->table_model_pre_change_id);
		gtk_signal_disconnect (GTK_OBJECT (etss->source),
				       etss->table_model_changed_id);
		gtk_signal_disconnect (GTK_OBJECT (etss->source),
				       etss->table_model_row_changed_id);
		gtk_signal_disconnect (GTK_OBJECT (etss->source),
				       etss->table_model_cell_changed_id);
		gtk_signal_disconnect (GTK_OBJECT (etss->source),
				       etss->table_model_row_inserted_id);
		gtk_signal_disconnect (GTK_OBJECT (etss->source),
				       etss->table_model_row_deleted_id);

		gtk_object_unref (GTK_OBJECT (etss->source));
		etss->source = NULL;

		etss->table_model_changed_id = 0;
		etss->table_model_row_changed_id = 0;
		etss->table_model_cell_changed_id = 0;
		etss->table_model_row_inserted_id = 0;
		etss->table_model_row_deleted_id = 0;
	}

	g_free (etss->map_table);

	GTK_OBJECT_CLASS (etss_parent_class)->destroy (object);
}

static int
etss_column_count (ETableModel *etm)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_column_count (etss->source);
}

static int
etss_row_count (ETableModel *etm)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return etss->n_map;
}

static void *
etss_value_at (ETableModel *etm, int col, int row)
{
	ETableSubset *etss = (ETableSubset *)etm;

	etss->last_access = row;
	d(g_print("g) Setting last_access to %d\n", row));
	return e_table_model_value_at (etss->source, col, etss->map_table [row]);
}

static void
etss_set_value_at (ETableModel *etm, int col, int row, const void *val)
{
	ETableSubset *etss = (ETableSubset *)etm;

	etss->last_access = row;
	d(g_print("h) Setting last_access to %d\n", row));
	return e_table_model_set_value_at (etss->source, col, etss->map_table [row], val);
}

static gboolean
etss_is_cell_editable (ETableModel *etm, int col, int row)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_is_cell_editable (etss->source, col, etss->map_table [row]);
}

static void
etss_append_row (ETableModel *etm, ETableModel *source, int row)
{
	ETableSubset *etss = (ETableSubset *)etm;
	e_table_model_append_row (etss->source, source, row);
}

static void *
etss_duplicate_value (ETableModel *etm, int col, const void *value)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_duplicate_value (etss->source, col, value);
}

static void
etss_free_value (ETableModel *etm, int col, void *value)
{
	ETableSubset *etss = (ETableSubset *)etm;

	e_table_model_free_value (etss->source, col, value);
}

static void *
etss_initialize_value (ETableModel *etm, int col)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_initialize_value (etss->source, col);
}

static gboolean
etss_value_is_empty (ETableModel *etm, int col, const void *value)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_value_is_empty (etss->source, col, value);
}

static char *
etss_value_to_string (ETableModel *etm, int col, const void *value)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_value_to_string (etss->source, col, value);
}

static void
etss_class_init (GtkObjectClass *object_class)
{
	ETableSubsetClass *klass        = (ETableSubsetClass *) object_class;
	ETableModelClass *table_class   = (ETableModelClass *) object_class;

	etss_parent_class               = gtk_type_class (PARENT_TYPE);
	
	object_class->destroy           = etss_destroy;

	table_class->column_count       = etss_column_count;
	table_class->row_count          = etss_row_count;
	table_class->value_at           = etss_value_at;
	table_class->set_value_at       = etss_set_value_at;
	table_class->is_cell_editable   = etss_is_cell_editable;
	table_class->append_row         = etss_append_row;
	table_class->duplicate_value    = etss_duplicate_value;
	table_class->free_value         = etss_free_value;
	table_class->initialize_value   = etss_initialize_value;
	table_class->value_is_empty     = etss_value_is_empty;
	table_class->value_to_string    = etss_value_to_string;

	klass->proxy_model_pre_change   = etss_proxy_model_pre_change_real;
	klass->proxy_model_changed      = etss_proxy_model_changed_real;
	klass->proxy_model_row_changed  = etss_proxy_model_row_changed_real;
	klass->proxy_model_cell_changed = etss_proxy_model_cell_changed_real;
	klass->proxy_model_row_inserted = NULL;
	klass->proxy_model_row_deleted  = NULL;
}

static void
etss_init (ETableSubset *etss)
{
	etss->last_access = 0;
}

E_MAKE_TYPE(e_table_subset, "ETableSubset", ETableSubset, etss_class_init, etss_init, PARENT_TYPE);

static void
etss_proxy_model_pre_change_real (ETableSubset *etss, ETableModel *etm)
{
	e_table_model_pre_change (E_TABLE_MODEL (etss));
}

static void
etss_proxy_model_changed_real (ETableSubset *etss, ETableModel *etm)
{
	e_table_model_changed (E_TABLE_MODEL (etss));
}

static void
etss_proxy_model_row_changed_real (ETableSubset *etss, ETableModel *etm, int row)
{
	int limit;
	const int n = etss->n_map;
	const int * const map_table = etss->map_table;
	int i;

	limit = MIN(n, etss->last_access + 10);
	for (i = etss->last_access; i < limit; i++) {
		if (map_table [i] == row){
			e_table_model_row_changed (E_TABLE_MODEL (etss), i);
			d(g_print("a) Found %d from %d\n", i, etss->last_access));
			etss->last_access = i;
			return;
		}
	}

	limit = MAX(0, etss->last_access - 10);
	for (i = etss->last_access - 1; i >= limit; i--) {
		if (map_table [i] == row){
			e_table_model_row_changed (E_TABLE_MODEL (etss), i);
			d(g_print("b) Found %d from %d\n", i, etss->last_access));
			etss->last_access = i;
			return;
		}
	}

	for (i = 0; i < n; i++){
		if (map_table [i] == row){
			e_table_model_row_changed (E_TABLE_MODEL (etss), i);
			d(g_print("c) Found %d from %d\n", i, etss->last_access));
			etss->last_access = i;
			return;
		}
	}
}

static void
etss_proxy_model_cell_changed_real (ETableSubset *etss, ETableModel *etm, int col, int row)
{
	int limit;
	const int n = etss->n_map;
	const int * const map_table = etss->map_table;
	int i;

	limit = MIN(n, etss->last_access + 10);
	for (i = etss->last_access; i < limit; i++) {
		if (map_table [i] == row){
			e_table_model_cell_changed (E_TABLE_MODEL (etss), col, i);
			d(g_print("d) Found %d from %d\n", i, etss->last_access));
			etss->last_access = i;
			return;
		}
	}

	limit = MAX(0, etss->last_access - 10);
	for (i = etss->last_access - 1; i >= limit; i--) {
		if (map_table [i] == row){
			e_table_model_cell_changed (E_TABLE_MODEL (etss), col, i);
			d(g_print("e) Found %d from %d\n", i, etss->last_access));
			etss->last_access = i;
			return;
		}
	}
		
	for (i = 0; i < n; i++){
		if (map_table [i] == row){
			e_table_model_cell_changed (E_TABLE_MODEL (etss), col, i);
			d(g_print("f) Found %d from %d\n", i, etss->last_access));
			etss->last_access = i;
			return;
		}
	}
}

static void
etss_proxy_model_pre_change (ETableModel *etm, ETableSubset *etss)
{
	if (ETSS_CLASS(etss)->proxy_model_pre_change)
		(ETSS_CLASS(etss)->proxy_model_pre_change) (etss, etm);
}

static void
etss_proxy_model_changed (ETableModel *etm, ETableSubset *etss)
{
	if (ETSS_CLASS(etss)->proxy_model_changed)
		(ETSS_CLASS(etss)->proxy_model_changed) (etss, etm);
}

static void
etss_proxy_model_row_changed (ETableModel *etm, int row, ETableSubset *etss)
{
	if (ETSS_CLASS(etss)->proxy_model_row_changed)
		(ETSS_CLASS(etss)->proxy_model_row_changed) (etss, etm, row);
}

static void
etss_proxy_model_cell_changed (ETableModel *etm, int row, int col, ETableSubset *etss)
{
	if (ETSS_CLASS(etss)->proxy_model_cell_changed)
		(ETSS_CLASS(etss)->proxy_model_cell_changed) (etss, etm, col, row);
}

static void
etss_proxy_model_row_inserted (ETableModel *etm, int row, ETableSubset *etss)
{
	if (ETSS_CLASS(etss)->proxy_model_row_inserted)
		(ETSS_CLASS(etss)->proxy_model_row_inserted) (etss, etm, row);
}

static void
etss_proxy_model_row_deleted (ETableModel *etm, int row, ETableSubset *etss)
{
	if (ETSS_CLASS(etss)->proxy_model_row_deleted)
		(ETSS_CLASS(etss)->proxy_model_row_deleted) (etss, etm, row);
}

ETableModel *
e_table_subset_construct (ETableSubset *etss, ETableModel *source, int nvals)
{
	unsigned int *buffer;
	int i;

	if (nvals) {
		buffer = (unsigned int *) g_malloc (sizeof (unsigned int) * nvals);
		if (buffer == NULL)
			return NULL;
	} else
		buffer = NULL;
	etss->map_table = buffer;
	etss->n_map = nvals;
	etss->source = source;
	gtk_object_ref (GTK_OBJECT (source));
	
	/* Init */
	for (i = 0; i < nvals; i++)
		etss->map_table [i] = i;

	etss->table_model_pre_change_id = gtk_signal_connect (GTK_OBJECT (source), "model_pre_change",
							      GTK_SIGNAL_FUNC (etss_proxy_model_pre_change), etss);
	etss->table_model_changed_id = gtk_signal_connect (GTK_OBJECT (source), "model_changed",
						     GTK_SIGNAL_FUNC (etss_proxy_model_changed), etss);
	etss->table_model_row_changed_id = gtk_signal_connect (GTK_OBJECT (source), "model_row_changed",
							 GTK_SIGNAL_FUNC (etss_proxy_model_row_changed), etss);
	etss->table_model_cell_changed_id = gtk_signal_connect (GTK_OBJECT (source), "model_cell_changed",
							  GTK_SIGNAL_FUNC (etss_proxy_model_cell_changed), etss);
	etss->table_model_row_inserted_id = gtk_signal_connect (GTK_OBJECT (source), "model_row_inserted",
								GTK_SIGNAL_FUNC (etss_proxy_model_row_inserted), etss);
	etss->table_model_row_deleted_id = gtk_signal_connect (GTK_OBJECT (source), "model_row_deleted",
							       GTK_SIGNAL_FUNC (etss_proxy_model_row_deleted), etss);
	
	return E_TABLE_MODEL (etss);
}

ETableModel *
e_table_subset_new (ETableModel *source, const int nvals)
{
	ETableSubset *etss = gtk_type_new (E_TABLE_SUBSET_TYPE);

	if (e_table_subset_construct (etss, source, nvals) == NULL){
		gtk_object_unref (GTK_OBJECT (etss));
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
		return table->source;
}

void
e_table_subset_print_debugging  (ETableSubset *table_model)
{
	int i;
	for (i = 0; i < table_model->n_map; i++) {
		g_print("%8d\n", table_model->map_table[i]);
	}
}
