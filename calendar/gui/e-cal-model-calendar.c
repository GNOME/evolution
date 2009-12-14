/*
 * Evolution calendar - Data model for ETable
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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include "e-cal-model-calendar.h"
#include "e-cell-date-edit-text.h"
#include "itip-utils.h"
#include "misc.h"
#include "dialogs/recur-comp.h"
#include "dialogs/send-comp.h"

struct _ECalModelCalendarPrivate {
	guint reserved;
};

static void e_cal_model_calendar_finalize (GObject *object);
static gint ecmc_column_count (ETableModel *etm);
static gpointer ecmc_value_at (ETableModel *etm, gint col, gint row);
static void ecmc_set_value_at (ETableModel *etm, gint col, gint row, gconstpointer value);
static gboolean ecmc_is_cell_editable (ETableModel *etm, gint col, gint row);
static gpointer ecmc_duplicate_value (ETableModel *etm, gint col, gconstpointer value);
static void ecmc_free_value (ETableModel *etm, gint col, gpointer value);
static gpointer ecmc_initialize_value (ETableModel *etm, gint col);
static gboolean ecmc_value_is_empty (ETableModel *etm, gint col, gconstpointer value);
static gchar *ecmc_value_to_string (ETableModel *etm, gint col, gconstpointer value);

static void ecmc_fill_component_from_model (ECalModel *model, ECalModelComponent *comp_data,
					    ETableModel *source_model, gint row);

G_DEFINE_TYPE (ECalModelCalendar, e_cal_model_calendar, E_TYPE_CAL_MODEL)

static void
e_cal_model_calendar_class_init (ECalModelCalendarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ETableModelClass *etm_class = E_TABLE_MODEL_CLASS (klass);
	ECalModelClass *model_class = E_CAL_MODEL_CLASS (klass);

	object_class->finalize = e_cal_model_calendar_finalize;

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
e_cal_model_calendar_init (ECalModelCalendar *model)
{
	ECalModelCalendarPrivate *priv;

	priv = g_new0 (ECalModelCalendarPrivate, 1);
	model->priv = priv;

	e_cal_model_set_component_kind (E_CAL_MODEL (model), ICAL_VEVENT_COMPONENT);
}

static void
e_cal_model_calendar_finalize (GObject *object)
{
	ECalModelCalendarPrivate *priv;
	ECalModelCalendar *model = (ECalModelCalendar *) object;

	g_return_if_fail (E_IS_CAL_MODEL_CALENDAR (model));

	priv = model->priv;
	if (priv) {
		g_free (priv);
		model->priv = NULL;
	}

	if (G_OBJECT_CLASS (e_cal_model_calendar_parent_class)->finalize)
		G_OBJECT_CLASS (e_cal_model_calendar_parent_class)->finalize (object);
}

/* ETableModel methods */
static gint
ecmc_column_count (ETableModel *etm)
{
	return E_CAL_MODEL_CALENDAR_FIELD_LAST;
}

static ECellDateEditValue *
get_dtend (ECalModelCalendar *model, ECalModelComponent *comp_data)
{
	struct icaltimetype tt_end;

	if (!comp_data->dtend) {
		icalproperty *prop;
		icaltimezone *zone = NULL, *model_zone = NULL;
		gboolean got_zone = FALSE;

		prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DTEND_PROPERTY);
		if (!prop)
			return NULL;

		tt_end = icalproperty_get_dtend (prop);

		if (icaltime_get_tzid (tt_end)
		    && e_cal_get_timezone (comp_data->client, icaltime_get_tzid (tt_end), &zone, NULL))
			got_zone = TRUE;

		model_zone = e_cal_model_get_timezone (E_CAL_MODEL (model));

		if (e_cal_model_get_flags (E_CAL_MODEL (model)) & E_CAL_MODEL_FLAGS_EXPAND_RECURRENCES) {
			if (got_zone) {
				tt_end = icaltime_from_timet_with_zone (comp_data->instance_end, tt_end.is_date, zone);
				if (model_zone)
					icaltimezone_convert_time (&tt_end, zone, model_zone);
			} else
				tt_end = icaltime_from_timet_with_zone (comp_data->instance_end, tt_end.is_date,
						model_zone);
		}

		if (!icaltime_is_valid_time (tt_end) || icaltime_is_null_time (tt_end))
			return NULL;

		comp_data->dtend = g_new0 (ECellDateEditValue, 1);
		comp_data->dtend->tt = tt_end;

		if (got_zone)
			comp_data->dtend->zone = zone;
		else
			comp_data->dtend->zone = NULL;
	}

	return comp_data->dtend;
}

static gpointer
get_location (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_LOCATION_PROPERTY);
	if (prop)
		return (gpointer) icalproperty_get_location (prop);

	return (gpointer) "";
}

static gpointer
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

static gpointer
ecmc_value_at (ETableModel *etm, gint col, gint row)
{
	ECalModelComponent *comp_data;
	ECalModelCalendar *model = (ECalModelCalendar *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_CALENDAR (model), NULL);

	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, NULL);
	g_return_val_if_fail (row >= 0 && row < e_table_model_row_count (etm), NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_calendar_parent_class)->value_at (etm, col, row);

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
	}

	return (gpointer) "";
}

static void
set_dtend (ECalModel* model, ECalModelComponent *comp_data, gconstpointer value)
{
	e_cal_model_update_comp_time (model, comp_data, value, ICAL_DTEND_PROPERTY, icalproperty_set_dtend, icalproperty_new_dtend);
}

