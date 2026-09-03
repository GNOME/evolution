/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Rodrigo Moya <rodrigo@ximian.com>
 */

#include "evolution-config.h"

#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
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

#if !ICAL_CHECK_VERSION(3, 99, 99)
#define i_cal_duration_as_utc_seconds i_cal_duration_as_int
#endif

struct _EDateEditValue {
	ICalTime *tt;
	ICalTimezone *zone;
};

EDateEditValue *
e_date_edit_value_new (const ICalTime *tt,
		       const ICalTimezone *zone)
{
	g_return_val_if_fail (I_CAL_IS_TIME ((ICalTime *) tt), NULL);
	if (zone)
		g_return_val_if_fail (I_CAL_IS_TIMEZONE ((ICalTimezone *) zone), NULL);

	return e_date_edit_value_new_take (i_cal_time_clone (tt),
		zone ? e_cal_util_copy_timezone (zone) : NULL);
}

EDateEditValue *
e_date_edit_value_new_take (ICalTime *tt,
			    ICalTimezone *zone)
{
	EDateEditValue *value;

	g_return_val_if_fail (I_CAL_IS_TIME (tt), NULL);
	if (zone)
		g_return_val_if_fail (I_CAL_IS_TIMEZONE (zone), NULL);

	value = g_new0 (EDateEditValue, 1);
	value->tt = tt;
	value->zone = zone;

	return value;
}

EDateEditValue *
e_date_edit_value_copy (const EDateEditValue *src)
{
	if (!src)
		return NULL;

	return e_date_edit_value_new (src->tt, src->zone);
}

void
e_date_edit_value_free (EDateEditValue *value)
{
	if (value) {
		g_clear_object (&value->tt);
		g_clear_object (&value->zone);
		g_free (value);
	}
}

ICalTime *
e_date_edit_value_get_time (const EDateEditValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);

	return value->tt;
}

void
e_date_edit_value_set_time (EDateEditValue *value,
			    const ICalTime *tt)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (I_CAL_IS_TIME ((ICalTime *) tt));

	e_date_edit_value_take_time (value, i_cal_time_clone (tt));
}

void
e_date_edit_value_take_time (EDateEditValue *value,
			     ICalTime *tt)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (I_CAL_IS_TIME (tt));

	if (value->tt != tt) {
		g_clear_object (&value->tt);
		value->tt = tt;
	} else {
		g_clear_object (&tt);
	}
}

ICalTimezone *
e_date_edit_value_get_zone (const EDateEditValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);

	return value->zone;
}

void
e_date_edit_value_set_zone (EDateEditValue *value,
			    const ICalTimezone *zone)
{
	g_return_if_fail (value != NULL);
	if (zone)
		g_return_if_fail (I_CAL_IS_TIMEZONE ((ICalTimezone *) zone));

	e_date_edit_value_take_zone (value, zone ? e_cal_util_copy_timezone (zone) : NULL);
}

void
e_date_edit_value_take_zone (EDateEditValue *value,
			     ICalTimezone *zone)
{
	g_return_if_fail (value != NULL);
	if (zone)
		g_return_if_fail (I_CAL_IS_TIMEZONE (zone));

	if (zone != value->zone) {
		g_clear_object (&value->zone);
		value->zone = zone;
	} else {
		g_clear_object (&zone);
	}
}

ECalModelComponent *
		_e_cal_model_test_add_component	(ECalModel *model,
							 const gchar *source_uid,
							 ICalComponent *icalcomp);
gboolean	_e_cal_model_test_modify_component	(ECalModel *model,
							 const gchar *source_uid,
							 ICalComponent *icalcomp);
gboolean	_e_cal_model_test_remove_component	(ECalModel *model,
							 const gchar *source_uid,
							 const gchar *uid,
							 const gchar *rid);
gboolean	_e_cal_model_test_reparent_component	(ECalModel *model,
							 ECalModelComponent *comp_data,
							 ECalModelComponent *new_parent);
guint		_e_cal_model_test_get_row_count		(ECalModel *model);
gchar *		_e_cal_model_test_get_tree_string	(ECalModel *model);

#if !ICAL_CHECK_VERSION(3, 99, 99)
#define ICalPropertyClassenum ICalProperty_Class
#endif

struct _ECalModelComponentPrivate {
	GString *categories_str;
	gint icon_index;

	/* ESource::uid + "\n" + ICalComponent::uid + "\n" + ICalComponent::rid,
	   computed once at creation, stable for the component's lifetime */
	gchar *own_key;
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

	/* Default reminder for events */
	gboolean use_default_reminder;
	gint default_reminder_interval;
	EDurationType default_reminder_units;

	/* Ask user to confirm before deleting components. */
	gboolean confirm_delete;

	/* RELATED-TO tree machinery */
	GHashTable *uid_to_row; /* gchar *key ~> ECalModelComponent *, borrowed */
	GHashTable *waiting_children; /* gchar *parent key ~> GPtrArray * of ECalModelComponent *, borrowed */
	GPtrArray *roots; /* ECalModelComponent *, borrowed, sorted */
	GPtrArray *visible_rows; /* ECalModelComponent *, borrowed, flattened order */
	GHashTable *component_to_visible_index; /* ECalModelComponent * (direct) ~> GUINT_TO_POINTER (index) */
	gboolean visible_dirty;
	GHashTable *collapsed_uids; /* gchar *key ~> NULL, session-only */
	ECalModelSortColumn *sort_columns;
	guint n_sort_columns;
	gboolean reparent_by_dnd;

	/* VTODO-only, inert otherwise */
	gboolean highlight_due_today;
	gchar *color_due_today;
	gboolean highlight_overdue;
	gchar *color_overdue;
};

static const gchar *cal_model_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data);

enum {
	PROP_0,
	PROP_CLIENT_CACHE,
	PROP_COMPONENT_KIND,
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
	PROP_WORK_DAY_END_SUN,
	PROP_HIGHLIGHT_DUE_TODAY,
	PROP_COLOR_DUE_TODAY,
	PROP_HIGHLIGHT_OVERDUE,
	PROP_COLOR_OVERDUE
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
static void e_cal_model_cal_data_model_subscriber_init (ECalDataModelSubscriberInterface *iface);
static void e_cal_model_virtual_tree_model_init (EVirtualTreeModelInterface *iface);

typedef enum {
	E_CAL_MODEL_DUE_NEVER,
	E_CAL_MODEL_DUE_FUTURE,
	E_CAL_MODEL_DUE_TODAY,
	E_CAL_MODEL_DUE_OVERDUE,
	E_CAL_MODEL_DUE_COMPLETE
} ECalModelDueStatus;

static void cal_model_set_component_kind (ECalModel *model, ICalComponentKind kind);
static ECalModelDueStatus cal_model_get_due_status (ECalModel *model, ECalModelComponent *comp_data);
static gboolean cal_model_is_complete (ECalModelComponent *comp_data);
static gboolean cal_model_is_status_canceled (ECalModelComponent *comp_data);
static gboolean cal_model_is_overdue (ECalModel *model, ECalModelComponent *comp_data);
static void cal_model_ensure_task_complete (ECalModelComponent *comp_data, time_t completed_date);
static void cal_model_ensure_task_partially_complete (ECalModelComponent *comp_data);
static void cal_model_ensure_task_not_complete (ECalModelComponent *comp_data, gboolean with_status);

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (ECalModel, e_cal_model, G_TYPE_OBJECT,
	G_ADD_PRIVATE (ECalModel)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER, e_cal_model_cal_data_model_subscriber_init)
	G_IMPLEMENT_INTERFACE (E_TYPE_VIRTUAL_TREE_MODEL, e_cal_model_virtual_tree_model_init))

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

	g_clear_pointer (&comp_data->dtstart, e_date_edit_value_free);
	g_clear_pointer (&comp_data->dtend, e_date_edit_value_free);
	g_clear_pointer (&comp_data->due, e_date_edit_value_free);
	g_clear_pointer (&comp_data->completed, e_date_edit_value_free);
	g_clear_pointer (&comp_data->created, e_date_edit_value_free);
	g_clear_pointer (&comp_data->lastmodified, e_date_edit_value_free);
	g_clear_pointer (&comp_data->color, g_free);

	if (comp_data->icalcomp && model)
		e_cal_model_set_instance_times (comp_data, model->priv->zone);
}

static void
e_cal_model_component_finalize (GObject *object)
{
	ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT (object);

	g_clear_object (&comp_data->client);
	g_clear_pointer (&comp_data->source_uid, g_free);
	g_clear_pointer (&comp_data->resolved_children, g_ptr_array_unref);
	g_clear_pointer (&comp_data->priv->own_key, g_free);

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
	ICalPropertyClassenum class_prop;

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

static EDateEditValue *
get_dtstart (ECalModel *model,
             ECalModelComponent *comp_data)
{
	if (!comp_data->dtstart) {
		comp_data->dtstart = e_cal_model_util_get_datetime_value (model, comp_data,
			I_CAL_DTSTART_PROPERTY, i_cal_property_get_dtstart);
	}

	return e_date_edit_value_copy (comp_data->dtstart);
}

static EDateEditValue *
get_datetime_from_utc (ECalModel *model,
                       ECalModelComponent *comp_data,
                       ICalPropertyKind propkind,
                       ECalModelTimeGetFuncType get_value,
		       EDateEditValue **buffer)
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

		*buffer = e_date_edit_value_new_take (tt_value, NULL);
	}

	return e_date_edit_value_copy (*buffer);
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
		ICalPropertyClassenum ical_class;

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
	if (!data_model)
		return;

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
	if (!registry)
		return;

	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (model->priv->registry == NULL);

	model->priv->registry = g_object_ref (registry);
}

