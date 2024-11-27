/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <e-util/e-util.h>

#include "comp-util.h"
#include "e-cal-component-preview.h"
#include "e-cal-ops.h"
#include "e-calendar-view.h"
#include "itip-utils.h"

#include "e-year-view.h"

/* #define WITH_PREV_NEXT_BUTTONS 1 */

static GtkTargetEntry target_table[] = {
	{ (gchar *) "application/x-e-calendar-event", 0, 0 }
};

typedef struct _ComponentData {
	ECalClient *client;
	ECalComponent *comp;
	gchar *uid;
	gchar *rid;

	guint day_from; /* day of year the comp is used at from, inclusive */
	guint day_to; /* day of year the comp is used at to, inclusive */

	guint date_mark; /* YYYYMMDD */
	guint time_mark; /* HHMMSS */
} ComponentData;

typedef struct _DayData {
	guint n_total; /* includes n_italic */
	guint n_italic;
	GSList *comps_data; /* ComponentData * */
} DayData;

typedef struct _DragData {
	ECalClient *client;
	ECalComponent *comp;
} DragData;

struct _EYearViewPrivate {
	ESourceRegistry *registry;
	GHashTable *client_colors; /* ESource * ~> GdkRGBA * */
	GtkCssProvider *css_provider;
	GtkWidget *hpaned;
	GtkWidget *preview_paned;
	GtkButton *prev_year_button1;
	GtkButton *prev_year_button2;
	GtkLabel *current_year_label;
	GtkButton *next_year_button1;
	GtkButton *next_year_button2;
	GtkTreeView *tree_view;
	GtkListStore *list_store;
	GtkWidget *attachment_bar;
	ECalComponentPreview *preview;
	ECalDataModel *data_model;
	EMonthWidget *months[12];
	DayData days[367];
	GHashTable *comps; /* ComponentData * ~> ComponentData * (itself, just for easier lookup) */
	gboolean clearing_comps;
	gboolean preview_visible;
	gboolean use_24hour_format;
	guint current_day;
	guint current_month;
	guint current_year;

	GSList *drag_data; /* DragData * */
	guint drag_day;
	guint drag_month;
	guint drag_year;

	/* Track today */
	gboolean highlight_today;
	gboolean today_fix_timeout;
	guint today_source_id;
	guint today_date; /* YYYYMMDD - current highlighted day */
};

enum {
	PROP_0,
	PROP_PREVIEW_VISIBLE,
	PROP_USE_24HOUR_FORMAT,
	PROP_HIGHLIGHT_TODAY,
	LAST_PROP,
	PROP_IS_EDITING /* override property as the last */
};

static void year_view_cal_data_model_subscriber_init (ECalDataModelSubscriberInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EYearView, e_year_view, E_TYPE_CALENDAR_VIEW,
	G_ADD_PRIVATE (EYearView)
	G_IMPLEMENT_INTERFACE (E_TYPE_CAL_DATA_MODEL_SUBSCRIBER, year_view_cal_data_model_subscriber_init))

static GParamSpec *obj_props[LAST_PROP] = { NULL, };

enum {
	COLUMN_BGCOLOR = 0,
	COLUMN_FGCOLOR,
	COLUMN_HAS_ICON_NAME,
	COLUMN_ICON_NAME,
	COLUMN_SUMMARY,
	COLUMN_TOOLTIP,
	COLUMN_SORTKEY,
	COLUMN_COMPONENT_DATA,
	N_COLUMNS
};

static ComponentData *
component_data_new (ECalClient *client,
		    ECalComponent *comp)
{
	ComponentData *cd;
	ECalComponentId *id;

	id = e_cal_component_get_id (comp);

	cd = g_new0 (ComponentData, 1);
	cd->client = g_object_ref (client);
	cd->comp = g_object_ref (comp);
	cd->uid = id ? g_strdup (e_cal_component_id_get_uid (id)) : NULL;
	cd->rid = id ? g_strdup (e_cal_component_id_get_rid (id)) : NULL;

	e_cal_component_id_free (id);

	return cd;
}

static void
component_data_free (gpointer ptr)
{
	ComponentData *cd = ptr;

	if (cd) {
		g_clear_object (&cd->client);
		g_clear_object (&cd->comp);
		g_free (cd->uid);
		g_free (cd->rid);
		g_free (cd);
	}
}

static guint
component_data_hash (gconstpointer ptr)
{
	const ComponentData *cd = ptr;

	if (!cd)
		return 0;

	return g_direct_hash (cd->client) ^
		(cd->uid ? g_str_hash (cd->uid) : 0) ^
		(cd->rid ? g_str_hash (cd->rid) : 0);
}

static gboolean
component_data_equal (gconstpointer ptr1,
		      gconstpointer ptr2)
{
	const ComponentData *cd1 = ptr1, *cd2 = ptr2;

	if (!cd1 || !cd2)
		return cd1 == cd2;

	return cd1->client == cd2->client &&
		g_strcmp0 (cd1->uid, cd2->uid) == 0 &&
		g_strcmp0 (cd1->rid, cd2->rid) == 0;
}

static DragData *
drag_data_new (ECalClient *client,
	       ECalComponent *comp)
{
	DragData *dd;

	dd = g_slice_new (DragData);
	dd->client = g_object_ref (client);
	dd->comp = g_object_ref (comp);

	return dd;
}

static void
drag_data_free (gpointer ptr)
{
	DragData *dd = ptr;

	if (dd) {
		g_clear_object (&dd->client);
		g_clear_object (&dd->comp);
		g_slice_free (DragData, dd);
	}
}

static void
year_view_calc_component_data (EYearView *self,
			       ComponentData *cd,
			       guint *out_day_from,
			       guint *out_day_to,
			       guint *out_date_mark,
			       guint *out_time_mark)
{
	ECalComponentDateTime *dtstart, *dtend = NULL, *dt;
	guint day_from = 0;
	guint day_to = 0;
	guint date_mark = 0;
	guint time_mark = 0;

	dtstart = e_cal_component_get_dtstart (cd->comp);

	if (e_cal_component_get_vtype (cd->comp) == E_CAL_COMPONENT_TODO) {
		if (!dtstart)
			dtstart = e_cal_component_get_due (cd->comp);
	} else {
		dtend = e_cal_component_get_dtend (cd->comp);
	}

	dt = dtstart ? dtstart : dtend;

	if (dt) {
		ICalTimezone *zone;
		ICalTime *itt;

		zone = e_cal_data_model_get_timezone (self->priv->data_model);
		itt = cal_comp_util_date_time_to_zone (dt, cd->client, zone);

		if (itt) {
			if (i_cal_time_get_year (itt) < self->priv->current_year) {
				i_cal_time_set_date (itt, self->priv->current_year, 1, 1);

				if (!i_cal_time_is_date (itt))
					i_cal_time_set_time (itt, 0, 0, 0);
			} else if (i_cal_time_get_year (itt) > self->priv->current_year) {
				i_cal_time_set_date (itt, self->priv->current_year, 12, 31);

				if (!i_cal_time_is_date (itt))
					i_cal_time_set_time (itt, 23, 59, 59);
			}

			day_from = i_cal_time_day_of_year (itt);
			day_to = day_from;
			date_mark = (i_cal_time_get_year (itt) * 10000) +
				(i_cal_time_get_month (itt) * 100) +
				i_cal_time_get_day (itt);

			if (!i_cal_time_is_date (itt)) {
				time_mark = (i_cal_time_get_hour (itt) * 10000) +
					(i_cal_time_get_minute (itt) * 100) +
					i_cal_time_get_second (itt);
			}

			g_object_unref (itt);
		}

		if (dtend && dt != dtend) {
			itt = cal_comp_util_date_time_to_zone (dtend, cd->client, zone);

			if (itt) {
				guint end_date_mark, end_time_mark = 0;

				if (i_cal_time_get_year (itt) < self->priv->current_year) {
					i_cal_time_set_date (itt, self->priv->current_year, 1, 1);

					if (!i_cal_time_is_date (itt))
						i_cal_time_set_time (itt, 0, 0, 0);
				} else if (i_cal_time_get_year (itt) > self->priv->current_year) {
					i_cal_time_set_date (itt, self->priv->current_year, 12, 31);

					if (!i_cal_time_is_date (itt))
						i_cal_time_set_time (itt, 23, 59, 59);
				}

				end_date_mark = (i_cal_time_get_year (itt) * 10000) +
					(i_cal_time_get_month (itt) * 100) +
					i_cal_time_get_day (itt);

				if (!i_cal_time_is_date (itt)) {
					end_time_mark = (i_cal_time_get_hour (itt) * 10000) +
						(i_cal_time_get_minute (itt) * 100) +
						i_cal_time_get_second (itt);
				}

				if (end_date_mark > date_mark || (end_date_mark == date_mark && end_time_mark > time_mark)) {
					/* The end time is excluded */
					i_cal_time_adjust (itt, i_cal_time_is_date (itt) ? -1 : 0, 0, 0, i_cal_time_is_date (itt) ? 0 : -1);
				}

				day_to = i_cal_time_day_of_year (itt);

				/* This should not happen */
				if (day_to < day_from)
					day_to = day_from;

				g_object_unref (itt);
			}
		}
	}

	e_cal_component_datetime_free (dtstart);
	e_cal_component_datetime_free (dtend);

	*out_day_from = day_from;
	*out_day_to = day_to;
	*out_date_mark = date_mark;
	*out_time_mark = time_mark;
}

