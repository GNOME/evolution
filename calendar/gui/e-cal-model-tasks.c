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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "calendar-config.h"
#include "e-cal-model-tasks.h"
#include "e-cell-date-edit-text.h"
#include "misc.h"

#define E_CAL_MODEL_TASKS_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_MODEL_TASKS, ECalModelTasksPrivate))

struct _ECalModelTasksPrivate {
	gboolean highlight_due_today;
	gchar *color_due_today;
	gboolean highlight_overdue;
	gchar *color_overdue;
};

enum {
	PROP_0,
	PROP_HIGHLIGHT_DUE_TODAY,
	PROP_COLOR_DUE_TODAY,
	PROP_HIGHLIGHT_OVERDUE,
	PROP_COLOR_OVERDUE
};

/* Forward Declarations */
static void	e_cal_model_tasks_table_model_init
					(ETableModelInterface *iface);

static ETableModelInterface *table_model_parent_interface;

G_DEFINE_TYPE_WITH_CODE (
	ECalModelTasks,
	e_cal_model_tasks,
	E_TYPE_CAL_MODEL,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_TABLE_MODEL,
		e_cal_model_tasks_table_model_init))

/* This makes sure a task is marked as complete.
 * It makes sure the "Date Completed" property is set. If the completed_date
 * is not -1, then that is used, otherwise if the "Date Completed" property
 * is not already set it is set to the current time.
 * It makes sure the percent is set to 100, and that the status is "Completed".
 * Note that this doesn't update the component on the server. */
static void
ensure_task_complete (ECalModelComponent *comp_data,
                      time_t completed_date)
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
		new_completed = icaltime_from_timet_with_zone (
			completed_date,
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
 * "Date Completed" property. If the percent is set to 100 it removes it,
 * and if the status is "Completed" it sets it to "Needs Action".
 * Note that this doesn't update the component on the client. */
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
		    && e_cal_client_get_timezone_sync (comp_data->client, icaltime_get_tzid (tt_completed), &zone, NULL, NULL))
			comp_data->completed->zone = zone;
		else
			comp_data->completed->zone = NULL;
	}

	return e_cal_model_copy_cell_date_value (comp_data->completed);
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
		    && e_cal_client_get_timezone_sync (comp_data->client, icaltime_get_tzid (tt_due), &zone, NULL, NULL))
			comp_data->due->zone = zone;
		else
			comp_data->due->zone = NULL;
	}

	return e_cal_model_copy_cell_date_value (comp_data->due);
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
		g_snprintf (
			buf, sizeof (buf), "%g %s, %g %s",
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
get_due_status (ECalModelTasks *model,
                ECalModelComponent *comp_data)
{
	icalproperty *prop;

	/* First, do we have a due date? */
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DUE_PROPERTY);
	if (!prop)
		return E_CAL_MODEL_TASKS_DUE_NEVER;
	else {
		struct icaltimetype now_tt, due_tt;
		icaltimezone *zone = NULL;

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
			e_cal_client_get_timezone_sync (
				comp_data->client, tzid, &zone, NULL, NULL);
			if (zone == NULL)
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
is_overdue (ECalModelTasks *model,
            ECalModelComponent *comp_data)
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

static void
set_completed (ECalModelTasks *model,
               ECalModelComponent *comp_data,
               gconstpointer value)
{
	ECellDateEditValue *dv = (ECellDateEditValue *) value;

	if (!dv)
		ensure_task_not_complete (comp_data);
	else {
		time_t t;

		if (dv->tt.is_date) {
			/* if it's a date, it will be floating,
			 * but completed needs a date time value */
			dv->tt.is_date = FALSE;
			t = icaltime_as_timet_with_zone (dv->tt, e_cal_model_get_timezone (E_CAL_MODEL (model)));
		} else {
			/* we assume that COMPLETED is entered in the current timezone,
			 * even though it gets stored in UTC */
			t = icaltime_as_timet_with_zone (dv->tt, dv->zone);
		}

		ensure_task_complete (comp_data, t);
	}
}

static void
set_complete (ECalModelComponent *comp_data,
              gconstpointer value)
{
	gint state = GPOINTER_TO_INT (value);

	if (state)
		ensure_task_complete (comp_data, -1);
	else
		ensure_task_not_complete (comp_data);
}

static void
set_due (ECalModel *model,
         ECalModelComponent *comp_data,
         gconstpointer value)
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

	dialog = gtk_message_dialog_new (
		NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"%s", _("The geographical position must be entered "
		"in the format: \n\n45.436845,125.862501"));
	gtk_widget_show (dialog);
}

static void
set_geo (ECalModelComponent *comp_data,
         const gchar *value)
{
	gdouble latitude, longitude;
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
set_status (ECalModelComponent *comp_data,
            const gchar *value)
{
	icalproperty_status status;
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_STATUS_PROPERTY);

	/* an empty string is the same as 'None' */
	if (!value[0])
		return;

	/* Translators: "None" for task's status */
	if (!e_util_utf8_strcasecmp (value, C_("cal-task-status", "None")))
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

	/* to make compiler happy */
	/* coverity[dead_error_begin] */
	default:
		break;
	}
}

