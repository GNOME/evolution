/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Evolution calendar - Data model for ETable
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
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

#include <math.h>
#include <sys/types.h>

#include <ctype.h>

#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnome/gnome-i18n.h>
#include <gal/widgets/e-unicode.h>
#include <gal/util/e-util.h>
#include <e-util/e-time-utils.h>
#include <cal-util/timeutil.h>
#include "calendar-commands.h"
#include "calendar-config.h"
#include "comp-util.h"
#include "itip-utils.h"
#include "calendar-model.h"
#include "evolution-activity-client.h"
#include "e-cell-date-edit-text.h"
#include "misc.h"

/* This specifies how often we refresh the list, so that completed tasks are
   hidden according to the config setting, and overdue tasks change color etc.
   It is in milliseconds, so this is 10 minutes.
   Note that if the user is editing an item in the list, they will probably
   lose their edit, so this isn't ideal. */
#define CALENDAR_MODEL_REFRESH_TIMEOUT	1000 * 60 * 10

/* These hold the date values of the objects, so we can free the values when
   we no longer need them. */
typedef struct _CalendarModelObjectData CalendarModelObjectData;
struct _CalendarModelObjectData {
	ECellDateEditValue *dtstart;
	ECellDateEditValue *dtend;
	ECellDateEditValue *due;
	ECellDateEditValue *completed;
};

/* We use a pointer to this value to indicate that the property is not set. */
static ECellDateEditValue unset_date_edit_value;

/* Private part of the ECalendarModel structure */
struct _CalendarModelPrivate {
	/* Calendar client we are using */
	CalClient *client;

	/* Types of objects we are dealing with */
	CalObjType type;

	/* S-expression for query and the query object */
	char *sexp;
	CalQuery *query;

	/* Array of pointers to calendar objects */
	GArray *objects;

	/* Array of CalendarModelObjectData* holding data for each of the
	   objects in the objects array above. */
	GArray *objects_data;

	/* UID -> array index hash */
	GHashTable *uid_index_hash;

	/* Type of components to create when using click-to-add in the table */
	CalComponentVType new_comp_vtype;

	/* Whether we display dates in 24-hour format. */
	gboolean use_24_hour_format;

	/* The default category to use when creating new tasks, e.g. when the
	   filter is set to a certain category we use that category when
	   creating a new task. */
	gchar *default_category;

	/* Addresses for determining icons */
	EAccountList *accounts;
	
	/* The current timezone. */
	icaltimezone *zone;

	/* The id of our timeout function for refreshing the list. */
	gint timeout_id;

	/* The activity client used to show messages on the status bar. */
	EvolutionActivityClient *activity;
};



static void calendar_model_class_init (CalendarModelClass *class);
static void calendar_model_init (CalendarModel *model);
static void calendar_model_finalize (GObject *object);

static int calendar_model_column_count (ETableModel *etm);
static int calendar_model_row_count (ETableModel *etm);
static void *calendar_model_value_at (ETableModel *etm, int col, int row);
static void calendar_model_set_value_at (ETableModel *etm, int col, int row, const void *value);
static gboolean calendar_model_is_cell_editable (ETableModel *etm, int col, int row);
static void calendar_model_append_row (ETableModel *etm, ETableModel *source, gint row);
static void *calendar_model_duplicate_value (ETableModel *etm, int col, const void *value);
static void calendar_model_free_value (ETableModel *etm, int col, void *value);
static void *calendar_model_initialize_value (ETableModel *etm, int col);
static gboolean calendar_model_value_is_empty (ETableModel *etm, int col, const void *value);
static char * calendar_model_value_to_string (ETableModel *etm, int col, const void *value);
static int remove_object (CalendarModel *model, const char *uid);
static void ensure_task_complete (CalComponent *comp,
				  time_t completed_date);
static void ensure_task_not_complete (CalComponent *comp);

static ETableModelClass *parent_class;



/**
 * calendar_model_get_type:
 * @void:
 *
 * Registers the #CalendarModel class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #CalendarModel class.
 **/

E_MAKE_TYPE (calendar_model, "CalendarModel", CalendarModel, calendar_model_class_init,
	     calendar_model_init, E_TABLE_MODEL_TYPE);

/* Class initialization function for the calendar table model */
static void
calendar_model_class_init (CalendarModelClass *class)
{
	GObjectClass *object_class;
	ETableModelClass *etm_class;

	object_class = (GObjectClass *) class;
	etm_class = (ETableModelClass *) class;

	parent_class = g_type_class_peek_parent (class);

	object_class->finalize = calendar_model_finalize;

	etm_class->column_count = calendar_model_column_count;
	etm_class->row_count = calendar_model_row_count;
	etm_class->value_at = calendar_model_value_at;
	etm_class->set_value_at = calendar_model_set_value_at;
	etm_class->is_cell_editable = calendar_model_is_cell_editable;
	etm_class->append_row = calendar_model_append_row;
	etm_class->duplicate_value = calendar_model_duplicate_value;
	etm_class->free_value = calendar_model_free_value;
	etm_class->initialize_value = calendar_model_initialize_value;
	etm_class->value_is_empty = calendar_model_value_is_empty;
	etm_class->value_to_string = calendar_model_value_to_string;
}


static gboolean
calendar_model_timeout_cb (gpointer data)
{
	CalendarModel *model;

	g_return_val_if_fail (IS_CALENDAR_MODEL (data), FALSE);

	model = CALENDAR_MODEL (data);

	GDK_THREADS_ENTER ();

	calendar_model_refresh (model);

	GDK_THREADS_LEAVE ();
	return TRUE;
}


/* Object initialization function for the calendar table model */
static void
calendar_model_init (CalendarModel *model)
{
	CalendarModelPrivate *priv;

	priv = g_new0 (CalendarModelPrivate, 1);
	model->priv = priv;

	priv->sexp = g_strdup ("#t"); /* match all by default */
	priv->query = NULL;

	priv->objects = g_array_new (FALSE, TRUE, sizeof (CalComponent *));
	priv->objects_data = g_array_new (FALSE, FALSE, sizeof (CalendarModelObjectData));
	priv->uid_index_hash = g_hash_table_new (g_str_hash, g_str_equal);
	priv->new_comp_vtype = CAL_COMPONENT_EVENT;
	priv->use_24_hour_format = TRUE;

	priv->timeout_id = g_timeout_add (CALENDAR_MODEL_REFRESH_TIMEOUT,
					  calendar_model_timeout_cb, model);

	priv->accounts = itip_addresses_get ();
	
	priv->zone = NULL;

	priv->activity = NULL;

	/* Preload here, to avoid corba calls later */
	/* Gross hack because gnome-canvas is not re-entrant */
	calendar_config_get_tasks_due_today_color ();
	calendar_config_get_tasks_overdue_color ();
	g_free (calendar_config_get_hide_completed_tasks_sexp ());
}

static void
calendar_model_free_object_data (CalendarModel *model,
				 CalendarModelObjectData *object_data)
{
	if (object_data->dtstart != &unset_date_edit_value)
		g_free (object_data->dtstart);

	if (object_data->dtend != &unset_date_edit_value)
		g_free (object_data->dtend);

	if (object_data->due != &unset_date_edit_value)
		g_free (object_data->due);

	if (object_data->completed != &unset_date_edit_value)
		g_free (object_data->completed);
}

/* Called from g_hash_table_foreach_remove(), frees a stored UID->index
 * mapping.
 */
static gboolean
free_uid_index (gpointer key, gpointer value, gpointer data)
{
	int *idx;

	idx = value;
	g_free (idx);

	return TRUE;
}

/* Frees the objects stored in the calendar model */
static void
free_objects (CalendarModel *model)
{
	CalendarModelPrivate *priv;
	int i;

	priv = model->priv;

	g_hash_table_foreach_remove (priv->uid_index_hash, free_uid_index, NULL);

	for (i = 0; i < priv->objects->len; i++) {
		CalComponent *comp;
		CalendarModelObjectData *object_data;

		comp = g_array_index (priv->objects, CalComponent *, i);
		g_assert (comp != NULL);
		g_object_unref (comp);

		object_data = &g_array_index (priv->objects_data,
					      CalendarModelObjectData, i);
		calendar_model_free_object_data (model, object_data);
	}

	g_array_set_size (priv->objects, 0);
	g_array_set_size (priv->objects_data, 0);
}

