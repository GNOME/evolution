/*
 * e-table-model.c
 *
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
 */

#include "e-table-model.h"

#define d(x)

d (static gint depth = 0;)

G_DEFINE_INTERFACE (ETableModel, e_table_model, G_TYPE_OBJECT)

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

static guint signals[LAST_SIGNAL] = { 0, };

static gint
table_model_is_frozen (ETableModel *table_model)
{
	gpointer data;

	data = g_object_get_data (G_OBJECT (table_model), "frozen");

	return (GPOINTER_TO_INT (data) != 0);
}

static void
e_table_model_default_init (ETableModelInterface *iface)
{
	signals[MODEL_NO_CHANGE] = g_signal_new (
		"model_no_change",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableModelInterface, model_no_change),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);

	signals[MODEL_CHANGED] = g_signal_new (
		"model_changed",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableModelInterface, model_changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);

	signals[MODEL_PRE_CHANGE] = g_signal_new (
		"model_pre_change",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableModelInterface, model_pre_change),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);

	signals[MODEL_ROW_CHANGED] = g_signal_new (
		"model_row_changed",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableModelInterface, model_row_changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		G_TYPE_INT);

	signals[MODEL_CELL_CHANGED] = g_signal_new (
		"model_cell_changed",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableModelInterface, model_cell_changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		G_TYPE_INT,
		G_TYPE_INT);

	signals[MODEL_ROWS_INSERTED] = g_signal_new (
		"model_rows_inserted",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableModelInterface, model_rows_inserted),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		G_TYPE_INT,
		G_TYPE_INT);

	signals[MODEL_ROWS_DELETED] = g_signal_new (
		"model_rows_deleted",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableModelInterface, model_rows_deleted),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		G_TYPE_INT,
		G_TYPE_INT);
}

/**
 * e_table_model_column_count:
 * @table_model: The e-table-model to operate on
 *
 * Returns: the number of columns in the table model.
 */
gint
e_table_model_column_count (ETableModel *table_model)
{
	ETableModelInterface *iface;

	g_return_val_if_fail (E_IS_TABLE_MODEL (table_model), 0);

	iface = E_TABLE_MODEL_GET_INTERFACE (table_model);
	g_return_val_if_fail (iface->column_count != NULL, 0);

	return iface->column_count (table_model);
}

/**
 * e_table_model_row_count:
 * @table_model: the e-table-model to operate on
 *
 * Returns: the number of rows in the Table model.
 */
gint
e_table_model_row_count (ETableModel *table_model)
{
	ETableModelInterface *iface;

	g_return_val_if_fail (E_IS_TABLE_MODEL (table_model), 0);

	iface = E_TABLE_MODEL_GET_INTERFACE (table_model);
	g_return_val_if_fail (iface->row_count != NULL, 0);

	return iface->row_count (table_model);
}

/**
 * e_table_model_append_row:
 * @table_model: the table model to append the a row to.
 * @source:
 * @row:
 *
 */
void
e_table_model_append_row (ETableModel *table_model,
                          ETableModel *source,
                          gint row)
{
	ETableModelInterface *iface;

	g_return_if_fail (E_IS_TABLE_MODEL (table_model));

	iface = E_TABLE_MODEL_GET_INTERFACE (table_model);

	if (iface->append_row != NULL)
		iface->append_row (table_model, source, row);
}

/**
 * e_table_value_at:
 * @table_model: the e-table-model to operate on
 * @col: column in the model to pull data from.
 * @row: row in the model to pull data from.
 *
 * Return value: This function returns the value that is stored
 * by the @table_model in column @col and row @row.  The data
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
e_table_model_value_at (ETableModel *table_model,
                        gint col,
                        gint row)
{
	ETableModelInterface *iface;

	g_return_val_if_fail (E_IS_TABLE_MODEL (table_model), NULL);

	iface = E_TABLE_MODEL_GET_INTERFACE (table_model);
	g_return_val_if_fail (iface->value_at != NULL, NULL);

	return iface->value_at (table_model, col, row);
}

/**
 * e_table_model_set_value_at:
 * @table_model: the table model to operate on.
 * @col: the column where the data will be stored in the model.
 * @row: the row where the data will be stored in the model.
 * @value: the data to be stored.
 *
 * This function instructs the model to store the value in @data in the
 * the @table_model at column @col and row @row.  The @data typically
 * comes from one of the ECell rendering objects.
 *
 * There should be an agreement between the Table Model and the user
 * of this function about the data being stored.  Typically it will
 * be a pointer to a set of data, or a datum that fits inside a gpointer .
 */
