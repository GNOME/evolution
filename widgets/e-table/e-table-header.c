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
#include "e-table-defines.h"

/* The arguments we take */
enum {
	ARG_0,
	ARG_SORT_INFO,
	ARG_WIDTH,
};

enum {
	STRUCTURE_CHANGE,
	DIMENSION_CHANGE,
	LAST_SIGNAL
};

static void eth_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void eth_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void eth_do_remove (ETableHeader *eth, int idx, gboolean do_unref);
static void eth_set_width(ETableHeader *eth, int width);
static void eth_set_size (ETableHeader *eth, int idx, int size);
static void eth_calc_widths (ETableHeader *eth);
static void dequeue(ETableHeader *eth, int *column, int *width);

static guint eth_signals [LAST_SIGNAL] = { 0, };

static GtkObjectClass *e_table_header_parent_class;

struct two_ints {
	int column;
	int width;
};

static gboolean
dequeue_idle(ETableHeader *eth)
{
	int column, width;
	dequeue(eth, &column, &width);
	while(eth->change_queue && ((struct two_ints *)eth->change_queue->data)->column == column)
		dequeue(eth, &column, &width);
	if (column == -1)
		eth_set_width(eth, width);
	else if (column < eth->col_count)
		eth_set_size(eth, column, width);
	if (eth->change_queue)
		return TRUE;
	else {
		eth_calc_widths(eth);
		eth->idle = 0;
		return FALSE;
	}
}

static void
enqueue(ETableHeader *eth, int column, int width)
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

static void
dequeue(ETableHeader *eth, int *column, int *width)
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

static void
eth_destroy (GtkObject *object)
{
	ETableHeader *eth = E_TABLE_HEADER (object);
	const int cols = eth->col_count;
	int i;
	
	if (eth->sort_info) {
		if (eth->sort_info_group_change_id)
			gtk_signal_disconnect(GTK_OBJECT(eth->sort_info),
					      eth->sort_info_group_change_id);
		gtk_object_unref(GTK_OBJECT(eth->sort_info));
	}

	g_slist_foreach(eth->change_queue, (GFunc) g_free, NULL);
	g_slist_free(eth->change_queue);
	
	/*
	 * Destroy columns
	 */
	for (i = cols - 1; i >= 0; i--){
		eth_do_remove (eth, i, TRUE);
	}
	
	if (e_table_header_parent_class->destroy)
		e_table_header_parent_class->destroy (object);
}

static void
e_table_header_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = eth_destroy;
	object_class->set_arg = eth_set_arg;
	object_class->get_arg = eth_get_arg;


	e_table_header_parent_class = (gtk_type_class (gtk_object_get_type ()));

	gtk_object_add_arg_type ("ETableHeader::width", GTK_TYPE_DOUBLE, 
				 GTK_ARG_READWRITE, ARG_WIDTH); 
	gtk_object_add_arg_type ("ETableHeader::sort_info", GTK_TYPE_OBJECT, 
				 GTK_ARG_READWRITE, ARG_SORT_INFO); 

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

static void
e_table_header_init (ETableHeader *eth)
{
	eth->col_count = 0;
	eth->width = 0;

	eth->sort_info = NULL;
	eth->sort_info_group_change_id = 0;

	eth->columns = NULL;
	eth->selectable = FALSE;
	
	eth->change_queue = NULL;
	eth->change_tail = NULL;
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
			(GtkObjectInitFunc) e_table_header_init,
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

	return eth;
}

static void
eth_group_info_changed(ETableSortInfo *info, ETableHeader *eth)
{
	enqueue(eth, -1, eth->nominal_width);
}

static void
eth_set_width(ETableHeader *eth, int width)
{
	eth->width = width;
}

static void
eth_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableHeader *eth = E_TABLE_HEADER (object);

	switch (arg_id) {
	case ARG_WIDTH:
		eth->nominal_width = GTK_VALUE_DOUBLE (*arg);
		enqueue(eth, -1, GTK_VALUE_DOUBLE (*arg));
		break;
	case ARG_SORT_INFO:
		if (eth->sort_info) {
			if (eth->sort_info_group_change_id)
				gtk_signal_disconnect(GTK_OBJECT(eth->sort_info), eth->sort_info_group_change_id);
			gtk_object_unref(GTK_OBJECT(eth->sort_info));
		}
		eth->sort_info = E_TABLE_SORT_INFO(GTK_VALUE_OBJECT (*arg));
		if (eth->sort_info) {
			gtk_object_ref(GTK_OBJECT(eth->sort_info));
			eth->sort_info_group_change_id 
				= gtk_signal_connect(GTK_OBJECT(eth->sort_info), "group_info_changed",
						     GTK_SIGNAL_FUNC(eth_group_info_changed), eth);
		}
		enqueue(eth, -1, eth->nominal_width);
		break;
	default:
		break;
	}
}

