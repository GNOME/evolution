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

#include <libebackend/libebackend.h>

#include <e-util/e-util.h>
#include <e-util/e-util-enumtypes.h>

#include "comp-util.h"
#include "e-cal-data-model-subscriber.h"
#include "e-cal-dialogs.h"
#include "e-cal-ops.h"
#include "itip-utils.h"

#include "e-cal-model.h"

struct _ECalModelComponentPrivate {
	GString *categories_str;
	gint icon_index;
};

struct _ECalModelPrivate {
	ECalDataModel *data_model;
	ESourceRegistry *registry;
	EShell *shell;
	EClientCache *client_cache;

	/* The default source uid of an ECalClient */
	gchar *default_source_uid;

	/* Array for storing the objects. Each element is of type ECalModelComponent */
	GPtrArray *objects;

	ICalComponentKind kind;
	ICalTimezone *zone;

	/* The time range to display */
	time_t start;
	time_t end;

	/* The search regular expression */
	gchar *search_sexp;

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
	gint work_day_start_mon;
	gint work_day_end_mon;
	gint work_day_start_tue;
	gint work_day_end_tue;
	gint work_day_start_wed;
	gint work_day_end_wed;
	gint work_day_start_thu;
	gint work_day_end_thu;
	gint work_day_start_fri;
	gint work_day_end_fri;
	gint work_day_start_sat;
	gint work_day_end_sat;
	gint work_day_start_sun;
	gint work_day_end_sun;

	/* callback, to retrieve start time for newly added rows by click-to-add */
	ECalModelDefaultTimeFunc get_default_time;
	gpointer get_default_time_user_data;

	/* Default reminder for events */
	gboolean use_default_reminder;
	gint default_reminder_interval;
	EDurationType default_reminder_units;

	/* Ask user to confirm before deleting components. */
	gboolean confirm_delete;
};

static const gchar *cal_model_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data);

enum {
	PROP_0,
	PROP_CLIENT_CACHE,
	PROP_COMPRESS_WEEKEND,
	PROP_CONFIRM_DELETE,
	PROP_DATA_MODEL,
	PROP_DEFAULT_REMINDER_INTERVAL,
	PROP_DEFAULT_REMINDER_UNITS,
	PROP_DEFAULT_SOURCE_UID,
	PROP_REGISTRY,
	PROP_SHELL,
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
	PROP_WORK_DAY_START_MINUTE,
	PROP_WORK_DAY_START_MON,
	PROP_WORK_DAY_END_MON,
	PROP_WORK_DAY_START_TUE,
	PROP_WORK_DAY_END_TUE,
	PROP_WORK_DAY_START_WED,
	PROP_WORK_DAY_END_WED,
	PROP_WORK_DAY_START_THU,
	PROP_WORK_DAY_END_THU,
	PROP_WORK_DAY_START_FRI,
	PROP_WORK_DAY_END_FRI,
	PROP_WORK_DAY_START_SAT,
	PROP_WORK_DAY_END_SAT,
	PROP_WORK_DAY_START_SUN,
	PROP_WORK_DAY_END_SUN
};

enum {
	TIME_RANGE_CHANGED,
	ROW_APPENDED,
	COMPS_DELETED,
	TIMEZONE_CHANGED,
	OBJECT_CREATED,
	LAST_SIGNAL
};

/* Forward Declarations */
static void e_cal_model_table_model_init (ETableModelInterface *iface);
static void e_cal_model_cal_data_model_subscriber_init (ECalDataModelSubscriberInterface *iface);

static guint signals[LAST_SIGNAL];

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ECalModel, e_cal_model, G_TYPE_OBJECT,
	G_ADD_PRIVATE (ECalModel)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_TABLE_MODEL, e_cal_model_table_model_init)
	G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER, e_cal_model_cal_data_model_subscriber_init))

G_DEFINE_TYPE_WITH_PRIVATE (ECalModelComponent, e_cal_model_component, G_TYPE_OBJECT)

static void
e_cal_model_component_set_icalcomponent (ECalModelComponent *comp_data,
					 ECalModel *model,
					 ICalComponent *icomp)
{
	if (model != NULL)
		g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (comp_data != NULL);

	g_clear_object (&comp_data->icalcomp);
	comp_data->icalcomp = icomp;

	if (comp_data->priv->categories_str)
		g_string_free (comp_data->priv->categories_str, TRUE);
	comp_data->priv->categories_str = NULL;
	comp_data->priv->icon_index = -1;

	g_clear_pointer (&comp_data->dtstart, e_cell_date_edit_value_free);
	g_clear_pointer (&comp_data->dtend, e_cell_date_edit_value_free);
	g_clear_pointer (&comp_data->due, e_cell_date_edit_value_free);
	g_clear_pointer (&comp_data->completed, e_cell_date_edit_value_free);
	g_clear_pointer (&comp_data->created, e_cell_date_edit_value_free);
	g_clear_pointer (&comp_data->lastmodified, e_cell_date_edit_value_free);
	g_clear_pointer (&comp_data->color, g_free);

	if (comp_data->icalcomp && model)
		e_cal_model_set_instance_times (comp_data, model->priv->zone);
}

static void
e_cal_model_component_finalize (GObject *object)
{
	ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT (object);

	g_clear_object (&comp_data->client);

	e_cal_model_component_set_icalcomponent (comp_data, NULL, NULL);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_cal_model_component_parent_class)->finalize (object);
}

/* Class initialization function for the calendar component object */
static void
e_cal_model_component_class_init (ECalModelComponentClass *class)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) class;

	object_class->finalize = e_cal_model_component_finalize;
}

static void
e_cal_model_component_init (ECalModelComponent *comp)
{
	comp->priv = e_cal_model_component_get_instance_private (comp);
	comp->priv->icon_index = -1;
	comp->is_new_component = FALSE;
}

static gpointer
get_categories (ECalModelComponent *comp_data)
{
	if (!comp_data->priv->categories_str) {
		ICalProperty *prop;

		comp_data->priv->categories_str = g_string_new ("");

		for (prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_CATEGORIES_PROPERTY);
		     prop;
		     g_object_unref (prop), prop = i_cal_component_get_next_property (comp_data->icalcomp, I_CAL_CATEGORIES_PROPERTY)) {
			const gchar *categories = i_cal_property_get_categories (prop);
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
	ICalProperty *prop;
	ICalProperty_Class class_prop;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_CLASS_PROPERTY);

	if (!prop)
		return _("Public");

	class_prop = i_cal_property_get_class (prop);

	g_clear_object (&prop);

	switch (class_prop) {
	case I_CAL_CLASS_PUBLIC:
		return _("Public");
	case I_CAL_CLASS_PRIVATE:
		return _("Private");
	case I_CAL_CLASS_CONFIDENTIAL:
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
	ICalProperty *prop;
	GString *str = NULL;

	if (i_cal_component_isa (comp_data->icalcomp) == I_CAL_VJOURNAL_COMPONENT) {
		for (prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_DESCRIPTION_PROPERTY);
		     prop;
		     g_object_unref (prop), prop = i_cal_component_get_next_property (comp_data->icalcomp, I_CAL_DESCRIPTION_PROPERTY)) {
			if (!str)
				str = g_string_new (NULL);
			g_string_append (str, i_cal_property_get_description (prop));
		}
	} else {
		prop = e_cal_util_component_find_property_for_locale (comp_data->icalcomp, I_CAL_DESCRIPTION_PROPERTY, NULL);
		if (prop) {
			str = g_string_new (i_cal_property_get_description (prop));
			g_clear_object (&prop);
		}
	}

	return str ? g_string_free (str, FALSE) : g_strdup ("");
}

static ECellDateEditValue *
get_dtstart (ECalModel *model,
             ECalModelComponent *comp_data)
{
	if (!comp_data->dtstart) {
		comp_data->dtstart = e_cal_model_util_get_datetime_value (model, comp_data,
			I_CAL_DTSTART_PROPERTY, i_cal_property_get_dtstart);
	}

	return e_cell_date_edit_value_copy (comp_data->dtstart);
}

static ECellDateEditValue *
get_datetime_from_utc (ECalModel *model,
                       ECalModelComponent *comp_data,
                       ICalPropertyKind propkind,
                       ICalTime * (*get_value) (ICalProperty *prop),
		       ECellDateEditValue **buffer)
{
	g_return_val_if_fail (buffer != NULL, NULL);

	if (!*buffer) {
		ECalModelPrivate *priv;
		ICalTime *tt_value;
		ICalProperty *prop;

		priv = model->priv;

		prop = i_cal_component_get_first_property (comp_data->icalcomp, propkind);
		if (!prop)
			return NULL;

		tt_value = get_value (prop);

		/* these are always in UTC, thus convert to default zone, if any and done */
		if (priv->zone)
			i_cal_time_convert_timezone (tt_value, i_cal_timezone_get_utc_timezone (), priv->zone);

		g_object_unref (prop);

		if (!i_cal_time_is_valid_time (tt_value) || i_cal_time_is_null_time (tt_value)) {
			g_clear_object (&tt_value);
			return NULL;
		}

		*buffer = e_cell_date_edit_value_new_take (tt_value, NULL);
	}

	return e_cell_date_edit_value_copy (*buffer);
}

static gpointer
get_summary (ECalModelComponent *comp_data)
{
	ICalProperty *prop;
	gchar *res = NULL;

	prop = e_cal_util_component_find_property_for_locale (comp_data->icalcomp, I_CAL_SUMMARY_PROPERTY, NULL);
	if (prop)
		res = g_strdup (i_cal_property_get_summary (prop));

	g_clear_object (&prop);

	if (!res)
		res = g_strdup ("");

	e_cal_model_until_sanitize_text_value (res, -1);

	return res;
}

