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

#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libedataserver/e-data-server-util.h>
#include "calendar-config.h"
#include "e-cal-model-tasks.h"
#include "e-cell-date-edit-text.h"
#include "misc.h"

struct _ECalModelTasksPrivate {
	guint reserved;
};

static void e_cal_model_tasks_finalize (GObject *object);

static gint ecmt_column_count (ETableModel *etm);
static gpointer ecmt_value_at (ETableModel *etm, gint col, gint row);
static void ecmt_set_value_at (ETableModel *etm, gint col, gint row, gconstpointer value);
static gboolean ecmt_is_cell_editable (ETableModel *etm, gint col, gint row);
static gpointer ecmt_duplicate_value (ETableModel *etm, gint col, gconstpointer value);
static void ecmt_free_value (ETableModel *etm, gint col, gpointer value);
static gpointer ecmt_initialize_value (ETableModel *etm, gint col);
static gboolean ecmt_value_is_empty (ETableModel *etm, gint col, gconstpointer value);
static gchar *ecmt_value_to_string (ETableModel *etm, gint col, gconstpointer value);

static const gchar *ecmt_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data);
static void ecmt_fill_component_from_model (ECalModel *model, ECalModelComponent *comp_data,
					    ETableModel *source_model, gint row);
static void commit_component_changes (ECalModelComponent *comp_data);

G_DEFINE_TYPE (ECalModelTasks, e_cal_model_tasks, E_TYPE_CAL_MODEL)

static void
e_cal_model_tasks_class_init (ECalModelTasksClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ETableModelClass *etm_class = E_TABLE_MODEL_CLASS (klass);
	ECalModelClass *model_class = E_CAL_MODEL_CLASS (klass);

	object_class->finalize = e_cal_model_tasks_finalize;

	etm_class->column_count = ecmt_column_count;
	etm_class->value_at = ecmt_value_at;
	etm_class->set_value_at = ecmt_set_value_at;
	etm_class->is_cell_editable = ecmt_is_cell_editable;
	etm_class->duplicate_value = ecmt_duplicate_value;
	etm_class->free_value = ecmt_free_value;
	etm_class->initialize_value = ecmt_initialize_value;
	etm_class->value_is_empty = ecmt_value_is_empty;
	etm_class->value_to_string = ecmt_value_to_string;

	model_class->get_color_for_component = ecmt_get_color_for_component;
	model_class->fill_component_from_model = ecmt_fill_component_from_model;
}

static void
e_cal_model_tasks_init (ECalModelTasks *model)
{
	ECalModelTasksPrivate *priv;

	priv = g_new0 (ECalModelTasksPrivate, 1);
	model->priv = priv;

	e_cal_model_set_component_kind (E_CAL_MODEL (model), ICAL_VTODO_COMPONENT);
}

static void
e_cal_model_tasks_finalize (GObject *object)
{
	ECalModelTasksPrivate *priv;
	ECalModelTasks *model = (ECalModelTasks *) object;

	g_return_if_fail (E_IS_CAL_MODEL_TASKS (model));

	priv = model->priv;
	if (priv) {
		g_free (priv);
		model->priv = NULL;
	}

	if (G_OBJECT_CLASS (e_cal_model_tasks_parent_class)->finalize)
		G_OBJECT_CLASS (e_cal_model_tasks_parent_class)->finalize (object);
}

/* ETableModel methods */
static gint
ecmt_column_count (ETableModel *etm)
{
	return E_CAL_MODEL_TASKS_FIELD_LAST;
}

/* This makes sure a task is marked as complete.
   It makes sure the "Date Completed" property is set. If the completed_date
   is not -1, then that is used, otherwise if the "Date Completed" property
   is not already set it is set to the current time.
   It makes sure the percent is set to 100, and that the status is "Completed".
   Note that this doesn't update the component on the server. */
static void
ensure_task_complete (ECalModelComponent *comp_data, time_t completed_date)
{
	icalproperty *prop;
	gboolean set_completed = TRUE;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_COMPLETED_PROPERTY);

	/* Date Completed. */
	if (completed_date == -1) {
		if (prop)
			set_completed = FALSE;
		else
			completed_date = time (NULL);
	}

	if (set_completed) {
		icaltimezone *utc_zone;
		struct icaltimetype new_completed;

		/* COMPLETED is stored in UTC. */
		utc_zone = icaltimezone_get_utc_timezone ();
		new_completed = icaltime_from_timet_with_zone (completed_date,
							       FALSE,
							       utc_zone);
		if (prop)
			icalproperty_set_completed (prop, new_completed);
		else {
			prop = icalproperty_new_completed (new_completed);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		}
	}

	/* Percent. */
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_PERCENTCOMPLETE_PROPERTY);
	if (!prop)
		icalcomponent_add_property (comp_data->icalcomp, icalproperty_new_percentcomplete (100));
	else
		icalproperty_set_percentcomplete (prop, 100);

	/* Status. */
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_STATUS_PROPERTY);
	if (prop)
		icalproperty_set_status (prop, ICAL_STATUS_COMPLETED);
	else
		icalcomponent_add_property (comp_data->icalcomp, icalproperty_new_status (ICAL_STATUS_COMPLETED));
}

