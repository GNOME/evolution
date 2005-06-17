/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-header.c
 * Copyright 1999, 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Miguel de Icaza <miguel@ximian.com>
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

#include <string.h>

#include <glib-object.h>
#include <gtk/gtk.h>

#include "e-util/e-util.h"

#include "e-table-defines.h"
#include "e-table-header.h"

/* The arguments we take */
enum {
	PROP_0,
	PROP_SORT_INFO,
	PROP_WIDTH,
	PROP_WIDTH_EXTRAS
};

enum {
	STRUCTURE_CHANGE,
	DIMENSION_CHANGE,
	EXPANSION_CHANGE,
	REQUEST_WIDTH,
	LAST_SIGNAL
};

static void eth_set_size (ETableHeader *eth, int idx, int size);
static void eth_calc_widths (ETableHeader *eth);

static guint eth_signals [LAST_SIGNAL] = { 0, };

static GObjectClass *e_table_header_parent_class;

struct two_ints {
	int column;
	int width;
};

static void
eth_set_width (ETableHeader *eth, int width)
{
	eth->width = width;
}

static void
dequeue (ETableHeader *eth, int *column, int *width)
{
	GSList *head;
	struct two_ints *store;
	head = eth->change_queue;
	eth->change_queue = eth->change_queue->next;
	if (!eth->change_queue)
		eth->change_tail = NULL;
	store = head->data;
	g_slist_free_1(head);
	if (column)
		*column = store->column;
	if (width)
		*width = store->width;
	g_free(store);
}

static gboolean
dequeue_idle (ETableHeader *eth)
{
	int column, width;

	dequeue (eth, &column, &width);
	while (eth->change_queue && ((struct two_ints *) eth->change_queue->data)->column == column)
		dequeue (eth, &column, &width);

	if (column == -1)
		eth_set_width (eth, width);
	else if (column < eth->col_count)
		eth_set_size (eth, column, width);
	if (eth->change_queue)
		return TRUE;
	else {
		eth_calc_widths (eth);
		eth->idle = 0;
		return FALSE;
	}
}

static void
enqueue (ETableHeader *eth, int column, int width)
{
	struct two_ints *store;
	store = g_new(struct two_ints, 1);
	store->column = column;
	store->width = width;
	
	eth->change_tail = g_slist_last(g_slist_append(eth->change_tail, store));
	if (!eth->change_queue)
		eth->change_queue = eth->change_tail;

	if (!eth->idle) {
		eth->idle = g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc) dequeue_idle, eth, NULL);
	}
}

void
e_table_header_set_size (ETableHeader *eth, int idx, int size)
{
	g_return_if_fail (eth != NULL);
	g_return_if_fail (E_IS_TABLE_HEADER (eth));

	enqueue (eth, idx, size);
}

static void
eth_do_remove (ETableHeader *eth, int idx, gboolean do_unref)
{
	if (do_unref)
		g_object_unref (eth->columns [idx]);
	
	memmove (&eth->columns [idx], &eth->columns [idx+1],
		 sizeof (ETableCol *) * (eth->col_count - idx - 1));
	eth->col_count--;
}

static void
eth_finalize (GObject *object)
{
	ETableHeader *eth = E_TABLE_HEADER (object);
	const int cols = eth->col_count;
	int i;
	
	if (eth->sort_info) {
		if (eth->sort_info_group_change_id)
			g_signal_handler_disconnect(G_OBJECT(eth->sort_info),
					            eth->sort_info_group_change_id);
		g_object_unref(eth->sort_info);
		eth->sort_info = NULL;
	}

	if (eth->idle)
		g_source_remove(eth->idle);
	eth->idle = 0;

	if (eth->change_queue) {
		g_slist_foreach(eth->change_queue, (GFunc) g_free, NULL);
		g_slist_free(eth->change_queue);
		eth->change_queue = NULL;
	}
	
	/*
	 * Destroy columns
	 */
	for (i = cols - 1; i >= 0; i--){
		eth_do_remove (eth, i, TRUE);
	}
	g_free (eth->columns);

	eth->col_count = 0;
	eth->columns = NULL;

	if (e_table_header_parent_class->finalize)
		e_table_header_parent_class->finalize (object);
}