static void
year_view_clear_comps (EYearView *self)
{
	guint ii;

	for (ii = 0; ii < 367; ii++) {
		g_slist_free (self->priv->days[ii].comps_data);

		self->priv->days[ii].n_total = 0;
		self->priv->days[ii].n_italic = 0;
		self->priv->days[ii].comps_data = NULL;
	}

	g_hash_table_remove_all (self->priv->comps);
}

static void
year_view_update_data_model (EYearView *self)
{
	time_t range_start, range_end;
	ICalTimezone *default_zone;
	GDate dt;

	self->priv->clearing_comps = TRUE;
	year_view_clear_comps (self);
	e_cal_data_model_unsubscribe (self->priv->data_model, E_CAL_DATA_MODEL_SUBSCRIBER (self));
	self->priv->clearing_comps = FALSE;

	default_zone = e_cal_data_model_get_timezone (self->priv->data_model);

	g_date_clear (&dt, 1);
	g_date_set_dmy (&dt, 1, 1, self->priv->current_year);
	range_start = time_day_begin_with_zone (cal_comp_gdate_to_timet (&dt, default_zone), default_zone);
	g_date_set_dmy (&dt, 31, 12, self->priv->current_year);
	range_end = time_day_end_with_zone (cal_comp_gdate_to_timet (&dt, default_zone), default_zone);

	e_cal_data_model_subscribe (self->priv->data_model,
		E_CAL_DATA_MODEL_SUBSCRIBER (self),
		range_start, range_end);
}

static void
year_view_get_comp_colors (EYearView *self,
			   ECalClient *client,
			   ECalComponent *comp,
			   GdkRGBA *out_bgcolor,
			   gboolean *out_bgcolor_set,
			   GdkRGBA *out_fgcolor,
			   gboolean *out_fgcolor_set)
{
	GdkRGBA *bgcolor = NULL, fgcolor = { 1.0, 1.0, 1.0, 1.0 };
	GdkRGBA stack_bgcolor;
	ICalProperty *prop;

	g_return_if_fail (out_bgcolor);
	g_return_if_fail (out_bgcolor_set);
	g_return_if_fail (out_fgcolor);
	g_return_if_fail (out_fgcolor_set);

	*out_bgcolor_set = FALSE;
	*out_fgcolor_set = FALSE;

	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	prop = i_cal_component_get_first_property (e_cal_component_get_icalcomponent (comp), I_CAL_COLOR_PROPERTY);
	if (prop) {
		const gchar *color_spec;

		color_spec = i_cal_property_get_color (prop);
		if (color_spec && gdk_rgba_parse (&stack_bgcolor, color_spec)) {
			bgcolor = &stack_bgcolor;
		}

		g_clear_object (&prop);
	}

	if (!bgcolor) {
		ESource *source = e_client_get_source (E_CLIENT (client));

		bgcolor = g_hash_table_lookup (self->priv->client_colors, source);

		if (!bgcolor && !g_hash_table_contains (self->priv->client_colors, source)) {
			ESourceSelectable *selectable = NULL;

			if (e_cal_client_get_source_type (client) == E_CAL_CLIENT_SOURCE_TYPE_EVENTS) {
				selectable = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
			} else if (e_cal_client_get_source_type (client) == E_CAL_CLIENT_SOURCE_TYPE_TASKS) {
				selectable = e_source_get_extension (source, E_SOURCE_EXTENSION_TASK_LIST);
			}

			if (selectable) {
				GdkRGBA rgba;
				gchar *color_spec;

				color_spec = e_source_selectable_dup_color (selectable);
				if (color_spec && gdk_rgba_parse (&rgba, color_spec)) {
					bgcolor = gdk_rgba_copy (&rgba);
					g_hash_table_insert (self->priv->client_colors, source, bgcolor);
				} else {
					g_hash_table_insert (self->priv->client_colors, source, NULL);
				}

				g_free (color_spec);
			} else {
				g_hash_table_insert (self->priv->client_colors, source, NULL);
			}
		}
	}

	if (bgcolor)
		fgcolor = e_utils_get_text_color_for_background (bgcolor);

	*out_bgcolor_set = bgcolor != NULL;
	if (bgcolor)
		*out_bgcolor = *bgcolor;

	*out_fgcolor_set = *out_bgcolor_set;
	*out_fgcolor = fgcolor;
}

static guint
year_view_get_describe_flags (EYearView *self)
{
	return (GTK_TEXT_DIR_RTL == gtk_widget_get_direction (GTK_WIDGET (self)) ? E_CAL_COMP_UTIL_DESCRIBE_FLAG_RTL : 0) |
		E_CAL_COMP_UTIL_DESCRIBE_FLAG_USE_MARKUP |
		E_CAL_COMP_UTIL_DESCRIBE_FLAG_ONLY_TIME |
		(self->priv->use_24hour_format ? E_CAL_COMP_UTIL_DESCRIBE_FLAG_24HOUR_FORMAT : 0);
}

static const gchar *
year_view_get_component_icon_name (EYearView *self,
				   ComponentData *cd)
{
	const gchar *icon_name;
	gboolean is_task = e_cal_component_get_vtype (cd->comp) == E_CAL_COMPONENT_TODO;

	if (is_task && e_cal_component_has_recurrences (cd->comp)) {
		icon_name = "stock_task-recurring";
	} else if (e_cal_component_has_attendees (cd->comp)) {
		if (is_task) {
			ESourceRegistry *registry = self->priv->registry;

			icon_name = "stock_task-assigned";

			if (itip_organizer_is_user (registry, cd->comp, cd->client)) {
				icon_name = "stock_task-assigned-to";
			} else {
				GSList *attendees = NULL, *link;

				attendees = e_cal_component_get_attendees (cd->comp);
				for (link = attendees; link; link = g_slist_next (link)) {
					ECalComponentAttendee *ca = link->data;
					const gchar *text;

					text = e_cal_util_get_attendee_email (ca);
					if (itip_address_is_user (registry, text)) {
						if (e_cal_component_attendee_get_delegatedto (ca))
							icon_name = "stock_task-assigned-to";
						break;
					}
				}

				g_slist_free_full (attendees, e_cal_component_attendee_free);
			}
		} else
			icon_name = "stock_people";
	} else {
		if (is_task)
			icon_name = "stock_task";
		else
			icon_name = "appointment-new";
	}

	return icon_name;
}

static void
year_view_add_to_list_store (EYearView *self,
			     ComponentData *cd)
{
	GtkTreeIter iter;
	GdkRGBA bgcolor, fgcolor;
	ICalTimezone *default_zone;
	ICalProperty *prop;
	gboolean bgcolor_set = FALSE, fgcolor_set = FALSE;
	gchar *summary, *tooltip, *sort_key;

	year_view_get_comp_colors (self, cd->client, cd->comp, &bgcolor, &bgcolor_set, &fgcolor, &fgcolor_set);

	default_zone = e_cal_data_model_get_timezone (self->priv->data_model);
	summary = cal_comp_util_describe (cd->comp, cd->client, default_zone, year_view_get_describe_flags (self));
	tooltip = cal_comp_util_dup_tooltip (cd->comp, cd->client, self->priv->registry, default_zone);
	prop = e_cal_util_component_find_property_for_locale (e_cal_component_get_icalcomponent (cd->comp), I_CAL_SUMMARY_PROPERTY, NULL);
	sort_key = g_strdup_printf ("%08u%06u-%s-%s-%s", cd->date_mark, cd->time_mark,
		prop ? i_cal_property_get_summary (prop) : "",
		cd->uid ? cd->uid : "", cd->rid ? cd->rid : "");
	g_clear_object (&prop);

	gtk_list_store_append (self->priv->list_store, &iter);
	gtk_list_store_set (self->priv->list_store, &iter,
		COLUMN_BGCOLOR, bgcolor_set ? &bgcolor : NULL,
		COLUMN_FGCOLOR, fgcolor_set ? &fgcolor : NULL,
		COLUMN_HAS_ICON_NAME, TRUE,
		COLUMN_ICON_NAME, year_view_get_component_icon_name (self, cd),
		COLUMN_SUMMARY, summary,
		COLUMN_TOOLTIP, tooltip,
		COLUMN_SORTKEY, sort_key,
		COLUMN_COMPONENT_DATA, cd,
		-1);

	g_free (summary);
	g_free (tooltip);
	g_free (sort_key);
}

static void
year_view_update_tree_view (EYearView *self)
{
	GDate date;
	GtkTreeViewColumn *column;
	GSList *link;
	gchar buffer[128] = { 0, };
	guint day_of_year;

	g_date_clear (&date, 1);
	g_date_set_dmy (&date, self->priv->current_day, self->priv->current_month, self->priv->current_year);

	e_datetime_format_format_inline ("calendar", "table", DTFormatKindDate, cal_comp_gdate_to_timet (&date, i_cal_timezone_get_utc_timezone ()), buffer, sizeof (buffer));

	column = gtk_tree_view_get_column (self->priv->tree_view, 0);
	gtk_tree_view_column_set_title (column, buffer);

	day_of_year = g_date_get_day_of_year (&date);
	g_return_if_fail (day_of_year < sizeof (self->priv->days));

	gtk_tree_view_set_model (self->priv->tree_view, NULL);

	gtk_list_store_clear (self->priv->list_store);

	for (link = self->priv->days[day_of_year].comps_data; link; link = g_slist_next (link)) {
		ComponentData *cd = link->data;

		year_view_add_to_list_store (self, cd);
	}

	gtk_tree_view_set_model (self->priv->tree_view, GTK_TREE_MODEL (self->priv->list_store));
}