static void
ensure_task_partially_complete (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	/* Date Completed. */
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_COMPLETED_PROPERTY);
	if (prop) {
		icalcomponent_remove_property (comp_data->icalcomp, prop);
		icalproperty_free (prop);
	}

	/* Percent. */
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_PERCENTCOMPLETE_PROPERTY);
	if (!prop)
		icalcomponent_add_property (comp_data->icalcomp, icalproperty_new_percentcomplete (50));
	else if (icalproperty_get_percentcomplete (prop) == 0 || icalproperty_get_percentcomplete (prop) == 100)
		icalproperty_set_percentcomplete (prop, 50);

	/* Status. */
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_STATUS_PROPERTY);
	if (prop)
		icalproperty_set_status (prop, ICAL_STATUS_INPROCESS);
	else
		icalcomponent_add_property (comp_data->icalcomp, icalproperty_new_status (ICAL_STATUS_INPROCESS));
}

/* This makes sure a task is marked as incomplete. It clears the
   "Date Completed" property. If the percent is set to 100 it removes it,
   and if the status is "Completed" it sets it to "Needs Action".
   Note that this doesn't update the component on the client. */
static void
ensure_task_not_complete (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	/* Date Completed. */
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_COMPLETED_PROPERTY);
	if (prop) {
		icalcomponent_remove_property (comp_data->icalcomp, prop);
		icalproperty_free (prop);
	}

	/* Percent. */
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_PERCENTCOMPLETE_PROPERTY);
	if (prop) {
		icalcomponent_remove_property (comp_data->icalcomp, prop);
		icalproperty_free (prop);
	}

	/* Status. */
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_STATUS_PROPERTY);
	if (prop)
		icalproperty_set_status (prop, ICAL_STATUS_NEEDSACTION);
}

static ECellDateEditValue *
get_completed (ECalModelComponent *comp_data)
{
	struct icaltimetype tt_completed;

	if (!comp_data->completed) {
		icaltimezone *zone;
		icalproperty *prop;

		prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_COMPLETED_PROPERTY);
		if (!prop)
			return NULL;

		tt_completed = icalproperty_get_completed (prop);
		if (!icaltime_is_valid_time (tt_completed) || icaltime_is_null_time (tt_completed))
			return NULL;

		comp_data->completed = g_new0 (ECellDateEditValue, 1);
		comp_data->completed->tt = tt_completed;

		if (icaltime_get_tzid (tt_completed)
		    && e_cal_get_timezone (comp_data->client, icaltime_get_tzid (tt_completed), &zone, NULL))
			comp_data->completed->zone = zone;
		else
			comp_data->completed->zone = NULL;
	}

	return comp_data->completed;
}

static ECellDateEditValue *
get_due (ECalModelComponent *comp_data)
{
	struct icaltimetype tt_due;

	if (!comp_data->due) {
		icaltimezone *zone;
		icalproperty *prop;

		prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DUE_PROPERTY);
		if (!prop)
			return NULL;

		tt_due = icalproperty_get_due (prop);
		if (!icaltime_is_valid_time (tt_due) || icaltime_is_null_time (tt_due))
			return NULL;

		comp_data->due = g_new0 (ECellDateEditValue, 1);
		comp_data->due->tt = tt_due;

		if (icaltime_get_tzid (tt_due)
		    && e_cal_get_timezone (comp_data->client, icaltime_get_tzid (tt_due), &zone, NULL))
			comp_data->due->zone = zone;
		else
			comp_data->due->zone = NULL;
	}

	return comp_data->due;
}

static gpointer
get_geo (ECalModelComponent *comp_data)
{
	icalproperty *prop;
	struct icalgeotype geo;
	static gchar buf[32];

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_GEO_PROPERTY);
	if (prop) {
		geo = icalproperty_get_geo (prop);
		g_snprintf (buf, sizeof (buf), "%g %s, %g %s",
			    fabs (geo.lat),
			    geo.lat >= 0.0 ? "N" : "S",
			    fabs (geo.lon),
			    geo.lon >= 0.0 ? "E" : "W");
		return buf;
	}

	return (gpointer) "";
}

