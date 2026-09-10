/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Hans Petter Jansson  <hpj@ximian.com>
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-cal-table-events.h"

#include "comp-util.h"
#include "e-cal-table-common.h"
#include "itip-utils.h"

static const ECalTableColumnDef event_table_columns[] = {
	{ "icon", N_("Type"), E_CAL_MODEL_FIELD_ICON, TRUE },
	{ "summary", N_("Summary"), E_CAL_MODEL_FIELD_SUMMARY, TRUE },
	{ "start-date", N_("Start Date"), E_CAL_MODEL_FIELD_DTSTART, TRUE },
	{ "end-date", N_("End Date"), E_CAL_MODEL_FIELD_DTEND, TRUE },
	{ "description", N_("Description"), E_CAL_MODEL_FIELD_DESCRIPTION, FALSE },
	{ "location", N_("Location"), E_CAL_MODEL_FIELD_LOCATION, FALSE },
	{ "categories", N_("Categories"), E_CAL_MODEL_FIELD_CATEGORIES, FALSE },
	{ "created", N_("Created"), E_CAL_MODEL_FIELD_CREATED, FALSE },
	{ "last-modified", N_("Last modified"), E_CAL_MODEL_FIELD_LASTMODIFIED, FALSE },
	{ "source", N_("Source"), E_CAL_MODEL_FIELD_SOURCE, FALSE },
	{ "status", N_("Status"), E_CAL_MODEL_FIELD_STATUS, FALSE },
	{ "calendar-color", N_("Calendar"), E_CAL_MODEL_FIELD_COLOR, TRUE }
};

#define N_EVENT_TABLE_COLUMNS (G_N_ELEMENTS (event_table_columns))

static const gchar *event_table_icon_names[] = {
	"x-office-calendar",
	"stock_people",
	"view-refresh"
};

struct _ECalTableEvents {
	ECalendarView parent_instance;

	EVirtualTree *vtree;
	gboolean search_active;
};

enum {
	PROP_0,
	PROP_IS_EDITING
};

G_DEFINE_TYPE (ECalTableEvents, e_cal_table_events, E_TYPE_CALENDAR_VIEW)

static void
event_table_column_data_func (EVirtualTree *vtree,
			      GtkCellRenderer *renderer,
			      GObject *row_object,
			      guint visible_row,
			      gpointer user_data)
{
	const ECalTableColumnFuncData *func_data = user_data;
	const ECalTableColumnDef *def = func_data->def;
	ECalModel *model = e_calendar_view_get_model (E_CALENDAR_VIEW (func_data->self));
	ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT (row_object);
	gboolean cancelled;

	if (def->field == E_CAL_MODEL_FIELD_COLOR) {
		e_cal_table_common_set_color_cell (model, comp_data, renderer);
		return;
	}

	cancelled = GPOINTER_TO_INT (e_cal_model_get_field_value (model, comp_data, E_CAL_MODEL_FIELD_CANCELLED)) != 0;

	if (def->field == E_CAL_MODEL_FIELD_ICON) {
		e_cal_table_common_set_icon_cell (model, comp_data, renderer, event_table_icon_names, G_N_ELEMENTS (event_table_icon_names));
		return;
	}

	if (def->field == E_CAL_MODEL_FIELD_DTSTART || def->field == E_CAL_MODEL_FIELD_DTEND ||
	    def->field == E_CAL_MODEL_FIELD_CREATED || def->field == E_CAL_MODEL_FIELD_LASTMODIFIED) {
		gchar *text = e_cal_table_common_date_value_to_text (model, comp_data, def->field);

		g_object_set (renderer, "text", text, "editable", FALSE, "strikethrough", cancelled, NULL);

		g_free (text);
		return;
	}

	if (def->field == E_CAL_MODEL_FIELD_SOURCE) {
		gchar *text = e_cal_model_get_field_value (model, comp_data, def->field);

		g_object_set (renderer, "text", text, "editable", FALSE, "strikethrough", cancelled, NULL);

		g_free (text);
		return;
	}

	if (def->field == E_CAL_MODEL_FIELD_SUMMARY || def->field == E_CAL_MODEL_FIELD_DESCRIPTION) {
		gchar *text = e_cal_model_get_field_value (model, comp_data, def->field);

		g_object_set (renderer, "text", text, "strikethrough", cancelled, NULL);

		g_free (text);
		return;
	}

	g_object_set (renderer, "text", e_cal_model_get_field_value (model, comp_data, def->field), "strikethrough", cancelled, NULL);
}

