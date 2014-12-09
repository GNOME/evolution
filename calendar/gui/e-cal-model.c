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

#include <string.h>
#include <glib/gi18n.h>

#include <libebackend/libebackend.h>

#include <e-util/e-util.h>
#include <e-util/e-util-enumtypes.h>

#include "comp-util.h"
#include "e-cal-model.h"
#include "itip-utils.h"
#include "misc.h"

typedef struct _ClientData ClientData;

struct _ECalModelComponentPrivate {
	GString *categories_str;
};

#define E_CAL_MODEL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_MODEL, ECalModelPrivate))

#define E_CAL_MODEL_COMPONENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_MODEL_COMPONENT, ECalModelComponentPrivate))

struct _ClientData {
	volatile gint ref_count;
	GWeakRef model;
	ECalClient *client;

	GMutex view_lock;
	gboolean do_query;
	ECalClientView *view;
	GCancellable *cancellable;

	gulong backend_died_handler_id;
	gulong objects_added_handler_id;
	gulong objects_modified_handler_id;
	gulong objects_removed_handler_id;
	gulong progress_handler_id;
	gulong complete_handler_id;
};

struct _ECalModelPrivate {
	ESourceRegistry *registry;

	/* Queue of ClientData structs */
	GQueue clients;
	GMutex clients_lock;

	/* The default client in the list */
	ECalClient *default_client;

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

	/* Whether we display dates in 24-hour format. */
        gboolean use_24_hour_format;

	/* Whether to compress weekends into one cell. */
	gboolean compress_weekend;

	/* First day of the week */
	GDateWeekday week_start_day;

	/* Work days.  Indices are based on GDateWeekday.
	 * The first element (G_DATE_BAD_WEEKDAY) is unused. */
	gboolean work_days[G_DATE_SUNDAY + 1];

	/* Work day timespan */
	gint work_day_start_hour;
	gint work_day_start_minute;
	gint work_day_end_hour;
	gint work_day_end_minute;

	/* callback, to retrieve start time for newly added rows by click-to-add */
	ECalModelDefaultTimeFunc get_default_time;
	gpointer get_default_time_user_data;

	/* Default reminder for events */
	gboolean use_default_reminder;
	gint default_reminder_interval;
	EDurationType default_reminder_units;

	/* Ask user to confirm before deleting components. */
	gboolean confirm_delete;

	gboolean in_added;
	gboolean in_modified;
	gboolean in_removed;

	GHashTable *notify_added;
	GHashTable *notify_modified;
	GHashTable *notify_removed;

	GMutex notify_lock;

	GCancellable *loading_clients;
};

typedef struct {
	const gchar *color;
	GList *uids;
} AssignedColorData;

static const gchar *cal_model_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data);

static gboolean add_new_client (ECalModel *model, ECalClient *client, gboolean do_query);
static void remove_client_objects (ECalModel *model, ClientData *client_data);
static void remove_client (ECalModel *model, ClientData *client_data);
static void redo_queries (ECalModel *model);

enum {
	PROP_0,
	PROP_COMPRESS_WEEKEND,
	PROP_CONFIRM_DELETE,
	PROP_DEFAULT_CLIENT,
	PROP_DEFAULT_REMINDER_INTERVAL,
	PROP_DEFAULT_REMINDER_UNITS,
	PROP_REGISTRY,
	PROP_TIMEZONE,
	PROP_USE_24_HOUR_FORMAT,
	PROP_USE_DEFAULT_REMINDER,
	PROP_WEEK_START_DAY,
	PROP_WORK_DAY_MONDAY,
	PROP_WORK_DAY_TUESDAY,
	PROP_WORK_DAY_WEDNESDAY,
	PROP_WORK_DAY_THURSDAY,
	PROP_WORK_DAY_FRIDAY,
	PROP_WORK_DAY_SATURDAY,
	PROP_WORK_DAY_SUNDAY,
	PROP_WORK_DAY_END_HOUR,
	PROP_WORK_DAY_END_MINUTE,
	PROP_WORK_DAY_START_HOUR,
	PROP_WORK_DAY_START_MINUTE
};

enum {
	TIME_RANGE_CHANGED,
	ROW_APPENDED,
	COMPS_DELETED,
	CAL_VIEW_PROGRESS,
	CAL_VIEW_COMPLETE,
	STATUS_MESSAGE,
	TIMEZONE_CHANGED,
	LAST_SIGNAL
};

/* Forward Declarations */
static void	e_cal_model_table_model_init
					(ETableModelInterface *iface);

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (
	ECalModel,
	e_cal_model,
	G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_TABLE_MODEL,
		e_cal_model_table_model_init))

G_DEFINE_TYPE (
	ECalModelComponent,
	e_cal_model_component,
	G_TYPE_OBJECT)

static void
client_data_backend_died_cb (ECalClient *client,
                             ClientData *client_data)
{
	ECalModel *model;

	model = g_weak_ref_get (&client_data->model);
	if (model != NULL) {
		e_cal_model_remove_client (model, client);
		g_object_unref (model);
	}
}

static ClientData *
client_data_new (ECalModel *model,
                 ECalClient *client,
                 gboolean do_query)
{
	ClientData *client_data;
	gulong handler_id;

	client_data = g_slice_new0 (ClientData);
	client_data->ref_count = 1;
	g_weak_ref_set (&client_data->model, model);
	client_data->client = g_object_ref (client);
	client_data->do_query = do_query;

	g_mutex_init (&client_data->view_lock);

	handler_id = g_signal_connect (
		client_data->client, "backend-died",
		G_CALLBACK (client_data_backend_died_cb), client_data);
	client_data->backend_died_handler_id = handler_id;

	return client_data;
}

static void
client_data_disconnect_view_handlers (ClientData *client_data)
{
	/* This MUST be called with the view_lock acquired. */

	g_return_if_fail (client_data->view != NULL);

	if (client_data->objects_added_handler_id > 0) {
		g_signal_handler_disconnect (
			client_data->view,
			client_data->objects_added_handler_id);
		client_data->objects_added_handler_id = 0;
	}

	if (client_data->objects_modified_handler_id > 0) {
		g_signal_handler_disconnect (
			client_data->view,
			client_data->objects_modified_handler_id);
		client_data->objects_modified_handler_id = 0;
	}

	if (client_data->objects_removed_handler_id > 0) {
		g_signal_handler_disconnect (
			client_data->view,
			client_data->objects_removed_handler_id);
		client_data->objects_removed_handler_id = 0;
	}

	if (client_data->progress_handler_id > 0) {
		g_signal_handler_disconnect (
			client_data->view,
			client_data->progress_handler_id);
		client_data->progress_handler_id = 0;
	}

	if (client_data->complete_handler_id > 0) {
		g_signal_handler_disconnect (
			client_data->view,
			client_data->complete_handler_id);
		client_data->complete_handler_id = 0;
	}
}

static ClientData *
client_data_ref (ClientData *client_data)
{
	g_return_val_if_fail (client_data != NULL, NULL);
	g_return_val_if_fail (client_data->ref_count > 0, NULL);

	g_atomic_int_inc (&client_data->ref_count);

	return client_data;
}

static void
client_data_unref (ClientData *client_data)
{
	g_return_if_fail (client_data != NULL);
	g_return_if_fail (client_data->ref_count > 0);

	if (g_atomic_int_dec_and_test (&client_data->ref_count)) {

		g_signal_handler_disconnect (
			client_data->client,
			client_data->backend_died_handler_id);

		if (client_data->view != NULL)
			client_data_disconnect_view_handlers (client_data);

		g_weak_ref_set (&client_data->model, NULL);

		g_clear_object (&client_data->client);
		g_clear_object (&client_data->view);
		g_clear_object (&client_data->cancellable);

		g_mutex_clear (&client_data->view_lock);

		g_slice_free (ClientData, client_data);
	}
}

static GList *
cal_model_clients_list (ECalModel *model)
{
	GList *list, *head;

	g_mutex_lock (&model->priv->clients_lock);

	head = g_queue_peek_head_link (&model->priv->clients);
	list = g_list_copy_deep (head, (GCopyFunc) client_data_ref, NULL);

	g_mutex_unlock (&model->priv->clients_lock);

	return list;
}

static ClientData *
cal_model_clients_lookup (ECalModel *model,
                          ECalClient *client)
{
	ClientData *client_data = NULL;
	GList *list, *link;

	list = cal_model_clients_list (model);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ClientData *candidate = link->data;

		if (candidate->client == client) {
			client_data = client_data_ref (candidate);
			break;
		}
	}

	g_list_free_full (list, (GDestroyNotify) client_data_unref);

	return client_data;
}

static ClientData *
cal_model_clients_peek (ECalModel *model)
{
	ClientData *client_data;

	g_mutex_lock (&model->priv->clients_lock);

	client_data = g_queue_peek_head (&model->priv->clients);
	if (client_data != NULL)
		client_data_ref (client_data);

	g_mutex_unlock (&model->priv->clients_lock);

	return client_data;
}

static ClientData *
cal_model_clients_pop (ECalModel *model)
{
	ClientData *client_data;

	g_mutex_lock (&model->priv->clients_lock);

	client_data = g_queue_pop_head (&model->priv->clients);

	g_mutex_unlock (&model->priv->clients_lock);

	return client_data;
}

static gboolean
cal_model_clients_remove (ECalModel *model,
                          ClientData *client_data)
{
	gboolean removed = FALSE;

	g_mutex_lock (&model->priv->clients_lock);

	if (g_queue_remove (&model->priv->clients, client_data)) {
		client_data_unref (client_data);
		removed = TRUE;
	}

	g_mutex_unlock (&model->priv->clients_lock);

	return removed;
}

