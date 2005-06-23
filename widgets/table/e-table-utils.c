/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-utils.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <libintl.h>		/* This file uses dgettext() but no _() */

#include "e-util/e-util.h"
#include "misc/e-unicode.h"

#include "e-table-utils.h"
#include "e-table-header-utils.h"

ETableHeader *
e_table_state_to_header (GtkWidget *widget, ETableHeader *full_header, ETableState *state)
{
	ETableHeader *nh;
	const int max_cols = e_table_header_count (full_header);
	int column;
	GValue *val = g_new0 (GValue, 1);

	g_return_val_if_fail (widget, NULL);
	g_return_val_if_fail (full_header, NULL);
	g_return_val_if_fail (state, NULL);

	nh = e_table_header_new ();
	g_value_init (val, G_TYPE_DOUBLE);
	g_value_set_double (val, e_table_header_width_extras (widget->style));
	g_object_set_property (G_OBJECT(nh), "width_extras", val);
	g_free (val);

	for (column = 0; column < state->col_count; column++) {
		int col;
		double expansion;
		ETableCol *table_col;

		col = state->columns[column];
		expansion = state->expansions[column];

		if (col >= max_cols)
			continue;

		table_col = e_table_header_get_column (full_header, col);

		if (expansion >= -1)
			table_col->expansion = expansion;

		e_table_header_add_column (nh, table_col, -1);
	}

	return nh;
}

static ETableCol *
et_col_spec_to_col (ETableColumnSpecification *col_spec,
		    ETableExtras              *ete,
		    const char                *domain)
{
	ETableCol *col = NULL;
	ECell *cell = NULL;
	GCompareFunc compare = NULL;
	ETableSearchFunc search = NULL;

	if (col_spec->cell)
		cell = e_table_extras_get_cell(ete, col_spec->cell);
	if (col_spec->compare)
		compare = e_table_extras_get_compare(ete, col_spec->compare);
	if (col_spec->search)
		search = e_table_extras_get_search(ete, col_spec->search);

	if (cell && compare) {
		char *title = dgettext (domain, col_spec->title);

		title = g_strdup (title);

		if (col_spec->pixbuf && *col_spec->pixbuf) {
			GdkPixbuf *pixbuf;

			pixbuf = e_table_extras_get_pixbuf(
				ete, col_spec->pixbuf);
			if (pixbuf) {
				col = e_table_col_new_with_pixbuf (
					col_spec->model_col, title,
					pixbuf, col_spec->expansion,
					col_spec->minimum_width,
					cell, compare, col_spec->resizable, col_spec->disabled, col_spec->priority);
			}
		}
		if (col == NULL && col_spec->title && *col_spec->title) {
			col = e_table_col_new (
				col_spec->model_col, title,
				col_spec->expansion, col_spec->minimum_width,
				cell, compare, col_spec->resizable, col_spec->disabled, col_spec->priority);
		}
		col->search = search;

		g_free (title);
	}
	if (col && col_spec->compare_col != col_spec->model_col)
		g_object_set (col,
			      "compare_col", col_spec->compare_col,
			      NULL);
	return col;
}

ETableHeader *
e_table_spec_to_full_header (ETableSpecification *spec,
			     ETableExtras        *ete)
{
	ETableHeader *nh;
	int column;

	g_return_val_if_fail (spec, NULL);
	g_return_val_if_fail (ete, NULL);

	nh = e_table_header_new ();

	for (column = 0; spec->columns[column]; column++) {
		ETableCol *col = et_col_spec_to_col (
			spec->columns[column], ete, spec->domain);

		if (col) {
			e_table_header_add_column (nh, col, -1);
			g_object_unref (col);
		}
	}

	return nh;
}

static gboolean
check_col (ETableCol *col, gpointer user_data)
{
	return col->search ? TRUE : FALSE;
}

ETableCol *
e_table_util_calculate_current_search_col (ETableHeader *header, ETableHeader *full_header, ETableSortInfo *sort_info, gboolean always_search)
{
	int i;
	int count;
	ETableCol *col = NULL;
	count = e_table_sort_info_grouping_get_count (sort_info);
	for (i = 0; i < count; i++) {
		ETableSortColumn column = e_table_sort_info_grouping_get_nth(sort_info, i);

		col = e_table_header_get_column (full_header, column.column);

		if (col && col->search)
			break;

		col = NULL;
	}

	if (col == NULL) {
		count = e_table_sort_info_sorting_get_count (sort_info);
		for (i = 0; i < count; i++) {
			ETableSortColumn column = e_table_sort_info_sorting_get_nth(sort_info, i);

			col = e_table_header_get_column (full_header, column.column);

			if (col && col->search)
				break;

			col = NULL;
		}
	}

	if (col == NULL && always_search) {
		col = e_table_header_prioritized_column_selected (header, check_col, NULL);
	}

	return col;
}