static gboolean
event_table_cell_clicked_cb (EVirtualTree *vtree,
			     guint visible_row,
			     GObject *row_object,
			     guint col_idx,
			     GtkCellRenderer *hit_renderer,
			     gpointer user_data)
{
	static const gint date_fields[] = {
		E_CAL_MODEL_FIELD_DTSTART,
		E_CAL_MODEL_FIELD_DTEND
	};
	ECalTableEvents *self = user_data;
	ECalModel *model = e_calendar_view_get_model (E_CALENDAR_VIEW (self));

	return cal_comp_util_cell_clicked_edit_datetime_popover (vtree, visible_row, row_object, col_idx, hit_renderer,
		model, date_fields, G_N_ELEMENTS (date_fields), FALSE);
}

static void
event_table_cell_edited_cb (EVirtualTree *vtree,
			    guint visible_row,
			    GObject *row_object,
			    guint col_idx,
			    GtkCellRenderer *renderer,
			    const gchar *new_text,
			    gpointer user_data)
{
	ECalTableEvents *self = user_data;
	ECalModel *model = e_calendar_view_get_model (E_CALENDAR_VIEW (self));
	ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT (row_object);
	gint field = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), "e-cal-field"));

	e_cal_model_set_field_value (model, comp_data, field, new_text, TRUE);
}

static GtkCellRenderer *
event_table_create_special_renderer (gint field,
				     gpointer user_data,
				     gboolean *out_center_align)
{
	ECalTableEvents *self = user_data;
	ECalModel *model = e_calendar_view_get_model (E_CALENDAR_VIEW (self));
	GtkCellRenderer *renderer;
	GtkListStore *store;

	if (field != E_CAL_MODEL_FIELD_STATUS)
		return NULL;

	store = e_cal_table_common_build_status_store (model);

	renderer = gtk_cell_renderer_combo_new ();
	g_object_set (renderer,
		"model", store,
		"text-column", 0,
		"has-entry", FALSE,
		"ellipsize", PANGO_ELLIPSIZE_END,
		NULL);
	g_object_unref (store);

	return renderer;
}

static void
event_table_setup_columns (ECalTableEvents *self)
{
	e_cal_table_common_setup_columns (self->vtree, self, event_table_columns, N_EVENT_TABLE_COLUMNS,
		event_table_column_data_func, event_table_create_special_renderer, self,
		NULL, NULL, -1);
}

static void
event_table_column_state_changed_cb (EVirtualTree *vtree,
				     gpointer user_data)
{
	ECalTableEvents *self = user_data;

	e_cal_table_common_column_state_changed (self->vtree, event_table_columns, N_EVENT_TABLE_COLUMNS,
		e_calendar_view_get_model (E_CALENDAR_VIEW (self)));
}

const gchar * const *
e_cal_table_events_get_legacy_etable_column_ids (guint *out_n_column_ids)
{
	static const gchar *column_ids[N_EVENT_TABLE_COLUMNS - 1];

	return e_cal_table_common_get_legacy_etable_column_ids (event_table_columns, N_EVENT_TABLE_COLUMNS, column_ids, out_n_column_ids);
}

static void
event_table_open_at_row_object (ECalTableEvents *self,
				GObject *row_object)
{
	ECalModelComponent *comp_data;

	comp_data = E_CAL_MODEL_COMPONENT (row_object);

	e_calendar_view_edit_appointment (E_CALENDAR_VIEW (self), comp_data->client, comp_data->icalcomp, EDIT_EVENT_AUTODETECT);
}

static void
event_table_row_activated_cb (EVirtualTree *vtree,
			      guint visible_row,
			      GObject *row_object,
			      gpointer user_data)
{
	event_table_open_at_row_object (E_CAL_TABLE_EVENTS (user_data), row_object);
}

static gboolean
event_table_right_click_cb (EVirtualTree *vtree,
			    guint visible_row,
			    GObject *row_object,
			    GdkEvent *event,
			    gpointer user_data)
{
	ECalTableEvents *self = user_data;

	e_calendar_view_popup_event (E_CALENDAR_VIEW (self), event);

	return TRUE;
}