static gpointer
get_categories (ECalModelComponent *comp_data)
{
	if (!comp_data->priv->categories_str) {
		icalproperty *prop;

		comp_data->priv->categories_str = g_string_new ("");

		for (prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_CATEGORIES_PROPERTY);
		     prop;
		     prop = icalcomponent_get_next_property (comp_data->icalcomp, ICAL_CATEGORIES_PROPERTY)) {
			const gchar *categories = icalproperty_get_categories (prop);
			if (!categories)
				continue;

			if (comp_data->priv->categories_str->len)
				g_string_append_c (comp_data->priv->categories_str, ',');
			g_string_append (comp_data->priv->categories_str, categories);
		}
	}

	return g_strdup (comp_data->priv->categories_str->str);
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
get_color (ECalModel *model,
           ECalModelComponent *comp_data)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return e_cal_model_get_color_for_component (model, comp_data);
}

static gpointer
get_description (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DESCRIPTION_PROPERTY);
	if (prop) {
		GString *str = g_string_new (NULL);
		do {
			str = g_string_append (str, icalproperty_get_description (prop));
		} while ((prop = icalcomponent_get_next_property (comp_data->icalcomp, ICAL_DESCRIPTION_PROPERTY)));

		return g_string_free (str, FALSE);
	}

	return g_strdup ("");
}

static ECellDateEditValue *
get_dtstart (ECalModel *model,
             ECalModelComponent *comp_data)
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
		    && e_cal_client_get_timezone_sync (comp_data->client, icaltime_get_tzid (tt_start), &zone, NULL, NULL))
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

	return e_cal_model_copy_cell_date_value (comp_data->dtstart);
}

static ECellDateEditValue *
get_datetime_from_utc (ECalModel *model,
                       ECalModelComponent *comp_data,
                       icalproperty_kind propkind,
                       struct icaltimetype (*get_value) (const icalproperty *prop),
		       ECellDateEditValue **buffer)
{
	g_return_val_if_fail (buffer != NULL, NULL);

	if (!*buffer) {
		ECalModelPrivate *priv;
		struct icaltimetype tt_value;
		icalproperty *prop;
		ECellDateEditValue *res;

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
	}

	return e_cal_model_copy_cell_date_value (*buffer);
}

static gpointer
get_summary (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_SUMMARY_PROPERTY);
	if (prop)
		return g_strdup (icalproperty_get_summary (prop));

	return g_strdup ("");
}

static gchar *
get_uid (ECalModelComponent *comp_data)
{
	return (gchar *) icalcomponent_get_uid (comp_data->icalcomp);
}

static void
set_categories (ECalModelComponent *comp_data,
                const gchar *value)
{
	icalproperty *prop;

	/* remove all categories first */
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_CATEGORIES_PROPERTY);
	while (prop) {
		icalproperty *to_remove = prop;
		prop = icalcomponent_get_next_property (comp_data->icalcomp, ICAL_CATEGORIES_PROPERTY);

		icalcomponent_remove_property (comp_data->icalcomp, to_remove);
		icalproperty_free (to_remove);
	}

	if (comp_data->priv->categories_str)
		g_string_free (comp_data->priv->categories_str, TRUE);
	comp_data->priv->categories_str = NULL;

	/* then set a new value; no need to populate categories_str,
	 * it'll be populated on demand (in the get_categories() function)
	*/
	if (value && *value) {
		prop = icalproperty_new_categories (value);
		icalcomponent_add_property (comp_data->icalcomp, prop);
	}
}

static void
set_classification (ECalModelComponent *comp_data,
                    const gchar *value)
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
set_description (ECalModelComponent *comp_data,
                 const gchar *value)
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
set_dtstart (ECalModel *model,
             ECalModelComponent *comp_data,
             gconstpointer value)
{
	e_cal_model_update_comp_time (
		model, comp_data, value,
		ICAL_DTSTART_PROPERTY,
		icalproperty_set_dtstart,
		icalproperty_new_dtstart);
}

static void
set_summary (ECalModelComponent *comp_data,
             const gchar *value)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (
		comp_data->icalcomp, ICAL_SUMMARY_PROPERTY);

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
datetime_to_zone (ECalClient *client,
                  struct icaltimetype *tt,
                  icaltimezone *tt_zone,
                  const gchar *tzid)
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
		e_cal_client_get_timezone_sync (client, tzid, &to, NULL, NULL);
	}

	icaltimezone_convert_time (tt, from, to);
}

static void
cal_model_set_registry (ECalModel *model,
                        ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (model->priv->registry == NULL);

	model->priv->registry = g_object_ref (registry);
}

