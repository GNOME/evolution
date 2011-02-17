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
#include <glib.h>
#include <glib/gi18n.h>
#include <libedataserver/e-flag.h>
#include <libedataserver/e-time-utils.h>
#include <libecal/e-cal-time-util.h>
#include "comp-util.h"
#include "e-cal-model.h"
#include "itip-utils.h"
#include "misc.h"
#include "e-util/e-extensible.h"
#include "e-util/e-util.h"

#define E_CAL_MODEL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_MODEL, ECalModelPrivate))

#define E_CAL_MODEL_COMPONENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_MODEL_COMPONENT, ECalModelComponentPrivate))

typedef struct {
	ECal *client;
	ECalView *query;

	gboolean do_query;
} ECalModelClient;

struct _ECalModelPrivate {
	/* The list of clients we are managing. Each element is of type ECalModelClient */
	GList *clients;

	/* The default client in the list */
	ECal *default_client;

	/* Array for storing the objects. Each element is of type ECalModelComponent */
	GPtrArray *objects;

	icalcomponent_kind kind;
	ECalModelFlags flags;
	icaltimezone *zone;

	/* The time range to display */
	time_t start;
	time_t end;

	/* The search regular expression */
	gchar *search_sexp;

	/* The full regular expression, including time range */
	gchar *full_sexp;

	/* The default category */
	gchar *default_category;

	/* Addresses for determining icons */
	EAccountList *accounts;

	/* Whether we display dates in 24-hour format. */
        gboolean use_24_hour_format;

	/* First day of the week: 0 (Monday) to 6 (Sunday) */
	gint week_start_day;

	/* callback, to retrieve start time for newly added rows by click-to-add */
	ECalModelDefaultTimeFunc get_default_time;
	gpointer get_default_time_user_data;

	gboolean in_added;
	gboolean in_modified;
	gboolean in_removed;

	GList *notify_added;
	GList *notify_modified;
	GList *notify_removed;

	GMutex *notify_lock;
};

static gint ecm_column_count (ETableModel *etm);
static gint ecm_row_count (ETableModel *etm);
static gpointer ecm_value_at (ETableModel *etm, gint col, gint row);
static void ecm_set_value_at (ETableModel *etm, gint col, gint row, gconstpointer value);
static gboolean ecm_is_cell_editable (ETableModel *etm, gint col, gint row);
static void ecm_append_row (ETableModel *etm, ETableModel *source, gint row);
static gpointer ecm_duplicate_value (ETableModel *etm, gint col, gconstpointer value);
static void ecm_free_value (ETableModel *etm, gint col, gpointer value);
static gpointer ecm_initialize_value (ETableModel *etm, gint col);
static gboolean ecm_value_is_empty (ETableModel *etm, gint col, gconstpointer value);
static gchar *ecm_value_to_string (ETableModel *etm, gint col, gconstpointer value);

static const gchar *ecm_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data);

static ECalModelClient *add_new_client (ECalModel *model, ECal *client, gboolean do_query);
static ECalModelClient *find_client_data (ECalModel *model, ECal *client);
static void remove_client_objects (ECalModel *model, ECalModelClient *client_data);
static void remove_client (ECalModel *model, ECalModelClient *client_data);

enum {
	PROP_0,
	PROP_DEFAULT_CLIENT,
	PROP_TIMEZONE,
	PROP_USE_24_HOUR_FORMAT,
	PROP_WEEK_START_DAY
};