static void
set_percent (ECalModelComponent *comp_data,
             gconstpointer value)
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
set_priority (ECalModelComponent *comp_data,
              const gchar *value)
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
set_url (ECalModelComponent *comp_data,
         const gchar *value)
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
cal_model_tasks_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_HIGHLIGHT_DUE_TODAY:
			e_cal_model_tasks_set_highlight_due_today (
				E_CAL_MODEL_TASKS (object),
				g_value_get_boolean (value));
			return;

		case PROP_COLOR_DUE_TODAY:
			e_cal_model_tasks_set_color_due_today (
				E_CAL_MODEL_TASKS (object),
				g_value_get_string (value));
			return;

		case PROP_HIGHLIGHT_OVERDUE:
			e_cal_model_tasks_set_highlight_overdue (
				E_CAL_MODEL_TASKS (object),
				g_value_get_boolean (value));
			return;

		case PROP_COLOR_OVERDUE:
			e_cal_model_tasks_set_color_overdue (
				E_CAL_MODEL_TASKS (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_model_tasks_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_HIGHLIGHT_DUE_TODAY:
			g_value_set_boolean (
				value,
				e_cal_model_tasks_get_highlight_due_today (
				E_CAL_MODEL_TASKS (object)));
			return;

		case PROP_COLOR_DUE_TODAY:
			g_value_set_string (
				value,
				e_cal_model_tasks_get_color_due_today (
				E_CAL_MODEL_TASKS (object)));
			return;

		case PROP_HIGHLIGHT_OVERDUE:
			g_value_set_boolean (
				value,
				e_cal_model_tasks_get_highlight_overdue (
				E_CAL_MODEL_TASKS (object)));
			return;

		case PROP_COLOR_OVERDUE:
			g_value_set_string (
				value,
				e_cal_model_tasks_get_color_overdue (
				E_CAL_MODEL_TASKS (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_model_tasks_finalize (GObject *object)
{
	ECalModelTasksPrivate *priv;

	priv = E_CAL_MODEL_TASKS_GET_PRIVATE (object);

	g_free (priv->color_due_today);
	g_free (priv->color_overdue);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_cal_model_tasks_parent_class)->finalize (object);
}

static const gchar *
cal_model_tasks_get_color_for_component (ECalModel *model,
                                         ECalModelComponent *comp_data)
{
	ECalModelTasks *tasks;

	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), NULL);
	g_return_val_if_fail (comp_data != NULL, NULL);

	tasks = E_CAL_MODEL_TASKS (model);

	/* XXX ECalModel's get_color_for_component() method should really
	 *     get a GdkColor instead of a color specification string. */

	switch (get_due_status (tasks, comp_data)) {
	case E_CAL_MODEL_TASKS_DUE_TODAY:
		if (!e_cal_model_tasks_get_highlight_due_today (tasks))
			break;
		return e_cal_model_tasks_get_color_due_today (tasks);
	case E_CAL_MODEL_TASKS_DUE_OVERDUE:
		if (!e_cal_model_tasks_get_highlight_overdue (tasks))
			break;
		return e_cal_model_tasks_get_color_overdue (tasks);
	case E_CAL_MODEL_TASKS_DUE_NEVER:
	case E_CAL_MODEL_TASKS_DUE_FUTURE:
	case E_CAL_MODEL_TASKS_DUE_COMPLETE:
		break;
	}

	return E_CAL_MODEL_CLASS (e_cal_model_tasks_parent_class)->
		get_color_for_component (model, comp_data);
}

static void
cal_model_tasks_store_values_from_model (ECalModel *model,
					 ETableModel *source_model,
					 gint row,
					 GHashTable *values)
{
	g_return_if_fail (E_IS_CAL_MODEL_TASKS (model));
	g_return_if_fail (E_IS_TABLE_MODEL (source_model));
	g_return_if_fail (values != NULL);

	e_cal_model_util_set_value (values, source_model, E_CAL_MODEL_TASKS_FIELD_COMPLETED, row);
	e_cal_model_util_set_value (values, source_model, E_CAL_MODEL_TASKS_FIELD_PERCENT, row);
	e_cal_model_util_set_value (values, source_model, E_CAL_MODEL_TASKS_FIELD_STATUS, row);
	e_cal_model_util_set_value (values, source_model, E_CAL_MODEL_TASKS_FIELD_DUE, row);
	e_cal_model_util_set_value (values, source_model, E_CAL_MODEL_TASKS_FIELD_GEO, row);
	e_cal_model_util_set_value (values, source_model, E_CAL_MODEL_TASKS_FIELD_PRIORITY, row);
	e_cal_model_util_set_value (values, source_model, E_CAL_MODEL_TASKS_FIELD_URL, row);
}

static void
cal_model_tasks_fill_component_from_values (ECalModel *model,
					    ECalModelComponent *comp_data,
					    GHashTable *values)
{
	gpointer value;

	g_return_if_fail (E_IS_CAL_MODEL_TASKS (model));
	g_return_if_fail (comp_data != NULL);
	g_return_if_fail (values != NULL);

	/* This just makes sure if anything indicates completion, all
	 * three fields do or if percent is 0, status is sane */

	value = e_cal_model_util_get_value (values, E_CAL_MODEL_TASKS_FIELD_COMPLETED);
	set_completed ((ECalModelTasks *) model, comp_data, value);
	if (!value) {
		value = e_cal_model_util_get_value (values, E_CAL_MODEL_TASKS_FIELD_PERCENT);
		set_percent (comp_data, value);
		if (GPOINTER_TO_INT (value) != 100 && GPOINTER_TO_INT (value) != 0)
			set_status (comp_data, e_cal_model_util_get_value (values, E_CAL_MODEL_TASKS_FIELD_STATUS));
	}

	set_due (model, comp_data, e_cal_model_util_get_value (values, E_CAL_MODEL_TASKS_FIELD_DUE));
	set_geo (comp_data, e_cal_model_util_get_value (values, E_CAL_MODEL_TASKS_FIELD_GEO));
	set_priority (comp_data, e_cal_model_util_get_value (values, E_CAL_MODEL_TASKS_FIELD_PRIORITY));
	set_url (comp_data, e_cal_model_util_get_value (values, E_CAL_MODEL_TASKS_FIELD_URL));
}

static gint
cal_model_tasks_column_count (ETableModel *etm)
{
	return E_CAL_MODEL_TASKS_FIELD_LAST;
}

static gpointer
cal_model_tasks_value_at (ETableModel *etm,
                          gint col,
                          gint row)
{
	ECalModelComponent *comp_data;
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), NULL);

	g_return_val_if_fail (col >= 0 && (col < E_CAL_MODEL_TASKS_FIELD_LAST || col == E_CAL_MODEL_TASKS_FIELD_STRIKEOUT), NULL);
	g_return_val_if_fail (row >= 0 && row < e_table_model_row_count (etm), NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->value_at (etm, col, row);

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
cal_model_tasks_set_value_at (ETableModel *etm,
                              gint col,
                              gint row,
                              gconstpointer value)
{
	ECalModelComponent *comp_data;
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_if_fail (E_IS_CAL_MODEL_TASKS (model));

	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST);
	g_return_if_fail (row >= 0 && row < e_table_model_row_count (etm));

	if (col < E_CAL_MODEL_FIELD_LAST) {
		table_model_parent_interface->set_value_at (etm, col, row, value);
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
		set_due ((ECalModel *) model, comp_data, value);
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

	e_cal_model_modify_component (E_CAL_MODEL (model), comp_data, E_CAL_OBJ_MOD_ALL);
}

static gboolean
cal_model_tasks_is_cell_editable (ETableModel *etm,
                                  gint col,
                                  gint row)
{
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), FALSE);

	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST, FALSE);
	g_return_val_if_fail (row >= -1 || (row >= 0 && row < e_table_model_row_count (etm)), FALSE);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->is_cell_editable (etm, col, row);

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
cal_model_tasks_duplicate_value (ETableModel *etm,
                                 gint col,
                                 gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->duplicate_value (etm, col, value);

	switch (col) {
	case E_CAL_MODEL_TASKS_FIELD_COMPLETED :
	case E_CAL_MODEL_TASKS_FIELD_DUE :
		return e_cal_model_copy_cell_date_value (value);

	case E_CAL_MODEL_TASKS_FIELD_GEO :
	case E_CAL_MODEL_TASKS_FIELD_PRIORITY :
	case E_CAL_MODEL_TASKS_FIELD_STATUS :
	case E_CAL_MODEL_TASKS_FIELD_URL :
	case E_CAL_MODEL_TASKS_FIELD_COMPLETE :
	case E_CAL_MODEL_TASKS_FIELD_PERCENT :
	case E_CAL_MODEL_TASKS_FIELD_OVERDUE :
		return (gpointer) value;
	}

	return NULL;
}

