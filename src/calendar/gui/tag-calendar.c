/*
 *
 * Evolution calendar - Utilities for tagging ECalendar widgets
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
 *		Damon Chaplin <damon@ximian.com>
 *      Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "shell/e-shell.h"
#include "calendar-config.h"
#include "comp-util.h"
#include "e-cal-data-model-subscriber.h"
#include "tag-calendar.h"

struct _ETagCalendarPrivate
{
	ECalendar *calendar;	/* weak-referenced */
	ECalendarItem *calitem;	/* weak-referenced */
	ECalDataModel *data_model; /* not referenced, due to circular dependency */
	gboolean recur_events_italic;

	GHashTable *objects;	/* ObjectInfo ~> 1 (unused) */
	GHashTable *dates;	/* julian date ~> DateInfo */

	guint32 range_start_julian;
	guint32 range_end_julian;
};

enum {
	PROP_0,
	PROP_CALENDAR,
	PROP_RECUR_EVENTS_ITALIC
};

static void e_tag_calendar_cal_data_model_subscriber_init (ECalDataModelSubscriberInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ETagCalendar, e_tag_calendar, G_TYPE_OBJECT,
	G_ADD_PRIVATE (ETagCalendar)
	G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER, e_tag_calendar_cal_data_model_subscriber_init))

typedef struct {
	gconstpointer client;
	ECalComponentId *id;
	gboolean is_transparent; /* neither of the two means is_single */
	gboolean is_recurring;
	guint32 start_julian;
	guint32 end_julian;
} ObjectInfo;

typedef struct {
	guint n_transparent;
	guint n_recurring;
	guint n_single;
} DateInfo;

static guint
object_info_hash (gconstpointer v)
{
	const ObjectInfo *oinfo = v;

	if (!v)
		return 0;

	return g_direct_hash (oinfo->client) ^ e_cal_component_id_hash (oinfo->id);
}

/* component-related equality, for hash tables */
static gboolean
object_info_equal (gconstpointer v1,
		   gconstpointer v2)
{
	const ObjectInfo *oinfo1 = v1;
	const ObjectInfo *oinfo2 = v2;

	if (oinfo1 == oinfo2)
		return TRUE;

	if (!oinfo1 || !oinfo2)
		return FALSE;

	return oinfo1->client == oinfo2->client &&
	       e_cal_component_id_equal (oinfo1->id, oinfo2->id);
}

/* date-related equality, for drawing changes */
static gboolean
object_info_data_equal (ObjectInfo *o1,
			ObjectInfo *o2)
{
	if (o1 == o2)
		return TRUE;

	if (!o1 || !o2)
		return FALSE;

	return (o1->is_transparent ? 1: 0) == (o2->is_transparent ? 1 : 0) &&
	       (o1->start_julian ? 1: 0) == (o2->is_recurring ? 1 : 0) &&
	       (o1->start_julian == o2->start_julian) &&
	       (o1->end_julian == o2->end_julian);
}

static ObjectInfo *
object_info_new (ECalClient *client,
		 ECalComponentId *id, /* will be consumed */
		 gboolean is_transparent,
		 gboolean is_recurring,
		 guint32 start_julian,
		 guint32 end_julian)
{
	ObjectInfo *oinfo;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (id != NULL, NULL);

	oinfo = g_slice_new0 (ObjectInfo);
	oinfo->client = client;
	oinfo->id = id;
	oinfo->is_transparent = is_transparent;
	oinfo->is_recurring = is_recurring;
	oinfo->start_julian = start_julian;
	oinfo->end_julian = end_julian;

	return oinfo;
}

static void
object_info_free (gpointer ptr)
{
	ObjectInfo *oinfo = ptr;

	if (oinfo) {
		e_cal_component_id_free (oinfo->id);
		g_slice_free (ObjectInfo, oinfo);
	}
}

static DateInfo *
date_info_new (void)
{
	return g_slice_new0 (DateInfo);
}

static void
date_info_free (gpointer ptr)
{
	DateInfo *dinfo = ptr;

	if (dinfo)
		g_slice_free (DateInfo, dinfo);
}

