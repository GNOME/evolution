/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-table-cell.c
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Bolian Yin <bolian.yin@sun.com> Sun Microsystem Inc., 2003
 *
 */

/* EaCellTable */

#include <glib.h>
#include <glib-object.h>

struct _EaCellTable {
	gint columns;
	gint rows;
	gboolean column_first;     /* index order */
	gchar **column_labels;
	gchar **row_labels;
	gpointer *cells;
};

typedef struct _EaCellTable EaCellTable;

EaCellTable * ea_cell_table_create (gint rows, gint columns,
				    gboolean column_first);
void ea_cell_table_destroy (EaCellTable * cell_data);
gpointer ea_cell_table_get_cell (EaCellTable * cell_data,
				   gint row, gint column);
gboolean ea_cell_table_set_cell (EaCellTable * cell_data,
				 gint row, gint column, gpointer cell);
gpointer ea_cell_table_get_cell_at_index (EaCellTable * cell_data,
					  gint index);
gboolean ea_cell_table_set_cell_at_index (EaCellTable * cell_data,
					  gint index, gpointer cell);

G_CONST_RETURN gchar*
ea_cell_table_get_column_label (EaCellTable * cell_data, gint column);
void ea_cell_table_set_column_label (EaCellTable * cell_data,
				     gint column, const gchar *label);
G_CONST_RETURN gchar*
ea_cell_table_get_row_label (EaCellTable * cell_data, gint row);
void ea_cell_table_set_row_label (EaCellTable * cell_data,
				  gint row, const gchar *label);
gint ea_cell_table_get_index (EaCellTable *cell_data,
			      gint row, gint column);
