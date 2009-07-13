/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <glib-object.h>

#include "e-util/e-util.h"

#include "e-table-model.h"

#define ETM_CLASS(e) (E_TABLE_MODEL_GET_CLASS (e))
#define ETM_FROZEN(e) (GPOINTER_TO_INT (g_object_get_data (G_OBJECT(e), "frozen")) != 0)

#define d(x)

d(static gint depth = 0;)

G_DEFINE_TYPE (ETableModel, e_table_model, G_TYPE_OBJECT)

enum {
	MODEL_NO_CHANGE,
	MODEL_CHANGED,
	MODEL_PRE_CHANGE,
	MODEL_ROW_CHANGED,
	MODEL_CELL_CHANGED,
	MODEL_ROWS_INSERTED,
	MODEL_ROWS_DELETED,
	ROW_SELECTION,
	LAST_SIGNAL
};

static guint e_table_model_signals [LAST_SIGNAL] = { 0, };

/**
 * e_table_model_column_count:
 * @e_table_model: The e-table-model to operate on
 *
 * Returns: the number of columns in the table model.
 */
gint
e_table_model_column_count (ETableModel *e_table_model)
{
	g_return_val_if_fail (e_table_model != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), 0);

	return ETM_CLASS (e_table_model)->column_count (e_table_model);
}

/**
 * e_table_model_row_count:
 * @e_table_model: the e-table-model to operate on
 *
 * Returns: the number of rows in the Table model.
 */
gint
e_table_model_row_count (ETableModel *e_table_model)
{
	g_return_val_if_fail (e_table_model != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), 0);

	return ETM_CLASS (e_table_model)->row_count (e_table_model);
}

/**
 * e_table_model_append_row:
 * @e_table_model: the table model to append the a row to.
 * @source:
 * @row:
 *
 */
void
e_table_model_append_row (ETableModel *e_table_model, ETableModel *source, gint row)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	if (ETM_CLASS (e_table_model)->append_row)
		ETM_CLASS (e_table_model)->append_row (e_table_model, source, row);
}

/**
 * e_table_value_at:
 * @e_table_model: the e-table-model to operate on
 * @col: column in the model to pull data from.
 * @row: row in the model to pull data from.
 *
 * Return value: This function returns the value that is stored
 * by the @e_table_model in column @col and row @row.  The data
 * returned can be a pointer or any data value that can be stored
 * inside a pointer.
 *
 * The data returned is typically used by an ECell renderer.
 *
 * The data returned must be valid until the model sends a signal that
 * affect that piece of data.  model_changed affects all data.
 * row_changed affects the data in that row.  cell_changed affects the
 * data in that cell.  rows_deleted affects all data in those rows.
 * rows_inserted and no_change don't affect any data in this way.
 **/
gpointer
e_table_model_value_at (ETableModel *e_table_model, gint col, gint row)
{
	g_return_val_if_fail (e_table_model != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), NULL);

	return ETM_CLASS (e_table_model)->value_at (e_table_model, col, row);
}

/**
 * e_table_model_set_value_at:
 * @e_table_model: the table model to operate on.
 * @col: the column where the data will be stored in the model.
 * @row: the row where the data will be stored in the model.
 * @value: the data to be stored.
 *
 * This function instructs the model to store the value in @data in the
 * the @e_table_model at column @col and row @row.  The @data typically
 * comes from one of the ECell rendering objects.
 *
 * There should be an agreement between the Table Model and the user
 * of this function about the data being stored.  Typically it will
 * be a pointer to a set of data, or a datum that fits inside a gpointer .
 */
void
e_table_model_set_value_at (ETableModel *e_table_model, gint col, gint row, gconstpointer value)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	ETM_CLASS (e_table_model)->set_value_at (e_table_model, col, row, value);
}

/**
 * e_table_model_is_cell_editable:
 * @e_table_model: the table model to query.
 * @col: column to query.
 * @row: row to query.
 *
 * Returns: %TRUE if the cell in @e_table_model at @col,@row can be
 * edited, %FALSE otherwise
 */
gboolean
e_table_model_is_cell_editable (ETableModel *e_table_model, gint col, gint row)
{
	g_return_val_if_fail (e_table_model != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), FALSE);

	return ETM_CLASS (e_table_model)->is_cell_editable (e_table_model, col, row);
}

