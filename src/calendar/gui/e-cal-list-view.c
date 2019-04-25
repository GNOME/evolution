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

#include "evolution-config.h"

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

struct _ECalListViewPrivate {
	/* The main display table */
	ETable *table;

	/* The last ECalendarViewEvent we returned from e_cal_list_view_get_selected_events(), to be freed */
	ECalendarViewEvent *cursor_event;
};

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

static gboolean  e_cal_list_view_on_table_double_click   (GtkWidget *table, gint row, gint col,
							 GdkEvent *event, gpointer data);
static gboolean  e_cal_list_view_on_table_right_click   (GtkWidget *table, gint row, gint col,
							 GdkEvent *event, gpointer data);
static gboolean e_cal_list_view_on_table_key_press	(ETable *table, gint row, gint col,
							 GdkEvent *event, gpointer data);
static gboolean  e_cal_list_view_on_table_white_space_event (ETable *table, GdkEvent *event, gpointer data);
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

	g_type_class_add_private (class, sizeof (ECalListViewPrivate));

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
	cal_list_view->priv = G_TYPE_INSTANCE_GET_PRIVATE (cal_list_view, E_TYPE_CAL_LIST_VIEW, ECalListViewPrivate);

	cal_list_view->priv->table = NULL;
	cal_list_view->priv->cursor_event = NULL;
}

/* Returns the current time, for the ECellDateEdit items. */
static struct tm
get_current_time_cb (ECellDateEdit *ecde,
                     gpointer data)
{
	ECalListView *cal_list_view = data;
	ICalTimezone *zone;
	ICalTime *tt;
	struct tm tmp_tm;

	zone = e_calendar_view_get_timezone (E_CALENDAR_VIEW (cal_list_view));
	tt = i_cal_time_from_timet_with_zone (time (NULL), FALSE, zone);

	tmp_tm = e_cal_util_icaltime_to_tm (tt);

	g_clear_object (&tt);

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
		"strikeout_column", E_CAL_MODEL_FIELD_CANCELLED,
		NULL);

	e_table_extras_add_cell (extras, "calstring", cell);
	g_object_unref (cell);

	/* Date fields */

	cell = e_cell_date_edit_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (
		cell,
		"bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		"strikeout_column", E_CAL_MODEL_FIELD_CANCELLED,
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
		"strikeout_column", E_CAL_MODEL_FIELD_CANCELLED,
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
	cal_list_view->priv->table = E_TABLE (widget);
	gtk_widget_show (widget);

	g_object_unref (specification);
	g_object_unref (extras);
	g_free (etspecfile);

	/* Connect signals */
	g_signal_connect (
		cal_list_view->priv->table, "double_click",
		G_CALLBACK (e_cal_list_view_on_table_double_click),
		cal_list_view);
	g_signal_connect (
		cal_list_view->priv->table, "right-click",
		G_CALLBACK (e_cal_list_view_on_table_right_click),
		cal_list_view);
	g_signal_connect (
		cal_list_view->priv->table, "key-press",
		G_CALLBACK (e_cal_list_view_on_table_key_press),
		cal_list_view);
	g_signal_connect (
		cal_list_view->priv->table, "white-space-event",
		G_CALLBACK (e_cal_list_view_on_table_white_space_event),
		cal_list_view);
	g_signal_connect_after (
		cal_list_view->priv->table, "cursor_change",
		G_CALLBACK (e_cal_list_view_cursor_change_cb),
		cal_list_view);
	e_signal_connect_notify_after (
		cal_list_view->priv->table, "notify::is-editing",
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

	if (cal_list_view->priv->cursor_event) {
		g_free (cal_list_view->priv->cursor_event);
		cal_list_view->priv->cursor_event = NULL;
	}

	if (cal_list_view->priv->table) {
		gtk_widget_destroy (GTK_WIDGET (cal_list_view->priv->table));
		cal_list_view->priv->table = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_cal_list_view_parent_class)->dispose (object);
}

static void
e_cal_list_view_show_popup_menu (ECalListView *cal_list_view,
                                 GdkEvent *event)
{
	e_calendar_view_popup_event (E_CALENDAR_VIEW (cal_list_view), event);
}

static gboolean
e_cal_list_view_popup_menu (GtkWidget *widget)
{
	ECalListView *cal_list_view = E_CAL_LIST_VIEW (widget);

	e_cal_list_view_show_popup_menu (cal_list_view, NULL);
	return TRUE;
}

static void
e_cal_list_view_open_at_row (ECalListView *cal_list_view,
			     gint row)
{
	ECalModelComponent *comp_data;

	g_return_if_fail (E_IS_CAL_LIST_VIEW (cal_list_view));

	comp_data = e_cal_model_get_component_at (e_calendar_view_get_model (E_CALENDAR_VIEW (cal_list_view)), row);
	g_warn_if_fail (comp_data != NULL);
	if (!comp_data)
		return;

	e_calendar_view_edit_appointment (E_CALENDAR_VIEW (cal_list_view), comp_data->client, comp_data->icalcomp, EDIT_EVENT_AUTODETECT);
}

static gboolean
e_cal_list_view_on_table_double_click (GtkWidget *table,
                                       gint row,
                                       gint col,
                                       GdkEvent *event,
                                       gpointer data)
{
	e_cal_list_view_open_at_row (data, row);

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

	e_cal_list_view_show_popup_menu (cal_list_view, event);

	return TRUE;
}

static gboolean
e_cal_list_view_on_table_key_press (ETable *table,
				    gint row,
				    gint col,
				    GdkEvent *event,
				    gpointer data)
{
	if (event && event->type == GDK_KEY_PRESS &&
	    (event->key.keyval == GDK_KEY_Return || event->key.keyval == GDK_KEY_KP_Enter) &&
	    (event->key.state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK)) == 0 &&
	    !e_table_is_editing (table)) {
		e_cal_list_view_open_at_row (data, row);
		return TRUE;
	}

	return FALSE;
}