static void
cal_model_set_property (GObject *object,
                        guint property_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COMPRESS_WEEKEND:
			e_cal_model_set_compress_weekend (
				E_CAL_MODEL (object),
				g_value_get_boolean (value));
			return;

		case PROP_CONFIRM_DELETE:
			e_cal_model_set_confirm_delete (
				E_CAL_MODEL (object),
				g_value_get_boolean (value));
			return;

		case PROP_DEFAULT_CLIENT:
			e_cal_model_set_default_client (
				E_CAL_MODEL (object),
				g_value_get_object (value));
			return;

		case PROP_DEFAULT_REMINDER_INTERVAL:
			e_cal_model_set_default_reminder_interval (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_DEFAULT_REMINDER_UNITS:
			e_cal_model_set_default_reminder_units (
				E_CAL_MODEL (object),
				g_value_get_enum (value));
			return;

		case PROP_REGISTRY:
			cal_model_set_registry (
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

		case PROP_USE_DEFAULT_REMINDER:
			e_cal_model_set_use_default_reminder (
				E_CAL_MODEL (object),
				g_value_get_boolean (value));
			return;

		case PROP_WEEK_START_DAY:
			e_cal_model_set_week_start_day (
				E_CAL_MODEL (object),
				g_value_get_enum (value));
			return;

		case PROP_WORK_DAY_MONDAY:
			e_cal_model_set_work_day (
				E_CAL_MODEL (object),
				G_DATE_MONDAY,
				g_value_get_boolean (value));
			return;

		case PROP_WORK_DAY_TUESDAY:
			e_cal_model_set_work_day (
				E_CAL_MODEL (object),
				G_DATE_TUESDAY,
				g_value_get_boolean (value));
			return;

		case PROP_WORK_DAY_WEDNESDAY:
			e_cal_model_set_work_day (
				E_CAL_MODEL (object),
				G_DATE_WEDNESDAY,
				g_value_get_boolean (value));
			return;

		case PROP_WORK_DAY_THURSDAY:
			e_cal_model_set_work_day (
				E_CAL_MODEL (object),
				G_DATE_THURSDAY,
				g_value_get_boolean (value));
			return;

		case PROP_WORK_DAY_FRIDAY:
			e_cal_model_set_work_day (
				E_CAL_MODEL (object),
				G_DATE_FRIDAY,
				g_value_get_boolean (value));
			return;

		case PROP_WORK_DAY_SATURDAY:
			e_cal_model_set_work_day (
				E_CAL_MODEL (object),
				G_DATE_SATURDAY,
				g_value_get_boolean (value));
			return;

		case PROP_WORK_DAY_SUNDAY:
			e_cal_model_set_work_day (
				E_CAL_MODEL (object),
				G_DATE_SUNDAY,
				g_value_get_boolean (value));
			return;

		case PROP_WORK_DAY_END_HOUR:
			e_cal_model_set_work_day_end_hour (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_MINUTE:
			e_cal_model_set_work_day_end_minute (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_HOUR:
			e_cal_model_set_work_day_start_hour (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_MINUTE:
			e_cal_model_set_work_day_start_minute (
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
		case PROP_COMPRESS_WEEKEND:
			g_value_set_boolean (
				value,
				e_cal_model_get_compress_weekend (
				E_CAL_MODEL (object)));
			return;

		case PROP_CONFIRM_DELETE:
			g_value_set_boolean (
				value,
				e_cal_model_get_confirm_delete (
				E_CAL_MODEL (object)));
			return;

		case PROP_DEFAULT_CLIENT:
			g_value_take_object (
				value,
				e_cal_model_ref_default_client (
				E_CAL_MODEL (object)));
			return;

		case PROP_DEFAULT_REMINDER_INTERVAL:
			g_value_set_int (
				value,
				e_cal_model_get_default_reminder_interval (
				E_CAL_MODEL (object)));
			return;

		case PROP_DEFAULT_REMINDER_UNITS:
			g_value_set_enum (
				value,
				e_cal_model_get_default_reminder_units (
				E_CAL_MODEL (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_cal_model_get_registry (
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

		case PROP_USE_DEFAULT_REMINDER:
			g_value_set_boolean (
				value,
				e_cal_model_get_use_default_reminder (
				E_CAL_MODEL (object)));
			return;

		case PROP_WEEK_START_DAY:
			g_value_set_enum (
				value,
				e_cal_model_get_week_start_day (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_MONDAY:
			g_value_set_boolean (
				value,
				e_cal_model_get_work_day (
				E_CAL_MODEL (object), G_DATE_MONDAY));
			return;

		case PROP_WORK_DAY_TUESDAY:
			g_value_set_boolean (
				value,
				e_cal_model_get_work_day (
				E_CAL_MODEL (object), G_DATE_TUESDAY));
			return;

		case PROP_WORK_DAY_WEDNESDAY:
			g_value_set_boolean (
				value,
				e_cal_model_get_work_day (
				E_CAL_MODEL (object), G_DATE_WEDNESDAY));
			return;

		case PROP_WORK_DAY_THURSDAY:
			g_value_set_boolean (
				value,
				e_cal_model_get_work_day (
				E_CAL_MODEL (object), G_DATE_THURSDAY));
			return;

		case PROP_WORK_DAY_FRIDAY:
			g_value_set_boolean (
				value,
				e_cal_model_get_work_day (
				E_CAL_MODEL (object), G_DATE_FRIDAY));
			return;

		case PROP_WORK_DAY_SATURDAY:
			g_value_set_boolean (
				value,
				e_cal_model_get_work_day (
				E_CAL_MODEL (object), G_DATE_SATURDAY));
			return;

		case PROP_WORK_DAY_SUNDAY:
			g_value_set_boolean (
				value,
				e_cal_model_get_work_day (
				E_CAL_MODEL (object), G_DATE_SUNDAY));
			return;

		case PROP_WORK_DAY_END_HOUR:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_end_hour (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_END_MINUTE:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_end_minute (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_START_HOUR:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_start_hour (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_START_MINUTE:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_start_minute (
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
	G_OBJECT_CLASS (e_cal_model_parent_class)->constructed (object);
}

static void
cal_model_dispose (GObject *object)
{
	ECalModelPrivate *priv;

	priv = E_CAL_MODEL_GET_PRIVATE (object);

	if (priv->registry != NULL) {
		g_object_unref (priv->registry);
		priv->registry = NULL;
	}

	if (priv->loading_clients) {
		g_cancellable_cancel (priv->loading_clients);
		g_object_unref (priv->loading_clients);
		priv->loading_clients = NULL;
	}

	while (!g_queue_is_empty (&priv->clients))
		client_data_unref (g_queue_pop_head (&priv->clients));

	priv->default_client = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_cal_model_parent_class)->dispose (object);
}

static void
cal_model_finalize (GObject *object)
{
	ECalModelPrivate *priv;
	gint ii;

	priv = E_CAL_MODEL_GET_PRIVATE (object);

	g_mutex_clear (&priv->clients_lock);

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
	g_ptr_array_free (priv->objects, TRUE);

	g_mutex_clear (&priv->notify_lock);

	g_hash_table_destroy (priv->notify_added);
	g_hash_table_destroy (priv->notify_modified);
	g_hash_table_destroy (priv->notify_removed);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_cal_model_parent_class)->finalize (object);
}

static const gchar *
cal_model_get_color_for_component (ECalModel *model,
                                   ECalModelComponent *comp_data)
{
	ESource *source;
	ESourceSelectable *extension;
	const gchar *color_spec;
	const gchar *extension_name;
	const gchar *uid;
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

	switch (e_cal_client_get_source_type (comp_data->client)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			break;
		default:
			g_return_val_if_reached (NULL);
	}

	source = e_client_get_source (E_CLIENT (comp_data->client));
	extension = e_source_get_extension (source, extension_name);
	color_spec = e_source_selectable_get_color (extension);

	if (color_spec != NULL) {
		g_free (comp_data->color);
		comp_data->color = g_strdup (color_spec);
		return comp_data->color;
	}

	uid = e_source_get_uid (source);

	for (i = 0; i < G_N_ELEMENTS (assigned_colors); i++) {
		GList *l;

		if (assigned_colors[i].uids == NULL) {
			first_empty = i;
			continue;
		}

		for (l = assigned_colors[i].uids; l != NULL; l = l->next)
			if (g_strcmp0 (l->data, uid) == 0)
				return assigned_colors[i].color;
	}

	/* return the first unused color */
	assigned_colors[first_empty].uids = g_list_append (
		assigned_colors[first_empty].uids, g_strdup (uid));

	return assigned_colors[first_empty].color;
}

static gint
cal_model_column_count (ETableModel *etm)
{
	return E_CAL_MODEL_FIELD_LAST;
}

static gint
cal_model_row_count (ETableModel *etm)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	priv = model->priv;

	return priv->objects->len;
}

static void
cal_model_append_row (ETableModel *etm,
                      ETableModel *source,
                      gint row)
{
	ECalModelClass *model_class;
	ECalModelComponent *comp_data;
	ECalModel *model = (ECalModel *) etm;
	gchar *uid = NULL;
	GError *error = NULL;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_TABLE_MODEL (source));

	comp_data = g_object_new (E_TYPE_CAL_MODEL_COMPONENT, NULL);

	comp_data->client = e_cal_model_ref_default_client (model);

	if (comp_data->client == NULL) {
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

	e_cal_client_create_object_sync (
		comp_data->client, comp_data->icalcomp, &uid, NULL, &error);

	if (error != NULL) {
		g_warning (
			G_STRLOC ": Could not create the object! %s",
			error->message);

		/* FIXME: show error dialog */
		g_error_free (error);
	} else {
		if (uid)
			icalcomponent_set_uid (comp_data->icalcomp, uid);

		g_signal_emit (model, signals[ROW_APPENDED], 0);
	}

	g_free (uid);
	g_object_unref (comp_data);
}

static gpointer
cal_model_value_at (ETableModel *etm,
                    gint col,
                    gint row)
{
	ECalModelPrivate *priv;
	ECalModelComponent *comp_data;
	ECalModel *model = (ECalModel *) etm;
	ESourceRegistry *registry;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	registry = e_cal_model_get_registry (model);

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
		return (gpointer) get_datetime_from_utc (
			model, comp_data, ICAL_CREATED_PROPERTY,
			icalproperty_get_created, &comp_data->created);
	case E_CAL_MODEL_FIELD_LASTMODIFIED :
		return (gpointer) get_datetime_from_utc (
			model, comp_data, ICAL_LASTMODIFIED_PROPERTY,
			icalproperty_get_lastmodified, &comp_data->lastmodified);
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
		return GINT_TO_POINTER (
			icalcomponent_get_first_component (
				comp_data->icalcomp,
				ICAL_VALARM_COMPONENT) != NULL);
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
			else if (itip_organizer_is_user (registry, comp, comp_data->client))
				retval = 3;
			else {
				GSList *attendees = NULL, *sl;

				e_cal_component_get_attendee_list (comp, &attendees);
				for (sl = attendees; sl != NULL; sl = sl->next) {
					ECalComponentAttendee *ca = sl->data;
					const gchar *text;

					text = itip_strip_mailto (ca->value);
					if (itip_address_is_user (registry, text)) {
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
cal_model_set_value_at (ETableModel *etm,
                        gint col,
                        gint row,
                        gconstpointer value)
{
	ECalModelPrivate *priv;
	ECalModelComponent *comp_data;
	ECalModel *model = (ECalModel *) etm;
	GError *error = NULL;

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
	e_cal_client_modify_object_sync (
		comp_data->client, comp_data->icalcomp,
		CALOBJ_MOD_ALL, NULL, &error);

	if (error != NULL) {
		g_warning (
			G_STRLOC ": Could not modify the object! %s",
			error->message);

		/* FIXME Show error dialog */
		g_error_free (error);
	}
}

static gboolean
cal_model_is_cell_editable (ETableModel *etm,
                            gint col,
                            gint row)
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

static gpointer
cal_model_duplicate_value (ETableModel *etm,
                           gint col,
                           gconstpointer value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, NULL);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return g_strdup (value);
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
		return (gpointer) value;
	case E_CAL_MODEL_FIELD_COMPONENT :
		return icalcomponent_new_clone ((icalcomponent *) value);
	case E_CAL_MODEL_FIELD_DTSTART :
	case E_CAL_MODEL_FIELD_CREATED :
	case E_CAL_MODEL_FIELD_LASTMODIFIED :
		return e_cal_model_copy_cell_date_value (value);
	}

	return NULL;
}

static void
cal_model_free_value (ETableModel *etm,
                      gint col,
                      gpointer value)
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
	case E_CAL_MODEL_FIELD_DTSTART:
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
cal_model_initialize_value (ETableModel *etm,
                            gint col)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, NULL);

	priv = model->priv;

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
		return g_strdup (priv->default_category ? priv->default_category:"");
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return g_strdup ("");
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
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
cal_model_value_is_empty (ETableModel *etm,
                          gint col,
                          gconstpointer value)
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
cal_model_value_to_string (ETableModel *etm,
                           gint col,
                           gconstpointer value)
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

static void
e_cal_model_class_init (ECalModelClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ECalModelPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = cal_model_set_property;
	object_class->get_property = cal_model_get_property;
	object_class->constructed = cal_model_constructed;
	object_class->dispose = cal_model_dispose;
	object_class->finalize = cal_model_finalize;

	class->get_color_for_component = cal_model_get_color_for_component;
	class->fill_component_from_model = NULL;

	g_object_class_install_property (
		object_class,
		PROP_COMPRESS_WEEKEND,
		g_param_spec_boolean (
			"compress-weekend",
			"Compress Weekend",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CONFIRM_DELETE,
		g_param_spec_boolean (
			"confirm-delete",
			"Confirm Delete",
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_CLIENT,
		g_param_spec_object (
			"default-client",
			"Default ECalClient",
			NULL,
			E_TYPE_CAL_CLIENT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_REMINDER_INTERVAL,
		g_param_spec_int (
			"default-reminder-interval",
			"Default Reminder Interval",
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_REMINDER_UNITS,
		g_param_spec_enum (
			"default-reminder-units",
			"Default Reminder Units",
			NULL,
			E_TYPE_DURATION_TYPE,
			E_DURATION_MINUTES,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Data source registry",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

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
		PROP_USE_DEFAULT_REMINDER,
		g_param_spec_boolean (
			"use-default-reminder",
			"Use Default Reminder",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WEEK_START_DAY,
		g_param_spec_enum (
			"week-start-day",
			"Week Start Day",
			NULL,
			E_TYPE_DATE_WEEKDAY,
			G_DATE_MONDAY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_MONDAY,
		g_param_spec_boolean (
			"work-day-monday",
			"Work Day: Monday",
			"Whether Monday is a work day",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_TUESDAY,
		g_param_spec_boolean (
			"work-day-tuesday",
			"Work Day: Tuesday",
			"Whether Tuesday is a work day",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_WEDNESDAY,
		g_param_spec_boolean (
			"work-day-wednesday",
			"Work Day: Wednesday",
			"Whether Wednesday is a work day",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_THURSDAY,
		g_param_spec_boolean (
			"work-day-thursday",
			"Work Day: Thursday",
			"Whether Thursday is a work day",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_FRIDAY,
		g_param_spec_boolean (
			"work-day-friday",
			"Work Day: Friday",
			"Whether Friday is a work day",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_SATURDAY,
		g_param_spec_boolean (
			"work-day-saturday",
			"Work Day: Saturday",
			"Whether Saturday is a work day",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_SUNDAY,
		g_param_spec_boolean (
			"work-day-sunday",
			"Work Day: Sunday",
			"Whether Sunday is a work day",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_HOUR,
		g_param_spec_int (
			"work-day-end-hour",
			"Work Day End Hour",
			NULL,
			0,
			23,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_MINUTE,
		g_param_spec_int (
			"work-day-end-minute",
			"Work Day End Minute",
			NULL,
			0,
			59,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_HOUR,
		g_param_spec_int (
			"work-day-start-hour",
			"Work Day Start Hour",
			NULL,
			0,
			23,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_MINUTE,
		g_param_spec_int (
			"work-day-start-minute",
			"Work Day Start Minute",
			NULL,
			0,
			59,
			0,
			G_PARAM_READWRITE));

	signals[TIME_RANGE_CHANGED] = g_signal_new (
		"time_range_changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalModelClass, time_range_changed),
		NULL, NULL,
		e_marshal_VOID__LONG_LONG,
		G_TYPE_NONE, 2,
		G_TYPE_LONG,
		G_TYPE_LONG);

	signals[ROW_APPENDED] = g_signal_new (
		"row_appended",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalModelClass, row_appended),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[COMPS_DELETED] = g_signal_new (
		"comps_deleted",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalModelClass, comps_deleted),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	signals[CAL_VIEW_PROGRESS] = g_signal_new (
		"cal_view_progress",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalModelClass, cal_view_progress),
		NULL, NULL,
		e_marshal_VOID__STRING_INT_INT,
		G_TYPE_NONE, 3,
		G_TYPE_STRING,
		G_TYPE_INT,
		G_TYPE_INT);

	signals[CAL_VIEW_COMPLETE] = g_signal_new (
		"cal_view_complete",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalModelClass, cal_view_complete),
		NULL, NULL,
		e_marshal_VOID__BOXED_INT,
		G_TYPE_NONE, 2,
		G_TYPE_ERROR,
		G_TYPE_INT);

	signals[STATUS_MESSAGE] = g_signal_new (
		"status-message",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ECalModelClass, status_message),
		NULL, NULL,
		e_marshal_VOID__STRING_DOUBLE,
		G_TYPE_NONE, 2,
		G_TYPE_STRING,
		G_TYPE_DOUBLE);

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
e_cal_model_table_model_init (ETableModelInterface *iface)
{
	iface->column_count = cal_model_column_count;
	iface->row_count = cal_model_row_count;
	iface->append_row = cal_model_append_row;

	iface->value_at = cal_model_value_at;
	iface->set_value_at = cal_model_set_value_at;
	iface->is_cell_editable = cal_model_is_cell_editable;

	iface->duplicate_value = cal_model_duplicate_value;
	iface->free_value = cal_model_free_value;
	iface->initialize_value = cal_model_initialize_value;
	iface->value_is_empty = cal_model_value_is_empty;
	iface->value_to_string = cal_model_value_to_string;
}

static void
e_cal_model_init (ECalModel *model)
{
	model->priv = E_CAL_MODEL_GET_PRIVATE (model);

	g_mutex_init (&model->priv->clients_lock);

	/* match none by default */
	model->priv->start = -1;
	model->priv->end = -1;
	model->priv->search_sexp = NULL;
	model->priv->full_sexp = g_strdup ("#f");

	model->priv->objects = g_ptr_array_new ();
	model->priv->kind = ICAL_NO_COMPONENT;
	model->priv->flags = 0;

	model->priv->use_24_hour_format = TRUE;

	model->priv->in_added = FALSE;
	model->priv->in_modified = FALSE;
	model->priv->in_removed = FALSE;
	model->priv->notify_added = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);
	model->priv->notify_modified = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);
	model->priv->notify_removed = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);
	g_mutex_init (&model->priv->notify_lock);

	model->priv->loading_clients = g_cancellable_new ();
}

/* updates time in a component, and keeps the timezone used in it, if exists */
void
e_cal_model_update_comp_time (ECalModel *model,
                              ECalModelComponent *comp_data,
                              gconstpointer time_value,
                              icalproperty_kind kind,
                              void (*set_func) (icalproperty *prop,
                                                struct icaltimetype v),
                              icalproperty * (*new_func) (struct icaltimetype v))
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
	 * we remove it if it exists. */
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
			icalparameter_set_tzid (param, (gchar *) tzid);
		} else {
			icalproperty_remove_parameter (prop, ICAL_TZID_PARAMETER);
		}
	}
}

/**
 * e_cal_model_test_row_editable
 * @model: an #ECalModel
 * @row: Row of our interest. -1 is editable only when default client is
 * editable.
 *
 * Checks if component at @row is editable or not.  It doesn't check bounds
 * for @row.
 *
 * Returns: Whether @row is editable or not.
 **/
gboolean
e_cal_model_test_row_editable (ECalModel *model,
                               gint row)
{
	gboolean readonly;
	ECalClient *client = NULL;

	if (row != -1) {
		ECalModelComponent *comp_data;

		comp_data = e_cal_model_get_component_at (model, row);

		if (comp_data != NULL && comp_data->client != NULL)
			client = g_object_ref (comp_data->client);

	} else {
		client = e_cal_model_ref_default_client (model);
	}

	readonly = (client == NULL);

	if (!readonly)
		readonly = e_client_is_readonly (E_CLIENT (client));

	g_clear_object (&client);

	return !readonly;
}

gboolean
e_cal_model_get_confirm_delete (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);

	return model->priv->confirm_delete;
}

void
e_cal_model_set_confirm_delete (ECalModel *model,
                                gboolean confirm_delete)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->confirm_delete == confirm_delete)
		return;

	model->priv->confirm_delete = confirm_delete;

	g_object_notify (G_OBJECT (model), "confirm-delete");
}

icalcomponent_kind
e_cal_model_get_component_kind (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), ICAL_NO_COMPONENT);

	return model->priv->kind;
}

void
e_cal_model_set_component_kind (ECalModel *model,
                                icalcomponent_kind kind)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	model->priv->kind = kind;
}

ECalModelFlags
e_cal_model_get_flags (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), 0);

	return model->priv->flags;
}

void
e_cal_model_set_flags (ECalModel *model,
                       ECalModelFlags flags)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	model->priv->flags = flags;
}

ESourceRegistry *
e_cal_model_get_registry (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return model->priv->registry;
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
	 * so we need to redisplay everything */
	e_table_model_changed (E_TABLE_MODEL (model));
	redo_queries (model);

	g_object_notify (G_OBJECT (model), "timezone");
	g_signal_emit (
		model, signals[TIMEZONE_CHANGED], 0,
		old_zone, zone);
}

gboolean
e_cal_model_get_compress_weekend (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);

	return model->priv->compress_weekend;
}

void
e_cal_model_set_compress_weekend (ECalModel *model,
                                  gboolean compress_weekend)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->compress_weekend == compress_weekend)
		return;

	model->priv->compress_weekend = compress_weekend;

	g_object_notify (G_OBJECT (model), "compress-weekend");
}

void
e_cal_model_set_default_category (ECalModel *model,
                                  const gchar *default_category)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	g_free (model->priv->default_category);
	model->priv->default_category = g_strdup (default_category);
}

gint
e_cal_model_get_default_reminder_interval (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), 0);

	return model->priv->default_reminder_interval;
}

void
e_cal_model_set_default_reminder_interval (ECalModel *model,
                                           gint default_reminder_interval)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->default_reminder_interval == default_reminder_interval)
		return;

	model->priv->default_reminder_interval = default_reminder_interval;

	g_object_notify (G_OBJECT (model), "default-reminder-interval");
}

EDurationType
e_cal_model_get_default_reminder_units (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), 0);

	return model->priv->default_reminder_units;
}

void
e_cal_model_set_default_reminder_units (ECalModel *model,
                                        EDurationType default_reminder_units)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->default_reminder_units == default_reminder_units)
		return;

	model->priv->default_reminder_units = default_reminder_units;

	g_object_notify (G_OBJECT (model), "default-reminder-units");
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

gboolean
e_cal_model_get_use_default_reminder (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);

	return model->priv->use_default_reminder;
}

void
e_cal_model_set_use_default_reminder (ECalModel *model,
                                      gboolean use_default_reminder)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->use_default_reminder == use_default_reminder)
		return;

	model->priv->use_default_reminder = use_default_reminder;

	g_object_notify (G_OBJECT (model), "use-default-reminder");
}