static gboolean
year_view_update_today (EYearView *self)
{
	if (self->priv->highlight_today) {
		ICalTime *now;
		gint year = 0, month = 0, day = 0, seconds = 0;
		guint today;

		now = i_cal_time_new_current_with_zone (e_cal_data_model_get_timezone (self->priv->data_model));

		/* Inc two seconds in case the GSource is invoked just before midnight */
		i_cal_time_adjust (now, 0, 0, 0, 2);
		i_cal_time_get_date (now, &year, &month, &day);
		i_cal_time_get_time (now, NULL, NULL, &seconds);

		g_clear_object (&now);

		today = (year * 10000) + (month * 100) + day;

		if (today != self->priv->today_date) {
			if (self->priv->today_date) {
				e_month_widget_remove_day_css_class (self->priv->months[((self->priv->today_date / 100) % 100) - 1],
					self->priv->today_date % 100, E_MONTH_WIDGET_CSS_CLASS_HIGHLIGHT);
			}

			self->priv->today_date = today;

			if (year == self->priv->current_year) {
				e_month_widget_add_day_css_class (self->priv->months[((self->priv->today_date / 100) % 100) - 1],
					self->priv->today_date % 100, E_MONTH_WIDGET_CSS_CLASS_HIGHLIGHT);
			}
		}

		if (seconds > 1) {
			if (self->priv->today_source_id)
				g_source_remove (self->priv->today_source_id);

			self->priv->today_fix_timeout = TRUE;
			self->priv->today_source_id = g_timeout_add_seconds (61 - seconds, (GSourceFunc) year_view_update_today, self);
		} else if (self->priv->today_fix_timeout || !self->priv->today_source_id) {
			self->priv->today_fix_timeout = FALSE;
			self->priv->today_source_id = g_timeout_add_seconds (60, (GSourceFunc) year_view_update_today, self);
		} else {
			return TRUE;
		}
	} else {
		if (self->priv->today_source_id) {
			g_source_remove (self->priv->today_source_id);
			self->priv->today_source_id = 0;
		}

		if (self->priv->today_date) {
			e_month_widget_remove_day_css_class (self->priv->months[((self->priv->today_date / 100) % 100) - 1],
				self->priv->today_date % 100, E_MONTH_WIDGET_CSS_CLASS_HIGHLIGHT);

			self->priv->today_date = 0;
		}

		self->priv->today_fix_timeout = FALSE;
	}

	return FALSE;
}

static void
year_view_set_year (EYearView *self,
		    guint year,
		    gint month,
		    guint day)
{
	gchar buffer[128];
	gint ii;

	if (self->priv->current_year == year) {
		if ((month && self->priv->current_month != month) ||
		    (day && self->priv->current_day != day)) {
			e_month_widget_set_day_selected (self->priv->months[self->priv->current_month - 1], self->priv->current_day, FALSE);

			if (month)
				self->priv->current_month = month;
			if (day)
				self->priv->current_day = day;

			e_month_widget_set_day_selected (self->priv->months[self->priv->current_month - 1], self->priv->current_day, TRUE);

			year_view_update_tree_view (self);
		}
	} else {
		self->priv->current_year = year;
		if (month)
			self->priv->current_month = month;
		if (day)
			self->priv->current_day = day;

		g_snprintf (buffer, sizeof (buffer), "%d", self->priv->current_year - 2);
		gtk_button_set_label (self->priv->prev_year_button2, buffer);

		g_snprintf (buffer, sizeof (buffer), "%d", self->priv->current_year - 1);
		gtk_button_set_label (self->priv->prev_year_button1, buffer);

		g_snprintf (buffer, sizeof (buffer), "%d", self->priv->current_year);
		gtk_label_set_label (self->priv->current_year_label, buffer);

		g_snprintf (buffer, sizeof (buffer), "%d", self->priv->current_year + 1);
		gtk_button_set_label (self->priv->next_year_button1, buffer);

		g_snprintf (buffer, sizeof (buffer), "%d", self->priv->current_year + 2);
		gtk_button_set_label (self->priv->next_year_button2, buffer);

		for (ii = 0; ii < 12; ii++) {
			e_month_widget_clear_day_tooltips (self->priv->months[ii]);
			e_month_widget_clear_day_css_classes (self->priv->months[ii]);
			e_month_widget_set_month (self->priv->months[ii], ii + 1, self->priv->current_year);
		}

		self->priv->today_date = 0;

		e_month_widget_set_day_selected (self->priv->months[self->priv->current_month - 1], self->priv->current_day, TRUE);

		year_view_update_data_model (self);
		year_view_update_tree_view (self);
		year_view_update_today (self);
	}
}

static GSList *
year_view_get_selected_events (ECalendarView *cal_view)
{
	EYearView *self;
	GtkTreeSelection *tree_selection;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	GList *selected, *link;
	GSList *selection = NULL;

	g_return_val_if_fail (E_IS_YEAR_VIEW (cal_view), NULL);

	self = E_YEAR_VIEW (cal_view);

	tree_selection = gtk_tree_view_get_selection (self->priv->tree_view);
	selected = gtk_tree_selection_get_selected_rows (tree_selection, &model);

	for (link = selected; link; link = g_list_next (link)) {
		if (gtk_tree_model_get_iter (model, &iter, selected->data)) {
			ComponentData *cd = NULL;

			gtk_tree_model_get (model, &iter,
				COLUMN_COMPONENT_DATA, &cd,
				-1);

			selection = g_slist_prepend (selection,
				e_calendar_view_selection_data_new (cd->client, e_cal_component_get_icalcomponent (cd->comp)));
		}
	}

	g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);

	return selection;
}

static gboolean
year_view_get_selected_time_range (ECalendarView *cal_view,
				   time_t *start_time,
				   time_t *end_time)
{
	EYearView *self;
	ICalTimezone *zone;
	GDate date;

	g_return_val_if_fail (E_IS_YEAR_VIEW (cal_view), FALSE);

	self = E_YEAR_VIEW (cal_view);
	zone = e_cal_data_model_get_timezone (self->priv->data_model);

	g_date_clear (&date, 1);
	g_date_set_dmy (&date, self->priv->current_day, self->priv->current_month, self->priv->current_year);

	if (start_time)
		*start_time = time_day_begin (cal_comp_gdate_to_timet (&date, zone));
	if (end_time)
		*end_time = time_day_end (*start_time);

	return TRUE;
}

static void
year_view_set_selected_time_range (ECalendarView *cal_view,
				   time_t start_time,
				   time_t end_time)
{
	EYearView *self;
	ICalTimezone *zone;
	GDate date;

	g_return_if_fail (E_IS_YEAR_VIEW (cal_view));

	self = E_YEAR_VIEW (cal_view);
	zone = e_cal_data_model_get_timezone (self->priv->data_model);

	time_to_gdate_with_zone (&date, start_time, zone);

	year_view_set_year (self, g_date_get_year (&date), g_date_get_month (&date), g_date_get_day (&date));
}

static time_t
year_view_add_days_in_year (time_t tt,
			    guint year)
{
	return time_add_day (tt, 31 + g_date_get_days_in_month (2, year) + 31 + 30 + 31 + 30 +
		31 + 31 + 30 + 31 + 30 + 31);
}

static gboolean
year_view_get_visible_time_range (ECalendarView *cal_view,
				  time_t *start_time,
				  time_t *end_time)
{
	EYearView *self;
	ICalTimezone *zone;
	GDate date;

	g_return_val_if_fail (E_IS_YEAR_VIEW (cal_view), FALSE);

	self = E_YEAR_VIEW (cal_view);
	zone = e_cal_data_model_get_timezone (self->priv->data_model);

	g_date_clear (&date, 1);
	g_date_set_dmy (&date, self->priv->current_day, self->priv->current_month, self->priv->current_year);

	if (start_time)
		*start_time = time_year_begin_with_zone (cal_comp_gdate_to_timet (&date, zone), zone);
	if (end_time)
		*end_time = year_view_add_days_in_year (*start_time, self->priv->current_year);

	return TRUE;
}

static void
year_view_precalc_visible_time_range (ECalendarView *cal_view,
				      time_t in_start_time,
				      time_t in_end_time,
				      time_t *out_start_time,
				      time_t *out_end_time)
{
	EYearView *self;
	ICalTimezone *zone;
	ICalTime *itt;

	g_return_if_fail (E_IS_YEAR_VIEW (cal_view));
	g_return_if_fail (out_start_time != NULL);
	g_return_if_fail (out_end_time != NULL);

	self = E_YEAR_VIEW (cal_view);
	zone = e_cal_data_model_get_timezone (self->priv->data_model);

	itt = i_cal_time_new_from_timet_with_zone (in_start_time, FALSE, zone);

	i_cal_time_set_date (itt, i_cal_time_get_year (itt), self->priv->current_month, self->priv->current_day);

	*out_start_time = i_cal_time_as_timet_with_zone (itt, zone);
	*out_end_time = *out_start_time + (24 * 3600);

	g_clear_object (&itt);
}

static void
year_view_paste_text (ECalendarView *cal_view)
{
	g_return_if_fail (E_IS_YEAR_VIEW (cal_view));

	/* Do nothing, inline editing not allowed here */
}

