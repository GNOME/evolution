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
#include "gal/util/e-i18n.h"
#include "gal/util/e-util.h"
#include "e-table-utils.h"
#include "e-table-header-utils.h"

ETableHeader *
e_table_state_to_header (GtkWidget *widget, ETableHeader *full_header, ETableState *state)
{
	ETableHeader *nh;
	const int max_cols = e_table_header_count (full_header);
	int column;

	g_return_val_if_fail (widget, NULL);
	g_return_val_if_fail (full_header, NULL);
	g_return_val_if_fail (state, NULL);

	nh = e_table_header_new ();

	gtk_object_set(GTK_OBJECT(nh),
		       "width_extras", e_table_header_width_extras(widget->style),
		       NULL);

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
		    ETableExtras              *ete)
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
		if (col_spec->pixbuf && *col_spec->pixbuf) {
			GdkPixbuf *pixbuf;

			pixbuf = e_table_extras_get_pixbuf(
				ete, col_spec->pixbuf);
			if (pixbuf) {
				col = e_table_col_new_with_pixbuf (
					col_spec->model_col, gettext (col_spec->title),
					pixbuf, col_spec->expansion,
					col_spec->minimum_width,
					cell, compare, col_spec->resizable, col_spec->disabled, col_spec->priority);
			}
		}
		if (col == NULL && col_spec->title && *col_spec->title) {
			col = e_table_col_new (
				col_spec->model_col, gettext (col_spec->title),
				col_spec->expansion, col_spec->minimum_width,
				cell, compare, col_spec->resizable, col_spec->disabled, col_spec->priority);
		}
		col->search = search;
	}
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
			spec->columns[column], ete);

		if (col)
			e_table_header_add_column (nh, col, -1);
	}

	return nh;
}