/* Destroy handler for the calendar table model */
static void
calendar_model_finalize (GObject *object)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CALENDAR_MODEL (object));

	model = CALENDAR_MODEL (object);
	priv = model->priv;

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	/* Free the calendar client interface object */

	if (priv->client) {
		g_signal_handlers_disconnect_matched (priv->client, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);
		g_object_unref (priv->client);
		priv->client = NULL;
	}

	if (priv->sexp) {
		g_free (priv->sexp);
		priv->sexp = NULL;
	}

	if (priv->query) {
		g_signal_handlers_disconnect_matched (priv->query, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, model);
		g_object_unref (priv->query);
		priv->query = NULL;
	}

	/* Free the uid->index hash data and the array of UIDs */

	free_objects (model);

	g_hash_table_destroy (priv->uid_index_hash);
	priv->uid_index_hash = NULL;

	g_array_free (priv->objects, TRUE);
	priv->objects = NULL;

	g_array_free (priv->objects_data, TRUE);
	priv->objects_data = NULL;

	g_free (priv->default_category);

	if (priv->activity) {
		g_object_unref (priv->activity);
		priv->activity = NULL;
	}

	/* Free the private structure */

	g_free (priv);
	model->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}



/* ETableModel methods */

/* column_count handler for the calendar table model */
static int
calendar_model_column_count (ETableModel *etm)
{
	return CAL_COMPONENT_FIELD_NUM_FIELDS;
}

/* row_count handler for the calendar table model */
static int
calendar_model_row_count (ETableModel *etm)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;

	model = CALENDAR_MODEL (etm);
	priv = model->priv;

	return priv->objects->len;
}

/* Builds a string based on the list of CATEGORIES properties of a calendar
 * component.
 */
static char *
get_categories (CalComponent *comp)
{
	const char *categories;

	cal_component_get_categories (comp, &categories);

	return categories ? (char*) categories : "";
}

/* Returns a string based on the CLASSIFICATION property of a calendar component */
static char *
get_classification (CalComponent *comp)
{
	CalComponentClassification classif;

	cal_component_get_classification (comp, &classif);

	switch (classif) {
	case CAL_COMPONENT_CLASS_PRIVATE:
		return _("Private");

	case CAL_COMPONENT_CLASS_CONFIDENTIAL:
		return _("Confidential");

	default:
		return _("Public");
	}
}

/* Returns an ECellDateEditValue* for a COMPLETED property of a
   calendar component. Note that we cache these in the objects_data array so
   we can free them eventually. */
static ECellDateEditValue*
get_completed	(CalendarModel *model,
		 CalComponent  *comp,
		 int row)
{
	CalendarModelPrivate *priv;
	CalendarModelObjectData *object_data;
	struct icaltimetype *completed;

	priv = model->priv;

	object_data = &g_array_index (priv->objects_data,
				      CalendarModelObjectData, row);

	if (!object_data->completed) {
		cal_component_get_completed (comp, &completed);

		if (completed) {
			object_data->completed = g_new (ECellDateEditValue, 1);
			object_data->completed->tt = *completed;
			object_data->completed->zone = icaltimezone_get_utc_timezone ();
			cal_component_free_icaltimetype (completed);
		} else {
			object_data->completed = &unset_date_edit_value;
		}
	}

	return (object_data->completed == &unset_date_edit_value)
		? NULL : object_data->completed;
}

/* Returns an ECellDateEditValue* for a DTSTART, DTEND or DUE property of a
   calendar component. Note that we cache these in the objects_data array so
   we can free them eventually. */
static ECellDateEditValue*
get_date_edit_value (CalendarModel *model, CalComponent *comp,
		     int col, int row)
{
	CalendarModelPrivate *priv;
	CalComponentDateTime dt;
	CalendarModelObjectData *object_data;
	ECellDateEditValue **value;

	priv = model->priv;

	object_data = &g_array_index (priv->objects_data,
				      CalendarModelObjectData, row);

	if (col == CAL_COMPONENT_FIELD_DTSTART)
		value = &object_data->dtstart;
	else if (col == CAL_COMPONENT_FIELD_DTEND)
		value = &object_data->dtend;
	else
		value = &object_data->due;

	if (!(*value)) {
		if (col == CAL_COMPONENT_FIELD_DTSTART)
			cal_component_get_dtstart (comp, &dt);
		else if (col == CAL_COMPONENT_FIELD_DTEND)
			cal_component_get_dtend (comp, &dt);
		else
			cal_component_get_due (comp, &dt);

		if (dt.value) {
			CalClientGetStatus status;
			icaltimezone *zone;

			/* For a DTEND with a DATE value, we subtract 1 from
			   the day to display it. */
			if (col == CAL_COMPONENT_FIELD_DTEND
			    && dt.value->is_date) {
				icaltime_adjust (dt.value, -1, 0, 0, 0);
			}

			*value = g_new (ECellDateEditValue, 1);
			(*value)->tt = *dt.value;

			/* FIXME: TIMEZONES: Handle error. */
			status = cal_client_get_timezone (model->priv->client,
							  dt.tzid, &zone);
			(*value)->zone = zone;
		} else {
			*value = &unset_date_edit_value;
		}

		cal_component_free_datetime (&dt);
	}

	return (*value == &unset_date_edit_value) ? NULL : *value;
}

/* Builds a string for the GEO property of a calendar component */
static char*
get_geo (CalComponent *comp)
{
	struct icalgeotype *geo;
	static gchar buf[32];

	cal_component_get_geo (comp, &geo);

	if (!geo)
		buf[0] = '\0';
	else {
		g_snprintf (buf, sizeof (buf), "%g %s, %g %s",
			    fabs (geo->lat),
			    geo->lat >= 0.0 ? _("N") : _("S"),
			    fabs (geo->lon),
			    geo->lon >= 0.0 ? _("E") : _("W"));
		cal_component_free_geo (geo);
	}

	return buf;
}

/* Builds a string for the PERCENT property of a calendar component */
static int
get_percent (CalComponent *comp)
{
	int *percent, retval;

	cal_component_get_percent (comp, &percent);

	if (percent) {
		retval = *percent;
		cal_component_free_percent (percent);
	} else {
		retval = -1;
	}

	return retval;
}

/* Builds a string for the PRIORITY property of a calendar component */
static char *
get_priority (CalComponent *comp)
{
	int *priority;
	char *retval = "";

	cal_component_get_priority (comp, &priority);

	if (priority) {
		retval = cal_util_priority_to_string (*priority);
		cal_component_free_priority (priority);
	}

	return retval;
}

/* Builds a string for the SUMMARY property of a calendar component */
static char *
get_summary (CalComponent *comp)
{
	CalComponentText summary;

	cal_component_get_summary (comp, &summary);

	if (summary.value)
		return (char *) summary.value;
	else
		return "";
}

/* Builds a string for the TRANSPARENCY property of a calendar component */
static char *
get_transparency (CalComponent *comp)
{
	CalComponentTransparency transp;

	cal_component_get_transparency (comp, &transp);

	if (transp == CAL_COMPONENT_TRANSP_TRANSPARENT)
		return _("Free");
	else
		return _("Busy");
}

/* Builds a string for the URL property of a calendar component */
static char *
get_url (CalComponent *comp)
{
	const char *url;

	cal_component_get_url (comp, &url);

	if (url)
		return (char *) url;
	else
		return "";
}

/* Returns whether the completion date has been set on a component */
static gboolean
is_complete (CalComponent *comp)
{
	struct icaltimetype *t;
	gboolean retval;

	cal_component_get_completed (comp, &t);
	retval = (t != NULL);

	if (retval)
		cal_component_free_icaltimetype (t);

	return retval;
}

typedef enum {
	CALENDAR_MODEL_DUE_NEVER,
	CALENDAR_MODEL_DUE_FUTURE,
	CALENDAR_MODEL_DUE_TODAY,
	CALENDAR_MODEL_DUE_OVERDUE,
	CALENDAR_MODEL_DUE_COMPLETE
} CalendarModelDueStatus;


static CalendarModelDueStatus
get_due_status (CalendarModel *model, CalComponent *comp)
{
	CalendarModelPrivate *priv;
	CalComponentDateTime dt;
	CalendarModelDueStatus retval;

	priv = model->priv;
	
	cal_component_get_due (comp, &dt);

	/* First, do we have a due date? */

	if (!dt.value)
		retval = CALENDAR_MODEL_DUE_NEVER;
	else {
		struct icaltimetype now_tt;
		CalClientGetStatus status;
		icaltimezone *zone;

		/* Second, is it already completed? */

		if (is_complete (comp)) {
			retval = CALENDAR_MODEL_DUE_COMPLETE;
			goto out;
		}

		/* Third, are we overdue as of right now? */

		if (dt.value->is_date) {
			int cmp;
			
			now_tt = icaltime_current_time_with_zone (priv->zone);
			cmp = icaltime_compare_date_only (*dt.value, now_tt);
			
			if (cmp < 0)
				retval = CALENDAR_MODEL_DUE_OVERDUE;
			else if (cmp == 0)
				retval = CALENDAR_MODEL_DUE_TODAY;
			else
				retval = CALENDAR_MODEL_DUE_FUTURE;
		} else {
			/* Get the current time in the same timezone as the DUE date.*/
			status = cal_client_get_timezone (model->priv->client, dt.tzid,
							  &zone);
			if (status != CAL_CLIENT_GET_SUCCESS) {
				retval = CALENDAR_MODEL_DUE_FUTURE;
				goto out;
			}
			
			now_tt = icaltime_current_time_with_zone (zone);

			if (icaltime_compare (*dt.value, now_tt) <= 0) 
				retval = CALENDAR_MODEL_DUE_OVERDUE;
			else
				if (icaltime_compare_date_only (*dt.value, now_tt) == 0)
					retval = CALENDAR_MODEL_DUE_TODAY;
				else
					retval = CALENDAR_MODEL_DUE_FUTURE;
		}
	}

 out:

	cal_component_free_datetime (&dt);

	return retval;
}