void
e_table_model_set_value_at (ETableModel *table_model,
                            gint col,
                            gint row,
                            gconstpointer value)
{
	ETableModelInterface *iface;

	g_return_if_fail (E_IS_TABLE_MODEL (table_model));

	iface = E_TABLE_MODEL_GET_INTERFACE (table_model);
	g_return_if_fail (iface->set_value_at != NULL);

	iface->set_value_at (table_model, col, row, value);
}

/**
 * e_table_model_is_cell_editable:
 * @table_model: the table model to query.
 * @col: column to query.
 * @row: row to query.
 *
 * Returns: %TRUE if the cell in @table_model at @col,@row can be
 * edited, %FALSE otherwise
 */
gboolean
e_table_model_is_cell_editable (ETableModel *table_model,
                                gint col,
                                gint row)
{
	ETableModelInterface *iface;

	g_return_val_if_fail (E_IS_TABLE_MODEL (table_model), FALSE);

	iface = E_TABLE_MODEL_GET_INTERFACE (table_model);
	g_return_val_if_fail (iface->is_cell_editable != NULL, FALSE);

	return iface->is_cell_editable (table_model, col, row);
}

gpointer
e_table_model_duplicate_value (ETableModel *table_model,
                               gint col,
                               gconstpointer value)
{
	ETableModelInterface *iface;

	g_return_val_if_fail (E_IS_TABLE_MODEL (table_model), NULL);

	iface = E_TABLE_MODEL_GET_INTERFACE (table_model);

	if (iface->duplicate_value == NULL)
		return NULL;

	return iface->duplicate_value (table_model, col, value);
}

void
e_table_model_free_value (ETableModel *table_model,
                          gint col,
                          gpointer value)
{
	ETableModelInterface *iface;

	g_return_if_fail (E_IS_TABLE_MODEL (table_model));

	iface = E_TABLE_MODEL_GET_INTERFACE (table_model);

	if (iface->free_value != NULL)
		iface->free_value (table_model, col, value);
}

gboolean
e_table_model_has_save_id (ETableModel *table_model)
{
	ETableModelInterface *iface;

	g_return_val_if_fail (E_IS_TABLE_MODEL (table_model), FALSE);

	iface = E_TABLE_MODEL_GET_INTERFACE (table_model);

	if (iface->has_save_id == NULL)
		return FALSE;

	return iface->has_save_id (table_model);
}

gchar *
e_table_model_get_save_id (ETableModel *table_model,
                           gint row)
{
	ETableModelInterface *iface;

	g_return_val_if_fail (E_IS_TABLE_MODEL (table_model), NULL);

	iface = E_TABLE_MODEL_GET_INTERFACE (table_model);

	if (iface->get_save_id == NULL)
		return NULL;

	return iface->get_save_id (table_model, row);
}

gboolean
e_table_model_has_change_pending (ETableModel *table_model)
{
	ETableModelInterface *iface;

	g_return_val_if_fail (E_IS_TABLE_MODEL (table_model), FALSE);

	iface = E_TABLE_MODEL_GET_INTERFACE (table_model);

	if (iface->has_change_pending == NULL)
		return FALSE;

	return iface->has_change_pending (table_model);
}

gpointer
e_table_model_initialize_value (ETableModel *table_model,
                                gint col)
{
	ETableModelInterface *iface;

	g_return_val_if_fail (E_IS_TABLE_MODEL (table_model), NULL);

	iface = E_TABLE_MODEL_GET_INTERFACE (table_model);

	if (iface->initialize_value == NULL)
		return NULL;

	return iface->initialize_value (table_model, col);
}