static void
event_table_selection_changed_cb (EVirtualTree *vtree,
				  gpointer user_data)
{
	ECalTableEvents *self = user_data;

	g_signal_emit_by_name (self, "selection_changed");
}

static void
event_table_update_empty_message (ECalTableEvents *self)
{
	ECalModel *model;
	guint row_count;

	if (!self->vtree)
		return;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (self));
	row_count = e_virtual_tree_model_get_row_count (E_VIRTUAL_TREE_MODEL (model));

	if (row_count > 0 || !self->search_active) {
		e_virtual_tree_set_empty_message (self->vtree, NULL);
	} else {
		e_virtual_tree_set_empty_message (self->vtree,
			_("No event satisfies your search criteria. Change search criteria by selecting "
			"a new Show appointments filter from the drop down list above or by running a new "
			"search either by clearing it with Search->Clear menu item or by changing the query above."));
	}
}

static void
event_table_row_count_changed_cb (EVirtualTreeModel *model,
				  gpointer user_data)
{
	ECalTableEvents *self = user_data;

	event_table_update_empty_message (self);
}

static gboolean
e_cal_table_events_popup_menu (GtkWidget *widget)
{
	e_calendar_view_popup_event (E_CALENDAR_VIEW (widget), NULL);

	return TRUE;
}

static gchar *
e_cal_table_events_get_description_text (ECalendarView *cal_view)
{
	ECalTableEvents *self;
	ECalModel *model;
	GString *string;
	const gchar *format;
	gint n_rows;
	guint n_selected;

	g_return_val_if_fail (E_IS_CAL_TABLE_EVENTS (cal_view), NULL);

	self = E_CAL_TABLE_EVENTS (cal_view);
	model = e_calendar_view_get_model (cal_view);
	n_rows = e_cal_model_get_object_array (model)->len;
	n_selected = e_virtual_tree_selected_count (self->vtree);
	string = g_string_sized_new (64);

	format = ngettext ("%d appointment", "%d appointments", n_rows);
	g_string_append_printf (string, format, n_rows);

	if (n_selected > 0) {
		format = _("%d selected");
		g_string_append_len (string, ", ", 2);
		g_string_append_printf (string, format, n_selected);
	}

	return g_string_free (string, FALSE);
}

static GSList *
e_cal_table_events_get_selected_events (ECalendarView *cal_view)
{
	ECalTableEvents *self = E_CAL_TABLE_EVENTS (cal_view);
	GSList *selection = NULL;
	GObject *row_object;

	row_object = e_virtual_tree_get_cursor_object (self->vtree);

	if (row_object) {
		ECalModelComponent *comp_data = E_CAL_MODEL_COMPONENT (row_object);

		selection = g_slist_prepend (selection,
			e_calendar_view_selection_data_new (comp_data->client, comp_data->icalcomp));
	}

	return selection;
}

static gboolean
e_cal_table_events_get_selected_time_range (ECalendarView *cal_view,
					    time_t *start_time,
					    time_t *end_time)
{
	GSList *selected;
	ICalTimezone *zone;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalendarViewSelectionData *sel_data = selected->data;
		ECalComponent *comp;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, i_cal_component_clone (sel_data->icalcomp));
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
		g_slist_free_full (selected, e_calendar_view_selection_data_free);

		return TRUE;
	}

	return FALSE;
}

static gboolean
e_cal_table_events_get_visible_time_range (ECalendarView *cal_view,
					   time_t *start_time,
					   time_t *end_time)
{
	return FALSE;
}