GDateWeekday
e_cal_model_get_week_start_day (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), G_DATE_BAD_WEEKDAY);

	return model->priv->week_start_day;
}

void
e_cal_model_set_week_start_day (ECalModel *model,
                                GDateWeekday week_start_day)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (g_date_valid_weekday (week_start_day));

	if (model->priv->week_start_day == week_start_day)
		return;

	model->priv->week_start_day = week_start_day;

	g_object_notify (G_OBJECT (model), "week-start-day");
}

gboolean
e_cal_model_get_work_day (ECalModel *model,
                          GDateWeekday weekday)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);
	g_return_val_if_fail (g_date_valid_weekday (weekday), FALSE);

	return model->priv->work_days[weekday];
}

void
e_cal_model_set_work_day (ECalModel *model,
                          GDateWeekday weekday,
                          gboolean work_day)
{
	const gchar *property_name = NULL;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (g_date_valid_weekday (weekday));

	if (work_day == model->priv->work_days[weekday])
		return;

	model->priv->work_days[weekday] = work_day;

	switch (weekday) {
		case G_DATE_MONDAY:
			property_name = "work-day-monday";
			break;
		case G_DATE_TUESDAY:
			property_name = "work-day-tuesday";
			break;
		case G_DATE_WEDNESDAY:
			property_name = "work-day-wednesday";
			break;
		case G_DATE_THURSDAY:
			property_name = "work-day-thursday";
			break;
		case G_DATE_FRIDAY:
			property_name = "work-day-friday";
			break;
		case G_DATE_SATURDAY:
			property_name = "work-day-saturday";
			break;
		case G_DATE_SUNDAY:
			property_name = "work-day-sunday";
			break;
		default:
			g_warn_if_reached ();
	}

	g_object_notify (G_OBJECT (model), property_name);
}