/* Returns whether a component is overdue. */
static gboolean
is_overdue (CalendarModel *model, CalComponent *comp)
{
	switch (get_due_status (model, comp)) {
	case CALENDAR_MODEL_DUE_NEVER:
	case CALENDAR_MODEL_DUE_FUTURE:
	case CALENDAR_MODEL_DUE_COMPLETE:
		return FALSE;
	case CALENDAR_MODEL_DUE_TODAY:
	case CALENDAR_MODEL_DUE_OVERDUE:
		return TRUE;
	}

	return FALSE;
}

/* Computes the color to be used to display a component */
static const char *
get_color (CalendarModel *model, CalComponent *comp)
{
	switch (get_due_status (model, comp)) {
	case CALENDAR_MODEL_DUE_NEVER:
	case CALENDAR_MODEL_DUE_FUTURE:
	case CALENDAR_MODEL_DUE_COMPLETE:
		return NULL;
	case CALENDAR_MODEL_DUE_TODAY:
		return calendar_config_get_tasks_due_today_color ();
	case CALENDAR_MODEL_DUE_OVERDUE:
		return calendar_config_get_tasks_overdue_color ();
	}

	return NULL;
}

static void *
get_status (CalComponent *comp)
{
	icalproperty_status status;

	cal_component_get_status (comp, &status);

	switch (status) {
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
		g_assert_not_reached ();
		return NULL;
	}
}

#if 0
static void *
get_location (CalComponent *comp)
{
	const char *location;

	cal_component_get_location (comp, &location);
	return (void*) location;
}
#endif

/* value_at handler for the calendar table model */
static void *
calendar_model_value_at (ETableModel *etm, int col, int row)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;
	CalComponent *comp;

	model = CALENDAR_MODEL (etm);
	priv = model->priv;

	g_return_val_if_fail (col >= 0 && col < CAL_COMPONENT_FIELD_NUM_FIELDS, NULL);
	g_return_val_if_fail (row >= 0 && row < priv->objects->len, NULL);

	comp = g_array_index (priv->objects, CalComponent *, row);
	g_assert (comp != NULL);

#if 0
	g_print ("In calendar_model_value_at: %i\n", col);
#endif

	switch (col) {
	case CAL_COMPONENT_FIELD_CATEGORIES:
		return get_categories (comp);

	case CAL_COMPONENT_FIELD_CLASSIFICATION:
		return get_classification (comp);

	case CAL_COMPONENT_FIELD_COMPLETED:
		return get_completed (model, comp, row);

	case CAL_COMPONENT_FIELD_DTEND:
	case CAL_COMPONENT_FIELD_DTSTART:
	case CAL_COMPONENT_FIELD_DUE:
		return get_date_edit_value (model, comp, col, row);

	case CAL_COMPONENT_FIELD_GEO:
		return get_geo (comp);

	case CAL_COMPONENT_FIELD_PERCENT:
		return GINT_TO_POINTER (get_percent (comp));

	case CAL_COMPONENT_FIELD_PRIORITY:
		return get_priority (comp);

	case CAL_COMPONENT_FIELD_SUMMARY:
		return get_summary (comp);

	case CAL_COMPONENT_FIELD_TRANSPARENCY:
		return get_transparency (comp);

	case CAL_COMPONENT_FIELD_URL:
		return get_url (comp);

	case CAL_COMPONENT_FIELD_HAS_ALARMS:
		return GINT_TO_POINTER (cal_component_has_alarms (comp));

	case CAL_COMPONENT_FIELD_ICON:
	{
		GSList *attendees = NULL, *sl;		
		gint retval = 0;

		if (cal_component_has_recurrences (comp))
			return GINT_TO_POINTER (1);

		if (itip_organizer_is_user (comp, priv->client))
			return GINT_TO_POINTER (3);
		
		cal_component_get_attendee_list (comp, &attendees);
		for (sl = attendees; sl != NULL; sl = sl->next) {
			CalComponentAttendee *ca = sl->data;
			const char *text;

			text = itip_strip_mailto (ca->value);
			if (e_account_list_find(priv->accounts, E_ACCOUNT_FIND_ID_ADDRESS, text) != NULL) {
				if (ca->delto != NULL)
					retval = 3;
				else
					retval = 2;
				break;
			}
		}

		cal_component_free_attendee_list (attendees);
		return GINT_TO_POINTER (retval);		
		break;
	}
	case CAL_COMPONENT_FIELD_COMPLETE:
		return GINT_TO_POINTER (is_complete (comp));

	case CAL_COMPONENT_FIELD_RECURRING:
		return GINT_TO_POINTER (cal_component_has_recurrences (comp));

	case CAL_COMPONENT_FIELD_OVERDUE:
		return GINT_TO_POINTER (is_overdue (model, comp));

	case CAL_COMPONENT_FIELD_COLOR:
		return (void *) get_color (model, comp);

	case CAL_COMPONENT_FIELD_STATUS:
		return get_status (comp);

	case CAL_COMPONENT_FIELD_COMPONENT:
		return comp;

#if 0
	case CAL_COMPONENT_FIELD_LOCATION :
		return get_location (comp);
#endif

	default:
		g_message ("calendar_model_value_at(): Requested invalid column %d", col);
		g_assert_not_reached ();
		return NULL;
	}
}

/* Builds a list of categories from a comma-delimited string */
static GSList *
categories_from_string (const char *value)
{
	GSList *list;
	const char *categ_start;
	const char *categ_end;
	const char *p;

	if (!value)
		return NULL;

	list = NULL;

	categ_start = categ_end = NULL;

	for (p = value; *p; p++) {
		if (categ_start) {
			if (*p == ',') {
				char *c;

				c = g_strndup (categ_start, categ_end - categ_start + 1);
				list = g_slist_prepend (list, c);

				categ_start = categ_end = NULL;
			} else if (!isspace (*p))
				categ_end = p;
		} else if (!isspace (*p) && *p != ',')
			categ_start = categ_end = p;
	}

	if (categ_start) {
		char *c;

		c = g_strndup (categ_start, categ_end - categ_start + 1);
		list = g_slist_prepend (list, c);
	}

	return g_slist_reverse (list);
}

/* Sets the list of categories from a comma-delimited string */
static void
set_categories (CalComponent *comp, const char *value)
{
	GSList *list;
	GSList *l;

	list = categories_from_string (value);

	cal_component_set_categories_list (comp, list);

	for (l = list; l; l = l->next) {
		char *s;

		s = l->data;
		g_free (s);
	}

	g_slist_free (list);
}


static void
set_classification (CalComponent *comp,
		    const char *value)
{
	CalComponentClassification classif;

	if (!g_strcasecmp (value, _("Private")))
		classif = CAL_COMPONENT_CLASS_PRIVATE;
	else if (!g_strcasecmp (value, _("Confidential")))
		classif = CAL_COMPONENT_CLASS_CONFIDENTIAL;
	else
		classif = CAL_COMPONENT_CLASS_PUBLIC;

	cal_component_set_classification (comp, classif);
}


/* Called to set the "Date Completed" field. We also need to update the
   Status and Percent fields to make sure they match. */
static void
set_completed (CalendarModel *model, CalComponent *comp, const void *value)
{
	CalendarModelPrivate *priv = model->priv;
	ECellDateEditValue *dv = (ECellDateEditValue*) value;

	if (!dv) {
		ensure_task_not_complete (comp);
	} else {
		time_t t;

		if (dv->tt.is_date) {
			/* If its a date, it will be floating, 
			   but completed needs a date time value */
			dv->tt.is_date = FALSE;
			t = icaltime_as_timet_with_zone (dv->tt, priv->zone);
		} else {
			/* We assume that COMPLETED is entered in the current timezone,
			   even though it gets stored in UTC. */
			t = icaltime_as_timet_with_zone (dv->tt, dv->zone);
		}
		
		ensure_task_complete (comp, t);
	}
}