static void
year_view_month_widget_day_clicked_cb (EMonthWidget *month_widget,
				       GdkEventButton *event,
				       guint year,
				       gint /* GDateMonth */ month,
				       guint day,
				       gpointer user_data)
{
	EYearView *self = user_data;

	if (event->button == GDK_BUTTON_PRIMARY)
		year_view_set_year (self, year, month, day);
}

static void
year_view_prev_year_clicked_cb (GtkWidget *button,
				gpointer user_data)
{
	EYearView *self = user_data;

	year_view_set_year (self, self->priv->current_year - 1, 0, 0);
}

static void
year_view_prev_year2_clicked_cb (GtkWidget *button,
				 gpointer user_data)
{
	EYearView *self = user_data;

	year_view_set_year (self, self->priv->current_year - 2, 0, 0);
}

static void
year_view_next_year_clicked_cb (GtkWidget *button,
				gpointer user_data)
{
	EYearView *self = user_data;

	year_view_set_year (self, self->priv->current_year + 1, 0, 0);
}

static void
year_view_next_year2_clicked_cb (GtkWidget *button,
				 gpointer user_data)
{
	EYearView *self = user_data;

	year_view_set_year (self, self->priv->current_year + 2, 0, 0);
}

static void
year_view_update_colors (EYearView *self)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = GTK_TREE_MODEL (self->priv->list_store);

	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		ComponentData *cd = NULL;

		gtk_tree_model_get (model, &iter,
			COLUMN_COMPONENT_DATA, &cd,
			-1);

		if (cd) {
			GdkRGBA bgcolor, fgcolor;
			gboolean bgcolor_set = FALSE, fgcolor_set = FALSE;

			year_view_get_comp_colors (self, cd->client, cd->comp, &bgcolor, &bgcolor_set, &fgcolor, &fgcolor_set);

			gtk_list_store_set (self->priv->list_store, &iter,
				COLUMN_BGCOLOR, bgcolor_set ? &bgcolor : NULL,
				COLUMN_FGCOLOR, fgcolor_set ? &fgcolor : NULL,
				-1);
		}
	} while (gtk_tree_model_iter_next (model, &iter));
}

static void
year_view_source_changed_cb (ESourceRegistry *registry,
			     ESource *source,
			     gpointer user_data)
{
	EYearView *self = user_data;

	if (g_hash_table_contains (self->priv->client_colors, source)) {
		ESourceSelectable *selectable = NULL;

		if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
			selectable = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
		else if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
			selectable = e_source_get_extension (source, E_SOURCE_EXTENSION_TASK_LIST);

		if (selectable) {
			GdkRGBA rgba;
			gchar *color_spec;

			color_spec = e_source_selectable_dup_color (selectable);
			if (color_spec && gdk_rgba_parse (&rgba, color_spec)) {
				GdkRGBA *current_rgba;

				current_rgba = g_hash_table_lookup (self->priv->client_colors, source);
				if (!gdk_rgba_equal (current_rgba, &rgba)) {
					g_hash_table_insert (self->priv->client_colors, source, gdk_rgba_copy (&rgba));
					year_view_update_colors (self);
				}
			}

			g_free (color_spec);
		}
	}
}

static void
year_view_source_removed_cb (ESourceRegistry *registry,
			     ESource *source,
			     gpointer user_data)
{
	EYearView *self = user_data;

	g_hash_table_remove (self->priv->client_colors, source);
}

static guint
year_view_get_current_day_of_year (EYearView *self)
{
	GDate dt;

	g_date_clear (&dt, 1);
	g_date_set_dmy (&dt, self->priv->current_day, self->priv->current_month, self->priv->current_year);

	return g_date_get_day_of_year (&dt);
}

static void
year_view_add_to_view (EYearView *self,
		       ComponentData *cd)
{
	ICalTime *itt;
	gboolean is_italic;
	guint day_of_year;
	guint ii;

	day_of_year = year_view_get_current_day_of_year (self);
	is_italic = e_cal_component_get_transparency (cd->comp) == E_CAL_COMPONENT_TRANSP_TRANSPARENT;
	itt = i_cal_time_new_from_day_of_year (cd->day_from, self->priv->current_year);

	for (ii = cd->day_from; ii <= cd->day_to; ii++) {
		gchar *tooltip;
		guint month, day;

		g_return_if_fail (ii < sizeof (self->priv->days));

		month = i_cal_time_get_month (itt);
		day = i_cal_time_get_day (itt);

		self->priv->days[ii].comps_data = g_slist_prepend (self->priv->days[ii].comps_data, cd);
		self->priv->days[ii].n_total++;
		e_month_widget_add_day_css_class (self->priv->months[month - 1], day, E_MONTH_WIDGET_CSS_CLASS_UNDERLINE);
		if (is_italic) {
			e_month_widget_add_day_css_class (self->priv->months[month - 1], day, E_MONTH_WIDGET_CSS_CLASS_ITALIC);
			self->priv->days[ii].n_italic++;
		} else {
			e_month_widget_add_day_css_class (self->priv->months[month - 1], day, E_MONTH_WIDGET_CSS_CLASS_BOLD);
		}

		tooltip = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%u event", "%u events", self->priv->days[ii].n_total), self->priv->days[ii].n_total);

		e_month_widget_set_day_tooltip_markup (self->priv->months[month - 1], day, tooltip);

		g_free (tooltip);

		if (ii == day_of_year)
			year_view_add_to_list_store (self, cd);

		i_cal_time_adjust (itt, 1, 0, 0, 0);
	}

	g_clear_object (&itt);
}

static void
year_view_remove_from_view (EYearView *self,
			    ComponentData *cd)
{
	ICalTime *itt;
	gboolean is_italic;
	guint day_of_year;
	guint ii;

	day_of_year = year_view_get_current_day_of_year (self);
	is_italic = e_cal_component_get_transparency (cd->comp) == E_CAL_COMPONENT_TRANSP_TRANSPARENT;
	itt = i_cal_time_new_from_day_of_year (cd->day_from, self->priv->current_year);

	for (ii = cd->day_from; ii <= cd->day_to; ii++) {
		gchar *tooltip;
		guint month, day;

		g_return_if_fail (ii < sizeof (self->priv->days));

		month = i_cal_time_get_month (itt);
		day = i_cal_time_get_day (itt);

		self->priv->days[ii].comps_data = g_slist_remove (self->priv->days[ii].comps_data, cd);
		self->priv->days[ii].n_total--;
		e_month_widget_remove_day_css_class (self->priv->months[month - 1], day, E_MONTH_WIDGET_CSS_CLASS_UNDERLINE);
		if (is_italic) {
			self->priv->days[ii].n_italic--;
			if (!self->priv->days[ii].n_italic)
				e_month_widget_remove_day_css_class (self->priv->months[month - 1], day, E_MONTH_WIDGET_CSS_CLASS_ITALIC);
		} else if (!self->priv->days[ii].n_total) {
			e_month_widget_remove_day_css_class (self->priv->months[month - 1], day, E_MONTH_WIDGET_CSS_CLASS_BOLD);
		}

		if (self->priv->days[ii].n_total > 0)
			tooltip = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%u event", "%u events", self->priv->days[ii].n_total), self->priv->days[ii].n_total);
		else
			tooltip = NULL;

		e_month_widget_set_day_tooltip_markup (self->priv->months[month - 1], day, tooltip);

		g_free (tooltip);

		if (ii == day_of_year) {
			GtkTreeIter iter;
			GtkTreeModel *model = GTK_TREE_MODEL (self->priv->list_store);

			if (gtk_tree_model_get_iter_first (model, &iter)) {
				do {
					ComponentData *comp_data = NULL;

					gtk_tree_model_get (model, &iter,
						COLUMN_COMPONENT_DATA, &comp_data,
						-1);

					if (comp_data == cd) {
						gtk_list_store_remove (self->priv->list_store, &iter);
						break;
					}
				} while (gtk_tree_model_iter_next (model, &iter));
			}
		}

		i_cal_time_adjust (itt, 1, 0, 0, 0);
	}

	g_clear_object (&itt);
}