static gint
get_percent (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_PERCENTCOMPLETE_PROPERTY);
	if (prop)
		return icalproperty_get_percentcomplete (prop);

	return 0;
}

static gpointer
get_priority (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_PRIORITY_PROPERTY);
	if (prop)
		return (gpointer) e_cal_util_priority_to_string (icalproperty_get_priority (prop));

	return (gpointer) "";
}

static gboolean
is_status_canceled (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_STATUS_PROPERTY);

	return prop && icalproperty_get_status (prop) == ICAL_STATUS_CANCELLED;
}

static gpointer
get_status (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_STATUS_PROPERTY);
	if (prop) {
		switch (icalproperty_get_status (prop)) {
		case ICAL_STATUS_NONE:
			return (gpointer) "";
		case ICAL_STATUS_NEEDSACTION:
			return (gpointer) _("Not Started");
		case ICAL_STATUS_INPROCESS:
			return (gpointer) _("In Progress");
		case ICAL_STATUS_COMPLETED:
			return (gpointer) _("Completed");
		case ICAL_STATUS_CANCELLED:
			return (gpointer) _("Canceled");
		default:
			return (gpointer) "";
		}
	}

	return (gpointer) "";
}

static gpointer
get_url (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_URL_PROPERTY);
	if (prop)
		return (gpointer) icalproperty_get_url (prop);

	return (gpointer) "";
}

static gboolean
is_complete (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_COMPLETED_PROPERTY);
	if (prop)
		return TRUE;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_PERCENTCOMPLETE_PROPERTY);
	if (prop && icalproperty_get_percentcomplete (prop) == 100)
		return TRUE;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_STATUS_PROPERTY);
	if (prop && icalproperty_get_status (prop) == ICAL_STATUS_COMPLETED)
		return TRUE;

	return FALSE;
}

typedef enum {
	E_CAL_MODEL_TASKS_DUE_NEVER,
	E_CAL_MODEL_TASKS_DUE_FUTURE,
	E_CAL_MODEL_TASKS_DUE_TODAY,
	E_CAL_MODEL_TASKS_DUE_OVERDUE,
	E_CAL_MODEL_TASKS_DUE_COMPLETE
} ECalModelTasksDueStatus;

static ECalModelTasksDueStatus
get_due_status (ECalModelTasks *model, ECalModelComponent *comp_data)
{
	icalproperty *prop;

	/* First, do we have a due date? */
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DUE_PROPERTY);
	if (!prop)
		return E_CAL_MODEL_TASKS_DUE_NEVER;
	else {
		struct icaltimetype now_tt, due_tt;
		icaltimezone *zone;

		/* Second, is it already completed? */
		if (is_complete (comp_data))
			return E_CAL_MODEL_TASKS_DUE_COMPLETE;

		/* Third, are we overdue as of right now? */
		due_tt = icalproperty_get_due (prop);
		if (due_tt.is_date) {
			gint cmp;

			now_tt = icaltime_current_time_with_zone (e_cal_model_get_timezone (E_CAL_MODEL (model)));
			cmp = icaltime_compare_date_only (due_tt, now_tt);

			if (cmp < 0)
				return E_CAL_MODEL_TASKS_DUE_OVERDUE;
			else if (cmp == 0)
				return E_CAL_MODEL_TASKS_DUE_TODAY;
			else
				return E_CAL_MODEL_TASKS_DUE_FUTURE;
		} else {
			icalparameter *param;
			const gchar *tzid;

			if (!(param = icalproperty_get_first_parameter (prop, ICAL_TZID_PARAMETER)))
				return E_CAL_MODEL_TASKS_DUE_FUTURE;

			/* Get the current time in the same timezone as the DUE date.*/
			tzid = icalparameter_get_tzid (param);
			if (!e_cal_get_timezone (comp_data->client, tzid, &zone, NULL))
				return E_CAL_MODEL_TASKS_DUE_FUTURE;

			now_tt = icaltime_current_time_with_zone (zone);

			if (icaltime_compare (due_tt, now_tt) <= 0)
				return E_CAL_MODEL_TASKS_DUE_OVERDUE;
			else
				if (icaltime_compare_date_only (due_tt, now_tt) == 0)
					return E_CAL_MODEL_TASKS_DUE_TODAY;
				else
					return E_CAL_MODEL_TASKS_DUE_FUTURE;
		}
	}
}