/* Sets a CalComponentDateTime value */
static void
set_datetime (CalendarModel *model, CalComponent *comp, const void *value,
	      void (* set_func) (CalComponent *comp, CalComponentDateTime *dt),
	      gboolean is_dtend)
{
	ECellDateEditValue *dv = (ECellDateEditValue*) value;

	if (!dv) {
		(* set_func) (comp, NULL);
	} else {
		CalComponentDateTime dt;

		dt.value = &dv->tt;
		dt.tzid = icaltimezone_get_tzid (dv->zone);

		/* For a DTEND with a DATE value, we add 1 day to it. */
		if (is_dtend && dt.value->is_date) {
			icaltime_adjust (dt.value, 1, 0, 0, 0);
		}

		(* set_func) (comp, &dt);
	}
}

/* FIXME: We need to set the "transient_for" property for the dialog, but the
 * model doesn't know anything about the windows.
 */
static void
show_geo_warning (void)
{
	GtkWidget *dialog;

	dialog = gnome_message_box_new (_("The geographical position must be entered "
					  "in the format: \n\n45.436845,125.862501"),
					GNOME_MESSAGE_BOX_ERROR,
					GNOME_STOCK_BUTTON_OK, NULL);
	gtk_widget_show (dialog);
}

/* Sets the geographical position value of a component */
static void
set_geo (CalComponent *comp, const char *value)
{
	double latitude, longitude;
	int matched;
	struct icalgeotype geo;

	if (string_is_empty (value)) {
		cal_component_set_geo (comp, NULL);
		return;
	}

	matched = sscanf (value, "%lg , %lg", &latitude, &longitude);

	if (matched != 2) {
		show_geo_warning ();
		return;
	}

	geo.lat = latitude;
	geo.lon = longitude;
	cal_component_set_geo (comp, &geo);
}

/* Sets the percent value of a calendar component */
static void
set_percent (CalComponent *comp, const void *value)
{
	gint percent = GPOINTER_TO_INT (value);

	g_return_if_fail (percent >= -1);
	g_return_if_fail (percent <= 100);

	/* A value of -1 means it isn't set. */
	if (percent == -1) {
		cal_component_set_percent (comp, NULL);
		ensure_task_not_complete (comp);
	} else {
		cal_component_set_percent (comp, &percent);

		if (percent == 100)
			ensure_task_complete (comp, -1);
		else {
			ensure_task_not_complete (comp);
			if (percent > 0)
				cal_component_set_status (comp, ICAL_STATUS_INPROCESS);
		}
	}
}

/* Sets the priority of a calendar component */
static void
set_priority (CalComponent *comp, const char *value)
{
	int priority;

	priority = cal_util_priority_from_string (value);
	/* If the priority is invalid (which should never happen) output a
	   warning and set it to undefined. */
	if (priority == -1) {
		g_warning ("Invalid priority");
		priority = 0;
	}

	cal_component_set_priority (comp, &priority);
}

/* Sets the summary of a calendar component */
static void
set_summary (CalComponent *comp, const char *value)
{
	CalComponentText text;

	if (string_is_empty (value)) {
		cal_component_set_summary (comp, NULL);
		return;
	}

	text.value = value;
	text.altrep = NULL; /* FIXME: should we preserve the old ALTREP? */

	cal_component_set_summary (comp, &text);
}

/* Sets the transparency of a calendar component */
static void
set_transparency (CalComponent *comp, const char *value)
{
	CalComponentTransparency transp;

	if (!g_strcasecmp (value, _("Free")))
		transp = CAL_COMPONENT_TRANSP_TRANSPARENT;
	else
		transp = CAL_COMPONENT_TRANSP_OPAQUE;

	cal_component_set_transparency (comp, transp);
}

/* Sets the URI of a calendar component */
static void
set_url (CalComponent *comp, const char *value)
{
	if (string_is_empty (value)) {
		cal_component_set_url (comp, NULL);
		return;
	}

	cal_component_set_url (comp, value);
}

/* Called to set the checkbutton field which indicates whether a task is
   complete. */
static void
set_complete (CalComponent *comp, const void *value)
{
	gint state = GPOINTER_TO_INT (value);

	if (state) {
		ensure_task_complete (comp, -1);
	} else {
		ensure_task_not_complete (comp);
	}
}

/* Sets the status of a calendar component. */
static void
set_status (CalComponent *comp, const char *value)
{
	icalproperty_status status;
	int percent;

	/* An empty string is the same as 'None'. */
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

	cal_component_set_status (comp, status);

	if (status == ICAL_STATUS_NEEDSACTION) {
		percent = 0;
		cal_component_set_percent (comp, &percent);
		cal_component_set_completed (comp, NULL);
	} else if (status == ICAL_STATUS_INPROCESS) {	
		ensure_task_not_complete (comp);	
		percent = 50;
		cal_component_set_percent (comp, &percent);
	} else if (status == ICAL_STATUS_COMPLETED) {
		ensure_task_complete (comp, -1);
	}
}

#if 0
static void
set_location (CalComponent *comp, const char *value)
{
	if (string_is_empty (value)) {
		cal_component_set_location (comp, NULL);
		return;
	}

	cal_component_set_location (comp, value);
}
#endif

/* set_value_at handler for the calendar table model */
static void
calendar_model_set_value_at (ETableModel *etm, int col, int row, const void *value)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;
	CalComponent *comp;

	model = CALENDAR_MODEL (etm);
	priv = model->priv;

	g_return_if_fail (col >= 0 && col < CAL_COMPONENT_FIELD_NUM_FIELDS);
	g_return_if_fail (row >= 0 && row < priv->objects->len);

	comp = g_array_index (priv->objects, CalComponent *, row);
	g_assert (comp != NULL);

#if 0
	g_print ("In calendar_model_set_value_at: %i\n", col);
#endif

	switch (col) {
	case CAL_COMPONENT_FIELD_CATEGORIES:
		set_categories (comp, value);
		break;

	case CAL_COMPONENT_FIELD_CLASSIFICATION:
		set_classification (comp, value);
		break;

	case CAL_COMPONENT_FIELD_COMPLETED:
		set_completed (model, comp, value);
		break;

	case CAL_COMPONENT_FIELD_DTEND:
		/* FIXME: Need to reset dtstart if dtend happens before it */
		set_datetime (model, comp, value, cal_component_set_dtend,
			      TRUE);
		break;

	case CAL_COMPONENT_FIELD_DTSTART:
		/* FIXME: Need to reset dtend if dtstart happens after it */
		set_datetime (model, comp, value, cal_component_set_dtstart,
			      FALSE);
		break;

	case CAL_COMPONENT_FIELD_DUE:
		set_datetime (model, comp, value, cal_component_set_due,
			      FALSE);
		break;

	case CAL_COMPONENT_FIELD_GEO:
		set_geo (comp, value);
		break;

	case CAL_COMPONENT_FIELD_PERCENT:
		set_percent (comp, value);
		break;

	case CAL_COMPONENT_FIELD_PRIORITY:
		set_priority (comp, value);
		break;

	case CAL_COMPONENT_FIELD_SUMMARY:
		set_summary (comp, value);
		break;

	case CAL_COMPONENT_FIELD_TRANSPARENCY:
		set_transparency (comp, value);
		break;

	case CAL_COMPONENT_FIELD_URL:
		set_url (comp, value);
		break;

	case CAL_COMPONENT_FIELD_COMPLETE:
		set_complete (comp, value);
		break;

	case CAL_COMPONENT_FIELD_STATUS:
		set_status (comp, value);
		break;

#if 0
	case CAL_COMPONENT_FIELD_LOCATION :
		set_location (comp, value);
		break;
#endif

	default:
		g_message ("calendar_model_set_value_at(): Requested invalid column %d", col);
		g_assert_not_reached ();
		return;
	}

	if (cal_client_update_object (priv->client, comp) != CAL_CLIENT_RESULT_SUCCESS)
		g_message ("calendar_model_set_value_at(): Could not update the object!");
}

/* is_cell_editable handler for the calendar table model */
static gboolean
calendar_model_is_cell_editable (ETableModel *etm, int col, int row)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;

	model = CALENDAR_MODEL (etm);
	priv = model->priv;

	g_return_val_if_fail (col >= 0 && col < CAL_COMPONENT_FIELD_NUM_FIELDS, FALSE);

	/* FIXME: We can't check this as 'click-to-add' passes row 0. */
	/*g_return_val_if_fail (row >= 0 && row < priv->objects->len, FALSE);*/

	switch (col) {
	case CAL_COMPONENT_FIELD_CATEGORIES:
	case CAL_COMPONENT_FIELD_CLASSIFICATION:
	case CAL_COMPONENT_FIELD_COMPLETED:
	case CAL_COMPONENT_FIELD_DTEND:
	case CAL_COMPONENT_FIELD_DTSTART:
	case CAL_COMPONENT_FIELD_DUE:
	case CAL_COMPONENT_FIELD_GEO:
	case CAL_COMPONENT_FIELD_PERCENT:
	case CAL_COMPONENT_FIELD_PRIORITY:
	case CAL_COMPONENT_FIELD_SUMMARY:
	case CAL_COMPONENT_FIELD_TRANSPARENCY:
	case CAL_COMPONENT_FIELD_URL:
	case CAL_COMPONENT_FIELD_COMPLETE:
	case CAL_COMPONENT_FIELD_STATUS:
		return TRUE;

	default:
		return FALSE;
	}
}

