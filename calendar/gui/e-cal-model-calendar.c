/* Evolution calendar - Data model for ETable
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Rodrigo Moya <rodrigo@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <string.h>
#include <libgnome/gnome-i18n.h>
#include <gal/util/e-util.h>
#include "e-cal-model-calendar.h"
#include "e-cell-date-edit-text.h"
#include "misc.h"

struct _ECalModelCalendarPrivate {
};

static void ecmc_class_init (ECalModelCalendarClass *klass);
static void ecmc_init (ECalModelCalendar *model, ECalModelCalendarClass *klass);
static void ecmc_finalize (GObject *object);
static int ecmc_column_count (ETableModel *etm);
static void *ecmc_value_at (ETableModel *etm, int col, int row);
static void ecmc_set_value_at (ETableModel *etm, int col, int row, const void *value);
static gboolean ecmc_is_cell_editable (ETableModel *etm, int col, int row);
static void *ecmc_duplicate_value (ETableModel *etm, int col, const void *value);
static void ecmc_free_value (ETableModel *etm, int col, void *value);
static void *ecmc_initialize_value (ETableModel *etm, int col);
static gboolean ecmc_value_is_empty (ETableModel *etm, int col, const void *value);
static char *ecmc_value_to_string (ETableModel *etm, int col, const void *value);

static void ecmc_fill_component_from_model (ECalModel *model, ECalModelComponent *comp_data,
					    ETableModel *source_model, gint row);

static GObjectClass *parent_class = NULL;

E_MAKE_TYPE (e_cal_model_calendar, "ECalModelCalendar", ECalModelCalendar, ecmc_class_init,
	     ecmc_init, E_TYPE_CAL_MODEL);

static void
ecmc_class_init (ECalModelCalendarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ETableModelClass *etm_class = E_TABLE_MODEL_CLASS (klass);
	ECalModelClass *model_class = E_CAL_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ecmc_finalize;

	etm_class->column_count = ecmc_column_count;
	etm_class->value_at = ecmc_value_at;
	etm_class->set_value_at = ecmc_set_value_at;
	etm_class->is_cell_editable = ecmc_is_cell_editable;
	etm_class->duplicate_value = ecmc_duplicate_value;
	etm_class->free_value = ecmc_free_value;
	etm_class->initialize_value = ecmc_initialize_value;
	etm_class->value_is_empty = ecmc_value_is_empty;
	etm_class->value_to_string = ecmc_value_to_string;

	model_class->fill_component_from_model = ecmc_fill_component_from_model;
}

static void
ecmc_init (ECalModelCalendar *model, ECalModelCalendarClass *klass)
{
	ECalModelCalendarPrivate *priv;

	priv = g_new0 (ECalModelCalendarPrivate, 1);
	model->priv = priv;

	e_cal_model_set_component_kind (E_CAL_MODEL (model), ICAL_VEVENT_COMPONENT);
}

static void
ecmc_finalize (GObject *object)
{
	ECalModelCalendarPrivate *priv;
	ECalModelCalendar *model = (ECalModelCalendar *) object;

	g_return_if_fail (E_IS_CAL_MODEL_CALENDAR (model));

	priv = model->priv;
	if (priv) {
		g_free (priv);
		model->priv = NULL;
	}

	if (parent_class->finalize)
		parent_class->finalize (object);
}

/* ETableModel methods */
static int
ecmc_column_count (ETableModel *etm)
{
	return E_CAL_MODEL_CALENDAR_FIELD_LAST;
}

static ECellDateEditValue *
get_dtend (ECalModelComponent *comp_data)
{
	struct icaltimetype tt_end;

	if (!comp_data->dtend) {
		icaltimezone *zone;

		tt_end = icalcomponent_get_dtend (comp_data->icalcomp);
		if (!icaltime_is_valid_time (tt_end))
			return NULL;

		comp_data->dtend = g_new0 (ECellDateEditValue, 1);
		comp_data->dtend->tt = tt_end;

		if (icaltime_get_tzid (tt_end)
		    && e_cal_get_timezone (comp_data->client, icaltime_get_tzid (tt_end), &zone, NULL)) 
			comp_data->dtend->zone = zone;
		else
			comp_data->dtend->zone = NULL;
	}

	return comp_data->dtend;
}

static void *
get_location (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_LOCATION_PROPERTY);
	if (prop)
		return (void *) icalproperty_get_location (prop);

	return NULL;
}

static void *
get_transparency (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_TRANSP_PROPERTY);
	if (prop) {
		icalproperty_transp transp;

		transp = icalproperty_get_transp (prop);
		if (transp == ICAL_TRANSP_TRANSPARENT ||
		    transp == ICAL_TRANSP_TRANSPARENTNOCONFLICT)
			return _("Free");
		else if (transp == ICAL_TRANSP_OPAQUE ||
			 transp == ICAL_TRANSP_OPAQUENOCONFLICT)
			return _("Busy");
	}

	return NULL;
}

static void *
ecmc_value_at (ETableModel *etm, int col, int row)
{
	ECalModelComponent *comp_data;
	ECalModelCalendarPrivate *priv;
	ECalModelCalendar *model = (ECalModelCalendar *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_CALENDAR (model), NULL);

	priv = model->priv;

	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, NULL);
	g_return_val_if_fail (row >= 0 && row < e_table_model_row_count (etm), NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (parent_class)->value_at (etm, col, row);

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (!comp_data)
		return "";

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		return get_dtend (comp_data);
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
		return get_location (comp_data);
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		return get_transparency (comp_data);
	}

	return "";
}

