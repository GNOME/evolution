/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "e-util/e-util.h"

#include "e-table-sorted-variable.h"
#include "e-table-sorting-utils.h"

#define d(x)

#define INCREMENT_AMOUNT 100

/* maximum insertions between an idle event that we will do without scheduling an idle sort */
#define ETSV_INSERT_MAX (4)

/* workaround for avoiding API breakage */
#define etsv_get_type e_table_sorted_variable_get_type
G_DEFINE_TYPE (ETableSortedVariable, etsv, E_TABLE_SUBSET_VARIABLE_TYPE)

static void etsv_sort_info_changed        (ETableSortInfo *info, ETableSortedVariable *etsv);
static void etsv_sort                     (ETableSortedVariable *etsv);
static void etsv_add                      (ETableSubsetVariable *etssv, gint                  row);
static void etsv_add_all                  (ETableSubsetVariable *etssv);

static void
etsv_dispose (GObject *object)
{
	ETableSortedVariable *etsv = E_TABLE_SORTED_VARIABLE (object);

	if (etsv->sort_info_changed_id)
		g_signal_handler_disconnect (G_OBJECT (etsv->sort_info),
					     etsv->sort_info_changed_id);
	etsv->sort_info_changed_id = 0;

	if (etsv->sort_idle_id) {
		g_source_remove(etsv->sort_idle_id);
		etsv->sort_idle_id = 0;
	}
	if (etsv->insert_idle_id) {
		g_source_remove(etsv->insert_idle_id);
		etsv->insert_idle_id = 0;
	}

	if (etsv->sort_info)
		g_object_unref(etsv->sort_info);
	etsv->sort_info = NULL;

	if (etsv->full_header)
		g_object_unref(etsv->full_header);
	etsv->full_header = NULL;

	G_OBJECT_CLASS (etsv_parent_class)->dispose (object);
}

static void
etsv_class_init (ETableSortedVariableClass *klass)
{
	ETableSubsetVariableClass *etssv_class = E_TABLE_SUBSET_VARIABLE_CLASS(klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = etsv_dispose;

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

static gboolean
etsv_sort_idle(ETableSortedVariable *etsv)
{
	g_object_ref(etsv);
	etsv_sort(etsv);
	etsv->sort_idle_id = 0;
	etsv->insert_count = 0;
	g_object_unref(etsv);
	return FALSE;
}

static gboolean
etsv_insert_idle(ETableSortedVariable *etsv)
{
	etsv->insert_count = 0;
	etsv->insert_idle_id = 0;
	return FALSE;
}

static void
etsv_add       (ETableSubsetVariable *etssv,
		gint                  row)
{
	ETableModel *etm = E_TABLE_MODEL(etssv);
	ETableSubset *etss = E_TABLE_SUBSET(etssv);
	ETableSortedVariable *etsv = E_TABLE_SORTED_VARIABLE (etssv);
	gint i;

	e_table_model_pre_change (etm);

	if (etss->n_map + 1 > etssv->n_vals_allocated) {
		etssv->n_vals_allocated += INCREMENT_AMOUNT;
		etss->map_table = g_realloc (etss->map_table, (etssv->n_vals_allocated) * sizeof(gint));
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
			i = e_table_sorting_utils_insert(etss->source, etsv->sort_info, etsv->full_header, etss->map_table, etss->n_map, row);
			memmove(etss->map_table + i + 1, etss->map_table + i, (etss->n_map - i) * sizeof(gint));
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
	gint rows;
	gint i;

	e_table_model_pre_change(etm);

	rows = e_table_model_row_count(etss->source);

	if (etss->n_map + rows > etssv->n_vals_allocated) {
		etssv->n_vals_allocated += MAX(INCREMENT_AMOUNT, rows);
		etss->map_table = g_realloc (etss->map_table, etssv->n_vals_allocated * sizeof(gint));
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
	ETableSortedVariable *etsv = g_object_new (E_TABLE_SORTED_VARIABLE_TYPE, NULL);
	ETableSubsetVariable *etssv = E_TABLE_SUBSET_VARIABLE (etsv);

	if (e_table_subset_variable_construct (etssv, source) == NULL) {
		g_object_unref (etsv);
		return NULL;
	}

	etsv->sort_info = sort_info;
	g_object_ref(etsv->sort_info);
	etsv->full_header = full_header;
	g_object_ref(etsv->full_header);

	etsv->sort_info_changed_id = g_signal_connect (G_OBJECT (sort_info), "sort_info_changed",
						       G_CALLBACK (etsv_sort_info_changed), etsv);

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
	static gint reentering = 0;
	if (reentering)
		return;
	reentering = 1;

	e_table_model_pre_change(E_TABLE_MODEL(etsv));

	e_table_sorting_utils_sort(etss->source, etsv->sort_info, etsv->full_header, etss->map_table, etss->n_map);

	e_table_model_changed (E_TABLE_MODEL(etsv));
	reentering = 0;
}
