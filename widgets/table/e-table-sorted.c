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
#include "e-table-sorted.h"
#include "e-table-sorting-utils.h"

#define d(x)

#define PARENT_TYPE E_TABLE_SUBSET_TYPE

#define INCREMENT_AMOUNT 100

/* maximum insertions between an idle event that we will do without scheduling an idle sort */
#define ETS_INSERT_MAX (4)

static ETableSubsetClass *ets_parent_class;

static void ets_sort_info_changed        (ETableSortInfo *info, ETableSorted *ets);
static void ets_sort                     (ETableSorted *ets);
static void ets_proxy_model_changed      (ETableSubset *etss, ETableModel *source);
static void ets_proxy_model_row_changed  (ETableSubset *etss, ETableModel *source, int row);
static void ets_proxy_model_cell_changed (ETableSubset *etss, ETableModel *source, int col, int row);
static void ets_proxy_model_row_inserted (ETableSubset *etss, ETableModel *source, int row);
static void ets_proxy_model_row_deleted  (ETableSubset *etss, ETableModel *source, int row);

static void
ets_destroy (GtkObject *object)
{
	ETableSorted *ets = E_TABLE_SORTED (object);

	if (ets->sort_idle_id) {
		g_source_remove(ets->sort_idle_id);
	}
	if (ets->insert_idle_id) {
		g_source_remove(ets->insert_idle_id);
	}

	if (ets->sort_info) {
		gtk_signal_disconnect (GTK_OBJECT (ets->sort_info),
				       ets->sort_info_changed_id);
		gtk_object_unref(GTK_OBJECT(ets->sort_info));
	}

	if (ets->full_header)
		gtk_object_unref(GTK_OBJECT(ets->full_header));

	GTK_OBJECT_CLASS (ets_parent_class)->destroy (object);
}

static void
ets_class_init (GtkObjectClass *object_class)
{
	ETableSubsetClass *etss_class = E_TABLE_SUBSET_CLASS(object_class);

	ets_parent_class = gtk_type_class (PARENT_TYPE);

	etss_class->proxy_model_changed = ets_proxy_model_changed;
	etss_class->proxy_model_row_changed = ets_proxy_model_row_changed;
	etss_class->proxy_model_cell_changed = ets_proxy_model_cell_changed;
	etss_class->proxy_model_row_inserted = ets_proxy_model_row_inserted;
	etss_class->proxy_model_row_deleted = ets_proxy_model_row_deleted;

	object_class->destroy = ets_destroy;
}

static void
ets_init (ETableSorted *ets)
{
	ets->full_header = NULL;
	ets->sort_info = NULL;

	ets->sort_info_changed_id = 0;

	ets->sort_idle_id = 0;
	ets->insert_count = 0;
}

E_MAKE_TYPE(e_table_sorted, "ETableSorted", ETableSorted, ets_class_init, ets_init, PARENT_TYPE);

static gboolean
ets_sort_idle(ETableSorted *ets)
{
	gtk_object_ref(GTK_OBJECT(ets));
	ets_sort(ets);
	ets->sort_idle_id = 0;
	ets->insert_count = 0;
	gtk_object_unref(GTK_OBJECT(ets));
	return FALSE;
}

#if 0
static gboolean
ets_insert_idle(ETableSorted *ets)
{
	ets->insert_count = 0;
	ets->insert_idle_id = 0;
	return FALSE;
}
#endif