enum {
	TIME_RANGE_CHANGED,
	ROW_APPENDED,
	COMPS_DELETED,
	CAL_VIEW_PROGRESS,
	#ifndef E_CAL_DISABLE_DEPRECATED
	CAL_VIEW_DONE,
	#endif
	CAL_VIEW_COMPLETE,
	STATUS_MESSAGE,
	TIMEZONE_CHANGED,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (
	ECalModel, e_cal_model, E_TABLE_MODEL_TYPE,
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
cal_model_set_property (GObject *object,
                        guint property_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DEFAULT_CLIENT:
			e_cal_model_set_default_client (
				E_CAL_MODEL (object),
				g_value_get_object (value));
			return;

		case PROP_TIMEZONE:
			e_cal_model_set_timezone (
				E_CAL_MODEL (object),
				g_value_get_pointer (value));
			return;

		case PROP_USE_24_HOUR_FORMAT:
			e_cal_model_set_use_24_hour_format (
				E_CAL_MODEL (object),
				g_value_get_boolean (value));
			return;

		case PROP_WEEK_START_DAY:
			e_cal_model_set_week_start_day (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_model_get_property (GObject *object,
                        guint property_id,
                        GValue *value,
                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DEFAULT_CLIENT:
			g_value_set_object (
				value,
				e_cal_model_get_default_client (
				E_CAL_MODEL (object)));
			return;

		case PROP_TIMEZONE:
			g_value_set_pointer (
				value,
				e_cal_model_get_timezone (
				E_CAL_MODEL (object)));
			return;

		case PROP_USE_24_HOUR_FORMAT:
			g_value_set_boolean (
				value,
				e_cal_model_get_use_24_hour_format (
				E_CAL_MODEL (object)));
			return;

		case PROP_WEEK_START_DAY:
			g_value_set_int (
				value,
				e_cal_model_get_week_start_day (
				E_CAL_MODEL (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_model_constructed (GObject *object)
{
	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Chain up to parent's constructed() method. */
	if (G_OBJECT_CLASS (parent_class)->constructed)
		G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
cal_model_dispose (GObject *object)
{
	ECalModelPrivate *priv;

	priv = E_CAL_MODEL_GET_PRIVATE (object);

	if (priv->clients) {
		while (priv->clients != NULL) {
			ECalModelClient *client_data = (ECalModelClient *) priv->clients->data;

			g_signal_handlers_disconnect_matched (client_data->client, G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, object);
			if (client_data->query)
				g_signal_handlers_disconnect_matched (client_data->query, G_SIGNAL_MATCH_DATA,
								      0, 0, NULL, NULL, object);

			priv->clients = g_list_remove (priv->clients, client_data);

			g_object_unref (client_data->client);
			if (client_data->query)
				g_object_unref (client_data->query);
			g_free (client_data);
		}

		priv->clients = NULL;
		priv->default_client = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
cal_model_finalize (GObject *object)
{
	ECalModelPrivate *priv;
	gint ii;

	priv = E_CAL_MODEL_GET_PRIVATE (object);

	g_free (priv->search_sexp);
	g_free (priv->full_sexp);

	g_free (priv->default_category);

	for (ii = 0; ii < priv->objects->len; ii++) {
		ECalModelComponent *comp_data;

		comp_data = g_ptr_array_index (priv->objects, ii);
		if (comp_data == NULL) {
			g_warning ("comp_data is null\n");
			continue;
		}
		g_object_unref (comp_data);
	}
	g_ptr_array_free (priv->objects, FALSE);

	g_mutex_free (priv->notify_lock);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
e_cal_model_class_init (ECalModelClass *class)
{
	GObjectClass *object_class;
	ETableModelClass *etm_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ECalModelPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = cal_model_set_property;
	object_class->get_property = cal_model_get_property;
	object_class->constructed = cal_model_constructed;
	object_class->dispose = cal_model_dispose;
	object_class->finalize = cal_model_finalize;

	etm_class = E_TABLE_MODEL_CLASS (class);
	etm_class->column_count = ecm_column_count;
	etm_class->row_count = ecm_row_count;
	etm_class->value_at = ecm_value_at;
	etm_class->set_value_at = ecm_set_value_at;
	etm_class->is_cell_editable = ecm_is_cell_editable;
	etm_class->append_row = ecm_append_row;
	etm_class->duplicate_value = ecm_duplicate_value;
	etm_class->free_value = ecm_free_value;
	etm_class->initialize_value = ecm_initialize_value;
	etm_class->value_is_empty = ecm_value_is_empty;
	etm_class->value_to_string = ecm_value_to_string;

	class->get_color_for_component = ecm_get_color_for_component;
	class->fill_component_from_model = NULL;

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_CLIENT,
		g_param_spec_object (
			"default-client",
			"Default Client",
			NULL,
			E_TYPE_CAL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_TIMEZONE,
		g_param_spec_pointer (
			"timezone",
			"Time Zone",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_USE_24_HOUR_FORMAT,
		g_param_spec_boolean (
			"use-24-hour-format",
			"Use 24-Hour Format",
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WEEK_START_DAY,
		g_param_spec_int (
			"week-start-day",
			"Week Start Day",
			NULL,
			0,  /* Monday */
			6,  /* Sunday */
			0,
			G_PARAM_READWRITE));

	signals[TIME_RANGE_CHANGED] =
		g_signal_new ("time_range_changed",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalModelClass, time_range_changed),
			      NULL, NULL,
			      e_marshal_VOID__LONG_LONG,
			      G_TYPE_NONE, 2, G_TYPE_LONG, G_TYPE_LONG);

	signals[ROW_APPENDED] =
		g_signal_new ("row_appended",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalModelClass, row_appended),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[COMPS_DELETED] =
		g_signal_new ("comps_deleted",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalModelClass, comps_deleted),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	signals[CAL_VIEW_PROGRESS] =
		g_signal_new ("cal_view_progress",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalModelClass, cal_view_progress),
			      NULL, NULL,
			      e_marshal_VOID__STRING_INT_INT,
			      G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);

	#ifndef E_CAL_DISABLE_DEPRECATED
	signals[CAL_VIEW_DONE] =
		g_signal_new ("cal_view_done",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalModelClass, cal_view_done),
			      NULL, NULL,
			      e_marshal_VOID__INT_INT,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
	#endif

	signals[CAL_VIEW_COMPLETE] =
		g_signal_new ("cal_view_complete",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalModelClass, cal_view_complete),
			      NULL, NULL,
			      e_marshal_VOID__INT_STRING_INT,
			      G_TYPE_NONE, 3, G_TYPE_INT, G_TYPE_STRING, G_TYPE_INT);

	signals[STATUS_MESSAGE] = g_signal_new (
		"status-message",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ECalModelClass, status_message),
		NULL, NULL,
		e_marshal_VOID__STRING_DOUBLE,
		G_TYPE_NONE, 2,
		G_TYPE_STRING, G_TYPE_DOUBLE);
	signals[TIMEZONE_CHANGED] = g_signal_new (
		"timezone-changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalModelClass, timezone_changed),
		NULL, NULL,
		e_marshal_VOID__POINTER_POINTER,
		G_TYPE_NONE, 2,
		G_TYPE_POINTER,
		G_TYPE_POINTER);
}

static void
e_cal_model_init (ECalModel *model)
{
	model->priv = E_CAL_MODEL_GET_PRIVATE (model);

	/* match none by default */
	model->priv->start = -1;
	model->priv->end = -1;
	model->priv->search_sexp = NULL;
	model->priv->full_sexp = g_strdup ("#f");

	model->priv->objects = g_ptr_array_new ();
	model->priv->kind = ICAL_NO_COMPONENT;
	model->priv->flags = 0;

	model->priv->accounts = itip_addresses_get ();

	model->priv->use_24_hour_format = TRUE;

	model->priv->in_added = FALSE;
	model->priv->in_modified = FALSE;
	model->priv->in_removed = FALSE;
	model->priv->notify_added = NULL;
	model->priv->notify_modified = NULL;
	model->priv->notify_removed = NULL;
	model->priv->notify_lock = g_mutex_new ();
}

/* ETableModel methods */

static gint
ecm_column_count (ETableModel *etm)
{
	return E_CAL_MODEL_FIELD_LAST;
}

static gint
ecm_row_count (ETableModel *etm)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	priv = model->priv;

	return priv->objects->len;
}

static gpointer
get_categories (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_CATEGORIES_PROPERTY);
	if (prop)
		return (gpointer) icalproperty_get_categories (prop);

	return (gpointer) "";
}

static gchar *
get_classification (ECalModelComponent *comp_data)
{
	icalproperty *prop;
	icalproperty_class class;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_CLASS_PROPERTY);

	if (!prop)
		return _("Public");

	class = icalproperty_get_class (prop);

	switch (class)
	{
	case ICAL_CLASS_PUBLIC:
		return _("Public");
	case ICAL_CLASS_PRIVATE:
		return _("Private");
	case ICAL_CLASS_CONFIDENTIAL:
		return _("Confidential");
	default:
		return _("Unknown");
	}
}

static const gchar *
get_color (ECalModel *model, ECalModelComponent *comp_data)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return e_cal_model_get_color_for_component (model, comp_data);
}

static gpointer
get_description (ECalModelComponent *comp_data)
{
	icalproperty *prop;
	static GString *str = NULL;

	if (str) {
		g_string_free (str, TRUE);
		str = NULL;
	}

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DESCRIPTION_PROPERTY);
	if (prop) {
		str = g_string_new (NULL);
		do {
			str = g_string_append (str, icalproperty_get_description (prop));
		} while ((prop = icalcomponent_get_next_property (comp_data->icalcomp, ICAL_DESCRIPTION_PROPERTY)));

		return str->str;
	}

	return (gpointer) "";
}

static ECellDateEditValue*
get_dtstart (ECalModel *model, ECalModelComponent *comp_data)
{
	ECalModelPrivate *priv;
        struct icaltimetype tt_start;

	priv = model->priv;

	if (!comp_data->dtstart) {
		icalproperty *prop;
		icaltimezone *zone;
		gboolean got_zone = FALSE;

		prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DTSTART_PROPERTY);
		if (!prop)
			return NULL;

		tt_start = icalproperty_get_dtstart (prop);

		if (icaltime_get_tzid (tt_start)
		    && e_cal_get_timezone (comp_data->client, icaltime_get_tzid (tt_start), &zone, NULL))
			got_zone = TRUE;

		if (e_cal_model_get_flags (model) & E_CAL_MODEL_FLAGS_EXPAND_RECURRENCES) {
			if (got_zone) {
				tt_start = icaltime_from_timet_with_zone (comp_data->instance_start, tt_start.is_date, zone);
				if (priv->zone)
					icaltimezone_convert_time (&tt_start, zone, priv->zone);
			} else
				if (priv->zone)
					tt_start = icaltime_from_timet_with_zone (comp_data->instance_start, tt_start.is_date, priv->zone);
		}

		if (!icaltime_is_valid_time (tt_start) || icaltime_is_null_time (tt_start))
			return NULL;

		comp_data->dtstart = g_new0 (ECellDateEditValue, 1);
		comp_data->dtstart->tt = tt_start;

		if (got_zone)
			comp_data->dtstart->zone = zone;
		else
			comp_data->dtstart->zone = NULL;
	}

	return comp_data->dtstart;
}

static ECellDateEditValue*
get_datetime_from_utc (ECalModel *model, ECalModelComponent *comp_data, icalproperty_kind propkind, struct icaltimetype (*get_value)(const icalproperty* prop), ECellDateEditValue **buffer)
{
	ECalModelPrivate *priv;
        struct icaltimetype tt_value;
	icalproperty *prop;
	ECellDateEditValue *res;

	g_return_val_if_fail (buffer!= NULL, NULL);

	if (*buffer)
		return *buffer;

	priv = model->priv;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, propkind);
	if (!prop)
		return NULL;

	tt_value = get_value (prop);

	/* these are always in UTC, thus convert to default zone, if any and done */
	if (priv->zone)
		icaltimezone_convert_time (&tt_value, icaltimezone_get_utc_timezone (), priv->zone);

	if (!icaltime_is_valid_time (tt_value) || icaltime_is_null_time (tt_value))
		return NULL;

	res = g_new0 (ECellDateEditValue, 1);
	res->tt = tt_value;
	res->zone = NULL;

	*buffer = res;

	return res;
}

static gpointer
get_summary (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_SUMMARY_PROPERTY);
	if (prop)
		return (gpointer) icalproperty_get_summary (prop);

	return (gpointer) "";
}

static gchar *
get_uid (ECalModelComponent *comp_data)
{
	return (gchar *) icalcomponent_get_uid (comp_data->icalcomp);
}

static gpointer
ecm_value_at (ETableModel *etm, gint col, gint row)
{
	ECalModelPrivate *priv;
	ECalModelComponent *comp_data;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, NULL);
	g_return_val_if_fail (row >= 0 && row < priv->objects->len, NULL);

	comp_data = g_ptr_array_index (priv->objects, row);
	g_return_val_if_fail (comp_data != NULL, NULL);
	g_return_val_if_fail (comp_data->icalcomp != NULL, NULL);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
		return get_categories (comp_data);
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
		return get_classification (comp_data);
	case E_CAL_MODEL_FIELD_COLOR :
		return (gpointer) get_color (model, comp_data);
	case E_CAL_MODEL_FIELD_COMPONENT :
		return comp_data->icalcomp;
	case E_CAL_MODEL_FIELD_DESCRIPTION :
		return get_description (comp_data);
	case E_CAL_MODEL_FIELD_DTSTART :
		return (gpointer) get_dtstart (model, comp_data);
	case E_CAL_MODEL_FIELD_CREATED :
		return (gpointer) get_datetime_from_utc (model, comp_data, ICAL_CREATED_PROPERTY, icalproperty_get_created, &comp_data->created);
	case E_CAL_MODEL_FIELD_LASTMODIFIED :
		return (gpointer) get_datetime_from_utc (model, comp_data, ICAL_LASTMODIFIED_PROPERTY, icalproperty_get_lastmodified, &comp_data->lastmodified);
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
		return GINT_TO_POINTER ((icalcomponent_get_first_component (comp_data->icalcomp,
									    ICAL_VALARM_COMPONENT) != NULL));
	case E_CAL_MODEL_FIELD_ICON :
	{
		ECalComponent *comp;
		icalcomponent *icalcomp;
		gint retval = 0;

		comp = e_cal_component_new ();
		icalcomp = icalcomponent_new_clone (comp_data->icalcomp);
		if (e_cal_component_set_icalcomponent (comp, icalcomp)) {
			if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_JOURNAL) {
				g_object_unref (comp);
				return GINT_TO_POINTER (retval);
			}

			if (e_cal_component_has_recurrences (comp))
				retval = 1;
			else if (itip_organizer_is_user (comp, comp_data->client))
				retval = 3;
			else {
				GSList *attendees = NULL, *sl;

				e_cal_component_get_attendee_list (comp, &attendees);
				for (sl = attendees; sl != NULL; sl = sl->next) {
					ECalComponentAttendee *ca = sl->data;
					const gchar *text;

					text = itip_strip_mailto (ca->value);
					if (e_account_list_find (priv->accounts, E_ACCOUNT_FIND_ID_ADDRESS, text) != NULL) {
						if (ca->delto != NULL)
							retval = 3;
						else
							retval = 2;
						break;
					}
				}

				e_cal_component_free_attendee_list (attendees);
			}
		} else
			icalcomponent_free (icalcomp);

		g_object_unref (comp);

		return GINT_TO_POINTER (retval);
	}
	case E_CAL_MODEL_FIELD_SUMMARY :
		return get_summary (comp_data);
	case E_CAL_MODEL_FIELD_UID :
		return get_uid (comp_data);
	}

	return (gpointer) "";
}

static void
set_categories (ECalModelComponent *comp_data, const gchar *value)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_CATEGORIES_PROPERTY);
	if (!value || !(*value)) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
	} else {
		if (!prop) {
			prop = icalproperty_new_categories (value);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		} else
			icalproperty_set_categories (prop, value);
	}
}