static gboolean
is_overdue (ECalModelTasks *model, ECalModelComponent *comp_data)
{
	switch (get_due_status (model, comp_data)) {
	case E_CAL_MODEL_TASKS_DUE_NEVER:
	case E_CAL_MODEL_TASKS_DUE_FUTURE:
	case E_CAL_MODEL_TASKS_DUE_COMPLETE:
		return FALSE;
	case E_CAL_MODEL_TASKS_DUE_TODAY:
	case E_CAL_MODEL_TASKS_DUE_OVERDUE:
		return TRUE;
	}

	return FALSE;
}

static gpointer
ecmt_value_at (ETableModel *etm, gint col, gint row)
{
	ECalModelComponent *comp_data;
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), NULL);

	g_return_val_if_fail (col >= 0 && (col < E_CAL_MODEL_TASKS_FIELD_LAST || col == E_CAL_MODEL_TASKS_FIELD_STRIKEOUT), NULL);
	g_return_val_if_fail (row >= 0 && row < e_table_model_row_count (etm), NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_tasks_parent_class)->value_at (etm, col, row);

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (!comp_data)
		return (gpointer) "";

	switch (col) {
	case E_CAL_MODEL_TASKS_FIELD_COMPLETED :
		return get_completed (comp_data);
	case E_CAL_MODEL_TASKS_FIELD_STRIKEOUT :
		return GINT_TO_POINTER (is_status_canceled (comp_data) || is_complete (comp_data));
	case E_CAL_MODEL_TASKS_FIELD_COMPLETE :
		return GINT_TO_POINTER (is_complete (comp_data));
	case E_CAL_MODEL_TASKS_FIELD_DUE :
		return get_due (comp_data);
	case E_CAL_MODEL_TASKS_FIELD_GEO :
		return get_geo (comp_data);
	case E_CAL_MODEL_TASKS_FIELD_OVERDUE :
		return GINT_TO_POINTER (is_overdue (model, comp_data));
	case E_CAL_MODEL_TASKS_FIELD_PERCENT :
		return GINT_TO_POINTER (get_percent (comp_data));
	case E_CAL_MODEL_TASKS_FIELD_PRIORITY :
		return get_priority (comp_data);
	case E_CAL_MODEL_TASKS_FIELD_STATUS :
		return get_status (comp_data);
	case E_CAL_MODEL_TASKS_FIELD_URL :
		return get_url (comp_data);
	}

	return (gpointer) "";
}

static void
set_completed (ECalModelTasks *model, ECalModelComponent *comp_data, gconstpointer value)
{
	ECellDateEditValue *dv = (ECellDateEditValue *) value;

	if (!dv)
		ensure_task_not_complete (comp_data);
	else {
		time_t t;

		if (dv->tt.is_date) {
			/* if it's a date, it will be floating,
			   but completed needs a date time value */
			dv->tt.is_date = FALSE;
			t = icaltime_as_timet_with_zone (dv->tt, e_cal_model_get_timezone (E_CAL_MODEL (model)));
		} else {
			/* we assume that COMPLETED is entered in the current timezone,
			   even though it gets stored in UTC */
			t = icaltime_as_timet_with_zone (dv->tt, dv->zone);
		}

		ensure_task_complete (comp_data, t);
	}
}

static void
set_complete (ECalModelComponent *comp_data, gconstpointer value)
{
	gint state = GPOINTER_TO_INT (value);

	if (state)
		ensure_task_complete (comp_data, -1);
	else
		ensure_task_not_complete (comp_data);
}

static void
set_due (ECalModel* model, ECalModelComponent *comp_data, gconstpointer value)
{
	e_cal_model_update_comp_time (model, comp_data, value, ICAL_DUE_PROPERTY, icalproperty_set_due, icalproperty_new_due);
}

/* FIXME: We need to set the "transient_for" property for the dialog, but the
 * model doesn't know anything about the windows.
 */
static void
show_geo_warning (void)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					 "%s", _("The geographical position must be entered "
					   "in the format: \n\n45.436845,125.862501"));
	gtk_widget_show (dialog);
}

static void
set_geo (ECalModelComponent *comp_data, const gchar *value)
{
	double latitude, longitude;
	gint matched;
	struct icalgeotype geo;
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_GEO_PROPERTY);

	if (string_is_empty (value)) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
	} else {
		matched = sscanf (value, "%lg , %lg", &latitude, &longitude);
		if (matched != 2)
			show_geo_warning ();

		geo.lat = latitude;
		geo.lon = longitude;
		if (prop)
			icalproperty_set_geo (prop, geo);
		else {
			prop = icalproperty_new_geo (geo);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		}

	}
}