static void
set_location (ECalModelComponent *comp_data, gconstpointer value)
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
			icalproperty_set_location (prop, (const gchar *) value);
		else {
			prop = icalproperty_new_location ((const gchar *) value);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		}
	}
}

static void
set_transparency (ECalModelComponent *comp_data, gconstpointer value)
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

		if (!g_ascii_strcasecmp (value, "FREE"))
			transp = ICAL_TRANSP_TRANSPARENT;
		else if (!g_ascii_strcasecmp (value, "OPAQUE"))
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
ecmc_set_value_at (ETableModel *etm, gint col, gint row, gconstpointer value)
{
	ECalModelComponent *comp_data;
	CalObjModType mod = CALOBJ_MOD_ALL;
	ECalComponent *comp;
	ECalModelCalendar *model = (ECalModelCalendar *) etm;

	g_return_if_fail (E_IS_CAL_MODEL_CALENDAR (model));
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST);
	g_return_if_fail (row >= 0 && row < e_table_model_row_count (etm));

	if (col < E_CAL_MODEL_FIELD_LAST) {
		E_TABLE_MODEL_CLASS (e_cal_model_calendar_parent_class)->set_value_at (etm, col, row, value);
		return;
	}

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (!comp_data)
		return;

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp))) {
		g_object_unref (comp);
		return;
	}

	/* ask about mod type */
	if (e_cal_component_is_instance (comp)) {
		if (!recur_component_dialog (comp_data->client, comp, &mod, NULL, FALSE)) {
			g_object_unref (comp);
			return;
		}
	}

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		set_dtend ((ECalModel *)model, comp_data, value);
		break;
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
		set_location (comp_data, value);
		break;
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		set_transparency (comp_data, value);
		break;
	}

	if (e_cal_modify_object (comp_data->client, comp_data->icalcomp, mod, NULL)) {
		gboolean strip_alarms = TRUE;

		if (itip_organizer_is_user (comp, comp_data->client) &&
		    send_component_dialog (NULL, comp_data->client, comp, FALSE, &strip_alarms, NULL)) {
			ECalComponent *send_comp = NULL;

			if (mod == CALOBJ_MOD_ALL && e_cal_component_is_instance (comp)) {
				/* Ensure we send the master object, not the instance only */
				icalcomponent *icalcomp = NULL;
				const gchar *uid = NULL;

				e_cal_component_get_uid (comp, &uid);
				if (e_cal_get_object (comp_data->client, uid, NULL, &icalcomp, NULL) && icalcomp) {
					send_comp = e_cal_component_new ();
					if (!e_cal_component_set_icalcomponent (send_comp, icalcomp)) {
						icalcomponent_free (icalcomp);
						g_object_unref (send_comp);
						send_comp = NULL;
					}
				}
			}

			itip_send_comp (E_CAL_COMPONENT_METHOD_REQUEST, send_comp ? send_comp : comp,
					comp_data->client, NULL, NULL, NULL, strip_alarms, FALSE);

			if (send_comp)
				g_object_unref (send_comp);
		}
	} else {
		g_warning (G_STRLOC ": Could not modify the object!");

		/* FIXME Show error dialog */
	}

	g_object_unref (comp);
}

static gboolean
ecmc_is_cell_editable (ETableModel *etm, gint col, gint row)
{
	ECalModelCalendar *model = (ECalModelCalendar *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_CALENDAR (model), FALSE);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, FALSE);
	g_return_val_if_fail (row >= -1 || (row >= 0 && row < e_table_model_row_count (etm)), FALSE);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_calendar_parent_class)->is_cell_editable (etm, col, row);

	if (!e_cal_model_test_row_editable (E_CAL_MODEL (etm), row))
		return FALSE;

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		return TRUE;
	}

	return FALSE;
}

static gpointer
ecmc_duplicate_value (ETableModel *etm, gint col, gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_calendar_parent_class)->duplicate_value (etm, col, value);

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
ecmc_free_value (ETableModel *etm, gint col, gpointer value)
{
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST);

	if (col < E_CAL_MODEL_FIELD_LAST) {
		E_TABLE_MODEL_CLASS (e_cal_model_calendar_parent_class)->free_value (etm, col, value);
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

static gpointer
ecmc_initialize_value (ETableModel *etm, gint col)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_calendar_parent_class)->initialize_value (etm, col);

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
ecmc_value_is_empty (ETableModel *etm, gint col, gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, TRUE);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_calendar_parent_class)->value_is_empty (etm, col, value);

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		return value ? FALSE : TRUE;
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		return string_is_empty (value);
	}

	return TRUE;
}

static gchar *
ecmc_value_to_string (ETableModel *etm, gint col, gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_CALENDAR_FIELD_LAST, g_strdup (""));

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_calendar_parent_class)->value_to_string (etm, col, value);

	switch (col) {
	case E_CAL_MODEL_CALENDAR_FIELD_DTEND :
		return e_cal_model_date_value_to_string (E_CAL_MODEL (etm), value);
	case E_CAL_MODEL_CALENDAR_FIELD_LOCATION :
	case E_CAL_MODEL_CALENDAR_FIELD_TRANSPARENCY :
		return g_strdup (value);
	}

	return g_strdup ("");
}

/* ECalModel class methods */

static void
ecmc_fill_component_from_model (ECalModel *model, ECalModelComponent *comp_data,
				ETableModel *source_model, gint row)
{
	g_return_if_fail (E_IS_CAL_MODEL_CALENDAR (model));
	g_return_if_fail (comp_data != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (source_model));

	set_dtend (model, comp_data,
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