static void
set_classification (ECalModelComponent *comp_data, const gchar *value)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_CLASS_PROPERTY);
	if (!value || !(*value)) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
	} else {
	  icalproperty_class ical_class;

	  if (!g_ascii_strcasecmp (value, "PUBLIC"))
	    ical_class = ICAL_CLASS_PUBLIC;
	  else if (!g_ascii_strcasecmp (value, "PRIVATE"))
	    ical_class = ICAL_CLASS_PRIVATE;
	  else if (!g_ascii_strcasecmp (value, "CONFIDENTIAL"))
	    ical_class = ICAL_CLASS_CONFIDENTIAL;
	  else
	    ical_class = ICAL_CLASS_NONE;

		if (!prop) {
			prop = icalproperty_new_class (ical_class);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		} else
			icalproperty_set_class (prop, ical_class);
	}
}

static void
set_description (ECalModelComponent *comp_data, const gchar *value)
{
	icalproperty *prop;

	/* remove old description(s) */
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DESCRIPTION_PROPERTY);
	while (prop) {
		icalproperty *next;

		next = icalcomponent_get_next_property (comp_data->icalcomp, ICAL_DESCRIPTION_PROPERTY);

		icalcomponent_remove_property (comp_data->icalcomp, prop);
		icalproperty_free (prop);

		prop = next;
	}

	/* now add the new description */
	if (!value || !(*value))
		return;

	prop = icalproperty_new_description (value);
	icalcomponent_add_property (comp_data->icalcomp, prop);
}

static void
datetime_to_zone (ECal *client, struct icaltimetype *tt, icaltimezone *tt_zone, const gchar *tzid)
{
	icaltimezone *from, *to;
	const gchar *tt_tzid = NULL;

	g_return_if_fail (tt != NULL);

	if (tt_zone)
		tt_tzid = icaltimezone_get_tzid (tt_zone);

	if (tt_tzid == NULL || tzid == NULL ||
	    tt_tzid == tzid || g_str_equal (tt_tzid, tzid))
		return;

	from = tt_zone;
	to = icaltimezone_get_builtin_timezone_from_tzid (tzid);
	if (!to) {
		/* do not check failure here, maybe the zone is not available there */
		e_cal_get_timezone (client, tzid, &to, NULL);
	}

	icaltimezone_convert_time (tt, from, to);
}

/* updates time in a component, and keeps the timezone used in it, if exists */
void
e_cal_model_update_comp_time (ECalModel *model, ECalModelComponent *comp_data, gconstpointer time_value, icalproperty_kind kind, void (*set_func)(icalproperty *prop, struct icaltimetype v), icalproperty * (*new_func)(struct icaltimetype v))
{
	ECellDateEditValue *dv = (ECellDateEditValue *) time_value;
	icalproperty *prop;
	icalparameter *param;
	struct icaltimetype tt;

	g_return_if_fail (model != NULL);
	g_return_if_fail (comp_data != NULL);
	g_return_if_fail (set_func != NULL);
	g_return_if_fail (new_func != NULL);

	prop = icalcomponent_get_first_property (comp_data->icalcomp, kind);
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

	tt = dv->tt;
	datetime_to_zone (comp_data->client, &tt, e_cal_model_get_timezone (model), param ? icalparameter_get_tzid (param) : NULL);

	if (prop) {
		set_func (prop, tt);
	} else {
		prop = new_func (tt);
		icalcomponent_add_property (comp_data->icalcomp, prop);
	}

	if (param) {
		const gchar *tzid = icalparameter_get_tzid (param);

		/* If the TZID is set to "UTC", we don't want to save the TZID. */
		if (tzid && strcmp (tzid, "UTC")) {
			if (param) {
				icalparameter_set_tzid (param, (gchar *) tzid);
			} else {
				param = icalparameter_new_tzid ((gchar *) tzid);
				icalproperty_add_parameter (prop, param);
			}
		} else {
			icalproperty_remove_parameter (prop, ICAL_TZID_PARAMETER);
		}
	}
}

static void
set_dtstart (ECalModel *model, ECalModelComponent *comp_data, gconstpointer value)
{
	e_cal_model_update_comp_time (model, comp_data, value, ICAL_DTSTART_PROPERTY, icalproperty_set_dtstart, icalproperty_new_dtstart);
}

static void
set_summary (ECalModelComponent *comp_data, const gchar *value)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_SUMMARY_PROPERTY);

	if (string_is_empty (value)) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
	} else {
		if (prop)
			icalproperty_set_summary (prop, value);
		else {
			prop = icalproperty_new_summary (value);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		}
	}
}

static void
ecm_set_value_at (ETableModel *etm, gint col, gint row, gconstpointer value)
{
	ECalModelPrivate *priv;
	ECalModelComponent *comp_data;
	ECalModel *model = (ECalModel *) etm;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST);
	g_return_if_fail (row >= 0 && row < priv->objects->len);

	comp_data = g_ptr_array_index (priv->objects, row);
	g_return_if_fail (comp_data != NULL);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
		set_categories (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
		set_classification (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_DESCRIPTION :
		set_description (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_DTSTART :
		set_dtstart (model, comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_SUMMARY :
		set_summary (comp_data, value);
		break;
	}

	/* FIXME ask about mod type */
	if (!e_cal_modify_object (comp_data->client, comp_data->icalcomp, CALOBJ_MOD_ALL, NULL)) {
		g_warning (G_STRLOC ": Could not modify the object!");

		/* FIXME Show error dialog */
	}
}

/**
 * e_cal_model_test_row_editable
 * Checks if component at row 'row' is editable or not. It doesn't check bounds for 'row'.
 *
 * @param model Calendar model.
 * @param row Row of our interest. -1 is editable only when default client is editable.
 * @return Whether row is editable or not.
 **/
gboolean
e_cal_model_test_row_editable (ECalModel *model, gint row)
{
	gboolean readonly;
	ECal *cal = NULL;

	if (row != -1) {
		ECalModelComponent *comp_data;

		comp_data = e_cal_model_get_component_at (model, row);

		if (comp_data)
			cal = comp_data->client;

	} else {
		cal = e_cal_model_get_default_client (model);
	}

	readonly = cal == NULL;

	if (!readonly)
		if (!e_cal_is_read_only (cal, &readonly, NULL))
			readonly = TRUE;

	return !readonly;
}

static gboolean
ecm_is_cell_editable (ETableModel *etm, gint col, gint row)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);

	priv = model->priv;

	g_return_val_if_fail (col >= 0 && col <= E_CAL_MODEL_FIELD_LAST, FALSE);
	g_return_val_if_fail (row >= -1 || (row >= 0 && row < priv->objects->len), FALSE);

	if (!e_cal_model_test_row_editable (E_CAL_MODEL (etm), row))
		return FALSE;

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_DTSTART :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return TRUE;
	}

	return FALSE;
}

static void
ecm_append_row (ETableModel *etm, ETableModel *source, gint row)
{
	ECalModelClass *model_class;
	ECalModelComponent *comp_data;
	ECalModel *model = (ECalModel *) etm;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_TABLE_MODEL (source));

	comp_data = g_object_new (E_TYPE_CAL_MODEL_COMPONENT, NULL);

	comp_data->client = e_cal_model_get_default_client (model);

	/* guard against saving before the calendar is open */
	if (!(comp_data->client && e_cal_get_load_state (comp_data->client) == E_CAL_LOAD_LOADED)) {
		g_object_unref (comp_data);
		return;
	}

	comp_data->icalcomp = e_cal_model_create_component_with_defaults (model, FALSE);

	/* set values for our fields */
	set_categories (comp_data, e_table_model_value_at (source, E_CAL_MODEL_FIELD_CATEGORIES, row));
	set_classification (comp_data, e_table_model_value_at (source, E_CAL_MODEL_FIELD_CLASSIFICATION, row));
	set_description (comp_data, e_table_model_value_at (source, E_CAL_MODEL_FIELD_DESCRIPTION, row));
	set_summary (comp_data, e_table_model_value_at (source, E_CAL_MODEL_FIELD_SUMMARY, row));

	if (e_table_model_value_at (source, E_CAL_MODEL_FIELD_DTSTART, row)) {
		set_dtstart (model, comp_data, e_table_model_value_at (source, E_CAL_MODEL_FIELD_DTSTART, row));
	} else if (model->priv->get_default_time) {
		time_t tt = model->priv->get_default_time (model, model->priv->get_default_time_user_data);

		if (tt > 0) {
			struct icaltimetype itt = icaltime_from_timet_with_zone (tt, FALSE, e_cal_model_get_timezone (model));
			icalproperty *prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DTSTART_PROPERTY);

			if (prop) {
				icalproperty_set_dtstart (prop, itt);
			} else {
				prop = icalproperty_new_dtstart (itt);
				icalcomponent_add_property (comp_data->icalcomp, prop);
			}
		}
	}

	/* call the class' method for filling the component */
	model_class = (ECalModelClass *) G_OBJECT_GET_CLASS (model);
	if (model_class->fill_component_from_model != NULL) {
		model_class->fill_component_from_model (model, comp_data, source, row);
	}

	if (!e_cal_create_object (comp_data->client, comp_data->icalcomp, NULL, NULL)) {
		g_warning (G_STRLOC ": Could not create the object!");

		/* FIXME: show error dialog */
	} else {
		g_signal_emit (G_OBJECT (model), signals[ROW_APPENDED], 0);
	}

	g_object_unref (comp_data);
}

static gpointer
ecm_duplicate_value (ETableModel *etm, gint col, gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, NULL);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return g_strdup (value);
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
		return (gpointer) value;
	case E_CAL_MODEL_FIELD_COMPONENT :
		return icalcomponent_new_clone ((icalcomponent *) value);
	case E_CAL_MODEL_FIELD_DTSTART :
	case E_CAL_MODEL_FIELD_CREATED :
	case E_CAL_MODEL_FIELD_LASTMODIFIED :
		if (value) {
			ECellDateEditValue *dv, *orig_dv;

			orig_dv = (ECellDateEditValue *) value;
			dv = g_new0 (ECellDateEditValue, 1);
			*dv = *orig_dv;

			return dv;
		}
		break;
	}

	return NULL;
}