#if 0
/* This takes source rows. */
static int
ets_compare(ETableSorted *ets, int row1, int row2)
{
	int j;
	int sort_count = e_table_sort_info_sorting_get_count(ets->sort_info);
	int comp_val = 0;
	int ascending = 1;
	ETableSubset *etss = E_TABLE_SUBSET(ets);

	for (j = 0; j < sort_count; j++) {
		ETableSortColumn column = e_table_sort_info_sorting_get_nth(ets->sort_info, j);
		ETableCol *col;
		col = e_table_header_get_column_by_col_idx(ets->full_header, column.column);
		if (col == NULL)
			col = e_table_header_get_column (ets->full_header, e_table_header_count (ets->full_header) - 1);
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
ets_add       (ETableSorted *ets
	       gint          row)
{
	ETableModel *etm = E_TABLE_MODEL(etssv);
	ETableSubset *etss = E_TABLE_SUBSET(etssv);
	ETableSorted *ets = E_TABLE_SORTED (etssv);
	int i;

	if (etss->n_map + 1 > etssv->n_vals_allocated) {
		etssv->n_vals_allocated += INCREMENT_AMOUNT;
		etss->map_table = g_realloc (etss->map_table, (etssv->n_vals_allocated) * sizeof(int));
	}
	i = etss->n_map;
	if (ets->sort_idle_id == 0) {
		/* this is to see if we're inserting a lot of things between idle loops.
		   If we are, we're busy, its faster to just append and perform a full sort later */
		ets->insert_count++;
		if (ets->insert_count > ETS_INSERT_MAX) {
			/* schedule a sort, and append instead */
			ets->sort_idle_id = g_idle_add_full(50, (GSourceFunc) ets_sort_idle, ets, NULL);
		} else {
			/* make sure we have an idle handler to reset the count every now and then */
			if (ets->insert_idle_id == 0) {
				ets->insert_idle_id = g_idle_add_full(40, (GSourceFunc) ets_insert_idle, ets, NULL);
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
							if (ets_compare(ets, etss->map_table[i], row) >= 0)
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
				while (i < etss->n_map && ets_compare(ets, etss->map_table[i], row) < 0)
					i++;
			}
			memmove(etss->map_table + i + 1, etss->map_table + i, (etss->n_map - i) * sizeof(int));
		}
	}
	etss->map_table[i] = row;
	etss->n_map++;

	e_table_model_row_inserted (etm, i);
}

#endif

ETableModel *
e_table_sorted_new (ETableModel *source, ETableHeader *full_header, ETableSortInfo *sort_info)
{
	ETableSorted *ets = gtk_type_new (E_TABLE_SORTED_TYPE);
	ETableSubset *etss = E_TABLE_SUBSET (ets);

	if (e_table_subset_construct (etss, source, 0) == NULL){
		gtk_object_unref (GTK_OBJECT (ets));
		return NULL;
	}

	ets->sort_info = sort_info;
	gtk_object_ref(GTK_OBJECT(ets->sort_info));
	ets->full_header = full_header;
	gtk_object_ref(GTK_OBJECT(ets->full_header));

	ets_proxy_model_changed(etss, source);

	ets->sort_info_changed_id = gtk_signal_connect (GTK_OBJECT (sort_info), "sort_info_changed",
							 GTK_SIGNAL_FUNC (ets_sort_info_changed), ets);

	return E_TABLE_MODEL(ets);
}

static void
ets_sort_info_changed (ETableSortInfo *info, ETableSorted *ets)
{
	ets_sort(ets);
}

static void
ets_proxy_model_changed (ETableSubset *subset, ETableModel *source)
{
	int rows, i;

	rows = e_table_model_row_count(source);

	g_free(subset->map_table);
	subset->n_map = rows;
	subset->map_table = g_new(int, rows);

	for (i = 0; i < rows; i++) {
		subset->map_table[i] = i;
	}

	if (!E_TABLE_SORTED(subset)->sort_idle_id)
		E_TABLE_SORTED(subset)->sort_idle_id = g_idle_add_full(50, (GSourceFunc) ets_sort_idle, subset, NULL);

	e_table_model_changed(E_TABLE_MODEL(subset));
}

static void
ets_proxy_model_row_changed (ETableSubset *subset, ETableModel *source, int row)
{
	if (!E_TABLE_SORTED(subset)->sort_idle_id)
		E_TABLE_SORTED(subset)->sort_idle_id = g_idle_add_full(50, (GSourceFunc) ets_sort_idle, subset, NULL);
}

static void
ets_proxy_model_cell_changed (ETableSubset *subset, ETableModel *source, int col, int row)
{
	ETableSorted *ets = E_TABLE_SORTED(subset);
	if (e_table_sorting_utils_affects_sort(source, ets->sort_info, ets->full_header, col))
		ets_proxy_model_row_changed(subset, source, row); 
	else if (ets_parent_class->proxy_model_cell_changed) {
		(ets_parent_class->proxy_model_cell_changed) (subset, source, col, row);
	}
		
}

static void
ets_proxy_model_row_inserted (ETableSubset *subset, ETableModel *source, int row)
{
	ets_proxy_model_changed(subset, source);
}

static void
ets_proxy_model_row_deleted (ETableSubset *subset, ETableModel *source, int row)
{
	ets_proxy_model_changed(subset, source);
}

static void
ets_sort(ETableSorted *ets)
{
	ETableSubset *etss = E_TABLE_SUBSET(ets);
	static int reentering = 0;
	if (reentering)
		return;
	reentering = 1;

	e_table_model_pre_change(E_TABLE_MODEL(ets));

	e_table_sorting_utils_sort(etss->source, ets->sort_info, ets->full_header, etss->map_table, etss->n_map);

	e_table_model_changed (E_TABLE_MODEL(ets));
	reentering = 0;
}