/**
 * e_cal_model_get_work_day_first:
 * @model: an #ECalModel
 *
 * Returns the first work day with respect to #ECalModel:work-week-start.
 * If no work days are set, the function returns %G_DATE_BAD_WEEKDAY.
 *
 * Returns: first work day of the week, or %G_DATE_BAD_WEEKDAY
 **/
GDateWeekday
e_cal_model_get_work_day_first (ECalModel *model)
{
	GDateWeekday weekday;
	gint ii;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), G_DATE_BAD_WEEKDAY);

	weekday = e_cal_model_get_week_start_day (model);

	for (ii = 0; ii < 7; ii++) {
		if (e_cal_model_get_work_day (model, weekday))
			return weekday;
		weekday = e_weekday_get_next (weekday);
	}

	return G_DATE_BAD_WEEKDAY;
}

/**
 * e_cal_model_get_work_day_last:
 * @model: an #ECalModel
 *
 * Returns the last work day with respect to #ECalModel:work-week-start.
 * If no work days are set, the function returns %G_DATE_BAD_WEEKDAY.
 *
 * Returns: last work day of the week, or %G_DATE_BAD_WEEKDAY
 **/
GDateWeekday
e_cal_model_get_work_day_last (ECalModel *model)
{
	GDateWeekday weekday;
	gint ii;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), G_DATE_BAD_WEEKDAY);

	weekday = e_cal_model_get_week_start_day (model);

	for (ii = 0; ii < 7; ii++) {
		weekday = e_weekday_get_prev (weekday);
		if (e_cal_model_get_work_day (model, weekday))
			return weekday;
	}

	return G_DATE_BAD_WEEKDAY;
}

gint
e_cal_model_get_work_day_end_hour (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), 0);

	return model->priv->work_day_end_hour;
}

void
e_cal_model_set_work_day_end_hour (ECalModel *model,
                                   gint work_day_end_hour)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_end_hour == work_day_end_hour)
		return;

	model->priv->work_day_end_hour = work_day_end_hour;

	g_object_notify (G_OBJECT (model), "work-day-end-hour");
}

gint
e_cal_model_get_work_day_end_minute (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), 0);

	return model->priv->work_day_end_minute;
}

void
e_cal_model_set_work_day_end_minute (ECalModel *model,
                                   gint work_day_end_minute)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_end_minute == work_day_end_minute)
		return;

	model->priv->work_day_end_minute = work_day_end_minute;

	g_object_notify (G_OBJECT (model), "work-day-end-minute");
}

gint
e_cal_model_get_work_day_start_hour (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), 0);

	return model->priv->work_day_start_hour;
}

void
e_cal_model_set_work_day_start_hour (ECalModel *model,
                                   gint work_day_start_hour)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_start_hour == work_day_start_hour)
		return;

	model->priv->work_day_start_hour = work_day_start_hour;

	g_object_notify (G_OBJECT (model), "work-day-start-hour");
}

gint
e_cal_model_get_work_day_start_minute (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), 0);

	return model->priv->work_day_start_minute;
}

void
e_cal_model_set_work_day_start_minute (ECalModel *model,
                                   gint work_day_start_minute)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_start_minute == work_day_start_minute)
		return;

	model->priv->work_day_start_minute = work_day_start_minute;

	g_object_notify (G_OBJECT (model), "work-day-start-minute");
}

ECalClient *
e_cal_model_ref_default_client (ECalModel *model)
{
	ClientData *client_data;
	ECalClient *default_client = NULL;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	if (model->priv->default_client != NULL)
		return g_object_ref (model->priv->default_client);

	client_data = cal_model_clients_peek (model);
	if (client_data != NULL) {
		default_client = g_object_ref (client_data->client);
		client_data_unref (client_data);
	}

	return default_client;
}

void
e_cal_model_set_default_client (ECalModel *model,
                                ECalClient *client)
{
	ECalModelPrivate *priv;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (client != NULL)
		g_return_if_fail (E_IS_CAL_CLIENT (client));

	priv = model->priv;

	if (priv->default_client == client)
		return;

	if (priv->default_client == NULL) {
		ClientData *client_data;

		client_data = cal_model_clients_lookup (
			model, priv->default_client);
		if (client_data != NULL) {
			if (!client_data->do_query)
				remove_client (model, client_data);
			client_data_unref (client_data);
		}
	}

	if (client != NULL) {
		/* Make sure its in the model */
		add_new_client (model, client, FALSE);

		/* Store the default client */
		priv->default_client = client;
	} else {
		priv->default_client = NULL;
	}

	g_object_notify (G_OBJECT (model), "default-client");
}

GList *
e_cal_model_list_clients (ECalModel *model)
{
	GQueue results = G_QUEUE_INIT;
	GList *list, *link;
	ECalClient *default_client;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	default_client = model->priv->default_client;

	list = cal_model_clients_list (model);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ClientData *client_data = link->data;
		ECalClient *client;

		client = client_data->client;

		/* Exclude the default client if we're not querying it. */
		if (client == default_client) {
			if (!client_data->do_query)
				continue;
		}

		g_queue_push_tail (&results, g_object_ref (client));
	}

	g_list_free_full (list, (GDestroyNotify) client_data_unref);

	return g_queue_peek_head_link (&results);
}

static ECalModelComponent *
search_by_id_and_client (ECalModelPrivate *priv,
                         ECalClient *client,
                         const ECalComponentId *id)
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

static void
remove_all_for_id_and_client (ECalModel *model,
                              ECalClient *client,
                              const ECalComponentId *id)
{
	ECalModelComponent *comp_data;

	while ((comp_data = search_by_id_and_client (model->priv, client, id))) {
		gint pos;
		GSList *list = NULL;

		pos = get_position_in_array (model->priv->objects, comp_data);

		if (!g_ptr_array_remove (model->priv->objects, comp_data))
			continue;

		list = g_slist_append (list, comp_data);
		g_signal_emit (model, signals[COMPS_DELETED], 0, list);

		g_slist_free (list);
		g_object_unref (comp_data);

		e_table_model_pre_change (E_TABLE_MODEL (model));
		e_table_model_row_deleted (E_TABLE_MODEL (model), pos);
	}
}

typedef struct {
	ECalClient *client;
	ECalClientView *view;
	ECalModel *model;
	icalcomponent *icalcomp;
} RecurrenceExpansionData;

static void
free_rdata (gpointer data)
{
	RecurrenceExpansionData *rdata = data;

	if (!rdata)
		return;

	g_object_unref (rdata->client);
	g_object_unref (rdata->view);
	g_object_unref (rdata->model);
	g_free (rdata);
}

static gboolean
add_instance_cb (ECalComponent *comp,
                 time_t instance_start,
                 time_t instance_end,
                 gpointer user_data)
{
	ECalModelComponent *comp_data;
	ECalModelPrivate *priv;
	RecurrenceExpansionData *rdata = user_data;
	icaltimetype time;
	ECalComponentDateTime datetime, to_set;
	icaltimezone *zone = NULL;
	ECalComponentId *id;

	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), TRUE);

	priv = rdata->model->priv;

	id = e_cal_component_get_id (comp);
	remove_all_for_id_and_client (rdata->model, rdata->client, id);
	e_cal_component_free_id (id);

	e_table_model_pre_change (E_TABLE_MODEL (rdata->model));

	/* set the right instance start date to component */
	e_cal_component_get_dtstart (comp, &datetime);
	if (datetime.tzid)
		e_cal_client_get_timezone_sync (rdata->client, datetime.tzid, &zone, NULL, NULL);
	time = icaltime_from_timet_with_zone (instance_start, FALSE, zone ? zone : priv->zone);
	to_set.value = &time;
	to_set.tzid = datetime.tzid;
	e_cal_component_set_dtstart (comp, &to_set);
	e_cal_component_free_datetime (&datetime);

	/* set the right instance end date to component*/
	e_cal_component_get_dtend (comp, &datetime);
	zone = NULL;
	if (datetime.tzid)
		e_cal_client_get_timezone_sync (rdata->client, datetime.tzid, &zone, NULL, NULL);
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
place_master_object_first_cb (gconstpointer p1,
                              gconstpointer p2)
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