/* append_row handler for the calendar model */
static void
calendar_model_append_row (ETableModel *etm, ETableModel *source, gint row)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;
	CalComponent *comp;

	model = CALENDAR_MODEL (etm);
	priv = model->priv;

	/* Guard against saving before the calendar is open */
	if (!(priv->client && cal_client_get_load_state (priv->client) == CAL_CLIENT_LOAD_LOADED))
		return;

	/* FIXME: This should also support journal components */
	switch (priv->new_comp_vtype) {
	case CAL_COMPONENT_EVENT:
		comp = cal_comp_event_new_with_defaults (priv->client);
		break;
	case CAL_COMPONENT_TODO:
		comp = cal_comp_task_new_with_defaults (priv->client);
		break;
	default:
		comp = cal_component_new ();
		cal_component_set_new_vtype (comp, priv->new_comp_vtype);		
	}

	set_categories (comp, e_table_model_value_at(source, CAL_COMPONENT_FIELD_CATEGORIES, row));
	set_classification (comp, e_table_model_value_at(source, CAL_COMPONENT_FIELD_CLASSIFICATION, row));
	set_completed (model, comp, e_table_model_value_at(source, CAL_COMPONENT_FIELD_COMPLETED, row));
	/* FIXME: Need to reset dtstart if dtend happens before it */
	set_datetime (model, comp, e_table_model_value_at(source, CAL_COMPONENT_FIELD_DTEND, row), cal_component_set_dtend, TRUE);
	/* FIXME: Need to reset dtend if dtstart happens after it */
	set_datetime (model, comp, e_table_model_value_at(source, CAL_COMPONENT_FIELD_DTSTART, row), cal_component_set_dtstart, FALSE);
	set_datetime (model, comp, e_table_model_value_at(source, CAL_COMPONENT_FIELD_DUE, row), cal_component_set_due, FALSE);
	set_geo (comp, e_table_model_value_at(source, CAL_COMPONENT_FIELD_GEO, row));
	set_percent (comp, e_table_model_value_at(source, CAL_COMPONENT_FIELD_PERCENT, row));
	set_priority (comp, e_table_model_value_at(source, CAL_COMPONENT_FIELD_PRIORITY, row));
	set_summary (comp, e_table_model_value_at(source, CAL_COMPONENT_FIELD_SUMMARY, row));
	set_transparency (comp, e_table_model_value_at(source, CAL_COMPONENT_FIELD_TRANSPARENCY, row));
	set_url (comp, e_table_model_value_at(source, CAL_COMPONENT_FIELD_URL, row));
	set_complete (comp, e_table_model_value_at(source, CAL_COMPONENT_FIELD_COMPLETE, row));
	set_status (comp, e_table_model_value_at(source, CAL_COMPONENT_FIELD_STATUS, row));

	if (cal_client_update_object (priv->client, comp) != CAL_CLIENT_RESULT_SUCCESS) {
		/* FIXME: Show error dialog. */
		g_message ("calendar_model_append_row(): Could not add new object!");
	}

	g_object_unref (comp);
}

/* Duplicates a string value */
static char *
dup_string (const char *value)
{
	return g_strdup (value);
}

static void*
dup_date_edit_value (const void *value)
{
	ECellDateEditValue *dv, *orig_dv;

	if (value == NULL)
		return NULL;

	orig_dv = (ECellDateEditValue*) value;

	dv = g_new (ECellDateEditValue, 1);
	*dv = *orig_dv;

	return dv;
}

/* duplicate_value handler for the calendar table model */
static void *
calendar_model_duplicate_value (ETableModel *etm, int col, const void *value)
{
	g_return_val_if_fail (col >= 0 && col < CAL_COMPONENT_FIELD_NUM_FIELDS, NULL);

	/* They are almost all dup_string()s for now, but we'll have real fields
	 * later.
	 */

	switch (col) {
	case CAL_COMPONENT_FIELD_CATEGORIES:
	case CAL_COMPONENT_FIELD_CLASSIFICATION:
	case CAL_COMPONENT_FIELD_GEO:
	case CAL_COMPONENT_FIELD_PRIORITY:
	case CAL_COMPONENT_FIELD_SUMMARY:
	case CAL_COMPONENT_FIELD_TRANSPARENCY:
	case CAL_COMPONENT_FIELD_URL:
	case CAL_COMPONENT_FIELD_STATUS:
		return dup_string (value);

	case CAL_COMPONENT_FIELD_COMPLETED:
	case CAL_COMPONENT_FIELD_DTEND:
	case CAL_COMPONENT_FIELD_DTSTART:
	case CAL_COMPONENT_FIELD_DUE:
		return dup_date_edit_value (value);

	case CAL_COMPONENT_FIELD_HAS_ALARMS:
	case CAL_COMPONENT_FIELD_ICON:
	case CAL_COMPONENT_FIELD_COMPLETE:
	case CAL_COMPONENT_FIELD_PERCENT:
	case CAL_COMPONENT_FIELD_RECURRING:
	case CAL_COMPONENT_FIELD_OVERDUE:
	case CAL_COMPONENT_FIELD_COLOR:
		return (void *) value;

	case CAL_COMPONENT_FIELD_COMPONENT: {
		CalComponent *comp;

		comp = CAL_COMPONENT (value);
		g_object_ref (comp);
		return comp;
	}

	default:
		g_message ("calendar_model_duplicate_value(): Requested invalid column %d", col);
		return NULL;
	}
}

/* free_value handler for the calendar table model */
static void
calendar_model_free_value (ETableModel *etm, int col, void *value)
{
	g_return_if_fail (col >= 0 && col < CAL_COMPONENT_FIELD_NUM_FIELDS);

	switch (col) {
	case CAL_COMPONENT_FIELD_CATEGORIES:
		if (value)
			g_free (value);
		break;

	case CAL_COMPONENT_FIELD_CLASSIFICATION:
		break;

	case CAL_COMPONENT_FIELD_COMPLETED:
	case CAL_COMPONENT_FIELD_DTEND:
	case CAL_COMPONENT_FIELD_DTSTART:
	case CAL_COMPONENT_FIELD_DUE:
	case CAL_COMPONENT_FIELD_GEO:
	case CAL_COMPONENT_FIELD_PRIORITY:
	case CAL_COMPONENT_FIELD_SUMMARY:
	case CAL_COMPONENT_FIELD_STATUS:
		if (value)
			g_free (value);
		break;

	case CAL_COMPONENT_FIELD_TRANSPARENCY:
		break;

	case CAL_COMPONENT_FIELD_URL:
		if (value)
			g_free (value);
		break;

	case CAL_COMPONENT_FIELD_PERCENT:
	case CAL_COMPONENT_FIELD_HAS_ALARMS:
	case CAL_COMPONENT_FIELD_ICON:
	case CAL_COMPONENT_FIELD_COMPLETE:
	case CAL_COMPONENT_FIELD_RECURRING:
	case CAL_COMPONENT_FIELD_OVERDUE:
	case CAL_COMPONENT_FIELD_COLOR:
		break;

	case CAL_COMPONENT_FIELD_COMPONENT:
		if (value)
			g_object_unref (value);
		break;

	default:
		g_message ("calendar_model_free_value(): Requested invalid column %d", col);
	}
}

/* Initializes a string value */
static char *
init_string (void)
{
	return g_strdup ("");
}

/* initialize_value handler for the calendar table model */
static void *
calendar_model_initialize_value (ETableModel *etm, int col)
{
	CalendarModel *model;

	g_return_val_if_fail (col >= 0 && col < CAL_COMPONENT_FIELD_NUM_FIELDS, NULL);

	model = CALENDAR_MODEL (etm);

	switch (col) {
	case CAL_COMPONENT_FIELD_CATEGORIES:
		return g_strdup (model->priv->default_category ? model->priv->default_category : "");

	case CAL_COMPONENT_FIELD_CLASSIFICATION:
	case CAL_COMPONENT_FIELD_GEO:
	case CAL_COMPONENT_FIELD_PRIORITY:
	case CAL_COMPONENT_FIELD_SUMMARY:
	case CAL_COMPONENT_FIELD_TRANSPARENCY:
	case CAL_COMPONENT_FIELD_URL:
	case CAL_COMPONENT_FIELD_STATUS:
		return init_string ();

	case CAL_COMPONENT_FIELD_COMPLETED:
	case CAL_COMPONENT_FIELD_DTEND:
	case CAL_COMPONENT_FIELD_DTSTART:
	case CAL_COMPONENT_FIELD_DUE:
	case CAL_COMPONENT_FIELD_HAS_ALARMS:
	case CAL_COMPONENT_FIELD_ICON:
	case CAL_COMPONENT_FIELD_COMPLETE:
	case CAL_COMPONENT_FIELD_RECURRING:
	case CAL_COMPONENT_FIELD_OVERDUE:
	case CAL_COMPONENT_FIELD_COLOR:
	case CAL_COMPONENT_FIELD_COMPONENT:
		return NULL;

	case CAL_COMPONENT_FIELD_PERCENT:
		return GINT_TO_POINTER (-1);

	default:
		g_message ("calendar_model_initialize_value(): Requested invalid column %d", col);
		return NULL;
	}
}