static void
cal_model_set_shell (ECalModel *model,
		     EShell *shell)
{
	EClientCache *client_cache;

	if (!shell)
		return;

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
		case PROP_COMPONENT_KIND:
			cal_model_set_component_kind (
				E_CAL_MODEL (object),
				g_value_get_int (value));
			return;

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

		case PROP_HIGHLIGHT_DUE_TODAY:
			e_cal_model_set_highlight_due_today (
				E_CAL_MODEL (object),
				g_value_get_boolean (value));
			return;

		case PROP_COLOR_DUE_TODAY:
			e_cal_model_set_color_due_today (
				E_CAL_MODEL (object),
				g_value_get_string (value));
			return;

		case PROP_HIGHLIGHT_OVERDUE:
			e_cal_model_set_highlight_overdue (
				E_CAL_MODEL (object),
				g_value_get_boolean (value));
			return;

		case PROP_COLOR_OVERDUE:
			e_cal_model_set_color_overdue (
				E_CAL_MODEL (object),
				g_value_get_string (value));
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
		case PROP_COMPONENT_KIND:
			g_value_set_int (
				value,
				e_cal_model_get_component_kind (
				E_CAL_MODEL (object)));
			return;

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

		case PROP_HIGHLIGHT_DUE_TODAY:
			g_value_set_boolean (
				value,
				e_cal_model_get_highlight_due_today (
				E_CAL_MODEL (object)));
			return;

		case PROP_COLOR_DUE_TODAY:
			g_value_set_string (
				value,
				e_cal_model_get_color_due_today (
				E_CAL_MODEL (object)));
			return;

		case PROP_HIGHLIGHT_OVERDUE:
			g_value_set_boolean (
				value,
				e_cal_model_get_highlight_overdue (
				E_CAL_MODEL (object)));
			return;

		case PROP_COLOR_OVERDUE:
			g_value_set_string (
				value,
				e_cal_model_get_color_overdue (
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

	g_free (self->priv->color_due_today);
	g_free (self->priv->color_overdue);

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

	g_hash_table_destroy (self->priv->uid_to_row);
	g_hash_table_destroy (self->priv->waiting_children);
	g_ptr_array_free (self->priv->roots, TRUE);
	g_ptr_array_free (self->priv->visible_rows, TRUE);
	g_hash_table_destroy (self->priv->component_to_visible_index);
	g_hash_table_destroy (self->priv->collapsed_uids);
	g_free (self->priv->sort_columns);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_cal_model_parent_class)->finalize (object);
}

static const gchar *
cal_model_get_color_for_component (ECalModel *model,
                                   ECalModelComponent *comp_data)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	if (i_cal_component_isa (comp_data->icalcomp) == I_CAL_VTODO_COMPONENT) {
		switch (cal_model_get_due_status (model, comp_data)) {
		case E_CAL_MODEL_DUE_TODAY:
			if (e_cal_model_get_highlight_due_today (model))
				return e_cal_model_get_color_due_today (model);
			break;
		case E_CAL_MODEL_DUE_OVERDUE:
			if (e_cal_model_get_highlight_overdue (model))
				return e_cal_model_get_color_overdue (model);
			break;
		case E_CAL_MODEL_DUE_NEVER:
		case E_CAL_MODEL_DUE_FUTURE:
		case E_CAL_MODEL_DUE_COMPLETE:
			break;
		}
	}

	cal_comp_util_set_color_for_component (comp_data->client, comp_data->icalcomp, &comp_data->color);

	return comp_data->color;
}

static EDateEditValue *
get_dtend (ECalModel *model,
           ECalModelComponent *comp_data)
{
	if (!comp_data->dtend) {
		comp_data->dtend = e_cal_model_util_get_datetime_value (model, comp_data,
			I_CAL_DTEND_PROPERTY, i_cal_property_get_dtend);

		if (comp_data->dtend) {
			ICalTime *tt;

			tt = e_date_edit_value_get_time (comp_data->dtend);

			if (tt && i_cal_time_is_date (tt))
				i_cal_time_adjust (tt, -1, 0, 0, 0);
		}
	}

	return e_date_edit_value_copy (comp_data->dtend);
}

static gpointer
get_location (ECalModelComponent *comp_data)
{
	ICalProperty *prop;
	const gchar *location = NULL;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_LOCATION_PROPERTY);
	if (prop) {
		location = i_cal_property_get_location (prop);
		g_object_unref (prop);
	}

	return (gpointer) (location ? location : "");
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

static EDateEditValue *
get_completed (ECalModel *model,
	       ECalModelComponent *comp_data)
{
	if (!comp_data->completed) {
		comp_data->completed = e_cal_model_util_get_datetime_value (model, comp_data,
			I_CAL_COMPLETED_PROPERTY, i_cal_property_get_completed);
	}

	return e_date_edit_value_copy (comp_data->completed);
}

static EDateEditValue *
get_due (ECalModel *model,
	 ECalModelComponent *comp_data)
{
	if (!comp_data->due) {
		comp_data->due = e_cal_model_util_get_datetime_value (model, comp_data,
			I_CAL_DUE_PROPERTY, i_cal_property_get_due);
	}

	return e_date_edit_value_copy (comp_data->due);
}

static gpointer
get_geo (ECalModelComponent *comp_data)
{
	ICalProperty *prop;
	ICalGeo *geo = NULL;
	static gchar buf[32];

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_GEO_PROPERTY);
	if (prop) {
		geo = i_cal_property_get_geo (prop);
		if (geo) {
			g_snprintf (
				buf, sizeof (buf), "%g %s, %g %s",
				fabs (i_cal_geo_get_lat (geo)),
				i_cal_geo_get_lat (geo) >= 0.0 ? "N" : "S",
				fabs (i_cal_geo_get_lon (geo)),
				i_cal_geo_get_lon (geo) >= 0.0 ? "E" : "W");
			g_object_unref (prop);
			g_object_unref (geo);
			return buf;
		}
	}

	g_clear_object (&prop);
	g_clear_object (&geo);

	return (gpointer) "";
}

static gint
get_percent (ECalModelComponent *comp_data)
{
	ICalProperty *prop;
	gint percent = 0;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_PERCENTCOMPLETE_PROPERTY);
	if (prop) {
		percent = i_cal_property_get_percentcomplete (prop);
		g_object_unref (prop);
	}

	return percent;
}

static gpointer
get_priority (ECalModelComponent *comp_data)
{
	ICalProperty *prop;
	const gchar *value = NULL;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_PRIORITY_PROPERTY);
	if (prop) {
		value = e_cal_util_priority_to_string (i_cal_property_get_priority (prop));
		g_clear_object (&prop);
	}

	if (!value)
		value = "";

	return (gpointer) value;
}

static gpointer
get_url (ECalModelComponent *comp_data)
{
	ICalProperty *prop;
	const gchar *url = NULL;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_URL_PROPERTY);
	if (prop) {
		url = i_cal_property_get_url (prop);
		g_object_unref (prop);
	}

	return (gpointer) (url ? url : "");
}

static void
set_completed (ECalModel *model,
               ECalModelComponent *comp_data,
               gconstpointer value)
{
	EDateEditValue *dv = (EDateEditValue *) value;

	if (!dv) {
		cal_model_ensure_task_not_complete (comp_data, TRUE);
	} else {
		ICalTime *tt;
		time_t t;

		tt = e_date_edit_value_get_time (dv);
		if (i_cal_time_is_date (tt)) {
			i_cal_time_set_is_date (tt, FALSE);
			t = i_cal_time_as_timet_with_zone (tt, e_cal_model_get_timezone (model));
		} else {
			t = i_cal_time_as_timet_with_zone (tt, e_date_edit_value_get_zone (dv));
		}

		cal_model_ensure_task_complete (comp_data, t);
	}
}

static void
set_complete (ECalModelComponent *comp_data,
              gconstpointer value)
{
	gint state = GPOINTER_TO_INT (value);

	if (state)
		cal_model_ensure_task_complete (comp_data, -1);
	else
		cal_model_ensure_task_not_complete (comp_data, TRUE);
}

static void
set_due (ECalModel *model,
         ECalModelComponent *comp_data,
         gconstpointer value)
{
	e_cal_model_update_comp_time (model, comp_data, value, I_CAL_DUE_PROPERTY, i_cal_property_set_due, i_cal_property_new_due);
}

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
	gdouble latitude = 0.0, longitude = 0.0;
	gint matched;
	ICalGeo *geo;
	ICalProperty *prop;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_GEO_PROPERTY);

	if (e_str_is_empty (value)) {
		if (prop) {
			i_cal_component_remove_property (comp_data->icalcomp, prop);
			g_object_unref (prop);
		}
	} else {
		matched = sscanf (value, "%lg , %lg", &latitude, &longitude);
		if (matched != 2)
			show_geo_warning ();

		geo = i_cal_geo_new (latitude, longitude);

		if (prop) {
			i_cal_property_set_geo (prop, geo);
			g_object_unref (prop);
		} else {
			prop = i_cal_property_new_geo (geo);
			i_cal_component_take_property (comp_data->icalcomp, prop);
		}
	}
}

static void
set_status_task (ECalModelComponent *comp_data,
                 const gchar *value)
{
	ICalPropertyStatus status;

	status = e_cal_model_util_set_status (comp_data, value);

	if (status == I_CAL_STATUS_NONE)
		return;

	if (status == I_CAL_STATUS_NEEDSACTION)
		cal_model_ensure_task_not_complete (comp_data, TRUE);
	else if (status == I_CAL_STATUS_INPROCESS)
		cal_model_ensure_task_partially_complete (comp_data);
	else if (status == I_CAL_STATUS_CANCELLED)
		cal_model_ensure_task_not_complete (comp_data, FALSE);
	else if (status == I_CAL_STATUS_COMPLETED)
		cal_model_ensure_task_complete (comp_data, -1);
}

static void
set_percent (ECalModelComponent *comp_data,
             gconstpointer value)
{
	ICalProperty *prop;
	gint percent = GPOINTER_TO_INT (value);

	g_return_if_fail (percent >= -1);
	g_return_if_fail (percent <= 100);

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_PERCENTCOMPLETE_PROPERTY);

	if (percent == -1) {
		if (prop) {
			i_cal_component_remove_property (comp_data->icalcomp, prop);
			g_object_unref (prop);
		}
		cal_model_ensure_task_not_complete (comp_data, TRUE);
	} else {
		if (prop) {
			i_cal_property_set_percentcomplete (prop, percent);
			g_object_unref (prop);
		} else {
			prop = i_cal_property_new_percentcomplete (percent);
			i_cal_component_take_property (comp_data->icalcomp, prop);
		}

		if (percent == 100) {
			cal_model_ensure_task_complete (comp_data, -1);
		} else {
			prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_COMPLETED_PROPERTY);
			if (prop) {
				i_cal_component_remove_property (comp_data->icalcomp, prop);
				g_object_unref (prop);
			}

			if (percent > 0)
				set_status_task (comp_data, _("In Progress"));
		}
	}
}