static void
set_status (ECalModelComponent *comp_data, const gchar *value)
{
	icalproperty_status status;
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_STATUS_PROPERTY);

	/* an empty string is the same as 'None' */
	if (!value[0] || !e_util_utf8_strcasecmp (value, _("None")))
		return;
	else if (!e_util_utf8_strcasecmp (value, _("Not Started")))
		status = ICAL_STATUS_NEEDSACTION;
	else if (!e_util_utf8_strcasecmp (value, _("In Progress")))
		status = ICAL_STATUS_INPROCESS;
	else if (!e_util_utf8_strcasecmp (value, _("Completed")))
		status = ICAL_STATUS_COMPLETED;
	else if (!e_util_utf8_strcasecmp (value, _("Canceled")))
		status = ICAL_STATUS_CANCELLED;
	else {
		g_warning ("Invalid status: %s\n", value);
		return;
	}

	if (prop)
		icalproperty_set_status (prop, status);
	else {
		prop = icalproperty_new_status (status);
		icalcomponent_add_property (comp_data->icalcomp, prop);
	}

	switch (status) {
	case ICAL_STATUS_NEEDSACTION:
		ensure_task_not_complete (comp_data);
		break;

	case ICAL_STATUS_INPROCESS:
		ensure_task_partially_complete (comp_data);
		break;

	case ICAL_STATUS_CANCELLED:
		ensure_task_not_complete (comp_data);
		/* do this again, because the previous function changed status to NEEDSACTION */
		icalproperty_set_status (prop, status);
		break;

	case ICAL_STATUS_COMPLETED:
		ensure_task_complete (comp_data, -1);
		break;
	default:
		break;
	}
}

static void
set_percent (ECalModelComponent *comp_data, gconstpointer value)
{
	icalproperty *prop;
	gint percent = GPOINTER_TO_INT (value);

	g_return_if_fail (percent >= -1);
	g_return_if_fail (percent <= 100);

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_PERCENTCOMPLETE_PROPERTY);

	/* A value of -1 means it isn't set */
	if (percent == -1) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
		ensure_task_not_complete (comp_data);
	} else {
		if (prop)
			icalproperty_set_percentcomplete (prop, percent);
		else {
			prop = icalproperty_new_percentcomplete (percent);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		}

		if (percent == 100)
			ensure_task_complete (comp_data, -1);
		else {
			prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_COMPLETED_PROPERTY);
			if (prop) {
				icalcomponent_remove_property (comp_data->icalcomp, prop);
				icalproperty_free (prop);
			}

			if (percent > 0)
				set_status (comp_data, _("In Progress"));
		}
	}

}

static void
set_priority (ECalModelComponent *comp_data, const gchar *value)
{
	icalproperty *prop;
	gint priority;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_PRIORITY_PROPERTY);

	priority = e_cal_util_priority_from_string (value);
	if (priority == -1) {
		g_warning ("Invalid priority");
		priority = 0;
	}

	if (prop)
		icalproperty_set_priority (prop, priority);
	else {
		prop = icalproperty_new_priority (priority);
		icalcomponent_add_property (comp_data->icalcomp, prop);
	}
}

static void
set_url (ECalModelComponent *comp_data, const gchar *value)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_URL_PROPERTY);

	if (string_is_empty (value)) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
	} else {
		if (prop)
			icalproperty_set_url (prop, value);
		else {
			prop = icalproperty_new_url (value);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		}
	}
}

static void
ecmt_set_value_at (ETableModel *etm, gint col, gint row, gconstpointer value)
{
	ECalModelComponent *comp_data;
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_if_fail (E_IS_CAL_MODEL_TASKS (model));

	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST);
	g_return_if_fail (row >= 0 && row < e_table_model_row_count (etm));

	if (col < E_CAL_MODEL_FIELD_LAST) {
		E_TABLE_MODEL_CLASS (e_cal_model_tasks_parent_class)->set_value_at (etm, col, row, value);
		return;
	}

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (!comp_data)
		return;

	switch (col) {
	case E_CAL_MODEL_TASKS_FIELD_COMPLETED :
		set_completed (model, comp_data, value);
		break;
	case E_CAL_MODEL_TASKS_FIELD_COMPLETE :
		set_complete (comp_data, value);
		break;
	case E_CAL_MODEL_TASKS_FIELD_DUE :
		set_due ((ECalModel*) model, comp_data, value);
		break;
	case E_CAL_MODEL_TASKS_FIELD_GEO :
		set_geo (comp_data, value);
		break;
	case E_CAL_MODEL_TASKS_FIELD_PERCENT :
		set_percent (comp_data, value);
		break;
	case E_CAL_MODEL_TASKS_FIELD_PRIORITY :
		set_priority (comp_data, value);
		break;
	case E_CAL_MODEL_TASKS_FIELD_STATUS :
		set_status (comp_data, value);
		break;
	case E_CAL_MODEL_TASKS_FIELD_URL :
		set_url (comp_data, value);
		break;
	}

	commit_component_changes (comp_data);
}

