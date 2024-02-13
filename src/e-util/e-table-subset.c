/*
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
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdlib.h>

#include "e-table-subset.h"

#define VALID_ROW(table_subset, row) \
	(row >= -1 && row < table_subset->n_map)
#define MAP_ROW(table_subset, row) \
	(row == -1 ? -1 : table_subset->map_table[row])

#define d(x)

struct _ETableSubsetPrivate {
	ETableModel *source_model;
	gulong table_model_pre_change_handler_id;
	gulong table_model_no_change_handler_id;
	gulong table_model_changed_handler_id;
	gulong table_model_row_changed_handler_id;
	gulong table_model_cell_changed_handler_id;
	gulong table_model_rows_inserted_handler_id;
	gulong table_model_rows_deleted_handler_id;

	gint last_access;
};

/* Forward Declarations */
static void	e_table_subset_table_model_init
					(ETableModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ETableSubset, e_table_subset, G_TYPE_OBJECT,
	G_ADD_PRIVATE (ETableSubset)
	G_IMPLEMENT_INTERFACE (E_TYPE_TABLE_MODEL, e_table_subset_table_model_init))

static gint
table_subset_get_view_row (ETableSubset *table_subset,
                           gint row)
{
	const gint n = table_subset->n_map;
	const gint * const map_table = table_subset->map_table;
	gint i;

	gint end = MIN (
		table_subset->n_map,
		table_subset->priv->last_access + 10);
	gint start = MAX (0, table_subset->priv->last_access - 10);
	gint initial = MAX (MIN (table_subset->priv->last_access, end), start);

	for (i = initial; i < end; i++) {
		if (map_table[i] == row) {
			table_subset->priv->last_access = i;
			return i;
		}
	}

	for (i = initial - 1; i >= start; i--) {
		if (map_table[i] == row) {
			table_subset->priv->last_access = i;
			return i;
		}
	}

	for (i = 0; i < n; i++) {
		if (map_table[i] == row) {
			table_subset->priv->last_access = i;
			return i;
		}
	}
	return -1;
}

static void
table_subset_dispose (GObject *object)
{
	ETableSubset *self = E_TABLE_SUBSET (object);

	if (self->priv->table_model_pre_change_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->source_model,
			self->priv->table_model_pre_change_handler_id);
		self->priv->table_model_pre_change_handler_id = 0;
	}

	if (self->priv->table_model_no_change_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->source_model,
			self->priv->table_model_no_change_handler_id);
		self->priv->table_model_no_change_handler_id = 0;
	}

	if (self->priv->table_model_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->source_model,
			self->priv->table_model_changed_handler_id);
		self->priv->table_model_changed_handler_id = 0;
	}

	if (self->priv->table_model_row_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->source_model,
			self->priv->table_model_row_changed_handler_id);
		self->priv->table_model_row_changed_handler_id = 0;
	}

	if (self->priv->table_model_cell_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->source_model,
			self->priv->table_model_cell_changed_handler_id);
		self->priv->table_model_cell_changed_handler_id = 0;
	}

	if (self->priv->table_model_rows_inserted_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->source_model,
			self->priv->table_model_rows_inserted_handler_id);
		self->priv->table_model_rows_inserted_handler_id = 0;
	}

	if (self->priv->table_model_rows_deleted_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->source_model,
			self->priv->table_model_rows_deleted_handler_id);
		self->priv->table_model_rows_deleted_handler_id = 0;
	}

	g_clear_object (&self->priv->source_model);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_table_subset_parent_class)->dispose (object);
}

static void
table_subset_finalize (GObject *object)
{
	ETableSubset *table_subset;

	table_subset = E_TABLE_SUBSET (object);

	g_free (table_subset->map_table);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_table_subset_parent_class)->finalize (object);
}

static void
table_subset_proxy_model_pre_change_real (ETableSubset *table_subset,
                                          ETableModel *source_model)
{
	e_table_model_pre_change (E_TABLE_MODEL (table_subset));
}

static void
table_subset_proxy_model_no_change_real (ETableSubset *table_subset,
                                         ETableModel *source_model)
{
	e_table_model_no_change (E_TABLE_MODEL (table_subset));
}

static void
table_subset_proxy_model_changed_real (ETableSubset *table_subset,
                                       ETableModel *source_model)
{
	e_table_model_changed (E_TABLE_MODEL (table_subset));
}