static void
year_view_add_component (EYearView *self,
			 ECalClient *client,
			 ECalComponent *comp)
{
	ECalComponentId *id;
	ComponentData *cd, tmp_cd = { 0, };
	guint day_from = 0, day_to = 0, date_mark = 0, time_mark = 0;

	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	id = e_cal_component_get_id (comp);
	g_return_if_fail (id != NULL);

	tmp_cd.client = client;
	tmp_cd.comp = comp;
	tmp_cd.uid = (gchar *) e_cal_component_id_get_uid (id);
	tmp_cd.rid = (gchar *) e_cal_component_id_get_rid (id);

	year_view_calc_component_data (self, &tmp_cd, &day_from, &day_to, &date_mark, &time_mark);

	cd = g_hash_table_lookup (self->priv->comps, &tmp_cd);

	e_cal_component_id_free (id);

	/* The component was modified */
	if (cd) {
		if (day_from != cd->day_from || day_to != cd->day_to ||
		    e_cal_component_get_transparency (comp) != e_cal_component_get_transparency (cd->comp)) {
			year_view_remove_from_view (self, cd);
			g_hash_table_remove (self->priv->comps, cd);
			cd = NULL;
		} else {
			g_object_ref (comp);
			g_clear_object (&cd->comp);
			cd->comp = comp;
		}
	}

	if (cd) {
		guint day_of_year = year_view_get_current_day_of_year (self);

		/* Update the list view */
		if (day_of_year >= day_from && day_of_year <= day_to) {
			GtkTreeModel *model;
			GtkTreeIter iter;

			model = GTK_TREE_MODEL (self->priv->list_store);

			if (gtk_tree_model_get_iter_first (model, &iter)) {
				ICalTimezone *default_zone = e_cal_data_model_get_timezone (self->priv->data_model);
				guint flags = year_view_get_describe_flags (self);

				do {
					ComponentData *comp_data = NULL;

					gtk_tree_model_get (model, &iter,
						COLUMN_COMPONENT_DATA, &comp_data,
						-1);

					if (comp_data == cd) {
						gchar *summary;
						gchar *tooltip;

						summary = cal_comp_util_describe (cd->comp, cd->client, default_zone, flags);
						tooltip = cal_comp_util_dup_tooltip (cd->comp, cd->client, self->priv->registry, default_zone);

						gtk_list_store_set (self->priv->list_store, &iter,
							COLUMN_SUMMARY, summary,
							COLUMN_TOOLTIP, tooltip,
							-1);

						g_free (summary);
						g_free (tooltip);

						if (self->priv->preview_visible) {
							GtkTreeSelection *selection;

							selection = gtk_tree_view_get_selection (self->priv->tree_view);
							if (gtk_tree_selection_iter_is_selected (selection, &iter))
								g_signal_emit_by_name (selection, "changed", 0, NULL);
						}

						break;
					}
				} while (gtk_tree_model_iter_next (model, &iter));
			}
		}
	} else {
		cd = component_data_new (client, comp);
		cd->day_from = day_from;
		cd->day_to = day_to;
		cd->date_mark = date_mark;
		cd->time_mark = time_mark;

		g_hash_table_insert (self->priv->comps, cd, cd);
		year_view_add_to_view (self, cd);
	}
}

static void
year_view_data_subscriber_component_added (ECalDataModelSubscriber *subscriber,
					   ECalClient *client,
					   ECalComponent *comp)
{
	g_return_if_fail (E_IS_YEAR_VIEW (subscriber));

	year_view_add_component (E_YEAR_VIEW (subscriber), client, comp);
}

static void
year_view_data_subscriber_component_modified (ECalDataModelSubscriber *subscriber,
					      ECalClient *client,
					      ECalComponent *comp)
{
	g_return_if_fail (E_IS_YEAR_VIEW (subscriber));

	year_view_add_component (E_YEAR_VIEW (subscriber), client, comp);
}

static void
year_view_data_subscriber_component_removed (ECalDataModelSubscriber *subscriber,
					     ECalClient *client,
					     const gchar *uid,
					     const gchar *rid)
{
	EYearView *self;
	ComponentData *cd, tmp_cd = { 0, };

	g_return_if_fail (E_IS_YEAR_VIEW (subscriber));

	self = E_YEAR_VIEW (subscriber);

	if (self->priv->clearing_comps)
		return;

	tmp_cd.client = client;
	tmp_cd.uid = (gchar *) uid;
	tmp_cd.rid = (gchar *) (rid && *rid ? rid : NULL);

	cd = g_hash_table_lookup (self->priv->comps, &tmp_cd);

	if (cd) {
		year_view_remove_from_view (self, cd);
		g_hash_table_remove (self->priv->comps, cd);
	}
}

static void
year_view_data_subscriber_freeze (ECalDataModelSubscriber *subscriber)
{
	g_return_if_fail (E_IS_YEAR_VIEW (subscriber));
}

static void
year_view_data_subscriber_thaw (ECalDataModelSubscriber *subscriber)
{
	g_return_if_fail (E_IS_YEAR_VIEW (subscriber));
}

static void
year_view_selection_changed_cb (GtkTreeSelection *in_selection, /* can be NULL */
				gpointer user_data)
{
	EYearView *self = user_data;
	GtkTreeSelection *selection;

	if (!self->priv->preview_visible) {
		g_signal_emit_by_name (self, "selection-changed");
		return;
	}

	e_cal_component_preview_clear (self->priv->preview);

	selection = gtk_tree_view_get_selection (self->priv->tree_view);

	if (gtk_tree_selection_count_selected_rows (selection) == 1) {
		GList *selected;
		GtkTreeModel *model = NULL;
		GtkTreeIter iter;

		selected = gtk_tree_selection_get_selected_rows (selection, &model);

		if (selected &&
		    gtk_tree_model_get_iter (model, &iter, selected->data)) {
			ComponentData *cd = NULL;

			gtk_tree_model_get (model, &iter,
				COLUMN_COMPONENT_DATA, &cd,
				-1);

			e_cal_component_preview_display (self->priv->preview,
				cd->client, cd->comp, e_cal_data_model_get_timezone (self->priv->data_model),
				self->priv->use_24hour_format);
		}

		g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);
	}

	g_signal_emit_by_name (self, "selection-changed");
}

static void
year_view_tree_view_popup_menu (EYearView *self,
				GdkEvent *button_event)
{
	e_calendar_view_popup_event (E_CALENDAR_VIEW (self), button_event);
}

static gboolean
year_view_tree_view_popup_menu_cb (GtkWidget *tree_view,
				   gpointer user_data)
{
	EYearView *self = user_data;

	year_view_tree_view_popup_menu (self, NULL);

	return TRUE;
}

static gboolean
year_view_tree_view_button_press_event_cb (GtkWidget *widget,
					   GdkEvent *event,
					   gpointer user_data)
{
	EYearView *self = user_data;

	if (event->type == GDK_BUTTON_PRESS &&
	    gdk_event_triggers_context_menu (event)) {
		GtkTreeSelection *selection;
		GtkTreePath *path;

		selection = gtk_tree_view_get_selection (self->priv->tree_view);
		if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_SINGLE)
			gtk_tree_selection_unselect_all (selection);

		if (gtk_tree_view_get_path_at_pos (self->priv->tree_view, event->button.x, event->button.y, &path, NULL, NULL, NULL)) {
			gtk_tree_selection_select_path (selection, path);
			gtk_tree_view_set_cursor (self->priv->tree_view, path, NULL, FALSE);

			gtk_tree_path_free (path);
		}

		year_view_tree_view_popup_menu (self, event);

		return TRUE;
	}

	return FALSE;
}

static void
year_view_tree_view_row_activated_cb (GtkTreeView *tree_view,
				      GtkTreePath *path,
				      GtkTreeViewColumn *column,
				      gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (tree_view);

	if (gtk_tree_model_get_iter (model, &iter, path)) {
		ComponentData *cd = NULL;

		gtk_tree_model_get (model, &iter,
			COLUMN_COMPONENT_DATA, &cd,
			-1);

		if (cd) {
			e_cal_ops_open_component_in_editor_sync (NULL, cd->client,
				e_cal_component_get_icalcomponent (cd->comp), FALSE);
		}
	}
}

static void
year_view_tree_view_drag_begin_cb (GtkWidget *tree_view,
				   GdkDragContext *context,
				   gpointer user_data)
{
	EYearView *self = user_data;
	GtkTreeSelection *selection;
	cairo_surface_t *surface = NULL;
	GList *selected, *link;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;

	g_slist_free_full (self->priv->drag_data, drag_data_free);
	self->priv->drag_data = NULL;

	selection = gtk_tree_view_get_selection (self->priv->tree_view);
	selected = gtk_tree_selection_get_selected_rows (selection, &model);

	for (link = selected; link; link = g_list_next (link)) {
		if (gtk_tree_model_get_iter (model, &iter, link->data)) {
			ComponentData *cd = NULL;

			gtk_tree_model_get (model, &iter,
				COLUMN_COMPONENT_DATA, &cd,
				-1);

			self->priv->drag_data = g_slist_prepend (self->priv->drag_data,
				drag_data_new (cd->client, cd->comp));

			if (!surface)
				surface = gtk_tree_view_create_row_drag_icon (self->priv->tree_view, link->data);
		}
	}

	g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);

	self->priv->drag_data = g_slist_reverse (self->priv->drag_data);
	self->priv->drag_day = self->priv->current_day;
	self->priv->drag_month = self->priv->current_month;
	self->priv->drag_year = self->priv->current_year;

	if (surface) {
		gtk_drag_set_icon_surface (context, surface);
		cairo_surface_destroy (surface);
	}
}

static void
year_view_tree_view_drag_end_cb (GtkWidget *widget,
				 GdkDragContext *context,
				 gpointer user_data)
{
	EYearView *self = user_data;

	g_slist_free_full (self->priv->drag_data, drag_data_free);
	self->priv->drag_data = NULL;
	self->priv->drag_day = 0;
	self->priv->drag_month = 0;
	self->priv->drag_year = 0;
}

static gboolean
year_view_month_drag_motion_cb (GtkWidget *widget,
				GdkDragContext *context,
				gint x,
				gint y,
				guint time,
				gpointer user_data)
{
	EYearView *self = user_data;
	guint day, year = 0;
	GDateMonth month = 0;
	GdkDragAction drag_action = GDK_ACTION_MOVE;
	gboolean can_drop;

	day = e_month_widget_get_day_at_position (E_MONTH_WIDGET (widget), x, y);
	e_month_widget_get_month (E_MONTH_WIDGET (widget), &month, &year);

	can_drop = day != 0 && self->priv->drag_data && (
		day != self->priv->drag_day ||
		month != self->priv->drag_month ||
		year != self->priv->drag_year);

	if (can_drop) {
		GSList *link;

		can_drop = FALSE;

		for (link = self->priv->drag_data; link && !can_drop; link = g_slist_next (link)) {
			DragData *dd = link->data;

			can_drop = !e_client_is_readonly (E_CLIENT (dd->client));
		}
	}

	if (can_drop) {
		GdkModifierType mask;

		gdk_window_get_pointer (gtk_widget_get_window (widget), NULL, NULL, &mask);

		if ((mask & GDK_CONTROL_MASK) != 0)
			drag_action = GDK_ACTION_COPY;
	}

	gdk_drag_status (context, can_drop ? drag_action : 0, time);

	return TRUE;
}

