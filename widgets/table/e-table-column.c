/*
 * e-table-column.c: TableColumn implementation
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Helix Code, Inc
 */
#include <config.h>
#include "e-table-column.h"

enum {
	STRUCTURE_CHANGE,
	DIMENSION_CHANGE,
	LAST_SIGNAL
};

static guint etc_signals [LAST_SIGNAL] = { 0, };

static GtkObjectClass *e_table_column_parent_class;

static void
e_table_column_destroy (GtkObject *object)
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
	
	if (e_table_column_parent_class->destroy)
		e_table_column_parent_class->destroy (object);
}

static void
e_table_column_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = e_table_column_destroy;

	e_table_column_parent_class = (gtk_type_class (gtk_object_get_type ()));

	etc_signals [STRUCTURE_CHANGE] =
		gtk_signal_new ("structure_change",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableColumn, structure_change),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	etc_signals [DIMENSION_CHANGE] = 
		gtk_signal_new ("dimension_change", 
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableColumn, dimension_change),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	gtk_object_class_add_signals (object_class, etc_signals, LAST_SIGNAL);
}

GtkType
e_table_column_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ETableColumn",
			sizeof (ETableColumn),
			sizeof (ETableColumnClass),
			(GtkClassInitFunc) e_table_column_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gtk_object_get_type (), &info);
	}

	return type;
}

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

	gtk_signal_emit (GTK_OBJECT (etc), etc_signals [STRUCTURE_CHANGE]);
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
	gtk_signal_emit (GTK_OBJECT (etc), etc_signals [STRUCTURE_CHANGE]);
}

void
e_table_column_remove (ETableColumn *etc, int idx)
{
	g_return_if_fail (etc != NULL);
	g_return_if_fail (E_IS_TABLE_COLUMN (etc));
	g_return_if_fail (idx >= 0);
	g_return_if_fail (idx < etc->col_count);

	etc_do_remove (etc, idx);
	gtk_signal_emit (GTK_OBJECT (etc), etc_signals [STRUCTURE_CHANGE]);
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
	gtk_signal_emit (GTK_OBJECT (etc), etc_signals [SIZE_CHANGE], idx);
}