gboolean
e_table_model_value_is_empty (ETableModel *table_model,
                              gint col,
                              gconstpointer value)
{
	ETableModelInterface *iface;

	g_return_val_if_fail (E_IS_TABLE_MODEL (table_model), FALSE);

	iface = E_TABLE_MODEL_GET_INTERFACE (table_model);

	if (iface->value_is_empty == NULL)
		return FALSE;

	return iface->value_is_empty (table_model, col, value);
}

gchar *
e_table_model_value_to_string (ETableModel *table_model,
                               gint col,
                               gconstpointer value)
{
	ETableModelInterface *iface;

	g_return_val_if_fail (E_IS_TABLE_MODEL (table_model), NULL);

	iface = E_TABLE_MODEL_GET_INTERFACE (table_model);

	if (iface->value_to_string == NULL)
		return g_strdup ("");

	return iface->value_to_string (table_model, col, value);
}

#if d(!)0
static void
print_tabs (void)
{
	gint i;
	for (i = 0; i < depth; i++)
		g_print ("\t");
}
#endif

void
e_table_model_pre_change (ETableModel *table_model)
{
	g_return_if_fail (E_IS_TABLE_MODEL (table_model));

	if (table_model_is_frozen (table_model))
		return;

	d (print_tabs ());
	d (depth++);
	g_signal_emit (table_model, signals[MODEL_PRE_CHANGE], 0);
	d (depth--);
}

/**
 * e_table_model_no_change:
 * @table_model: the table model to notify of the lack of a change
 *
 * Use this function to notify any views of this table model that
 * the contents of the table model have changed.  This will emit
 * the signal "model_no_change" on the @table_model object.
 *
 * It is preferable to use the e_table_model_row_changed() and
 * the e_table_model_cell_changed() to notify of smaller changes
 * than to invalidate the entire model, as the views might have
 * ways of caching the information they render from the model.
 */
void
e_table_model_no_change (ETableModel *table_model)
{
	g_return_if_fail (E_IS_TABLE_MODEL (table_model));

	if (table_model_is_frozen (table_model))
		return;

	d (print_tabs ());
	d (depth++);
	g_signal_emit (table_model, signals[MODEL_NO_CHANGE], 0);
	d (depth--);
}

/**
 * e_table_model_changed:
 * @table_model: the table model to notify of the change
 *
 * Use this function to notify any views of this table model that
 * the contents of the table model have changed.  This will emit
 * the signal "model_changed" on the @table_model object.
 *
 * It is preferable to use the e_table_model_row_changed() and
 * the e_table_model_cell_changed() to notify of smaller changes
 * than to invalidate the entire model, as the views might have
 * ways of caching the information they render from the model.
 */
void
e_table_model_changed (ETableModel *table_model)
{
	g_return_if_fail (E_IS_TABLE_MODEL (table_model));

	if (table_model_is_frozen (table_model))
		return;

	d (print_tabs ());
	d (depth++);
	g_signal_emit (table_model, signals[MODEL_CHANGED], 0);
	d (depth--);
}

/**
 * e_table_model_row_changed:
 * @table_model: the table model to notify of the change
 * @row: the row that was changed in the model.
 *
 * Use this function to notify any views of the table model that
 * the contents of row @row have changed in model.  This function
 * will emit the "model_row_changed" signal on the @table_model
 * object
 */
void
e_table_model_row_changed (ETableModel *table_model,
                           gint row)
{
	g_return_if_fail (E_IS_TABLE_MODEL (table_model));

	if (table_model_is_frozen (table_model))
		return;

	d (print_tabs ());
	d (depth++);
	g_signal_emit (table_model, signals[MODEL_ROW_CHANGED], 0, row);
	d (depth--);
}

/**
 * e_table_model_cell_changed:
 * @table_model: the table model to notify of the change
 * @col: the column.
 * @row: the row
 *
 * Use this function to notify any views of the table model that
 * contents of the cell at @col,@row has changed. This will emit
 * the "model_cell_changed" signal on the @table_model
 * object
 */