static void
e_cal_table_events_get_property (GObject *object,
				 guint property_id,
				 GValue *value,
				 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IS_EDITING:
			g_value_set_boolean (value, FALSE);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_cal_table_events_dispose (GObject *object)
{
	ECalTableEvents *self = E_CAL_TABLE_EVENTS (object);

	if (self->vtree) {
		e_virtual_tree_set_model (self->vtree, NULL);
		self->vtree = NULL;
	}

	G_OBJECT_CLASS (e_cal_table_events_parent_class)->dispose (object);
}

static void
e_cal_table_events_class_init (ECalTableEventsClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	ECalendarViewClass *view_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = e_cal_table_events_get_property;
	object_class->dispose = e_cal_table_events_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->popup_menu = e_cal_table_events_popup_menu;

	view_class = E_CALENDAR_VIEW_CLASS (class);
	view_class->get_selected_events = e_cal_table_events_get_selected_events;
	view_class->get_selected_time_range = e_cal_table_events_get_selected_time_range;
	view_class->get_visible_time_range = e_cal_table_events_get_visible_time_range;
	view_class->get_description_text = e_cal_table_events_get_description_text;

	g_object_class_override_property (
		object_class,
		PROP_IS_EDITING,
		"is-editing");
}

static void
e_cal_table_events_init (ECalTableEvents *self)
{
}

static void
event_table_setup (ECalTableEvents *self)
{
	ECalModel *model;
	GtkWidget *scrolled_window;
	GSettings *settings;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (self));

	self->vtree = E_VIRTUAL_TREE (e_virtual_tree_new (E_VIRTUAL_TREE_MODEL (model)));
	e_virtual_tree_set_selection_mode (self->vtree, GTK_SELECTION_MULTIPLE);

	g_signal_connect_object (model, "row-count-changed",
		G_CALLBACK (event_table_row_count_changed_cb), self, 0);

	event_table_update_empty_message (self);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	g_settings_bind (settings, "table-sort-on-header-click",
		self->vtree, "header-click-sort-policy",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (settings, "allow-direct-summary-edit",
		self->vtree, "editable",
		G_SETTINGS_BIND_DEFAULT);
	g_clear_object (&settings);

	g_signal_connect (self->vtree, "get-legacy-etable-column-map",
		G_CALLBACK (e_cal_table_common_get_legacy_etable_column_map_cb), e_cal_table_events_get_legacy_etable_column_ids);

	event_table_setup_columns (self);

	g_signal_connect (self->vtree, "column-state-changed",
		G_CALLBACK (event_table_column_state_changed_cb), self);
	g_signal_connect (self->vtree, "row-activated",
		G_CALLBACK (event_table_row_activated_cb), self);
	g_signal_connect (self->vtree, "right-click",
		G_CALLBACK (event_table_right_click_cb), self);
	g_signal_connect (self->vtree, "selection-changed",
		G_CALLBACK (event_table_selection_changed_cb), self);
	g_signal_connect (self->vtree, "cell-clicked",
		G_CALLBACK (event_table_cell_clicked_cb), self);
	g_signal_connect (self->vtree, "cell-edited",
		G_CALLBACK (event_table_cell_edited_cb), self);

	e_virtual_tree_set_row_tooltip_markup_func (self->vtree, e_cal_table_common_row_tooltip_markup_cb, model, NULL);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolled_window),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_grid_attach (GTK_GRID (self), scrolled_window, 0, 1, 2, 2);
	g_object_set (G_OBJECT (scrolled_window),
		"hexpand", TRUE,
		"vexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (scrolled_window);

	gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (self->vtree));
	gtk_widget_show (GTK_WIDGET (self->vtree));
}

ECalendarView *
e_cal_table_events_new (ECalModel *model)
{
	ECalendarView *cal_view;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	cal_view = g_object_new (E_TYPE_CAL_TABLE_EVENTS, "model", model, NULL);
	event_table_setup (E_CAL_TABLE_EVENTS (cal_view));

	return cal_view;
}

EVirtualTree *
e_cal_table_events_get_virtual_tree (ECalTableEvents *self)
{
	g_return_val_if_fail (E_IS_CAL_TABLE_EVENTS (self), NULL);

	return self->vtree;
}

EPrintable *
e_cal_table_events_get_printable (ECalTableEvents *self)
{
	g_return_val_if_fail (E_IS_CAL_TABLE_EVENTS (self), NULL);

	return e_virtual_tree_get_printable (self->vtree);
}

void
e_cal_table_events_set_search_active (ECalTableEvents *self,
				      gboolean search_active)
{
	g_return_if_fail (E_IS_CAL_TABLE_EVENTS (self));

	search_active = !!search_active;

	if ((self->search_active ? 1 : 0) == (search_active ? 1 : 0))
		return;

	self->search_active = search_active;

	event_table_update_empty_message (self);
}

gboolean
e_cal_table_events_get_search_active (ECalTableEvents *self)
{
	g_return_val_if_fail (E_IS_CAL_TABLE_EVENTS (self), FALSE);

	return self->search_active;
}
