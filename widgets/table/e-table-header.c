/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-col-head.c: TableColHead implementation
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Helix Code, Inc
 */
#include <config.h>
#include <string.h>
#include <gtk/gtkobject.h>
#include <gtk/gtksignal.h>
#include "e-table-header.h"

enum {
	STRUCTURE_CHANGE,
	DIMENSION_CHANGE,
	LAST_SIGNAL
};

static guint eth_signals [LAST_SIGNAL] = { 0, };

static GtkObjectClass *e_table_header_parent_class;

static void
e_table_header_destroy (GtkObject *object)
{
	ETableHeader *eth = E_TABLE_HEADER (object);
	const int cols = eth->col_count;
	int i;
	
	/*
	 * Destroy columns
	 */
	for (i = cols - 1; i >= 0; i--){
		e_table_header_remove (eth, i);
	}
	
	if (e_table_header_parent_class->destroy)
		e_table_header_parent_class->destroy (object);
}

static void
e_table_header_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = e_table_header_destroy;

	e_table_header_parent_class = (gtk_type_class (gtk_object_get_type ()));

	eth_signals [STRUCTURE_CHANGE] =
		gtk_signal_new ("structure_change",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableHeaderClass, structure_change),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	eth_signals [DIMENSION_CHANGE] = 
		gtk_signal_new ("dimension_change", 
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableHeaderClass, dimension_change),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	gtk_object_class_add_signals (object_class, eth_signals, LAST_SIGNAL);
}

GtkType
e_table_header_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ETableHeader",
			sizeof (ETableHeader),
			sizeof (ETableHeaderClass),
			(GtkClassInitFunc) e_table_header_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gtk_object_get_type (), &info);
	}

	return type;
}

ETableHeader *
e_table_header_new (void)
{
	ETableHeader *eth;

	eth = gtk_type_new (e_table_header_get_type ());
	eth->frozen_count = 0;

	return eth;
}

static void
eth_do_insert (ETableHeader *eth, int pos, ETableCol *val)
{
	memmove (&eth->columns [pos+1], &eth->columns [pos],
		sizeof (ETableCol *) * (eth->col_count - pos));
	eth->columns [pos] = val;
	eth->col_count ++;
}

static void
eth_update_offsets (ETableHeader *eth)
{
	int i;
	int x = 0;
	
	for (i = 0; i < eth->col_count; i++){
		ETableCol *etc = eth->columns [i];

		etc->x = x;
		x += etc->width;
	}
}

void
e_table_header_add_column (ETableHeader *eth, ETableCol *tc, int pos)
{
	g_return_if_fail (eth != NULL);
	g_return_if_fail (E_IS_TABLE_HEADER (eth));
	g_return_if_fail (tc != NULL);
	g_return_if_fail (E_IS_TABLE_COL (tc));
	g_return_if_fail (pos >= -1 && pos <= eth->col_count);

	if (pos == -1)
		pos = eth->col_count;
	eth->columns = g_realloc (eth->columns, sizeof (ETableCol *) * (eth->col_count + 1));

	/*
	 * We are the primary owners of the column
	 */
	gtk_object_ref (GTK_OBJECT (tc));
	gtk_object_sink (GTK_OBJECT (tc));
	
	eth_do_insert (eth, pos, tc);
	eth_update_offsets (eth);

	gtk_signal_emit (GTK_OBJECT (eth), eth_signals [STRUCTURE_CHANGE]);
	gtk_signal_emit (GTK_OBJECT (eth), eth_signals [DIMENSION_CHANGE]);
}

ETableCol *
e_table_header_get_column (ETableHeader *eth, int column)
{
	g_return_val_if_fail (eth != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), NULL);

	if (column < 0)
		return NULL;

	if (column >= eth->col_count)
		return NULL;

	return eth->columns [column];
}

int
e_table_header_count (ETableHeader *eth)
{
	g_return_val_if_fail (eth != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), 0);

	return eth->col_count;
}

int
e_table_header_index (ETableHeader *eth, int col)
{
	g_return_val_if_fail (eth != NULL, -1);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), -1);
	g_return_val_if_fail (col < eth->col_count, -1);

	return eth->columns [col]->col_idx;
}