static void
cal_model_tasks_free_value (ETableModel *etm,
                            gint col,
                            gpointer value)
{
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST);

	if (col < E_CAL_MODEL_FIELD_LAST) {
		table_model_parent_interface->free_value (etm, col, value);
		return;
	}

	switch (col) {
	case E_CAL_MODEL_TASKS_FIELD_COMPLETED :
	case E_CAL_MODEL_TASKS_FIELD_DUE :
		if (value)
			g_free (value);
		break;
	case E_CAL_MODEL_TASKS_FIELD_GEO :
	case E_CAL_MODEL_TASKS_FIELD_PRIORITY :
	case E_CAL_MODEL_TASKS_FIELD_STATUS :
	case E_CAL_MODEL_TASKS_FIELD_URL :
		break;
	case E_CAL_MODEL_TASKS_FIELD_PERCENT :
	case E_CAL_MODEL_TASKS_FIELD_COMPLETE :
	case E_CAL_MODEL_TASKS_FIELD_OVERDUE :
		break;
	}
}

static gpointer
cal_model_tasks_initialize_value (ETableModel *etm,
                                  gint col)
{
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), NULL);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->initialize_value (etm, col);

	switch (col) {
	case E_CAL_MODEL_TASKS_FIELD_GEO :
	case E_CAL_MODEL_TASKS_FIELD_PRIORITY :
	case E_CAL_MODEL_TASKS_FIELD_STATUS :
	case E_CAL_MODEL_TASKS_FIELD_URL :
		return (gpointer) "";
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
cal_model_tasks_value_is_empty (ETableModel *etm,
                                gint col,
                                gconstpointer value)
{
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), TRUE);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST, TRUE);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->value_is_empty (etm, col, value);

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
cal_model_tasks_value_to_string (ETableModel *etm,
                                 gint col,
                                 gconstpointer value)
{
	ECalModelTasks *model = (ECalModelTasks *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), g_strdup (""));
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_TASKS_FIELD_LAST, g_strdup (""));

	if (col < E_CAL_MODEL_FIELD_LAST)
		return table_model_parent_interface->value_to_string (etm, col, value);

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

