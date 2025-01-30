/*
 * Evolution calendar - Data model for ETable
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>
#include "e-cal-model-calendar.h"
#include "e-cell-date-edit-text.h"
#include "itip-utils.h"
#include "e-cal-dialogs.h"

/* Forward Declarations */
static void	e_cal_model_calendar_table_model_init
					(ETableModelInterface *iface);

static ETableModelInterface *table_model_parent_interface;

G_DEFINE_TYPE_WITH_CODE (
	ECalModelCalendar,
	e_cal_model_calendar,
	E_TYPE_CAL_MODEL,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_TABLE_MODEL,
		e_cal_model_calendar_table_model_init))

static ECellDateEditValue *
get_dtend (ECalModelCalendar *model,
           ECalModelComponent *comp_data)
{
	if (!comp_data->dtend) {
		comp_data->dtend = e_cal_model_util_get_datetime_value (E_CAL_MODEL (model), comp_data,
			I_CAL_DTEND_PROPERTY, i_cal_property_get_dtend);
	}

	return e_cell_date_edit_value_copy (comp_data->dtend);
}

static gpointer
get_location (ECalModelComponent *comp_data)
{
	ICalProperty *prop;
	const gchar *res = NULL;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_LOCATION_PROPERTY);
	if (prop) {
		res = i_cal_property_get_location (prop);
		g_clear_object (&prop);
	}

	if (!res)
		res = "";

	return (gpointer) res;
}

static gpointer
get_transparency (ECalModelComponent *comp_data)
{
	ICalProperty *prop;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_TRANSP_PROPERTY);
	if (prop) {
		ICalPropertyTransp transp;
		const gchar *res = NULL;

		transp = i_cal_property_get_transp (prop);
		if (transp == I_CAL_TRANSP_TRANSPARENT ||
		    transp == I_CAL_TRANSP_TRANSPARENTNOCONFLICT)
			res = _("Free");
		else if (transp == I_CAL_TRANSP_OPAQUE ||
			 transp == I_CAL_TRANSP_OPAQUENOCONFLICT)
			res = _("Busy");

		g_clear_object (&prop);

		return (gpointer) res;
	}

	return NULL;
}

static void
set_dtend (ECalModel *model,
           ECalModelComponent *comp_data,
           gconstpointer value)
{
	e_cal_model_update_comp_time (model, comp_data, value, I_CAL_DTEND_PROPERTY, i_cal_property_set_dtend, i_cal_property_new_dtend);
	e_cal_util_component_remove_property_by_kind (comp_data->icalcomp, I_CAL_DURATION_PROPERTY, TRUE);
}

static void
set_location (ECalModelComponent *comp_data,
              gconstpointer value)
{
	ICalProperty *prop;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_LOCATION_PROPERTY);

	if (e_str_is_empty (value)) {
		if (prop) {
			i_cal_component_remove_property (comp_data->icalcomp, prop);
			g_object_unref (prop);
		}
	} else {
		if (prop) {
			i_cal_property_set_location (prop, (const gchar *) value);
			g_object_unref (prop);
		} else {
			prop = i_cal_property_new_location ((const gchar *) value);
			i_cal_component_take_property (comp_data->icalcomp, prop);
		}
	}
}

static void
set_transparency (ECalModelComponent *comp_data,
                  gconstpointer value)
{
	ICalProperty *prop;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_TRANSP_PROPERTY);

	if (e_str_is_empty (value)) {
		if (prop) {
			i_cal_component_remove_property (comp_data->icalcomp, prop);
			g_object_unref (prop);
		}
	} else {
		ICalPropertyTransp transp;

		if (!g_ascii_strcasecmp (value, "FREE"))
			transp = I_CAL_TRANSP_TRANSPARENT;
		else if (!g_ascii_strcasecmp (value, "OPAQUE"))
			transp = I_CAL_TRANSP_OPAQUE;
		else {
			if (prop) {
				i_cal_component_remove_property (comp_data->icalcomp, prop);
				g_object_unref (prop);
			}

			return;
		}

		if (prop) {
			i_cal_property_set_transp (prop, transp);
			g_object_unref (prop);
		} else {
			prop = i_cal_property_new_transp (transp);
			i_cal_component_take_property (comp_data->icalcomp, prop);
		}
	}
}