static void
eth_group_info_changed(ETableSortInfo *info, ETableHeader *eth)
{
	enqueue(eth, -1, eth->nominal_width);
}

static void
eth_set_property (GObject *object, guint prop_id, const GValue *val, GParamSpec *pspec)
{
	ETableHeader *eth = E_TABLE_HEADER (object);

	switch (prop_id) {
	case PROP_WIDTH:
		eth->nominal_width = g_value_get_double (val);
		enqueue(eth, -1, eth->nominal_width);
		break;
	case PROP_WIDTH_EXTRAS:
		eth->width_extras = g_value_get_double (val);
		enqueue(eth, -1, eth->nominal_width);
		break;
	case PROP_SORT_INFO:
		if (eth->sort_info) {
			if (eth->sort_info_group_change_id)
				g_signal_handler_disconnect(G_OBJECT(eth->sort_info), eth->sort_info_group_change_id);
			g_object_unref (eth->sort_info);
		}
		eth->sort_info = E_TABLE_SORT_INFO(g_value_get_object (val));
		if (eth->sort_info) {
			g_object_ref(eth->sort_info);
			eth->sort_info_group_change_id 
				= g_signal_connect(G_OBJECT(eth->sort_info), "group_info_changed",
						   G_CALLBACK(eth_group_info_changed), eth);
		}
		enqueue(eth, -1, eth->nominal_width);
		break;
	default:
		break;
	}
}

static void
eth_get_property (GObject *object, guint prop_id, GValue *val, GParamSpec *pspec)
{
	ETableHeader *eth = E_TABLE_HEADER (object);

	switch (prop_id) {
	case PROP_SORT_INFO:
		g_value_set_object (val, G_OBJECT(eth->sort_info));
		break;
	case PROP_WIDTH:
		g_value_set_double (val, eth->nominal_width);
		break;
	case PROP_WIDTH_EXTRAS:
		g_value_set_double (val, eth->width_extras);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_table_header_class_init (GObjectClass *object_class)
{
	ETableHeaderClass *klass = E_TABLE_HEADER_CLASS (object_class);

	object_class->finalize = eth_finalize;
	object_class->set_property = eth_set_property;
	object_class->get_property = eth_get_property;

	e_table_header_parent_class = g_type_class_peek_parent (object_class);

	g_object_class_install_property (
		object_class, PROP_WIDTH,
		g_param_spec_double ("width", "Width", "Width", 
				     0.0, G_MAXDOUBLE, 0.0, 
				     G_PARAM_READWRITE)); 

	g_object_class_install_property (
		object_class, PROP_WIDTH_EXTRAS,
		g_param_spec_double ("width_extras", "Width of Extras", "Width of Extras", 
				     0.0, G_MAXDOUBLE, 0.0, 
				     G_PARAM_READWRITE)); 

	g_object_class_install_property (
		object_class, PROP_SORT_INFO,
		g_param_spec_object ("sort_info", "Sort Info", "Sort Info", 
				     E_TABLE_SORT_INFO_TYPE,
				     G_PARAM_READWRITE)); 

	eth_signals [STRUCTURE_CHANGE] =
		g_signal_new ("structure_change",
			      E_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableHeaderClass, structure_change),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	eth_signals [DIMENSION_CHANGE] = 
		g_signal_new ("dimension_change", 
			      E_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableHeaderClass, dimension_change),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	eth_signals [EXPANSION_CHANGE] = 
		g_signal_new ("expansion_change", 
			      E_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableHeaderClass, expansion_change),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	eth_signals [REQUEST_WIDTH] = 
		g_signal_new ("request_width",
			      E_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableHeaderClass, request_width),
			      (GSignalAccumulator) NULL, NULL,
			      e_util_marshal_INT__INT,
			      G_TYPE_INT, 1, G_TYPE_INT);

	klass->structure_change = NULL;
	klass->dimension_change = NULL;
	klass->expansion_change = NULL;
	klass->request_width = NULL;
}

static void
e_table_header_init (ETableHeader *eth)
{
	eth->col_count                 = 0;
	eth->width                     = 0;

	eth->sort_info                 = NULL;
	eth->sort_info_group_change_id = 0;

	eth->columns                   = NULL;
	
	eth->change_queue              = NULL;
	eth->change_tail               = NULL;

	eth->width_extras              = 0;
}

/**
 * e_table_header_new:
 *
 * Returns: A new @ETableHeader object.
 */
ETableHeader *
e_table_header_new (void)
{

	return (ETableHeader *) g_object_new (E_TABLE_HEADER_TYPE, NULL);
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

static void
eth_do_insert (ETableHeader *eth, int pos, ETableCol *val)
{
	memmove (&eth->columns [pos+1], &eth->columns [pos],
		sizeof (ETableCol *) * (eth->col_count - pos));
	eth->columns [pos] = val;
	eth->col_count ++;
}

/**
 * e_table_header_add_column:
 * @eth: the table header to add the column to.
 * @tc: the ETableCol definition
 * @pos: position where the ETableCol will go.
 *
 * This function adds the @tc ETableCol definition into the @eth ETableHeader
 * at position @pos.  This is the way you add new ETableCols to the
 * ETableHeader.  The header will assume ownership of the @tc; you should not
 * unref it after you add it.
 *
 * This function will emit the "structure_change" signal on the @eth object.
 * The ETableCol is assumed 
 */
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
	g_object_ref (tc);
	
	eth_do_insert (eth, pos, tc);

	enqueue(eth, -1, eth->nominal_width);
	g_signal_emit (G_OBJECT (eth), eth_signals [STRUCTURE_CHANGE], 0);
}

/**
 * e_table_header_get_column:
 * @eth: the ETableHeader to query
 * @column: the column inside the @eth.
 *
 * Returns: The ETableCol at @column in the @eth object
 */
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

/**
 * e_table_header_get_column_by_col_id:
 * @eth: the ETableHeader to query
 * @col_id: the col_id to search for.
 *
 * Returns: The ETableCol with col_idx = @col_idx in the @eth object
 */
ETableCol *
e_table_header_get_column_by_col_idx (ETableHeader *eth, int col_idx)
{
	int i;
	g_return_val_if_fail (eth != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), NULL);

	for (i = 0; i < eth->col_count; i++) {
		if (eth->columns[i]->col_idx == col_idx) {
			return eth->columns [i];
		}
	}

	return NULL;
}