static void
set_priority (ECalModelComponent *comp_data,
              const gchar *value)
{
	ICalProperty *prop;
	gint priority;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_PRIORITY_PROPERTY);

	priority = e_cal_util_priority_from_string (value);
	if (priority == -1) {
		g_warning ("Invalid priority");
		priority = 0;
	}

	if (prop) {
		i_cal_property_set_priority (prop, priority);
		g_object_unref (prop);
	} else {
		prop = i_cal_property_new_priority (priority);
		i_cal_component_take_property (comp_data->icalcomp, prop);
	}
}

static void
set_url (ECalModelComponent *comp_data,
         const gchar *value)
{
	ICalProperty *prop;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_URL_PROPERTY);

	if (e_str_is_empty (value)) {
		if (prop) {
			i_cal_component_remove_property (comp_data->icalcomp, prop);
			g_object_unref (prop);
		}
	} else {
		if (prop) {
			i_cal_property_set_url (prop, value);
			g_object_unref (prop);
		} else {
			prop = i_cal_property_new_url (value);
			i_cal_component_take_property (comp_data->icalcomp, prop);
		}
	}
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

static gpointer
get_estimated_duration (ECalModelComponent *comp_data)
{
	ICalProperty *prop;
	gpointer res = NULL;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_ESTIMATEDDURATION_PROPERTY);
	if (prop) {
		ICalDuration *duration;
		gint duration_int;

		duration = i_cal_property_get_estimatedduration (prop);
		duration_int = duration ? i_cal_duration_as_utc_seconds (duration) : 0;

		if (duration_int > 0) {
			gint64 *pvalue;

			pvalue = g_new (gint64, 1);
			*pvalue = duration_int;

			res = pvalue;
		}

		g_clear_object (&duration);
		g_object_unref (prop);
	}

	return res;
}

gpointer
e_cal_model_get_field_value (ECalModel *model,
                             ECalModelComponent *comp_data,
                             gint col)
{
	ESourceRegistry *registry;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data), NULL);
	g_return_val_if_fail (comp_data->icalcomp != NULL, NULL);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, NULL);

	registry = e_cal_model_get_registry (model);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES:
		return get_categories (comp_data);
	case E_CAL_MODEL_FIELD_CLASSIFICATION:
		return get_classification (comp_data);
	case E_CAL_MODEL_FIELD_COLOR:
		return (gpointer) get_color (model, comp_data);
	case E_CAL_MODEL_FIELD_COMPONENT:
		return comp_data->icalcomp;
	case E_CAL_MODEL_FIELD_DESCRIPTION:
		return get_description (comp_data);
	case E_CAL_MODEL_FIELD_DTSTART:
		return (gpointer) get_dtstart (model, comp_data);
	case E_CAL_MODEL_FIELD_CREATED:
		return (gpointer) get_datetime_from_utc (
			model, comp_data, I_CAL_CREATED_PROPERTY,
			i_cal_property_get_created, &comp_data->created);
	case E_CAL_MODEL_FIELD_LASTMODIFIED:
	{
		gpointer result = (gpointer) get_datetime_from_utc (
			model, comp_data, I_CAL_LASTMODIFIED_PROPERTY,
			i_cal_property_get_lastmodified, &comp_data->lastmodified);
		if (!result && !e_cal_util_component_has_property (comp_data->icalcomp, I_CAL_METHOD_PROPERTY))
			return get_datetime_from_utc (model, comp_data, I_CAL_DTSTAMP_PROPERTY, i_cal_property_get_dtstamp, &comp_data->lastmodified);

		return result;

	}
	case E_CAL_MODEL_FIELD_HAS_ALARMS:
		return GINT_TO_POINTER (e_cal_util_component_has_alarms (comp_data->icalcomp));
	case E_CAL_MODEL_FIELD_ICON:
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
	case E_CAL_MODEL_FIELD_SUMMARY:
		return get_summary (comp_data);
	case E_CAL_MODEL_FIELD_UID:
		return get_uid (comp_data);
	case E_CAL_MODEL_FIELD_SOURCE:
		return get_source_description (registry, comp_data);
	case E_CAL_MODEL_FIELD_CANCELLED:
		return GINT_TO_POINTER (i_cal_component_get_status (comp_data->icalcomp) == I_CAL_STATUS_CANCELLED ? 1 : 0);
	case E_CAL_MODEL_FIELD_DTEND:
		return (gpointer) get_dtend (model, comp_data);
	case E_CAL_MODEL_FIELD_LOCATION:
		return get_location (comp_data);
	case E_CAL_MODEL_FIELD_TRANSPARENCY:
		return get_transparency (comp_data);
	case E_CAL_MODEL_FIELD_STATUS:
		return e_cal_model_util_get_status (comp_data);
	case E_CAL_MODEL_FIELD_COMPLETED:
		return (gpointer) get_completed (model, comp_data);
	case E_CAL_MODEL_FIELD_STRIKEOUT:
		return GINT_TO_POINTER (cal_model_is_status_canceled (comp_data) || cal_model_is_complete (comp_data));
	case E_CAL_MODEL_FIELD_COMPLETE:
		return GINT_TO_POINTER (cal_model_is_complete (comp_data));
	case E_CAL_MODEL_FIELD_DUE:
		return (gpointer) get_due (model, comp_data);
	case E_CAL_MODEL_FIELD_GEO:
		return get_geo (comp_data);
	case E_CAL_MODEL_FIELD_OVERDUE:
		return GINT_TO_POINTER (cal_model_is_overdue (model, comp_data));
	case E_CAL_MODEL_FIELD_PERCENT:
		return GINT_TO_POINTER (get_percent (comp_data));
	case E_CAL_MODEL_FIELD_PRIORITY:
		return get_priority (comp_data);
	case E_CAL_MODEL_FIELD_URL:
		return get_url (comp_data);
	case E_CAL_MODEL_FIELD_ESTIMATED_DURATION:
		return get_estimated_duration (comp_data);
	}

	return (gpointer) "";
}