/* value_is_empty handler for the calendar model. This should return TRUE
   unless a significant value has been set. The 'click-to-add' feature
   checks all fields to see if any are not empty and if so it adds a new
   row, so we only want to return FALSE if we have a useful object. */
static gboolean
calendar_model_value_is_empty (ETableModel *etm, int col, const void *value)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;

	g_return_val_if_fail (col >= 0 && col < CAL_COMPONENT_FIELD_NUM_FIELDS, TRUE);

	model = CALENDAR_MODEL (etm);
	priv = model->priv;

	switch (col) {
	case CAL_COMPONENT_FIELD_CATEGORIES:
		/* This could be a hack or not.  If the categories field only
		 * contains the default category, then it possibly means that
		 * the user has not entered anything at all in the click-to-add;
		 * the category is in the value because we put it there in
		 * calendar_model_initialize_value().
		 */
		if (priv->default_category && value && strcmp (priv->default_category, value) == 0)
			return TRUE;
		else
			return string_is_empty (value);

	case CAL_COMPONENT_FIELD_CLASSIFICATION: /* actually goes here, not by itself */
	case CAL_COMPONENT_FIELD_GEO:
	case CAL_COMPONENT_FIELD_PRIORITY:
	case CAL_COMPONENT_FIELD_SUMMARY:
	case CAL_COMPONENT_FIELD_TRANSPARENCY:
	case CAL_COMPONENT_FIELD_URL:
	case CAL_COMPONENT_FIELD_STATUS:
		return string_is_empty (value);

	case CAL_COMPONENT_FIELD_COMPLETED:
	case CAL_COMPONENT_FIELD_DTEND:
	case CAL_COMPONENT_FIELD_DTSTART:
	case CAL_COMPONENT_FIELD_DUE:
		return value ? FALSE : TRUE;

	case CAL_COMPONENT_FIELD_PERCENT:
		return (GPOINTER_TO_INT (value) < 0) ? TRUE : FALSE;

	case CAL_COMPONENT_FIELD_HAS_ALARMS:
	case CAL_COMPONENT_FIELD_ICON:
	case CAL_COMPONENT_FIELD_COMPLETE:
	case CAL_COMPONENT_FIELD_RECURRING:
	case CAL_COMPONENT_FIELD_OVERDUE:
	case CAL_COMPONENT_FIELD_COLOR:
	case CAL_COMPONENT_FIELD_COMPONENT:
		return TRUE;

	default:
		g_message ("calendar_model_value_is_empty(): Requested invalid column %d", col);
		return TRUE;
	}
}

static char*
date_value_to_string (ETableModel *etm, const void *value)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;
	ECellDateEditValue *dv = (ECellDateEditValue *) value;
	struct icaltimetype tt;
	struct tm tmp_tm;
	char buffer[64];

	model = CALENDAR_MODEL (etm);
	priv = model->priv;

	if (!dv)
		return g_strdup ("");

	/* We currently convert all the dates to the current timezone. */
	tt = dv->tt;
	icaltimezone_convert_time (&tt, dv->zone, priv->zone);

	tmp_tm.tm_year = tt.year - 1900;
	tmp_tm.tm_mon = tt.month - 1;
	tmp_tm.tm_mday = tt.day;
	tmp_tm.tm_hour = tt.hour;
	tmp_tm.tm_min = tt.minute;
	tmp_tm.tm_sec = tt.second;
	tmp_tm.tm_isdst = -1;

	tmp_tm.tm_wday = time_day_of_week (tt.day, tt.month - 1, tt.year);

	e_time_format_date_and_time (&tmp_tm, priv->use_24_hour_format,
				     TRUE, FALSE,
				     buffer, sizeof (buffer));
	return g_strdup (buffer);
}


static char *
calendar_model_value_to_string (ETableModel *etm, int col, const void *value)
{
	g_return_val_if_fail (col >= 0 && col < CAL_COMPONENT_FIELD_NUM_FIELDS, NULL);

	switch (col) {
	case CAL_COMPONENT_FIELD_CATEGORIES:
	case CAL_COMPONENT_FIELD_CLASSIFICATION:
	case CAL_COMPONENT_FIELD_GEO:
	case CAL_COMPONENT_FIELD_PRIORITY:
	case CAL_COMPONENT_FIELD_SUMMARY:
	case CAL_COMPONENT_FIELD_TRANSPARENCY:
	case CAL_COMPONENT_FIELD_URL:
	case CAL_COMPONENT_FIELD_STATUS:
		return g_strdup (value);

	case CAL_COMPONENT_FIELD_COMPLETED:
	case CAL_COMPONENT_FIELD_DTEND:
	case CAL_COMPONENT_FIELD_DTSTART:
	case CAL_COMPONENT_FIELD_DUE:
		return date_value_to_string (etm, value);

	case CAL_COMPONENT_FIELD_ICON:
		if (GPOINTER_TO_INT (value) == 0)
			return _("Normal");
		else if (GPOINTER_TO_INT (value) == 1)
			return _("Recurring");
		else
			return _("Assigned");

	case CAL_COMPONENT_FIELD_HAS_ALARMS:
	case CAL_COMPONENT_FIELD_COMPLETE:
	case CAL_COMPONENT_FIELD_RECURRING:
	case CAL_COMPONENT_FIELD_OVERDUE:
		return value ? _("Yes") : _("No");

	case CAL_COMPONENT_FIELD_COLOR:
		return NULL;

	case CAL_COMPONENT_FIELD_COMPONENT:
		return NULL;

	case CAL_COMPONENT_FIELD_PERCENT:
		if (GPOINTER_TO_INT (value) < 0)
			return g_strdup ("N/A");
		else
			return g_strdup_printf ("%i%%", GPOINTER_TO_INT (value));

	default:
		g_message ("calendar_model_value_as_string(): Requested invalid column %d", col);
		return NULL;
	}
}



/**
 * calendar_model_new:
 *
 * Creates a new calendar model.  It must be told about the calendar client
 * interface object it will monitor with calendar_model_set_cal_client().
 *
 * Return value: A newly-created calendar model.
 **/
CalendarModel *
calendar_model_new (void)
{
	return CALENDAR_MODEL (g_object_new (TYPE_CALENDAR_MODEL, NULL));
}


/* Callback used when a component is updated in the live query */
static void
query_obj_updated_cb (CalQuery *query, const char *uid,
		      gboolean query_in_progress, int n_scanned, int total,
		      gpointer data)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;
	int orig_idx;
	CalComponent *new_comp;
	const char *new_comp_uid;
	int *new_idx;
	CalClientGetStatus status;
	CalendarModelObjectData new_object_data = { NULL, NULL, NULL, NULL };

	model = CALENDAR_MODEL (data);
	priv = model->priv;

	e_table_model_pre_change (E_TABLE_MODEL (model));

	orig_idx = remove_object (model, uid);

	status = cal_client_get_object (priv->client, uid, &new_comp);

	switch (status) {
	case CAL_CLIENT_GET_SUCCESS:
		/* Insert the object into the model */

		cal_component_get_uid (new_comp, &new_comp_uid);

		if (orig_idx == -1) {
			/* The object not in the model originally, so we just append it */

			g_array_append_val (priv->objects, new_comp);
			g_array_append_val (priv->objects_data, new_object_data);

			new_idx = g_new (int, 1);
			*new_idx = priv->objects->len - 1;

			g_hash_table_insert (priv->uid_index_hash, (char *) new_comp_uid, new_idx);
			e_table_model_row_inserted (E_TABLE_MODEL (model), *new_idx);
		} else {
			int i;

			/* Insert the new version of the object in its old position */

			g_array_insert_val (priv->objects, orig_idx, new_comp);
			g_array_insert_val (priv->objects_data, orig_idx,
					    new_object_data);

			new_idx = g_new (int, 1);
			*new_idx = orig_idx;
			g_hash_table_insert (priv->uid_index_hash, (char *) new_comp_uid, new_idx);

			/* Increase the indices of all subsequent objects */

			for (i = orig_idx + 1; i < priv->objects->len; i++) {
				CalComponent *comp;
				int *comp_idx;
				const char *comp_uid;

				comp = g_array_index (priv->objects, CalComponent *, i);
				g_assert (comp != NULL);

				cal_component_get_uid (comp, &comp_uid);

				comp_idx = g_hash_table_lookup (priv->uid_index_hash, comp_uid);
				g_assert (comp_idx != NULL);

				(*comp_idx)++;
			}

			e_table_model_row_changed (E_TABLE_MODEL (model), *new_idx);
		}

		break;

	case CAL_CLIENT_GET_NOT_FOUND:
		/* Nothing; the object may have been removed from the server.  We just
		 * notify that the old object was deleted.
		 */
		if (orig_idx != -1)
			e_table_model_row_deleted (E_TABLE_MODEL (model), orig_idx);
		else
			e_table_model_no_change (E_TABLE_MODEL (model));

		break;

	case CAL_CLIENT_GET_SYNTAX_ERROR:
		g_message ("obj_updated_cb(): Syntax error when getting object `%s'", uid);

		/* Same notification as above */
		if (orig_idx != -1)
			e_table_model_row_deleted (E_TABLE_MODEL (model), orig_idx);
		else
			e_table_model_no_change (E_TABLE_MODEL (model));

		break;

	default:
		g_assert_not_reached ();
	}
}