static gboolean
date_info_update (DateInfo *dinfo,
		  ObjectInfo *oinfo,
		  gboolean inc)
{
	gint nn = inc ? +1 : -1;
	gboolean ui_changed = FALSE;

	g_return_val_if_fail (dinfo != NULL, FALSE);
	g_return_val_if_fail (oinfo != NULL, FALSE);

	if (oinfo->is_transparent) {
		dinfo->n_transparent += nn;
		ui_changed = ui_changed || (inc && dinfo->n_transparent == 1) || (!inc && dinfo->n_transparent == 0);
	} else if (oinfo->is_recurring) {
		dinfo->n_recurring += nn;
		ui_changed = ui_changed || (inc && dinfo->n_recurring == 1) || (!inc && dinfo->n_recurring == 0);
	} else {
		dinfo->n_single += nn;
		ui_changed = ui_changed || (inc && dinfo->n_single == 1) || (!inc && dinfo->n_single == 0);
	}

	return ui_changed;
}

static guint8
date_info_get_style (DateInfo *dinfo,
		     gboolean recur_events_italic)
{
	guint8 style = 0;

	g_return_val_if_fail (dinfo != NULL, 0);

	if (dinfo->n_transparent > 0 ||
	    (recur_events_italic && dinfo->n_recurring > 0))
		style |= E_CALENDAR_ITEM_MARK_ITALIC;

	if (dinfo->n_single > 0 ||
	    (!recur_events_italic && dinfo->n_recurring > 0))
		style |= E_CALENDAR_ITEM_MARK_BOLD;

	return style;
}

static gint32
encode_ymd_to_julian (gint year,
		      gint month,
		      gint day)
{
	GDate dt;

	g_date_clear (&dt, 1);
	g_date_set_dmy (&dt, day, month, year);

	return g_date_get_julian (&dt);
}

static guint32
encode_timet_to_julian (time_t t,
			gboolean is_date,
			const ICalTimezone *zone)
{
	ICalTime *tt;
	guint32 res;

	if (!t)
		return 0;

	tt = i_cal_time_new_from_timet_with_zone (t, is_date, (ICalTimezone *) zone);

	if (!tt || !i_cal_time_is_valid_time (tt) || i_cal_time_is_null_time (tt)) {
		g_clear_object (&tt);
		return 0;
	}

	res = encode_ymd_to_julian (i_cal_time_get_year (tt),
				    i_cal_time_get_month (tt),
				    i_cal_time_get_day (tt));

	g_clear_object (&tt);

	return res;
}

static void
decode_julian (guint32 julian,
	       gint *year,
	       gint *month,
	       gint *day)
{
	GDate dt;

	g_date_clear (&dt, 1);
	g_date_set_julian (&dt, julian);

	*year = g_date_get_year (&dt);
	*month = g_date_get_month (&dt);
	*day = g_date_get_day (&dt);
}

static void
tag_calendar_date_cb (gpointer key,
		      gpointer value,
		      gpointer user_data)
{
	ETagCalendar *tag_calendar = user_data;
	DateInfo *dinfo = value;
	gint year, month, day;

	decode_julian (GPOINTER_TO_UINT (key), &year, &month, &day);

	e_calendar_item_mark_day (tag_calendar->priv->calitem, year, month - 1, day,
		date_info_get_style (dinfo, tag_calendar->priv->recur_events_italic), FALSE);
}

static void
e_tag_calendar_remark_days (ETagCalendar *tag_calendar)
{
	g_return_if_fail (E_IS_TAG_CALENDAR (tag_calendar));
	g_return_if_fail (tag_calendar->priv->calitem != NULL);

	e_calendar_item_clear_marks (tag_calendar->priv->calitem);

	g_hash_table_foreach (tag_calendar->priv->dates, tag_calendar_date_cb, tag_calendar);
}

static time_t
e_tag_calendar_date_to_timet (gint year,
			      gint month,
			      gint day,
			      const ICalTimezone *with_zone)
{
	GDate *date;
	time_t tt;

	date = g_date_new_dmy (day, month, year);
	g_return_val_if_fail (date != NULL, (time_t) -1);

	tt = cal_comp_gdate_to_timet (date, with_zone);

	g_date_free (date);

	return tt;
}