/**
 * e_table_header_count:
 * @eth: the ETableHeader to query
 *
 * Returns: the number of columns in this ETableHeader.
 */
int
e_table_header_count (ETableHeader *eth)
{
	g_return_val_if_fail (eth != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), 0);

	return eth->col_count;
}

/** 
 * e_table_header_index:
 * @eth: the ETableHeader to query
 * @col: the column to fetch.
 *
 * ETableHeaders contain the visual list of columns that the user will
 * view.  The visible columns will typically map to different columns
 * in the ETableModel (because the user reordered the data for
 * example).
 *
 * Returns: the column in the model that the @col column
 * in the ETableHeader points to.  */
int
e_table_header_index (ETableHeader *eth, int col)
{
	g_return_val_if_fail (eth != NULL, -1);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), -1);
	g_return_val_if_fail (col >= 0 && col < eth->col_count, -1);

	return eth->columns [col]->col_idx;
}

/**
 * e_table_header_get_index_at:
 * @eth: the ETableHeader to query
 * @x_offset: a pixel count from the beginning of the ETableHeader
 *
 * This will return the ETableHeader column that would contain
 * the @x_offset pixel.
 *
 * Returns: the column that contains pixel @x_offset, or -1
 * if no column inside this ETableHeader contains that pixel.
 */
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

/**
 * e_table_header_get_columns:
 * @eth: The ETableHeader to query
 *
 * Returns: A NULL terminated array of the ETableCols
 * contained in the ETableHeader @eth.  Note that every
 * returned ETableCol in the array has been referenced, to release
 * this information you need to g_free the buffer returned
 * and you need to g_object_unref every element returned
 */
