/*
 * ECalListView - display calendar events in an ETable.
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
 * Authors:
 *		Hans Petter Jansson  <hpj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-cal-list-view.h"
#include "ea-calendar.h"

#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include "e-cal-model-calendar.h"
#include "e-cell-date-edit-text.h"
#include "comp-util.h"
#include "itip-utils.h"
#include "calendar-config.h"
#include "misc.h"

enum {
	PROP_0,
	PROP_IS_EDITING
};

static void      e_cal_list_view_dispose                (GObject *object);

static GList    *e_cal_list_view_get_selected_events    (ECalendarView *cal_view);
static gboolean  e_cal_list_view_get_selected_time_range (ECalendarView *cal_view, time_t *start_time, time_t *end_time);
static gboolean  e_cal_list_view_get_visible_time_range (ECalendarView *cal_view, time_t *start_time,
							 time_t *end_time);

static gboolean  e_cal_list_view_popup_menu             (GtkWidget *widget);

static void      e_cal_list_view_show_popup_menu        (ECalListView *cal_list_view, gint row,
							 GdkEvent *event);
static gboolean  e_cal_list_view_on_table_double_click   (GtkWidget *table, gint row, gint col,
							 GdkEvent *event, gpointer data);
static gboolean  e_cal_list_view_on_table_right_click   (GtkWidget *table, gint row, gint col,
							 GdkEvent *event, gpointer data);
static void e_cal_list_view_cursor_change_cb (ETable *etable, gint row, gpointer data);

G_DEFINE_TYPE (ECalListView, e_cal_list_view, E_TYPE_CALENDAR_VIEW)

static void
e_cal_list_view_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	ECalListView *eclv = E_CAL_LIST_VIEW (object);

	switch (property_id) {
	case PROP_IS_EDITING:
		g_value_set_boolean (value, e_cal_list_view_is_editing (eclv));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_cal_list_view_class_init (ECalListViewClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	ECalendarViewClass *view_class;

	object_class = (GObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;
	view_class = (ECalendarViewClass *) class;

	/* Method override */
	object_class->dispose = e_cal_list_view_dispose;
	object_class->get_property = e_cal_list_view_get_property;

	widget_class->popup_menu = e_cal_list_view_popup_menu;

	view_class->get_selected_events = e_cal_list_view_get_selected_events;
	view_class->get_selected_time_range = e_cal_list_view_get_selected_time_range;
	view_class->get_visible_time_range = e_cal_list_view_get_visible_time_range;

	g_object_class_override_property (
		object_class,
		PROP_IS_EDITING,
		"is-editing");
}

static void
e_cal_list_view_init (ECalListView *cal_list_view)
{
	cal_list_view->table = NULL;
	cal_list_view->cursor_event = NULL;
	cal_list_view->set_table_id = 0;
}

/* Returns the current time, for the ECellDateEdit items. */
static struct tm
get_current_time_cb (ECellDateEdit *ecde,
                     gpointer data)
{
	ECalListView *cal_list_view = data;
	icaltimezone *zone;
	struct tm tmp_tm = { 0 };
	struct icaltimetype tt;

	zone = e_calendar_view_get_timezone (E_CALENDAR_VIEW (cal_list_view));
	tt = icaltime_from_timet_with_zone (time (NULL), FALSE, zone);

	/* Now copy it to the struct tm and return it. */
	tmp_tm.tm_year = tt.year - 1900;
	tmp_tm.tm_mon = tt.month - 1;
	tmp_tm.tm_mday = tt.day;
	tmp_tm.tm_hour = tt.hour;
	tmp_tm.tm_min = tt.minute;
	tmp_tm.tm_sec = tt.second;
	tmp_tm.tm_isdst = -1;

	return tmp_tm;
}

static void
e_cal_list_view_table_editing_changed_cb (ETable *table,
                                          GParamSpec *param,
                                          ECalListView *eclv)
{
	g_return_if_fail (E_IS_CAL_LIST_VIEW (eclv));

	g_object_notify (G_OBJECT (eclv), "is-editing");
}