static void
e_tag_calendar_date_range_changed_cb (ETagCalendar *tag_calendar)
{
	gint start_year, start_month, start_day, end_year, end_month, end_day;
	time_t range_start, range_end;

	g_return_if_fail (E_IS_TAG_CALENDAR (tag_calendar));

	if (!tag_calendar->priv->data_model ||
	    !tag_calendar->priv->calitem)
		return;

	g_return_if_fail (E_IS_CALENDAR_ITEM (tag_calendar->priv->calitem));

	/* This can fail on start, when the ECalendarItem wasn't updated (drawn) yet */
	if (!e_calendar_item_get_date_range (tag_calendar->priv->calitem,
		&start_year, &start_month, &start_day, &end_year, &end_month, &end_day))
		return;

	start_month++;
	end_month++;

	range_start = e_tag_calendar_date_to_timet (start_year, start_month, start_day, NULL);
	range_end = e_tag_calendar_date_to_timet (end_year, end_month, end_day, NULL);

	tag_calendar->priv->range_start_julian = encode_ymd_to_julian (start_year, start_month, start_day);
	tag_calendar->priv->range_end_julian = encode_ymd_to_julian (end_year, end_month, end_day);

	/* Range change causes removal of marks in the calendar */
	e_tag_calendar_remark_days (tag_calendar);

	e_cal_data_model_subscribe (tag_calendar->priv->data_model,
		E_CAL_DATA_MODEL_SUBSCRIBER (tag_calendar),
		range_start, range_end);
}

static gboolean
e_tag_calendar_query_tooltip_cb (ECalendar *calendar,
				 gint x,
				 gint y,
				 gboolean keayboard_mode,
				 GtkTooltip *tooltip,
				 ETagCalendar *tag_calendar)
{
	GDate date;
	gint32 julian, events;
	DateInfo *date_info;
	gchar *msg;

	g_return_val_if_fail (E_IS_CALENDAR (calendar), FALSE);
	g_return_val_if_fail (E_IS_TAG_CALENDAR (tag_calendar), FALSE);
	g_return_val_if_fail (GTK_IS_TOOLTIP (tooltip), FALSE);

	if (!e_calendar_item_convert_position_to_date (e_calendar_get_item (calendar), x, y, &date))
		return FALSE;

	julian = encode_ymd_to_julian (g_date_get_year (&date), g_date_get_month (&date), g_date_get_day (&date));
	date_info = g_hash_table_lookup (tag_calendar->priv->dates, GINT_TO_POINTER (julian));

	if (!date_info)
		return FALSE;

	events = date_info->n_transparent + date_info->n_recurring + date_info->n_single;

	if (events <= 0)
		return FALSE;

	msg = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d event", "%d events", events), events);

	gtk_tooltip_set_text (tooltip, msg);

	g_free (msg);

	return TRUE;
}

static void
get_component_julian_range (ECalClient *client,
			    ECalComponent *comp,
			    guint32 *start_julian,
			    guint32 *end_julian)
{
	ICalTime *instance_start = NULL, *instance_end = NULL;
	time_t start_tt, end_tt;
	const ICalTimezone *zone;

	g_return_if_fail (client != NULL);
	g_return_if_fail (comp != NULL);

	zone = calendar_config_get_icaltimezone ();

	cal_comp_get_instance_times (client, e_cal_component_get_icalcomponent (comp),
		zone, &instance_start, &instance_end, NULL);

	start_tt = i_cal_time_as_timet_with_zone (instance_start, i_cal_time_get_timezone (instance_start));
	end_tt = i_cal_time_as_timet_with_zone (instance_end, i_cal_time_get_timezone (instance_end));

	*start_julian = encode_timet_to_julian (start_tt, i_cal_time_is_date (instance_start), zone);
	*end_julian = encode_timet_to_julian (end_tt - (end_tt == start_tt ? 0 : 1), i_cal_time_is_date (instance_end), zone);

	g_clear_object (&instance_start);
	g_clear_object (&instance_end);
}

