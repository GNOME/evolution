/*
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
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "ea-cell-table.h"

EaCellTable *
ea_cell_table_create (gint rows,
                      gint columns,
                      gboolean column_first)
{
	EaCellTable *cell_data;
	gint index;

	g_return_val_if_fail (((columns > 0) && (rows > 0)), NULL);

	cell_data = g_new0 (EaCellTable, 1);

	cell_data->column_first = column_first;
	cell_data->columns = columns;
	cell_data->rows = rows;

	cell_data->column_labels = g_new0 (gchar *, columns);
	for (index = columns -1; index >= 0; --index)
		cell_data->column_labels[index] = NULL;

	cell_data->row_labels = g_new0 (gchar *, rows);
	for (index = rows -1; index >= 0; --index)
		cell_data->row_labels[index] = NULL;

	cell_data->cells = g_new0 (gpointer, (columns * rows));
	for (index = (columns * rows) -1; index >= 0; --index)
		cell_data->cells[index] = NULL;
	return cell_data;
}

void
ea_cell_table_destroy (EaCellTable *cell_data)
{
	gint index;
	g_return_if_fail (cell_data);

	for (index = 0; index < cell_data->columns; ++index)
		g_free (cell_data->column_labels[index]);

	g_free (cell_data->column_labels);

	for (index = 0; index < cell_data->rows; ++index)
		g_free (cell_data->row_labels[index]);

	g_free (cell_data->row_labels);

	for (index = (cell_data->columns * cell_data->rows) -1;
	     index >= 0; --index)
		if (cell_data->cells[index] &&
		    G_IS_OBJECT (cell_data->cells[index]))
			g_object_unref (cell_data->cells[index]);

	g_free (cell_data->cells);
	g_free (cell_data);
}

gpointer
ea_cell_table_get_cell (EaCellTable *cell_data,
                        gint row,
                        gint column)
{
	gint index;

	g_return_val_if_fail (cell_data, NULL);

	index = ea_cell_table_get_index (cell_data, column, row);
	if (index == -1)
		return NULL;

	return cell_data->cells[index];
}

gboolean
ea_cell_table_set_cell (EaCellTable *cell_data,
                        gint row,
                        gint column,
                        gpointer cell)
{
	gint index;

	g_return_val_if_fail (cell_data, FALSE);

	index = ea_cell_table_get_index (cell_data, column, row);
	if (index == -1)
		return FALSE;

	if (cell && G_IS_OBJECT (cell))
		g_object_ref (cell);
	if (cell_data->cells[index] &&
	    G_IS_OBJECT (cell_data->cells[index]))
		g_object_unref (cell_data->cells[index]);
	cell_data->cells[index] = cell;

	return TRUE;
}

gpointer
ea_cell_table_get_cell_at_index (EaCellTable *cell_data,
                                 gint index)
{
	g_return_val_if_fail (cell_data, NULL);

	if (index >=0 && index < (cell_data->columns * cell_data->rows))
		return cell_data->cells[index];
	return NULL;
}

gboolean
ea_cell_table_set_cell_at_index (EaCellTable *cell_data,
                                 gint index,
                                 gpointer cell)
{
	g_return_val_if_fail (cell_data, FALSE);

	if (index < 0 || index >=cell_data->columns * cell_data->rows)
		return FALSE;

	if (cell && G_IS_OBJECT (cell))
		g_object_ref (cell);
	if (cell_data->cells[index] &&
	    G_IS_OBJECT (cell_data->cells[index]))
		g_object_unref (cell_data->cells[index]);
	cell_data->cells[index] = cell;

	return TRUE;
}

const gchar *
ea_cell_table_get_column_label (EaCellTable *cell_data,
                                gint column)
{
	g_return_val_if_fail (cell_data, NULL);
	g_return_val_if_fail ((column >= 0 && column < cell_data->columns), NULL);

	return cell_data->column_labels[column];
}

void
ea_cell_table_set_column_label (EaCellTable *cell_data,
                                gint column,
                                const gchar *label)
{
	g_return_if_fail (cell_data);
	g_return_if_fail ((column >= 0 && column < cell_data->columns));

	g_free (cell_data->column_labels[column]);
	cell_data->column_labels[column] = g_strdup (label);
}

const gchar *
ea_cell_table_get_row_label (EaCellTable *cell_data,
                             gint row)
{
	g_return_val_if_fail (cell_data, NULL);
	g_return_val_if_fail ((row >= 0 && row < cell_data->rows), NULL);

	return cell_data->row_labels[row];
}

void
ea_cell_table_set_row_label (EaCellTable *cell_data,
                             gint row,
                             const gchar *label)
{
	g_return_if_fail (cell_data);
	g_return_if_fail ((row >= 0 && row < cell_data->rows));

	g_free (cell_data->row_labels[row]);
	cell_data->row_labels[row] = g_strdup (label);
}

gint
ea_cell_table_get_index (EaCellTable *cell_data,
                         gint row,
                         gint column)
{
	g_return_val_if_fail (cell_data, -1);
	if (row < 0 || row >= cell_data->rows ||
	    column < 0 || column >= cell_data->columns)
		return -1;

	if (cell_data->column_first)
		return column * cell_data->rows + row;
	else
		return row * cell_data->columns + column;
}