static void
ecm_free_value (ETableModel *etm, gint col, gpointer value)
{
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		if (value)
			g_free (value);
		break;
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
		break;
	case E_CAL_MODEL_FIELD_DTSTART :
	case E_CAL_MODEL_FIELD_CREATED :
	case E_CAL_MODEL_FIELD_LASTMODIFIED :
		if (value)
			g_free (value);
		break;
	case E_CAL_MODEL_FIELD_COMPONENT :
		if (value)
			icalcomponent_free ((icalcomponent *) value);
		break;
	}
}

static gpointer
ecm_initialize_value (ETableModel *etm, gint col)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, NULL);

	priv = model->priv;

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
		return g_strdup (priv->default_category?priv->default_category:"");
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return g_strdup ("");
	case E_CAL_MODEL_FIELD_DTSTART :
	case E_CAL_MODEL_FIELD_CREATED :
	case E_CAL_MODEL_FIELD_LASTMODIFIED :
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
	case E_CAL_MODEL_FIELD_COMPONENT :
		return NULL;
	}

	return NULL;
}

static gboolean
ecm_value_is_empty (ETableModel *etm, gint col, gconstpointer value)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), TRUE);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, TRUE);

	priv = model->priv;

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
		/* This could be a hack or not.  If the categories field only
		 * contains the default category, then it possibly means that
		 * the user has not entered anything at all in the click-to-add;
		 * the category is in the value because we put it there in
		 * ecm_initialize_value().
		 */
		if (priv->default_category && value && strcmp (priv->default_category, value) == 0)
			return TRUE;
		else
			return string_is_empty (value);
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return string_is_empty (value);
	case E_CAL_MODEL_FIELD_DTSTART :
	case E_CAL_MODEL_FIELD_CREATED :
	case E_CAL_MODEL_FIELD_LASTMODIFIED :
		return value ? FALSE : TRUE;
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
	case E_CAL_MODEL_FIELD_COMPONENT :
		return TRUE;
	}

	return TRUE;
}

static gchar *
ecm_value_to_string (ETableModel *etm, gint col, gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, g_strdup (""));

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return g_strdup (value);
	case E_CAL_MODEL_FIELD_DTSTART :
	case E_CAL_MODEL_FIELD_CREATED :
	case E_CAL_MODEL_FIELD_LASTMODIFIED :
		return e_cal_model_date_value_to_string (E_CAL_MODEL (etm), value);
	case E_CAL_MODEL_FIELD_ICON :
		if (GPOINTER_TO_INT (value) == 0)
			return g_strdup (_("Normal"));
		else if (GPOINTER_TO_INT (value) == 1)
			return g_strdup (_("Recurring"));
		else
			return g_strdup (_("Assigned"));
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
		return g_strdup (value ? _("Yes") : _("No"));
	case E_CAL_MODEL_FIELD_COLOR :
	case E_CAL_MODEL_FIELD_COMPONENT :
		return g_strdup ("");
	}

	return g_strdup ("");
}

/* ECalModel class methods */

typedef struct {
	const gchar *color;
	GList *uris;
} AssignedColorData;

static const gchar *
ecm_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data)
{
	ESource *source;
	const gchar *color_spec;
	gint i, first_empty = 0;
	static AssignedColorData assigned_colors[] = {
		{ "#BECEDD", NULL }, /* 190 206 221     Blue */
		{ "#E2F0EF", NULL }, /* 226 240 239     Light Blue */
		{ "#C6E2B7", NULL }, /* 198 226 183     Green */
		{ "#E2F0D3", NULL }, /* 226 240 211     Light Green */
		{ "#E2D4B7", NULL }, /* 226 212 183     Khaki */
		{ "#EAEAC1", NULL }, /* 234 234 193     Light Khaki */
		{ "#F0B8B7", NULL }, /* 240 184 183     Pink */
		{ "#FED4D3", NULL }, /* 254 212 211     Light Pink */
		{ "#E2C6E1", NULL }, /* 226 198 225     Purple */
		{ "#F0E2EF", NULL }  /* 240 226 239     Light Purple */
	};

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	source = e_cal_get_source (comp_data->client);
	color_spec = e_source_peek_color_spec (source);
	if (color_spec != NULL) {
		g_free (comp_data->color);
		comp_data->color = g_strdup (color_spec);
		return comp_data->color;
	}

	for (i = 0; i < G_N_ELEMENTS (assigned_colors); i++) {
		GList *l;

		if (assigned_colors[i].uris == NULL) {
			first_empty = i;
			continue;
		}

		for (l = assigned_colors[i].uris; l != NULL; l = l->next) {
			if (!strcmp ((const gchar *) l->data,
				     e_cal_get_uri (comp_data->client)))
			{
				return assigned_colors[i].color;
			}
		}
	}

	/* return the first unused color */
	assigned_colors[first_empty].uris = g_list_append (assigned_colors[first_empty].uris,
							   g_strdup (e_cal_get_uri (comp_data->client)));

	return assigned_colors[first_empty].color;
}

icalcomponent_kind
e_cal_model_get_component_kind (ECalModel *model)
{
	ECalModelPrivate *priv;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), ICAL_NO_COMPONENT);

	priv = model->priv;
	return priv->kind;
}

void
e_cal_model_set_component_kind (ECalModel *model, icalcomponent_kind kind)
{
	ECalModelPrivate *priv;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;
	priv->kind = kind;
}

ECalModelFlags
e_cal_model_get_flags (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), 0);

	return model->priv->flags;
}

void
e_cal_model_set_flags (ECalModel *model, ECalModelFlags flags)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	model->priv->flags = flags;
}

icaltimezone *
e_cal_model_get_timezone (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return model->priv->zone;
}

void
e_cal_model_set_timezone (ECalModel *model,
                          icaltimezone *zone)
{
	icaltimezone *old_zone;
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->zone == zone)
		return;

	e_table_model_pre_change (E_TABLE_MODEL (model));
	old_zone = model->priv->zone;
	model->priv->zone = zone;

	/* the timezone affects the times shown for date fields,
	   so we need to redisplay everything */
	e_table_model_changed (E_TABLE_MODEL (model));

	g_object_notify (G_OBJECT (model), "timezone");
	g_signal_emit (G_OBJECT (model), signals[TIMEZONE_CHANGED], 0,
		       old_zone, zone);
}

void
e_cal_model_set_default_category (ECalModel *model,
                                  const gchar *default_category)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	g_free (model->priv->default_category);
	model->priv->default_category = g_strdup (default_category);
}

gboolean
e_cal_model_get_use_24_hour_format (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);

	return model->priv->use_24_hour_format;
}

void
e_cal_model_set_use_24_hour_format (ECalModel *model,
                                    gboolean use_24_hour_format)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->use_24_hour_format == use_24_hour_format)
		return;

	e_table_model_pre_change (E_TABLE_MODEL (model));
	model->priv->use_24_hour_format = use_24_hour_format;

	/* Get the views to redraw themselves. */
	e_table_model_changed (E_TABLE_MODEL (model));

	g_object_notify (G_OBJECT (model), "use-24-hour-format");
}

gint
e_cal_model_get_week_start_day (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), 0);

	return model->priv->week_start_day;
}

void
e_cal_model_set_week_start_day (ECalModel *model,
                                gint week_start_day)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (week_start_day >= 0);
	g_return_if_fail (week_start_day < 7);

	if (model->priv->week_start_day == week_start_day)
		return;

	model->priv->week_start_day = week_start_day;

	g_object_notify (G_OBJECT (model), "week-start-day");
}

ECal *
e_cal_model_get_default_client (ECalModel *model)
{
	ECalModelPrivate *priv;
	ECalModelClient *client_data;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	/* FIXME Should we force the client to be open? */

	/* we always return a valid ECal, since we rely on it in many places */
	if (priv->default_client)
		return priv->default_client;

	if (!priv->clients)
		return NULL;

	client_data = (ECalModelClient *) priv->clients->data;

	return client_data ? client_data->client : NULL;
}

void
e_cal_model_set_default_client (ECalModel *model, ECal *client)
{
	ECalModelPrivate *priv;
	ECalModelClient *client_data;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (client != NULL)
		g_return_if_fail (E_IS_CAL (client));

	priv = model->priv;

	if (priv->default_client) {
		client_data = find_client_data (model, priv->default_client);
		if (!client_data) {
			g_warning ("client_data is NULL\n");
		} else {
			if (!client_data->do_query)
				remove_client (model, client_data);
		}
	}

	if (client != NULL) {
		/* Make sure its in the model */
		client_data = add_new_client (model, client, FALSE);

		/* Store the default client */
		priv->default_client = client_data->client;
	} else
		priv->default_client = NULL;

	g_object_notify (G_OBJECT (model), "default-client");
}

GList *
e_cal_model_get_client_list (ECalModel *model)
{
	GList *list = NULL, *l;
	ECal *default_client;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	default_client = model->priv->default_client;

	for (l = model->priv->clients; l != NULL; l = l->next) {
		ECalModelClient *client_data = (ECalModelClient *) l->data;

		/* Exclude the default client if we're not querying it. */
		if (client_data->client == default_client && !client_data->do_query)
			continue;

		list = g_list_append (list, client_data->client);
	}

	return list;
}

/**
 * e_cal_model_get_client_for_uri:
 * @model: A calendar model.
 * @uri: Uri for the client to get.
 */
ECal *
e_cal_model_get_client_for_uri (ECalModel *model, const gchar *uri)
{
	GList *l;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	for (l = model->priv->clients; l != NULL; l = l->next) {
		ECalModelClient *client_data = (ECalModelClient *) l->data;

		if (!strcmp (uri, e_cal_get_uri (client_data->client)))
			return client_data->client;
	}

	return NULL;
}