static void
setup_e_table (ECalListView *cal_list_view)
{
	ECalModel *model;
	ETableExtras *extras;
	ETableSpecification *specification;
	GList *strings;
	ECell *cell, *popup_cell;
	GtkWidget *container;
	GtkWidget *widget;
	gchar *etspecfile;
	GError *local_error = NULL;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (cal_list_view));

	/* Create the header columns */

	extras = e_table_extras_new ();

	/* Normal string fields */

	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell,
		"bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		NULL);

	e_table_extras_add_cell (extras, "calstring", cell);
	g_object_unref (cell);

	/* Date fields */

	cell = e_cell_date_edit_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell,
		"bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		NULL);

	e_binding_bind_property (
		model, "timezone",
		cell, "timezone",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		model, "use-24-hour-format",
		cell, "use-24-hour-format",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	popup_cell = e_cell_date_edit_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	e_binding_bind_property (
		model, "use-24-hour-format",
		popup_cell, "use-24-hour-format",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_table_extras_add_cell (extras, "dateedit", popup_cell);
	g_object_unref (popup_cell);
	cal_list_view->dates_cell = E_CELL_DATE_EDIT (popup_cell);

	gtk_widget_hide (E_CELL_DATE_EDIT (popup_cell)->none_button);

	e_cell_date_edit_set_get_time_callback (
		E_CELL_DATE_EDIT (popup_cell),
		get_current_time_cb,
		cal_list_view, NULL);

	/* Combo fields */

	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell,
		"bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		"editable", FALSE,
		NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (gchar *) _("Public"));
	strings = g_list_append (strings, (gchar *) _("Private"));
	strings = g_list_append (strings, (gchar *) _("Confidential"));
	e_cell_combo_set_popdown_strings (
		E_CELL_COMBO (popup_cell),
		strings);
	g_list_free (strings);

	e_table_extras_add_cell (extras, "classification", popup_cell);
	g_object_unref (popup_cell);

	/* Sorting */

	e_table_extras_add_compare (
		extras, "date-compare",
		e_cell_date_edit_compare_cb);

	/* set proper format component for a default 'date' cell renderer */
	cell = e_table_extras_get_cell (extras, "date");
	e_cell_date_set_format_component (E_CELL_DATE (cell), "calendar");

	/* Create table view */

	container = GTK_WIDGET (cal_list_view);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 2, 2);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"vexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (widget);

	container = widget;

	etspecfile = g_build_filename (
		EVOLUTION_ETSPECDIR, "e-cal-list-view.etspec", NULL);
	specification = e_table_specification_new (etspecfile, &local_error);

	/* Failure here is fatal. */
	if (local_error != NULL) {
		g_error ("%s: %s", etspecfile, local_error->message);
		g_return_if_reached ();
	}

	widget = e_table_new (E_TABLE_MODEL (model), extras, specification);
	gtk_container_add (GTK_CONTAINER (container), widget);
	cal_list_view->table = E_TABLE (widget);
	gtk_widget_show (widget);

	g_object_unref (specification);
	g_object_unref (extras);
	g_free (etspecfile);

	/* Connect signals */
	g_signal_connect (
		cal_list_view->table, "double_click",
		G_CALLBACK (e_cal_list_view_on_table_double_click),
		cal_list_view);
	g_signal_connect (
		cal_list_view->table, "right-click",
		G_CALLBACK (e_cal_list_view_on_table_right_click),
		cal_list_view);
	g_signal_connect_after (
		cal_list_view->table, "cursor_change",
		G_CALLBACK (e_cal_list_view_cursor_change_cb),
		cal_list_view);
	e_signal_connect_notify_after (
		cal_list_view->table, "notify::is-editing",
		G_CALLBACK (e_cal_list_view_table_editing_changed_cb),
		cal_list_view);
}

/**
 * e_cal_list_view_new:
 * @Returns: a new #ECalListView.
 *
 * Creates a new #ECalListView.
 **/