static void
cal_model_calendar_store_values_from_model (ECalModel *model,
					    ETableModel *source_model,
					    gint row,
					    GHashTable *values)
{
	g_return_if_fail (E_IS_CAL_MODEL_CALENDAR (model));
	g_return_if_fail (E_IS_TABLE_MODEL (source_model));
	g_return_if_fail (values != NULL);

	e_cal_model_util_set_value (values, source_model, E_CAL_MODEL_CALENDAR_FIELD_DTEND, row);
	e_cal_model_util_set_value (values, source_model, E_CAL_MODEL_CALENDAR_FIELD_LOCATION, row);
	e_cal_model_util_set_value (values, source_model, E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY, row);
	e_cal_model_util_set_value (values, source_model, E_CAL_MODEL_CALENDAR_FIELD_STATUS, row);
}

static void
cal_model_calendar_fill_component_from_values (ECalModel *model,
					       ECalModelComponent *comp_data,
					       GHashTable *values)
{
	g_return_if_fail (E_IS_CAL_MODEL_CALENDAR (model));
	g_return_if_fail (comp_data != NULL);
	g_return_if_fail (values != NULL);

	set_dtend (model, comp_data, e_cal_model_util_get_value (values, E_CAL_MODEL_CALENDAR_FIELD_DTEND));
	set_location (comp_data, e_cal_model_util_get_value (values, E_CAL_MODEL_CALENDAR_FIELD_LOCATION));
	set_transparency (comp_data, e_cal_model_util_get_value (values, E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY));
	e_cal_model_util_set_status (comp_data, e_cal_model_util_get_value (values, E_CAL_MODEL_CALENDAR_FIELD_STATUS));
}

static gint
cal_model_calendar_column_count (ETableModel *etm)
{
	return E_CAL_MODEL_CALENDAR_FIELD_LAST;
}

static gpointer
cal_model_calendar_value_at (ETableModel *etm,
                             gint col,
                             gint row)
{
	ECalModelComponent *comp_data;
	ECalModelCalendar *model = (ECalModelCalendar *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_CALENDAR (model), NULL);

	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, NULL);
	g_return_val_if_fail (row >= 0 && row < e_table_model_row_count (etm), NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->value_at (etm, col, row);

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (!comp_data)
		return (gpointer) "";

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		return get_dtend (model, comp_data);
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
		return get_location (comp_data);
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		return get_transparency (comp_data);
	case E_CAL_MODEL_CALENDAR_FIELD_STATUS:
		return e_cal_model_util_get_status (comp_data);
	}

	return (gpointer) "";
}

static void
cal_model_calendar_set_value_at (ETableModel *etm,
                                 gint col,
                                 gint row,
                                 gconstpointer value)
{
	ECalModelComponent *comp_data;
	ECalObjModType mod = E_CAL_OBJ_MOD_ALL;
	ECalComponent *comp;
	ECalModelCalendar *model = (ECalModelCalendar *) etm;

	g_return_if_fail (E_IS_CAL_MODEL_CALENDAR (model));
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST);
	g_return_if_fail (row >= 0 && row < e_table_model_row_count (etm));

	if (col < E_CAL_MODEL_FIELD_LAST) {
		table_model_parent_interface->set_value_at (etm, col, row, value);
		return;
	}

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (!comp_data)
		return;

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (comp_data->icalcomp));
	if (!comp) {
		return;
	}

	/* ask about mod type */
	if (e_cal_component_is_instance (comp)) {
		if (!e_cal_dialogs_recur_component (comp_data->client, comp, &mod, NULL, FALSE)) {
			g_object_unref (comp);
			return;
		}
	}

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		set_dtend ((ECalModel *) model, comp_data, value);
		break;
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
		set_location (comp_data, value);
		break;
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		set_transparency (comp_data, value);
		break;
	case E_CAL_MODEL_CALENDAR_FIELD_STATUS:
		e_cal_model_util_set_status (comp_data, value);
		break;
	}

	e_cal_model_modify_component (E_CAL_MODEL (model), comp_data, mod);

	g_object_unref (comp);
}