static void
table_subset_proxy_model_row_changed_real (ETableSubset *table_subset,
                                           ETableModel *source_model,
                                           gint row)
{
	gint view_row = table_subset_get_view_row (table_subset, row);

	if (view_row != -1)
		e_table_model_row_changed (
			E_TABLE_MODEL (table_subset), view_row);
	else
		e_table_model_no_change (E_TABLE_MODEL (table_subset));
}

static void
table_subset_proxy_model_cell_changed_real (ETableSubset *table_subset,
                                            ETableModel *source_model,
                                            gint col,
                                            gint row)
{
	gint view_row = table_subset_get_view_row (table_subset, row);

	if (view_row != -1)
		e_table_model_cell_changed (
			E_TABLE_MODEL (table_subset), col, view_row);
	else
		e_table_model_no_change (E_TABLE_MODEL (table_subset));
}

static void
table_subset_proxy_model_rows_inserted_real (ETableSubset *table_subset,
                                             ETableModel *source_model,
                                             gint row,
                                             gint count)
{
	e_table_model_no_change (E_TABLE_MODEL (table_subset));
}

static void
table_subset_proxy_model_rows_deleted_real (ETableSubset *table_subset,
                                            ETableModel *source_model,
                                            gint row,
                                            gint count)
{
	e_table_model_no_change (E_TABLE_MODEL (table_subset));
}

static gint
table_subset_column_count (ETableModel *table_model)
{
	ETableSubset *table_subset = (ETableSubset *) table_model;

	return e_table_model_column_count (table_subset->priv->source_model);
}

static gint
table_subset_row_count (ETableModel *table_model)
{
	ETableSubset *table_subset = (ETableSubset *) table_model;

	return table_subset->n_map;
}

static gpointer
table_subset_value_at (ETableModel *table_model,
                       gint col,
                       gint row)
{
	ETableSubset *table_subset = (ETableSubset *) table_model;

	g_return_val_if_fail (VALID_ROW (table_subset, row), NULL);

	table_subset->priv->last_access = row;

	return e_table_model_value_at (
		table_subset->priv->source_model,
		col, MAP_ROW (table_subset, row));
}

static void
table_subset_set_value_at (ETableModel *table_model,
                           gint col,
                           gint row,
                           gconstpointer val)
{
	ETableSubset *table_subset = (ETableSubset *) table_model;

	g_return_if_fail (VALID_ROW (table_subset, row));

	table_subset->priv->last_access = row;

	e_table_model_set_value_at (
		table_subset->priv->source_model,
		col, MAP_ROW (table_subset, row), val);
}

static gboolean
table_subset_is_cell_editable (ETableModel *table_model,
                               gint col,
                               gint row)
{
	ETableSubset *table_subset = (ETableSubset *) table_model;

	g_return_val_if_fail (VALID_ROW (table_subset, row), FALSE);

	return e_table_model_is_cell_editable (
		table_subset->priv->source_model,
		col, MAP_ROW (table_subset, row));
}

static gboolean
table_subset_has_save_id (ETableModel *table_model)
{
	return TRUE;
}

static gchar *
table_subset_get_save_id (ETableModel *table_model,
                          gint row)
{
	ETableSubset *table_subset = (ETableSubset *) table_model;

	g_return_val_if_fail (VALID_ROW (table_subset, row), NULL);

	if (e_table_model_has_save_id (table_subset->priv->source_model))
		return e_table_model_get_save_id (
			table_subset->priv->source_model,
			MAP_ROW (table_subset, row));
	else
		return g_strdup_printf ("%d", MAP_ROW (table_subset, row));
}

static void
table_subset_append_row (ETableModel *table_model,
                         ETableModel *source,
                         gint row)
{
	ETableSubset *table_subset = (ETableSubset *) table_model;

	e_table_model_append_row (
		table_subset->priv->source_model, source, row);
}

static gpointer
table_subset_duplicate_value (ETableModel *table_model,
                              gint col,
                              gconstpointer value)
{
	ETableSubset *table_subset = (ETableSubset *) table_model;

	return e_table_model_duplicate_value (
		table_subset->priv->source_model, col, value);
}

static void
table_subset_free_value (ETableModel *table_model,
                         gint col,
                         gpointer value)
{
	ETableSubset *table_subset = (ETableSubset *) table_model;

	e_table_model_free_value (
		table_subset->priv->source_model, col, value);
}

static gpointer
table_subset_initialize_value (ETableModel *table_model,
                               gint col)
{
	ETableSubset *table_subset = (ETableSubset *) table_model;

	return e_table_model_initialize_value (
		table_subset->priv->source_model, col);
}

static gboolean
table_subset_value_is_empty (ETableModel *table_model,
                             gint col,
                             gconstpointer value)
{
	ETableSubset *table_subset = (ETableSubset *) table_model;

	return e_table_model_value_is_empty (
		table_subset->priv->source_model, col, value);
}