gpointer
e_table_model_duplicate_value (ETableModel *e_table_model, gint col, gconstpointer value)
{
	g_return_val_if_fail (e_table_model != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), NULL);

	if (ETM_CLASS (e_table_model)->duplicate_value)
		return ETM_CLASS (e_table_model)->duplicate_value (e_table_model, col, value);
	else
		return NULL;
}

void
e_table_model_free_value (ETableModel *e_table_model, gint col, gpointer value)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	if (ETM_CLASS (e_table_model)->free_value)
		ETM_CLASS (e_table_model)->free_value (e_table_model, col, value);
}

gboolean
e_table_model_has_save_id (ETableModel *e_table_model)
{
	g_return_val_if_fail (e_table_model != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), FALSE);

	if (ETM_CLASS (e_table_model)->has_save_id)
		return ETM_CLASS (e_table_model)->has_save_id (e_table_model);
	else
		return FALSE;
}

gchar *
e_table_model_get_save_id (ETableModel *e_table_model, gint row)
{
	g_return_val_if_fail (e_table_model != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), NULL);

	if (ETM_CLASS (e_table_model)->get_save_id)
		return ETM_CLASS (e_table_model)->get_save_id (e_table_model, row);
	else
		return NULL;
}

gboolean
e_table_model_has_change_pending(ETableModel *e_table_model)
{
	g_return_val_if_fail (e_table_model != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), FALSE);

	if (ETM_CLASS (e_table_model)->has_change_pending)
		return ETM_CLASS (e_table_model)->has_change_pending (e_table_model);
	else
		return FALSE;
}

gpointer
e_table_model_initialize_value (ETableModel *e_table_model, gint col)
{
	g_return_val_if_fail (e_table_model != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), NULL);

	if (ETM_CLASS (e_table_model)->initialize_value)
		return ETM_CLASS (e_table_model)->initialize_value (e_table_model, col);
	else
		return NULL;
}

gboolean
e_table_model_value_is_empty (ETableModel *e_table_model, gint col, gconstpointer value)
{
	g_return_val_if_fail (e_table_model != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), FALSE);

	if (ETM_CLASS (e_table_model)->value_is_empty)
		return ETM_CLASS (e_table_model)->value_is_empty (e_table_model, col, value);
	else
		return FALSE;
}

gchar *
e_table_model_value_to_string (ETableModel *e_table_model, gint col, gconstpointer value)
{
	g_return_val_if_fail (e_table_model != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), NULL);

	if (ETM_CLASS (e_table_model)->value_to_string)
		return ETM_CLASS (e_table_model)->value_to_string (e_table_model, col, value);
	else
		return g_strdup("");
}

static void
e_table_model_finalize (GObject *object)
{
	if (G_OBJECT_CLASS (e_table_model_parent_class)->finalize)
		(* G_OBJECT_CLASS (e_table_model_parent_class)->finalize)(object);
}