static void
set_dtend (ECalModelComponent *comp_data, const void *value)
{
	icalproperty *prop;
	ECellDateEditValue *dv = (ECellDateEditValue *) value;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DTEND_PROPERTY);
	if (!dv) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
	} else
		icalcomponent_set_dtend (comp_data->icalcomp, dv->tt);
}

static void
set_location (ECalModelComponent *comp_data, const void *value)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_LOCATION_PROPERTY);

	if (string_is_empty (value)) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
	} else {
		if (prop)
			icalproperty_set_location (prop, (const char *) value);
		else {
			prop = icalproperty_new_location ((const char *) value);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		}
	}
}

static void
set_transparency (ECalModelComponent *comp_data, const void *value)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_TRANSP_PROPERTY);

	if (string_is_empty (value)) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
	} else {
		icalproperty_transp transp;

		if (!strcasecmp (value, "FREE"))
			transp = ICAL_TRANSP_TRANSPARENT;
		else if (!strcasecmp (value, "OPAQUE"))
			transp = ICAL_TRANSP_OPAQUE;
		else {
			if (prop) {
				icalcomponent_remove_property (comp_data->icalcomp, prop);
				icalproperty_free (prop);
			}

			return;
		}

		if (prop)
			icalproperty_set_transp (prop, transp);
		else {
			prop = icalproperty_new_transp (transp);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		}
	}
}

static void
ecmc_set_value_at (ETableModel *etm, int col, int row, const void *value)
{
	ECalModelComponent *comp_data;
	ECalModelCalendar *model = (ECalModelCalendar *) etm;

	g_return_if_fail (E_IS_CAL_MODEL_CALENDAR (model));
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST);
	g_return_if_fail (row >= 0 && row < e_table_model_row_count (etm));

	if (col < E_CAL_MODEL_FIELD_LAST) {
		E_TABLE_MODEL_CLASS (parent_class)->set_value_at (etm, col, row, value);
		return;
	}

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (!comp_data)
		return;

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		set_dtend (comp_data, value);
		break;
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
		set_location (comp_data, value);
		break;
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		set_transparency (comp_data, value);
		break;
	}

	/* FIXME ask about mod type */
	if (!e_cal_modify_object (comp_data->client, comp_data->icalcomp, CALOBJ_MOD_ALL, NULL)) {
		g_warning (G_STRLOC ": Could not modify the object!");
		
		/* FIXME Show error dialog */
	}
}

static gboolean
ecmc_is_cell_editable (ETableModel *etm, int col, int row)
{
	ECalModelCalendar *model = (ECalModelCalendar *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_CALENDAR (model), FALSE);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, FALSE);

	/* FIXME: We can't check this as 'click-to-add' passes row 0. */
	/* g_return_val_if_fail (row >= 0 && row < e_table_model_get_row_count (etm), FALSE); */

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (parent_class)->is_cell_editable (etm, col, row);

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		return TRUE;
	}

	return FALSE;
}

static void *
ecmc_duplicate_value (ETableModel *etm, int col, const void *value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (parent_class)->duplicate_value (etm, col, value);

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		if (value) {
			ECellDateEditValue *dv, *orig_dv;

			orig_dv = (ECellDateEditValue *) value;
			dv = g_new0 (ECellDateEditValue, 1);
			*dv = *orig_dv;

			return dv;
		}
		break;
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		return g_strdup (value);
	}

	return NULL;
}

static void
ecmc_free_value (ETableModel *etm, int col, void *value)
{
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST);

	if (col < E_CAL_MODEL_FIELD_LAST) {
		E_TABLE_MODEL_CLASS (parent_class)->free_value (etm, col, value);
		return;
	}

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		if (value)
			g_free (value);
		break;
	}
}

static void *
ecmc_initialize_value (ETableModel *etm, int col)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (parent_class)->initialize_value (etm, col);

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		return NULL;
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		return g_strdup ("");
	}

	return NULL;
}

static gboolean
ecmc_value_is_empty (ETableModel *etm, int col, const void *value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, TRUE);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (parent_class)->value_is_empty (etm, col, value);

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		return value ? FALSE : TRUE;
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		return string_is_empty (value);
	}

	return TRUE;
}

static char *
ecmc_value_to_string (ETableModel *etm, int col, const void *value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (parent_class)->value_to_string (etm, col, value);

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		return e_cal_model_date_value_to_string (E_CAL_MODEL (etm), value);
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		return g_strdup (value);
	}

	return NULL;
}

/* ECalModel class methods */

static void
ecmc_fill_component_from_model (ECalModel *model, ECalModelComponent *comp_data,
				ETableModel *source_model, gint row)
{
	g_return_if_fail (E_IS_CAL_MODEL_CALENDAR (model));
	g_return_if_fail (comp_data != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (source_model));

	set_dtend (comp_data,
		   e_table_model_value_at (source_model, E_CAL_MODEL_CALENDAR_FIELD_DTEND, row));
	set_location (comp_data,
		      e_table_model_value_at (source_model, E_CAL_MODEL_CALENDAR_FIELD_LOCATION, row));
	set_transparency (comp_data,
			  e_table_model_value_at (source_model, E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY, row));
}

/**
 * e_cal_model_calendar_new
 */
ECalModelCalendar *
e_cal_model_calendar_new (void)
{
	return g_object_new (E_TYPE_CAL_MODEL_CALENDAR, NULL);
}