static ECalModelClient *
find_client_data (ECalModel *model, ECal *client)
{
	ECalModelPrivate *priv;
	GList *l;

	priv = model->priv;

	for (l = priv->clients; l != NULL; l = l->next) {
		ECalModelClient *client_data = (ECalModelClient *) l->data;

		if (client_data->client == client)
			return client_data;
	}

	return NULL;
}

static ECalModelComponent *
search_by_id_and_client (ECalModelPrivate *priv, ECal *client, const ECalComponentId *id)
{
	gint i;

	for (i = 0; i < priv->objects->len; i++) {
		ECalModelComponent *comp_data = g_ptr_array_index (priv->objects, i);

		if (comp_data) {
			const gchar *uid;
			gchar *rid = NULL;
			struct icaltimetype icalrid;
			gboolean has_rid = (id->rid && *id->rid);

			uid = icalcomponent_get_uid (comp_data->icalcomp);
			icalrid = icalcomponent_get_recurrenceid (comp_data->icalcomp);
			if (!icaltime_is_null_time (icalrid))
				rid = icaltime_as_ical_string_r (icalrid);

			if (uid && *uid) {
				if ((!client || comp_data->client == client) && !strcmp (id->uid, uid)) {
					if (has_rid) {
						if (!(rid && *rid && !strcmp (rid, id->rid))) {
							g_free (rid);
							continue;
						}
					}
					g_free (rid);
					return comp_data;
				}
			}

			g_free (rid);
		}
	}

	return NULL;
}

typedef struct {
	ECal *client;
	ECalView *query;
	ECalModel *model;
	icalcomponent *icalcomp;
} RecurrenceExpansionData;

static gboolean
add_instance_cb (ECalComponent *comp, time_t instance_start, time_t instance_end, gpointer user_data)
{
	ECalModelComponent *comp_data;
	ECalModelPrivate *priv;
	RecurrenceExpansionData *rdata = user_data;
	icaltimetype time;
	ECalComponentDateTime datetime, to_set;
	icaltimezone *zone = NULL;

	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), TRUE);

	priv = rdata->model->priv;

	e_table_model_pre_change (E_TABLE_MODEL (rdata->model));

	/* set the right instance start date to component */
	e_cal_component_get_dtstart (comp, &datetime);
	e_cal_get_timezone (rdata->client, datetime.tzid, &zone, NULL);
	time = icaltime_from_timet_with_zone (instance_start, FALSE, zone ? zone : priv->zone);
	to_set.value = &time;
	to_set.tzid = datetime.tzid;
	e_cal_component_set_dtstart (comp, &to_set);
	e_cal_component_free_datetime (&datetime);

	/* set the right instance end date to component*/
	e_cal_component_get_dtend (comp, &datetime);
	e_cal_get_timezone (rdata->client, datetime.tzid, &zone, NULL);
	time = icaltime_from_timet_with_zone (instance_end, FALSE, zone ? zone : priv->zone);
	to_set.value = &time;
	to_set.tzid = datetime.tzid;
	e_cal_component_set_dtend (comp, &to_set);
	e_cal_component_free_datetime (&datetime);

	comp_data = g_object_new (E_TYPE_CAL_MODEL_COMPONENT, NULL);
	comp_data->client = g_object_ref (rdata->client);
	comp_data->icalcomp = icalcomponent_new_clone (e_cal_component_get_icalcomponent (comp));
	comp_data->instance_start = instance_start;
	comp_data->instance_end = instance_end;

	g_ptr_array_add (priv->objects, comp_data);
	e_table_model_row_inserted (E_TABLE_MODEL (rdata->model), priv->objects->len - 1);

	return TRUE;
}

/* We do this check since the calendar items are downloaded from the server
 * in the open_method, since the default timezone might not be set there. */
static void
ensure_dates_are_in_default_zone (ECalModel *model,
                                  icalcomponent *icalcomp)
{
	icaltimetype dt;
	icaltimezone *zone;

	zone = e_cal_model_get_timezone (model);
	if (!zone)
		return;

	dt = icalcomponent_get_dtstart (icalcomp);
	if (dt.is_utc) {
		dt = icaltime_convert_to_zone (dt, zone);
		icalcomponent_set_dtstart (icalcomp, dt);
	}

	dt = icalcomponent_get_dtend (icalcomp);
	if (dt.is_utc) {
		dt = icaltime_convert_to_zone (dt, zone);
		icalcomponent_set_dtend (icalcomp, dt);
	}
}

static gint
place_master_object_first_cb (gconstpointer p1, gconstpointer p2)
{
	icalcomponent *c1 = (icalcomponent *) p1, *c2 = (icalcomponent *) p2;
	const gchar *uid1, *uid2;
	gint res;

	g_return_val_if_fail (c1 != NULL, 0);
	g_return_val_if_fail (c2 != NULL, 0);

	uid1 = icalcomponent_get_uid (c1);
	uid2 = icalcomponent_get_uid (c2);

	res = g_strcmp0 (uid1, uid2);
	if (res == 0) {
		struct icaltimetype rid1, rid2;

		rid1 = icalcomponent_get_recurrenceid (c1);
		rid2 = icalcomponent_get_recurrenceid (c2);

		if (icaltime_is_null_time (rid1)) {
			if (!icaltime_is_null_time (rid2))
				res = -1;
		} else if (icaltime_is_null_time (rid2)) {
			res = 1;
		} else {
			res = icaltime_compare (rid1, rid2);
		}
	}

	return res;
}

static void e_cal_view_objects_added_cb (ECalView *query, GList *objects, ECalModel *model);

static void
process_added (ECalView *query, GList *objects, ECalModel *model)
{
	ECalModelPrivate *priv;
	GList *l, *copy;

	priv = model->priv;

	/* order matters, process master object first, then detached instances */
	copy = g_list_sort (g_list_copy (objects), place_master_object_first_cb);

	for (l = copy; l; l = l->next) {
		ECalModelComponent *comp_data;
		ECalComponentId *id;
		ECalComponent *comp = e_cal_component_new ();
		ECal *client = e_cal_view_get_client (query);

		/* This will fail for alarm or VCalendar component */
		if (!e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (l->data))) {
			g_object_unref (comp);
			continue;
		}

		id = e_cal_component_get_id (comp);

		/* remove the components if they are already present and re-add them */
		while ((comp_data = search_by_id_and_client (priv, client,
							      id))) {
			gint pos;
			GSList *list = NULL;

			pos = get_position_in_array (priv->objects, comp_data);

			if (!g_ptr_array_remove (priv->objects, comp_data))
				continue;

			list = g_slist_append (list, comp_data);
			g_signal_emit (G_OBJECT (model), signals[COMPS_DELETED], 0, list);

			g_slist_free (list);
			g_object_unref (comp_data);

			e_table_model_pre_change (E_TABLE_MODEL (model));
			e_table_model_row_deleted (E_TABLE_MODEL (model), pos);
		}

		e_cal_component_free_id (id);
		g_object_unref (comp);
		ensure_dates_are_in_default_zone (model, l->data);

		if (e_cal_util_component_has_recurrences (l->data) && (priv->flags & E_CAL_MODEL_FLAGS_EXPAND_RECURRENCES)) {
			RecurrenceExpansionData rdata;

			rdata.client = e_cal_view_get_client (query);
			rdata.query = query;
			rdata.model = model;
			rdata.icalcomp = l->data;
			e_cal_generate_instances_for_object (rdata.client, l->data,
							     priv->start, priv->end,
							     (ECalRecurInstanceFn) add_instance_cb,
							     &rdata);
		} else {
			e_table_model_pre_change (E_TABLE_MODEL (model));

			comp_data = g_object_new (E_TYPE_CAL_MODEL_COMPONENT, NULL);
			comp_data->client = g_object_ref (e_cal_view_get_client (query));
			comp_data->icalcomp = icalcomponent_new_clone (l->data);
			e_cal_model_set_instance_times (comp_data, priv->zone);

			g_ptr_array_add (priv->objects, comp_data);
			e_table_model_row_inserted (E_TABLE_MODEL (model), priv->objects->len - 1);
		}
	}

	g_list_free (copy);
}

static void
process_modified (ECalView *query, GList *objects, ECalModel *model)
{
	ECalModelPrivate *priv;
	GList *l, *list = NULL;

	priv = model->priv;

	/*  re-add only the recurrence objects */
	for (l = objects; l != NULL; l = g_list_next (l)) {
		if (!e_cal_util_component_is_instance (l->data) && e_cal_util_component_has_recurrences (l->data) && (priv->flags & E_CAL_MODEL_FLAGS_EXPAND_RECURRENCES))
			list = g_list_prepend (list, l->data);
		else {
			gint pos;
			ECalModelComponent *comp_data;
			ECalComponentId *id;
			ECalComponent *comp = e_cal_component_new ();
			ECal *client = e_cal_view_get_client (query);

			if (!e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (l->data))) {
				g_object_unref (comp);
				continue;
			}

			e_table_model_pre_change (E_TABLE_MODEL (model));

			id = e_cal_component_get_id (comp);

			comp_data = search_by_id_and_client (priv, client, id);

			e_cal_component_free_id (id);
			g_object_unref (comp);

			if (!comp_data) {
				/* the modified component is not in the model yet, just skip it */
				continue;
			}

			if (comp_data->icalcomp)
				icalcomponent_free (comp_data->icalcomp);
			if (comp_data->dtstart) {
				g_free (comp_data->dtstart);
				comp_data->dtstart = NULL;
			}
			if (comp_data->dtend) {
				g_free (comp_data->dtend);
				comp_data->dtend = NULL;
			}
			if (comp_data->due) {
				g_free (comp_data->due);
				comp_data->due = NULL;
			}
			if (comp_data->completed) {
				g_free (comp_data->completed);
				comp_data->completed = NULL;
			}
			if (comp_data->created) {
				g_free (comp_data->created);
				comp_data->created = NULL;
			}
			if (comp_data->lastmodified) {
				g_free (comp_data->lastmodified);
				comp_data->lastmodified = NULL;
			}
			if (comp_data->color) {
				g_free (comp_data->color);
				comp_data->color = NULL;
			}

			comp_data->icalcomp = icalcomponent_new_clone (l->data);
			e_cal_model_set_instance_times (comp_data, priv->zone);

			pos = get_position_in_array (priv->objects, comp_data);

			e_table_model_row_changed (E_TABLE_MODEL (model), pos);
		}
	}

	e_cal_view_objects_added_cb (query, list, model);
	g_list_free (list);
}