static void
e_table_model_class_init (ETableModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = e_table_model_finalize;

	e_table_model_signals [MODEL_NO_CHANGE] =
		g_signal_new ("model_no_change",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableModelClass, model_no_change),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	e_table_model_signals [MODEL_CHANGED] =
		g_signal_new ("model_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableModelClass, model_changed),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	e_table_model_signals [MODEL_PRE_CHANGE] =
		g_signal_new ("model_pre_change",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableModelClass, model_pre_change),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	e_table_model_signals [MODEL_ROW_CHANGED] =
		g_signal_new ("model_row_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableModelClass, model_row_changed),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	e_table_model_signals [MODEL_CELL_CHANGED] =
		g_signal_new ("model_cell_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableModelClass, model_cell_changed),
			      (GSignalAccumulator) NULL, NULL,
			      e_marshal_VOID__INT_INT,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

	e_table_model_signals [MODEL_ROWS_INSERTED] =
		g_signal_new ("model_rows_inserted",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableModelClass, model_rows_inserted),
			      (GSignalAccumulator) NULL, NULL,
			      e_marshal_VOID__INT_INT,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

	e_table_model_signals [MODEL_ROWS_DELETED] =
		g_signal_new ("model_rows_deleted",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableModelClass, model_rows_deleted),
			      (GSignalAccumulator) NULL, NULL,
			      e_marshal_VOID__INT_INT,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

	klass->column_count        = NULL;
	klass->row_count           = NULL;
	klass->append_row          = NULL;

	klass->value_at            = NULL;
	klass->set_value_at        = NULL;
	klass->is_cell_editable    = NULL;

	klass->has_save_id         = NULL;
	klass->get_save_id         = NULL;

	klass->has_change_pending  = NULL;

	klass->duplicate_value     = NULL;
	klass->free_value          = NULL;
	klass->initialize_value    = NULL;
	klass->value_is_empty      = NULL;
	klass->value_to_string     = NULL;

	klass->model_no_change     = NULL;
	klass->model_changed       = NULL;
	klass->model_row_changed   = NULL;
	klass->model_cell_changed  = NULL;
	klass->model_rows_inserted = NULL;
	klass->model_rows_deleted  = NULL;
}

static void
e_table_model_init (ETableModel *e_table_model)
{
	/* nothing to do */
}

#if d(!)0
static void
print_tabs (void)
{
	gint i;
	for (i = 0; i < depth; i++)
		g_print("\t");
}
#endif

void
e_table_model_pre_change (ETableModel *e_table_model)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	if (ETM_FROZEN (e_table_model))
		return;

	d(print_tabs());
	d(g_print("Emitting pre_change on model 0x%p, a %s.\n", e_table_model, g_type_name (GTK_OBJECT(e_table_model)->klass->type)));
	d(depth++);
	g_signal_emit (G_OBJECT (e_table_model),
		       e_table_model_signals [MODEL_PRE_CHANGE], 0);
	d(depth--);
}

/**
 * e_table_model_no_change:
 * @e_table_model: the table model to notify of the lack of a change
 *
 * Use this function to notify any views of this table model that
 * the contents of the table model have changed.  This will emit
 * the signal "model_no_change" on the @e_table_model object.
 *
 * It is preferable to use the e_table_model_row_changed() and
 * the e_table_model_cell_changed() to notify of smaller changes
 * than to invalidate the entire model, as the views might have
 * ways of caching the information they render from the model.
 */
void
e_table_model_no_change (ETableModel *e_table_model)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	if (ETM_FROZEN (e_table_model))
		return;

	d(print_tabs());
	d(g_print("Emitting model_no_change on model 0x%p, a %s.\n", e_table_model, g_type_name (GTK_OBJECT(e_table_model)->klass->type)));
	d(depth++);
	g_signal_emit (G_OBJECT (e_table_model),
		       e_table_model_signals [MODEL_NO_CHANGE], 0);
	d(depth--);
}

/**
 * e_table_model_changed:
 * @e_table_model: the table model to notify of the change
 *
 * Use this function to notify any views of this table model that
 * the contents of the table model have changed.  This will emit
 * the signal "model_changed" on the @e_table_model object.
 *
 * It is preferable to use the e_table_model_row_changed() and
 * the e_table_model_cell_changed() to notify of smaller changes
 * than to invalidate the entire model, as the views might have
 * ways of caching the information they render from the model.
 */
void
e_table_model_changed (ETableModel *e_table_model)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	if (ETM_FROZEN (e_table_model))
		return;

	d(print_tabs());
	d(g_print("Emitting model_changed on model 0x%p, a %s.\n", e_table_model, g_type_name (GTK_OBJECT(e_table_model)->klass->type)));
	d(depth++);
	g_signal_emit (G_OBJECT (e_table_model),
		       e_table_model_signals [MODEL_CHANGED], 0);
	d(depth--);
}

/**
 * e_table_model_row_changed:
 * @e_table_model: the table model to notify of the change
 * @row: the row that was changed in the model.
 *
 * Use this function to notify any views of the table model that
 * the contents of row @row have changed in model.  This function
 * will emit the "model_row_changed" signal on the @e_table_model
 * object
 */
void
e_table_model_row_changed (ETableModel *e_table_model, gint row)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	if (ETM_FROZEN (e_table_model))
		return;

	d(print_tabs());
	d(g_print("Emitting row_changed on model 0x%p, a %s, row %d.\n", e_table_model, g_type_name (GTK_OBJECT(e_table_model)->klass->type), row));
	d(depth++);
	g_signal_emit (G_OBJECT (e_table_model),
		       e_table_model_signals [MODEL_ROW_CHANGED], 0, row);
	d(depth--);
}