int
e_table_header_get_index_at (ETableHeader *eth, int x_offset)
{
	int i, total;
	
	g_return_val_if_fail (eth != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), 0);

	total = 0;
	for (i = 0; i < eth->col_count; i++){
		total += eth->columns [i]->width;

		if (x_offset < total)
			return i;
	}

	return -1;
}

ETableCol **
e_table_header_get_columns (ETableHeader *eth)
{
	ETableCol **ret;
	
	g_return_val_if_fail (eth != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), 0);

	ret = g_new (ETableCol *, eth->col_count + 1);
	memcpy (ret, eth->columns, sizeof (ETableCol *) * eth->col_count);
	ret [eth->col_count] = NULL;

	return ret;
}

gboolean
e_table_header_selection_ok (ETableHeader *eth)
{
	g_return_val_if_fail (eth != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), FALSE);

	return eth->selectable;
}

int
e_table_header_get_selected (ETableHeader *eth)
{
	int i;
	int selected = 0;
	
	g_return_val_if_fail (eth != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), 0);

	for (i = 0; i < eth->col_count; i++){
		if (eth->columns [i]->selected)
			selected++;
	}

	return selected;
}

int
e_table_header_total_width (ETableHeader *eth)
{
	int total, i;
	
	g_return_val_if_fail (eth != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), 0);

	total = 0;
	for (i = 0; i < eth->col_count; i++)
		total += eth->columns [i]->width;

	return total;
}

static void
eth_do_remove (ETableHeader *eth, int idx, gboolean do_unref)
{
	if (do_unref)
		gtk_object_unref (GTK_OBJECT (eth->columns [idx]));
	
	memmove (&eth->columns [idx], &eth->columns [idx+1],
		sizeof (ETableCol *) * eth->col_count - idx);
	eth->col_count--;
}

void
e_table_header_move (ETableHeader *eth, int source_index, int target_index)
{
	ETableCol *old;
	
	g_return_if_fail (eth != NULL);
	g_return_if_fail (E_IS_TABLE_HEADER (eth));
	g_return_if_fail (source_index >= 0);
	g_return_if_fail (target_index >= 0);
	g_return_if_fail (source_index < eth->col_count);
	g_return_if_fail (target_index < eth->col_count);

	old = eth->columns [source_index];
	eth_do_remove (eth, source_index, FALSE);
	eth_do_insert (eth, target_index, old);
	eth_update_offsets (eth);
	
	gtk_signal_emit (GTK_OBJECT (eth), eth_signals [STRUCTURE_CHANGE]);
	gtk_signal_emit (GTK_OBJECT (eth), eth_signals [DIMENSION_CHANGE]);
}

void
e_table_header_remove (ETableHeader *eth, int idx)
{
	g_return_if_fail (eth != NULL);
	g_return_if_fail (E_IS_TABLE_HEADER (eth));
	g_return_if_fail (idx >= 0);
	g_return_if_fail (idx < eth->col_count);

	eth_do_remove (eth, idx, TRUE);
	gtk_signal_emit (GTK_OBJECT (eth), eth_signals [STRUCTURE_CHANGE]);
	gtk_signal_emit (GTK_OBJECT (eth), eth_signals [DIMENSION_CHANGE]);
}

void
e_table_header_set_selection (ETableHeader *eth, gboolean allow_selection)
{
}

void
e_table_header_set_size (ETableHeader *eth, int idx, int size)
{
	g_return_if_fail (eth != NULL);
	g_return_if_fail (E_IS_TABLE_HEADER (eth));
	g_return_if_fail (idx >= 0);
	g_return_if_fail (idx < eth->col_count);
	g_return_if_fail (size > 0);

	eth->columns [idx]->width = size;
	gtk_signal_emit (GTK_OBJECT (eth), eth_signals [DIMENSION_CHANGE], idx);
}

int
e_table_header_col_diff (ETableHeader *eth, int start_col, int end_col)
{
	int total, col;
	
	g_return_val_if_fail (eth != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), 0);

	{
		if ( start_col < 0 )
			start_col = 0;
		if ( end_col > eth->col_count )
			end_col = eth->col_count - 1;
		
		total = 0;
		for (col = start_col; col < end_col; col++){
			
			total += eth->columns [col]->width;
		}
	}

	return total;
}

void
e_table_header_set_frozen_columns (ETableHeader *eth, int idx)
{
	eth->frozen_count = idx;
}