static gboolean
year_view_month_drag_drop_cb (GtkWidget *widget,
			      GdkDragContext *context,
			      gint x,
			      gint y,
			      guint time,
			      gpointer user_data)
{
	EYearView *self = user_data;
	guint day, year = 0;
	GDateMonth month = 0;
	gboolean can_drop;

	day = e_month_widget_get_day_at_position (E_MONTH_WIDGET (widget), x, y);
	e_month_widget_get_month (E_MONTH_WIDGET (widget), &month, &year);

	can_drop = day != 0 && self->priv->drag_data && (
		day != self->priv->drag_day ||
		month != self->priv->drag_month ||
		year != self->priv->drag_year);

	if (can_drop) {
		GDate *from, *to;
		gint diff_days;

		from = g_date_new_dmy (self->priv->drag_day, self->priv->drag_month, self->priv->drag_year);
		to = g_date_new_dmy (day, month, year);

		diff_days = g_date_days_between (from, to);

		if (diff_days != 0) {
			ECalModel *model = e_calendar_view_get_model (E_CALENDAR_VIEW (self));
			GtkWidget *toplevel;
			GtkWindow *parent;
			GSList *drag_data, *link;
			gboolean is_move;

			drag_data = g_steal_pointer (&self->priv->drag_data);
			toplevel = gtk_widget_get_toplevel (widget);
			parent = GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL;
			is_move = gdk_drag_context_get_selected_action (context) == GDK_ACTION_MOVE;

			for (link = drag_data; link; link = g_slist_next (link)) {
				DragData *dd = link->data;
				if (!cal_comp_util_move_component_by_days (parent, model,
					dd->client, dd->comp, diff_days, is_move))
					break;
			}

			g_slist_free_full (drag_data, drag_data_free);
		}

		g_date_free (from);
		g_date_free (to);
	}

	gdk_drag_status (context, 0, time);

	return FALSE;
}

static void
year_view_timezone_changed_cb (GObject *object,
			       GParamSpec *param,
			       gpointer user_data)
{
	EYearView *self = user_data;

	self->priv->current_year--;

	/* This updates everything */
	year_view_set_year (self, self->priv->current_year + 1, 0, 0);
}

static GtkWidget *
year_view_construct_year_widget (EYearView *self)
{
	GtkWidget *widget, *top_container, *container, *hbox;
	GtkStyleContext *style_context;
	GtkStyleProvider *style_provider;
	ECalModel *model;
	GSettings *settings;
	GDate *date;
	gint ii;

	style_provider = GTK_STYLE_PROVIDER (self->priv->css_provider);

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (self));
	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);

	gtk_style_context_add_class (gtk_widget_get_style_context (widget), GTK_STYLE_CLASS_VIEW);

	top_container = widget;
	container = widget;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);

	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_CENTER,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"margin-top", 12,
		"margin-bottom", 6,
		NULL);

	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	hbox = widget;

	#ifdef WITH_PREV_NEXT_BUTTONS
	widget = gtk_button_new_from_icon_name ("go-previous-symbolic", GTK_ICON_SIZE_BUTTON);

	g_object_set (G_OBJECT (widget),
		"valign", GTK_ALIGN_BASELINE,
		NULL);

	gtk_style_context_add_class (gtk_widget_get_style_context (widget), GTK_STYLE_CLASS_FLAT);

	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	g_signal_connect (widget, "clicked", G_CALLBACK (year_view_prev_year_clicked_cb), self);
	#endif

	widget = gtk_button_new ();
	self->priv->prev_year_button2 = GTK_BUTTON (widget);

	g_object_set (G_OBJECT (widget),
		"valign", GTK_ALIGN_BASELINE,
		NULL);

	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_provider (style_context, style_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_FLAT);
	gtk_style_context_add_class (style_context, "prev-year");

	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	g_signal_connect (widget, "clicked", G_CALLBACK (year_view_prev_year2_clicked_cb), self);

	widget = gtk_button_new ();
	self->priv->prev_year_button1 = GTK_BUTTON (widget);

	g_object_set (G_OBJECT (widget),
		"valign", GTK_ALIGN_BASELINE,
		NULL);

	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_provider (style_context, style_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_FLAT);
	gtk_style_context_add_class (style_context, "prev-year");

	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	g_signal_connect (widget, "clicked", G_CALLBACK (year_view_prev_year_clicked_cb), self);

	widget = gtk_label_new ("");
	self->priv->current_year_label = GTK_LABEL (widget);

	g_object_set (G_OBJECT (widget),
		"valign", GTK_ALIGN_BASELINE,
		NULL);

	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_provider (style_context, style_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_style_context_add_class (style_context, "current-year");

	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	widget = gtk_button_new ();
	self->priv->next_year_button1 = GTK_BUTTON (widget);

	g_object_set (G_OBJECT (widget),
		"valign", GTK_ALIGN_BASELINE,
		NULL);

	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_provider (style_context, style_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_FLAT);
	gtk_style_context_add_class (style_context, "next-year");

	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	g_signal_connect (widget, "clicked", G_CALLBACK (year_view_next_year_clicked_cb), self);

	widget = gtk_button_new ();
	self->priv->next_year_button2 = GTK_BUTTON (widget);

	g_object_set (G_OBJECT (widget),
		"valign", GTK_ALIGN_BASELINE,
		NULL);

	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_provider (style_context, style_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_FLAT);
	gtk_style_context_add_class (style_context, "next-year");

	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	g_signal_connect (widget, "clicked", G_CALLBACK (year_view_next_year2_clicked_cb), self);

	#ifdef WITH_PREV_NEXT_BUTTONS
	widget = gtk_button_new_from_icon_name ("go-next-symbolic", GTK_ICON_SIZE_BUTTON);

	g_object_set (G_OBJECT (widget),
		"valign", GTK_ALIGN_BASELINE,
		NULL);

	gtk_style_context_add_class (gtk_widget_get_style_context (widget), GTK_STYLE_CLASS_FLAT);

	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	g_signal_connect (widget, "clicked", G_CALLBACK (year_view_next_year_clicked_cb), self);
	#endif

	widget = gtk_scrolled_window_new (NULL, NULL);

	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"min-content-width", 50,
		"min-content-height", 50,
		NULL);

	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_provider (style_context, style_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_FLAT);
	gtk_style_context_add_class (style_context, "calendar-window");

	gtk_container_add (GTK_CONTAINER (top_container), widget);

	container = widget;

	widget = gtk_flow_box_new ();

	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"column-spacing", 12,
		"row-spacing", 12,
		"homogeneous", TRUE,
		"min-children-per-line", 1,
		"max-children-per-line", 6,
		"selection-mode", GTK_SELECTION_NONE,
		NULL);

	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_provider (style_context, style_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_VIEW);
	gtk_style_context_add_class (style_context, "calendar-flowbox");

	gtk_container_add (GTK_CONTAINER (container), widget);
	container = widget;

	/* The date is used only for the month name */
	date = g_date_new_dmy (1, 1, self->priv->current_year);

	for (ii = 0; ii < 12; ii++) {
		GtkFlowBoxChild *child;
		GtkWidget *vbox;
		gchar buffer[128];

		g_date_strftime (buffer, sizeof (buffer), "%B", date);
		g_date_add_months (date, 1);

		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

		widget = gtk_label_new (buffer);

		g_object_set (G_OBJECT (widget),
			"halign", GTK_ALIGN_CENTER,
			"valign", GTK_ALIGN_CENTER,
			"xalign", 0.5,
			"yalign", 0.5,
			NULL);

		gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

		widget = e_month_widget_new ();

		g_object_set (G_OBJECT (widget),
			"halign", GTK_ALIGN_CENTER,
			"valign", GTK_ALIGN_CENTER,
			NULL);

		gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

		self->priv->months[ii] = E_MONTH_WIDGET (widget);

		g_signal_connect (widget, "day-clicked",
			G_CALLBACK (year_view_month_widget_day_clicked_cb), self);

		e_binding_bind_property (model, "week-start-day", widget, "week-start-day", G_BINDING_SYNC_CREATE);
		g_settings_bind (settings, "show-week-numbers", widget, "show-week-numbers", G_SETTINGS_BIND_GET);
		g_settings_bind (settings, "year-show-day-names", widget, "show-day-names", G_SETTINGS_BIND_GET);

		e_month_widget_set_month (E_MONTH_WIDGET (widget), ii + 1, self->priv->current_year);

		gtk_drag_dest_set (
			widget, GTK_DEST_DEFAULT_ALL,
			target_table, G_N_ELEMENTS (target_table),
			GDK_ACTION_COPY | GDK_ACTION_MOVE);

		g_signal_connect_object (widget, "drag-motion",
			G_CALLBACK (year_view_month_drag_motion_cb), self, 0);

		g_signal_connect_object (widget, "drag-drop",
			G_CALLBACK (year_view_month_drag_drop_cb), self, 0);

		gtk_container_add (GTK_CONTAINER (container), vbox);

		child = gtk_flow_box_get_child_at_index (GTK_FLOW_BOX (container), ii);

		g_object_set (G_OBJECT (child),
			"halign", GTK_ALIGN_CENTER,
			"valign", GTK_ALIGN_START,
			NULL);
	}

	g_clear_object (&settings);
	g_date_free (date);

	gtk_widget_show_all (top_container);

	return top_container;
}