static gboolean
e_cal_list_view_on_table_white_space_event (ETable *table,
					    GdkEvent *event,
					    gpointer user_data)
{
	ECalListView *cal_list_view = user_data;
	guint event_button = 0;

	g_return_val_if_fail (E_IS_CAL_LIST_VIEW (cal_list_view), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (event->type == GDK_BUTTON_PRESS &&
	    gdk_event_get_button (event, &event_button) &&
	    event_button == 3) {
		GtkWidget *table_canvas;

		table_canvas = GTK_WIDGET (table->table_canvas);

		if (!gtk_widget_has_focus (table_canvas))
			gtk_widget_grab_focus (table_canvas);

		e_cal_list_view_show_popup_menu (cal_list_view, event);

		return TRUE;
	}

	return FALSE;
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
	ICalTimezone *zone;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;
		ECalComponent *comp;

		if (!is_comp_data_valid (event))
			return FALSE;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, i_cal_component_new_clone (event->comp_data->icalcomp));
		if (start_time) {
			ECalComponentDateTime *dt;

			dt = e_cal_component_get_dtstart (comp);

			if (dt) {
				if (e_cal_component_datetime_get_tzid (dt)) {
					zone = i_cal_component_get_timezone (e_cal_component_get_icalcomponent (comp), e_cal_component_datetime_get_tzid (dt));
				} else {
					zone = NULL;
				}
				*start_time = i_cal_time_as_timet_with_zone (e_cal_component_datetime_get_value (dt), zone);
			} else {
				*start_time = (time_t) 0;
			}

			e_cal_component_datetime_free (dt);
		}
		if (end_time) {
			ECalComponentDateTime *dt;

			dt = e_cal_component_get_dtend (comp);

			if (dt) {
				if (e_cal_component_datetime_get_tzid (dt)) {
					zone = i_cal_component_get_timezone (e_cal_component_get_icalcomponent (comp), e_cal_component_datetime_get_tzid (dt));
				} else {
					zone = NULL;
				}
				*end_time = i_cal_time_as_timet_with_zone (e_cal_component_datetime_get_value (dt), zone);
			} else {
				*end_time = (time_t) 0;
			}

			e_cal_component_datetime_free (dt);
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

	if (E_CAL_LIST_VIEW (cal_view)->priv->cursor_event) {
		g_free (E_CAL_LIST_VIEW (cal_view)->priv->cursor_event);
		E_CAL_LIST_VIEW (cal_view)->priv->cursor_event = NULL;
	}

	cursor_row = e_table_get_cursor_row (
		E_CAL_LIST_VIEW (cal_view)->priv->table);

	if (cursor_row >= 0) {
		ECalendarViewEvent *event;

		event = E_CAL_LIST_VIEW (cal_view)->priv->cursor_event = g_new0 (ECalendarViewEvent, 1);
		event->comp_data =
			e_cal_model_get_component_at (
				e_calendar_view_get_model (cal_view),
				cursor_row);
		event_list = g_list_prepend (event_list, event);
	}

	return event_list;
}

/* It also frees 'itt' */
static void
adjust_range (ICalTime *itt,
              time_t *earliest,
              time_t *latest,
              gboolean *set)
{
	time_t t;

	if (!itt || !i_cal_time_is_valid_time (itt)) {
		g_clear_object (&itt);
		return;
	}

	t = i_cal_time_as_timet (itt);

	*earliest = MIN (*earliest, t);
	*latest   = MAX (*latest, t);
	*set = TRUE;

	g_clear_object (&itt);
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
		ICalComponent *icomp;

		comp = e_cal_model_get_component_at (e_calendar_view_get_model (cal_view), i);
		if (!comp)
			continue;

		icomp = comp->icalcomp;
		if (!icomp)
			continue;

		adjust_range (i_cal_component_get_dtstart (icomp), &earliest, &latest, &set);
		adjust_range (i_cal_component_get_dtend (icomp), &earliest, &latest, &set);
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

ETable *
e_cal_list_view_get_table (ECalListView *cal_list_view)
{
	g_return_val_if_fail (E_IS_CAL_LIST_VIEW (cal_list_view), NULL);

	return cal_list_view->priv->table;
}

gboolean
e_cal_list_view_get_range_shown (ECalListView *cal_list_view,
                                 GDate *start_date,
                                 gint *days_shown)
{
	time_t  first, last;
	GDate   end_date;

	g_return_val_if_fail (E_IS_CAL_LIST_VIEW (cal_list_view), FALSE);

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

	return eclv->priv->table && e_table_is_editing (eclv->priv->table);
}