static void
e_cal_model_tasks_class_init (ECalModelTasksClass *class)
{
	GObjectClass *object_class;
	ECalModelClass *cal_model_class;

	g_type_class_add_private (class, sizeof (ECalModelTasksPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = cal_model_tasks_set_property;
	object_class->get_property = cal_model_tasks_get_property;
	object_class->finalize = cal_model_tasks_finalize;

	cal_model_class = E_CAL_MODEL_CLASS (class);
	cal_model_class->get_color_for_component = cal_model_tasks_get_color_for_component;
	cal_model_class->store_values_from_model = cal_model_tasks_store_values_from_model;
	cal_model_class->fill_component_from_values = cal_model_tasks_fill_component_from_values;

	g_object_class_install_property (
		object_class,
		PROP_HIGHLIGHT_DUE_TODAY,
		g_param_spec_boolean (
			"highlight-due-today",
			"Highlight Due Today",
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_COLOR_DUE_TODAY,
		g_param_spec_string (
			"color-due-today",
			"Color Due Today",
			NULL,
			"#1e90ff",
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_HIGHLIGHT_OVERDUE,
		g_param_spec_boolean (
			"highlight-overdue",
			"Highlight Overdue",
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_COLOR_OVERDUE,
		g_param_spec_string (
			"color-overdue",
			"Color Overdue",
			NULL,
			"#ff0000",
			G_PARAM_READWRITE));
}

static void
e_cal_model_tasks_table_model_init (ETableModelInterface *iface)
{
	table_model_parent_interface =
		g_type_interface_peek_parent (iface);

	iface->column_count = cal_model_tasks_column_count;

	iface->value_at = cal_model_tasks_value_at;
	iface->set_value_at = cal_model_tasks_set_value_at;
	iface->is_cell_editable = cal_model_tasks_is_cell_editable;

	iface->duplicate_value = cal_model_tasks_duplicate_value;
	iface->free_value = cal_model_tasks_free_value;
	iface->initialize_value = cal_model_tasks_initialize_value;
	iface->value_is_empty = cal_model_tasks_value_is_empty;
	iface->value_to_string = cal_model_tasks_value_to_string;
}

static void
e_cal_model_tasks_init (ECalModelTasks *model)
{
	model->priv = E_CAL_MODEL_TASKS_GET_PRIVATE (model);

	model->priv->highlight_due_today = TRUE;
	model->priv->highlight_overdue = TRUE;

	e_cal_model_set_component_kind (
		E_CAL_MODEL (model), ICAL_VTODO_COMPONENT);
}

ECalModel *
e_cal_model_tasks_new (ECalDataModel *data_model,
		       ESourceRegistry *registry,
		       EShell *shell)
{
	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), NULL);
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return g_object_new (
		E_TYPE_CAL_MODEL_TASKS,
		"data-model", data_model,
		"registry", registry,
		"shell", shell,
		NULL);
}

gboolean
e_cal_model_tasks_get_highlight_due_today (ECalModelTasks *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), FALSE);

	return model->priv->highlight_due_today;
}

void
e_cal_model_tasks_set_highlight_due_today (ECalModelTasks *model,
                                           gboolean highlight)
{
	g_return_if_fail (E_IS_CAL_MODEL_TASKS (model));

	if (model->priv->highlight_due_today == highlight)
		return;

	model->priv->highlight_due_today = highlight;

	g_object_notify (G_OBJECT (model), "highlight-due-today");
}

const gchar *
e_cal_model_tasks_get_color_due_today (ECalModelTasks *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), NULL);

	return model->priv->color_due_today;
}