ECalendarView *
e_cal_list_view_new (ECalModel *model)
{
	ECalendarView *cal_list_view;

	cal_list_view = g_object_new (
		E_TYPE_CAL_LIST_VIEW, "model", model, NULL);
	setup_e_table (E_CAL_LIST_VIEW (cal_list_view));

	return cal_list_view;
}

static void
e_cal_list_view_dispose (GObject *object)
{
	ECalListView *cal_list_view;

	cal_list_view = E_CAL_LIST_VIEW (object);

	if (cal_list_view->set_table_id) {
		g_source_remove (cal_list_view->set_table_id);
		cal_list_view->set_table_id = 0;
	}

	if (cal_list_view->cursor_event) {
		g_free (cal_list_view->cursor_event);
		cal_list_view->cursor_event = NULL;
	}

	if (cal_list_view->table) {
		gtk_widget_destroy (GTK_WIDGET (cal_list_view->table));
		cal_list_view->table = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_cal_list_view_parent_class)->dispose (object);
}

static void
e_cal_list_view_show_popup_menu (ECalListView *cal_list_view,
                                 gint row,
                                 GdkEvent *event)
{
	e_calendar_view_popup_event (E_CALENDAR_VIEW (cal_list_view), event);
}

static gboolean
e_cal_list_view_popup_menu (GtkWidget *widget)
{
	ECalListView *cal_list_view = E_CAL_LIST_VIEW (widget);

	e_cal_list_view_show_popup_menu (cal_list_view, -1, NULL);
	return TRUE;
}

static gboolean
e_cal_list_view_on_table_double_click (GtkWidget *table,
                                       gint row,
                                       gint col,
                                       GdkEvent *event,
                                       gpointer data)
{
	ECalListView *cal_list_view = E_CAL_LIST_VIEW (data);
	ECalModelComponent *comp_data;

	comp_data = e_cal_model_get_component_at (e_calendar_view_get_model (E_CALENDAR_VIEW (cal_list_view)), row);
	e_calendar_view_edit_appointment (E_CALENDAR_VIEW (cal_list_view), comp_data->client, comp_data->icalcomp, EDIT_EVENT_AUTODETECT);

	return TRUE;
}

static gboolean
e_cal_list_view_on_table_right_click (GtkWidget *table,
                                      gint row,
                                      gint col,
                                      GdkEvent *event,
                                      gpointer data)
{
	ECalListView *cal_list_view = E_CAL_LIST_VIEW (data);

	e_cal_list_view_show_popup_menu (cal_list_view, row, event);

	return TRUE;
}

static void
e_cal_list_view_cursor_change_cb (ETable *etable,
                                  gint row,
                                  gpointer data)
{
	ECalListView *cal_list_view = E_CAL_LIST_VIEW (data);

	g_signal_emit_by_name (cal_list_view, "selection_changed");
}

static gboolean
e_cal_list_view_get_selected_time_range (ECalendarView *cal_view,
                                         time_t *start_time,
                                         time_t *end_time)
{
	GList *selected;
	icaltimezone *zone;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;
		ECalComponentDateTime dtstart, dtend;
		ECalComponent *comp;

		if (!is_comp_data_valid (event))
			return FALSE;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
		if (start_time) {
			e_cal_component_get_dtstart (comp, &dtstart);
			if (dtstart.tzid) {
				zone = icalcomponent_get_timezone (e_cal_component_get_icalcomponent (comp), dtstart.tzid);
			} else {
				zone = NULL;
			}
			*start_time = icaltime_as_timet_with_zone (*dtstart.value, zone);
			e_cal_component_free_datetime (&dtstart);
		}
		if (end_time) {
			e_cal_component_get_dtend (comp, &dtend);
			if (dtend.tzid) {
				zone = icalcomponent_get_timezone (e_cal_component_get_icalcomponent (comp), dtend.tzid);
			} else {
				zone = NULL;
			}
			*end_time = icaltime_as_timet_with_zone (*dtend.value, zone);
			e_cal_component_free_datetime (&dtend);
		}

		g_object_unref (comp);
		g_list_free (selected);

		return TRUE;
	}

	return FALSE;
}