void
e_cal_model_set_field_value (ECalModel *model,
                             ECalModelComponent *comp_data,
                             gint col,
                             gconstpointer value,
                             gboolean save)
{
	ECalObjModType mod = E_CAL_OBJ_MOD_ALL;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST);

	if (save && !e_cal_dialogs_recur_icalcomp (comp_data->client, comp_data->icalcomp, &mod, NULL, FALSE))
		return;

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES:
		set_categories (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_CLASSIFICATION:
		set_classification (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_DESCRIPTION:
		set_description (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_DTSTART:
		set_dtstart (model, comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_SUMMARY:
		set_summary (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_DTEND:
		set_dtend (model, comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_LOCATION:
		set_location (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_TRANSPARENCY:
		set_transparency (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_STATUS:
		if (e_cal_model_get_component_kind (model) == I_CAL_VTODO_COMPONENT)
			set_status_task (comp_data, value);
		else
			e_cal_model_util_set_status (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_COMPLETED:
		set_completed (model, comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_COMPLETE:
		set_complete (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_DUE:
		set_due (model, comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_GEO:
		set_geo (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_PERCENT:
		set_percent (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_PRIORITY:
		set_priority (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_URL:
		set_url (comp_data, value);
		break;
	}

	if (save)
		e_cal_ops_modify_component (e_cal_model_get_data_model (model), comp_data->client, comp_data->icalcomp, mod, E_CAL_OPS_SEND_FLAG_DONT_SEND);
}

static gboolean
cal_model_test_component_editable (ECalModelComponent *comp_data)
{
	gboolean readonly;
	ECalClient *client = NULL;

	if (comp_data->client != NULL)
		client = g_object_ref (comp_data->client);

	readonly = (client == NULL);

	if (!readonly)
		readonly = e_client_is_readonly (E_CLIENT (client));

	g_clear_object (&client);

	return !readonly;
}

gboolean
e_cal_model_is_field_editable (ECalModel *model,
                               ECalModelComponent *comp_data,
                               gint col)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);
	g_return_val_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data), FALSE);
	g_return_val_if_fail (col >= 0 && col <= E_CAL_MODEL_FIELD_LAST, FALSE);

	if (!cal_model_test_component_editable (comp_data))
		return FALSE;

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES:
	case E_CAL_MODEL_FIELD_CLASSIFICATION:
	case E_CAL_MODEL_FIELD_DESCRIPTION:
	case E_CAL_MODEL_FIELD_DTSTART:
	case E_CAL_MODEL_FIELD_SUMMARY:
	case E_CAL_MODEL_FIELD_DTEND:
	case E_CAL_MODEL_FIELD_LOCATION:
	case E_CAL_MODEL_FIELD_TRANSPARENCY:
	case E_CAL_MODEL_FIELD_STATUS:
	case E_CAL_MODEL_FIELD_COMPLETED:
	case E_CAL_MODEL_FIELD_COMPLETE:
	case E_CAL_MODEL_FIELD_DUE:
	case E_CAL_MODEL_FIELD_GEO:
	case E_CAL_MODEL_FIELD_PERCENT:
	case E_CAL_MODEL_FIELD_PRIORITY:
	case E_CAL_MODEL_FIELD_URL:
		return TRUE;
	case E_CAL_MODEL_FIELD_ESTIMATED_DURATION:
		return FALSE;
	}

	return FALSE;
}

static void
cal_model_free_field_value (gint col,
                            gpointer value)
{
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES:
	case E_CAL_MODEL_FIELD_DESCRIPTION:
	case E_CAL_MODEL_FIELD_SUMMARY:
	case E_CAL_MODEL_FIELD_SOURCE:
		g_free (value);
		break;
	case E_CAL_MODEL_FIELD_CLASSIFICATION:
	case E_CAL_MODEL_FIELD_HAS_ALARMS:
	case E_CAL_MODEL_FIELD_ICON:
	case E_CAL_MODEL_FIELD_COLOR:
	case E_CAL_MODEL_FIELD_CANCELLED:
		break;
	case E_CAL_MODEL_FIELD_DTSTART:
	case E_CAL_MODEL_FIELD_CREATED:
	case E_CAL_MODEL_FIELD_LASTMODIFIED:
		if (value)
			e_date_edit_value_free (value);
		break;
	case E_CAL_MODEL_FIELD_COMPONENT:
		if (value)
			g_object_unref ((ICalComponent *) value);
		break;
	case E_CAL_MODEL_FIELD_DTEND:
	case E_CAL_MODEL_FIELD_COMPLETED:
	case E_CAL_MODEL_FIELD_DUE:
		if (value)
			e_date_edit_value_free (value);
		break;
	case E_CAL_MODEL_FIELD_LOCATION:
	case E_CAL_MODEL_FIELD_TRANSPARENCY:
	case E_CAL_MODEL_FIELD_STATUS:
	case E_CAL_MODEL_FIELD_GEO:
	case E_CAL_MODEL_FIELD_PRIORITY:
	case E_CAL_MODEL_FIELD_URL:
	case E_CAL_MODEL_FIELD_PERCENT:
	case E_CAL_MODEL_FIELD_COMPLETE:
	case E_CAL_MODEL_FIELD_OVERDUE:
		break;
	case E_CAL_MODEL_FIELD_ESTIMATED_DURATION:
		g_free (value);
		break;
	}
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

static gchar *
cal_model_build_key (const gchar *source_uid,
		     const gchar *uid,
		     const gchar *rid)
{
	return g_strdup_printf ("%s\n%s\n%s", source_uid ? source_uid : "", uid ? uid : "", rid ? rid : "");
}

static gchar *
cal_model_component_build_key (ECalModelComponent *comp_data)
{
	gchar *rid;
	gchar *key;

	rid = e_cal_util_component_get_recurid_as_string (comp_data->icalcomp);
	key = cal_model_build_key (comp_data->source_uid, i_cal_component_get_uid (comp_data->icalcomp), rid);

	g_free (rid);

	return key;
}

static gchar *
cal_model_component_get_related_to_parent_uid (ICalComponent *icalcomp)
{
	ICalProperty *prop, *next;
	ICalParameter *param;
	gboolean is_parent;
	const gchar *related_to;
	gchar *res;

	if (!icalcomp)
		return NULL;

	prop = i_cal_component_get_first_property (icalcomp, I_CAL_RELATEDTO_PROPERTY);

	while (prop) {
		is_parent = TRUE;
		param = i_cal_property_get_first_parameter (prop, I_CAL_RELTYPE_PARAMETER);

		if (param) {
			is_parent = i_cal_parameter_get_reltype (param) == I_CAL_RELTYPE_PARENT;
			g_object_unref (param);
		}

		if (is_parent) {
			related_to = i_cal_property_get_relatedto (prop);
			res = related_to && *related_to ? g_strdup (related_to) : NULL;
			g_object_unref (prop);

			return res;
		}

		next = i_cal_component_get_next_property (icalcomp, I_CAL_RELATEDTO_PROPERTY);
		g_object_unref (prop);
		prop = next;
	}

	return NULL;
}

static void
cal_model_rewrite_related_to (ECalModelComponent *comp_data,
			      ECalModelComponent *new_parent)
{
	ICalProperty *prop, *next;
	ICalParameter *param;
	gboolean is_parent;
	const gchar *new_parent_uid;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_RELATEDTO_PROPERTY);

	while (prop) {
		is_parent = TRUE;
		param = i_cal_property_get_first_parameter (prop, I_CAL_RELTYPE_PARAMETER);

		if (param) {
			is_parent = i_cal_parameter_get_reltype (param) == I_CAL_RELTYPE_PARENT;
			g_object_unref (param);
		}

		next = i_cal_component_get_next_property (comp_data->icalcomp, I_CAL_RELATEDTO_PROPERTY);

		if (is_parent)
			i_cal_component_remove_property (comp_data->icalcomp, prop);

		g_object_unref (prop);
		prop = next;
	}

	if (new_parent) {
		new_parent_uid = i_cal_component_get_uid (new_parent->icalcomp);
		i_cal_component_take_property (comp_data->icalcomp, i_cal_property_new_relatedto (new_parent_uid));
	}
}

static gboolean
cal_model_would_form_cycle (ECalModel *model,
			    ECalModelComponent *comp_data,
			    ECalModelComponent *candidate_parent)
{
	ECalModelComponent *walk;
	GHashTable *visited;
	gchar *raw_target;
	gchar *key;
	gboolean found_cycle = FALSE;

	visited = g_hash_table_new (g_direct_hash, g_direct_equal);
	walk = candidate_parent;

	while (walk) {
		if (walk == comp_data) {
			found_cycle = TRUE;
			break;
		}

		if (g_hash_table_contains (visited, walk))
			break;

		g_hash_table_add (visited, walk);

		if (walk->resolved_parent) {
			walk = walk->resolved_parent;
			continue;
		}

		raw_target = cal_model_component_get_related_to_parent_uid (walk->icalcomp);

		if (!raw_target)
			break;

		key = cal_model_build_key (walk->source_uid, raw_target, "");
		walk = g_hash_table_lookup (model->priv->uid_to_row, key);

		g_free (key);
		g_free (raw_target);
	}

	g_hash_table_destroy (visited);

	return found_cycle;
}

static GPtrArray *
cal_model_children_array_for (ECalModel *model,
			      ECalModelComponent *parent_or_null)
{
	if (!parent_or_null)
		return model->priv->roots;

	if (!parent_or_null->resolved_children)
		parent_or_null->resolved_children = g_ptr_array_new ();

	return parent_or_null->resolved_children;
}

static gint
cal_model_compare_field_values (ECalModel *model,
				ECalModelComponent *comp_a,
				ECalModelComponent *comp_b,
				ECalModelField field)
{
	gpointer value_a, value_b;
	ICalTime *time_a, *time_b;
	gint64 dur_a, dur_b;
	gint cmp;

	value_a = e_cal_model_get_field_value (model, comp_a, field);
	value_b = e_cal_model_get_field_value (model, comp_b, field);

	switch (field) {
	case E_CAL_MODEL_FIELD_ESTIMATED_DURATION:
		dur_a = value_a ? *((gint64 *) value_a) : 0;
		dur_b = value_b ? *((gint64 *) value_b) : 0;
		cmp = (dur_a > dur_b) - (dur_a < dur_b);
		break;
	case E_CAL_MODEL_FIELD_DTSTART:
	case E_CAL_MODEL_FIELD_CREATED:
	case E_CAL_MODEL_FIELD_LASTMODIFIED:
	case E_CAL_MODEL_FIELD_DTEND:
	case E_CAL_MODEL_FIELD_DUE:
	case E_CAL_MODEL_FIELD_COMPLETED:
		time_a = value_a ? e_date_edit_value_get_time (value_a) : NULL;
		time_b = value_b ? e_date_edit_value_get_time (value_b) : NULL;

		if (time_a && time_b)
			cmp = i_cal_time_compare (time_a, time_b);
		else
			cmp = (time_a ? 1 : 0) - (time_b ? 1 : 0);
		break;
	case E_CAL_MODEL_FIELD_HAS_ALARMS:
	case E_CAL_MODEL_FIELD_ICON:
	case E_CAL_MODEL_FIELD_CANCELLED:
	case E_CAL_MODEL_FIELD_COMPLETE:
	case E_CAL_MODEL_FIELD_OVERDUE:
	case E_CAL_MODEL_FIELD_PERCENT:
	case E_CAL_MODEL_FIELD_STRIKEOUT:
		cmp = GPOINTER_TO_INT (value_a) - GPOINTER_TO_INT (value_b);
		break;
	default:
		cmp = g_strcmp0 ((const gchar *) value_a, (const gchar *) value_b);
		break;
	}

	cal_model_free_field_value (field, value_a);
	cal_model_free_field_value (field, value_b);

	return cmp;
}

static gint
cal_model_component_compare (gconstpointer aa,
			     gconstpointer bb,
			     gpointer user_data)
{
	ECalModel *model = user_data;
	ECalModelComponent *comp_a = *(ECalModelComponent * const *) aa;
	ECalModelComponent *comp_b = *(ECalModelComponent * const *) bb;
	guint ii;
	gint cmp = 0;
	const gchar *uid_a, *uid_b;

	for (ii = 0; ii < model->priv->n_sort_columns && cmp == 0; ii++) {
		cmp = cal_model_compare_field_values (model, comp_a, comp_b, model->priv->sort_columns[ii].field);

		if (model->priv->sort_columns[ii].order == GTK_SORT_DESCENDING)
			cmp = -cmp;
	}

	if (cmp == 0) {
		uid_a = i_cal_component_get_uid (comp_a->icalcomp);
		uid_b = i_cal_component_get_uid (comp_b->icalcomp);
		cmp = g_strcmp0 (uid_a, uid_b);
	}

	return cmp;
}

static void
cal_model_sorted_insert (ECalModel *model,
			 GPtrArray *array,
			 ECalModelComponent *comp_data)
{
	guint low, high, mid;
	gint cmp;

	low = 0;
	high = array->len;

	while (low < high) {
		mid = low + (high - low) / 2;
		cmp = cal_model_component_compare (&comp_data, &g_ptr_array_index (array, mid), model);

		if (cmp < 0)
			high = mid;
		else
			low = mid + 1;
	}

	g_ptr_array_insert (array, low, comp_data);
}

static void
cal_model_place_component (ECalModel *model,
			   ECalModelComponent *comp_data,
			   const gchar *target_uid)
{
	ECalModelComponent *parent = NULL;
	gchar *parent_key;
	GPtrArray *waiting;

	if (target_uid && *target_uid) {
		parent_key = cal_model_build_key (comp_data->source_uid, target_uid, "");
		parent = g_hash_table_lookup (model->priv->uid_to_row, parent_key);

		if (parent && g_strcmp0 (parent->source_uid, comp_data->source_uid) != 0)
			parent = NULL;

		if (parent && cal_model_would_form_cycle (model, comp_data, parent))
			parent = NULL;

		if (!parent) {
			waiting = g_hash_table_lookup (model->priv->waiting_children, parent_key);

			if (!waiting) {
				waiting = g_ptr_array_new ();
				g_hash_table_insert (model->priv->waiting_children, g_strdup (parent_key), waiting);
			}

			g_ptr_array_add (waiting, comp_data);
		}

		g_free (parent_key);
	}

	comp_data->resolved_parent = parent;
	cal_model_sorted_insert (model, cal_model_children_array_for (model, parent), comp_data);
}

static void
cal_model_reclaim_waiting (ECalModel *model,
			   ECalModelComponent *comp_data)
{
	GPtrArray *waiting;
	GPtrArray *attached;
	guint ii;
	ECalModelComponent *child;

	waiting = g_hash_table_lookup (model->priv->waiting_children, comp_data->priv->own_key);

	if (!waiting)
		return;

	attached = g_ptr_array_new ();

	for (ii = 0; ii < waiting->len; ii++) {
		child = g_ptr_array_index (waiting, ii);

		if (cal_model_would_form_cycle (model, child, comp_data))
			continue;

		g_ptr_array_remove (model->priv->roots, child);
		child->resolved_parent = comp_data;
		cal_model_sorted_insert (model, cal_model_children_array_for (model, comp_data), child);
		g_ptr_array_add (attached, child);
	}

	g_hash_table_remove (model->priv->waiting_children, comp_data->priv->own_key);

	for (ii = 0; ii < attached->len; ii++) {
		cal_model_reclaim_waiting (model, g_ptr_array_index (attached, ii));
	}

	g_ptr_array_unref (attached);
}

static void
cal_model_attach_component (ECalModel *model,
			    ECalModelComponent *comp_data,
			    const gchar *target_uid)
{
	cal_model_place_component (model, comp_data, target_uid);
	cal_model_reclaim_waiting (model, comp_data);
}

static void
cal_model_detach_component (ECalModel *model,
			    ECalModelComponent *comp_data)
{
	gchar *target_uid;
	gchar *parent_key;
	GPtrArray *waiting;

	if (comp_data->resolved_parent) {
		g_ptr_array_remove (comp_data->resolved_parent->resolved_children, comp_data);
		comp_data->resolved_parent = NULL;
		return;
	}

	g_ptr_array_remove (model->priv->roots, comp_data);

	target_uid = cal_model_component_get_related_to_parent_uid (comp_data->icalcomp);

	if (target_uid) {
		parent_key = cal_model_build_key (comp_data->source_uid, target_uid, "");
		waiting = g_hash_table_lookup (model->priv->waiting_children, parent_key);

		if (waiting)
			g_ptr_array_remove (waiting, comp_data);

		g_free (parent_key);
		g_free (target_uid);
	}
}

static void
cal_model_reposition (ECalModel *model,
		      ECalModelComponent *comp_data)
{
	GPtrArray *array;

	array = cal_model_children_array_for (model, comp_data->resolved_parent);

	g_ptr_array_remove (array, comp_data);
	cal_model_sorted_insert (model, array, comp_data);
}

static void
cal_model_remove_component_from_tree (ECalModel *model,
				      ECalModelComponent *comp_data)
{
	GPtrArray *children;
	GPtrArray *waiting;
	guint ii;
	ECalModelComponent *child;

	cal_model_detach_component (model, comp_data);

	children = comp_data->resolved_children;

	if (!children || children->len == 0)
		return;

	waiting = g_hash_table_lookup (model->priv->waiting_children, comp_data->priv->own_key);

	if (!waiting) {
		waiting = g_ptr_array_new ();
		g_hash_table_insert (model->priv->waiting_children, g_strdup (comp_data->priv->own_key), waiting);
	}

	for (ii = 0; ii < children->len; ii++) {
		child = g_ptr_array_index (children, ii);

		child->resolved_parent = NULL;
		cal_model_sorted_insert (model, model->priv->roots, child);
		g_ptr_array_add (waiting, child);
	}

	g_ptr_array_set_size (children, 0);
}

static void
cal_model_resort_recursive (ECalModel *model,
			    GPtrArray *array)
{
	guint ii;
	ECalModelComponent *child;

	g_ptr_array_sort_with_data (array, cal_model_component_compare, model);

	for (ii = 0; ii < array->len; ii++) {
		child = g_ptr_array_index (array, ii);

		if (child->resolved_children && child->resolved_children->len > 0)
			cal_model_resort_recursive (model, child->resolved_children);
	}
}

static void
cal_model_resort_all (ECalModel *model)
{
	cal_model_resort_recursive (model, model->priv->roots);
}

static void
cal_model_flatten_recursive (ECalModel *model,
			     ECalModelComponent *comp_data)
{
	gboolean expanded;
	guint ii;

	g_ptr_array_add (model->priv->visible_rows, comp_data);
	g_hash_table_insert (model->priv->component_to_visible_index, comp_data,
		GUINT_TO_POINTER (model->priv->visible_rows->len - 1));

	if (!comp_data->resolved_children || comp_data->resolved_children->len == 0)
		return;

	expanded = !g_hash_table_contains (model->priv->collapsed_uids, comp_data->priv->own_key);

	if (!expanded)
		return;

	for (ii = 0; ii < comp_data->resolved_children->len; ii++) {
		cal_model_flatten_recursive (model, g_ptr_array_index (comp_data->resolved_children, ii));
	}
}

static void
cal_model_flatten_recursive_all (ECalModel *model,
				 ECalModelComponent *comp_data,
				 GPtrArray *out)
{
	guint ii;

	g_ptr_array_add (out, comp_data);

	if (!comp_data->resolved_children)
		return;

	for (ii = 0; ii < comp_data->resolved_children->len; ii++) {
		cal_model_flatten_recursive_all (model, g_ptr_array_index (comp_data->resolved_children, ii), out);
	}
}

static void
cal_model_ensure_visible_rows (ECalModel *model)
{
	guint ii;

	if (!model->priv->visible_dirty)
		return;

	/* Emit before-rebuild while visible_rows is still the old, pre-sort data. */
	model->priv->visible_dirty = FALSE;

	e_virtual_tree_model_emit_before_rebuild (E_VIRTUAL_TREE_MODEL (model));

	g_ptr_array_set_size (model->priv->visible_rows, 0);
	g_hash_table_remove_all (model->priv->component_to_visible_index);

	for (ii = 0; ii < model->priv->roots->len; ii++) {
		cal_model_flatten_recursive (model, g_ptr_array_index (model->priv->roots, ii));
	}

	e_virtual_tree_model_emit_after_rebuild (E_VIRTUAL_TREE_MODEL (model));
	e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (model));
}

static void
cal_model_untrack_component (ECalModel *model,
			     ECalModelComponent *comp_data)
{
	cal_model_remove_component_from_tree (model, comp_data);

	g_hash_table_remove (model->priv->uid_to_row, comp_data->priv->own_key);

	model->priv->visible_dirty = TRUE;
}

static guint
cal_model_virtual_tree_model_get_row_count (EVirtualTreeModel *self)
{
	ECalModel *model = E_CAL_MODEL (self);

	cal_model_ensure_visible_rows (model);

	return model->priv->visible_rows->len;
}

static GPtrArray *
cal_model_virtual_tree_model_dup_rows (EVirtualTreeModel *self,
				       guint first_row,
				       guint last_row,
				       gboolean include_collapsed)
{
	ECalModel *model = E_CAL_MODEL (self);
	GPtrArray *result;
	GPtrArray *source;
	GPtrArray *full = NULL;
	guint ii;

	cal_model_ensure_visible_rows (model);

	if (include_collapsed) {
		full = g_ptr_array_new ();

		for (ii = 0; ii < model->priv->roots->len; ii++) {
			cal_model_flatten_recursive_all (model, g_ptr_array_index (model->priv->roots, ii), full);
		}

		source = full;
	} else {
		source = model->priv->visible_rows;
	}

	result = g_ptr_array_new_with_free_func (g_object_unref);

	for (ii = first_row; ii <= last_row && ii < source->len; ii++) {
		g_ptr_array_add (result, g_object_ref (g_ptr_array_index (source, ii)));
	}

	g_clear_pointer (&full, g_ptr_array_unref);

	return result;
}

static guint
cal_model_virtual_tree_model_get_depth (EVirtualTreeModel *self,
					GObject *row_object)
{
	ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT (row_object);
	guint depth = 0;

	while (comp_data->resolved_parent) {
		depth++;
		comp_data = comp_data->resolved_parent;
	}

	return depth;
}

static gboolean
cal_model_virtual_tree_model_is_expandable (EVirtualTreeModel *self,
					    GObject *row_object)
{
	ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT (row_object);

	return comp_data->resolved_children && comp_data->resolved_children->len > 0;
}

static gboolean
cal_model_virtual_tree_model_get_expanded (EVirtualTreeModel *self,
					   GObject *row_object)
{
	ECalModel *model = E_CAL_MODEL (self);
	ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT (row_object);

	return !g_hash_table_contains (model->priv->collapsed_uids, comp_data->priv->own_key);
}

static void
cal_model_virtual_tree_model_set_expanded (EVirtualTreeModel *self,
					   GObject *row_object,
					   gboolean expanded)
{
	e_cal_model_set_row_expanded (E_CAL_MODEL (self), E_CAL_MODEL_COMPONENT (row_object), expanded);
}

static gconstpointer
cal_model_virtual_tree_model_get_row_key (EVirtualTreeModel *self,
					  GObject *row_object)
{
	return E_CAL_MODEL_COMPONENT (row_object)->priv->own_key;
}

static EVirtualTreeKeyType
cal_model_virtual_tree_model_get_key_type (EVirtualTreeModel *self)
{
	EVirtualTreeKeyType key_type;

	key_type.hash_func = g_str_hash;
	key_type.equal_func = g_str_equal;
	key_type.copy_func = (GBoxedCopyFunc) g_strdup;
	key_type.free_func = g_free;

	return key_type;
}

static guint
cal_model_virtual_tree_model_find_row_by_key (EVirtualTreeModel *self,
					      gconstpointer key)
{
	ECalModel *model = E_CAL_MODEL (self);
	ECalModelComponent *comp_data;
	gpointer index_ptr;

	cal_model_ensure_visible_rows (model);

	comp_data = g_hash_table_lookup (model->priv->uid_to_row, key);

	if (!comp_data)
		return (guint) -1;

	if (!g_hash_table_lookup_extended (model->priv->component_to_visible_index, comp_data, NULL, &index_ptr))
		return (guint) -1;

	return GPOINTER_TO_UINT (index_ptr);
}

static void
e_cal_model_virtual_tree_model_init (EVirtualTreeModelInterface *iface)
{
	iface->get_row_count = cal_model_virtual_tree_model_get_row_count;
	iface->dup_rows = cal_model_virtual_tree_model_dup_rows;
	iface->get_depth = cal_model_virtual_tree_model_get_depth;
	iface->is_expandable = cal_model_virtual_tree_model_is_expandable;
	iface->get_expanded = cal_model_virtual_tree_model_get_expanded;
	iface->set_expanded = cal_model_virtual_tree_model_set_expanded;
	iface->get_row_key = cal_model_virtual_tree_model_get_row_key;
	iface->get_key_type = cal_model_virtual_tree_model_get_key_type;
	iface->find_row_by_key = cal_model_virtual_tree_model_find_row_by_key;
}

static void
cal_model_data_subscriber_component_added_or_modified (ECalDataModelSubscriber *subscriber,
						       ECalClient *client,
						       ECalComponent *comp,
						       gboolean is_added)
{
	ECalModel *model;
	ECalModelComponent *comp_data;
	ECalComponentId *id;
	ICalComponent *icomp;
	gchar *old_target, *new_target;
	gint index;

	model = E_CAL_MODEL (subscriber);

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
					cal_model_untrack_component (model, comp_data);

					g_ptr_array_remove_index (model->priv->objects, index);
					removed_comps = g_slist_prepend (removed_comps, comp_data);

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
		comp_data = g_object_new (E_TYPE_CAL_MODEL_COMPONENT, NULL);
		comp_data->is_new_component = FALSE;
		comp_data->client = g_object_ref (client);
		comp_data->source_uid = g_strdup (e_source_get_uid (e_client_get_source (E_CLIENT (client))));
		comp_data->icalcomp = icomp;
		e_cal_model_set_instance_times (comp_data, model->priv->zone);
		g_ptr_array_add (model->priv->objects, comp_data);

		comp_data->priv->own_key = cal_model_component_build_key (comp_data);
		g_hash_table_insert (model->priv->uid_to_row, comp_data->priv->own_key, comp_data);

		new_target = cal_model_component_get_related_to_parent_uid (icomp);
		cal_model_attach_component (model, comp_data, new_target);
		g_free (new_target);

		model->priv->visible_dirty = TRUE;
	} else {
		comp_data = g_ptr_array_index (model->priv->objects, index);

		old_target = cal_model_component_get_related_to_parent_uid (comp_data->icalcomp);

		e_cal_model_component_set_icalcomponent (comp_data, model, icomp);

		new_target = cal_model_component_get_related_to_parent_uid (comp_data->icalcomp);

		if (g_strcmp0 (old_target, new_target) == 0) {
			cal_model_reposition (model, comp_data);
		} else {
			cal_model_detach_component (model, comp_data);
			cal_model_place_component (model, comp_data, new_target);
		}

		g_free (old_target);
		g_free (new_target);

		model->priv->visible_dirty = TRUE;
	}

	e_virtual_tree_model_notify_row_changed (E_VIRTUAL_TREE_MODEL (model), comp_data->priv->own_key, index < 0);
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
	ECalComponentId *id;
	GSList *link;
	gint index;

	model = E_CAL_MODEL (subscriber);

	id = e_cal_component_id_new (uid, rid);

	index = e_cal_model_get_component_index (model, client, id);

	e_cal_component_id_free (id);

	if (index < 0)
		return;

	comp_data = g_ptr_array_remove_index (model->priv->objects, index);
	if (!comp_data)
		return;

	cal_model_untrack_component (model, comp_data);

	link = g_slist_append (NULL, comp_data);
	g_signal_emit (model, signals[COMPS_DELETED], 0, link);

	g_slist_free (link);
	g_object_unref (comp_data);

	e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (model));
}

static void
e_cal_model_data_subscriber_freeze (ECalDataModelSubscriber *subscriber)
{
}

static void
e_cal_model_data_subscriber_thaw (ECalDataModelSubscriber *subscriber)
{
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
		PROP_COMPONENT_KIND,
		g_param_spec_int (
			"component-kind",
			"Component Kind",
			NULL,
			G_MININT,
			G_MAXINT,
			I_CAL_NO_COMPONENT,
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

	model->priv->uid_to_row = g_hash_table_new (g_str_hash, g_str_equal);
	model->priv->waiting_children = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);
	model->priv->roots = g_ptr_array_new ();
	model->priv->visible_rows = g_ptr_array_new ();
	model->priv->component_to_visible_index = g_hash_table_new (g_direct_hash, g_direct_equal);
	model->priv->collapsed_uids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	model->priv->visible_dirty = TRUE;

	model->priv->highlight_due_today = TRUE;
	model->priv->highlight_overdue = TRUE;
}

/* updates time in a component, and keeps the timezone used in it, if exists */
void
e_cal_model_update_comp_time (ECalModel *model,
                              ECalModelComponent *comp_data,
                              gconstpointer time_value,
                              ICalPropertyKind kind,
                              ECalModelTimeSetFuncType set_func,
                              ECalModelTimeNewFuncType new_func)
{
	EDateEditValue *dv = (EDateEditValue *) time_value;
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
	tt = e_date_edit_value_get_time (dv);
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

ECalModel *
e_cal_model_new (ECalDataModel *data_model,
		 ESourceRegistry *registry,
		 EShell *shell,
		 ICalComponentKind kind)
{
	g_return_val_if_fail (E_IS_CAL_DATA_MODEL (data_model), NULL);
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return g_object_new (
		E_TYPE_CAL_MODEL,
		"data-model", data_model,
		"registry", registry,
		"shell", shell,
		"component-kind", (gint) kind,
		NULL);
}

ECalModel *
e_cal_model_new_events (ECalDataModel *data_model,
			ESourceRegistry *registry,
			EShell *shell)
{
	return e_cal_model_new (data_model, registry, shell, I_CAL_VEVENT_COMPONENT);
}

ECalModel *
e_cal_model_new_memos (ECalDataModel *data_model,
		       ESourceRegistry *registry,
		       EShell *shell)
{
	return e_cal_model_new (data_model, registry, shell, I_CAL_VJOURNAL_COMPONENT);
}

ECalModel *
e_cal_model_new_tasks (ECalDataModel *data_model,
		       ESourceRegistry *registry,
		       EShell *shell)
{
	return e_cal_model_new (data_model, registry, shell, I_CAL_VTODO_COMPONENT);
}

static void
cal_model_set_component_kind (ECalModel *model,
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
	guint row_count;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->zone == zone)
		return;

	old_zone = model->priv->zone;
	model->priv->zone = zone ? e_cal_util_copy_timezone (zone) : NULL;

	/* the timezone affects the times shown for date fields,
	 * so we need to redisplay everything */
	row_count = e_virtual_tree_model_get_row_count (E_VIRTUAL_TREE_MODEL (model));
	if (row_count > 0)
		e_virtual_tree_model_emit_rows_changed (E_VIRTUAL_TREE_MODEL (model), 0, row_count - 1);

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
	guint row_count;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->use_24_hour_format == use_24_hour_format)
		return;

	model->priv->use_24_hour_format = use_24_hour_format;

	/* Get the views to redraw themselves. */
	row_count = e_virtual_tree_model_get_row_count (E_VIRTUAL_TREE_MODEL (model));
	if (row_count > 0)
		e_virtual_tree_model_emit_rows_changed (E_VIRTUAL_TREE_MODEL (model), 0, row_count - 1);

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
	GSList *comps = NULL;
	guint ii;

	for (ii = 0; ii < model->priv->objects->len; ii++) {
		comp_data = g_ptr_array_index (model->priv->objects, ii);

		if (comp_data) {
			comps = g_slist_prepend (comps, comp_data);
			cal_model_untrack_component (model, comp_data);
		}
	}

	g_ptr_array_set_size (model->priv->objects, 0);

	cal_model_ensure_visible_rows (model);

	if (comps)
		g_signal_emit (model, signals[COMPS_DELETED], 0, comps);

	g_slist_free_full (comps, g_object_unref);
}

void
e_cal_model_remove_component (ECalModel *model,
			      ECalClient *client,
			      const gchar *uid,
			      const gchar *rid)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL_CLIENT (client));

	e_cal_model_data_subscriber_component_removed (E_CAL_DATA_MODEL_SUBSCRIBER (model), client, uid, rid);
}

void
e_cal_model_add_component (ECalModel *model,
			   ECalClient *client,
			   ICalComponent *icalcomp)
{
	ECalComponent *comp;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (icalcomp != NULL);

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icalcomp));
	if (!comp)
		return;

	cal_model_data_subscriber_component_added_or_modified (E_CAL_DATA_MODEL_SUBSCRIBER (model), client, comp, TRUE);

	g_object_unref (comp);
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
		case I_CAL_VEVENT_COMPONENT:
			comp = cal_comp_event_new_with_defaults_sync (
				client, all_day,
				e_cal_model_get_use_default_reminder (model),
				e_cal_model_get_default_reminder_interval (model),
				e_cal_model_get_default_reminder_units (model),
				cancellable, error);
			break;
		case I_CAL_VTODO_COMPONENT:
			comp = cal_comp_task_new_with_defaults_sync (client, cancellable, error);
			break;
		case I_CAL_VJOURNAL_COMPONENT:
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
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (comp_data != NULL, NULL);

	return cal_model_get_color_for_component (model, comp_data);
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
	EDateEditValue *dv = (EDateEditValue *) value;
	ICalTime *itt;
	struct tm tmp_tm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), g_strdup (""));

	priv = model->priv;

	if (!dv)
		return g_strdup ("");

	itt = e_date_edit_value_get_time (dv);

	/* We currently convert all the dates to the current timezone. */
	tmp_tm = e_cal_util_icaltime_to_tm_with_zone (itt, e_date_edit_value_get_zone (dv), priv->zone);

	return e_datetime_format_format_tm ("calendar", "table",
		i_cal_time_is_date (itt) ? DTFormatKindDate : DTFormatKindDateTime, &tmp_tm);
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

	n = model->priv->objects->len;
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