void
e_cal_model_tasks_set_color_due_today (ECalModelTasks *model,
                                       const gchar *color_due_today)
{
	g_return_if_fail (E_IS_CAL_MODEL_TASKS (model));
	g_return_if_fail (color_due_today != NULL);

	if (g_strcmp0 (model->priv->color_due_today, color_due_today) == 0)
		return;

	g_free (model->priv->color_due_today);
	model->priv->color_due_today = g_strdup (color_due_today);

	g_object_notify (G_OBJECT (model), "color-due-today");
}

gboolean
e_cal_model_tasks_get_highlight_overdue (ECalModelTasks *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), FALSE);

	return model->priv->highlight_overdue;
}

void
e_cal_model_tasks_set_highlight_overdue (ECalModelTasks *model,
                                         gboolean highlight)
{
	g_return_if_fail (E_IS_CAL_MODEL_TASKS (model));

	if (model->priv->highlight_overdue == highlight)
		return;

	model->priv->highlight_overdue = highlight;

	g_object_notify (G_OBJECT (model), "highlight-overdue");
}

const gchar *
e_cal_model_tasks_get_color_overdue (ECalModelTasks *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL_TASKS (model), NULL);

	return model->priv->color_overdue;
}

void
e_cal_model_tasks_set_color_overdue (ECalModelTasks *model,
                                     const gchar *color_overdue)
{
	g_return_if_fail (E_IS_CAL_MODEL_TASKS (model));
	g_return_if_fail (color_overdue != NULL);

	if (g_strcmp0 (model->priv->color_overdue, color_overdue) == 0)
		return;

	g_free (model->priv->color_overdue);
	model->priv->color_overdue = g_strdup (color_overdue);

	g_object_notify (G_OBJECT (model), "color-overdue");
}

/**
 * e_cal_model_tasks_mark_comp_complete
 * @model: Currently not used...
 * @comp_data: Component of our interest
 *
 * Marks component as complete and commits changes to the calendar backend.
 **/
void
e_cal_model_tasks_mark_comp_complete (ECalModelTasks *model,
                                      ECalModelComponent *comp_data)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (comp_data != NULL);

	/* we will receive changes when committed, so don't do this */
	/*e_table_model_pre_change (E_TABLE_MODEL (model));*/

	ensure_task_complete (comp_data, -1);

	/*e_table_model_row_changed (E_TABLE_MODEL (model), model_row);*/

	e_cal_model_modify_component (E_CAL_MODEL (model), comp_data, E_CAL_OBJ_MOD_ALL);
}

/**
 * e_cal_model_tasks_mark_comp_incomplete
 * @model: Currently not used...
 * @comp_data: Component of our interest
 *
 * Marks component as incomplete and commits changes to the calendar backend.
 **/
void
e_cal_model_tasks_mark_comp_incomplete (ECalModelTasks *model,
                                        ECalModelComponent *comp_data)
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

	e_cal_model_modify_component (E_CAL_MODEL (model), comp_data, E_CAL_OBJ_MOD_ALL);
}

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