static void
process_removed (ECalView *query, GList *ids, ECalModel *model)
{
	ECalModelPrivate *priv;
	GList *l;

	priv = model->priv;

	for (l = ids; l; l = l->next) {
		ECalModelComponent *comp_data = NULL;
		ECalComponentId *id = l->data;
		gint pos;

		/* make sure we remove all objects with this UID */
		while ((comp_data = search_by_id_and_client (priv, e_cal_view_get_client (query), id))) {
			GSList *l = NULL;

			pos = get_position_in_array (priv->objects, comp_data);

			if (!g_ptr_array_remove (priv->objects, comp_data))
				continue;

			l = g_slist_append (l, comp_data);
			g_signal_emit (G_OBJECT (model), signals[COMPS_DELETED], 0, l);

			g_slist_free (l);
			g_object_unref (comp_data);

			e_table_model_pre_change (E_TABLE_MODEL (model));
			e_table_model_row_deleted (E_TABLE_MODEL (model), pos);
		}
	}

	/* to notify about changes, because in call of row_deleted there are still all events */
	e_table_model_changed (E_TABLE_MODEL (model));
}

static gpointer
copy_comp_id (gpointer id)
{
	ECalComponentId *comp_id = (ECalComponentId *) id, *copy;

	g_return_val_if_fail (comp_id != NULL, NULL);

	copy = g_new0 (ECalComponentId, 1);
	copy->uid = g_strdup (comp_id->uid);
	copy->rid = g_strdup (comp_id->rid);

	return copy;
}

static void
free_comp_id (gpointer id)
{
	ECalComponentId *comp_id = (ECalComponentId *) id;

	g_return_if_fail (comp_id != NULL);

	g_free (comp_id->uid);
	g_free (comp_id->rid);
	g_free (comp_id);
}

static void
process_event (ECalView *query, GList *objects, ECalModel *model,
	void (*process_fn) (ECalView *query, GList *objects, ECalModel *model),
	gboolean *in, GList **save_list, gpointer (*copy_fn) (gpointer data), void (*free_fn)(gpointer data))
{
	gboolean skip = FALSE;
	GList *l;

	g_mutex_lock (model->priv->notify_lock);
	if (*in) {
		skip = TRUE;
		for (l = objects; l; l = l->next) {
			if (l->data)
				*save_list = g_list_append (*save_list, copy_fn (l->data));
		}
	} else {
		*in = TRUE;
	}

	g_mutex_unlock (model->priv->notify_lock);

	if (skip)
		return;

	/* do it */
	process_fn (query, objects, model);

	g_mutex_lock (model->priv->notify_lock);
	while (*save_list) {
		GList *copy = *save_list;
		*save_list = NULL;
		g_mutex_unlock (model->priv->notify_lock);

		/* do it */
		process_fn (query, copy, model);

		for (l = copy; l; l = l->next) {
			if (l->data) {
				free_fn (l->data);
			}
		}
		g_list_free (copy);

		g_mutex_lock (model->priv->notify_lock);
	}

	*in = FALSE;
	g_mutex_unlock (model->priv->notify_lock);
}

static void
e_cal_view_objects_added_cb (ECalView *query, GList *objects, ECalModel *model)
{
	process_event (query, objects, model,
		process_added, &model->priv->in_added, &model->priv->notify_added,
		(gpointer (*)(gpointer)) icalcomponent_new_clone, (void (*)(gpointer)) icalcomponent_free);
}

static void
e_cal_view_objects_modified_cb (ECalView *query, GList *objects, ECalModel *model)
{
	process_event (query, objects, model,
		process_modified, &model->priv->in_modified, &model->priv->notify_modified,
		(gpointer (*)(gpointer)) icalcomponent_new_clone, (void (*)(gpointer)) icalcomponent_free);
}

static void
e_cal_view_objects_removed_cb (ECalView *query, GList *ids, ECalModel *model)
{
	process_event (query, ids, model,
		process_removed, &model->priv->in_removed, &model->priv->notify_removed,
		copy_comp_id, free_comp_id);
}

static void
e_cal_view_progress_cb (ECalView *query, const gchar *message, gint percent, gpointer user_data)
{
	ECalModel *model = (ECalModel *) user_data;
	ECal *client = e_cal_view_get_client (query);

	g_return_if_fail (E_IS_CAL_MODEL (model));

	g_signal_emit (G_OBJECT (model), signals[CAL_VIEW_PROGRESS], 0, message,
			percent, e_cal_get_source_type (client));
}

static void
e_cal_view_complete_cb (ECalView *query, ECalendarStatus status, const gchar *error_msg, gpointer user_data)
{
	ECalModel *model = (ECalModel *) user_data;
	ECal *client = e_cal_view_get_client (query);

	g_return_if_fail (E_IS_CAL_MODEL (model));

	#ifndef E_CAL_DISABLE_DEPRECATED
	/* emit the signal on the model and let the view catch it to display */
	g_signal_emit (G_OBJECT (model), signals[CAL_VIEW_DONE], 0, status,
			e_cal_get_source_type (client));
	#endif
	g_signal_emit (G_OBJECT (model), signals[CAL_VIEW_COMPLETE], 0, status, error_msg,
			e_cal_get_source_type (client));
}

static void
update_e_cal_view_for_client (ECalModel *model, ECalModelClient *client_data)
{
	ECalModelPrivate *priv;
	GError *error = NULL;
	gint tries = 0;

	priv = model->priv;

	/* Skip if this client has not finished loading yet */
	if (e_cal_get_load_state (client_data->client) != E_CAL_LOAD_LOADED)
		return;

	/* free the previous query, if any */
	if (client_data->query) {
		g_signal_handlers_disconnect_matched (client_data->query, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, model);
		g_object_unref (client_data->query);
		client_data->query = NULL;
	}

	/* prepare the query */
	g_return_if_fail (priv->full_sexp != NULL);

	/* Don't create the new query if we won't use it */
	if (!client_data->do_query)
		return;

try_again:
	if (!e_cal_get_query (client_data->client, priv->full_sexp, &client_data->query, &error)) {
		if (error->code == E_CALENDAR_STATUS_BUSY && tries != 10) {
			tries++;
			/*TODO chose an optimal value */
			g_usleep (500);
			g_clear_error (&error);
			goto try_again;
		}

		g_warning (G_STRLOC ": Unable to get query, %s", error->message);

		return;
	}

	g_signal_connect (client_data->query, "objects_added", G_CALLBACK (e_cal_view_objects_added_cb), model);
	g_signal_connect (client_data->query, "objects_modified", G_CALLBACK (e_cal_view_objects_modified_cb), model);
	g_signal_connect (client_data->query, "objects_removed", G_CALLBACK (e_cal_view_objects_removed_cb), model);
	g_signal_connect (client_data->query, "view_progress", G_CALLBACK (e_cal_view_progress_cb), model);
	g_signal_connect (client_data->query, "view_complete", G_CALLBACK (e_cal_view_complete_cb), model);

	e_cal_view_start (client_data->query);
}

void
e_cal_model_update_status_message (ECalModel *model, const gchar *message, gdouble percent)
{
	g_return_if_fail (model != NULL);

	g_signal_emit (model, signals[STATUS_MESSAGE], 0, message, percent);
}

static void
backend_died_cb (ECal *client, gpointer user_data)
{
	ECalModel *model;

	model = E_CAL_MODEL (user_data);

	e_cal_model_remove_client (model, client);
}

static gboolean
wait_open_cb (gpointer data)
{
	ECal *client = data;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (E_IS_CAL (client), FALSE);

	e_cal_open_async (client, FALSE);

	return FALSE;
}

static void
cal_opened_cb (ECal *client, const GError *error, gpointer user_data)
{
	ECalModel *model = (ECalModel *) user_data;
	ECalModelClient *client_data;

	if (g_error_matches (error, E_CALENDAR_ERROR, E_CALENDAR_STATUS_BUSY)) {
		g_timeout_add (250, wait_open_cb, client);
		return;
	}

	/* Stop listening for this calendar to be opened */
	g_signal_handlers_disconnect_matched (client, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA, 0, 0, NULL, cal_opened_cb, model);

	if (error) {
		e_cal_model_remove_client (model, client);
		e_cal_model_update_status_message (model, NULL, -1.0);
		return;
	}

	e_cal_model_update_status_message (model, NULL, -1.0);

	client_data = find_client_data (model, client);
	g_return_if_fail (client_data);

	update_e_cal_view_for_client (model, client_data);
}