static gboolean
cal_model_calendar_is_cell_editable (ETableModel *etm,
                                     gint col,
                                     gint row)
{
	ECalModelCalendar *model = (ECalModelCalendar *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_CALENDAR (model), FALSE);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, FALSE);
	g_return_val_if_fail (row >= -1 || (row >= 0 && row < e_table_model_row_count (etm)), FALSE);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->is_cell_editable (etm, col, row);

	if (!e_cal_model_test_row_editable (E_CAL_MODEL (etm), row))
		return FALSE;

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
	case E_CAL_MODEL_CALENDAR_FIELD_STATUS:
		return TRUE;
	}

	return FALSE;
}

static gpointer
cal_model_calendar_duplicate_value (ETableModel *etm,
                                    gint col,
                                    gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->duplicate_value (etm, col, value);

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		return e_cell_date_edit_value_copy (value);
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		return g_strdup (value);
	case E_CAL_MODEL_CALENDAR_FIELD_STATUS:
		return (gpointer) value;
	}

	return NULL;
}

static void
cal_model_calendar_free_value (ETableModel *etm,
                               gint col,
                               gpointer value)
{
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST);

	if (col < E_CAL_MODEL_FIELD_LAST) {
		table_model_parent_interface->free_value (etm, col, value);
		return;
	}

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		e_cell_date_edit_value_free (value);
		break;
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
	case E_CAL_MODEL_CALENDAR_FIELD_STATUS:
		break;
	}
}

static gpointer
cal_model_calendar_initialize_value (ETableModel *etm,
                                     gint col)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->initialize_value (etm, col);

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		return NULL;
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		return g_strdup ("");
	case E_CAL_MODEL_CALENDAR_FIELD_STATUS:
		return (gpointer) "";
	}

	return NULL;
}

static gboolean
cal_model_calendar_value_is_empty (ETableModel *etm,
                                   gint col,
                                   gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, TRUE);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->value_is_empty (etm, col, value);

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		return value ? FALSE : TRUE;
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
	case E_CAL_MODEL_CALENDAR_FIELD_STATUS:
		return e_str_is_empty (value);
	}

	return TRUE;
}

static gchar *
cal_model_calendar_value_to_string (ETableModel *etm,
                                    gint col,
                                    gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, g_strdup (""));

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->value_to_string (etm, col, value);

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		return e_cal_model_date_value_to_string (E_CAL_MODEL (etm), value);
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
	case E_CAL_MODEL_CALENDAR_FIELD_STATUS:
		return g_strdup (value);
	}

	return g_strdup ("");
}

static void
e_cal_model_calendar_class_init (ECalModelCalendarClass *class)
{
	ECalModelClass *model_class;

	model_class = E_CAL_MODEL_CLASS (class);
	model_class->store_values_from_model = cal_model_calendar_store_values_from_model;
	model_class->fill_component_from_values = cal_model_calendar_fill_component_from_values;
}

static void
e_cal_model_calendar_table_model_init (ETableModelInterface *iface)
{
	table_model_parent_interface =
		g_type_interface_peek_parent (iface);

	iface->column_count = cal_model_calendar_column_count;
	iface->value_at = cal_model_calendar_value_at;
	iface->set_value_at = cal_model_calendar_set_value_at;
	iface->is_cell_editable = cal_model_calendar_is_cell_editable;
	iface->duplicate_value = cal_model_calendar_duplicate_value;
	iface->free_value = cal_model_calendar_free_value;
	iface->initialize_value = cal_model_calendar_initialize_value;
	iface->value_is_empty = cal_model_calendar_value_is_empty;
	iface->value_to_string = cal_model_calendar_value_to_string;
}

static void
e_cal_model_calendar_init (ECalModelCalendar *model)
{
	e_cal_model_set_component_kind (
		E_CAL_MODEL (model), I_CAL_VEVENT_COMPONENT);
}

ECalModel *
e_cal_model_calendar_new (ECalDataModel *data_model,
			  ESourceRegistry *registry,
			  EShell *shell)
{
	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), NULL);
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return g_object_new (
		E_TYPE_CAL_MODEL_CALENDAR,
		"data-model", data_model,
		"registry", registry,
		"shell", shell,
		NULL);
}