/* Callback used when a component is removed from the live query */
static void
query_obj_removed_cb (CalQuery *query, const char *uid, gpointer data)
{
	CalendarModel *model;
	int idx;

	model = CALENDAR_MODEL (data);

	e_table_model_pre_change (E_TABLE_MODEL (model));

	idx = remove_object (model, uid);

	if (idx != -1)
		e_table_model_row_deleted (E_TABLE_MODEL (model), idx);
	else
		e_table_model_no_change (E_TABLE_MODEL (model));
}

/* Callback used when a query ends */
static void
query_query_done_cb (CalQuery *query, CalQueryDoneStatus status, const char *error_str, gpointer data)
{
	CalendarModel *model;

	model = CALENDAR_MODEL (data);

	/* FIXME */

	calendar_model_set_status_message (model, NULL);

	if (status != CAL_QUERY_DONE_SUCCESS)
		g_warning ("query done: %s\n", error_str);
}

/* Callback used when an evaluation error occurs when running a query */
static void
query_eval_error_cb (CalQuery *query, const char *error_str, gpointer data)
{
	CalendarModel *model;

	model = CALENDAR_MODEL (data);

	/* FIXME */

	calendar_model_set_status_message (model, NULL);

	g_warning ("eval error: %s\n", error_str);
}

/* Builds a complete query sexp for the calendar model by adding the predicates
 * to filter only for the type of objects that the model supports, and
 * whether we want completed tasks.
 */
static char *
adjust_query_sexp (CalendarModel *model, const char *sexp)
{
	CalendarModelPrivate *priv;
	CalObjType type;
	char *type_sexp;
	char *completed_sexp;
	char *new_sexp;

	priv = model->priv;

	type = priv->type;

	if (!(type & CALOBJ_TYPE_ANY))
		type_sexp = g_strdup ("#t");
	else
		type_sexp = g_strdup_printf (
			"(or %s %s %s)",
			(type & CALOBJ_TYPE_EVENT) ? "(= (get-vtype) \"VEVENT\")" : "",
			(type & CALOBJ_TYPE_TODO) ? "(= (get-vtype) \"VTODO\")" : "",
			(type & CALOBJ_TYPE_JOURNAL) ? "(= (get-vtype) \"VJOURNAL\")" : "");

	/* Create a sub-expression for filtering out completed tasks, based on
	   the config settings. */
	completed_sexp = calendar_config_get_hide_completed_tasks_sexp ();

	new_sexp = g_strdup_printf ("(and %s %s %s)", type_sexp,
				    completed_sexp ? completed_sexp : "",
				    sexp);
	g_free (type_sexp);
	g_free (completed_sexp);

#if 0
	g_print ("Calendar model sexp:\n%s\n", new_sexp);
#endif

	return new_sexp;
}

/* Restarts a query */
static void
update_query (CalendarModel *model)
{
	CalendarModelPrivate *priv;
	CalQuery *old_query;
	char *real_sexp;

	priv = model->priv;

	e_table_model_pre_change (E_TABLE_MODEL (model));
	free_objects (model);
	e_table_model_changed (E_TABLE_MODEL (model));

	if (!(priv->client
	      && cal_client_get_load_state (priv->client) == CAL_CLIENT_LOAD_LOADED))
		return;

	old_query = priv->query;
	priv->query = NULL;

	if (old_query) {
		g_signal_handlers_disconnect_matched (old_query, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, model);
		g_object_unref (old_query);
	}

	g_assert (priv->sexp != NULL);
	real_sexp = adjust_query_sexp (model, priv->sexp);

	calendar_model_set_status_message (model, _("Searching"));
	priv->query = cal_client_get_query (priv->client, real_sexp);
	g_free (real_sexp);

	if (!priv->query) {
		g_message ("update_query(): Could not create the query");
		calendar_model_set_status_message (model, NULL);
		return;
	}

	g_signal_connect (priv->query, "obj_updated",
			  G_CALLBACK (query_obj_updated_cb), model);
	g_signal_connect (priv->query, "obj_removed",
			  G_CALLBACK (query_obj_removed_cb), model);
	g_signal_connect (priv->query, "query_done",
			  G_CALLBACK (query_query_done_cb), model);
	g_signal_connect (priv->query, "eval_error",
			  G_CALLBACK (query_eval_error_cb), model);
}

/* Callback used when a calendar is opened into the server */
static void
cal_opened_cb (CalClient *client, CalClientOpenStatus status, gpointer data)
{
	CalendarModel *model;

	model = CALENDAR_MODEL (data);

	if (status != CAL_CLIENT_OPEN_SUCCESS)
		return;

	update_query (model);
}


/* Removes an object from the model and updates all the indices that follow.
 * Returns the index of the object that was removed, or -1 if no object with
 * such UID was found.
 */
static int
remove_object (CalendarModel *model, const char *uid)
{
	CalendarModelPrivate *priv;
	int *idx;
	CalComponent *orig_comp;
	int i;
	int n;
	CalendarModelObjectData *object_data;

	priv = model->priv;

	/* Find the index of the object to be removed */

	idx = g_hash_table_lookup (priv->uid_index_hash, uid);
	if (!idx)
		return -1;

	orig_comp = g_array_index (priv->objects, CalComponent *, *idx);
	g_assert (orig_comp != NULL);

	/* Decrease the indices of all the objects that follow in the array */

	for (i = *idx + 1; i < priv->objects->len; i++) {
		CalComponent *comp;
		int *comp_idx;
		const char *comp_uid;

		comp = g_array_index (priv->objects, CalComponent *, i);
		g_assert (comp != NULL);

		cal_component_get_uid (comp, &comp_uid);

		comp_idx = g_hash_table_lookup (priv->uid_index_hash, comp_uid);
		g_assert (comp_idx != NULL);

		(*comp_idx)--;
		g_assert (*comp_idx >= 0);
	}

	/* Remove this object from the array and hash */

	g_hash_table_remove (priv->uid_index_hash, uid);
	g_array_remove_index (priv->objects, *idx);

	object_data = &g_array_index (priv->objects_data,
				      CalendarModelObjectData, *idx);
	calendar_model_free_object_data (model, object_data);
	g_array_remove_index (priv->objects_data, *idx);

	g_object_unref (orig_comp);

	n = *idx;
	g_free (idx);

	return n;
}

/* Displays messages on the status bar */
#define EVOLUTION_TASKS_PROGRESS_IMAGE "evolution-tasks-mini.png"
static GdkPixbuf *progress_icon[2] = { NULL, NULL };

void
calendar_model_set_status_message (CalendarModel *model, const char *message)
{
	extern EvolutionShellClient *global_shell_client; /* ugly */
	CalendarModelPrivate *priv;

	g_return_if_fail (IS_CALENDAR_MODEL (model));

	priv = model->priv;

	if (!message || !*message) {
		if (priv->activity) {
			g_object_unref (priv->activity);
			priv->activity = NULL;
		}
	}
	else if (!priv->activity) {
		int display;
		char *client_id = g_strdup_printf ("%p", model);

		if (progress_icon[0] == NULL)
			progress_icon[0] = gdk_pixbuf_new_from_file (EVOLUTION_IMAGESDIR "/" EVOLUTION_TASKS_PROGRESS_IMAGE, NULL);
		priv->activity = evolution_activity_client_new (
			global_shell_client, client_id,
			progress_icon, message, TRUE, &display);

		g_free (client_id);
	}
	else
		evolution_activity_client_update (priv->activity, message, -1.0);
}

/**
 * calendar_model_get_cal_client:
 * @model: A calendar model.
 *
 * Queries the calendar client interface object that a calendar model is using.
 *
 * Return value: A calendar client interface object.
 **/
CalClient *
calendar_model_get_cal_client (CalendarModel *model)
{
	CalendarModelPrivate *priv;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (IS_CALENDAR_MODEL (model), NULL);

	priv = model->priv;

	return priv->client;
}