static gchar *
table_subset_value_to_string (ETableModel *table_model,
                              gint col,
                              gconstpointer value)
{
	ETableSubset *table_subset = (ETableSubset *) table_model;

	return e_table_model_value_to_string (
		table_subset->priv->source_model, col, value);
}

static void
e_table_subset_class_init (ETableSubsetClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = table_subset_dispose;
	object_class->finalize = table_subset_finalize;

	class->proxy_model_pre_change = table_subset_proxy_model_pre_change_real;
	class->proxy_model_no_change = table_subset_proxy_model_no_change_real;
	class->proxy_model_changed = table_subset_proxy_model_changed_real;
	class->proxy_model_row_changed = table_subset_proxy_model_row_changed_real;
	class->proxy_model_cell_changed = table_subset_proxy_model_cell_changed_real;
	class->proxy_model_rows_inserted = table_subset_proxy_model_rows_inserted_real;
	class->proxy_model_rows_deleted = table_subset_proxy_model_rows_deleted_real;
}

static void
e_table_subset_table_model_init (ETableModelInterface *iface)
{
	iface->column_count = table_subset_column_count;
	iface->row_count = table_subset_row_count;
	iface->append_row = table_subset_append_row;

	iface->value_at = table_subset_value_at;
	iface->set_value_at = table_subset_set_value_at;
	iface->is_cell_editable = table_subset_is_cell_editable;

	iface->has_save_id = table_subset_has_save_id;
	iface->get_save_id = table_subset_get_save_id;

	iface->duplicate_value = table_subset_duplicate_value;
	iface->free_value = table_subset_free_value;
	iface->initialize_value = table_subset_initialize_value;
	iface->value_is_empty = table_subset_value_is_empty;
	iface->value_to_string = table_subset_value_to_string;
}

static void
e_table_subset_init (ETableSubset *table_subset)
{
	table_subset->priv = e_table_subset_get_instance_private (table_subset);
}

static void
table_subset_proxy_model_pre_change (ETableModel *source_model,
                                     ETableSubset *table_subset)
{
	ETableSubsetClass *class;

	class = E_TABLE_SUBSET_GET_CLASS (table_subset);

	if (class->proxy_model_pre_change != NULL)
		class->proxy_model_pre_change (table_subset, source_model);
}

static void
table_subset_proxy_model_no_change (ETableModel *source_model,
                                    ETableSubset *table_subset)
{
	ETableSubsetClass *class;

	class = E_TABLE_SUBSET_GET_CLASS (table_subset);

	if (class->proxy_model_no_change != NULL)
		class->proxy_model_no_change (table_subset, source_model);
}

static void
table_subset_proxy_model_changed (ETableModel *source_model,
                                  ETableSubset *table_subset)
{
	ETableSubsetClass *class;

	class = E_TABLE_SUBSET_GET_CLASS (table_subset);

	if (class->proxy_model_changed != NULL)
		class->proxy_model_changed (table_subset, source_model);
}

static void
table_subset_proxy_model_row_changed (ETableModel *source_model,
                                      gint row,
                                      ETableSubset *table_subset)
{
	ETableSubsetClass *class;

	class = E_TABLE_SUBSET_GET_CLASS (table_subset);

	if (class->proxy_model_row_changed != NULL)
		class->proxy_model_row_changed (
			table_subset, source_model, row);
}

static void
table_subset_proxy_model_cell_changed (ETableModel *source_model,
                                       gint col,
                                       gint row,
                                       ETableSubset *table_subset)
{
	ETableSubsetClass *class;

	class = E_TABLE_SUBSET_GET_CLASS (table_subset);

	if (class->proxy_model_cell_changed != NULL)
		class->proxy_model_cell_changed (
			table_subset, source_model, col, row);
}

static void
table_subset_proxy_model_rows_inserted (ETableModel *source_model,
                                        gint row,
                                        gint col,
                                        ETableSubset *table_subset)
{
	ETableSubsetClass *class;

	class = E_TABLE_SUBSET_GET_CLASS (table_subset);

	if (class->proxy_model_rows_inserted != NULL)
		class->proxy_model_rows_inserted (
			table_subset, source_model, row, col);
}

static void
table_subset_proxy_model_rows_deleted (ETableModel *source_model,
                                       gint row,
                                       gint col,
                                       ETableSubset *table_subset)
{
	ETableSubsetClass *class;

	class = E_TABLE_SUBSET_GET_CLASS (table_subset);

	if (class->proxy_model_rows_deleted != NULL)
		class->proxy_model_rows_deleted (
			table_subset, source_model, row, col);
}