void
e_table_model_cell_changed (ETableModel *table_model,
                            gint col,
                            gint row)
{
	g_return_if_fail (E_IS_TABLE_MODEL (table_model));

	if (table_model_is_frozen (table_model))
		return;

	d (print_tabs ());
	d (depth++);
	g_signal_emit (
		table_model, signals[MODEL_CELL_CHANGED], 0, col, row);
	d (depth--);
}

/**
 * e_table_model_rows_inserted:
 * @table_model: the table model to notify of the change
 * @row: the row that was inserted into the model.
 * @count: The number of rows that were inserted.
 *
 * Use this function to notify any views of the table model that
 * @count rows at row @row have been inserted into the model.  This
 * function will emit the "model_rows_inserted" signal on the
 * @table_model object
 */
void
e_table_model_rows_inserted (ETableModel *table_model,
                             gint row,
                             gint count)
{
	g_return_if_fail (E_IS_TABLE_MODEL (table_model));

	if (table_model_is_frozen (table_model))
		return;

	d (print_tabs ());
	d (depth++);
	g_signal_emit (
		table_model, signals[MODEL_ROWS_INSERTED], 0, row, count);
	d (depth--);
}

/**
 * e_table_model_row_inserted:
 * @table_model: the table model to notify of the change
 * @row: the row that was inserted into the model.
 *
 * Use this function to notify any views of the table model that the
 * row @row has been inserted into the model.  This function will emit
 * the "model_rows_inserted" signal on the @table_model object
 */
void
e_table_model_row_inserted (ETableModel *table_model,
                            gint row)
{
	g_return_if_fail (E_IS_TABLE_MODEL (table_model));

	e_table_model_rows_inserted (table_model, row, 1);
}

/**
 * e_table_model_row_deleted:
 * @table_model: the table model to notify of the change
 * @row: the row that was deleted
 * @count: The number of rows deleted
 *
 * Use this function to notify any views of the table model that
 * @count rows at row @row have been deleted from the model.  This
 * function will emit the "model_rows_deleted" signal on the
 * @table_model object
 */
void
e_table_model_rows_deleted (ETableModel *table_model,
                            gint row,
                            gint count)
{
	g_return_if_fail (E_IS_TABLE_MODEL (table_model));

	if (table_model_is_frozen (table_model))
		return;

	d (print_tabs ());
	d (depth++);
	g_signal_emit (
		table_model, signals[MODEL_ROWS_DELETED], 0, row, count);
	d (depth--);
}

/**
 * e_table_model_row_deleted:
 * @table_model: the table model to notify of the change
 * @row: the row that was deleted
 *
 * Use this function to notify any views of the table model that the
 * row @row has been deleted from the model.  This function will emit
 * the "model_rows_deleted" signal on the @table_model object
 */
void
e_table_model_row_deleted (ETableModel *table_model,
                           gint row)
{
	g_return_if_fail (E_IS_TABLE_MODEL (table_model));

	e_table_model_rows_deleted (table_model, row, 1);
}

void
e_table_model_freeze (ETableModel *table_model)
{
	gpointer data;

	g_return_if_fail (E_IS_TABLE_MODEL (table_model));

	e_table_model_pre_change (table_model);

	data = g_object_get_data (G_OBJECT (table_model), "frozen");
	data = GINT_TO_POINTER (GPOINTER_TO_INT (data) + 1);
	g_object_set_data (G_OBJECT (table_model), "frozen", data);
}

void
e_table_model_thaw (ETableModel *table_model)
{
	gpointer data;

	g_return_if_fail (E_IS_TABLE_MODEL (table_model));

	data = g_object_get_data (G_OBJECT (table_model), "frozen");
	data = GINT_TO_POINTER (GPOINTER_TO_INT (data) - 1);
	g_object_set_data (G_OBJECT (table_model), "frozen", data);

	e_table_model_changed (table_model);
}

