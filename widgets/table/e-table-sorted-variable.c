/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-sorted.c: Implements a table that sorts another table
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Ximian, Inc.
 */
#include <config.h>
#include <stdlib.h>
#include <gtk/gtksignal.h>
#include <string.h>
#include "gal/util/e-util.h"
#include "e-table-sorted-variable.h"
#include "e-table-sorting-utils.h"

#define d(x)

#define PARENT_TYPE E_TABLE_SUBSET_VARIABLE_TYPE

#define INCREMENT_AMOUNT 100

/* maximum insertions between an idle event that we will do without scheduling an idle sort */
#define ETSV_INSERT_MAX (4)

static ETableSubsetVariableClass *etsv_parent_class;

static void etsv_sort_info_changed        (ETableSortInfo *info, ETableSortedVariable *etsv);
static void etsv_sort                     (ETableSortedVariable *etsv);
static void etsv_add                      (ETableSubsetVariable *etssv, gint                  row);
static void etsv_add_all                  (ETableSubsetVariable *etssv);

static void
etsv_destroy (GtkObject *object)
{
	ETableSortedVariable *etsv = E_TABLE_SORTED_VARIABLE (object);

	gtk_signal_disconnect (GTK_OBJECT (etsv->sort_info),
			       etsv->sort_info_changed_id);

	if (etsv->sort_idle_id) {
		g_source_remove(etsv->sort_idle_id);
	}
	if (etsv->insert_idle_id) {
		g_source_remove(etsv->insert_idle_id);
	}

	if (etsv->sort_info)
		gtk_object_unref(GTK_OBJECT(etsv->sort_info));
	if (etsv->full_header)
		gtk_object_unref(GTK_OBJECT(etsv->full_header));

	GTK_OBJECT_CLASS (etsv_parent_class)->destroy (object);
}

static void
etsv_class_init (GtkObjectClass *object_class)
{
	ETableSubsetVariableClass *etssv_class = E_TABLE_SUBSET_VARIABLE_CLASS(object_class);

	etsv_parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = etsv_destroy;

	etssv_class->add = etsv_add;
	etssv_class->add_all = etsv_add_all;
}

static void
etsv_init (ETableSortedVariable *etsv)
{
	etsv->full_header = NULL;
	etsv->sort_info = NULL;

	etsv->sort_info_changed_id = 0;

	etsv->sort_idle_id = 0;
	etsv->insert_count = 0;
}

E_MAKE_TYPE(e_table_sorted_variable, "ETableSortedVariable", ETableSortedVariable, etsv_class_init, etsv_init, PARENT_TYPE);

static gboolean
etsv_sort_idle(ETableSortedVariable *etsv)
{
	gtk_object_ref(GTK_OBJECT(etsv));
	etsv_sort(etsv);
	etsv->sort_idle_id = 0;
	etsv->insert_count = 0;
	gtk_object_unref(GTK_OBJECT(etsv));
	return FALSE;
}

static gboolean
etsv_insert_idle(ETableSortedVariable *etsv)
{
	etsv->insert_count = 0;
	etsv->insert_idle_id = 0;
	return FALSE;
}

/* This takes source rows. */
static int
etsv_compare(ETableSortedVariable *etsv, int row1, int row2)
{
	int j;
	int sort_count = e_table_sort_info_sorting_get_count(etsv->sort_info);
	int comp_val = 0;
	int ascending = 1;
	ETableSubset *etss = E_TABLE_SUBSET(etsv);

	for (j = 0; j < sort_count; j++) {
		ETableSortColumn column = e_table_sort_info_sorting_get_nth(etsv->sort_info, j);
		ETableCol *col;
		col = e_table_header_get_column_by_col_idx(etsv->full_header, column.column);
		if (col == NULL)
			col = e_table_header_get_column (etsv->full_header, e_table_header_count (etsv->full_header) - 1);
		comp_val = (*col->compare)(e_table_model_value_at (etss->source, col->col_idx, row1),
					   e_table_model_value_at (etss->source, col->col_idx, row2));
		ascending = column.ascending;
		if (comp_val != 0)
			break;
	}
	if (comp_val == 0) {
		if (row1 < row2)
			comp_val = -1;
		if (row1 > row2)
			comp_val = 1;
	}
	if (!ascending)
		comp_val = -comp_val;
	return comp_val;
}