EDateEditValue *
e_cal_model_util_get_datetime_value (ECalModel *model,
				     ECalModelComponent *comp_data,
				     ICalPropertyKind kind,
				     ECalModelTimeGetFuncType get_time_func)
{
	EDateEditValue *value;
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

	value = e_date_edit_value_new_take (tt, zone ? e_cal_util_copy_timezone (zone) : NULL);

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

guint
e_cal_model_get_visible_row_count (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), 0);

	cal_model_ensure_visible_rows (model);

	return model->priv->visible_rows->len;
}

ECalModelComponent *
e_cal_model_get_visible_row (ECalModel *model,
			     guint visible_row_index,
			     guint *out_depth,
			     gboolean *out_expandable)
{
	ECalModelComponent *comp_data;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	cal_model_ensure_visible_rows (model);

	if (visible_row_index >= model->priv->visible_rows->len)
		return NULL;

	comp_data = g_ptr_array_index (model->priv->visible_rows, visible_row_index);

	if (out_depth)
		*out_depth = cal_model_virtual_tree_model_get_depth (E_VIRTUAL_TREE_MODEL (model), G_OBJECT (comp_data));

	if (out_expandable)
		*out_expandable = cal_model_virtual_tree_model_is_expandable (E_VIRTUAL_TREE_MODEL (model), G_OBJECT (comp_data));

	return comp_data;
}