static gboolean
ecmt_is_cell_editable (ETableModel *etm, gint col, gint row)
{
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), FALSE);

	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST, FALSE);
	g_return_val_if_fail (row >= -1 || (row >= 0 && row < e_table_model_row_count (etm)), FALSE);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_tasks_parent_class)->is_cell_editable (etm, col, row);

	if (!e_cal_model_test_row_editable (E_CAL_MODEL (etm), row))
		return FALSE;

	switch (col) {
	case E_CAL_MODEL_TASKS_FIELD_COMPLETED :
	case E_CAL_MODEL_TASKS_FIELD_COMPLETE :
	case E_CAL_MODEL_TASKS_FIELD_DUE :
	case E_CAL_MODEL_TASKS_FIELD_GEO :
	case E_CAL_MODEL_TASKS_FIELD_PERCENT :
	case E_CAL_MODEL_TASKS_FIELD_PRIORITY :
	case E_CAL_MODEL_TASKS_FIELD_STATUS :
	case E_CAL_MODEL_TASKS_FIELD_URL :
		return TRUE;
	}

	return FALSE;
}

static gpointer
ecmt_duplicate_value (ETableModel *etm, gint col, gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_tasks_parent_class)->duplicate_value (etm, col, value);

	switch (col) {
	case E_CAL_MODEL_TASKS_FIELD_GEO :
	case E_CAL_MODEL_TASKS_FIELD_PRIORITY :
	case E_CAL_MODEL_TASKS_FIELD_STATUS :
	case E_CAL_MODEL_TASKS_FIELD_URL :
		return g_strdup (value);
	case E_CAL_MODEL_TASKS_FIELD_COMPLETED :
	case E_CAL_MODEL_TASKS_FIELD_DUE :
		if (value) {
			ECellDateEditValue *dv, *orig_dv;

			orig_dv = (ECellDateEditValue *) value;
			dv = g_new0 (ECellDateEditValue, 1);
			*dv = *orig_dv;

			return dv;
		}
		break;

	case E_CAL_MODEL_TASKS_FIELD_COMPLETE :
	case E_CAL_MODEL_TASKS_FIELD_PERCENT :
	case E_CAL_MODEL_TASKS_FIELD_OVERDUE :
		return (gpointer) value;
	}

	return NULL;
}

static void
ecmt_free_value (ETableModel *etm, gint col, gpointer value)
{
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST);

	if (col < E_CAL_MODEL_FIELD_LAST) {
		E_TABLE_MODEL_CLASS (e_cal_model_tasks_parent_class)->free_value (etm, col, value);
		return;
	}

	switch (col) {
	case E_CAL_MODEL_TASKS_FIELD_COMPLETED :
	case E_CAL_MODEL_TASKS_FIELD_DUE :
	case E_CAL_MODEL_TASKS_FIELD_GEO :
	case E_CAL_MODEL_TASKS_FIELD_PRIORITY :
	case E_CAL_MODEL_TASKS_FIELD_STATUS :
	case E_CAL_MODEL_TASKS_FIELD_URL :
		if (value)
			g_free (value);
		break;
	case E_CAL_MODEL_TASKS_FIELD_PERCENT :
	case E_CAL_MODEL_TASKS_FIELD_COMPLETE :
	case E_CAL_MODEL_TASKS_FIELD_OVERDUE :
		break;
	}
}

static gpointer
ecmt_initialize_value (ETableModel *etm, gint col)
{
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), NULL);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_tasks_parent_class)->initialize_value (etm, col);

	switch (col) {
	case E_CAL_MODEL_TASKS_FIELD_GEO :
	case E_CAL_MODEL_TASKS_FIELD_PRIORITY :
	case E_CAL_MODEL_TASKS_FIELD_STATUS :
	case E_CAL_MODEL_TASKS_FIELD_URL :
		return g_strdup ("");
	case E_CAL_MODEL_TASKS_FIELD_COMPLETED :
	case E_CAL_MODEL_TASKS_FIELD_DUE :
	case E_CAL_MODEL_TASKS_FIELD_COMPLETE :
	case E_CAL_MODEL_TASKS_FIELD_OVERDUE :
		return NULL;
	case E_CAL_MODEL_TASKS_FIELD_PERCENT :
		return GINT_TO_POINTER (-1);
	}

	return NULL;
}