static void
e_tag_calendar_update_by_oinfo (ETagCalendar *tag_calendar,
				ObjectInfo *oinfo,
				gboolean inc)
{
	ECalendarItem *calitem;
	guint32 dt, start_julian, end_julian;
	DateInfo *dinfo;

	g_return_if_fail (tag_calendar->priv->calitem != NULL);

	calitem = tag_calendar->priv->calitem;
	g_return_if_fail (calitem != NULL);

	if (!oinfo)
		return;

	start_julian = oinfo->start_julian;
	end_julian = oinfo->end_julian;

	if (inc) {
		if (start_julian < tag_calendar->priv->range_start_julian)
			start_julian = tag_calendar->priv->range_start_julian;

		if (end_julian > tag_calendar->priv->range_end_julian)
			end_julian = tag_calendar->priv->range_end_julian;
	}

	for (dt = start_julian; dt <= end_julian; dt++) {
		dinfo = g_hash_table_lookup (tag_calendar->priv->dates, GUINT_TO_POINTER (dt));

		if (!dinfo) {
			if (!inc)
				continue;

			dinfo = date_info_new ();
			g_hash_table_insert (tag_calendar->priv->dates, GUINT_TO_POINTER (dt), dinfo);
		}

		if (date_info_update (dinfo, oinfo, inc)) {
			gint year, month, day;
			guint8 style;

			decode_julian (dt, &year, &month, &day);
			style = date_info_get_style (dinfo, tag_calendar->priv->recur_events_italic);

			e_calendar_item_mark_day (calitem, year, month - 1, day, style, FALSE);

			if (!style && !inc)
				g_hash_table_remove (tag_calendar->priv->dates, GUINT_TO_POINTER (dt));
		}
	}
}

static void
e_tag_calendar_update_component_dates (ETagCalendar *tag_calendar,
				       ObjectInfo *old_oinfo,
				       ObjectInfo *new_oinfo)
{
	g_return_if_fail (tag_calendar->priv->calitem != NULL);

	e_tag_calendar_update_by_oinfo (tag_calendar, old_oinfo, FALSE);
	e_tag_calendar_update_by_oinfo (tag_calendar, new_oinfo, TRUE);
}

static void
e_tag_calendar_data_subscriber_component_added (ECalDataModelSubscriber *subscriber,
						ECalClient *client,
						ECalComponent *comp)
{
	ETagCalendar *tag_calendar;
	ECalComponentTransparency transparency;
	guint32 start_julian = 0, end_julian = 0;
	ObjectInfo *oinfo;

	g_return_if_fail (E_IS_TAG_CALENDAR (subscriber));

	tag_calendar = E_TAG_CALENDAR (subscriber);

	get_component_julian_range (client, comp, &start_julian, &end_julian);
	if (start_julian == 0 || end_julian == 0)
		return;

	transparency = e_cal_component_get_transparency (comp);

	oinfo = object_info_new (client, e_cal_component_get_id (comp),
		transparency == E_CAL_COMPONENT_TRANSP_TRANSPARENT,
		e_cal_component_is_instance (comp),
		start_julian, end_julian);

	e_tag_calendar_update_component_dates (tag_calendar, NULL, oinfo);

	g_hash_table_replace (tag_calendar->priv->objects, oinfo, NULL);
}

static void
e_tag_calendar_data_subscriber_component_modified (ECalDataModelSubscriber *subscriber,
						   ECalClient *client,
						   ECalComponent *comp)
{
	ETagCalendar *tag_calendar;
	ECalComponentTransparency transparency;
	guint32 start_julian = 0, end_julian = 0;
	gpointer orig_key, orig_value;
	ObjectInfo *old_oinfo = NULL, *new_oinfo;

	g_return_if_fail (E_IS_TAG_CALENDAR (subscriber));

	tag_calendar = E_TAG_CALENDAR (subscriber);

	get_component_julian_range (client, comp, &start_julian, &end_julian);
	if (start_julian == 0 || end_julian == 0)
		return;

	transparency = e_cal_component_get_transparency (comp);

	new_oinfo = object_info_new (client, e_cal_component_get_id (comp),
		transparency == E_CAL_COMPONENT_TRANSP_TRANSPARENT,
		e_cal_component_is_instance (comp),
		start_julian, end_julian);

	if (!g_hash_table_lookup_extended (tag_calendar->priv->objects, new_oinfo, &orig_key, &orig_value)) {
		object_info_free (new_oinfo);
		return;
	}

	old_oinfo = orig_key;

	if (object_info_data_equal (old_oinfo, new_oinfo)) {
		object_info_free (new_oinfo);
		return;
	}

	e_tag_calendar_update_component_dates (tag_calendar, old_oinfo, new_oinfo);

	/* it also frees old_oinfo */
	g_hash_table_replace (tag_calendar->priv->objects, new_oinfo, NULL);
}