static void
year_view_set_property (GObject *object,
			guint property_id,
			const GValue *value,
			GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREVIEW_VISIBLE:
			e_year_view_set_preview_visible (
				E_YEAR_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_USE_24HOUR_FORMAT:
			e_year_view_set_use_24hour_format (
				E_YEAR_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_HIGHLIGHT_TODAY:
			e_year_view_set_highlight_today (
				E_YEAR_VIEW (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
year_view_get_property (GObject *object,
			guint property_id,
			GValue *value,
			GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IS_EDITING:
			g_value_set_boolean (value, FALSE);
			return;

		case PROP_PREVIEW_VISIBLE:
			g_value_set_boolean (value,
				e_year_view_get_preview_visible (E_YEAR_VIEW (object)));
			return;

		case PROP_USE_24HOUR_FORMAT:
			g_value_set_boolean (value,
				e_year_view_get_use_24hour_format (E_YEAR_VIEW (object)));
			return;

		case PROP_HIGHLIGHT_TODAY:
			g_value_set_boolean (value,
				e_year_view_get_highlight_today (E_YEAR_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
year_view_constructed (GObject *object)
{
	EYearView *self = E_YEAR_VIEW (object);
	EAttachmentBar *attachment_bar;
	EAttachmentStore *attachment_store;
	ECalModel *model;
	GSettings *settings;
	GtkWidget *widget;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GError *error = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_year_view_parent_class)->constructed (object);

	self->priv->registry = e_source_registry_new_sync (NULL, &error);

	if (self->priv->registry) {
		g_signal_connect_object (self->priv->registry, "source-changed",
			G_CALLBACK (year_view_source_changed_cb), self, 0);
		g_signal_connect_object (self->priv->registry, "source-disabled",
			G_CALLBACK (year_view_source_removed_cb), self, 0);
		g_signal_connect_object (self->priv->registry, "source-removed",
			G_CALLBACK (year_view_source_removed_cb), self, 0);
	} else {
		g_warning ("%s: Failed to create source registry: %s", G_STRFUNC, error ? error->message : "Unknown error");
		g_clear_error (&error);
	}

	self->priv->css_provider = gtk_css_provider_new ();

	if (!gtk_css_provider_load_from_data (self->priv->css_provider,
		"EYearView .prev-year {"
		"   font-size:90%;"
		"}"
		"EYearView .current-year {"
		"   font-size:120%;"
		"   font-weight:bold;"
		"}"
		"EYearView .next-year {"
		"   font-size:90%;"
		"}"
		"EYearView .calendar-window {"
		"   border-top: 1px solid @theme_bg_color;"
		"}"
		"EYearView .calendar-flowbox {"
		"   padding-top: 12px;"
		"   padding-bottom: 12px;"
		"}",
		-1, &error)) {
		g_warning ("%s: Failed to parse CSS: %s", G_STRFUNC, error ? error->message : "Unknown error");
		g_clear_error (&error);
	}

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (self));
	self->priv->data_model = g_object_ref (e_cal_model_get_data_model (model));

	self->priv->preview_paned = e_paned_new (GTK_ORIENTATION_HORIZONTAL);

	g_object_set (G_OBJECT (self->priv->preview_paned),
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"vexpand", TRUE,
		"visible", TRUE,
		NULL);

	gtk_grid_attach (GTK_GRID (self), self->priv->preview_paned, 0, 0, 1, 1);

	self->priv->hpaned = e_paned_new (GTK_ORIENTATION_HORIZONTAL);

	g_object_set (G_OBJECT (self->priv->hpaned),
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"vexpand", TRUE,
		"visible", TRUE,
		NULL);

	gtk_paned_pack1 (GTK_PANED (self->priv->preview_paned), self->priv->hpaned, TRUE, FALSE);

	attachment_store = E_ATTACHMENT_STORE (e_attachment_store_new ());
	widget = e_attachment_bar_new (attachment_store);
	gtk_widget_set_visible (widget, TRUE);
	self->priv->attachment_bar = widget;
	attachment_bar = E_ATTACHMENT_BAR (widget);

	gtk_paned_pack2 (GTK_PANED (self->priv->preview_paned), widget, FALSE, FALSE);

	e_binding_bind_property_full (
		attachment_store, "num-attachments",
		attachment_bar, "attachments-visible",
		G_BINDING_SYNC_CREATE,
		e_attachment_store_transform_num_attachments_to_visible_boolean,
		NULL, NULL, NULL);

	self->priv->preview = E_CAL_COMPONENT_PREVIEW (e_cal_component_preview_new ());
	g_object_set (G_OBJECT (self->priv->preview),
		"width-request", 50,
		"height-request", 50,
		"visible", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (e_attachment_bar_get_content_area (attachment_bar)), GTK_WIDGET (self->priv->preview), TRUE, TRUE, 0);

	e_cal_component_preview_set_attachment_store (self->priv->preview, attachment_store);
	g_clear_object (&attachment_store);

	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (widget),
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"min-content-width", 50,
		"min-content-height", 50,
		"visible", TRUE,
		NULL);
	gtk_paned_pack1 (GTK_PANED (self->priv->hpaned), widget, TRUE, FALSE);

	gtk_container_add (GTK_CONTAINER (widget), year_view_construct_year_widget (self));

	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (widget),
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"min-content-width", 50,
		"min-content-height", 50,
		"visible", TRUE,
		NULL);

	gtk_paned_pack2 (GTK_PANED (self->priv->hpaned), widget, FALSE, FALSE);

	self->priv->list_store = gtk_list_store_new (N_COLUMNS,
		GDK_TYPE_RGBA,		/* COLUMN_BGCOLOR */
		GDK_TYPE_RGBA,		/* COLUMN_FGCOLOR */
		G_TYPE_BOOLEAN,		/* COLUMN_HAS_ICON_NAME */
		G_TYPE_STRING,		/* COLUMN_ICON_NAME */
		G_TYPE_STRING,		/* COLUMN_SUMMARY */
		G_TYPE_STRING,		/* COLUMN_TOOLTIP */
		G_TYPE_STRING,		/* COLUMN_SORTKEY */
		G_TYPE_POINTER);	/* COLUMN_COMPONENT_DATA */

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self->priv->list_store), COLUMN_SORTKEY, GTK_SORT_ASCENDING);

	self->priv->tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());

	g_object_set (G_OBJECT (self->priv->tree_view),
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"vexpand", TRUE,
		"visible", TRUE,
		"fixed-height-mode", TRUE,
		"headers-clickable", FALSE,
		"headers-visible", TRUE,
		"reorderable", FALSE,
		"search-column", COLUMN_SUMMARY,
		"tooltip-column", COLUMN_TOOLTIP,
		"enable-grid-lines", GTK_TREE_VIEW_GRID_LINES_HORIZONTAL,
		"model", self->priv->list_store,
		NULL);

	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (self->priv->tree_view));

	column = gtk_tree_view_column_new ();

	g_object_set (G_OBJECT (column),
		"expand", TRUE,
		"clickable", FALSE,
		"resizable", FALSE,
		"reorderable", FALSE,
		"sizing", GTK_TREE_VIEW_COLUMN_FIXED,
		"alignment", 0.5f,
		NULL);

	renderer = gtk_cell_renderer_pixbuf_new ();

	gtk_tree_view_column_pack_start (column, renderer, FALSE);

	gtk_tree_view_column_set_attributes (column, renderer,
		"cell-background-rgba", COLUMN_BGCOLOR,
		"icon-name", COLUMN_ICON_NAME,
		"visible", COLUMN_HAS_ICON_NAME,
		NULL);

	renderer = gtk_cell_renderer_text_new ();

	g_object_set (G_OBJECT (renderer),
		"ellipsize", PANGO_ELLIPSIZE_END,
		NULL);

	gtk_tree_view_column_pack_start (column, renderer, TRUE);

	gtk_tree_view_column_set_attributes (column, renderer,
		"markup", COLUMN_SUMMARY,
		"background-rgba", COLUMN_BGCOLOR,
		"foreground-rgba", COLUMN_FGCOLOR,
		NULL);

	gtk_tree_view_append_column (self->priv->tree_view, column);

	gtk_drag_source_set (GTK_WIDGET (self->priv->tree_view), GDK_BUTTON1_MASK,
		target_table, G_N_ELEMENTS (target_table),
		GDK_ACTION_COPY | GDK_ACTION_MOVE);

	selection = gtk_tree_view_get_selection (self->priv->tree_view);

	g_signal_connect_object (selection, "changed",
		G_CALLBACK (year_view_selection_changed_cb), self, 0);

	g_signal_connect_object (self->priv->tree_view, "popup-menu",
		G_CALLBACK (year_view_tree_view_popup_menu_cb), self, 0);

	g_signal_connect_object (self->priv->tree_view, "button-press-event",
		G_CALLBACK (year_view_tree_view_button_press_event_cb), self, 0);

	g_signal_connect_object (self->priv->tree_view, "row-activated",
		G_CALLBACK (year_view_tree_view_row_activated_cb), self, 0);

	g_signal_connect_object (self->priv->tree_view, "drag-begin",
		G_CALLBACK (year_view_tree_view_drag_begin_cb), self, 0);

	g_signal_connect_object (self->priv->tree_view, "drag-end",
		G_CALLBACK (year_view_tree_view_drag_end_cb), self, 0);

	g_signal_connect_object (self->priv->data_model, "notify::timezone",
		G_CALLBACK (year_view_timezone_changed_cb), self, 0);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_bind (
		settings, "year-hpane-position",
		self->priv->hpaned, "hposition",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
		settings, "use-24hour-format",
		self, "use-24hour-format",
		G_SETTINGS_BIND_GET);

	if (e_year_view_get_preview_orientation (self) == GTK_ORIENTATION_HORIZONTAL) {
		g_settings_bind (
			settings, "year-hpreview-position",
			self->priv->preview_paned, "hposition",
			G_SETTINGS_BIND_DEFAULT);
	} else {
		g_settings_bind (
			settings, "year-vpreview-position",
			self->priv->preview_paned, "vposition",
			G_SETTINGS_BIND_DEFAULT);
	}

	g_object_unref (settings);

	/* To update the top year buttons */
	self->priv->current_year--;
	year_view_set_year (self, self->priv->current_year + 1, 0, 0);
}