ETableCol **
e_table_header_get_columns (ETableHeader *eth)
{
	ETableCol **ret;
	int i;
	
	g_return_val_if_fail (eth != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), 0);

	ret = g_new (ETableCol *, eth->col_count + 1);
	memcpy (ret, eth->columns, sizeof (ETableCol *) * eth->col_count);
	ret [eth->col_count] = NULL;

	for (i = 0; i < eth->col_count; i++) {
		g_object_ref(ret[i]);
	}

	return ret;
}

/**
 * e_table_header_get_selected:
 * @eth: The ETableHeader to query
 *
 * Returns: The number of selected columns in the @eth object.
 */
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

/**
 * e_table_header_total_width:
 * @eth: The ETableHeader to query
 *
 * Returns: the number of pixels used by the @eth object
 * when rendered on screen
 */
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

/**
 * e_table_header_min_width:
 * @eth: The ETableHeader to query
 *
 * Returns: the minimum number of pixels required by the @eth object.
 **/
int
e_table_header_min_width (ETableHeader *eth)
{
	int total, i;
	
	g_return_val_if_fail (eth != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), 0);

	total = 0;
	for (i = 0; i < eth->col_count; i++)
		total += eth->columns [i]->min_width;

	return total;
}

/**
 * e_table_header_move:
 * @eth: The ETableHeader to operate on.
 * @source_index: the source column to move.
 * @target_index: the target location for the column
 *
 * This function moves the column @source_index to @target_index
 * inside the @eth ETableHeader.  The signals "dimension_change"
 * and "structure_change" will be emmited
 */
void
e_table_header_move (ETableHeader *eth, int source_index, int target_index)
{
	ETableCol *old;
	
	g_return_if_fail (eth != NULL);
	g_return_if_fail (E_IS_TABLE_HEADER (eth));
	g_return_if_fail (source_index >= 0);
	g_return_if_fail (target_index >= 0);
	g_return_if_fail (source_index < eth->col_count);
	g_return_if_fail (target_index < eth->col_count + 1); /* Can be moved beyond the last item. */

	if (source_index < target_index)
		target_index --;

	old = eth->columns [source_index];
	eth_do_remove (eth, source_index, FALSE);
	eth_do_insert (eth, target_index, old);
	eth_update_offsets (eth);
	
	g_signal_emit (G_OBJECT (eth), eth_signals [DIMENSION_CHANGE], 0, eth->width);
	g_signal_emit (G_OBJECT (eth), eth_signals [STRUCTURE_CHANGE], 0);
}

/**
 * e_table_header_remove:
 * @eth: The ETableHeader to operate on.
 * @idx: the index to the column to be removed.
 *
 * Removes the column at @idx position in the ETableHeader @eth.
 * This emmits the "structure_change" signal on the @eth object.
 */
void
e_table_header_remove (ETableHeader *eth, int idx)
{
	g_return_if_fail (eth != NULL);
	g_return_if_fail (E_IS_TABLE_HEADER (eth));
	g_return_if_fail (idx >= 0);
	g_return_if_fail (idx < eth->col_count);

	eth_do_remove (eth, idx, TRUE);
	enqueue(eth, -1, eth->nominal_width);
	g_signal_emit (G_OBJECT (eth), eth_signals [STRUCTURE_CHANGE], 0);
}

/*
 * FIXME: deprecated?
 */
void
e_table_header_set_selection (ETableHeader *eth, gboolean allow_selection)
{
	g_return_if_fail (eth != NULL);
	g_return_if_fail (E_IS_TABLE_HEADER (eth));
}