static GList *
e_cal_list_view_get_selected_events (ECalendarView *cal_view)
{
	GList *event_list = NULL;
	gint   cursor_row;

	if (E_CAL_LIST_VIEW (cal_view)->cursor_event) {
		g_free (E_CAL_LIST_VIEW (cal_view)->cursor_event);
		E_CAL_LIST_VIEW (cal_view)->cursor_event = NULL;
	}

	cursor_row = e_table_get_cursor_row (
		E_CAL_LIST_VIEW (cal_view)->table);

	if (cursor_row >= 0) {
		ECalendarViewEvent *event;

		event = E_CAL_LIST_VIEW (cal_view)->cursor_event = g_new0 (ECalendarViewEvent, 1);
		event->comp_data =
			e_cal_model_get_component_at (
				e_calendar_view_get_model (cal_view),
				cursor_row);
		event_list = g_list_prepend (event_list, event);
	}

	return event_list;
}

static void
adjust_range (icaltimetype icaltime,
              time_t *earliest,
              time_t *latest,
              gboolean *set)
{
	time_t t;

	if (!icaltime_is_valid_time (icaltime))
		return;

	t = icaltime_as_timet (icaltime);
	*earliest = MIN (*earliest, t);
	*latest   = MAX (*latest, t);

	*set = TRUE;
}

/* NOTE: Time use for this function increases linearly with number of events.
 * This is not ideal, since it's used in a couple of places. We could probably
 * be smarter about it, and use do it less frequently... */
static gboolean
e_cal_list_view_get_visible_time_range (ECalendarView *cal_view,
                                        time_t *start_time,
                                        time_t *end_time)
{
	time_t   earliest = G_MAXINT, latest = 0;
	gboolean set = FALSE;
	gint     n_rows, i;

	n_rows = e_table_model_row_count (E_TABLE_MODEL (e_calendar_view_get_model (cal_view)));

	for (i = 0; i < n_rows; i++) {
		ECalModelComponent *comp;
		icalcomponent      *icalcomp;

		comp = e_cal_model_get_component_at (e_calendar_view_get_model (cal_view), i);
		if (!comp)
			continue;

		icalcomp = comp->icalcomp;
		if (!icalcomp)
			continue;

		adjust_range (icalcomponent_get_dtstart (icalcomp), &earliest, &latest, &set);
		adjust_range (icalcomponent_get_dtend (icalcomp), &earliest, &latest, &set);
	}

	if (set) {
		*start_time = earliest;
		*end_time   = latest;
		return TRUE;
	}

	if (!n_rows) {
		ECalModel *model = e_calendar_view_get_model (cal_view);

		/* Use time range set in the model when nothing shown in the list view */
		e_cal_model_get_time_range (model, start_time, end_time);

		return TRUE;
	}

	return FALSE;
}

gboolean
e_cal_list_view_get_range_shown (ECalListView *cal_list_view,
                                 GDate *start_date,
                                 gint *days_shown)
{
	time_t  first, last;
	GDate   end_date;

	if (!e_cal_list_view_get_visible_time_range (E_CALENDAR_VIEW (cal_list_view), &first, &last))
		return FALSE;

	time_to_gdate_with_zone (start_date, first, e_calendar_view_get_timezone (E_CALENDAR_VIEW (cal_list_view)));
	time_to_gdate_with_zone (&end_date, last, e_calendar_view_get_timezone (E_CALENDAR_VIEW (cal_list_view)));

	*days_shown = g_date_days_between (start_date, &end_date);
	return TRUE;
}

gboolean
e_cal_list_view_is_editing (ECalListView *eclv)
{
	g_return_val_if_fail (E_IS_CAL_LIST_VIEW (eclv), FALSE);

	return eclv->table && e_table_is_editing (eclv->table);
}
