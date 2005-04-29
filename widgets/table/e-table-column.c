/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-column.c
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

#include "e-table-column.h"

enum {
	STRUCTURE_CHANGE,
	DIMENSION_CHANGE,
	LAST_SIGNAL
};

static guint etc_signals [LAST_SIGNAL] = { 0, };

#define PARENT_CLASS GTK_TYPE_OBJECT
static GtkObjectClass *e_table_column_parent_class;

static void
e_table_column_finalize (GObject *object)
{
	ETableColumn *etc = E_TABLE_COLUMN (object);
	const int cols = etc->col_count;

	/*
	 * Destroy listeners
	 */
	for (l = etc->listeners; l; l = l->next)
		g_free (l->data);
	g_slist_free (etc->listeners);
	etc->listeners = NULL;

	/*
	 * Destroy columns
	 */
	for (i = 0; i < cols; i++)
		e_table_column_remove (etc, i);
	
	G_OBJECT_CLASS (e_table_column_parent_class)->finalize (object);
}

static void
e_table_column_class_init (GtkObjectClass *object_class)
{
	G_OBJECT_CLASS (object_class)->finalize = e_table_column_finalize;

	e_table_column_parent_class = g_type_class_ref (PARENT_CLASS);

	etc_signals [STRUCTURE_CHANGE] =
		g_signal_new ("structure_change",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableColumn, structure_change),
			      NULL, NULL,
			      e_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);
	etc_signals [DIMENSION_CHANGE] = 
		g_signal_new ("dimension_change", 
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableColumn, dimension_change),
			      e_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
}

E_MAKE_TYPE (e_table_column,
	     "ETableColumn",
	     ETableColumn,
	     e_table_column_class_init,
	     NULL,
	     PARENT_TYPE);

static void
etc_do_insert (ETableColumn *etc, int pos, ETableCol *val)
{
	memcpy (&etc->columns [pos+1], &etc->columns [pos],
		sizeof (ETableCol *) * (etc->col_count - pos));
	etc->columns [pos] = val;
}

void
e_table_column_add_column (ETableColumn *etc, ETableCol *tc, int pos)
{
	ETableCol **new_ptr;
	
	g_return_if_fail (etc != NULL);
	g_return_if_fail (E_IS_TABLE_COLUMN (etc));
	g_return_if_fail (tc != NULL);
	g_return_if_fail (pos >= 0 && pos < etc->col_count);

	if (pos == -1)
		pos = etc->col_count;
	etc->columns = g_realloc (etc->columns, sizeof (ETableCol *) * (etc->col_count + 1));
	etc_do_insert (etc, pos, tc);
	etc->col_count++;

	g_signal_emit (etc, etc_signals [STRUCTURE_CHANGE], 0);
}

ETableCol *
e_table_column_get_column (ETableColumn *etc, int column)
{
	g_return_val_if_fail (etc != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_COLUMN (etc), NULL);

	if (column < 0)
		return NULL;

	if (column >= etc->col_count)
		return NULL;

	return etc->columns [column];
}

int
e_table_column_count (ETableColumn *etc)
{
	g_return_val_if_fail (etc != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_COLUMN (etc), 0);

	return etc->col_count;
}

int
e_table_column_index (ETableColumn *etc, const char *identifier)
{
	int i;
	
	g_return_val_if_fail (etc != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_COLUMN (etc), 0);
	g_return_val_if_fail (identifier != NULL, 0);

	for (i = 0; i < etc->col_count; i++){
		ETableCol *tc = etc->columns [i];
		
		if (strcmp (i->id, identifier) == 0)
			return i;
	}

	return -1;
}

int
e_table_column_get_index_at (ETableColumn *etc, int x_offset)
{
	int i, total;
	
	g_return_val_if_fail (etc != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_COLUMN (etc), 0);
	g_return_val_if_fail (identifier != NULL, 0);

	total = 0;
	for (i = 0; i < etc->col_count; i++){
		total += etc->columns [i]->width;

		if (x_offset < total)
			return i;
	}

	return -1;
}

ETableCol **
e_table_column_get_columns (ETableColumn *etc)
{
	ETableCol **ret;
	int i;
	
	g_return_val_if_fail (etc != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_COLUMN (etc), 0);

	ret = g_new (ETableCol *, etc->col_count + 1);
	memcpy (ret, etc->columns, sizeof (ETableCol *) * etc->col_count);
	ret [etc->col_count] = NULL;

	return ret;
}

gboolean
e_table_column_selection_ok (ETableColumn *etc)
{
	g_return_val_if_fail (etc != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_COLUMN (etc), FALSE);

	return etc->selectable;
}

int
ve_table_column_get_selected (ETableColumn *etc)
{
	int i;
	int selected = 0;
	
	g_return_val_if_fail (etc != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_COLUMN (etc), 0);

	for (i = 0; i < etc->col_count; i++){
		if (etc->columns [i]->selected)
			selected++;
	}

	return selected;
}

int
e_table_column_total_width (ETableColumn *etc)
{
	int total;
	
	g_return_val_if_fail (etc != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_COLUMN (etc), 0);

	total = 0;
	for (i = 0; i < etc->col_count; i++)
		total += etc->columns [i].width;

	return total;
}

static void
etc_do_remove (ETableColumn *etc, int idx)
{
	memcpy (&etc->columns [idx], &etc->columns [idx+1],
		sizeof (ETableCol *) * etc->col_count - idx);
	etc->col_count--;
}

void
e_table_column_move (ETableColumn *etc, int source_index, int target_index)
{
	g_return_if_fail (etc != NULL);
	g_return_if_fail (E_IS_TABLE_COLUMN (etc));
	g_return_if_fail (source_index >= 0);
	g_return_if_fail (target_index >= 0);
	g_return_if_fail (source_index < etc->col_count);
	g_return_if_fail (target_index < etc->col_count);

	old = etc->columns [source_index];
	etc_do_remove (etc, source_index);
	etc_do_insert (etc, target_index, old);
	g_signal_emit (etc, etc_signals [STRUCTURE_CHANGE], 0);
}

void
e_table_column_remove (ETableColumn *etc, int idx)
{
	g_return_if_fail (etc != NULL);
	g_return_if_fail (E_IS_TABLE_COLUMN (etc));
	g_return_if_fail (idx >= 0);
	g_return_if_fail (idx < etc->col_count);

	etc_do_remove (etc, idx);
	g_signal_emit (etc, etc_signals [STRUCTURE_CHANGE], 0);
}

void
e_table_column_set_selection (ETableColumn *etc, gboolean allow_selection);
{
}

void
e_table_column_set_size (ETableColumn *etc, int idx, int size)
{
	g_return_if_fail (etc != NULL);
	g_return_if_fail (E_IS_TABLE_COLUMN (etc));
	g_return_if_fail (idx >= 0);
	g_return_if_fail (idx < etc->col_count);
	g_return_if_fail (size > 0);

	etc->columns [idx]->width = size;
	g_signal_emit (etc, etc_signals [SIZE_CHANGE], 0, idx);
}