static void
eth_set_size (ETableHeader *eth, int idx, int size)
{
	double expansion;
	double old_expansion;
	int min_width;
	int left_width;
	int total_extra;
	int expandable_count;
	int usable_width;
	int i;
	g_return_if_fail (eth != NULL);
	g_return_if_fail (E_IS_TABLE_HEADER (eth));
	g_return_if_fail (idx >= 0);
	g_return_if_fail (idx < eth->col_count);
	
	/* If this column is not resizable, don't do anything. */
	if (!eth->columns[idx]->resizable)
		return;

	expansion = 0;
	min_width = 0;
	left_width = 0;
	expandable_count = -1;

	/* Calculate usable area. */
	for (i = 0; i < idx; i++) {
		left_width += eth->columns[i]->width;
	}
	/* - 1 to account for the last pixel border. */
	usable_width = eth->width - left_width - 1;

	if (eth->sort_info)
		usable_width -= e_table_sort_info_grouping_get_count(eth->sort_info) * GROUP_INDENT;

	/* Calculate minimum_width of stuff on the right as well as
	 * total usable expansion on the right. 
	 */
	for (; i < eth->col_count; i++) {
		min_width += eth->columns[i]->min_width + eth->width_extras;
		if (eth->columns[i]->resizable) {
			expansion += eth->columns[i]->expansion;
			expandable_count ++;
		}
	}
	/* If there's no room for anything, don't change. */
	if (expansion == 0)
		return;

	/* (1) If none of the columns to the right are expandable, use
	 * all the expansion space in this column.
	 */
	if(expandable_count == 0) {
		eth->columns[idx]->expansion = expansion;
		for (i = idx + 1; i < eth->col_count; i++) {
			eth->columns[i]->expansion = 0;
		}

		g_signal_emit (G_OBJECT (eth), eth_signals [EXPANSION_CHANGE], 0);
		return;
	}

	total_extra = usable_width - min_width;
	/* If there's no extra space, set all expansions to 0. */
	if (total_extra <= 0) {
		for (i = idx; i < eth->col_count; i++) {
			eth->columns[i]->expansion = 0;
		}
		g_signal_emit (G_OBJECT (eth), eth_signals [EXPANSION_CHANGE], 0);
		return;
	}

	/* If you try to resize smaller than the minimum width, it
	 * uses the minimum. */
	if (size < eth->columns[idx]->min_width + eth->width_extras)
		size = eth->columns[idx]->min_width + eth->width_extras;

	/* If all the extra space will be used up in this column, use
	 * all the expansion and set all others to 0.
	 */
	if (size >= total_extra + eth->columns[idx]->min_width + eth->width_extras) {
		eth->columns[idx]->expansion = expansion;
		for (i = idx + 1; i < eth->col_count; i++) {
			eth->columns[i]->expansion = 0;
		}
		g_signal_emit (G_OBJECT (eth), eth_signals [EXPANSION_CHANGE], 0);
		return;
	}
	
	/* The old_expansion used by columns to the right. */
	old_expansion = expansion;
	old_expansion -= eth->columns[idx]->expansion;
	/* Set the new expansion so that it will generate the desired size. */
	eth->columns[idx]->expansion = expansion * (((double)(size - (eth->columns[idx]->min_width + eth->width_extras)))/((double)total_extra));
	/* The expansion left for the columns on the right. */
	expansion -= eth->columns[idx]->expansion;

	/* (2) If the old columns to the right didn't have any
	 * expansion before, expand them evenly.  old_expansion > 0 by
	 * expansion = SUM(i=idx to col_count -1,
	 * columns[i]->min_width) - columns[idx]->min_width) =
	 * SUM(non-negatives).
	 */
	if (old_expansion == 0) {
		for (i = idx + 1; i < eth->col_count; i++) {
			if (eth->columns[idx]->resizable) {
				/* expandable_count != 0 by (1) */
				eth->columns[i]->expansion = expansion / expandable_count;
			}
		}
		g_signal_emit (G_OBJECT (eth), eth_signals [EXPANSION_CHANGE], 0);
		return;
	}

	/* Remove from total_extra the amount used for this column. */
	total_extra -= size - (eth->columns[idx]->min_width + eth->width_extras);
	for (i = idx + 1; i < eth->col_count; i++) {
		if (eth->columns[idx]->resizable) {
			/* old_expansion != 0 by (2) */
			eth->columns[i]->expansion *= expansion / old_expansion;
		}
	}
	g_signal_emit (G_OBJECT (eth), eth_signals [EXPANSION_CHANGE], 0);
}

/**
 * e_table_header_col_diff:
 * @eth: the ETableHeader to query.
 * @start_col: the starting column
 * @end_col: the ending column.
 *
 * Computes the number of pixels between the columns @start_col and
 * @end_col.
 *
 * Returns: the number of pixels between @start_col and @end_col on the
 * @eth ETableHeader object
 */