static void
process_event (ECalClientView *view,
               const GSList *objects,
               ECalModel *model,
               void (*process_fn) (ECalClientView *view,
                                   const GSList *objects,
                                   ECalModel *model),
               gboolean *in,
               GHashTable *save_hash,
               gpointer (*copy_fn) (gpointer data),
               void (*free_fn) (gpointer data))
{
	gboolean skip = FALSE;
	const GSList *l;

	g_return_if_fail (save_hash != NULL);

	g_mutex_lock (&model->priv->notify_lock);
	if (*in) {
		GSList *save_list = g_hash_table_lookup (save_hash, view);

		skip = TRUE;
		for (l = objects; l; l = l->next) {
			if (l->data)
				save_list = g_slist_append (save_list, copy_fn (l->data));
		}

		g_hash_table_insert (save_hash, g_object_ref (view), save_list);
	} else {
		*in = TRUE;
	}

	g_mutex_unlock (&model->priv->notify_lock);

	if (skip)
		return;

	/* do it */
	process_fn (view, objects, model);

	g_mutex_lock (&model->priv->notify_lock);
	while (g_hash_table_size (save_hash)) {
		gpointer key = NULL, value = NULL;
		GHashTableIter iter;
		GSList *save_list;

		g_hash_table_iter_init (&iter, save_hash);
		if (!g_hash_table_iter_next (&iter, &key, &value)) {
			g_debug ("%s: Failed to get first item of the save_hash", G_STRFUNC);
			break;
		}

		save_list = value;
		view = g_object_ref (key);

		g_hash_table_remove (save_hash, view);

		g_mutex_unlock (&model->priv->notify_lock);

		/* do it */
		process_fn (view, save_list, model);

		for (l = save_list; l; l = l->next) {
			if (l->data) {
				free_fn (l->data);
			}
		}
		g_slist_free (save_list);
		g_object_unref (view);

		g_mutex_lock (&model->priv->notify_lock);
	}

	*in = FALSE;
	g_mutex_unlock (&model->priv->notify_lock);
}

static void
process_added (ECalClientView *view,
               const GSList *objects,
               ECalModel *model)
{
	ECalModelPrivate *priv;
	const GSList *l;
	GSList *copy;

	priv = model->priv;

	/* order matters, process master object first, then detached instances */
	copy = g_slist_sort (g_slist_copy ((GSList *) objects), place_master_object_first_cb);

	for (l = copy; l; l = l->next) {
		ECalModelComponent *comp_data;
		ECalComponentId *id;
		ECalComponent *comp = e_cal_component_new ();
		ECalClient *client = e_cal_client_view_get_client (view);

		/* This will fail for alarm or VCalendar component */
		if (!e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (l->data))) {
			g_object_unref (comp);
			continue;
		}

		id = e_cal_component_get_id (comp);

		/* remove the components if they are already present and re-add them */
		remove_all_for_id_and_client (model, client, id);

		e_cal_component_free_id (id);
		g_object_unref (comp);
		ensure_dates_are_in_default_zone (model, l->data);

		if (e_cal_util_component_has_recurrences (l->data) && (priv->flags & E_CAL_MODEL_FLAGS_EXPAND_RECURRENCES)) {
			ClientData *client_data;

			client_data = cal_model_clients_lookup (model, client);

			if (client_data != NULL) {
				RecurrenceExpansionData *rdata = g_new0 (RecurrenceExpansionData, 1);
				rdata->client = g_object_ref (client);
				rdata->view = g_object_ref (view);
				rdata->model = g_object_ref (model);

				e_cal_client_generate_instances_for_object (rdata->client, l->data, priv->start, priv->end, client_data->cancellable,
									    (ECalRecurInstanceFn) add_instance_cb, rdata, free_rdata);

				client_data_unref (client_data);
			}
		} else {
			e_table_model_pre_change (E_TABLE_MODEL (model));

			comp_data = g_object_new (E_TYPE_CAL_MODEL_COMPONENT, NULL);
			comp_data->client = g_object_ref (client);
			comp_data->icalcomp = icalcomponent_new_clone (l->data);
			e_cal_model_set_instance_times (comp_data, priv->zone);

			g_ptr_array_add (priv->objects, comp_data);
			e_table_model_row_inserted (E_TABLE_MODEL (model), priv->objects->len - 1);
		}
	}

	g_slist_free (copy);
}

