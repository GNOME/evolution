/* Evolution calendar - Data model for ETable
 *
 * Copyright (C) 2004 Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <string.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include "calendar-config.h"
#include "e-cal-model-tasks.h"
#include "e-cell-date-edit-text.h"
#include "misc.h"

struct _ECalModelTasksPrivate {
};

static void e_cal_model_tasks_finalize (GObject *object);

static int ecmt_column_count (ETableModel *etm);
static void *ecmt_value_at (ETableModel *etm, int col, int row);
static void ecmt_set_value_at (ETableModel *etm, int col, int row, const void *value);
static gboolean ecmt_is_cell_editable (ETableModel *etm, int col, int row);
static void *ecmt_duplicate_value (ETableModel *etm, int col, const void *value);
static void ecmt_free_value (ETableModel *etm, int col, void *value);
static void *ecmt_initialize_value (ETableModel *etm, int col);
static gboolean ecmt_value_is_empty (ETableModel *etm, int col, const void *value);
static char *ecmt_value_to_string (ETableModel *etm, int col, const void *value);

static const char *ecmt_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data);
static void ecmt_fill_component_from_model (ECalModel *model, ECalModelComponent *comp_data,
					    ETableModel *source_model, gint row);

G_DEFINE_TYPE (ECalModelTasks, e_cal_model_tasks, E_TYPE_CAL_MODEL);

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
static int
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

static char *
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
			    geo.lat >= 0.0 ? _("N") : _("S"),
			    fabs (geo.lon),
			    geo.lon >= 0.0 ? _("E") : _("W"));
		return buf;
	}

	return "";
}

static int
get_percent (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_PERCENTCOMPLETE_PROPERTY);
	if (prop)
		return icalproperty_get_percentcomplete (prop);

	return 0;
}

static char *
get_priority (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_PRIORITY_PROPERTY);
	if (prop)
		return e_cal_util_priority_to_string (icalproperty_get_priority (prop));

	return "";
}

static char *
get_status (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_STATUS_PROPERTY);
	if (prop) {
		switch (icalproperty_get_status (prop)) {
		case ICAL_STATUS_NONE:
			return "";
		case ICAL_STATUS_NEEDSACTION:
			return _("Not Started");
		case ICAL_STATUS_INPROCESS:
			return _("In Progress");
		case ICAL_STATUS_COMPLETED:
			return _("Completed");
		case ICAL_STATUS_CANCELLED:
			return _("Cancelled");
		default:
			return "";
		}
	}

	return "";
}

static char *
get_url (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_URL_PROPERTY);
	if (prop)
		return (char *) icalproperty_get_url (prop);

	return "";
}

static gboolean
is_complete (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_COMPLETED_PROPERTY);

	return prop ? TRUE : FALSE;
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
			int cmp;
			
			now_tt = icaltime_current_time_with_zone (e_cal_model_get_timezone (E_CAL_MODEL (model)));
			cmp = icaltime_compare_date_only (due_tt, now_tt);
			
			if (cmp < 0)
				return E_CAL_MODEL_TASKS_DUE_OVERDUE;
			else if (cmp == 0)
				return E_CAL_MODEL_TASKS_DUE_TODAY;
			else
				return E_CAL_MODEL_TASKS_DUE_FUTURE;
		} else {
			/* Get the current time in the same timezone as the DUE date.*/
			if (!e_cal_get_timezone (comp_data->client, icaltime_get_tzid (due_tt), &zone, NULL))
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

	return E_CAL_MODEL_TASKS_DUE_NEVER;
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

static void *
ecmt_value_at (ETableModel *etm, int col, int row)
{
	ECalModelTasksPrivate *priv;
	ECalModelComponent *comp_data;
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), NULL);

	priv = model->priv;

	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST, NULL);
	g_return_val_if_fail (row >= 0 && row < e_table_model_row_count (etm), NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_tasks_parent_class)->value_at (etm, col, row);

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (!comp_data)
		return "";

	switch (col) {
	case E_CAL_MODEL_TASKS_FIELD_COMPLETED :
		return get_completed (comp_data);
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

	return "";
}

static void
set_completed (ECalModelTasks *model, ECalModelComponent *comp_data, const void *value)
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
set_complete (ECalModelComponent *comp_data, const void *value)
{
	gint state = GPOINTER_TO_INT (value);

	if (state)
		ensure_task_complete (comp_data, -1);
	else
		ensure_task_not_complete (comp_data);
}