/**
 * calendar_model_set_cal_client:
 * @model: A calendar model.
 * @client: A calendar client interface object.
 * @type: Type of objects to present.
 *
 * Sets the calendar client interface object that a calendar model will monitor.
 * It also sets the types of objects this model will present to an #ETable.
 **/
void
calendar_model_set_cal_client (CalendarModel *model, CalClient *client, CalObjType type)
{
	CalendarModelPrivate *priv;

	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_CALENDAR_MODEL (model));

	if (client)
		g_return_if_fail (IS_CAL_CLIENT (client));

	priv = model->priv;

	if (priv->client == client && priv->type == type)
		return;

	if (client)
		g_object_ref (client);

	if (priv->client) {
		g_signal_handlers_disconnect_matched (priv->client, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);
		g_object_unref (priv->client);
	}

	priv->client = client;
	priv->type = type;

	if (priv->client) {
		if (cal_client_get_load_state (priv->client) == CAL_CLIENT_LOAD_LOADED)
			update_query (model);
		else
			g_signal_connect (priv->client, "cal_opened", G_CALLBACK (cal_opened_cb), model);
	}
}

/**
 * calendar_model_set_query:
 * @model: A calendar model.
 * @sexp: Sexp that defines the query.
 * 
 * Sets the query sexp that a calendar model will use to filter its contents.
 **/
void
calendar_model_set_query (CalendarModel *model, const char *sexp)
{
	CalendarModelPrivate *priv;

	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_CALENDAR_MODEL (model));
	g_return_if_fail (sexp != NULL);

	priv = model->priv;

	if (priv->sexp)
		g_free (priv->sexp);

	priv->sexp = g_strdup (sexp);

	update_query (model);
}


/**
 * calendar_model_set_new_comp_vtype:
 * @model: A calendar model.
 * @vtype: Type of calendar components to create.
 *
 * Sets the type of calendar components that will be created by a calendar table
 * model when the click-to-add functionality of the table is used.
 **/
void
calendar_model_set_new_comp_vtype (CalendarModel *model, CalComponentVType vtype)
{
	CalendarModelPrivate *priv;

	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_CALENDAR_MODEL (model));
	g_return_if_fail (vtype != CAL_COMPONENT_NO_TYPE);

	priv = model->priv;
	priv->new_comp_vtype = vtype;
}

/**
 * calendar_model_get_new_comp_vtype:
 * @model: A calendar model.
 *
 * Queries the type of calendar components that are created by a calendar table
 * model when using the click-to-add functionality in a table.
 *
 * Return value: Type of components that are created.
 **/
CalComponentVType
calendar_model_get_new_comp_vtype (CalendarModel *model)
{
	CalendarModelPrivate *priv;

	g_return_val_if_fail (model != NULL, CAL_COMPONENT_NO_TYPE);
	g_return_val_if_fail (IS_CALENDAR_MODEL (model), CAL_COMPONENT_NO_TYPE);

	priv = model->priv;
	return priv->new_comp_vtype;
}


void
calendar_model_mark_task_complete (CalendarModel *model,
				   gint row)
{
	CalendarModelPrivate *priv;
	CalComponent *comp;

	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_CALENDAR_MODEL (model));

	priv = model->priv;

	g_return_if_fail (row >= 0 && row < priv->objects->len);

	comp = g_array_index (priv->objects, CalComponent *, row);
	g_assert (comp != NULL);

	ensure_task_complete (comp, -1);

	if (cal_client_update_object (priv->client, comp) != CAL_CLIENT_RESULT_SUCCESS)
		g_message ("calendar_model_mark_task_complete(): Could not update the object!");
}


/**
 * calendar_model_get_component:
 * @model: A calendar model.
 * @row: Row number of sought calendar component.
 *
 * Queries a calendar component from a calendar model based on its row number.
 *
 * Return value: The sought calendar component.
 **/
CalComponent *
calendar_model_get_component (CalendarModel *model,
			      gint	      row)
{
	CalendarModelPrivate *priv;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (IS_CALENDAR_MODEL (model), NULL);

	priv = model->priv;

	g_return_val_if_fail (row >= 0 && row < priv->objects->len, NULL);

	return g_array_index (priv->objects, CalComponent *, row);
}


/* This makes sure a task is marked as complete.
   It makes sure the "Date Completed" property is set. If the completed_date
   is not -1, then that is used, otherwise if the "Date Completed" property
   is not already set it is set to the current time.
   It makes sure the percent is set to 100, and that the status is "Completed".
   Note that this doesn't update the component on the server. */
static void
ensure_task_complete (CalComponent *comp,
		      time_t completed_date)
{
	struct icaltimetype *old_completed = NULL;
	int *old_percent, new_percent;
	icalproperty_status status;
	gboolean set_completed = TRUE;

	/* Date Completed. */
	if (completed_date == -1) {
		cal_component_get_completed (comp, &old_completed);

		if (old_completed) {
			cal_component_free_icaltimetype (old_completed);
			set_completed = FALSE;
		} else {
			completed_date = time (NULL);
		}
	}

	if (set_completed) {
		icaltimezone *utc_zone;
		struct icaltimetype new_completed;

		/* COMPLETED is stored in UTC. */
		utc_zone = icaltimezone_get_utc_timezone ();
		new_completed = icaltime_from_timet_with_zone (completed_date,
							       FALSE,
							       utc_zone);
		cal_component_set_completed (comp, &new_completed);
	}

	/* Percent. */
	cal_component_get_percent (comp, &old_percent);
	if (!old_percent || *old_percent != 100) {
		new_percent = 100;
		cal_component_set_percent (comp, &new_percent);
	}
	if (old_percent)
		cal_component_free_percent (old_percent);

	/* Status. */
	cal_component_get_status (comp, &status);
	if (status != ICAL_STATUS_COMPLETED) {
		cal_component_set_status (comp, ICAL_STATUS_COMPLETED);
	}
}


/* This makes sure a task is marked as incomplete. It clears the
   "Date Completed" property. If the percent is set to 100 it removes it,
   and if the status is "Completed" it sets it to "Needs Action".
   Note that this doesn't update the component on the client. */
static void
ensure_task_not_complete (CalComponent *comp)
{
	icalproperty_status old_status;
	int *old_percent;

	/* Date Completed. */
	cal_component_set_completed (comp, NULL);

	/* Percent. */
	cal_component_get_percent (comp, &old_percent);
	if (old_percent && *old_percent == 100)
		cal_component_set_percent (comp, NULL);
	if (old_percent)
		cal_component_free_percent (old_percent);

	/* Status. */
	cal_component_get_status (comp, &old_status);
	if (old_status == ICAL_STATUS_COMPLETED)
		cal_component_set_status (comp, ICAL_STATUS_NEEDSACTION);
}


/* Whether we use 24 hour format to display the times. */
gboolean
calendar_model_get_use_24_hour_format (CalendarModel *model)
{
	g_return_val_if_fail (IS_CALENDAR_MODEL (model), TRUE);

	return model->priv->use_24_hour_format;
}


void
calendar_model_set_use_24_hour_format (CalendarModel *model,
				       gboolean	      use_24_hour_format)
{
	g_return_if_fail (IS_CALENDAR_MODEL (model));

	if (model->priv->use_24_hour_format != use_24_hour_format) {
		e_table_model_pre_change (E_TABLE_MODEL (model));
		model->priv->use_24_hour_format = use_24_hour_format;
		/* Get the views to redraw themselves. */
		e_table_model_changed (E_TABLE_MODEL (model));
	}
}


void
calendar_model_set_default_category	(CalendarModel	*model,
					 const char	*default_category)
{
	g_return_if_fail (IS_CALENDAR_MODEL (model));

	g_free (model->priv->default_category);
	model->priv->default_category = g_strdup (default_category);
}



/* The current timezone. */
icaltimezone*
calendar_model_get_timezone		(CalendarModel	*model)
{
	g_return_val_if_fail (IS_CALENDAR_MODEL (model), NULL);

	return model->priv->zone;
}


void
calendar_model_set_timezone		(CalendarModel	*model,
					 icaltimezone	*zone)
{
	g_return_if_fail (IS_CALENDAR_MODEL (model));

	if (model->priv->zone != zone) {
		e_table_model_pre_change (E_TABLE_MODEL (model));
		model->priv->zone = zone;

		/* The timezone affects the times shown for COMPLETED and
		   maybe other fields, so we need to redisplay everything. */
		e_table_model_changed (E_TABLE_MODEL (model));
	}
}


/**
 * calendar_model_refresh:
 * @model: A calendar model.
 * 
 * Refreshes the calendar model, reloading the events/tasks from the server.
 * Be careful about doing this when the user is editing an event/task.
 **/
void
calendar_model_refresh (CalendarModel *model)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_CALENDAR_MODEL (model));

	update_query (model);
}