static void
process_modified (ECalClientView *view,
                  const GSList *objects,
                  ECalModel *model)
{
	ECalModelPrivate *priv;
	const GSList *l;
	GSList *list = NULL;

	priv = model->priv;

	/*  re-add only the recurrence objects */
	for (l = objects; l != NULL; l = g_slist_next (l)) {
		if (!e_cal_util_component_is_instance (l->data) && e_cal_util_component_has_recurrences (l->data) && (priv->flags & E_CAL_MODEL_FLAGS_EXPAND_RECURRENCES))
			list = g_slist_prepend (list, l->data);
		else {
			gint pos;
			ECalModelComponent *comp_data;
			ECalComponentId *id;
			ECalComponent *comp = e_cal_component_new ();
			ECalClient *client = e_cal_client_view_get_client (view);

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

	process_event (
		view, list, model, process_added,
		&model->priv->in_added,
		model->priv->notify_added,
		(gpointer (*)(gpointer)) icalcomponent_new_clone,
		(void (*)(gpointer)) icalcomponent_free);

	g_slist_free (list);
}

static void
process_removed (ECalClientView *view,
                 const GSList *ids,
                 ECalModel *model)
{
	ECalModelPrivate *priv;
	const GSList *l;

	priv = model->priv;

	for (l = ids; l; l = l->next) {
		ECalModelComponent *comp_data = NULL;
		ECalComponentId *id = l->data;
		gint pos;

		/* make sure we remove all objects with this UID */
		while ((comp_data = search_by_id_and_client (priv, e_cal_client_view_get_client (view), id))) {
			GSList *l = NULL;

			pos = get_position_in_array (priv->objects, comp_data);

			if (!g_ptr_array_remove (priv->objects, comp_data))
				continue;

			l = g_slist_append (l, comp_data);
			g_signal_emit (model, signals[COMPS_DELETED], 0, l);

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
client_view_objects_added_cb (ECalClientView *view,
                              const GSList *objects,
                              GWeakRef *weak_ref_model)
{
	ECalModel *model;

	model = g_weak_ref_get (weak_ref_model);

	if (model != NULL) {
		process_event (
			view, objects, model, process_added,
			&model->priv->in_added,
			model->priv->notify_added,
			(gpointer (*)(gpointer)) icalcomponent_new_clone,
			(void (*)(gpointer)) icalcomponent_free);
		g_object_unref (model);
	}
}

static void
client_view_objects_modified_cb (ECalClientView *view,
                                 const GSList *objects,
                                 GWeakRef *weak_ref_model)
{
	ECalModel *model;

	model = g_weak_ref_get (weak_ref_model);

	if (model != NULL) {
		process_event (
			view, objects, model, process_modified,
			&model->priv->in_modified,
			model->priv->notify_modified,
			(gpointer (*)(gpointer)) icalcomponent_new_clone,
			(void (*)(gpointer)) icalcomponent_free);
		g_object_unref (model);
	}
}

static void
client_view_objects_removed_cb (ECalClientView *view,
                                const GSList *ids,
                                GWeakRef *weak_ref_model)
{
	ECalModel *model;

	model = g_weak_ref_get (weak_ref_model);

	if (model != NULL) {
		process_event (
			view, ids, model, process_removed,
			&model->priv->in_removed,
			model->priv->notify_removed,
			copy_comp_id, free_comp_id);
		g_object_unref (model);
	}
}

static void
client_view_progress_cb (ECalClientView *view,
                         gint percent,
                         const gchar *message,
                         GWeakRef *weak_ref_model)
{
	ECalModel *model;

	model = g_weak_ref_get (weak_ref_model);

	if (model != NULL) {
		ECalClient *client;
		ECalClientSourceType source_type;

		client = e_cal_client_view_get_client (view);
		source_type = e_cal_client_get_source_type (client);

		g_signal_emit (
			model, signals[CAL_VIEW_PROGRESS], 0,
			message, percent, source_type);

		g_object_unref (model);
	}
}

static void
client_view_complete_cb (ECalClientView *view,
                         const GError *error,
                         GWeakRef *weak_ref_model)
{
	ECalModel *model;

	model = g_weak_ref_get (weak_ref_model);

	if (model != NULL) {
		ECalClient *client;
		ECalClientSourceType source_type;

		client = e_cal_client_view_get_client (view);
		source_type = e_cal_client_get_source_type (client);

		g_signal_emit (
			model, signals[CAL_VIEW_COMPLETE], 0,
			error, source_type);

		g_object_unref (model);
	}
}

static void
cal_model_get_view_cb (GObject *source_object,
                       GAsyncResult *result,
                       gpointer user_data)
{
	ClientData *client_data = user_data;
	ECalClientView *view = NULL;
	ECalModel *model = NULL;
	GError *error = NULL;

	e_cal_client_get_view_finish (
		E_CAL_CLIENT (source_object), result, &view, &error);

	/* Sanity check. */
	g_return_if_fail (
		((view != NULL) && (error == NULL)) ||
		((view == NULL) && (error != NULL)));

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		goto exit;
	}

	model = g_weak_ref_get (&client_data->model);

	if (view != NULL && model != NULL) {
		gulong handler_id;

		g_mutex_lock (&client_data->view_lock);

		client_data->view = g_object_ref (view);

		handler_id = g_signal_connect_data (
			view, "objects-added",
			G_CALLBACK (client_view_objects_added_cb),
			e_weak_ref_new (model),
			(GClosureNotify) e_weak_ref_free, 0);
		client_data->objects_added_handler_id = handler_id;

		handler_id = g_signal_connect_data (
			view, "objects-modified",
			G_CALLBACK (client_view_objects_modified_cb),
			e_weak_ref_new (model),
			(GClosureNotify) e_weak_ref_free, 0);
		client_data->objects_modified_handler_id = handler_id;

		handler_id = g_signal_connect_data (
			view, "objects-removed",
			G_CALLBACK (client_view_objects_removed_cb),
			e_weak_ref_new (model),
			(GClosureNotify) e_weak_ref_free, 0);
		client_data->objects_removed_handler_id = handler_id;

		handler_id = g_signal_connect_data (
			view, "progress",
			G_CALLBACK (client_view_progress_cb),
			e_weak_ref_new (model),
			(GClosureNotify) e_weak_ref_free, 0);
		client_data->progress_handler_id = handler_id;

		handler_id = g_signal_connect_data (
			view, "complete",
			G_CALLBACK (client_view_complete_cb),
			e_weak_ref_new (model),
			(GClosureNotify) e_weak_ref_free, 0);
		client_data->complete_handler_id = handler_id;

		g_mutex_unlock (&client_data->view_lock);

		e_cal_client_view_start (view, &error);

		if (error != NULL) {
			g_warning (
				"%s: Failed to start view: %s",
				G_STRFUNC, error->message);
			g_error_free (error);
		}

	} else if (error != NULL) {
		g_warning (
			"%s: Failed to get view: %s",
			G_STRFUNC, error->message);
		g_error_free (error);
	}

exit:
	g_clear_object (&model);
	g_clear_object (&view);

	client_data_unref (client_data);
}

static void
update_e_cal_view_for_client (ECalModel *model,
                              ClientData *client_data)
{
	ECalModelPrivate *priv;
	GCancellable *cancellable;

	priv = model->priv;

	g_return_if_fail (model->priv->full_sexp != NULL);

	/* free the previous view, if any */
	g_mutex_lock (&client_data->view_lock);
	if (client_data->view != NULL) {
		client_data_disconnect_view_handlers (client_data);
		g_clear_object (&client_data->view);
	}
	g_mutex_unlock (&client_data->view_lock);

	/* Don't create the new query if we won't use it */
	if (!client_data->do_query)
		return;

	/* prepare the view */

	cancellable = g_cancellable_new ();

	g_mutex_lock (&client_data->view_lock);

	if (client_data->cancellable != NULL) {
		g_cancellable_cancel (client_data->cancellable);
		g_clear_object (&client_data->cancellable);
	}

	client_data->cancellable = g_object_ref (cancellable);

	g_mutex_unlock (&client_data->view_lock);

	e_cal_client_get_view (
		client_data->client, priv->full_sexp,
		cancellable, cal_model_get_view_cb,
		client_data_ref (client_data));

	g_object_unref (cancellable);
}

void
e_cal_model_update_status_message (ECalModel *model,
                                   const gchar *message,
                                   gdouble percent)
{
	g_return_if_fail (model != NULL);

	g_signal_emit (model, signals[STATUS_MESSAGE], 0, message, percent);
}

static gboolean
add_new_client (ECalModel *model,
                ECalClient *client,
                gboolean do_query)
{
	ClientData *client_data;
	gboolean update_view = TRUE;

	/* Look to see if we already have this client */
	client_data = cal_model_clients_lookup (model, client);
	if (client_data != NULL) {
		if (client_data->do_query)
			update_view = FALSE;
		else
			client_data->do_query = do_query;

	} else {
		client_data = client_data_new (model, client, do_query);

		g_mutex_lock (&model->priv->clients_lock);
		g_queue_push_tail (
			&model->priv->clients,
			client_data_ref (client_data));
		g_mutex_unlock (&model->priv->clients_lock);
	}

	if (update_view)
		update_e_cal_view_for_client (model, client_data);

	client_data_unref (client_data);

	return update_view;
}

/**
 * e_cal_model_add_client:
 * @model: an #ECalModel
 * @client: an #ECalClient
 *
 * Adds @client to @model and creates an internal #ECalClientView for it.
 *
 * If @model already has @client from a previous e_cal_model_add_client()
 * call (in other words, excluding e_cal_model_set_default_client()), then
 * the function does nothing and returns %FALSE.
 *
 * Returns: %TRUE if @client was added, %FALSE if @model already had it
 */
gboolean
e_cal_model_add_client (ECalModel *model,
                        ECalClient *client)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), FALSE);

	return add_new_client (model, client, TRUE);
}

static void
remove_client_objects (ECalModel *model,
                       ClientData *client_data)
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
			g_signal_emit (model, signals[COMPS_DELETED], 0, l);

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
remove_client (ECalModel *model,
               ClientData *client_data)
{
	g_mutex_lock (&client_data->view_lock);
	if (client_data->view != NULL)
		client_data_disconnect_view_handlers (client_data);
	g_mutex_unlock (&client_data->view_lock);

	remove_client_objects (model, client_data);

	/* If this is the default client and we were querying (so it
	 * was also a source), keep it around but don't query it */
	if (model->priv->default_client == client_data->client && client_data->do_query) {
		client_data->do_query = FALSE;

		return;
	}

	if (model->priv->default_client == client_data->client)
		model->priv->default_client = NULL;

	cal_model_clients_remove (model, client_data);
}

/**
 * e_cal_model_remove_client
 * @model: an #ECalModel
 * @client: an #ECalClient
 *
 * Removes @client from @model along with its internal #ECalClientView.
 *
 * If @model does not have @client then the function does nothing and
 * returns %FALSE.
 *
 * Returns: %TRUE is @client was remove, %FALSE if @model did not have it
 */
gboolean
e_cal_model_remove_client (ECalModel *model,
                           ECalClient *client)
{
	ClientData *client_data;
	gboolean removed = FALSE;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), FALSE);

	client_data = cal_model_clients_lookup (model, client);
	if (client_data != NULL) {
		remove_client (model, client_data);
		client_data_unref (client_data);
		removed = TRUE;
	}

	return removed;
}

/**
 * e_cal_model_remove_all_clients
 */
void
e_cal_model_remove_all_clients (ECalModel *model)
{
	ClientData *client_data;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	while ((client_data = cal_model_clients_pop (model)) != NULL) {
		remove_client (model, client_data);
		client_data_unref (client_data);
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
	g_signal_emit (model, signals[COMPS_DELETED], 0, slist);

	e_table_model_rows_deleted (E_TABLE_MODEL (model), 0, len);

	g_slist_foreach (slist, (GFunc) g_object_unref, NULL);
	g_slist_free (slist);

	e_flag_set (data->eflag);

	return FALSE;
}

static void
redo_queries (ECalModel *model)
{
	ECalModelPrivate *priv;
	GList *list, *link;
	struct cc_data data;

	priv = model->priv;

	if (priv->full_sexp)
		g_free (priv->full_sexp);

	if (priv->start != -1 && priv->end != -1) {
		gchar *iso_start, *iso_end;
		const gchar *default_tzloc = NULL;

		iso_start = isodate_from_time_t (priv->start);
		iso_end = isodate_from_time_t (priv->end);

		if (priv->zone && priv->zone != icaltimezone_get_utc_timezone ())
			default_tzloc = icaltimezone_get_location (priv->zone);
		if (!default_tzloc)
			default_tzloc = "";

		if (priv->search_sexp) {
			priv->full_sexp = g_strdup_printf (
				"(and (occur-in-time-range? (make-time \"%s\") (make-time \"%s\") \"%s\") %s)",
				iso_start, iso_end, default_tzloc,
				priv->search_sexp ? priv->search_sexp : "");
		} else {
			priv->full_sexp = g_strdup_printf (
				"(occur-in-time-range? (make-time \"%s\") (make-time \"%s\") \"%s\")",
				iso_start, iso_end, default_tzloc);
		}

		g_free (iso_start);
		g_free (iso_end);
	} else if (priv->search_sexp) {
		priv->full_sexp = g_strdup (priv->search_sexp);
	} else {
		priv->full_sexp = g_strdup ("#f");
	}

	/* clean up the current contents, which should be done
	 * always from the main thread, because of gtk calls during removal */
	data.model = model;
	data.eflag = e_flag_new ();

	if (!g_main_context_is_owner (g_main_context_default ())) {
		/* function called from other than main thread */
		e_named_timeout_add (10, cleanup_content_cb, &data);
		e_flag_wait (data.eflag);
	} else {
		cleanup_content_cb (&data);
	}

	e_flag_free (data.eflag);

	/* update the view for all clients */

	list = cal_model_clients_list (model);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ClientData *client_data = link->data;

		update_e_cal_view_for_client (model, client_data);
	}

	g_list_free_full (list, (GDestroyNotify) client_data_unref);
}

void
e_cal_model_get_time_range (ECalModel *model,
                            time_t *start,
                            time_t *end)
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
e_cal_model_set_time_range (ECalModel *model,
                            time_t start,
                            time_t end)
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

	g_signal_emit (model, signals[TIME_RANGE_CHANGED], 0, start, end);
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
e_cal_model_set_search_query (ECalModel *model,
                              const gchar *sexp)
{
	ECalModelPrivate *priv;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	if (!strcmp (sexp ? sexp : "", priv->search_sexp ? priv->search_sexp : ""))
		return;

	if (priv->search_sexp)
		g_free (priv->search_sexp);

	if (!sexp || !*sexp)
		priv->search_sexp = NULL;
	else
		priv->search_sexp = g_strdup (sexp);

	redo_queries (model);
}

/**
 * e_cal_model_set_query
 */