static ECalModelClient *
add_new_client (ECalModel *model, ECal *client, gboolean do_query)
{
	ECalModelPrivate *priv;
	ECalModelClient *client_data;

	priv = model->priv;

	/* DEBUG the load state should always be loaded here
	if (e_cal_get_load_state (client) != E_CAL_LOAD_LOADED) {
		g_assert_not_reached ();
	} */

	/* Look to see if we already have this client */
	client_data = find_client_data (model, client);
	if (client_data) {
		if (client_data->do_query)
			return client_data;
		else
			client_data->do_query = do_query;

		goto load;
	}

	client_data = g_new0 (ECalModelClient, 1);
	client_data->client = g_object_ref (client);
	client_data->query = NULL;
	client_data->do_query = do_query;

	priv->clients = g_list_append (priv->clients, client_data);

	g_signal_connect (G_OBJECT (client_data->client), "backend_died",
			  G_CALLBACK (backend_died_cb), model);

 load:
	if (e_cal_get_load_state (client) == E_CAL_LOAD_LOADED) {
		update_e_cal_view_for_client (model, client_data);
	} else {
		gchar *msg;

		msg = g_strdup_printf (_("Opening %s"), e_cal_get_uri (client));
		e_cal_model_update_status_message (model, msg, -1.0);
		g_free (msg);

		e_cal_set_default_timezone (client, e_cal_model_get_timezone (model), NULL);

		g_signal_connect (client, "cal_opened_ex", G_CALLBACK (cal_opened_cb), model);
		e_cal_open_async (client, TRUE);
	}

	return client_data;
}

/**
 * e_cal_model_add_client
 */
void
e_cal_model_add_client (ECalModel *model, ECal *client)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL (client));

	add_new_client (model, client, TRUE);
}

static void
remove_client_objects (ECalModel *model, ECalModelClient *client_data)
{
	gint i;

	/* remove all objects belonging to this client */
	for (i = model->priv->objects->len; i > 0; i--) {
		ECalModelComponent *comp_data = (ECalModelComponent *) g_ptr_array_index (model->priv->objects, i - 1);

		g_return_if_fail (comp_data != NULL);

		if (comp_data->client == client_data->client) {
			GSList *l = NULL;

			g_ptr_array_remove (model->priv->objects, comp_data);

			l = g_slist_append (l, comp_data);
			g_signal_emit (G_OBJECT (model), signals[COMPS_DELETED], 0, l);

			g_slist_free (l);
			g_object_unref (comp_data);

			e_table_model_pre_change (E_TABLE_MODEL (model));
			e_table_model_row_deleted (E_TABLE_MODEL (model), i - 1);
		}
	}

	/* to notify about changes, because in call of row_deleted there are still all events */
	e_table_model_changed (E_TABLE_MODEL (model));
}

static void
remove_client (ECalModel *model, ECalModelClient *client_data)
{
	/* FIXME We might not want to disconnect the open signal for the default client */
	g_signal_handlers_disconnect_matched (client_data->client, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);
	if (client_data->query)
		g_signal_handlers_disconnect_matched (client_data->query, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);

	remove_client_objects (model, client_data);

	/* If this is the default client and we were querying (so it
	 * was also a source), keep it around but don't query it */
	if (model->priv->default_client == client_data->client && client_data->do_query) {
		client_data->do_query = FALSE;

		return;
	}

	if (model->priv->default_client == client_data->client)
		model->priv->default_client = NULL;

	/* Remove the client from the list */
	model->priv->clients = g_list_remove (model->priv->clients, client_data);

	/* free all remaining memory */
	g_object_unref (client_data->client);
	if (client_data->query)
		g_object_unref (client_data->query);
	g_free (client_data);
}

/**
 * e_cal_model_remove_client
 */
void
e_cal_model_remove_client (ECalModel *model, ECal *client)
{
	ECalModelClient *client_data;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL (client));

	client_data = find_client_data (model, client);
	if (client_data)
		remove_client (model, client_data);
}

/**
 * e_cal_model_remove_all_clients
 */
void
e_cal_model_remove_all_clients (ECalModel *model)
{
	ECalModelPrivate *priv;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;
	while (priv->clients != NULL) {
		ECalModelClient *client_data = (ECalModelClient *) priv->clients->data;
		remove_client (model, client_data);
	}
}

static GSList *
get_objects_as_list (ECalModel *model)
{
	gint i;
	GSList *l = NULL;
	ECalModelPrivate *priv = model->priv;

	for (i = 0; i < priv->objects->len; i++) {
		ECalModelComponent *comp_data;

		comp_data = g_ptr_array_index (priv->objects, i);
		if (comp_data == NULL) {
			g_warning ("comp_data is null\n");
			continue;
		}

		l = g_slist_prepend (l, comp_data);
	}

	return l;
}

struct cc_data
{
	ECalModel *model;
	EFlag *eflag;
};

static gboolean
cleanup_content_cb (gpointer user_data)
{
	ECalModel *model;
	ECalModelPrivate *priv;
	GSList *slist;
	gint len;
	struct cc_data *data = user_data;

	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (data->model != NULL, FALSE);
	g_return_val_if_fail (data->eflag != NULL, FALSE);

	model = data->model;
	priv = model->priv;

	g_return_val_if_fail (priv != NULL, FALSE);

	e_table_model_pre_change (E_TABLE_MODEL (model));
	len = priv->objects->len;

	slist = get_objects_as_list (model);
	g_ptr_array_set_size (priv->objects, 0);
	g_signal_emit (G_OBJECT (model), signals[COMPS_DELETED], 0, slist);

	e_table_model_rows_deleted (E_TABLE_MODEL (model), 0, len);

	g_slist_foreach (slist, (GFunc)g_object_unref, NULL);
	g_slist_free (slist);

	e_flag_set (data->eflag);

	return FALSE;
}

static void
redo_queries (ECalModel *model)
{
	ECalModelPrivate *priv;
	GList *l;
	struct cc_data data;

	priv = model->priv;

	if (priv->full_sexp)
		g_free (priv->full_sexp);

	if (priv->start != -1 && priv->end != -1) {
		gchar *iso_start, *iso_end;

		iso_start = isodate_from_time_t (priv->start);
		iso_end = isodate_from_time_t (priv->end);

		if (priv->search_sexp) {
			priv->full_sexp = g_strdup_printf ("(and (occur-in-time-range? (make-time \"%s\")"
					"                           (make-time \"%s\"))"
					"     %s)",
					iso_start, iso_end,
					priv->search_sexp ? priv->search_sexp : "");
		} else {
			priv->full_sexp = g_strdup_printf ("(occur-in-time-range? (make-time \"%s\")"
					"                           (make-time \"%s\"))",
					iso_start, iso_end);
		}

		g_free (iso_start);
		g_free (iso_end);
	} else if (priv->search_sexp) {
		priv->full_sexp = g_strdup (priv->search_sexp);
	} else {
		priv->full_sexp = g_strdup ("#f");
	}

	/* clean up the current contents, which should be done
	   always from the main thread, because of gtk calls during removal */
	data.model = model;
	data.eflag = e_flag_new ();

	if (!g_main_context_is_owner (g_main_context_default ())) {
		/* function called from other than main thread */
		g_timeout_add (10, cleanup_content_cb, &data);
		e_flag_wait (data.eflag);
	} else {
		cleanup_content_cb (&data);
	}

	e_flag_free (data.eflag);

	/* update the query for all clients */
	for (l = priv->clients; l != NULL; l = l->next) {
		ECalModelClient *client_data;

		client_data = (ECalModelClient *) l->data;
		update_e_cal_view_for_client (model, client_data);
	}
}

void
e_cal_model_get_time_range (ECalModel *model, time_t *start, time_t *end)
{
	ECalModelPrivate *priv;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	if (start)
		*start = priv->start;

	if (end)
		*end = priv->end;
}

void
e_cal_model_set_time_range (ECalModel *model, time_t start, time_t end)
{
	ECalModelPrivate *priv;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (start >= 0 && end >= 0);
	g_return_if_fail (start <= end);

	priv = model->priv;

	if (priv->start == start && priv->end == end)
		return;

	priv->start = start;
	priv->end = end;

	g_signal_emit (G_OBJECT (model), signals[TIME_RANGE_CHANGED], 0, start, end);
	redo_queries (model);
}

const gchar *
e_cal_model_get_search_query (ECalModel *model)
{
	ECalModelPrivate *priv;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	return priv->search_sexp;
}

/**
 * e_cal_model_set_query
 */
void
e_cal_model_set_search_query (ECalModel *model, const gchar *sexp)
{
	ECalModelPrivate *priv;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	if (!strcmp (sexp ? sexp : "", priv->search_sexp ? priv->search_sexp : ""))
		return;

	if (priv->search_sexp)
		g_free (priv->search_sexp);

	priv->search_sexp = g_strdup (sexp);

	redo_queries (model);
}

/**
 * e_cal_model_set_query
 */
void
e_cal_model_set_search_query_with_time_range (ECalModel *model, const gchar *sexp, time_t start, time_t end)
{
	ECalModelPrivate *priv;
	gboolean do_query = FALSE;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	if (strcmp (sexp ? sexp : "", priv->search_sexp ? priv->search_sexp : "")) {
		if (priv->search_sexp)
			g_free (priv->search_sexp);

		priv->search_sexp = g_strdup (sexp);
		do_query = TRUE;
	}

	if (!(priv->start == start && priv->end == end)) {
		priv->start = start;
		priv->end = end;
		do_query = TRUE;

		g_signal_emit (G_OBJECT (model), signals[TIME_RANGE_CHANGED], 0, start, end);
	}

	if (do_query)
		redo_queries (model);
}

/**
 * e_cal_model_create_component_with_defaults
 */
icalcomponent *
e_cal_model_create_component_with_defaults (ECalModel *model, gboolean all_day)
{
	ECalModelPrivate *priv;
	ECalComponent *comp;
	icalcomponent *icalcomp;
	ECal *client;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	g_return_val_if_fail (priv->clients != NULL, NULL);

	client = e_cal_model_get_default_client (model);
	if (!client)
		return icalcomponent_new (priv->kind);

	switch (priv->kind) {
	case ICAL_VEVENT_COMPONENT :
		comp = cal_comp_event_new_with_defaults (client, all_day);
		break;
	case ICAL_VTODO_COMPONENT :
		comp = cal_comp_task_new_with_defaults (client);
		break;
	case ICAL_VJOURNAL_COMPONENT :
		comp = cal_comp_memo_new_with_defaults (client);
		break;
	default:
		return NULL;
	}

	if (!comp)
		return icalcomponent_new (priv->kind);

	icalcomp = icalcomponent_new_clone (e_cal_component_get_icalcomponent (comp));
	g_object_unref (comp);

	/* make sure the component has an UID */
	if (!icalcomponent_get_uid (icalcomp)) {
		gchar *uid;

		uid = e_cal_component_gen_uid ();
		icalcomponent_set_uid (icalcomp, uid);

		g_free (uid);
	}

	return icalcomp;
}