gboolean
e_cal_model_get_row_expanded (ECalModel *model,
			      ECalModelComponent *comp_data)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);
	g_return_val_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data), FALSE);

	return !g_hash_table_contains (model->priv->collapsed_uids, comp_data->priv->own_key);
}

void
e_cal_model_set_row_expanded (ECalModel *model,
			      ECalModelComponent *comp_data,
			      gboolean expanded)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));

	if (expanded)
		g_hash_table_remove (model->priv->collapsed_uids, comp_data->priv->own_key);
	else
		g_hash_table_add (model->priv->collapsed_uids, g_strdup (comp_data->priv->own_key));

	model->priv->visible_dirty = TRUE;

	cal_model_ensure_visible_rows (model);
}

guint
e_cal_model_find_visible_row_for_component (ECalModel *model,
					    ECalClient *client,
					    const gchar *uid,
					    const gchar *rid)
{
	gchar *source_uid;
	gchar *key;
	guint result;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), (guint) -1);

	source_uid = client ? g_strdup (e_source_get_uid (e_client_get_source (E_CLIENT (client)))) : NULL;
	key = cal_model_build_key (source_uid, uid, rid);

	result = cal_model_virtual_tree_model_find_row_by_key (E_VIRTUAL_TREE_MODEL (model), key);

	g_free (key);
	g_free (source_uid);

	return result;
}