void
e_cal_model_set_search_query_with_time_range (ECalModel *model,
                                              const gchar *sexp,
                                              time_t start,
                                              time_t end)
{
	ECalModelPrivate *priv;
	gboolean do_query = FALSE;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	if (strcmp (sexp ? sexp : "", priv->search_sexp ? priv->search_sexp : "")) {
		if (priv->search_sexp)
			g_free (priv->search_sexp);

		if (!sexp || !*sexp)
			priv->search_sexp = NULL;
		else
			priv->search_sexp = g_strdup (sexp);
		do_query = TRUE;
	}

	if (!(priv->start == start && priv->end == end)) {
		priv->start = start;
		priv->end = end;
		do_query = TRUE;

		g_signal_emit (model, signals[TIME_RANGE_CHANGED], 0, start, end);
	}

	if (do_query)
		redo_queries (model);
}

/**
 * e_cal_model_create_component_with_defaults
 */
icalcomponent *
e_cal_model_create_component_with_defaults (ECalModel *model,
                                            gboolean all_day)
{
	ECalModelPrivate *priv;
	ECalComponent *comp;
	icalcomponent *icalcomp;
	ECalClient *client;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	client = e_cal_model_ref_default_client (model);
	if (client == NULL)
		return icalcomponent_new (priv->kind);

	switch (priv->kind) {
	case ICAL_VEVENT_COMPONENT :
		comp = cal_comp_event_new_with_defaults (
			client, all_day,
			e_cal_model_get_use_default_reminder (model),
			e_cal_model_get_default_reminder_interval (model),
			e_cal_model_get_default_reminder_units (model));
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

	g_object_unref (client);

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
 * Returns information about attendees in the component.
 * If there are no attendees, the function returns NULL.
 *
 * The information is like "Status: Accepted: X   Declined: Y  ...".
 *
 * Free returned pointer with g_free.
 **/
gchar *
e_cal_model_get_attendees_status_info (ECalModel *model,
                                       ECalComponent *comp,
                                       ECalClient *cal_client)
{
	struct _values {
		icalparameter_partstat status;
		const gchar *caption;
		gint count;
	} values[] = {
		{ ICAL_PARTSTAT_ACCEPTED,    N_("Accepted"),     0 },
		{ ICAL_PARTSTAT_DECLINED,    N_("Declined"),     0 },
		{ ICAL_PARTSTAT_TENTATIVE,   N_("Tentative"),    0 },
		{ ICAL_PARTSTAT_DELEGATED,   N_("Delegated"),    0 },
		{ ICAL_PARTSTAT_NEEDSACTION, N_("Needs action"), 0 },
		{ ICAL_PARTSTAT_NONE,        N_("Other"),        0 },
		{ ICAL_PARTSTAT_X,           NULL,              -1 }
	};

	ESourceRegistry *registry;
	GSList *attendees = NULL, *a;
	gboolean have = FALSE;
	gchar *res = NULL;
	gint i;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	registry = e_cal_model_get_registry (model);

	if (!comp || !e_cal_component_has_attendees (comp) ||
	    !itip_organizer_is_user_ex (registry, comp, cal_client, TRUE))
		return NULL;

	e_cal_component_get_attendee_list (comp, &attendees);

	for (a = attendees; a; a = a->next) {
		ECalComponentAttendee *att = a->data;

		if (att && att->cutype == ICAL_CUTYPE_INDIVIDUAL &&
		    (att->role == ICAL_ROLE_CHAIR ||
		     att->role == ICAL_ROLE_REQPARTICIPANT ||
		     att->role == ICAL_ROLE_OPTPARTICIPANT)) {
			have = TRUE;

			for (i = 0; values[i].count != -1; i++) {
				if (att->status == values[i].status || values[i].status == ICAL_PARTSTAT_NONE) {
					values[i].count++;
					break;
				}
			}
		}
	}

	if (have) {
		GString *str = g_string_new ("");

		for (i = 0; values[i].count != -1; i++) {
			if (values[i].count > 0) {
				if (str->str && *str->str)
					g_string_append (str, "   ");

				g_string_append_printf (str, "%s: %d", _(values[i].caption), values[i].count);
			}
		}

		g_string_prepend (str, ": ");

		/* To Translators: 'Status' here means the state of the attendees, the resulting string will be in a form:
		 * Status: Accepted: X   Declined: Y   ... */
		g_string_prepend (str, _("Status"));

		res = g_string_free (str, FALSE);
	}

	if (attendees)
		e_cal_component_free_attendee_list (attendees);

	return res;
}

/**
 * e_cal_model_get_color_for_component
 */
const gchar *
e_cal_model_get_color_for_component (ECalModel *model,
                                     ECalModelComponent *comp_data)
{
	ECalModelClass *model_class;
	const gchar *color = NULL;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (comp_data != NULL, NULL);

	model_class = (ECalModelClass *) G_OBJECT_GET_CLASS (model);
	if (model_class->get_color_for_component != NULL)
		color = model_class->get_color_for_component (model, comp_data);

	if (!color)
		color = cal_model_get_color_for_component (model, comp_data);

	return color;
}

/**
 * e_cal_model_get_rgb_color_for_component
 */
gboolean
e_cal_model_get_rgb_color_for_component (ECalModel *model,
                                         ECalModelComponent *comp_data,
                                         gdouble *red,
                                         gdouble *green,
                                         gdouble *blue)
{
	GdkColor gdk_color;
	const gchar *color;

	color = e_cal_model_get_color_for_component (model, comp_data);
	if (color && gdk_color_parse (color, &gdk_color)) {

		if (red)
			*red = ((gdouble) gdk_color.red)/0xffff;
		if (green)
			*green = ((gdouble) gdk_color.green)/0xffff;
		if (blue)
			*blue = ((gdouble) gdk_color.blue)/0xffff;

		return TRUE;
	}

	return FALSE;
}

/**
 * e_cal_model_get_component_at
 */
ECalModelComponent *
e_cal_model_get_component_at (ECalModel *model,
                              gint row)
{
	ECalModelPrivate *priv;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	g_return_val_if_fail (row >= 0 && row < priv->objects->len, NULL);

	return g_ptr_array_index (priv->objects, row);
}

ECalModelComponent *
e_cal_model_get_component_for_uid (ECalModel *model,
                                   const ECalComponentId *id)
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
e_cal_model_date_value_to_string (ECalModel *model,
                                  gconstpointer value)
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
 *
	new_ecdv = g_new0 (ECellDateEditValue, 1);
	new_ecdv->tt = ecdv ? ecdv->tt : icaltime_null_time ();
	new_ecdv->zone = ecdv ? ecdv->zone : NULL;
 *
	return new_ecdv;
} */

static void e_cal_model_component_finalize (GObject *object);

/* Class initialization function for the calendar component object */
static void
e_cal_model_component_class_init (ECalModelComponentClass *class)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) class;
	g_type_class_add_private (class, sizeof (ECalModelComponentPrivate));

	object_class->finalize = e_cal_model_component_finalize;
}

static void
e_cal_model_component_finalize (GObject *object)
{
	ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT (object);

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

	if (comp_data->priv->categories_str)
		g_string_free (comp_data->priv->categories_str, TRUE);
	comp_data->priv->categories_str = NULL;

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_cal_model_component_parent_class)->finalize (object);
}

static void
e_cal_model_component_init (ECalModelComponent *comp)
{
	comp->priv = E_CAL_MODEL_COMPONENT_GET_PRIVATE (comp);
}

/**
 * e_cal_model_generate_instances_sync
 *
 * cb function is not called with cb_data, but with ECalModelGenerateInstancesData which contains cb_data
 */
void
e_cal_model_generate_instances_sync (ECalModel *model,
                                     time_t start,
                                     time_t end,
                                     ECalRecurInstanceFn cb,
                                     gpointer cb_data)
{
	ECalModelGenerateInstancesData mdata;
	gint i, n;

	n = e_table_model_row_count (E_TABLE_MODEL (model));
	for (i = 0; i < n; i++) {
		ECalModelComponent *comp_data = e_cal_model_get_component_at (model, i);

		mdata.comp_data = comp_data;
		mdata.cb_data = cb_data;

		if (comp_data->instance_start < end && comp_data->instance_end > start)
			e_cal_client_generate_instances_for_object_sync (comp_data->client, comp_data->icalcomp, start, end, cb, &mdata);
	}
}

/**
 * e_cal_model_get_object_array
 */
GPtrArray *
e_cal_model_get_object_array (ECalModel *model)
{
	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (model->priv != NULL, NULL);

	return model->priv->objects;
}

void
e_cal_model_set_instance_times (ECalModelComponent *comp_data,
                                const icaltimezone *zone)
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
			 * same day, we add 1 day to DTEND. This means that most
			 * events created with the old Evolution behavior will still
			 * work OK. */
			icaltime_adjust (&end_time, 1, 0, 0, 0);
			icalcomponent_set_dtend (comp_data->icalcomp, end_time);
		}
	}

	/* Some events can have missing DTEND, then use the start_time for them */
	if (icaltime_is_null_time (end_time))
		end_time = start_time;

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
				if (tzid)
					e_cal_client_get_timezone_sync (comp_data->client, tzid, &st_zone, NULL, NULL);

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
				if (tzid)
					e_cal_client_get_timezone_sync (comp_data->client, tzid, &end_zone, NULL, NULL);

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
e_cal_model_set_default_time_func (ECalModel *model,
                                   ECalModelDefaultTimeFunc func,
                                   gpointer user_data)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	model->priv->get_default_time = func;
	model->priv->get_default_time_user_data = user_data;
}


ECellDateEditValue *
e_cal_model_copy_cell_date_value (const ECellDateEditValue *value)
{
	ECellDateEditValue *copy;

	if (!value)
		return NULL;


	copy = g_new0 (ECellDateEditValue, 1);
	copy->tt = value->tt;
	copy->zone = value->zone;

	return copy;
}