static gchar *
get_uid (ECalModelComponent *comp_data)
{
	return (gchar *) i_cal_component_get_uid (comp_data->icalcomp);
}

static gchar *
get_source_description (ESourceRegistry *registry,
			ECalModelComponent *comp_data)
{
	if (!registry || !comp_data || !comp_data->client)
		return NULL;

	return e_util_get_source_full_name (registry, e_client_get_source (E_CLIENT (comp_data->client)));
}

static void
set_categories (ECalModelComponent *comp_data,
                const gchar *value)
{
	ICalProperty *prop;

	/* remove all categories first */
	e_cal_util_component_remove_property_by_kind (comp_data->icalcomp, I_CAL_CATEGORIES_PROPERTY, TRUE);

	if (comp_data->priv->categories_str)
		g_string_free (comp_data->priv->categories_str, TRUE);
	comp_data->priv->categories_str = NULL;

	/* then set a new value; no need to populate categories_str,
	 * it'll be populated on demand (in the get_categories() function)
	*/
	if (value && *value) {
		prop = i_cal_property_new_categories (value);
		i_cal_component_take_property (comp_data->icalcomp, prop);
	}
}

static void
set_classification (ECalModelComponent *comp_data,
                    const gchar *value)
{
	ICalProperty *prop;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_CLASS_PROPERTY);
	if (!value || !(*value)) {
		if (prop) {
			i_cal_component_remove_property (comp_data->icalcomp, prop);
			g_clear_object (&prop);
		}
	} else {
		ICalProperty_Class ical_class;

		if (!g_ascii_strcasecmp (value, "PUBLIC"))
			ical_class = I_CAL_CLASS_PUBLIC;
		else if (!g_ascii_strcasecmp (value, "PRIVATE"))
			ical_class = I_CAL_CLASS_PRIVATE;
		else if (!g_ascii_strcasecmp (value, "CONFIDENTIAL"))
			ical_class = I_CAL_CLASS_CONFIDENTIAL;
		else
			ical_class = I_CAL_CLASS_NONE;

		if (!prop) {
			prop = i_cal_property_new_class (ical_class);
			i_cal_component_take_property (comp_data->icalcomp, prop);
		} else {
			i_cal_property_set_class (prop, ical_class);
			g_clear_object (&prop);
		}
	}
}

static void
set_description (ECalModelComponent *comp_data,
                 const gchar *value)
{
	ICalProperty *prop;

	/* remove old description(s) */
	e_cal_util_component_remove_property_by_kind (comp_data->icalcomp, I_CAL_DESCRIPTION_PROPERTY, TRUE);

	/* now add the new description */
	if (!value || !(*value))
		return;

	prop = i_cal_property_new_description (value);
	i_cal_component_take_property (comp_data->icalcomp, prop);
}

static void
set_dtstart (ECalModel *model,
             ECalModelComponent *comp_data,
             gconstpointer value)
{
	e_cal_model_update_comp_time (
		model, comp_data, value,
		I_CAL_DTSTART_PROPERTY,
		i_cal_property_set_dtstart,
		i_cal_property_new_dtstart);
}

static void
set_summary (ECalModelComponent *comp_data,
             const gchar *value)
{
	ICalProperty *prop;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_SUMMARY_PROPERTY);

	if (e_str_is_empty (value)) {
		if (prop) {
			i_cal_component_remove_property (comp_data->icalcomp, prop);
			g_clear_object (&prop);
		}
	} else {
		if (prop) {
			i_cal_property_set_summary (prop, value);
			g_clear_object (&prop);
		} else {
			prop = i_cal_property_new_summary (value);
			i_cal_component_take_property (comp_data->icalcomp, prop);
		}
	}
}

static void
datetime_to_zone (ECalClient *client,
		  ICalTime *tt,
		  ICalTimezone *tt_zone,
                  const gchar *tzid)
{
	ICalTimezone *from, *to;
	const gchar *tt_tzid = NULL;

	g_return_if_fail (tt != NULL);

	if (tt_zone)
		tt_tzid = i_cal_timezone_get_tzid (tt_zone);

	if (tt_tzid == NULL || tzid == NULL ||
	    tt_tzid == tzid || g_str_equal (tt_tzid, tzid))
		return;

	from = tt_zone;
	to = i_cal_timezone_get_builtin_timezone_from_tzid (tzid);
	if (!to) {
		/* do not abort on failure here, maybe the zone is not available there */
		if (!e_cal_client_get_timezone_sync (client, tzid, &to, NULL, NULL))
			to = NULL;
	}

	i_cal_time_convert_timezone (tt, from, to);
}