static void
set_due (ECalModelComponent *comp_data, const void *value)
{
	ECellDateEditValue *dv = (ECellDateEditValue *) value;
	icalproperty *prop;
	icalparameter *param;
	const char *tzid;
	
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DUE_PROPERTY);
	if (prop)
		param = icalproperty_get_first_parameter (prop, ICAL_TZID_PARAMETER);
	else
		param = NULL;

	/* If we are setting the property to NULL (i.e. removing it), then
	   we remove it if it exists. */
	if (!dv) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}

		return;
	}
	
	/* If the TZID is set to "UTC", we set the is_utc flag. */
	tzid = dv->zone ? icaltimezone_get_tzid (dv->zone) : "UTC";
	if (tzid && !strcmp (tzid, "UTC"))
		dv->tt.is_utc = 1;
	else
		dv->tt.is_utc = 0;

	if (prop) {
		icalproperty_set_due (prop, dv->tt);
	} else {
		prop = icalproperty_new_due (dv->tt);
		icalcomponent_add_property (comp_data->icalcomp, prop);
	}

	/* If the TZID is set to "UTC", we don't want to save the TZID. */
	if (tzid && strcmp (tzid, "UTC")) {
		if (param) {
			icalparameter_set_tzid (param, (char *) tzid);
		} else {
			param = icalparameter_new_tzid ((char *) tzid);
			icalproperty_add_parameter (prop, param);
		}
	} else if (param) {
		icalproperty_remove_parameter (prop, ICAL_TZID_PARAMETER);
	}
}

/* FIXME: We need to set the "transient_for" property for the dialog, but the
 * model doesn't know anything about the windows.
 */
static void
show_geo_warning (void)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					 _("The geographical position must be entered "
					   "in the format: \n\n45.436845,125.862501"));
	gtk_widget_show (dialog);
}

static void
set_geo (ECalModelComponent *comp_data, const char *value)
{
	double latitude, longitude;
	int matched;
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
set_status (ECalModelComponent *comp_data, const char *value)
{
	icalproperty_status status;
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_STATUS_PROPERTY);

	/* an empty string is the same as 'None' */
	if (!value[0] || !g_strcasecmp (value, _("None")))
		status = ICAL_STATUS_NONE;
	else if (!g_strcasecmp (value, _("Not Started")))
		status = ICAL_STATUS_NEEDSACTION;
	else if (!g_strcasecmp (value, _("In Progress")))
		status = ICAL_STATUS_INPROCESS;
	else if (!g_strcasecmp (value, _("Completed")))
		status = ICAL_STATUS_COMPLETED;
	else if (!g_strcasecmp (value, _("Cancelled")))
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
		break;

	case ICAL_STATUS_COMPLETED:
		ensure_task_complete (comp_data, -1);
		break;		
	default:
		break;
	}
}

static void
set_percent (ECalModelComponent *comp_data, const void *value)
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
set_priority (ECalModelComponent *comp_data, const char *value)
{
	icalproperty *prop;
	int priority;

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
set_url (ECalModelComponent *comp_data, const char *value)
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
ecmt_set_value_at (ETableModel *etm, int col, int row, const void *value)
{
	ECalModelTasksPrivate *priv;
	ECalModelComponent *comp_data;
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_if_fail (E_IS_CAL_MODEL_TASKS (model));

	priv = model->priv;

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
		set_due (comp_data, value);
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

	/* FIXME ask about mod type */
	if (!e_cal_modify_object (comp_data->client, comp_data->icalcomp, CALOBJ_MOD_ALL, NULL)) {
		g_warning (G_STRLOC ": Could not modify the object!");
		
		/* FIXME Show error dialog */
	}
}

static gboolean
ecmt_is_cell_editable (ETableModel *etm, int col, int row)
{
	ECalModelTasksPrivate *priv;
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), FALSE);

	priv = model->priv;

	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST, FALSE);
	g_return_val_if_fail (row >= -1 || (row >= 0 && row < e_table_model_row_count (etm)), FALSE);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_tasks_parent_class)->is_cell_editable (etm, col, row);

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

static void *
ecmt_duplicate_value (ETableModel *etm, int col, const void *value)
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
		return (void *) value;
	}

	return NULL;
}

static void
ecmt_free_value (ETableModel *etm, int col, void *value)
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

static void *
ecmt_initialize_value (ETableModel *etm, int col)
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
ecmt_value_is_empty (ETableModel *etm, int col, const void *value)
{
	ECalModelTasksPrivate *priv;
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), TRUE);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST, TRUE);

	priv = model->priv;

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

static char *
ecmt_value_to_string (ETableModel *etm, int col, const void *value)
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

static const char *
ecmt_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data)
{
	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), NULL);
	g_return_val_if_fail (comp_data != NULL, NULL);

	switch (get_due_status ((ECalModelTasks *) model, comp_data)) {
	case E_CAL_MODEL_TASKS_DUE_TODAY:
		return calendar_config_get_tasks_due_today_color ();
	case E_CAL_MODEL_TASKS_DUE_OVERDUE:
		return calendar_config_get_tasks_overdue_color ();
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
	void *value;
	
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
	
	set_due (comp_data,
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
 * e_cal_model_tasks_mark_task_complete
 */
void
e_cal_model_tasks_mark_task_complete (ECalModelTasks *model, gint model_row)
{
	ECalModelTasksPrivate *priv;
	ECalModelComponent *comp_data;

	g_return_if_fail (E_IS_CAL_MODEL_TASKS (model));
	g_return_if_fail (model_row >= 0 && model_row < e_table_model_row_count (E_TABLE_MODEL (model)));

	priv = model->priv;

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), model_row);
	if (comp_data) {
		e_table_model_pre_change (E_TABLE_MODEL (model));

		ensure_task_complete (comp_data, -1);

		e_table_model_row_changed (E_TABLE_MODEL (model), model_row);
	}
}