/**
 * e_cal_model_get_color_for_component
 */
const gchar *
e_cal_model_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data)
{
	ECalModelClass *model_class;
	const gchar *color = NULL;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (comp_data != NULL, NULL);

	model_class = (ECalModelClass *) G_OBJECT_GET_CLASS (model);
	if (model_class->get_color_for_component != NULL)
		color = model_class->get_color_for_component (model, comp_data);

	if (!color)
		color = ecm_get_color_for_component (model, comp_data);

	return color;
}

/**
 * e_cal_model_get_rgb_color_for_component
 */
gboolean
e_cal_model_get_rgb_color_for_component (ECalModel *model, ECalModelComponent *comp_data, double *red, double *green, double *blue)
{
	GdkColor gdk_color;
	const gchar *color;

	color = e_cal_model_get_color_for_component (model, comp_data);
	if (color && gdk_color_parse (color, &gdk_color)) {

		if (red)
			*red = ((double) gdk_color.red)/0xffff;
		if (green)
			*green = ((double) gdk_color.green)/0xffff;
		if (blue)
			*blue = ((double) gdk_color.blue)/0xffff;

		return TRUE;
	}

	return FALSE;
}

/**
 * e_cal_model_get_component_at
 */
ECalModelComponent *
e_cal_model_get_component_at (ECalModel *model, gint row)
{
	ECalModelPrivate *priv;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	g_return_val_if_fail (row >= 0 && row < priv->objects->len, NULL);

	return g_ptr_array_index (priv->objects, row);
}

ECalModelComponent *
e_cal_model_get_component_for_uid  (ECalModel *model, const ECalComponentId *id)
{
	ECalModelPrivate *priv;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	return search_by_id_and_client (priv, NULL, id);
}

/**
 * e_cal_model_date_value_to_string
 */
gchar *
e_cal_model_date_value_to_string (ECalModel *model, gconstpointer value)
{
	ECalModelPrivate *priv;
	ECellDateEditValue *dv = (ECellDateEditValue *) value;
	struct icaltimetype tt;
	struct tm tmp_tm;
	gchar buffer[64];

	g_return_val_if_fail (E_IS_CAL_MODEL (model), g_strdup (""));

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

	memset (buffer, 0, sizeof (buffer));
	e_time_format_date_and_time (&tmp_tm, priv->use_24_hour_format,
				     TRUE, FALSE,
				     buffer, sizeof (buffer));
	return g_strdup (buffer);
}

/* FIXME is it still needed ?
static ECellDateEditValue *
copy_ecdv (ECellDateEditValue *ecdv)
{
	ECellDateEditValue *new_ecdv;

	new_ecdv = g_new0 (ECellDateEditValue, 1);
	new_ecdv->tt = ecdv ? ecdv->tt : icaltime_null_time ();
	new_ecdv->zone = ecdv ? ecdv->zone : NULL;

	return new_ecdv;
} */

struct _ECalModelComponentPrivate {
	gchar nouse;
};

static void e_cal_model_component_finalize (GObject *object);

static GObjectClass *component_parent_class;

/* Class initialization function for the calendar component object */
static void
e_cal_model_component_class_init (ECalModelComponentClass *class)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) class;

	component_parent_class = g_type_class_peek_parent (class);

	object_class->finalize = e_cal_model_component_finalize;
}

static void
e_cal_model_component_finalize (GObject *object)
{
	ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT(object);

	if (comp_data->client) {
		g_object_unref (comp_data->client);
		comp_data->client = NULL;
	}
	if (comp_data->icalcomp) {
		icalcomponent_free (comp_data->icalcomp);
		comp_data->icalcomp = NULL;
	}
	if (comp_data->dtstart) {
		g_free (comp_data->dtstart);
		comp_data->dtstart = NULL;
	}
	if (comp_data->dtend) {
		g_free (comp_data->dtend);
		comp_data->dtend = NULL;
	}
	if (comp_data->due) {
		g_free (comp_data->due);
		comp_data->due = NULL;
	}
	if (comp_data->completed) {
		g_free (comp_data->completed);
		comp_data->completed = NULL;
	}
	if (comp_data->created) {
		g_free (comp_data->created);
		comp_data->created = NULL;
	}
	if (comp_data->lastmodified) {
		g_free (comp_data->lastmodified);
		comp_data->lastmodified = NULL;
	}
	if (comp_data->color) {
		g_free (comp_data->color);
		comp_data->color = NULL;
	}

	if (G_OBJECT_CLASS (component_parent_class)->finalize)
		(* G_OBJECT_CLASS (component_parent_class)->finalize) (object);
}

/* Object initialization function for the calendar component object */
static void
e_cal_model_component_init (ECalModelComponent *comp)
{
	comp->dtstart = NULL;
	comp->dtend = NULL;
	comp->due = NULL;
	comp->completed = NULL;
	comp->created = NULL;
	comp->lastmodified = NULL;
	comp->color = NULL;
}

GType
e_cal_model_component_get_type (void)
{
	static GType e_cal_model_component_type = 0;

	if (!e_cal_model_component_type) {
		static GTypeInfo info = {
                        sizeof (ECalModelComponentClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) e_cal_model_component_class_init,
                        NULL, NULL,
                        sizeof (ECalModelComponent),
                        0,
                        (GInstanceInitFunc) e_cal_model_component_init
                };
		e_cal_model_component_type = g_type_register_static (G_TYPE_OBJECT, "ECalModelComponent", &info, 0);
	}

	return e_cal_model_component_type;
}

/**
 * e_cal_model_generate_instances
 *
 * cb function is not called with cb_data, but with ECalModelGenerateInstancesData which contains cb_data
 */
void
e_cal_model_generate_instances (ECalModel *model, time_t start, time_t end,
				ECalRecurInstanceFn cb, gpointer cb_data)
{
	ECalModelGenerateInstancesData mdata;
	gint i, n;

	n = e_table_model_row_count (E_TABLE_MODEL (model));
	for (i = 0; i < n; i++) {
		ECalModelComponent *comp_data = e_cal_model_get_component_at (model, i);

		mdata.comp_data = comp_data;
		mdata.cb_data = cb_data;

		if (comp_data->instance_start < end && comp_data->instance_end > start)
			e_cal_generate_instances_for_object (comp_data->client, comp_data->icalcomp, start, end, cb, &mdata);
	}
}

/**
 * e_cal_model_get_object_array
 */
GPtrArray *
e_cal_model_get_object_array (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return model->priv->objects;
}

void
e_cal_model_set_instance_times (ECalModelComponent *comp_data, const icaltimezone *zone)
{
	struct icaltimetype start_time, end_time;
	icalcomponent_kind kind;

	kind = icalcomponent_isa (comp_data->icalcomp);
	start_time = icalcomponent_get_dtstart (comp_data->icalcomp);
	end_time = icalcomponent_get_dtend (comp_data->icalcomp);

	if (kind == ICAL_VEVENT_COMPONENT) {
		if (start_time.is_date && icaltime_is_null_time (end_time)) {
			/* If end_time is null and it's an all day event,
			 * just make start_time = end_time so that end_time
			 * will be a valid date
			 */
			end_time = start_time;
			icaltime_adjust (&end_time, 1, 0, 0, 0);
			icalcomponent_set_dtend (comp_data->icalcomp, end_time);
		} else if (start_time.is_date && end_time.is_date &&
			   (icaltime_compare_date_only (start_time, end_time) == 0)) {
			/* If both DTSTART and DTEND are DATE values, and they are the
			   same day, we add 1 day to DTEND. This means that most
			   events created with the old Evolution behavior will still
			   work OK. */
			icaltime_adjust (&end_time, 1, 0, 0, 0);
			icalcomponent_set_dtend (comp_data->icalcomp, end_time);
		}
	}

	if (start_time.zone)
		zone = start_time.zone;
	else {
		icalparameter *param = NULL;
		icalproperty *prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DTSTART_PROPERTY);

	       if (prop)	{
			param = icalproperty_get_first_parameter (prop, ICAL_TZID_PARAMETER);

			if (param) {
				const gchar *tzid = NULL;
				icaltimezone *st_zone = NULL;

				tzid = icalparameter_get_tzid (param);
				e_cal_get_timezone (comp_data->client, tzid, &st_zone, NULL);

				if (st_zone)
					zone = st_zone;
			}
	       }
	}

	comp_data->instance_start = icaltime_as_timet_with_zone (start_time, zone);

	if (end_time.zone)
		zone = end_time.zone;
	else {
		icalparameter *param = NULL;
		icalproperty *prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DTSTART_PROPERTY);

	       if (prop)	{
			param = icalproperty_get_first_parameter (prop, ICAL_TZID_PARAMETER);

			if (param) {
				const gchar *tzid = NULL;
				icaltimezone *end_zone = NULL;

				tzid = icalparameter_get_tzid (param);
				e_cal_get_timezone (comp_data->client, tzid, &end_zone, NULL);

				if (end_zone)
					zone = end_zone;
			}
	       }

	}
	comp_data->instance_end = icaltime_as_timet_with_zone (end_time, zone);
}

/**
 * e_cal_model_set_default_time_func:
 * This function will be used when creating new item from the "click-to-add",
 * when user didn't fill a start date there.
 **/
void
e_cal_model_set_default_time_func (ECalModel *model, ECalModelDefaultTimeFunc func, gpointer user_data)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	model->priv->get_default_time = func;
	model->priv->get_default_time_user_data = user_data;
}
