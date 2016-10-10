/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-table-utils.h"

#include <libintl.h>		/* This file uses dgettext() but no _() */
#include <string.h>

#include "e-table-header-utils.h"
#include "e-unicode.h"

ETableHeader *
e_table_state_to_header (GtkWidget *widget,
                         ETableHeader *full_header,
                         ETableState *state)
{
	ETableHeader *nh;
	gint ii;
	GValue *val = g_new0 (GValue, 1);

	g_return_val_if_fail (widget, NULL);
	g_return_val_if_fail (full_header, NULL);
	g_return_val_if_fail (state, NULL);

	nh = e_table_header_new ();
	g_value_init (val, G_TYPE_DOUBLE);
	g_value_set_double (val, e_table_header_width_extras (widget));
	g_object_set_property (G_OBJECT (nh), "width_extras", val);
	g_free (val);

	for (ii = 0; ii < state->col_count; ii++) {
		ETableCol *table_col;

		table_col = e_table_header_get_column_by_spec (
			full_header, state->column_specs[ii]);

		if (table_col == NULL)
			continue;

		if (state->expansions[ii] >= -1)
			table_col->expansion = state->expansions[ii];

		e_table_header_add_column (nh, table_col, -1);
	}

	return nh;
}

static ETableCol *
et_col_spec_to_col (ETableColumnSpecification *col_spec,
                    ETableExtras *ete,
                    const gchar *domain)
{
	ETableCol *col = NULL;
	ECell *cell = NULL;
	GCompareDataFunc compare = NULL;
	ETableSearchFunc search = NULL;

	if (col_spec->cell)
		cell = e_table_extras_get_cell (ete, col_spec->cell);
	if (col_spec->compare)
		compare = e_table_extras_get_compare (ete, col_spec->compare);
	if (col_spec->search)
		search = e_table_extras_get_search (ete, col_spec->search);

	if (cell && compare) {
		gchar *title = dgettext (domain, col_spec->title);

		title = g_strdup (title);

		if (col_spec->pixbuf && *col_spec->pixbuf) {
			const gchar *icon_name;

			icon_name = e_table_extras_get_icon_name (
				ete, col_spec->pixbuf);
			if (icon_name != NULL) {
				col = e_table_col_new (
					col_spec,
					title, icon_name,
					cell, compare);
			}
		}

		if (col == NULL && col_spec->title && *col_spec->title) {
			col = e_table_col_new (
				col_spec,
				title, NULL,
				cell, compare);
		}

		if (col != NULL)
			col->search = search;

		g_free (title);
	}

	return col;
}

ETableHeader *
e_table_spec_to_full_header (ETableSpecification *spec,
                             ETableExtras *ete)
{
	ETableHeader *nh;
	GPtrArray *columns;
	guint ii;

	g_return_val_if_fail (spec, NULL);
	g_return_val_if_fail (ete, NULL);

	nh = e_table_header_new ();

	columns = e_table_specification_ref_columns (spec);

	for (ii = 0; ii < columns->len; ii++) {
		ETableColumnSpecification *col_spec;
		ETableCol *col;

		col_spec = g_ptr_array_index (columns, ii);
		col = et_col_spec_to_col (col_spec, ete, spec->domain);

		if (col != NULL) {
			e_table_header_add_column (nh, col, -1);
			g_object_unref (col);
		}
	}

	g_ptr_array_unref (columns);

	return nh;
}

static gboolean
check_col (ETableCol *col,
           gpointer user_data)
{
	return col->search ? TRUE : FALSE;
}

ETableCol *
e_table_util_calculate_current_search_col (ETableHeader *header,
                                           ETableHeader *full_header,
                                           ETableSortInfo *sort_info,
                                           gboolean always_search)
{
	gint i;
	gint count;
	ETableCol *col = NULL;

	count = e_table_sort_info_grouping_get_count (sort_info);

	for (i = 0; i < count; i++) {
		ETableColumnSpecification *spec;

		spec = e_table_sort_info_grouping_get_nth (sort_info, i, NULL);
		col = e_table_header_get_column_by_spec (full_header, spec);

		if (col != NULL && col->search)
			return col;

		col = NULL;
	}

	count = e_table_sort_info_sorting_get_count (sort_info);
	for (i = 0; i < count; i++) {
		ETableColumnSpecification *spec;

		spec = e_table_sort_info_sorting_get_nth (sort_info, i, NULL);
		col = e_table_header_get_column_by_spec (full_header, spec);

		if (col != NULL && col->search)
			return col;

		col = NULL;
	}

	if (always_search)
		col = e_table_header_prioritized_column_selected (
			header, check_col, NULL);

	return col;
}