static void
e_tag_calendar_data_subscriber_component_removed (ECalDataModelSubscriber *subscriber,
						  ECalClient *client,
						  const gchar *uid,
						  const gchar *rid)
{
	ETagCalendar *tag_calendar;
	ECalComponentId *id;
	gpointer orig_key, orig_value;
	ObjectInfo fake_oinfo, *old_oinfo;

	g_return_if_fail (E_IS_TAG_CALENDAR (subscriber));

	tag_calendar = E_TAG_CALENDAR (subscriber);

	id = e_cal_component_id_new (uid, rid);

	/* only these two values are used for GHashTable compare */
	fake_oinfo.client = client;
	fake_oinfo.id = id;

	if (!g_hash_table_lookup_extended (tag_calendar->priv->objects, &fake_oinfo, &orig_key, &orig_value)) {
		e_cal_component_id_free (id);
		return;
	}

	old_oinfo = orig_key;

	e_tag_calendar_update_component_dates (tag_calendar, old_oinfo, NULL);

	g_hash_table_remove (tag_calendar->priv->objects, old_oinfo);

	e_cal_component_id_free (id);
}

static void
e_tag_calendar_data_subscriber_freeze (ECalDataModelSubscriber *subscriber)
{
	/* Ignore freezes here */
}

static void
e_tag_calendar_data_subscriber_thaw (ECalDataModelSubscriber *subscriber)
{
	/* Ignore freezes here */
}

static void
e_tag_calendar_set_calendar (ETagCalendar *tag_calendar,
			     ECalendar *calendar)
{
	g_return_if_fail (E_IS_TAG_CALENDAR (tag_calendar));
	g_return_if_fail (E_IS_CALENDAR (calendar));
	g_return_if_fail (e_calendar_get_item (calendar) != NULL);
	g_return_if_fail (tag_calendar->priv->calendar == NULL);

	tag_calendar->priv->calendar = calendar;
	tag_calendar->priv->calitem = e_calendar_get_item (calendar);

	g_object_weak_ref (G_OBJECT (tag_calendar->priv->calendar),
		(GWeakNotify) g_nullify_pointer, &tag_calendar->priv->calendar);
	g_object_weak_ref (G_OBJECT (tag_calendar->priv->calitem),
		(GWeakNotify) g_nullify_pointer, &tag_calendar->priv->calitem);
}