ETableModel *
e_table_subset_construct (ETableSubset *table_subset,
                          ETableModel *source_model,
                          gint nvals)
{
	gulong handler_id;
	guint *buffer = NULL;
	gint i;

	if (nvals > 0)
		buffer = (guint *) g_malloc (sizeof (guint) * nvals);
	table_subset->map_table = (gint *) buffer;
	table_subset->n_map = nvals;
	table_subset->priv->source_model = g_object_ref (source_model);

	/* Init */
	for (i = 0; i < nvals; i++)
		table_subset->map_table[i] = i;

	handler_id = g_signal_connect (
		source_model, "model_pre_change",
		G_CALLBACK (table_subset_proxy_model_pre_change),
		table_subset);
	table_subset->priv->table_model_pre_change_handler_id = handler_id;

	handler_id = g_signal_connect (
		source_model, "model_no_change",
		G_CALLBACK (table_subset_proxy_model_no_change),
		table_subset);
	table_subset->priv->table_model_no_change_handler_id = handler_id;

	handler_id = g_signal_connect (
		source_model, "model_changed",
		G_CALLBACK (table_subset_proxy_model_changed),
		table_subset);
	table_subset->priv->table_model_changed_handler_id = handler_id;

	handler_id = g_signal_connect (
		source_model, "model_row_changed",
		G_CALLBACK (table_subset_proxy_model_row_changed),
		table_subset);
	table_subset->priv->table_model_row_changed_handler_id = handler_id;

	handler_id = g_signal_connect (
		source_model, "model_cell_changed",
		G_CALLBACK (table_subset_proxy_model_cell_changed),
		table_subset);
	table_subset->priv->table_model_cell_changed_handler_id = handler_id;

	handler_id = g_signal_connect (
		source_model, "model_rows_inserted",
		G_CALLBACK (table_subset_proxy_model_rows_inserted),
		table_subset);
	table_subset->priv->table_model_rows_inserted_handler_id = handler_id;

	handler_id = g_signal_connect (
		source_model, "model_rows_deleted",
		G_CALLBACK (table_subset_proxy_model_rows_deleted),
		table_subset);
	table_subset->priv->table_model_rows_deleted_handler_id = handler_id;

	return E_TABLE_MODEL (table_subset);
}

ETableModel *
e_table_subset_new (ETableModel *source_model,
                    const gint nvals)
{
	ETableSubset *table_subset;

	g_return_val_if_fail (E_IS_TABLE_MODEL (source_model), NULL);

	table_subset = g_object_new (E_TYPE_TABLE_SUBSET, NULL);

	if (e_table_subset_construct (table_subset, source_model, nvals) == NULL) {
		g_object_unref (table_subset);
		return NULL;
	}

	return (ETableModel *) table_subset;
}

ETableModel *
e_table_subset_get_source_model (ETableSubset *table_subset)
{
	g_return_val_if_fail (E_IS_TABLE_SUBSET (table_subset), NULL);

	return table_subset->priv->source_model;
}

gint
e_table_subset_model_to_view_row (ETableSubset *table_subset,
                                  gint model_row)
{
	gint i;

	g_return_val_if_fail (E_IS_TABLE_SUBSET (table_subset), -1);

	for (i = 0; i < table_subset->n_map; i++) {
		if (table_subset->map_table[i] == model_row)
			return i;
	}
	return -1;
}

gint
e_table_subset_view_to_model_row (ETableSubset *table_subset,
                                  gint view_row)
{
	g_return_val_if_fail (E_IS_TABLE_SUBSET (table_subset), -1);

	if (view_row >= 0 && view_row < table_subset->n_map)
		return table_subset->map_table[view_row];
	else
		return -1;
}

ETableModel *
e_table_subset_get_toplevel (ETableSubset *table_subset)
{
	g_return_val_if_fail (E_IS_TABLE_SUBSET (table_subset), NULL);

	if (E_IS_TABLE_SUBSET (table_subset->priv->source_model))
		return e_table_subset_get_toplevel (
			E_TABLE_SUBSET (table_subset->priv->source_model));
	else
		return table_subset->priv->source_model;
}

void
e_table_subset_print_debugging (ETableSubset *table_subset)
{
	gint i;

	g_return_if_fail (E_IS_TABLE_SUBSET (table_subset));

	for (i = 0; i < table_subset->n_map; i++) {
		g_print ("%8d\n", table_subset->map_table[i]);
	}
}