static void
year_view_dispose (GObject *object)
{
	EYearView *self = E_YEAR_VIEW (object);

	if (self->priv->data_model) {
		self->priv->clearing_comps = TRUE;
		year_view_clear_comps (self);
		e_cal_data_model_unsubscribe (self->priv->data_model, E_CAL_DATA_MODEL_SUBSCRIBER (self));
		self->priv->clearing_comps = FALSE;
	}

	if (self->priv->today_source_id) {
		g_source_remove (self->priv->today_source_id);
		self->priv->today_source_id = 0;
	}

	g_clear_object (&self->priv->registry);
	g_clear_object (&self->priv->list_store);
	g_clear_object (&self->priv->data_model);
	g_clear_object (&self->priv->css_provider);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_year_view_parent_class)->dispose (object);
}

static void
year_view_finalize (GObject *object)
{
	EYearView *self = E_YEAR_VIEW (object);

	year_view_clear_comps (self);

	g_slist_free_full (self->priv->drag_data, drag_data_free);
	g_hash_table_destroy (self->priv->client_colors);
	g_hash_table_destroy (self->priv->comps);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_year_view_parent_class)->finalize (object);
}

static void
e_year_view_class_init (EYearViewClass *klass)
{
	GObjectClass *object_class;
	ECalendarViewClass *view_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = year_view_set_property;
	object_class->get_property = year_view_get_property;
	object_class->constructed = year_view_constructed;
	object_class->dispose = year_view_dispose;
	object_class->finalize = year_view_finalize;

	gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (klass), "EYearView");

	view_class = E_CALENDAR_VIEW_CLASS (klass);
	view_class->get_selected_events = year_view_get_selected_events;
	view_class->get_selected_time_range = year_view_get_selected_time_range;
	view_class->set_selected_time_range = year_view_set_selected_time_range;
	view_class->get_visible_time_range = year_view_get_visible_time_range;
	view_class->precalc_visible_time_range = year_view_precalc_visible_time_range;
	view_class->paste_text = year_view_paste_text;

	g_object_class_override_property (
		object_class,
		PROP_IS_EDITING,
		"is-editing");

	obj_props[PROP_PREVIEW_VISIBLE] =
		g_param_spec_boolean ("preview-visible", NULL, NULL,
			TRUE,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	obj_props[PROP_USE_24HOUR_FORMAT] =
		g_param_spec_boolean ("use-24hour-format", NULL, NULL,
			FALSE,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	obj_props[PROP_HIGHLIGHT_TODAY] =
		g_param_spec_boolean ("highlight-today", NULL, NULL,
			TRUE,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, LAST_PROP, obj_props);
}

static void
e_year_view_init (EYearView *self)
{
	self->priv = e_year_view_get_instance_private (self);
	self->priv->preview_visible = TRUE;
	self->priv->highlight_today = TRUE;
	self->priv->current_day = 1;
	self->priv->current_month = 1;
	self->priv->current_year = 2000;

	self->priv->client_colors = g_hash_table_new_full (g_direct_hash, g_direct_equal,
		NULL, (GDestroyNotify) gdk_rgba_free);

	self->priv->comps = g_hash_table_new_full (component_data_hash, component_data_equal,
		component_data_free, NULL);
}

static void
year_view_cal_data_model_subscriber_init (ECalDataModelSubscriberInterface *iface)
{
	iface->component_added = year_view_data_subscriber_component_added;
	iface->component_modified = year_view_data_subscriber_component_modified;
	iface->component_removed = year_view_data_subscriber_component_removed;
	iface->freeze = year_view_data_subscriber_freeze;
	iface->thaw = year_view_data_subscriber_thaw;
}

ECalendarView *
e_year_view_new (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return g_object_new (E_TYPE_YEAR_VIEW, "model", model, NULL);
}

void
e_year_view_set_preview_visible (EYearView *self,
				 gboolean value)
{
	g_return_if_fail (E_IS_YEAR_VIEW (self));

	if ((self->priv->preview_visible ? 1 : 0) == (value ? 1 : 0))
		return;

	self->priv->preview_visible = value;

	gtk_widget_set_visible (GTK_WIDGET (self->priv->attachment_bar), self->priv->preview_visible);

	if (self->priv->preview_visible)
		year_view_selection_changed_cb (NULL, self);
	else
		e_cal_component_preview_clear (self->priv->preview);

	e_year_view_update_actions (self);

	g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_PREVIEW_VISIBLE]);
}

gboolean
e_year_view_get_preview_visible (EYearView *self)
{
	g_return_val_if_fail (E_IS_YEAR_VIEW (self), FALSE);

	return self->priv->preview_visible;
}

void
e_year_view_set_preview_orientation (EYearView *self,
				     GtkOrientation value)
{
	GSettings *settings;

	g_return_if_fail (E_IS_YEAR_VIEW (self));

	if (gtk_orientable_get_orientation (GTK_ORIENTABLE (self->priv->preview_paned)) == value)
		return;

	g_settings_unbind (self->priv->preview_paned, "hposition");
	g_settings_unbind (self->priv->preview_paned, "vposition");

	gtk_orientable_set_orientation (GTK_ORIENTABLE (self->priv->preview_paned), value);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	if (value == GTK_ORIENTATION_HORIZONTAL) {
		g_settings_bind (
			settings, "year-hpreview-position",
			self->priv->preview_paned, "hposition",
			G_SETTINGS_BIND_DEFAULT);
	} else {
		g_settings_bind (
			settings, "year-vpreview-position",
			self->priv->preview_paned, "vposition",
			G_SETTINGS_BIND_DEFAULT);
	}

	g_clear_object (&settings);
}

GtkOrientation
e_year_view_get_preview_orientation (EYearView *self)
{
	g_return_val_if_fail (E_IS_YEAR_VIEW (self), GTK_ORIENTATION_HORIZONTAL);

	return gtk_orientable_get_orientation (GTK_ORIENTABLE (self->priv->preview_paned));
}

void
e_year_view_set_use_24hour_format (EYearView *self,
				   gboolean value)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	g_return_if_fail (E_IS_YEAR_VIEW (self));

	if ((self->priv->use_24hour_format ? 1 : 0) == (value ? 1 : 0))
		return;

	self->priv->use_24hour_format = value;

	model = GTK_TREE_MODEL (self->priv->list_store);

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		ICalTimezone *default_zone = e_cal_data_model_get_timezone (self->priv->data_model);
		guint flags = year_view_get_describe_flags (self);

		do {
			ComponentData *cd = NULL;

			gtk_tree_model_get (model, &iter,
				COLUMN_COMPONENT_DATA, &cd,
				-1);

			if (cd) {
				gchar *summary;

				summary = cal_comp_util_describe (cd->comp, cd->client, default_zone, flags);

				gtk_list_store_set (self->priv->list_store, &iter,
					COLUMN_SUMMARY, summary,
					-1);

				g_free (summary);
			}
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_USE_24HOUR_FORMAT]);
}

gboolean
e_year_view_get_use_24hour_format (EYearView *self)
{
	g_return_val_if_fail (E_IS_YEAR_VIEW (self), FALSE);

	return self->priv->use_24hour_format;
}

void
e_year_view_set_highlight_today (EYearView *self,
				 gboolean value)
{
	g_return_if_fail (E_IS_YEAR_VIEW (self));

	if ((self->priv->highlight_today ? 1 : 0) == (value ? 1 : 0))
		return;

	self->priv->highlight_today = value;

	year_view_update_today (self);

	g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_HIGHLIGHT_TODAY]);
}

gboolean
e_year_view_get_highlight_today (EYearView *self)
{
	g_return_val_if_fail (E_IS_YEAR_VIEW (self), FALSE);

	return self->priv->highlight_today;
}

void
e_year_view_update_actions (EYearView *self)
{
	g_return_if_fail (E_IS_YEAR_VIEW (self));

	if (e_year_view_get_preview_visible (self))
		e_web_view_update_actions (E_WEB_VIEW (self->priv->preview));
}
