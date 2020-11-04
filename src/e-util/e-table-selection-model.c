/*
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>

#include <gdk/gdkkeysyms.h>

#include <glib/gi18n.h>

#include "e-table-selection-model.h"

G_DEFINE_TYPE (
	ETableSelectionModel,
	e_table_selection_model,
	E_TYPE_SELECTION_MODEL_ARRAY)

static gint etsm_get_row_count (ESelectionModelArray *esm);

enum {
	PROP_0,
	PROP_MODEL,
	PROP_HEADER
};

static void
save_to_hash (gint model_row,
              gpointer closure)
{
	ETableSelectionModel *etsm = closure;
	const gchar *key = e_table_model_get_save_id (etsm->model, model_row);

	g_hash_table_insert (etsm->hash, (gpointer) key, (gpointer) key);
}

static void
free_hash (ETableSelectionModel *etsm)
{
	g_clear_pointer (&etsm->hash, g_hash_table_destroy);
	g_clear_pointer (&etsm->cursor_id, g_free);
}

static void
model_pre_change (ETableModel *etm,
                  ETableSelectionModel *etsm)
{
	free_hash (etsm);

	if (etsm->model && e_table_model_has_save_id (etsm->model)) {
		gint cursor_row;

		etsm->hash = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) NULL);
		e_selection_model_foreach (E_SELECTION_MODEL (etsm), save_to_hash, etsm);

		g_object_get (
			etsm,
			"cursor_row", &cursor_row,
			NULL);
		g_free (etsm->cursor_id);
		if (cursor_row != -1)
			etsm->cursor_id = e_table_model_get_save_id (etm, cursor_row);
		else
			etsm->cursor_id = NULL;
	}
}

static gint
model_changed_idle (ETableSelectionModel *etsm)
{
	ETableModel *etm = etsm->model;

	e_selection_model_clear (E_SELECTION_MODEL (etsm));

	if (etsm->cursor_id && etm && e_table_model_has_save_id (etm)) {
		gint row_count = e_table_model_row_count (etm);
		gint cursor_row = -1;
		gint cursor_col = -1;
		gint i;
		e_selection_model_array_confirm_row_count (E_SELECTION_MODEL_ARRAY (etsm));
		for (i = 0; i < row_count; i++) {
			gchar *save_id = e_table_model_get_save_id (etm, i);
			if (g_hash_table_lookup (etsm->hash, save_id))
				e_selection_model_change_one_row (E_SELECTION_MODEL (etsm), i, TRUE);

			if (etsm->cursor_id && !strcmp (etsm->cursor_id, save_id)) {
				cursor_row = i;
				cursor_col = e_selection_model_cursor_col (E_SELECTION_MODEL (etsm));
				if (cursor_col == -1) {
					if (etsm->eth) {
						cursor_col = e_table_header_prioritized_column (etsm->eth);
					} else
						cursor_col = 0;
				}
				e_selection_model_change_cursor (E_SELECTION_MODEL (etsm), cursor_row, cursor_col);
				g_free (etsm->cursor_id);
				etsm->cursor_id = NULL;
			}
			g_free (save_id);
		}
		free_hash (etsm);
		e_selection_model_selection_changed (E_SELECTION_MODEL (etsm));
		e_selection_model_cursor_changed (E_SELECTION_MODEL (etsm), cursor_row, cursor_col);
	}
	etsm->model_changed_idle_id = 0;
	return FALSE;
}

static void
model_changed (ETableModel *etm,
               ETableSelectionModel *etsm)
{
	e_selection_model_clear (E_SELECTION_MODEL (etsm));
	if (!etsm->model_changed_idle_id && etm && e_table_model_has_save_id (etm)) {
		etsm->model_changed_idle_id = g_idle_add_full (G_PRIORITY_HIGH, (GSourceFunc) model_changed_idle, etsm, NULL);
	}
}

static void
model_row_changed (ETableModel *etm,
                   gint row,
                   ETableSelectionModel *etsm)
{
	free_hash (etsm);
}

static void
model_cell_changed (ETableModel *etm,
                    gint col,
                    gint row,
                    ETableSelectionModel *etsm)
{
	free_hash (etsm);
}

#if 1
static void
model_rows_inserted (ETableModel *etm,
                     gint row,
                     gint count,
                     ETableSelectionModel *etsm)
{
	e_selection_model_array_insert_rows (E_SELECTION_MODEL_ARRAY (etsm), row, count);
	free_hash (etsm);
}

static void
model_rows_deleted (ETableModel *etm,
                    gint row,
                    gint count,
                    ETableSelectionModel *etsm)
{
	e_selection_model_array_delete_rows (E_SELECTION_MODEL_ARRAY (etsm), row, count);
	free_hash (etsm);
}

#else

static void
model_rows_inserted (ETableModel *etm,
                     gint row,
                     gint count,
                     ETableSelectionModel *etsm)
{
	model_changed (etm, etsm);
}

static void
model_rows_deleted (ETableModel *etm,
                    gint row,
                    gint count,
                    ETableSelectionModel *etsm)
{
	model_changed (etm, etsm);
}
#endif

inline static void
add_model (ETableSelectionModel *etsm,
           ETableModel *model)
{
	etsm->model = model;
	if (model) {
		g_object_ref (model);
		etsm->model_pre_change_id = g_signal_connect (
			model, "model_pre_change",
			G_CALLBACK (model_pre_change), etsm);
		etsm->model_changed_id = g_signal_connect (
			model, "model_changed",
			G_CALLBACK (model_changed), etsm);
		etsm->model_row_changed_id = g_signal_connect (
			model, "model_row_changed",
			G_CALLBACK (model_row_changed), etsm);
		etsm->model_cell_changed_id = g_signal_connect (
			model, "model_cell_changed",
			G_CALLBACK (model_cell_changed), etsm);
		etsm->model_rows_inserted_id = g_signal_connect (
			model, "model_rows_inserted",
			G_CALLBACK (model_rows_inserted), etsm);
		etsm->model_rows_deleted_id = g_signal_connect (
			model, "model_rows_deleted",
			G_CALLBACK (model_rows_deleted), etsm);
	}
	e_selection_model_array_confirm_row_count (E_SELECTION_MODEL_ARRAY (etsm));
}

inline static void
drop_model (ETableSelectionModel *etsm)
{
	if (etsm->model) {
		g_signal_handler_disconnect (
			etsm->model,
			etsm->model_pre_change_id);
		g_signal_handler_disconnect (
			etsm->model,
			etsm->model_changed_id);
		g_signal_handler_disconnect (
			etsm->model,
			etsm->model_row_changed_id);
		g_signal_handler_disconnect (
			etsm->model,
			etsm->model_cell_changed_id);
		g_signal_handler_disconnect (
			etsm->model,
			etsm->model_rows_inserted_id);
		g_signal_handler_disconnect (
			etsm->model,
			etsm->model_rows_deleted_id);

		g_object_unref (etsm->model);
	}
	etsm->model = NULL;
}

static void
etsm_dispose (GObject *object)
{
	ETableSelectionModel *etsm;

	etsm = E_TABLE_SELECTION_MODEL (object);

	if (etsm->model_changed_idle_id)
		g_source_remove (etsm->model_changed_idle_id);
	etsm->model_changed_idle_id = 0;

	drop_model (etsm);
	free_hash (etsm);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_table_selection_model_parent_class)->dispose (object);
}

static void
etsm_get_property (GObject *object,
                   guint property_id,
                   GValue *value,
                   GParamSpec *pspec)
{
	ETableSelectionModel *etsm = E_TABLE_SELECTION_MODEL (object);

	switch (property_id) {
	case PROP_MODEL:
		g_value_set_object (value, etsm->model);
		break;
	case PROP_HEADER:
		g_value_set_object (value, etsm->eth);
		break;
	}
}

static void
etsm_set_property (GObject *object,
                   guint property_id,
                   const GValue *value,
                   GParamSpec *pspec)
{
	ETableSelectionModel *etsm = E_TABLE_SELECTION_MODEL (object);

	switch (property_id) {
	case PROP_MODEL:
		drop_model (etsm);
		add_model (etsm, g_value_get_object (value) ? E_TABLE_MODEL (g_value_get_object (value)) : NULL);
		break;
	case PROP_HEADER:
		etsm->eth = E_TABLE_HEADER (g_value_get_object (value));
		break;
	}
}

static void
e_table_selection_model_init (ETableSelectionModel *selection)
{
	selection->model = NULL;
	selection->hash = NULL;
	selection->cursor_id = NULL;

	selection->model_changed_idle_id = 0;
}

static void
e_table_selection_model_class_init (ETableSelectionModelClass *class)
{
	GObjectClass *object_class;
	ESelectionModelArrayClass *esma_class;

	object_class = G_OBJECT_CLASS (class);
	esma_class = E_SELECTION_MODEL_ARRAY_CLASS (class);

	object_class->dispose = etsm_dispose;
	object_class->get_property = etsm_get_property;
	object_class->set_property = etsm_set_property;

	esma_class->get_row_count = etsm_get_row_count;

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			"Model",
			NULL,
			E_TYPE_TABLE_MODEL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_HEADER,
		g_param_spec_object (
			"header",
			"Header",
			NULL,
			E_TYPE_TABLE_HEADER,
			G_PARAM_READWRITE));
}

/**
 * e_table_selection_model_new
 *
 * This routine creates a new #ETableSelectionModel.
 *
 * Returns: The new #ETableSelectionModel.
 */
ETableSelectionModel *
e_table_selection_model_new (void)
{
	return g_object_new (E_TYPE_TABLE_SELECTION_MODEL, NULL);
}

static gint
etsm_get_row_count (ESelectionModelArray *esma)
{
	ETableSelectionModel *etsm = E_TABLE_SELECTION_MODEL (esma);

	if (etsm->model)
		return e_table_model_row_count (etsm->model);
	else
		return 0;
}