void
e_cal_model_set_sort_columns (ECalModel *model,
			      const ECalModelSortColumn *columns,
			      guint n_columns)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	g_free (model->priv->sort_columns);
	model->priv->sort_columns = NULL;
	model->priv->n_sort_columns = n_columns;

	if (n_columns) {
		model->priv->sort_columns = g_new (ECalModelSortColumn, n_columns);
		memcpy (model->priv->sort_columns, columns, sizeof (ECalModelSortColumn) * n_columns);
	}

	cal_model_resort_all (model);

	model->priv->visible_dirty = TRUE;

	cal_model_ensure_visible_rows (model);
}

gboolean
e_cal_model_get_reparent_by_dnd (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);

	return model->priv->reparent_by_dnd;
}

void
e_cal_model_set_reparent_by_dnd (ECalModel *model,
				 gboolean reparent_by_dnd)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	model->priv->reparent_by_dnd = reparent_by_dnd;
}

gboolean
e_cal_model_can_reparent_component (ECalModel *model,
				    ECalModelComponent *comp_data,
				    ECalModelComponent *new_parent)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);
	g_return_val_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data), FALSE);

	if (!model->priv->reparent_by_dnd)
		return FALSE;

	if (comp_data == new_parent)
		return FALSE;

	if (new_parent && g_strcmp0 (comp_data->source_uid, new_parent->source_uid) != 0)
		return FALSE;

	if (new_parent && cal_model_would_form_cycle (model, comp_data, new_parent))
		return FALSE;

	return TRUE;
}

void
e_cal_model_reparent_component (ECalModel *model,
				ECalModelComponent *comp_data,
				ECalModelComponent *new_parent)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));

	if (!e_cal_model_can_reparent_component (model, comp_data, new_parent))
		return;

	cal_model_rewrite_related_to (comp_data, new_parent);

	cal_model_detach_component (model, comp_data);
	cal_model_place_component (model, comp_data, new_parent ? i_cal_component_get_uid (new_parent->icalcomp) : NULL);

	model->priv->visible_dirty = TRUE;

	e_cal_model_modify_component (model, comp_data, E_CAL_OBJ_MOD_THIS);
}

static gboolean
cal_model_is_complete (ECalModelComponent *comp_data)
{
	ICalProperty *prop;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_COMPLETED_PROPERTY);
	if (prop) {
		g_object_unref (prop);
		return TRUE;
	}

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_PERCENTCOMPLETE_PROPERTY);
	if (prop && i_cal_property_get_percentcomplete (prop) == 100) {
		g_object_unref (prop);
		return TRUE;
	}

	g_clear_object (&prop);

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_STATUS_PROPERTY);
	if (prop && i_cal_property_get_status (prop) == I_CAL_STATUS_COMPLETED) {
		g_object_unref (prop);
		return TRUE;
	}

	g_clear_object (&prop);

	return FALSE;
}

static gboolean
cal_model_is_status_canceled (ECalModelComponent *comp_data)
{
	ICalProperty *prop;
	gboolean res;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_STATUS_PROPERTY);

	res = prop && i_cal_property_get_status (prop) == I_CAL_STATUS_CANCELLED;

	g_clear_object (&prop);

	return res;
}

static ECalModelDueStatus
cal_model_get_due_status (ECalModel *model,
			  ECalModelComponent *comp_data)
{
	ICalProperty *prop;

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_DUE_PROPERTY);
	if (!prop) {
		return E_CAL_MODEL_DUE_NEVER;
	} else {
		ICalTime *now_tt, *due_tt;
		ICalTimezone *zone = NULL;

		if (cal_model_is_complete (comp_data)) {
			g_object_unref (prop);
			return E_CAL_MODEL_DUE_COMPLETE;
		}

		due_tt = i_cal_property_get_due (prop);
		if (i_cal_time_is_date (due_tt)) {
			gint cmp;

			zone = e_cal_model_get_timezone (model);

			i_cal_time_adjust (due_tt, -1, 0, 0, 0);

			now_tt = i_cal_time_new_current_with_zone (zone);
			cmp = i_cal_time_compare_date_only_tz (due_tt, now_tt, zone);

			g_object_unref (now_tt);
			g_object_unref (due_tt);
			g_object_unref (prop);

			if (cmp < 0)
				return E_CAL_MODEL_DUE_OVERDUE;
			else if (cmp == 0)
				return E_CAL_MODEL_DUE_TODAY;
			else
				return E_CAL_MODEL_DUE_FUTURE;
		} else {
			ECalModelDueStatus res;
			ICalParameter *param;
			ICalTimezone *local_zone;

			param = i_cal_property_get_first_parameter (prop, I_CAL_TZID_PARAMETER);

			if (param) {
				const gchar *tzid;

				tzid = i_cal_parameter_get_tzid (param);
				if (!e_cal_client_get_timezone_sync (comp_data->client, tzid, &zone, NULL, NULL))
					zone = NULL;

				g_object_unref (param);
			}

			g_object_unref (prop);

			local_zone = e_cal_model_get_timezone (model);

			if (!zone) {
				if (i_cal_time_is_utc (due_tt))
					zone = i_cal_timezone_get_utc_timezone ();
				else
					zone = local_zone;
			}

			now_tt = i_cal_time_new_current_with_zone (local_zone);
			i_cal_time_set_timezone (now_tt, local_zone);
			i_cal_time_set_timezone (due_tt, zone);

			if (i_cal_time_compare (due_tt, now_tt) <= 0)
				res = E_CAL_MODEL_DUE_OVERDUE;
			else if (i_cal_time_compare_date_only_tz (due_tt, now_tt, local_zone) == 0)
				res = E_CAL_MODEL_DUE_TODAY;
			else
				res = E_CAL_MODEL_DUE_FUTURE;

			g_clear_object (&due_tt);
			g_clear_object (&now_tt);

			return res;
		}
	}
}

static gboolean
cal_model_is_overdue (ECalModel *model,
		      ECalModelComponent *comp_data)
{
	switch (cal_model_get_due_status (model, comp_data)) {
	case E_CAL_MODEL_DUE_NEVER:
	case E_CAL_MODEL_DUE_FUTURE:
	case E_CAL_MODEL_DUE_COMPLETE:
		return FALSE;
	case E_CAL_MODEL_DUE_TODAY:
	case E_CAL_MODEL_DUE_OVERDUE:
		return TRUE;
	}

	return FALSE;
}

static void
cal_model_ensure_task_complete (ECalModelComponent *comp_data,
				time_t completed_date)
{
	e_cal_util_mark_task_complete_sync (comp_data->icalcomp, completed_date,
		comp_data->client, NULL, NULL);
}