static gboolean
ecmt_value_is_empty (ETableModel *etm, gint col, gconstpointer value)
{
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), TRUE);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST, TRUE);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_tasks_parent_class)->value_is_empty (etm, col, value);

	switch (col) {
	case E_CAL_MODEL_TASKS_FIELD_GEO :
	case E_CAL_MODEL_TASKS_FIELD_PRIORITY :
	case E_CAL_MODEL_TASKS_FIELD_STATUS :
	case E_CAL_MODEL_TASKS_FIELD_URL :
		return string_is_empty (value);
	case E_CAL_MODEL_TASKS_FIELD_COMPLETED :
	case E_CAL_MODEL_TASKS_FIELD_DUE :
		return value ? FALSE : TRUE;
	case E_CAL_MODEL_TASKS_FIELD_PERCENT :
		return (GPOINTER_TO_INT (value) < 0) ? TRUE : FALSE;
	case E_CAL_MODEL_TASKS_FIELD_COMPLETE :
	case E_CAL_MODEL_TASKS_FIELD_OVERDUE :
		return TRUE;
	}

	return TRUE;
}

static gchar *
ecmt_value_to_string (ETableModel *etm, gint col, gconstpointer value)
{
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), g_strdup (""));
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST, g_strdup (""));

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_tasks_parent_class)->value_to_string (etm, col, value);

	switch (col) {
	case E_CAL_MODEL_TASKS_FIELD_GEO :
	case E_CAL_MODEL_TASKS_FIELD_PRIORITY :
	case E_CAL_MODEL_TASKS_FIELD_STATUS :
	case E_CAL_MODEL_TASKS_FIELD_URL :
		return g_strdup (value);
	case E_CAL_MODEL_TASKS_FIELD_COMPLETED :
	case E_CAL_MODEL_TASKS_FIELD_DUE :
		return e_cal_model_date_value_to_string (E_CAL_MODEL (model), value);
	case E_CAL_MODEL_TASKS_FIELD_COMPLETE :
	case E_CAL_MODEL_TASKS_FIELD_OVERDUE :
		return g_strdup (value ? _("Yes") : _("No"));
	case E_CAL_MODEL_TASKS_FIELD_PERCENT :
		if (GPOINTER_TO_INT (value) < 0)
			return g_strdup ("N/A");
		else
			return g_strdup_printf ("%i%%", GPOINTER_TO_INT (value));
	}

	return g_strdup ("");
}

/* ECalModel class methods */

static const gchar *
ecmt_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data)
{
	static gchar color_spec[16];
	GdkColor color;

	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), NULL);
	g_return_val_if_fail (comp_data != NULL, NULL);

	/* XXX ECalModel's get_color_for_component() method should really
	 *     get a GdkColor instead of a color specification string. */

	switch (get_due_status ((ECalModelTasks *) model, comp_data)) {
	case E_CAL_MODEL_TASKS_DUE_TODAY:
		/* XXX ugly hack */
		calendar_config_get_tasks_due_today_color (&color);
		g_snprintf (color_spec, sizeof (color_spec), "#%04x%04x%04x",
			color.red, color.green, color.blue);
		return color_spec;
	case E_CAL_MODEL_TASKS_DUE_OVERDUE:
		/* XXX ugly hack */
		calendar_config_get_tasks_overdue_color (&color);
		g_snprintf (color_spec, sizeof (color_spec), "#%04x%04x%04x",
			color.red, color.green, color.blue);
		return color_spec;
	case E_CAL_MODEL_TASKS_DUE_NEVER:
	case E_CAL_MODEL_TASKS_DUE_FUTURE:
	case E_CAL_MODEL_TASKS_DUE_COMPLETE:
		break;
	}

	return E_CAL_MODEL_CLASS (e_cal_model_tasks_parent_class)->get_color_for_component (model, comp_data);
}