int
e_table_header_col_diff (ETableHeader *eth, int start_col, int end_col)
{
	int total, col;

	g_return_val_if_fail (eth != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), 0);

	if (start_col < 0)
		start_col = 0;
	if (end_col > eth->col_count)
		end_col = eth->col_count;

	total = 0;
	for (col = start_col; col < end_col; col++){
		
		total += eth->columns [col]->width;
	}

	return total;
}

static void
eth_calc_widths (ETableHeader *eth)
{
	int i;
	int extra;
	double expansion;
	int last_position = 0;
	double next_position = 0;
	int last_resizable = -1;
	int *widths;
	gboolean changed;

	widths = g_new (int, eth->col_count);

	/* - 1 to account for the last pixel border. */
	extra = eth->width - 1;
	expansion = 0;
	for (i = 0; i < eth->col_count; i++) {
		extra -= eth->columns[i]->min_width + eth->width_extras;
		if (eth->columns[i]->resizable && eth->columns[i]->expansion > 0)
			last_resizable = i;
		expansion += eth->columns[i]->resizable ? eth->columns[i]->expansion : 0;
		widths[i] = eth->columns[i]->min_width + eth->width_extras;
	}
	if (eth->sort_info)
		extra -= e_table_sort_info_grouping_get_count(eth->sort_info) * GROUP_INDENT;
	if (expansion != 0 && extra > 0) {
		for (i = 0; i < last_resizable; i++) {
			next_position += extra * (eth->columns[i]->resizable ? eth->columns[i]->expansion : 0)/expansion;
			widths[i] += next_position - last_position;
			last_position = next_position;
		}
		widths[i] += extra - last_position;
	}

	changed = FALSE;

	for (i = 0; i < eth->col_count; i++) {
		if (eth->columns[i]->width != widths[i]) {
			changed = TRUE;
			eth->columns[i]->width = widths[i];
		}
	}
	g_free (widths);
	if (changed)
		g_signal_emit (G_OBJECT (eth), eth_signals [DIMENSION_CHANGE], 0, eth->width);
	eth_update_offsets (eth);
}

void
e_table_header_update_horizontal (ETableHeader *eth)
{
	int i;
	int cols;

	cols = eth->col_count;

	for (i = 0; i < cols; i++) {
		int width = 0;
		
		g_signal_emit_by_name (G_OBJECT (eth),
					 "request_width",
					 i, &width);
		eth->columns[i]->min_width = width + 10;
		eth->columns[i]->expansion = 1;
	}
	enqueue(eth, -1, eth->nominal_width);
	g_signal_emit (G_OBJECT (eth), eth_signals [EXPANSION_CHANGE], 0);
}

E_MAKE_TYPE(e_table_header, "ETableHeader", ETableHeader, e_table_header_class_init, e_table_header_init, G_TYPE_OBJECT)

int
e_table_header_prioritized_column (ETableHeader *eth)
{
	int best_model_col = 0;
	int best_priority;
	int i;
	int count;

	count = e_table_header_count (eth);
	if (count == 0)
		return -1;
	best_priority = e_table_header_get_column (eth, 0)->priority;
	best_model_col = e_table_header_get_column (eth, 0)->col_idx;
	for (i = 1; i < count; i++) {
		int priority = e_table_header_get_column (eth, i)->priority;
		if (priority > best_priority) {
			best_priority = priority;
			best_model_col = e_table_header_get_column (eth, i)->col_idx;
		}
	}
	return best_model_col;
}

ETableCol *
e_table_header_prioritized_column_selected (ETableHeader *eth, ETableColCheckFunc check_func, gpointer user_data)
{
	ETableCol *best_col = NULL;
	int best_priority = G_MININT;
	int i;
	int count;

	count = e_table_header_count (eth);
	if (count == 0)
		return NULL;
	for (i = 1; i < count; i++) {
		ETableCol *col = e_table_header_get_column (eth, i);
		if (col) {
			if ((best_col == NULL || col->priority > best_priority) && check_func (col, user_data)) {
				best_priority = col->priority;
				best_col = col;
			}
		}
	}
	return best_col;
}