static void
cal_model_ensure_task_partially_complete (ECalModelComponent *comp_data)
{
	ICalProperty *prop;

	e_cal_util_component_remove_property_by_kind (comp_data->icalcomp, I_CAL_COMPLETED_PROPERTY, TRUE);

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_PERCENTCOMPLETE_PROPERTY);
	if (!prop)
		i_cal_component_take_property (comp_data->icalcomp, i_cal_property_new_percentcomplete (50));
	else if (i_cal_property_get_percentcomplete (prop) == 0 || i_cal_property_get_percentcomplete (prop) == 100)
		i_cal_property_set_percentcomplete (prop, 50);
	g_clear_object (&prop);

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_STATUS_PROPERTY);
	if (prop)
		i_cal_property_set_status (prop, I_CAL_STATUS_INPROCESS);
	else
		i_cal_component_take_property (comp_data->icalcomp, i_cal_property_new_status (I_CAL_STATUS_INPROCESS));
	g_clear_object (&prop);
}

static void
cal_model_ensure_task_not_complete (ECalModelComponent *comp_data,
				    gboolean with_status)
{
	ICalProperty *prop;

	e_cal_util_component_remove_property_by_kind (comp_data->icalcomp, I_CAL_COMPLETED_PROPERTY, TRUE);
	e_cal_util_component_remove_property_by_kind (comp_data->icalcomp, I_CAL_PERCENTCOMPLETE_PROPERTY, TRUE);

	if (with_status) {
		prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_STATUS_PROPERTY);
		if (prop) {
			i_cal_property_set_status (prop, I_CAL_STATUS_NEEDSACTION);
			g_object_unref (prop);
		}
	}
}

gboolean
e_cal_model_get_highlight_due_today (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);

	return model->priv->highlight_due_today;
}

void
e_cal_model_set_highlight_due_today (ECalModel *model,
				     gboolean highlight)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->highlight_due_today == highlight)
		return;

	model->priv->highlight_due_today = highlight;

	g_object_notify (G_OBJECT (model), "highlight-due-today");
}

const gchar *
e_cal_model_get_color_due_today (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return model->priv->color_due_today;
}

void
e_cal_model_set_color_due_today (ECalModel *model,
				 const gchar *color_due_today)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (color_due_today != NULL);

	if (g_strcmp0 (model->priv->color_due_today, color_due_today) == 0)
		return;

	g_free (model->priv->color_due_today);
	model->priv->color_due_today = g_strdup (color_due_today);

	g_object_notify (G_OBJECT (model), "color-due-today");
}

gboolean
e_cal_model_get_highlight_overdue (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);

	return model->priv->highlight_overdue;
}

void
e_cal_model_set_highlight_overdue (ECalModel *model,
				   gboolean highlight)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->highlight_overdue == highlight)
		return;

	model->priv->highlight_overdue = highlight;

	g_object_notify (G_OBJECT (model), "highlight-overdue");
}

const gchar *
e_cal_model_get_color_overdue (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return model->priv->color_overdue;
}

void
e_cal_model_set_color_overdue (ECalModel *model,
			       const gchar *color_overdue)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (color_overdue != NULL);

	if (g_strcmp0 (model->priv->color_overdue, color_overdue) == 0)
		return;

	g_free (model->priv->color_overdue);
	model->priv->color_overdue = g_strdup (color_overdue);

	g_object_notify (G_OBJECT (model), "color-overdue");
}

void
e_cal_model_mark_comp_complete (ECalModel *model,
				ECalModelComponent *comp_data)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (comp_data != NULL);

	cal_model_ensure_task_complete (comp_data, -1);

	e_cal_model_modify_component (model, comp_data, E_CAL_OBJ_MOD_ALL);
}

void
e_cal_model_mark_comp_incomplete (ECalModel *model,
				  ECalModelComponent *comp_data)
{
	ICalProperty *prop;

	g_return_if_fail (model != NULL);
	g_return_if_fail (comp_data != NULL);

	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_STATUS_PROPERTY);
	if (prop)
		i_cal_property_set_status (prop, I_CAL_STATUS_NEEDSACTION);
	else
		i_cal_component_take_property (comp_data->icalcomp, i_cal_property_new_status (I_CAL_STATUS_NEEDSACTION));
	g_clear_object (&prop);

	e_cal_util_component_remove_property_by_kind (comp_data->icalcomp, I_CAL_COMPLETED_PROPERTY, TRUE);
	e_cal_util_component_remove_property_by_kind (comp_data->icalcomp, I_CAL_PERCENTCOMPLETE_PROPERTY, TRUE);

	e_cal_model_modify_component (model, comp_data, E_CAL_OBJ_MOD_ALL);
}

void
e_cal_model_update_due_tasks (ECalModel *model)
{
	guint ii;
	ECalModelComponent *comp_data;
	ECalModelDueStatus status;
	gpointer visible_index_ptr;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	cal_model_ensure_visible_rows (model);

	for (ii = 0; ii < model->priv->objects->len; ii++) {
		comp_data = g_ptr_array_index (model->priv->objects, ii);
		status = cal_model_get_due_status (model, comp_data);

		if (status == E_CAL_MODEL_DUE_TODAY || status == E_CAL_MODEL_DUE_OVERDUE) {
			if (g_hash_table_lookup_extended (model->priv->component_to_visible_index, comp_data, NULL, &visible_index_ptr)) {
				guint visible_index = GPOINTER_TO_UINT (visible_index_ptr);

				e_virtual_tree_model_emit_rows_changed (E_VIRTUAL_TREE_MODEL (model), visible_index, visible_index);
			}
		}
	}
}

ECalModelComponent *
_e_cal_model_test_add_component (ECalModel *model,
				 const gchar *source_uid,
				 ICalComponent *icalcomp)
{
	ECalModelComponent *comp_data;
	gchar *target;

	comp_data = g_object_new (E_TYPE_CAL_MODEL_COMPONENT, NULL);
	comp_data->is_new_component = FALSE;
	comp_data->source_uid = g_strdup (source_uid);
	comp_data->icalcomp = icalcomp;

	g_ptr_array_add (model->priv->objects, comp_data);

	comp_data->priv->own_key = cal_model_component_build_key (comp_data);
	g_hash_table_insert (model->priv->uid_to_row, comp_data->priv->own_key, comp_data);

	target = cal_model_component_get_related_to_parent_uid (icalcomp);
	cal_model_attach_component (model, comp_data, target);
	g_free (target);

	model->priv->visible_dirty = TRUE;

	return comp_data;
}

gboolean
_e_cal_model_test_modify_component (ECalModel *model,
				    const gchar *source_uid,
				    ICalComponent *icalcomp)
{
	gchar *key;
	gchar *rid;
	ECalModelComponent *comp_data;
	gchar *old_target, *new_target;

	rid = e_cal_util_component_get_recurid_as_string (icalcomp);
	key = cal_model_build_key (source_uid, i_cal_component_get_uid (icalcomp), rid);
	g_free (rid);

	comp_data = g_hash_table_lookup (model->priv->uid_to_row, key);
	g_free (key);

	if (!comp_data)
		return FALSE;

	old_target = cal_model_component_get_related_to_parent_uid (comp_data->icalcomp);

	g_clear_object (&comp_data->icalcomp);
	comp_data->icalcomp = icalcomp;

	new_target = cal_model_component_get_related_to_parent_uid (comp_data->icalcomp);

	if (g_strcmp0 (old_target, new_target) == 0) {
		cal_model_reposition (model, comp_data);
	} else {
		cal_model_detach_component (model, comp_data);
		cal_model_place_component (model, comp_data, new_target);
	}

	g_free (old_target);
	g_free (new_target);

	model->priv->visible_dirty = TRUE;

	return TRUE;
}

gboolean
_e_cal_model_test_remove_component (ECalModel *model,
				    const gchar *source_uid,
				    const gchar *uid,
				    const gchar *rid)
{
	gchar *key;
	ECalModelComponent *comp_data;
	guint index;

	key = cal_model_build_key (source_uid, uid, rid);
	comp_data = g_hash_table_lookup (model->priv->uid_to_row, key);
	g_free (key);

	if (!comp_data)
		return FALSE;

	cal_model_untrack_component (model, comp_data);

	if (!g_ptr_array_find (model->priv->objects, comp_data, &index))
		return FALSE;

	g_ptr_array_remove_index (model->priv->objects, index);
	g_object_unref (comp_data);

	return TRUE;
}

gboolean
_e_cal_model_test_reparent_component (ECalModel *model,
				      ECalModelComponent *comp_data,
				      ECalModelComponent *new_parent)
{
	if (!e_cal_model_can_reparent_component (model, comp_data, new_parent))
		return FALSE;

	cal_model_rewrite_related_to (comp_data, new_parent);

	cal_model_detach_component (model, comp_data);
	cal_model_place_component (model, comp_data, new_parent ? i_cal_component_get_uid (new_parent->icalcomp) : NULL);

	model->priv->visible_dirty = TRUE;

	return TRUE;
}

guint
_e_cal_model_test_get_row_count (ECalModel *model)
{
	return e_cal_model_get_visible_row_count (model);
}

static void
cal_model_test_append_tree_string (GString *out,
				   GPtrArray *array)
{
	guint ii;
	ECalModelComponent *comp_data;

	for (ii = 0; ii < array->len; ii++) {
		comp_data = g_ptr_array_index (array, ii);

		if (ii > 0)
			g_string_append_c (out, ',');

		g_string_append (out, i_cal_component_get_uid (comp_data->icalcomp));

		if (comp_data->resolved_children && comp_data->resolved_children->len > 0) {
			g_string_append_c (out, '(');
			cal_model_test_append_tree_string (out, comp_data->resolved_children);
			g_string_append_c (out, ')');
		}
	}
}

gchar *
_e_cal_model_test_get_tree_string (ECalModel *model)
{
	GString *out;

	out = g_string_new ("");
	cal_model_test_append_tree_string (out, model->priv->roots);

	return g_string_free (out, FALSE);
}
