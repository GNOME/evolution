/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Bolian Yin <bolian.yin@sun.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

/* EaCellTable */

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

const gchar *
ea_cell_table_get_column_label (EaCellTable * cell_data, gint column);
void ea_cell_table_set_column_label (EaCellTable * cell_data,
				     gint column, const gchar *label);
const gchar *
ea_cell_table_get_row_label (EaCellTable * cell_data, gint row);
void ea_cell_table_set_row_label (EaCellTable * cell_data,
				  gint row, const gchar *label);
gint ea_cell_table_get_index (EaCellTable *cell_data,
			      gint row, gint column);