static void
etsv_add       (ETableSubsetVariable *etssv,
		gint                  row)
{
	ETableModel *etm = E_TABLE_MODEL(etssv);
	ETableSubset *etss = E_TABLE_SUBSET(etssv);
	ETableSortedVariable *etsv = E_TABLE_SORTED_VARIABLE (etssv);
	int i;

	if (etss->n_map + 1 > etssv->n_vals_allocated) {
		etssv->n_vals_allocated += INCREMENT_AMOUNT;
		etss->map_table = g_realloc (etss->map_table, (etssv->n_vals_allocated) * sizeof(int));
	}
	i = etss->n_map;
	if (etsv->sort_idle_id == 0) {
		/* this is to see if we're inserting a lot of things between idle loops.
		   If we are, we're busy, its faster to just append and perform a full sort later */
		etsv->insert_count++;
		if (etsv->insert_count > ETSV_INSERT_MAX) {
			/* schedule a sort, and append instead */
			etsv->sort_idle_id = g_idle_add_full(50, (GSourceFunc) etsv_sort_idle, etsv, NULL);
		} else {
			/* make sure we have an idle handler to reset the count every now and then */
			if (etsv->insert_idle_id == 0) {
				etsv->insert_idle_id = g_idle_add_full(40, (GSourceFunc) etsv_insert_idle, etsv, NULL);
			}
			i = 0;
			/* handle insertions when we have a 'sort group' */
			if (e_table_model_has_sort_group(etss->source)) {
				/* find the row this row maps to */
				char *group = g_strdup(e_table_model_row_sort_group(etss->source, row));
				const char *newgroup;
				int cmp, grouplen, newgrouplen;
				
				newgroup = strrchr(group, '/');
				grouplen = strlen(group);
				if (newgroup)
					cmp = newgroup-group;
				else
					cmp = grouplen;
				
				/* find first common parent */
				while (i<etss->n_map) {
					newgroup = e_table_model_row_sort_group(etss->source, etss->map_table[i]);
					if (strncmp(newgroup, group, cmp) == 0) {
						break;
					}
					i++;
				}

				/* check matching records */
				while (i<etss->n_map) {
					newgroup = e_table_model_row_sort_group(etss->source, etss->map_table[i]);
					newgrouplen = strlen(newgroup);
					if (strncmp(newgroup, group, cmp) == 0) {
						/* common parent, check for same level */
						if (grouplen == newgrouplen) {
							if (etsv_compare(etsv, etss->map_table[i], row) >= 0)
								break;
						} else if (strncmp(newgroup + cmp, group + cmp, grouplen - cmp) == 0)
							/* Found a child of the inserted node.  Insert here. */
							break;
					} else {
						/* ran out of common parents, insert here */
						break;
					}
					i++;
				}
				g_free(group);
			} else {
				while (i < etss->n_map && etsv_compare(etsv, etss->map_table[i], row) < 0)
					i++;
			}
			memmove(etss->map_table + i + 1, etss->map_table + i, (etss->n_map - i) * sizeof(int));
		}
	}
	etss->map_table[i] = row;
	etss->n_map++;

	e_table_model_row_inserted (etm, i);
}

static void
etsv_add_all   (ETableSubsetVariable *etssv)
{
	ETableModel *etm = E_TABLE_MODEL(etssv);
	ETableSubset *etss = E_TABLE_SUBSET(etssv);
	ETableSortedVariable *etsv = E_TABLE_SORTED_VARIABLE (etssv);
	int rows;
	int i;

	e_table_model_pre_change(etm);

	rows = e_table_model_row_count(etss->source);

	if (etss->n_map + rows > etssv->n_vals_allocated){
		etssv->n_vals_allocated += MAX(INCREMENT_AMOUNT, rows);
		etss->map_table = g_realloc (etss->map_table, etssv->n_vals_allocated * sizeof(int));
	}
	for (i = 0; i < rows; i++)
		etss->map_table[etss->n_map++] = i;

	if (etsv->sort_idle_id == 0) {
		etsv->sort_idle_id = g_idle_add_full(50, (GSourceFunc) etsv_sort_idle, etsv, NULL);
	}

	e_table_model_changed (etm);
}

ETableModel *
e_table_sorted_variable_new (ETableModel *source, ETableHeader *full_header, ETableSortInfo *sort_info)
{
	ETableSortedVariable *etsv = gtk_type_new (E_TABLE_SORTED_VARIABLE_TYPE);
	ETableSubsetVariable *etssv = E_TABLE_SUBSET_VARIABLE (etsv);

	if (e_table_subset_variable_construct (etssv, source) == NULL){
		gtk_object_unref (GTK_OBJECT (etsv));
		return NULL;
	}

	etsv->sort_info = sort_info;
	gtk_object_ref(GTK_OBJECT(etsv->sort_info));
	etsv->full_header = full_header;
	gtk_object_ref(GTK_OBJECT(etsv->full_header));

	etsv->sort_info_changed_id = gtk_signal_connect (GTK_OBJECT (sort_info), "sort_info_changed",
							 GTK_SIGNAL_FUNC (etsv_sort_info_changed), etsv);

	return E_TABLE_MODEL(etsv);
}

static void
etsv_sort_info_changed (ETableSortInfo *info, ETableSortedVariable *etsv)
{
	etsv_sort(etsv);
}

static void
etsv_sort(ETableSortedVariable *etsv)
{
	ETableSubset *etss = E_TABLE_SUBSET(etsv);
	static int reentering = 0;
	if (reentering)
		return;
	reentering = 1;

	e_table_model_pre_change(E_TABLE_MODEL(etsv));

	e_table_sorting_utils_sort(etss->source, etsv->sort_info, etsv->full_header, etss->map_table, etss->n_map);

	e_table_model_changed (E_TABLE_MODEL(etsv));
	reentering = 0;
}