static void
e_tag_calendar_set_property (GObject *object,
			     guint property_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CALENDAR:
			e_tag_calendar_set_calendar (
				E_TAG_CALENDAR (object),
				g_value_get_object (value));
			return;

		case PROP_RECUR_EVENTS_ITALIC:
			e_tag_calendar_set_recur_events_italic (
				E_TAG_CALENDAR (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_tag_calendar_get_property (GObject *object,
			     guint property_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CALENDAR:
			g_value_set_object (value,
				e_tag_calendar_get_calendar (E_TAG_CALENDAR (object)));
			return;

		case PROP_RECUR_EVENTS_ITALIC:
			g_value_set_boolean (value,
				e_tag_calendar_get_recur_events_italic (E_TAG_CALENDAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_tag_calendar_constructed (GObject *object)
{
	ETagCalendar *tag_calendar = E_TAG_CALENDAR (object);
	GSettings *settings;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_tag_calendar_parent_class)->constructed (object);

	g_return_if_fail (tag_calendar->priv->calendar != NULL);
	g_return_if_fail (tag_calendar->priv->calitem != NULL);

	g_signal_connect_swapped (tag_calendar->priv->calitem, "date-range-changed",
		G_CALLBACK (e_tag_calendar_date_range_changed_cb), tag_calendar);

	g_signal_connect (tag_calendar->priv->calendar, "query-tooltip",
		G_CALLBACK (e_tag_calendar_query_tooltip_cb), tag_calendar);

	gtk_widget_set_has_tooltip (GTK_WIDGET (tag_calendar->priv->calendar), TRUE);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_bind (
		settings, "recur-events-italic",
		tag_calendar, "recur-events-italic",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	g_object_unref (settings);
}

static void
e_tag_calendar_dispose (GObject *object)
{
	ETagCalendar *tag_calendar = E_TAG_CALENDAR (object);

	if (tag_calendar->priv->calendar != NULL) {
		g_signal_handlers_disconnect_by_func (e_calendar_get_item (tag_calendar->priv->calendar),
			G_CALLBACK (e_tag_calendar_date_range_changed_cb), tag_calendar);
		g_signal_handlers_disconnect_by_func (tag_calendar->priv->calendar,
			G_CALLBACK (e_tag_calendar_query_tooltip_cb), tag_calendar);
		g_object_weak_unref (G_OBJECT (tag_calendar->priv->calendar),
			(GWeakNotify) g_nullify_pointer, &tag_calendar->priv->calendar);
		tag_calendar->priv->calendar = NULL;
	}

	if (tag_calendar->priv->calitem != NULL) {
		g_object_weak_unref (G_OBJECT (tag_calendar->priv->calitem),
			(GWeakNotify) g_nullify_pointer, &tag_calendar->priv->calitem);
		tag_calendar->priv->calitem = NULL;
	}

	if (tag_calendar->priv->data_model)
		e_tag_calendar_unsubscribe (tag_calendar, tag_calendar->priv->data_model);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_tag_calendar_parent_class)->dispose (object);
}

static void
e_tag_calendar_finalize (GObject *object)
{
	ETagCalendar *tag_calendar = E_TAG_CALENDAR (object);

	g_warn_if_fail (tag_calendar->priv->data_model == NULL);

	g_hash_table_destroy (tag_calendar->priv->objects);
	g_hash_table_destroy (tag_calendar->priv->dates);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_tag_calendar_parent_class)->finalize (object);
}

static void
e_tag_calendar_class_init (ETagCalendarClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = e_tag_calendar_set_property;
	object_class->get_property = e_tag_calendar_get_property;
	object_class->constructed = e_tag_calendar_constructed;
	object_class->dispose = e_tag_calendar_dispose;
	object_class->finalize = e_tag_calendar_finalize;

	g_object_class_install_property (
		object_class,
		PROP_CALENDAR,
		g_param_spec_object (
			"calendar",
			"Calendar",
			NULL,
			E_TYPE_CALENDAR,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_RECUR_EVENTS_ITALIC,
		g_param_spec_boolean (
			"recur-events-italic",
			"Recur Events Italic",
			NULL,
			FALSE,
			G_PARAM_READWRITE));
}

static void
e_tag_calendar_cal_data_model_subscriber_init (ECalDataModelSubscriberInterface *iface)
{
	iface->component_added = e_tag_calendar_data_subscriber_component_added;
	iface->component_modified = e_tag_calendar_data_subscriber_component_modified;
	iface->component_removed = e_tag_calendar_data_subscriber_component_removed;
	iface->freeze = e_tag_calendar_data_subscriber_freeze;
	iface->thaw = e_tag_calendar_data_subscriber_thaw;
}

static void
e_tag_calendar_init (ETagCalendar *tag_calendar)
{
	tag_calendar->priv = e_tag_calendar_get_instance_private (tag_calendar);

	tag_calendar->priv->objects = g_hash_table_new_full (
		object_info_hash,
		object_info_equal,
		object_info_free,
		NULL);

	tag_calendar->priv->dates = g_hash_table_new_full (
		g_direct_hash,
		g_direct_equal,
		NULL,
		date_info_free);
}

ETagCalendar *
e_tag_calendar_new (ECalendar *calendar)
{
	return g_object_new (E_TYPE_TAG_CALENDAR, "calendar", calendar, NULL);
}

ECalendar *
e_tag_calendar_get_calendar (ETagCalendar *tag_calendar)
{
	g_return_val_if_fail (E_IS_TAG_CALENDAR (tag_calendar), NULL);

	return tag_calendar->priv->calendar;
}

gboolean
e_tag_calendar_get_recur_events_italic (ETagCalendar *tag_calendar)
{
	g_return_val_if_fail (E_IS_TAG_CALENDAR (tag_calendar), FALSE);

	return tag_calendar->priv->recur_events_italic;
}

void
e_tag_calendar_set_recur_events_italic (ETagCalendar *tag_calendar,
					gboolean recur_events_italic)
{
	g_return_if_fail (E_IS_TAG_CALENDAR (tag_calendar));

	if ((tag_calendar->priv->recur_events_italic ? 1 : 0) == (recur_events_italic ? 1 : 0))
		return;

	tag_calendar->priv->recur_events_italic = recur_events_italic;

	g_object_notify (G_OBJECT (tag_calendar), "recur-events-italic");

	e_tag_calendar_remark_days (tag_calendar);
}

void
e_tag_calendar_subscribe (ETagCalendar *tag_calendar,
			  ECalDataModel *data_model)
{
	g_return_if_fail (E_IS_TAG_CALENDAR (tag_calendar));
	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (tag_calendar->priv->data_model != data_model);

	/* if the reference is held by the priv->data_model, then
	   an unsubscribe may cause free of the tag_calendar */
	g_object_ref (tag_calendar);

	if (tag_calendar->priv->data_model)
		e_tag_calendar_unsubscribe (tag_calendar, tag_calendar->priv->data_model);

	tag_calendar->priv->data_model = data_model;
	e_tag_calendar_date_range_changed_cb (tag_calendar);

	g_object_unref (tag_calendar);
}

void
e_tag_calendar_unsubscribe (ETagCalendar *tag_calendar,
			    ECalDataModel *data_model)
{
	g_return_if_fail (E_IS_TAG_CALENDAR (tag_calendar));
	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (tag_calendar->priv->data_model == data_model);

	e_cal_data_model_unsubscribe (data_model, E_CAL_DATA_MODEL_SUBSCRIBER (tag_calendar));
	tag_calendar->priv->data_model = NULL;

	/* calitem can be NULL during dispose of an ECalBaseShellContents */
	if (tag_calendar->priv->calitem)
		e_calendar_item_clear_marks (tag_calendar->priv->calitem);

	g_hash_table_remove_all (tag_calendar->priv->objects);
	g_hash_table_remove_all (tag_calendar->priv->dates);
}

struct calendar_tag_closure {
	ECalendarItem *calitem;
	ICalTimezone *zone;
	time_t start_time;
	time_t end_time;

	gboolean skip_transparent_events;
	gboolean recur_events_italic;
};

static void
calendar_tag_closure_free (gpointer ptr)
{
	struct calendar_tag_closure *closure = ptr;

	if (closure)
		g_slice_free (struct calendar_tag_closure, closure);
}

/* Clears all the tags in a calendar and fills a closure structure with the
 * necessary information for iterating over occurrences.  Returns FALSE if
 * the calendar has no dates shown.  */
static gboolean
prepare_tag (ECalendar *ecal,
	     struct calendar_tag_closure *closure,
	     ICalTimezone *zone,
	     gboolean clear_first)
{
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;
	ICalTime *start_tt = NULL;
	ICalTime *end_tt = NULL;

	if (clear_first)
		e_calendar_item_clear_marks (e_calendar_get_item (ecal));

	if (!e_calendar_item_get_date_range (
		e_calendar_get_item (ecal),
		&start_year, &start_month, &start_day,
		&end_year, &end_month, &end_day))
		return FALSE;

	start_tt = i_cal_time_new_null_time ();
	i_cal_time_set_date (start_tt,
		start_year,
		start_month + 1,
		start_day);

	end_tt = i_cal_time_new_null_time ();
	i_cal_time_set_date (end_tt,
		end_year,
		end_month + 1,
		end_day);

	i_cal_time_adjust (end_tt, 1, 0, 0, 0);

	closure->calitem = e_calendar_get_item (ecal);

	if (zone != NULL)
		closure->zone = zone;
	else
		closure->zone = calendar_config_get_icaltimezone ();

	closure->start_time =
		i_cal_time_as_timet_with_zone (start_tt, closure->zone);
	closure->end_time =
		i_cal_time_as_timet_with_zone (end_tt, closure->zone);

	g_clear_object (&start_tt);
	g_clear_object (&end_tt);

	return TRUE;
}

/* Marks the specified range in an ECalendar;
 * called from e_cal_generate_instances() */
static gboolean
tag_calendar_cb (ICalComponent *comp,
		 ICalTime *instance_start,
		 ICalTime *instance_end,
		 gpointer user_data,
		 GCancellable *cancellable,
		 GError **error)
{
	struct calendar_tag_closure *closure = user_data;
	ICalPropertyTransp transp = I_CAL_TRANSP_NONE;
	ICalProperty *prop;
	guint8 style = 0;

	/* If we are skipping TRANSPARENT events, return if the event is
	 * transparent. */
	prop = i_cal_component_get_first_property (comp, I_CAL_TRANSP_PROPERTY);
	if (prop) {
		transp = i_cal_property_get_transp (prop);
		g_object_unref (prop);
	}

	if (transp == I_CAL_TRANSP_TRANSPARENT ||
	    transp == I_CAL_TRANSP_TRANSPARENTNOCONFLICT) {
		if (closure->skip_transparent_events)
			return TRUE;

		style = E_CALENDAR_ITEM_MARK_ITALIC;
	} else if (closure->recur_events_italic && e_cal_util_component_is_instance (comp)) {
		style = E_CALENDAR_ITEM_MARK_ITALIC;
	} else {
		style = E_CALENDAR_ITEM_MARK_BOLD;
	}

	e_calendar_item_mark_days (
		closure->calitem,
		i_cal_time_get_year (instance_start),
		i_cal_time_get_month (instance_start) - 1,
		i_cal_time_get_day (instance_start),
		i_cal_time_get_year (instance_end),
		i_cal_time_get_month (instance_end) - 1,
		i_cal_time_get_day (instance_end),
		style, TRUE);

	return TRUE;
}

/**
 * tag_calendar_by_comp:
 * @ecal: Calendar widget to tag.
 * @comp: A calendar component object.
 * @clear_first: Whether the #ECalendar should be cleared of any marks first.
 *
 * Tags an #ECalendar widget with any occurrences of a specific calendar
 * component that occur within the calendar's current time range.
 * Note that TRANSPARENT events are also tagged here.
 *
 * If comp_is_on_server is FALSE, it will try to resolve TZIDs using builtin
 * timezones first, before querying the server, since the timezones may not
 * have been added to the calendar on the server yet.
 **/
void
tag_calendar_by_comp (ECalendar *ecal,
                      ECalComponent *comp,
                      ECalClient *client,
                      ICalTimezone *display_zone,
                      gboolean clear_first,
                      gboolean comp_is_on_server,
                      gboolean can_recur_events_italic,
                      GCancellable *cancellable)
{
	GSettings *settings;
	struct calendar_tag_closure closure;

	g_return_if_fail (E_IS_CALENDAR (ecal));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	/* If the ECalendar isn't visible, we just return. */
	if (!gtk_widget_is_visible (GTK_WIDGET (ecal)))
		return;

	if (!prepare_tag (ecal, &closure, display_zone, clear_first))
		return;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	closure.skip_transparent_events = FALSE;
	closure.recur_events_italic =
		can_recur_events_italic &&
		g_settings_get_boolean (settings, "recur-events-italic");

	g_object_unref (settings);

	if (comp_is_on_server) {
		struct calendar_tag_closure *alloced_closure;

		alloced_closure = g_slice_new0 (struct calendar_tag_closure);

		*alloced_closure = closure;

		e_cal_client_generate_instances_for_object (
			client, e_cal_component_get_icalcomponent (comp),
			closure.start_time, closure.end_time, cancellable,
			tag_calendar_cb,
			alloced_closure, calendar_tag_closure_free);
	} else {
		ICalTime *start, *end;

		start = i_cal_time_new_from_timet_with_zone (closure.start_time, FALSE, display_zone);
		end = i_cal_time_new_from_timet_with_zone (closure.end_time, FALSE, display_zone);

		e_cal_recur_generate_instances_sync (e_cal_component_get_icalcomponent (comp),
			start, end, tag_calendar_cb, &closure,
			e_cal_client_tzlookup_cb, client,
			display_zone, cancellable, NULL);

		g_clear_object (&start);
		g_clear_object (&end);
	}
}