/**
 * e_table_model_cell_changed:
 * @e_table_model: the table model to notify of the change
 * @col: the column.
 * @row: the row
 *
 * Use this function to notify any views of the table model that
 * contents of the cell at @col,@row has changed. This will emit
 * the "model_cell_changed" signal on the @e_table_model
 * object
 */
void
e_table_model_cell_changed (ETableModel *e_table_model, gint col, gint row)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	if (ETM_FROZEN (e_table_model))
		return;

	d(print_tabs());
	d(g_print("Emitting cell_changed on model 0x%p, a %s, row %d, col %d.\n", e_table_model, g_type_name (GTK_OBJECT(e_table_model)->klass->type), row, col));
	d(depth++);
	g_signal_emit (G_OBJECT (e_table_model),
		       e_table_model_signals [MODEL_CELL_CHANGED], 0, col, row);
	d(depth--);
}

/**
 * e_table_model_rows_inserted:
 * @e_table_model: the table model to notify of the change
 * @row: the row that was inserted into the model.
 * @count: The number of rows that were inserted.
 *
 * Use this function to notify any views of the table model that
 * @count rows at row @row have been inserted into the model.  This
 * function will emit the "model_rows_inserted" signal on the
 * @e_table_model object
 */
void
e_table_model_rows_inserted (ETableModel *e_table_model, gint row, gint count)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	if (ETM_FROZEN (e_table_model))
		return;

	d(print_tabs());
	d(g_print("Emitting row_inserted on model 0x%p, a %s, row %d.\n", e_table_model, g_type_name (GTK_OBJECT(e_table_model)->klass->type), row));
	d(depth++);
	g_signal_emit (G_OBJECT (e_table_model),
		       e_table_model_signals [MODEL_ROWS_INSERTED], 0, row, count);
	d(depth--);
}

/**
 * e_table_model_row_inserted:
 * @e_table_model: the table model to notify of the change
 * @row: the row that was inserted into the model.
 *
 * Use this function to notify any views of the table model that the
 * row @row has been inserted into the model.  This function will emit
 * the "model_rows_inserted" signal on the @e_table_model object
 */
void
e_table_model_row_inserted (ETableModel *e_table_model, gint row)
{
	e_table_model_rows_inserted(e_table_model, row, 1);
}

/**
 * e_table_model_row_deleted:
 * @e_table_model: the table model to notify of the change
 * @row: the row that was deleted
 * @count: The number of rows deleted
 *
 * Use this function to notify any views of the table model that
 * @count rows at row @row have been deleted from the model.  This
 * function will emit the "model_rows_deleted" signal on the
 * @e_table_model object
 */
void
e_table_model_rows_deleted (ETableModel *e_table_model, gint row, gint count)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	if (ETM_FROZEN (e_table_model))
		return;

	d(print_tabs());
	d(g_print("Emitting row_deleted on model 0x%p, a %s, row %d.\n", e_table_model, g_type_name (GTK_OBJECT(e_table_model)->klass->type), row));
	d(depth++);
	g_signal_emit (G_OBJECT (e_table_model),
		       e_table_model_signals [MODEL_ROWS_DELETED], 0, row, count);
	d(depth--);
}

/**
 * e_table_model_row_deleted:
 * @e_table_model: the table model to notify of the change
 * @row: the row that was deleted
 *
 * Use this function to notify any views of the table model that the
 * row @row has been deleted from the model.  This function will emit
 * the "model_rows_deleted" signal on the @e_table_model object
 */
void
e_table_model_row_deleted (ETableModel *e_table_model, gint row)
{
	e_table_model_rows_deleted(e_table_model, row, 1);
}

void
e_table_model_freeze (ETableModel *e_table_model)
{
	e_table_model_pre_change (e_table_model);
	g_object_set_data (G_OBJECT (e_table_model), "frozen", GINT_TO_POINTER (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (e_table_model), "frozen")) + 1));
}

void
e_table_model_thaw (ETableModel *e_table_model)
{
	g_object_set_data (G_OBJECT (e_table_model), "frozen", GINT_TO_POINTER (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (e_table_model), "frozen")) - 1));
	e_table_model_changed (e_table_model);
}

