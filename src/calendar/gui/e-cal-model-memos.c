/*
 * Evolution memos - Data model for ETable
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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *      Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>
#include "e-cal-model-memos.h"
#include "e-cell-date-edit-text.h"

#define d(x) (x)

/* Forward Declarations */
static void	e_cal_model_memos_table_model_init
					(ETableModelInterface *iface);

static ETableModelInterface *table_model_parent_interface;

G_DEFINE_TYPE_WITH_CODE (
	ECalModelMemos,
	e_cal_model_memos,
	E_TYPE_CAL_MODEL,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_TABLE_MODEL,
		e_cal_model_memos_table_model_init))

static void
cal_model_memos_store_values_from_model (ECalModel *model,
					 ETableModel *source_model,
					 gint row,
					 GHashTable *values)
{
	g_return_if_fail (E_IS_CAL_MODEL_MEMOS (model));
	g_return_if_fail (E_IS_TABLE_MODEL (source_model));
	g_return_if_fail (values != NULL);

	e_cal_model_util_set_value (values, source_model, E_CAL_MODEL_MEMOS_FIELD_STATUS, row);
}

static void
cal_model_memos_fill_component_from_values (ECalModel *model,
					    ECalModelComponent *comp_data,
					    GHashTable *values)
{
	ICalTime *dtstart;

	g_return_if_fail (E_IS_CAL_MODEL_MEMOS (model));
	g_return_if_fail (comp_data != NULL);
	g_return_if_fail (values != NULL);

	dtstart = i_cal_component_get_dtstart (comp_data->icalcomp);
	if (!dtstart || i_cal_time_is_null_time (dtstart) || !i_cal_time_is_valid_time (dtstart)) {
		g_clear_object (&dtstart);

		dtstart = i_cal_time_new_today ();
		i_cal_component_set_dtstart (comp_data->icalcomp, dtstart);
	}

	g_clear_object (&dtstart);

	e_cal_model_util_set_status (comp_data, e_cal_model_util_get_value (values, E_CAL_MODEL_MEMOS_FIELD_STATUS));
}

static gint
cal_model_memos_column_count (ETableModel *etm)
{
	return E_CAL_MODEL_MEMOS_FIELD_LAST;
}

static gpointer
cal_model_memos_value_at (ETableModel *etm,
                          gint col,
                          gint row)
{
	ECalModelComponent *comp_data;
	ECalModelMemos *model = (ECalModelMemos *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_MEMOS (model), NULL);

	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, NULL);
	g_return_val_if_fail (row >= 0 && row < e_table_model_row_count (etm), NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->value_at (etm, col, row);

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (!comp_data)
		return (gpointer) "";

	switch (col) {
	case E_CAL_MODEL_MEMOS_FIELD_STATUS:
		return e_cal_model_util_get_status (comp_data);
	}

	return (gpointer) "";
}

static void
cal_model_memos_set_value_at (ETableModel *etm,
                              gint col,
                              gint row,
                              gconstpointer value)
{
	ECalModelComponent *comp_data;
	ECalModelMemos *model = (ECalModelMemos *) etm;

	g_return_if_fail (E_IS_CAL_MODEL_MEMOS (model));
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST);
	g_return_if_fail (row >= 0 && row < e_table_model_row_count (etm));

	if (col < E_CAL_MODEL_FIELD_LAST) {
		table_model_parent_interface->set_value_at (etm, col, row, value);
		return;
	}

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (!comp_data) {
		g_warning ("couldn't get component data: row == %d", row);
		return;
	}

	switch (col) {
	case E_CAL_MODEL_MEMOS_FIELD_STATUS:
		e_cal_model_util_set_status (comp_data, value);
		break;
	}

	e_cal_model_modify_component (E_CAL_MODEL (model), comp_data, E_CAL_OBJ_MOD_ALL);
}

static gboolean
cal_model_memos_is_cell_editable (ETableModel *etm,
                                  gint col,
                                  gint row)
{
	ECalModelMemos *model = (ECalModelMemos *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_MEMOS (model), FALSE);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, FALSE);
	g_return_val_if_fail (row >= -1 || (row >= 0 && row < e_table_model_row_count (etm)), FALSE);

	if (!e_cal_model_test_row_editable (E_CAL_MODEL (etm), row))
		return FALSE;

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->is_cell_editable (etm, col, row);

	switch (col) {
	case E_CAL_MODEL_MEMOS_FIELD_STATUS:
		return TRUE;
	}

	return FALSE;
}

static gpointer
cal_model_memos_duplicate_value (ETableModel *etm,
                                 gint col,
                                 gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->duplicate_value (etm, col, value);

	switch (col) {
	case E_CAL_MODEL_MEMOS_FIELD_STATUS:
		return (gpointer) value;
	}

	return NULL;
}

static void
cal_model_memos_free_value (ETableModel *etm,
                            gint col,
                            gpointer value)
{
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST);

	if (col < E_CAL_MODEL_FIELD_LAST) {
		table_model_parent_interface->free_value (etm, col, value);
		return;
	}

	switch (col) {
	case E_CAL_MODEL_MEMOS_FIELD_STATUS:
		break;
	}
}

static gpointer
cal_model_memos_initialize_value (ETableModel *etm,
                                  gint col)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->initialize_value (etm, col);

	switch (col) {
	case E_CAL_MODEL_MEMOS_FIELD_STATUS:
		return (gpointer) "";
	}

	return NULL;
}

static gboolean
cal_model_memos_value_is_empty (ETableModel *etm,
                                gint col,
                                gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, TRUE);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->value_is_empty (etm, col, value);

	switch (col) {
	case E_CAL_MODEL_MEMOS_FIELD_STATUS:
		return e_str_is_empty (value);
	}

	return TRUE;
}

static gchar *
cal_model_memos_value_to_string (ETableModel *etm,
                                 gint col,
                                 gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, g_strdup (""));

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->value_to_string (etm, col, value);

	switch (col) {
	case E_CAL_MODEL_MEMOS_FIELD_STATUS:
		return g_strdup (value);
	}

	return g_strdup ("");
}

static void
e_cal_model_memos_class_init (ECalModelMemosClass *class)
{
	ECalModelClass *model_class;

	model_class = E_CAL_MODEL_CLASS (class);
	model_class->store_values_from_model = cal_model_memos_store_values_from_model;
	model_class->fill_component_from_values = cal_model_memos_fill_component_from_values;
}

static void
e_cal_model_memos_table_model_init (ETableModelInterface *iface)
{
	table_model_parent_interface =
		g_type_interface_peek_parent (iface);

	iface->column_count = cal_model_memos_column_count;

	iface->value_at = cal_model_memos_value_at;
	iface->set_value_at = cal_model_memos_set_value_at;
	iface->is_cell_editable = cal_model_memos_is_cell_editable;

	iface->duplicate_value = cal_model_memos_duplicate_value;
	iface->free_value = cal_model_memos_free_value;
	iface->initialize_value = cal_model_memos_initialize_value;
	iface->value_is_empty = cal_model_memos_value_is_empty;
	iface->value_to_string = cal_model_memos_value_to_string;
}

static void
e_cal_model_memos_init (ECalModelMemos *model)
{
	e_cal_model_set_component_kind (
		E_CAL_MODEL (model), I_CAL_VJOURNAL_COMPONENT);
}

ECalModel *
e_cal_model_memos_new (ECalDataModel *data_model,
		       ESourceRegistry *registry,
		       EShell *shell)
{
	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), NULL);
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return g_object_new (
		E_TYPE_CAL_MODEL_MEMOS,
		"data-model", data_model,
		"registry", registry,
		"shell", shell,
		NULL);
}