static void
cal_model_set_data_model (ECalModel *model,
			  ECalDataModel *data_model)
{
	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (model->priv->data_model == NULL);

	/* Be aware of a circular dependency, once this @model is subscribed to
	   the @data_model, then the @data_model increases reference count
	   of the @model.
	*/
	model->priv->data_model = g_object_ref (data_model);
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
cal_model_set_shell (ECalModel *model,
		     EShell *shell)
{
	EClientCache *client_cache;

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (model->priv->shell == NULL);

	model->priv->shell = g_object_ref (shell);

	client_cache = e_shell_get_client_cache (shell);

	g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));
	g_return_if_fail (model->priv->client_cache == NULL);

	model->priv->client_cache = g_object_ref (client_cache);
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

		case PROP_DATA_MODEL:
			cal_model_set_data_model (
				E_CAL_MODEL (object),
				g_value_get_object (value));
			return;

		case PROP_DEFAULT_SOURCE_UID:
			e_cal_model_set_default_source_uid (
				E_CAL_MODEL (object),
				g_value_get_string (value));
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

		case PROP_SHELL:
			cal_model_set_shell (
				E_CAL_MODEL (object),
				g_value_get_object (value));
			return;

		case PROP_TIMEZONE:
			e_cal_model_set_timezone (
				E_CAL_MODEL (object),
				g_value_get_object (value));
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

		case PROP_WORK_DAY_START_MON:
			e_cal_model_set_work_day_start_mon (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_MON:
			e_cal_model_set_work_day_end_mon (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_TUE:
			e_cal_model_set_work_day_start_tue (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_TUE:
			e_cal_model_set_work_day_end_tue (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_WED:
			e_cal_model_set_work_day_start_wed (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_WED:
			e_cal_model_set_work_day_end_wed (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_THU:
			e_cal_model_set_work_day_start_thu (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_THU:
			e_cal_model_set_work_day_end_thu (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_FRI:
			e_cal_model_set_work_day_start_fri (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_FRI:
			e_cal_model_set_work_day_end_fri (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_SAT:
			e_cal_model_set_work_day_start_sat (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_SAT:
			e_cal_model_set_work_day_end_sat (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_SUN:
			e_cal_model_set_work_day_start_sun (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_SUN:
			e_cal_model_set_work_day_end_sun (
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
		case PROP_CLIENT_CACHE:
			g_value_set_object (
				value,
				e_cal_model_get_client_cache (
				E_CAL_MODEL (object)));
			return;

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

		case PROP_DATA_MODEL:
			g_value_set_object (
				value,
				e_cal_model_get_data_model (
				E_CAL_MODEL (object)));
			return;

		case PROP_DEFAULT_SOURCE_UID:
			g_value_set_string (
				value,
				e_cal_model_get_default_source_uid (
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

		case PROP_SHELL:
			g_value_set_object (
				value,
				e_cal_model_get_shell (
				E_CAL_MODEL (object)));
			return;

		case PROP_TIMEZONE:
			g_value_set_object (
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

		case PROP_WORK_DAY_START_MON:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_start_mon (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_END_MON:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_end_mon (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_START_TUE:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_start_tue (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_END_TUE:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_end_tue (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_START_WED:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_start_wed (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_END_WED:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_end_wed (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_START_THU:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_start_thu (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_END_THU:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_end_thu (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_START_FRI:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_start_fri (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_END_FRI:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_end_fri (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_START_SAT:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_start_sat (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_END_SAT:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_end_sat (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_START_SUN:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_start_sun (
				E_CAL_MODEL (object)));
			return;

		case PROP_WORK_DAY_END_SUN:
			g_value_set_int (
				value,
				e_cal_model_get_work_day_end_sun (
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
	ECalModel *self = E_CAL_MODEL (object);

	g_clear_object (&self->priv->data_model);
	g_clear_object (&self->priv->registry);
	g_clear_object (&self->priv->shell);
	g_clear_object (&self->priv->client_cache);
	g_clear_object (&self->priv->zone);

	g_clear_pointer (&self->priv->default_source_uid, g_free);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_cal_model_parent_class)->dispose (object);
}

static void
cal_model_finalize (GObject *object)
{
	ECalModel *self = E_CAL_MODEL (object);
	gint ii;

	g_free (self->priv->default_category);

	for (ii = 0; ii < self->priv->objects->len; ii++) {
		ECalModelComponent *comp_data;

		comp_data = g_ptr_array_index (self->priv->objects, ii);
		if (comp_data == NULL) {
			g_warning ("comp_data is null\n");
			continue;
		}
		g_object_unref (comp_data);
	}
	g_ptr_array_free (self->priv->objects, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_cal_model_parent_class)->finalize (object);
}

static const gchar *
cal_model_get_color_for_component (ECalModel *model,
                                   ECalModelComponent *comp_data)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	cal_comp_util_set_color_for_component (comp_data->client, comp_data->icalcomp, &comp_data->color);

	return comp_data->color;
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

static const gchar *
cal_model_kind_to_extension_name (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	switch (model->priv->kind) {
		case I_CAL_VEVENT_COMPONENT:
			return E_SOURCE_EXTENSION_CALENDAR;
		case I_CAL_VJOURNAL_COMPONENT:
			return E_SOURCE_EXTENSION_MEMO_LIST;
		case I_CAL_VTODO_COMPONENT:
			return E_SOURCE_EXTENSION_TASK_LIST;
		default:
			g_warn_if_reached ();
			break;
	}

	return NULL;
}

typedef struct {
	ECalModel *model;
	ETableModel *table_model;
	GHashTable *values;
	gboolean success;
} CreateComponentData;

static void
create_component_data_free (gpointer ptr)
{
	CreateComponentData *ccd = ptr;

	if (ccd) {
		GHashTableIter iter;
		gpointer key, value;

		g_hash_table_iter_init (&iter, ccd->values);
		while (g_hash_table_iter_next (&iter, &key, &value)) {
			gint column = GPOINTER_TO_INT (key);

			e_table_model_free_value (ccd->table_model, column, value);
		}

		if (ccd->success)
			g_signal_emit (ccd->model, signals[ROW_APPENDED], 0);

		g_clear_object (&ccd->model);
		g_clear_object (&ccd->table_model);
		g_hash_table_destroy (ccd->values);
		g_slice_free (CreateComponentData, ccd);
	}
}

static void
cal_model_create_component_from_values_thread (EAlertSinkThreadJobData *job_data,
					       gpointer user_data,
					       GCancellable *cancellable,
					       GError **error)
{
	CreateComponentData *ccd = user_data;
	EClientCache *client_cache;
	ESourceRegistry *registry;
	ESource *source;
	EClient *client;
	ECalModelComponent *comp_data;
	ICalProperty *prop;
	const gchar *source_uid;
	gchar *display_name;
	GError *local_error = NULL;

	g_return_if_fail (ccd != NULL);

	source_uid = e_cal_model_get_default_source_uid (ccd->model);
	g_return_if_fail (source_uid != NULL);

	client_cache = e_cal_model_get_client_cache (ccd->model);
	registry = e_cal_model_get_registry (ccd->model);

	source = e_source_registry_ref_source (registry, source_uid);
	if (!source) {
		g_set_error (&local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
			_("Source with UID “%s” not found"), source_uid);
		e_alert_sink_thread_job_set_alert_arg_0 (job_data, source_uid);
		return;
	}

	display_name = e_util_get_source_full_name (registry, source);
	e_alert_sink_thread_job_set_alert_arg_0 (job_data, display_name);
	g_free (display_name);

	client = e_client_cache_get_client_sync (client_cache, source,
		cal_model_kind_to_extension_name (ccd->model), (guint32) -1, cancellable, &local_error);
	g_clear_object (&source);

	if (!client) {
		e_util_propagate_open_source_job_error (job_data,
			cal_model_kind_to_extension_name (ccd->model), local_error, error);
		return;
	}

	comp_data = g_object_new (E_TYPE_CAL_MODEL_COMPONENT, NULL);
	comp_data->client = E_CAL_CLIENT (g_object_ref (client));
	comp_data->icalcomp = e_cal_model_create_component_with_defaults_sync (ccd->model, comp_data->client, FALSE, cancellable, error);

	if (comp_data->icalcomp) {
		ECalModelClass *model_class;
		gchar *uid = NULL;
		gpointer dtstart;

		/* set values for our fields */
		set_categories (comp_data, e_cal_model_util_get_value (ccd->values, E_CAL_MODEL_FIELD_CATEGORIES));
		set_classification (comp_data, e_cal_model_util_get_value (ccd->values, E_CAL_MODEL_FIELD_CLASSIFICATION));
		set_description (comp_data, e_cal_model_util_get_value (ccd->values, E_CAL_MODEL_FIELD_DESCRIPTION));
		set_summary (comp_data, e_cal_model_util_get_value (ccd->values, E_CAL_MODEL_FIELD_SUMMARY));

		dtstart = e_cal_model_util_get_value (ccd->values, E_CAL_MODEL_FIELD_DTSTART);
		if (dtstart) {
			set_dtstart (ccd->model, comp_data, dtstart);
		} else if (ccd->model->priv->get_default_time) {
			time_t tt = ccd->model->priv->get_default_time (ccd->model, ccd->model->priv->get_default_time_user_data);

			if (tt > 0) {
				/* Store Memo DTSTART as date, not as date-time */
				ICalTime *itt = i_cal_time_new_from_timet_with_zone (tt,
					i_cal_component_isa (comp_data->icalcomp) == I_CAL_VJOURNAL_COMPONENT, e_cal_model_get_timezone (ccd->model));

				prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_DTSTART_PROPERTY);

				if (prop) {
					i_cal_property_set_dtstart (prop, itt);
					g_object_unref (prop);
				} else {
					prop = i_cal_property_new_dtstart (itt);
					i_cal_component_take_property (comp_data->icalcomp, prop);
				}
			}
		}

		/* call the class' method for filling the component */
		model_class = E_CAL_MODEL_GET_CLASS (ccd->model);
		if (model_class->fill_component_from_values != NULL) {
			model_class->fill_component_from_values (ccd->model, comp_data, ccd->values);
		}

		prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_CLASS_PROPERTY);
		if (!prop || i_cal_property_get_class (prop) == I_CAL_CLASS_NONE) {
			ICalProperty_Class ical_class = I_CAL_CLASS_PUBLIC;
			GSettings *settings;

			settings = e_util_ref_settings ("org.gnome.evolution.calendar");
			if (g_settings_get_boolean (settings, "classify-private"))
				ical_class = I_CAL_CLASS_PRIVATE;
			g_object_unref (settings);

			if (!prop) {
				prop = i_cal_property_new_class (ical_class);
				i_cal_component_take_property (comp_data->icalcomp, prop);
			} else {
				i_cal_property_set_class (prop, ical_class);
				g_object_unref (prop);
			}
		} else {
			g_clear_object (&prop);
		}

		ccd->success = e_cal_client_create_object_sync (comp_data->client, comp_data->icalcomp, E_CAL_OPERATION_FLAG_NONE, &uid, cancellable, error);

		g_free (uid);
	}

	g_object_unref (comp_data);
	g_object_unref (client);
}

static void
cal_model_append_row (ETableModel *etm,
                      ETableModel *source,
                      gint row)
{
	ECalModelClass *model_class;
	ECalModel *model = (ECalModel *) etm;
	GHashTable *values;
	GCancellable *cancellable;
	CreateComponentData *ccd;
	const gchar *description;
	const gchar *alert_ident;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_TABLE_MODEL (source));

	switch (e_cal_model_get_component_kind (model)) {
		case I_CAL_VEVENT_COMPONENT:
			description = _("Creating an event");
			alert_ident = "calendar:failed-create-event";
			break;
		case I_CAL_VJOURNAL_COMPONENT:
			description = _("Creating a memo");
			alert_ident = "calendar:failed-create-memo";
			break;
		case I_CAL_VTODO_COMPONENT:
			description = _("Creating a task");
			alert_ident = "calendar:failed-create-task";
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	values = g_hash_table_new (g_direct_hash, g_direct_equal);

	/* store values for our fields */
	e_cal_model_util_set_value (values, source, E_CAL_MODEL_FIELD_CATEGORIES, row);
	e_cal_model_util_set_value (values, source, E_CAL_MODEL_FIELD_CLASSIFICATION, row);
	e_cal_model_util_set_value (values, source, E_CAL_MODEL_FIELD_DESCRIPTION, row);
	e_cal_model_util_set_value (values, source, E_CAL_MODEL_FIELD_SUMMARY, row);
	e_cal_model_util_set_value (values, source, E_CAL_MODEL_FIELD_DTSTART, row);

	/* call the class' method to store other values */
	model_class = E_CAL_MODEL_GET_CLASS (model);
	if (model_class->store_values_from_model != NULL) {
		model_class->store_values_from_model (model, source, row, values);
	}

	ccd = g_slice_new0 (CreateComponentData);
	ccd->model = g_object_ref (model);
	ccd->table_model = g_object_ref (source);
	ccd->values = values;
	ccd->success = FALSE;

	cancellable = e_cal_data_model_submit_thread_job (model->priv->data_model, description,
		alert_ident, NULL, cal_model_create_component_from_values_thread,
		ccd, create_component_data_free);

	g_clear_object (&cancellable);
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
			model, comp_data, I_CAL_CREATED_PROPERTY,
			i_cal_property_get_created, &comp_data->created);
	case E_CAL_MODEL_FIELD_LASTMODIFIED :
	{
		gpointer result = (gpointer) get_datetime_from_utc (
			model, comp_data, I_CAL_LASTMODIFIED_PROPERTY,
			i_cal_property_get_lastmodified, &comp_data->lastmodified);
		if (!result && !e_cal_util_component_has_property (comp_data->icalcomp, I_CAL_METHOD_PROPERTY))
			return get_datetime_from_utc (model, comp_data, I_CAL_DTSTAMP_PROPERTY, i_cal_property_get_dtstamp, &comp_data->lastmodified);

		return result;

	}
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
		return GINT_TO_POINTER (e_cal_util_component_has_alarms (comp_data->icalcomp));
	case E_CAL_MODEL_FIELD_ICON :
	{
		gint retval = comp_data->priv->icon_index;

		if (retval >= 0)
			return GINT_TO_POINTER (retval);

		retval = 0;

		if (i_cal_component_isa (comp_data->icalcomp) == I_CAL_VEVENT_COMPONENT) {
			if (e_cal_util_component_has_attendee (comp_data->icalcomp))
				retval = 1;
			if (e_cal_util_component_has_recurrences (comp_data->icalcomp) ||
			    e_cal_util_component_is_instance (comp_data->icalcomp))
				retval = 2;
		} else if (i_cal_component_isa (comp_data->icalcomp) == I_CAL_VJOURNAL_COMPONENT) {
			if (e_cal_util_component_has_attendee (comp_data->icalcomp))
				retval = 1;
		} else {
			ECalComponent *comp;

			comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (comp_data->icalcomp));
			if (comp) {
				if (e_cal_component_has_recurrences (comp))
					retval = 1;
				else if (itip_organizer_is_user (registry, comp, comp_data->client))
					retval = 3;
				else {
					GSList *attendees = NULL, *sl;

					attendees = e_cal_component_get_attendees (comp);
					for (sl = attendees; sl != NULL; sl = sl->next) {
						ECalComponentAttendee *ca = sl->data;
						const gchar *text;

						text = e_cal_util_get_attendee_email (ca);
						if (itip_address_is_user (registry, text)) {
							if (e_cal_component_attendee_get_delegatedto (ca) != NULL)
								retval = 3;
							else
								retval = 2;
							break;
						}
					}

					g_slist_free_full (attendees, e_cal_component_attendee_free);
				}

				g_object_unref (comp);
			}
		}

		comp_data->priv->icon_index = retval;

		return GINT_TO_POINTER (retval);
	}
	case E_CAL_MODEL_FIELD_SUMMARY :
		return get_summary (comp_data);
	case E_CAL_MODEL_FIELD_UID :
		return get_uid (comp_data);
	case E_CAL_MODEL_FIELD_SOURCE:
		return get_source_description (registry, comp_data);
	case E_CAL_MODEL_FIELD_CANCELLED:
		return GINT_TO_POINTER (i_cal_component_get_status (comp_data->icalcomp) == I_CAL_STATUS_CANCELLED ? 1 : 0);
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
	ECalObjModType mod = E_CAL_OBJ_MOD_ALL;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST);
	g_return_if_fail (row >= 0 && row < priv->objects->len);

	comp_data = g_ptr_array_index (priv->objects, row);
	g_return_if_fail (comp_data != NULL);

	if (!e_cal_dialogs_recur_icalcomp (comp_data->client, comp_data->icalcomp, &mod, NULL, FALSE))
		return;

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

	e_cal_ops_modify_component (e_cal_model_get_data_model (model), comp_data->client, comp_data->icalcomp, mod, E_CAL_OPS_SEND_FLAG_DONT_SEND);
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
	case E_CAL_MODEL_FIELD_SOURCE:
		return g_strdup (value);
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
	case E_CAL_MODEL_FIELD_CANCELLED:
		return (gpointer) value;
	case E_CAL_MODEL_FIELD_COMPONENT :
		return i_cal_component_clone ((ICalComponent *) value);
	case E_CAL_MODEL_FIELD_DTSTART :
	case E_CAL_MODEL_FIELD_CREATED :
	case E_CAL_MODEL_FIELD_LASTMODIFIED :
		return e_cell_date_edit_value_copy (value);
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
	case E_CAL_MODEL_FIELD_SOURCE:
		g_free (value);
		break;
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
	case E_CAL_MODEL_FIELD_CANCELLED:
		break;
	case E_CAL_MODEL_FIELD_DTSTART:
	case E_CAL_MODEL_FIELD_CREATED :
	case E_CAL_MODEL_FIELD_LASTMODIFIED :
		if (value)
			e_cell_date_edit_value_free (value);
		break;
	case E_CAL_MODEL_FIELD_COMPONENT :
		if (value)
			g_object_unref ((ICalComponent *) value);
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
	case E_CAL_MODEL_FIELD_SOURCE:
		return g_strdup ("");
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DTSTART :
	case E_CAL_MODEL_FIELD_CREATED :
	case E_CAL_MODEL_FIELD_LASTMODIFIED :
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
	case E_CAL_MODEL_FIELD_COMPONENT :
	case E_CAL_MODEL_FIELD_CANCELLED:
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
			return e_str_is_empty (value);
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
	case E_CAL_MODEL_FIELD_SOURCE:
		return e_str_is_empty (value);
	case E_CAL_MODEL_FIELD_DTSTART :
	case E_CAL_MODEL_FIELD_CREATED :
	case E_CAL_MODEL_FIELD_LASTMODIFIED :
		return value ? FALSE : TRUE;
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
	case E_CAL_MODEL_FIELD_COMPONENT :
	case E_CAL_MODEL_FIELD_CANCELLED:
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
	case E_CAL_MODEL_FIELD_SOURCE:
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
	case E_CAL_MODEL_FIELD_CANCELLED:
		return g_strdup (value ? _("Yes") : _("No"));
	case E_CAL_MODEL_FIELD_COLOR :
	case E_CAL_MODEL_FIELD_COMPONENT :
		return g_strdup ("");
	}

	return g_strdup ("");
}

static gint
e_cal_model_get_component_index (ECalModel *model,
				 ECalClient *client,
				 const ECalComponentId *id)
{
	gint ii;

	for (ii = 0; ii < model->priv->objects->len; ii++) {
		ECalModelComponent *comp_data = g_ptr_array_index (model->priv->objects, ii);

		if (comp_data) {
			const gchar *uid;
			gboolean has_rid = e_cal_component_id_get_rid (id) != NULL;

			uid = i_cal_component_get_uid (comp_data->icalcomp);

			if (uid && *uid) {
				if ((!client || comp_data->client == client) && strcmp (uid, e_cal_component_id_get_uid (id)) == 0) {
					if (has_rid) {
						gchar *rid;

						rid = e_cal_util_component_get_recurid_as_string (comp_data->icalcomp);

						if (!(rid && *rid && strcmp (rid, e_cal_component_id_get_rid (id)) == 0)) {
							g_free (rid);
							continue;
						}

						g_free (rid);
					}

					return ii;
				}
			}
		}
	}

	return -1;
}

static void
cal_model_data_subscriber_component_added_or_modified (ECalDataModelSubscriber *subscriber,
						       ECalClient *client,
						       ECalComponent *comp,
						       gboolean is_added)
{
	ECalModel *model;
	ECalModelComponent *comp_data;
	ETableModel *table_model;
	ECalComponentId *id;
	ICalComponent *icomp;
	gint index;

	model = E_CAL_MODEL (subscriber);
	table_model = E_TABLE_MODEL (model);

	id = e_cal_component_get_id (comp);

	/* The component should not exist, when it's claimed being added, thus, when it's the main
	   component, remove any existing instances and add it from scratch. */
	if (is_added && !e_cal_component_id_get_rid (id)) {
		GSList *removed_comps = NULL;

		for (index = 0; index < model->priv->objects->len; index++) {
			comp_data = g_ptr_array_index (model->priv->objects, index);

			if (comp_data && comp_data->client == client) {
				const gchar *uid;

				uid = i_cal_component_get_uid (comp_data->icalcomp);

				if (uid && *uid && g_strcmp0 (uid, e_cal_component_id_get_uid (id)) == 0) {
					e_table_model_pre_change (table_model);

					g_ptr_array_remove_index (model->priv->objects, index);
					removed_comps = g_slist_prepend (removed_comps, comp_data);
					e_table_model_row_deleted (table_model, index);

					index--;
				}
			}
		}

		g_signal_emit (model, signals[COMPS_DELETED], 0, removed_comps);

		g_slist_free_full (removed_comps, g_object_unref);

		index = -1;
	} else {
		index = e_cal_model_get_component_index (model, client, id);
	}

	e_cal_component_id_free (id);

	if (index < 0 && !is_added)
		return;

	icomp = i_cal_component_clone (e_cal_component_get_icalcomponent (comp));

	if (index < 0) {
		e_table_model_pre_change (table_model);

		comp_data = g_object_new (E_TYPE_CAL_MODEL_COMPONENT, NULL);
		comp_data->is_new_component = FALSE;
		comp_data->client = g_object_ref (client);
		comp_data->icalcomp = icomp;
		e_cal_model_set_instance_times (comp_data, model->priv->zone);
		g_ptr_array_add (model->priv->objects, comp_data);

		e_table_model_row_inserted (table_model, model->priv->objects->len - 1);
	} else {
		e_table_model_pre_change (table_model);

		comp_data = g_ptr_array_index (model->priv->objects, index);
		e_cal_model_component_set_icalcomponent (comp_data, model, icomp);

		e_table_model_row_changed (table_model, index);
	}
}

static void
e_cal_model_data_subscriber_component_added (ECalDataModelSubscriber *subscriber,
					     ECalClient *client,
					     ECalComponent *comp)
{
	cal_model_data_subscriber_component_added_or_modified (subscriber, client, comp, TRUE);
}

static void
e_cal_model_data_subscriber_component_modified (ECalDataModelSubscriber *subscriber,
						ECalClient *client,
						ECalComponent *comp)
{
	cal_model_data_subscriber_component_added_or_modified (subscriber, client, comp, FALSE);
}

static void
e_cal_model_data_subscriber_component_removed (ECalDataModelSubscriber *subscriber,
					       ECalClient *client,
					       const gchar *uid,
					       const gchar *rid)
{
	ECalModel *model;
	ECalModelComponent *comp_data;
	ETableModel *table_model;
	ECalComponentId *id;
	GSList *link;
	gint index;

	model = E_CAL_MODEL (subscriber);

	id = e_cal_component_id_new (uid, rid);

	index = e_cal_model_get_component_index (model, client, id);

	e_cal_component_id_free (id);

	if (index < 0)
		return;

	table_model = E_TABLE_MODEL (model);
	e_table_model_pre_change (table_model);

	comp_data = g_ptr_array_remove_index (model->priv->objects, index);
	if (!comp_data) {
		e_table_model_no_change (table_model);
		return;
	}

	link = g_slist_append (NULL, comp_data);
	g_signal_emit (model, signals[COMPS_DELETED], 0, link);

	g_slist_free (link);
	g_object_unref (comp_data);

	e_table_model_row_deleted (table_model, index);
}

static void
e_cal_model_data_subscriber_freeze (ECalDataModelSubscriber *subscriber)
{
	/* No freeze/thaw, the ETableModel doesn't notify about changes when frozen */

	/* ETableModel *table_model = E_TABLE_MODEL (subscriber);
	e_table_model_freeze (table_model); */
}

static void
e_cal_model_data_subscriber_thaw (ECalDataModelSubscriber *subscriber)
{
	/* No freeze/thaw, the ETableModel doesn't notify about changes when frozen */

	/* ETableModel *table_model = E_TABLE_MODEL (subscriber);
	e_table_model_thaw (table_model); */
}

static void
e_cal_model_class_init (ECalModelClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = cal_model_set_property;
	object_class->get_property = cal_model_get_property;
	object_class->constructed = cal_model_constructed;
	object_class->dispose = cal_model_dispose;
	object_class->finalize = cal_model_finalize;

	class->get_color_for_component = cal_model_get_color_for_component;

	g_object_class_install_property (
		object_class,
		PROP_DATA_MODEL,
		g_param_spec_object (
			"data-model",
			"Calendar Data Model",
			NULL,
			E_TYPE_CAL_DATA_MODEL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_CLIENT_CACHE,
		g_param_spec_object (
			"client-cache",
			"Client Cache",
			NULL,
			E_TYPE_CLIENT_CACHE,
			G_PARAM_READABLE));

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
		PROP_DEFAULT_SOURCE_UID,
		g_param_spec_string (
			"default-source-uid",
			"Default source UID of an ECalClient",
			NULL,
			NULL,
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
		PROP_SHELL,
		g_param_spec_object (
			"shell",
			"Shell",
			"EShell",
			E_TYPE_SHELL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_TIMEZONE,
		g_param_spec_object (
			"timezone",
			"Time Zone",
			NULL,
			I_CAL_TYPE_TIMEZONE,
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

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_MON,
		g_param_spec_int (
			"work-day-start-mon",
			"Work Day Start for Monday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_MON,
		g_param_spec_int (
			"work-day-end-mon",
			"Work Day End for Monday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_TUE,
		g_param_spec_int (
			"work-day-start-tue",
			"Work Day Start for Tuesday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_TUE,
		g_param_spec_int (
			"work-day-end-tue",
			"Work Day End for Tuesday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_WED,
		g_param_spec_int (
			"work-day-start-wed",
			"Work Day Start for Wednesday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_WED,
		g_param_spec_int (
			"work-day-end-wed",
			"Work Day End for Wednesday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_THU,
		g_param_spec_int (
			"work-day-start-thu",
			"Work Day Start for Thursday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_THU,
		g_param_spec_int (
			"work-day-end-thu",
			"Work Day End for Thursday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_FRI,
		g_param_spec_int (
			"work-day-start-fri",
			"Work Day Start for Friday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_FRI,
		g_param_spec_int (
			"work-day-end-fri",
			"Work Day End for Friday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_SAT,
		g_param_spec_int (
			"work-day-start-sat",
			"Work Day Start for Saturday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_SAT,
		g_param_spec_int (
			"work-day-end-sat",
			"Work Day End for Saturday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_SUN,
		g_param_spec_int (
			"work-day-start-sun",
			"Work Day Start for Sunday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_SUN,
		g_param_spec_int (
			"work-day-end-sun",
			"Work Day End for Sunday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	signals[TIME_RANGE_CHANGED] = g_signal_new (
		"time_range_changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalModelClass, time_range_changed),
		NULL, NULL,
		e_marshal_VOID__INT64_INT64,
		G_TYPE_NONE, 2,
		G_TYPE_INT64,
		G_TYPE_INT64);

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

	signals[TIMEZONE_CHANGED] = g_signal_new (
		"timezone-changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalModelClass, timezone_changed),
		NULL, NULL,
		e_marshal_VOID__OBJECT_OBJECT,
		G_TYPE_NONE, 2,
		I_CAL_TYPE_TIMEZONE,
		I_CAL_TYPE_TIMEZONE);

	signals[OBJECT_CREATED] = g_signal_new (
		"object-created",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalModelClass, object_created),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1, E_TYPE_CAL_CLIENT);
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
e_cal_model_cal_data_model_subscriber_init (ECalDataModelSubscriberInterface *iface)
{
	iface->component_added = e_cal_model_data_subscriber_component_added;
	iface->component_modified = e_cal_model_data_subscriber_component_modified;
	iface->component_removed = e_cal_model_data_subscriber_component_removed;
	iface->freeze = e_cal_model_data_subscriber_freeze;
	iface->thaw = e_cal_model_data_subscriber_thaw;
}

static void
e_cal_model_init (ECalModel *model)
{
	model->priv = e_cal_model_get_instance_private (model);

	/* match none by default */
	model->priv->start = (time_t) -1;
	model->priv->end = (time_t) -1;

	model->priv->objects = g_ptr_array_new ();
	model->priv->kind = I_CAL_NO_COMPONENT;

	model->priv->use_24_hour_format = TRUE;
}

/* updates time in a component, and keeps the timezone used in it, if exists */
void
e_cal_model_update_comp_time (ECalModel *model,
                              ECalModelComponent *comp_data,
                              gconstpointer time_value,
                              ICalPropertyKind kind,
                              void (*set_func) (ICalProperty *prop,
                                                ICalTime *v),
                              ICalProperty * (*new_func) (ICalTime *v))
{
	ECellDateEditValue *dv = (ECellDateEditValue *) time_value;
	ICalProperty *prop;
	ICalParameter *param;
	ICalTimezone *model_zone;
	ICalTime *tt;

	g_return_if_fail (model != NULL);
	g_return_if_fail (comp_data != NULL);
	g_return_if_fail (set_func != NULL);
	g_return_if_fail (new_func != NULL);

	prop = i_cal_component_get_first_property (comp_data->icalcomp, kind);
	if (prop)
		param = i_cal_property_get_first_parameter (prop, I_CAL_TZID_PARAMETER);
	else
		param = NULL;

	/* If we are setting the property to NULL (i.e. removing it), then
	 * we remove it if it exists. */
	if (!dv) {
		if (prop) {
			i_cal_component_remove_property (comp_data->icalcomp, prop);
			g_object_unref (prop);
		}

		return;
	}

	model_zone = e_cal_model_get_timezone (model);
	tt = e_cell_date_edit_value_get_time (dv);
	datetime_to_zone (comp_data->client, tt, model_zone, param ? i_cal_parameter_get_tzid (param) : NULL);

	if (prop) {
		set_func (prop, tt);
	} else {
		prop = new_func (tt);
		i_cal_component_take_property (comp_data->icalcomp, prop);

		prop = i_cal_component_get_first_property (comp_data->icalcomp, kind);
	}

	if (param) {
		const gchar *tzid = i_cal_parameter_get_tzid (param);

		/* If the TZID is set to "UTC", we don't want to save the TZID. */
		if (!tzid || !*tzid || !strcmp (tzid, "UTC")) {
			i_cal_property_remove_parameter_by_kind (prop, I_CAL_TZID_PARAMETER);
		}
	} else if (model_zone) {
		const gchar *tzid = i_cal_timezone_get_tzid (model_zone);

		if (tzid && *tzid) {
			param = i_cal_parameter_new_tzid (tzid);
			i_cal_property_take_parameter (prop, param);
		}
	}

	g_clear_object (&prop);
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
	gboolean readonly = FALSE;
	ECalClient *client = NULL;

	if (row != -1) {
		ECalModelComponent *comp_data;

		comp_data = e_cal_model_get_component_at (model, row);

		if (comp_data != NULL && comp_data->client != NULL)
			client = g_object_ref (comp_data->client);

		readonly = (client == NULL);
	} else {
		const gchar *source_uid;

		source_uid = e_cal_model_get_default_source_uid (model);

		/* if the source cannot be opened, then expect the client being writable;
		   there will be shown an error if not, when saving changes anyway */
		readonly = source_uid == NULL;

		if (source_uid != NULL) {
			ESourceRegistry *registry = e_cal_model_get_registry (model);
			EClientCache *client_cache = e_cal_model_get_client_cache (model);
			ESource *source;

			source = e_source_registry_ref_source (registry, source_uid);
			if (source) {
				EClient *e_client;

				e_client = e_client_cache_ref_cached_client (client_cache, source,
					cal_model_kind_to_extension_name (model));
				if (e_client) {
					client = E_CAL_CLIENT (e_client);
				} else {
					const gchar *parent_uid = e_source_get_parent (source);

					/* There are couple known to be always read-only */
					readonly = g_strcmp0 (parent_uid, "webcal-stub") == 0 ||
						   g_strcmp0 (parent_uid, "weather-stub") == 0 ||
						   g_strcmp0 (parent_uid, "contacts-stub") == 0;
				}
			}

			g_clear_object (&source);
		}
	}

	if (!readonly && client)
		readonly = e_client_is_readonly (E_CLIENT (client));

	g_clear_object (&client);

	return !readonly;
}

ESourceRegistry *
e_cal_model_get_registry (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return model->priv->registry;
}

EShell *
e_cal_model_get_shell (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return model->priv->shell;
}

ECalDataModel *
e_cal_model_get_data_model (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return model->priv->data_model;
}

EClientCache *
e_cal_model_get_client_cache (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return model->priv->client_cache;
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

ICalComponentKind
e_cal_model_get_component_kind (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), I_CAL_NO_COMPONENT);

	return model->priv->kind;
}

void
e_cal_model_set_component_kind (ECalModel *model,
				ICalComponentKind kind)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	model->priv->kind = kind;
}

ICalTimezone *
e_cal_model_get_timezone (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return model->priv->zone;
}

void
e_cal_model_set_timezone (ECalModel *model,
			  const ICalTimezone *zone)
{
	ICalTimezone *old_zone;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->zone == zone)
		return;

	e_table_model_pre_change (E_TABLE_MODEL (model));
	old_zone = model->priv->zone;
	model->priv->zone = zone ? e_cal_util_copy_timezone (zone) : NULL;

	/* the timezone affects the times shown for date fields,
	 * so we need to redisplay everything */
	e_table_model_changed (E_TABLE_MODEL (model));

	g_object_notify (G_OBJECT (model), "timezone");
	g_signal_emit (
		model, signals[TIMEZONE_CHANGED], 0,
		old_zone, model->priv->zone);

	g_clear_object (&old_zone);
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

gint
e_cal_model_get_work_day_start_mon (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	return model->priv->work_day_start_mon;
}

void
e_cal_model_set_work_day_start_mon (ECalModel *model,
				    gint work_day_start)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_start_mon == work_day_start)
		return;

	model->priv->work_day_start_mon = work_day_start;

	g_object_notify (G_OBJECT (model), "work-day-start-mon");
}

gint
e_cal_model_get_work_day_end_mon (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	return model->priv->work_day_end_mon;
}

void
e_cal_model_set_work_day_end_mon (ECalModel *model,
				  gint work_day_end)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_end_mon == work_day_end)
		return;

	model->priv->work_day_end_mon = work_day_end;

	g_object_notify (G_OBJECT (model), "work-day-end-mon");
}

gint
e_cal_model_get_work_day_start_tue (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	return model->priv->work_day_start_tue;
}

void
e_cal_model_set_work_day_start_tue (ECalModel *model,
				    gint work_day_start)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_start_tue == work_day_start)
		return;

	model->priv->work_day_start_tue = work_day_start;

	g_object_notify (G_OBJECT (model), "work-day-start-tue");
}

gint
e_cal_model_get_work_day_end_tue (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	return model->priv->work_day_end_tue;
}

void
e_cal_model_set_work_day_end_tue (ECalModel *model,
				  gint work_day_end)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_end_tue == work_day_end)
		return;

	model->priv->work_day_end_tue = work_day_end;

	g_object_notify (G_OBJECT (model), "work-day-end-tue");
}

gint
e_cal_model_get_work_day_start_wed (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	return model->priv->work_day_start_wed;
}

void
e_cal_model_set_work_day_start_wed (ECalModel *model,
				    gint work_day_start)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_start_wed == work_day_start)
		return;

	model->priv->work_day_start_wed = work_day_start;

	g_object_notify (G_OBJECT (model), "work-day-start-wed");
}

gint
e_cal_model_get_work_day_end_wed (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	return model->priv->work_day_end_wed;
}

void
e_cal_model_set_work_day_end_wed (ECalModel *model,
				  gint work_day_end)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_end_wed == work_day_end)
		return;

	model->priv->work_day_end_wed = work_day_end;

	g_object_notify (G_OBJECT (model), "work-day-end-wed");
}

gint
e_cal_model_get_work_day_start_thu (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	return model->priv->work_day_start_thu;
}

void
e_cal_model_set_work_day_start_thu (ECalModel *model,
				    gint work_day_start)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_start_thu == work_day_start)
		return;

	model->priv->work_day_start_thu = work_day_start;

	g_object_notify (G_OBJECT (model), "work-day-start-thu");
}

gint
e_cal_model_get_work_day_end_thu (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	return model->priv->work_day_end_thu;
}

void
e_cal_model_set_work_day_end_thu (ECalModel *model,
				  gint work_day_end)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_end_thu == work_day_end)
		return;

	model->priv->work_day_end_thu = work_day_end;

	g_object_notify (G_OBJECT (model), "work-day-end-thu");
}

gint
e_cal_model_get_work_day_start_fri (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	return model->priv->work_day_start_fri;
}

void
e_cal_model_set_work_day_start_fri (ECalModel *model,
				    gint work_day_start)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_start_fri == work_day_start)
		return;

	model->priv->work_day_start_fri = work_day_start;

	g_object_notify (G_OBJECT (model), "work-day-start-fri");
}

gint
e_cal_model_get_work_day_end_fri (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	return model->priv->work_day_end_fri;
}

void
e_cal_model_set_work_day_end_fri (ECalModel *model,
				  gint work_day_end)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_end_fri == work_day_end)
		return;

	model->priv->work_day_end_fri = work_day_end;

	g_object_notify (G_OBJECT (model), "work-day-end-fri");
}

gint
e_cal_model_get_work_day_start_sat (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	return model->priv->work_day_start_sat;
}

void
e_cal_model_set_work_day_start_sat (ECalModel *model,
				    gint work_day_start)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_start_sat == work_day_start)
		return;

	model->priv->work_day_start_sat = work_day_start;

	g_object_notify (G_OBJECT (model), "work-day-start-sat");
}

gint
e_cal_model_get_work_day_end_sat (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	return model->priv->work_day_end_sat;
}

void
e_cal_model_set_work_day_end_sat (ECalModel *model,
				  gint work_day_end)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_end_sat == work_day_end)
		return;

	model->priv->work_day_end_sat = work_day_end;

	g_object_notify (G_OBJECT (model), "work-day-end-sat");
}

gint
e_cal_model_get_work_day_start_sun (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	return model->priv->work_day_start_sun;
}

void
e_cal_model_set_work_day_start_sun (ECalModel *model,
				    gint work_day_start)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_start_sun == work_day_start)
		return;

	model->priv->work_day_start_sun = work_day_start;

	g_object_notify (G_OBJECT (model), "work-day-start-sun");
}

gint
e_cal_model_get_work_day_end_sun (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	return model->priv->work_day_end_sun;
}

void
e_cal_model_set_work_day_end_sun (ECalModel *model,
				  gint work_day_end)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->work_day_end_sun == work_day_end)
		return;

	model->priv->work_day_end_sun = work_day_end;

	g_object_notify (G_OBJECT (model), "work-day-end-sun");
}

void
e_cal_model_get_work_day_range_for (ECalModel *model,
				    GDateWeekday weekday,
				    gint *start_hour,
				    gint *start_minute,
				    gint *end_hour,
				    gint *end_minute)
{
	gint start_adept = -1, end_adept = -1;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (start_hour != NULL);
	g_return_if_fail (start_minute != NULL);
	g_return_if_fail (end_hour != NULL);
	g_return_if_fail (end_minute != NULL);

	switch (weekday) {
		case G_DATE_MONDAY:
			start_adept = e_cal_model_get_work_day_start_mon (model);
			end_adept = e_cal_model_get_work_day_end_mon (model);
			break;
		case G_DATE_TUESDAY:
			start_adept = e_cal_model_get_work_day_start_tue (model);
			end_adept = e_cal_model_get_work_day_end_tue (model);
			break;
		case G_DATE_WEDNESDAY:
			start_adept = e_cal_model_get_work_day_start_wed (model);
			end_adept = e_cal_model_get_work_day_end_wed (model);
			break;
		case G_DATE_THURSDAY:
			start_adept = e_cal_model_get_work_day_start_thu (model);
			end_adept = e_cal_model_get_work_day_end_thu (model);
			break;
		case G_DATE_FRIDAY:
			start_adept = e_cal_model_get_work_day_start_fri (model);
			end_adept = e_cal_model_get_work_day_end_fri (model);
			break;
		case G_DATE_SATURDAY:
			start_adept = e_cal_model_get_work_day_start_sat (model);
			end_adept = e_cal_model_get_work_day_end_sat (model);
			break;
		case G_DATE_SUNDAY:
			start_adept = e_cal_model_get_work_day_start_sun (model);
			end_adept = e_cal_model_get_work_day_end_sun (model);
			break;
		default:
			break;
	}

	if (start_adept > 0 && (start_adept / 100) >= 0 && (start_adept / 100) <= 23 &&
	    (start_adept % 100) >= 0 && (start_adept % 100) <= 59) {
		*start_hour = start_adept / 100;
		*start_minute = start_adept % 100;
	} else {
		*start_hour = e_cal_model_get_work_day_start_hour (model);
		*start_minute = e_cal_model_get_work_day_start_minute (model);
	}

	if (end_adept > 0 && (end_adept / 100) >= 0 && (end_adept / 100) <= 23 &&
	    (end_adept % 100) >= 0 && (end_adept % 100) <= 59) {
		*end_hour = end_adept / 100;
		*end_minute = end_adept % 100;
	} else {
		*end_hour = e_cal_model_get_work_day_end_hour (model);
		*end_minute = e_cal_model_get_work_day_end_minute (model);
	}
}

const gchar *
e_cal_model_get_default_source_uid (ECalModel *model)
{
	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	if (model->priv->default_source_uid && !*model->priv->default_source_uid)
		return NULL;

	return model->priv->default_source_uid;
}

void
e_cal_model_set_default_source_uid (ECalModel *model,
				    const gchar *source_uid)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (g_strcmp0 (model->priv->default_source_uid, source_uid) == 0)
		return;

	g_free (model->priv->default_source_uid);
	model->priv->default_source_uid = g_strdup (source_uid);

	g_object_notify (G_OBJECT (model), "default-source-uid");
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
			gchar *rid;
			gboolean has_rid = e_cal_component_id_get_rid (id) != NULL;

			uid = i_cal_component_get_uid (comp_data->icalcomp);
			rid = e_cal_util_component_get_recurid_as_string (comp_data->icalcomp);

			if (uid && *uid) {
				if ((!client || comp_data->client == client) &&
				    !g_strcmp0 (e_cal_component_id_get_uid (id), uid)) {
					if (has_rid) {
						if (!(rid && *rid && !g_strcmp0 (e_cal_component_id_get_rid (id), rid))) {
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

void
e_cal_model_remove_all_objects (ECalModel *model)
{
	ECalModelComponent *comp_data;
	ETableModel *table_model;
	GSList *comps = NULL;
	guint ii;

	table_model = E_TABLE_MODEL (model);

	for (ii = 0; ii < model->priv->objects->len; ii++) {
		comp_data = g_ptr_array_index (model->priv->objects, ii);

		if (comp_data)
			comps = g_slist_prepend (comps, comp_data);
	}

	ii = model->priv->objects->len;

	e_table_model_pre_change (table_model);
	e_table_model_rows_deleted (table_model, 0, ii);

	g_ptr_array_set_size (model->priv->objects, 0);

	if (comps)
		g_signal_emit (model, signals[COMPS_DELETED], 0, comps);

	g_slist_free_full (comps, g_object_unref);
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
	ECalDataModelSubscriber *subscriber;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (start >= 0 && end >= 0);
	g_return_if_fail (start <= end);

	priv = model->priv;

	if (start != (time_t) 0 && end != (time_t) 0) {
		end = time_day_end_with_zone (end, priv->zone) - 1;
	}

	if (priv->start == start && priv->end == end)
		return;

	subscriber = E_CAL_DATA_MODEL_SUBSCRIBER (model);
	priv->start = start;
	priv->end = end;

	g_signal_emit (model, signals[TIME_RANGE_CHANGED], 0, (gint64) start, (gint64) end);

	e_cal_data_model_subscribe (model->priv->data_model, subscriber, start, end);
}

/**
 * e_cal_model_create_component_with_defaults_sync
 */
ICalComponent *
e_cal_model_create_component_with_defaults_sync (ECalModel *model,
						 ECalClient *client,
						 gboolean all_day,
						 GCancellable *cancellable,
						 GError **error)
{
	ECalComponent *comp = NULL;
	ICalComponent *icomp;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	if (client) {
		switch (model->priv->kind) {
		case I_CAL_VEVENT_COMPONENT :
			comp = cal_comp_event_new_with_defaults_sync (
				client, all_day,
				e_cal_model_get_use_default_reminder (model),
				e_cal_model_get_default_reminder_interval (model),
				e_cal_model_get_default_reminder_units (model),
				cancellable, error);
			break;
		case I_CAL_VTODO_COMPONENT :
			comp = cal_comp_task_new_with_defaults_sync (client, cancellable, error);
			break;
		case I_CAL_VJOURNAL_COMPONENT :
			comp = cal_comp_memo_new_with_defaults_sync (client, cancellable, error);
			break;
		default:
			g_warn_if_reached ();
			return NULL;
		}
	}

	if (comp) {
		icomp = i_cal_component_clone (e_cal_component_get_icalcomponent (comp));
		g_object_unref (comp);
	} else {
		icomp = i_cal_component_new (model->priv->kind);
	}

	/* make sure the component has a UID */
	if (!i_cal_component_get_uid (icomp)) {
		gchar *uid;

		uid = e_util_generate_uid ();
		i_cal_component_set_uid (icomp, uid);

		g_free (uid);
	}

	return icomp;
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
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return cal_comp_util_dup_attendees_status_info (comp, cal_client, e_cal_model_get_registry (model));
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

gboolean
e_cal_model_get_rgba_for_component (ECalModel *model,
				    ECalModelComponent *comp_data,
				    GdkRGBA *rgba)
{
	const gchar *color;

	color = e_cal_model_get_color_for_component (model, comp_data);
	if (!color)
		return FALSE;

	return gdk_rgba_parse (rgba, color);
}

/**
 * e_cal_model_get_rgb_color_for_component:
 *
 * Deprecated: 3.20: Use e_cal_model_get_rgba_for_component() instead
 */
gboolean
e_cal_model_get_rgb_color_for_component (ECalModel *model,
                                         ECalModelComponent *comp_data,
                                         gdouble *red,
                                         gdouble *green,
                                         gdouble *blue)
{
	GdkRGBA rgba;

	if (!e_cal_model_get_rgba_for_component (model, comp_data, &rgba))
		return FALSE;

	if (red)
		*red = rgba.red;
	if (green)
		*green = rgba.green;
	if (blue)
		*blue = rgba.blue;

	return TRUE;
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
e_cal_model_get_component_for_client_and_uid (ECalModel *model,
					      ECalClient *client,
					      const ECalComponentId *id)
{
	ECalModelPrivate *priv;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	return search_by_id_and_client (priv, client, id);
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
	struct tm tmp_tm;
	gchar buffer[64];

	g_return_val_if_fail (E_IS_CAL_MODEL (model), g_strdup (""));

	priv = model->priv;

	if (!dv)
		return g_strdup ("");

	/* We currently convert all the dates to the current timezone. */
	tmp_tm = e_cal_util_icaltime_to_tm_with_zone (e_cell_date_edit_value_get_time (dv), e_cell_date_edit_value_get_zone (dv), priv->zone);

	memset (buffer, 0, sizeof (buffer));
	e_time_format_date_and_time (&tmp_tm, priv->use_24_hour_format,
				     TRUE, FALSE,
				     buffer, sizeof (buffer));
	return g_strdup (buffer);
}

typedef struct _GenerateInstacesData {
	ECalModelGenerateInstancesData mdata;
	ECalRecurInstanceCb cb;
	ECalClient *client;
	ICalTimezone *zone;
} GenerateInstancesData;

static gboolean
ecm_generate_instances_cb (ICalComponent *comp,
			   ICalTime *instance_start,
			   ICalTime *instance_end,
			   gpointer user_data,
			   GCancellable *cancellable,
			   GError **error)
{
	GenerateInstancesData *gid = user_data;
	ICalTime *changed_instance_start = NULL, *changed_instance_end = NULL;
	gboolean res;

	g_return_val_if_fail (gid != NULL, FALSE);
	g_return_val_if_fail (gid->mdata.comp_data != NULL, FALSE);

	cal_comp_get_instance_times (gid->mdata.comp_data->client, comp,
		gid->zone, &changed_instance_start, &changed_instance_end, cancellable);

	res = gid->cb (comp, changed_instance_start, changed_instance_end, &gid->mdata, cancellable, error);

	g_clear_object (&changed_instance_start);
	g_clear_object (&changed_instance_end);

	return res;
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
				     GCancellable *cancellable,
                                     ECalRecurInstanceCb cb,
                                     gpointer cb_data)
{
	GenerateInstancesData gid;
	gint i, n;

	g_return_if_fail (cb != NULL);

	gid.mdata.cb_data = cb_data;
	gid.cb = cb;
	gid.zone = model->priv->zone;

	n = e_table_model_row_count (E_TABLE_MODEL (model));
	for (i = 0; i < n; i++) {
		ECalModelComponent *comp_data = e_cal_model_get_component_at (model, i);

		if (comp_data->instance_start < end && comp_data->instance_end > start) {
			gid.mdata.comp_data = comp_data;

			e_cal_client_generate_instances_for_object_sync (comp_data->client, comp_data->icalcomp, start, end,
				cancellable, ecm_generate_instances_cb, &gid);
		}
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
				const ICalTimezone *zone)
{
	ICalTime *instance_start = NULL, *instance_end = NULL;

	if (i_cal_component_isa (comp_data->icalcomp) == I_CAL_VEVENT_COMPONENT) {
		ICalTime *start_time, *end_time;

		start_time = i_cal_component_get_dtstart (comp_data->icalcomp);
		end_time = i_cal_component_get_dtend (comp_data->icalcomp);

		if (i_cal_time_is_date (start_time) && i_cal_time_is_null_time (end_time)) {
			/* If end_time is null and it's an all day event,
			 * just make start_time = end_time so that end_time
			 * will be a valid date
			 */
			g_clear_object (&end_time);
			end_time = i_cal_time_clone (start_time);
			i_cal_time_adjust (end_time, 1, 0, 0, 0);
			i_cal_component_set_dtend (comp_data->icalcomp, end_time);
		} else if (i_cal_time_is_date (start_time) && i_cal_time_is_date (end_time) &&
			   (i_cal_time_compare_date_only (start_time, end_time) == 0)) {
			/* If both DTSTART and DTEND are DATE values, and they are the
			 * same day, we add 1 day to DTEND. This means that most
			 * events created with the old Evolution behavior will still
			 * work OK. */
			i_cal_time_adjust (end_time, 1, 0, 0, 0);
			i_cal_component_set_dtend (comp_data->icalcomp, end_time);
		}

		g_clear_object (&start_time);
		g_clear_object (&end_time);
	}

	cal_comp_get_instance_times (comp_data->client, comp_data->icalcomp, zone,
		&instance_start, &instance_end, NULL);

	comp_data->instance_start = instance_start ? i_cal_time_as_timet_with_zone (instance_start,
		i_cal_time_get_timezone (instance_start)) : comp_data->instance_start;
	comp_data->instance_end = instance_end ? i_cal_time_as_timet_with_zone (instance_end,
		i_cal_time_get_timezone (instance_end)) : comp_data->instance_end;

	g_clear_object (&instance_start);
	g_clear_object (&instance_end);
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

void
e_cal_model_modify_component (ECalModel *model,
			      ECalModelComponent *comp_data,
			      ECalObjModType mod)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));

	e_cal_ops_modify_component (e_cal_model_get_data_model (model), comp_data->client, comp_data->icalcomp, mod, E_CAL_OPS_SEND_FLAG_ASK);
}

void
e_cal_model_util_set_value (GHashTable *values,
			    ETableModel *table_model,
			    gint column,
			    gint row)
{
	gpointer value;

	g_return_if_fail (values != NULL);

	value = e_table_model_value_at (table_model, column, row);

	g_hash_table_insert (values, GINT_TO_POINTER (column),
		e_table_model_duplicate_value (table_model, column, value));
}

gpointer
e_cal_model_util_get_value (GHashTable *values,
			    gint column)
{
	g_return_val_if_fail (values != NULL, NULL);

	return g_hash_table_lookup (values, GINT_TO_POINTER (column));
}

void
e_cal_model_emit_object_created (ECalModel *model,
				 ECalClient *where)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL_CLIENT (where));

	g_signal_emit (model, signals[OBJECT_CREATED], 0, where);
}

gpointer
e_cal_model_util_get_status (ECalModelComponent *comp_data)
{
	ICalProperty *prop;
	const gchar *res = "";

	g_return_val_if_fail (comp_data != NULL, (gpointer) "");

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_STATUS_PROPERTY);
	if (prop) {
		ICalPropertyStatus status;

		status = i_cal_property_get_status (prop);

		g_object_unref (prop);

		res = cal_comp_util_status_to_localized_string (i_cal_component_isa (comp_data->icalcomp), status);
		if (!res)
			res = "";
	}

	return (gpointer) res;
}

ICalPropertyStatus
e_cal_model_util_set_status (ECalModelComponent *comp_data,
			     gconstpointer value)
{
	ICalProperty *prop;
	ICalPropertyStatus status;
	const gchar *str_value = value;

	g_return_val_if_fail (comp_data != NULL, I_CAL_STATUS_NONE);

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_STATUS_PROPERTY);

	if (!str_value || !*str_value) {
		if (prop) {
			i_cal_component_remove_property (comp_data->icalcomp, prop);
			g_object_unref (prop);
		}

		return I_CAL_STATUS_NONE;
	}

	status = cal_comp_util_localized_string_to_status (i_cal_component_isa (comp_data->icalcomp), str_value, NULL, NULL);

	if (status == I_CAL_STATUS_NONE) {
		if (prop) {
			i_cal_component_remove_property (comp_data->icalcomp, prop);
			g_object_unref (prop);
		}
	} else if (prop) {
		i_cal_property_set_status (prop, status);
		g_object_unref (prop);
	} else {
		prop = i_cal_property_new_status (status);
		i_cal_component_take_property (comp_data->icalcomp, prop);
	}

	return status;
}

static const gchar *
get_cmp_cache_str (gpointer cmp_cache,
		   const gchar *str)
{
	const gchar *value;

	if (!cmp_cache || !str)
		return str;

	value = e_table_sorting_utils_lookup_cmp_cache (cmp_cache, str);
	if (!value) {
		gchar *ckey;

		ckey = g_utf8_collate_key (str, -1);
		e_table_sorting_utils_add_to_cmp_cache (cmp_cache, (gchar *) str, ckey);
		value = ckey;
	}

	return value;
}

static gint
cmp_cache_strings (gconstpointer str_a,
		   gconstpointer str_b,
		   gpointer cmp_cache)
{
	if (!cmp_cache)
		return g_utf8_collate (str_a, str_b);

	str_b = get_cmp_cache_str (cmp_cache, str_b);

	g_return_val_if_fail (str_a != NULL, 0);
	g_return_val_if_fail (str_b != NULL, 0);

	return g_strcmp0 (str_a, str_b);
}

gint
e_cal_model_util_status_compare_cb (gconstpointer a,
				    gconstpointer b,
				    gpointer cmp_cache)
{
	const gchar *string_a = a;
	const gchar *string_b = b;
	gint status_a = -2;
	gint status_b = -2;

	if (!string_a || !*string_a) {
		status_a = -1;
	} else {
		const gchar *cache_str = get_cmp_cache_str (cmp_cache, string_a);

		status_a = cal_comp_util_localized_string_to_status (I_CAL_ANY_COMPONENT, cache_str, cmp_cache_strings, cmp_cache);

		if (status_a == I_CAL_STATUS_NONE)
			status_a = -1;
	}

	if (string_b == NULL || *string_b == '\0')
		status_b = -1;
	else {
		const gchar *cache_str = get_cmp_cache_str (cmp_cache, string_b);

		status_b = cal_comp_util_localized_string_to_status (I_CAL_ANY_COMPONENT, cache_str, cmp_cache_strings, cmp_cache);

		if (status_b == I_CAL_STATUS_NONE)
			status_b = -1;
	}

	return status_a - status_b;
}

ECellDateEditValue *
e_cal_model_util_get_datetime_value (ECalModel *model,
				     ECalModelComponent *comp_data,
				     ICalPropertyKind kind,
				     ICalTime * (*get_time_func) (ICalProperty *prop))
{
	ECellDateEditValue *value;
	ICalProperty *prop;
	ICalParameter *param = NULL;
	ICalTimezone *zone = NULL;
	ICalTime *tt;
	const gchar *tzid;
	gboolean is_date;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data), NULL);
	g_return_val_if_fail (get_time_func != NULL, NULL);

	prop = i_cal_component_get_first_property (comp_data->icalcomp, kind);
	if (!prop) {
		if (kind == I_CAL_DTEND_PROPERTY &&
		    e_cal_util_component_has_property (comp_data->icalcomp, I_CAL_DURATION_PROPERTY) &&
		    e_cal_util_component_has_property (comp_data->icalcomp, I_CAL_DTSTART_PROPERTY)) {
			/* Get the TZID from the DTSTART */
			prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_DTSTART_PROPERTY);
			/* The libical calculates the DTEND from the DTSTART+DURATION */
			tt = i_cal_component_get_dtend (comp_data->icalcomp);
		} else {
			return NULL;
		}
	} else {
		tt = get_time_func (prop);
	}

	if (!tt || !i_cal_time_is_valid_time (tt) || i_cal_time_is_null_time (tt)) {
		g_clear_object (&prop);
		g_clear_object (&tt);

		return NULL;
	}

	is_date = i_cal_time_is_date (tt);

	if (!is_date) {
		param = i_cal_property_get_first_parameter (prop, I_CAL_TZID_PARAMETER);
		tzid = param ? i_cal_parameter_get_tzid (param) : NULL;

		if (!tzid || !*tzid ||
		    !e_cal_client_get_timezone_sync (comp_data->client, tzid, &zone, NULL, NULL))
			zone = NULL;

		if (!zone && i_cal_time_is_utc (tt))
			zone = i_cal_timezone_get_utc_timezone ();
	}

	if (e_cal_data_model_get_expand_recurrences (model->priv->data_model)) {
		time_t instance_tt = (time_t) 0;

		if (kind == I_CAL_DTSTART_PROPERTY)
			instance_tt = comp_data->instance_start;
		else if (kind == I_CAL_DTEND_PROPERTY)
			instance_tt = comp_data->instance_end;
		else
			g_warn_if_reached ();

		if (zone) {
			g_clear_object (&tt);
			tt = i_cal_time_new_from_timet_with_zone (instance_tt, is_date, zone);
		} else if (model->priv->zone) {
			g_clear_object (&tt);
			tt = i_cal_time_new_from_timet_with_zone (instance_tt, is_date, model->priv->zone);
		}

		if (kind == I_CAL_DTEND_PROPERTY && is_date) {
			ICalProperty *dtstart;

			dtstart = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_DTSTART_PROPERTY);

			if (dtstart) {
				ICalTime *tt_start;
				ICalTimezone *start_zone = NULL;

				tt_start = i_cal_property_get_dtstart (dtstart);

				g_clear_object (&param);

				if (!i_cal_time_is_date (tt_start)) {
					param = i_cal_property_get_first_parameter (dtstart, I_CAL_TZID_PARAMETER);
					tzid = param ? i_cal_parameter_get_tzid (param) : NULL;

					if (!tzid || !*tzid ||
					    !e_cal_client_get_timezone_sync (comp_data->client, tzid, &start_zone, NULL, NULL))
						start_zone = NULL;
				}

				if (start_zone) {
					g_clear_object (&tt_start);
					tt_start = i_cal_time_new_from_timet_with_zone (comp_data->instance_start, is_date, start_zone);
				} else {
					g_clear_object (&tt_start);
					tt_start = i_cal_time_new_from_timet_with_zone (comp_data->instance_start, is_date, model->priv->zone);
				}

				i_cal_time_adjust (tt_start, 1, 0, 0, 0);

				/* Decrease by a day only if the DTSTART will still be before, or the same as, DTEND */
				if (i_cal_time_compare (tt_start, tt) <= 0)
					i_cal_time_adjust (tt, -1, 0, 0, 0);

				g_clear_object (&tt_start);
				g_clear_object (&dtstart);
				g_clear_object (&param);
			}
		}
	}

	value = e_cell_date_edit_value_new_take (tt, zone ? e_cal_util_copy_timezone (zone) : NULL);

	g_clear_object (&prop);
	g_clear_object (&param);

	return value;
}

/* Removes unneeded characters from the 'value'.
   It modifies the 'value' inline. */
void
e_cal_model_until_sanitize_text_value (gchar *value,
				       gint value_length)
{
	if (value && (value_length > 0 || value_length == -1) && *value) {
		gchar *ptr, *pos;

		for (ptr = value, pos = value; (value_length > 0 || value_length == -1) && *ptr; ptr++, pos++) {
			if (*ptr == '\r')
				pos--;
			else if (*ptr == '\n' || *ptr == '\t')
				*pos = ' ';
			else if (pos != ptr)
				*pos = *ptr;

			if (value_length != -1)
				value_length--;
		}

		if (pos < ptr)
			*pos = '\0';
	}
}