static void
eth_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableHeader *eth = E_TABLE_HEADER (object);

	switch (arg_id) {
	case ARG_SORT_INFO:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(eth->sort_info);
		break;
	case ARG_WIDTH:
		GTK_VALUE_DOUBLE (*arg) = eth->nominal_width;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
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

	enqueue(eth, -1, eth->nominal_width);
	gtk_signal_emit (GTK_OBJECT (eth), eth_signals [STRUCTURE_CHANGE]);
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
		 sizeof (ETableCol *) * (eth->col_count - idx - 1));
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
	g_return_if_fail (target_index < eth->col_count + 1); /* Can be moved beyond the last item. */

	if (source_index < target_index)
		target_index --;

	old = eth->columns [source_index];
	eth_do_remove (eth, source_index, FALSE);
	eth_do_insert (eth, target_index, old);
	eth_update_offsets (eth);
	
	gtk_signal_emit (GTK_OBJECT (eth), eth_signals [DIMENSION_CHANGE]);
	gtk_signal_emit (GTK_OBJECT (eth), eth_signals [STRUCTURE_CHANGE]);
}

void
e_table_header_remove (ETableHeader *eth, int idx)
{
	g_return_if_fail (eth != NULL);
	g_return_if_fail (E_IS_TABLE_HEADER (eth));
	g_return_if_fail (idx >= 0);
	g_return_if_fail (idx < eth->col_count);

	eth_do_remove (eth, idx, TRUE);
	enqueue(eth, -1, eth->nominal_width);
	gtk_signal_emit (GTK_OBJECT (eth), eth_signals [STRUCTURE_CHANGE]);
}

void
e_table_header_set_selection (ETableHeader *eth, gboolean allow_selection)
{
}

void
e_table_header_set_size(ETableHeader *eth, int idx, int size)
{
	enqueue(eth, idx, size);
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
	if (!eth->columns[idx]->resizeable)
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
		min_width += eth->columns[i]->min_width;
		if (eth->columns[i]->resizeable) {
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
		return;
	}

	total_extra = usable_width - min_width;
	/* If there's no extra space, set all expansions to 0. */
	if (total_extra <= 0) {
		for (i = idx; i < eth->col_count; i++) {
			eth->columns[i]->expansion = 0;
		}
		return;
	}

	/* If you try to resize smaller than the minimum width, it
	 * uses the minimum. */
	if (size < eth->columns[idx]->min_width)
		size = eth->columns[idx]->min_width;

	/* If all the extra space will be used up in this column, use
	 * all the expansion and set all others to 0.
	 */
	if (size >= total_extra + eth->columns[idx]->min_width) {
		eth->columns[idx]->expansion = expansion;
		for (i = idx + 1; i < eth->col_count; i++) {
			eth->columns[i]->expansion = 0;
		}
		return;
	}
	
	/* The old_expansion used by columns to the right. */
	old_expansion = expansion;
	old_expansion -= eth->columns[idx]->expansion;
	/* Set the new expansion so that it will generate the desired size. */
	eth->columns[idx]->expansion = expansion * (((double)(size - eth->columns[idx]->min_width))/((double)total_extra));
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
			if (eth->columns[idx]->resizeable) {
				/* expandable_count != 0 by (1) */
				eth->columns[i]->expansion = expansion / expandable_count;
			}
		}
		return;
	}

	/* Remove from total_extra the amount used for this column. */
	total_extra -= size - eth->columns[idx]->min_width;
	for (i = idx + 1; i < eth->col_count; i++) {
		if (eth->columns[idx]->resizeable) {
			/* old_expansion != 0 by (2) */
			eth->columns[i]->expansion *= expansion / old_expansion;
		}
	}
}

int
e_table_header_col_diff (ETableHeader *eth, int start_col, int end_col)
{
	int total, col;
	
	g_return_val_if_fail (eth != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_HEADER (eth), 0);

	{
		if (start_col < 0)
			start_col = 0;
		if (end_col > eth->col_count)
			end_col = eth->col_count;
		
		total = 0;
		for (col = start_col; col < end_col; col++){
			
			total += eth->columns [col]->width;
		}
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
	/* - 1 to account for the last pixel border. */
	extra = eth->width - 1;
	expansion = 0;
	for (i = 0; i < eth->col_count; i++) {
		extra -= eth->columns[i]->min_width;
		if (eth->columns[i]->resizeable && eth->columns[i]->expansion > 0)
			last_resizable = i;
		expansion += eth->columns[i]->resizeable ? eth->columns[i]->expansion : 0;
		eth->columns[i]->width = eth->columns[i]->min_width;
	}
	if (eth->sort_info)
		extra -= e_table_sort_info_grouping_get_count(eth->sort_info) * GROUP_INDENT;
	if (expansion == 0 || extra <= 0)
		return;
	for (i = 0; i < last_resizable; i++) {
		next_position += extra * (eth->columns[i]->resizeable ? eth->columns[i]->expansion : 0)/expansion;
		eth->columns[i]->width += next_position - last_position;
		last_position = next_position;
	}
	eth->columns[i]->width += extra - last_position;

	eth_update_offsets (eth);
	gtk_signal_emit (GTK_OBJECT (eth), eth_signals [DIMENSION_CHANGE]);
}