static void
ecmt_fill_component_from_model (ECalModel *model, ECalModelComponent *comp_data,
				ETableModel *source_model, gint row)
{
	gpointer value;

	g_return_if_fail (E_IS_CAL_MODEL_TASKS (model));
	g_return_if_fail (comp_data != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (source_model));

	/* This just makes sure if anything indicates completion, all
	 * three fields do or if percent is 0, status is sane */

	value = e_table_model_value_at (source_model, E_CAL_MODEL_TASKS_FIELD_COMPLETED, row);
	set_completed ((ECalModelTasks *) model, comp_data, value);
	if (!value) {
		value = e_table_model_value_at (source_model, E_CAL_MODEL_TASKS_FIELD_PERCENT, row);
		set_percent (comp_data, value);
		if (GPOINTER_TO_INT (value) != 100 && GPOINTER_TO_INT (value) != 0)
			set_status (comp_data, e_table_model_value_at (source_model, E_CAL_MODEL_TASKS_FIELD_STATUS, row));
	}

	set_due (model, comp_data,
		 e_table_model_value_at (source_model, E_CAL_MODEL_TASKS_FIELD_DUE, row));
	set_geo (comp_data,
		 e_table_model_value_at (source_model, E_CAL_MODEL_TASKS_FIELD_GEO, row));
	set_priority (comp_data,
		      e_table_model_value_at (source_model, E_CAL_MODEL_TASKS_FIELD_PRIORITY, row));
	set_url (comp_data,
		 e_table_model_value_at (source_model, E_CAL_MODEL_TASKS_FIELD_URL, row));
}

/**
 * e_cal_model_tasks_new
 */
ECalModelTasks *
e_cal_model_tasks_new (void)
{
	return g_object_new (E_TYPE_CAL_MODEL_TASKS, NULL);
}

/**
 * e_cal_model_tasks_mark_comp_complete
 * Marks component as complete and commits changes to the calendar backend.
 *
 * @param model Currently not used...
 * @param comp_data Component of our interest
 **/
void e_cal_model_tasks_mark_comp_complete (ECalModelTasks *model, ECalModelComponent *comp_data)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (comp_data != NULL);

	/* we will receive changes when committed, so don't do this */
	/*e_table_model_pre_change (E_TABLE_MODEL (model));*/

	ensure_task_complete (comp_data, -1);

	/*e_table_model_row_changed (E_TABLE_MODEL (model), model_row);*/

	commit_component_changes (comp_data);
}

/**
 * e_cal_model_tasks_mark_comp_incomplete
 * Marks component as incomplete and commits changes to the calendar backend.
 *
 * @param model Currently not used...
 * @param comp_data Component of our interest
 **/
void e_cal_model_tasks_mark_comp_incomplete (ECalModelTasks *model, ECalModelComponent *comp_data)
{
	icalproperty *prop,*prop1;

	g_return_if_fail (model != NULL);
	g_return_if_fail (comp_data != NULL);

	/* we will receive changes when committed, so don't do this */
	/*e_table_model_pre_change (E_TABLE_MODEL (model));*/

	/* Status */
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_STATUS_PROPERTY);
	if (prop)
		icalproperty_set_status (prop, ICAL_STATUS_NEEDSACTION);
	else
		icalcomponent_add_property (comp_data->icalcomp, icalproperty_new_status (ICAL_STATUS_NEEDSACTION));

	/*complete property*/
	prop1 = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_COMPLETED_PROPERTY);
	if (prop1) {
		icalcomponent_remove_property (comp_data->icalcomp, prop1);
		icalproperty_free (prop1);
	}

	/* Percent. */
	prop1 = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_PERCENTCOMPLETE_PROPERTY);
	if (prop1) {
		icalcomponent_remove_property (comp_data->icalcomp, prop1);
		icalproperty_free (prop1);
	}

	/*e_table_model_row_changed (E_TABLE_MODEL (model), model_row);*/

	commit_component_changes (comp_data);
}

/**
 * commit_component_changes
 * Commits changes to the backend calendar of the component.
 *
 * @param comp_data Component of our interest, which has been changed.
 **/
static void
commit_component_changes (ECalModelComponent *comp_data)
{
	g_return_if_fail (comp_data != NULL);

	/* FIXME ask about mod type */
	if (!e_cal_modify_object (comp_data->client, comp_data->icalcomp, CALOBJ_MOD_ALL, NULL)) {
		g_warning (G_STRLOC ": Could not modify the object!");

		/* FIXME Show error dialog */
	}
}

/**
 * e_cal_model_tasks_update_due_tasks
 */
void
e_cal_model_tasks_update_due_tasks (ECalModelTasks *model)
{
	gint row, row_count;
	ECalModelComponent *comp_data;
	ECalModelTasksDueStatus status;

	g_return_if_fail (E_IS_CAL_MODEL_TASKS (model));

	row_count = e_table_model_row_count (E_TABLE_MODEL (model));

	for (row = 0; row < row_count; row++)
	{
		comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
		status = get_due_status (E_CAL_MODEL_TASKS (model), comp_data);
		if ((status == E_CAL_MODEL_TASKS_DUE_TODAY) || (status == E_CAL_MODEL_TASKS_DUE_OVERDUE))
		{
			e_table_model_pre_change (E_TABLE_MODEL (model));
			e_table_model_row_changed (E_TABLE_MODEL (model), row);
		}
	}
}
