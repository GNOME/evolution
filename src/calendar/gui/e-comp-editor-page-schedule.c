/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <e-util/e-util.h>

#include "e-comp-editor-page-schedule.h"

struct _ECompEditorPageSchedulePrivate {
	EMeetingStore *store;
	EMeetingTimeSelector *selector;
	ENameSelector *name_selector;
};

enum {
	PROP_0,
	PROP_STORE,
	PROP_NAME_SELECTOR
};

G_DEFINE_TYPE_WITH_PRIVATE (ECompEditorPageSchedule, e_comp_editor_page_schedule, E_TYPE_COMP_EDITOR_PAGE)

static void
ecep_schedule_get_work_day_range_for (GSettings *settings,
				      gint weekday,
				      gint *start_hour,
				      gint *start_minute,
				      gint *end_hour,
				      gint *end_minute)
{
	gint start_adept = -1, end_adept = -1;
	const gchar *start_key = NULL, *end_key = NULL;

	g_return_if_fail (G_IS_SETTINGS (settings));
	g_return_if_fail (start_hour != NULL);
	g_return_if_fail (start_minute != NULL);
	g_return_if_fail (end_hour != NULL);
	g_return_if_fail (end_minute != NULL);

	switch (weekday) {
		case G_DATE_MONDAY:
			start_key = "day-start-mon";
			end_key = "day-end-mon";
			break;
		case G_DATE_TUESDAY:
			start_key = "day-start-tue";
			end_key = "day-end-tue";
			break;
		case G_DATE_WEDNESDAY:
			start_key = "day-start-wed";
			end_key = "day-end-wed";
			break;
		case G_DATE_THURSDAY:
			start_key = "day-start-thu";
			end_key = "day-end-thu";
			break;
		case G_DATE_FRIDAY:
			start_key = "day-start-fri";
			end_key = "day-end-fri";
			break;
		case G_DATE_SATURDAY:
			start_key = "day-start-sat";
			end_key = "day-end-sat";
			break;
		case G_DATE_SUNDAY:
			start_key = "day-start-sun";
			end_key = "day-end-sun";
			break;
		default:
			break;
	}

	if (start_key && end_key) {
		start_adept = g_settings_get_int (settings, start_key);
		end_adept = g_settings_get_int (settings, end_key);
	}

	if (start_adept > 0 && (start_adept / 100) >= 0 && (start_adept / 100) <= 23 &&
	    (start_adept % 100) >= 0 && (start_adept % 100) <= 59) {
		*start_hour = start_adept / 100;
		*start_minute = start_adept % 100;
	} else {
		*start_hour = g_settings_get_int (settings, "day-start-hour");
		*start_minute = g_settings_get_int (settings, "day-start-minute");
	}

	if (end_adept > 0 && (end_adept / 100) >= 0 && (end_adept / 100) <= 23 &&
	    (end_adept % 100) >= 0 && (end_adept % 100) <= 59) {
		*end_hour = end_adept / 100;
		*end_minute = end_adept % 100;
	} else {
		*end_hour = g_settings_get_int (settings, "day-end-hour");
		*end_minute = g_settings_get_int (settings, "day-end-minute");
	}
}

static void
ecep_schedule_editor_times_changed_cb (ECompEditor *comp_editor,
				       ECompEditorPageSchedule *page_schedule)
{
	ECompEditorPropertyPartDatetime *dtstart, *dtend;
	ECompEditorPropertyPart *dtstart_part = NULL, *dtend_part = NULL;
	EDateEdit *start_date_edit, *end_date_edit;
	ICalTime *start_tt, *end_tt;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_SCHEDULE (page_schedule));
	g_return_if_fail (page_schedule->priv->selector != NULL);

	e_comp_editor_get_time_parts (comp_editor, &dtstart_part, &dtend_part);

	if (!dtstart_part || !dtend_part)
		return;

	dtstart = E_COMP_EDITOR_PROPERTY_PART_DATETIME (dtstart_part);
	dtend = E_COMP_EDITOR_PROPERTY_PART_DATETIME (dtend_part);

	start_tt = e_comp_editor_property_part_datetime_get_value (dtstart);
	end_tt = e_comp_editor_property_part_datetime_get_value (dtend);

	if (!start_tt || !end_tt) {
		g_clear_object (&start_tt);
		g_clear_object (&end_tt);
		return;
	}

	/* For All Day Events, if DTEND is after DTSTART, we subtract 1 day from it. */
	if (i_cal_time_is_date (start_tt) && i_cal_time_is_date (end_tt) &&
	    i_cal_time_compare_date_only (end_tt, start_tt) > 0)
		i_cal_time_adjust (end_tt, -1, 0, 0, 0);

	e_comp_editor_page_set_updating (E_COMP_EDITOR_PAGE (page_schedule), TRUE);

	start_date_edit = E_DATE_EDIT (page_schedule->priv->selector->start_date_edit);
	end_date_edit = E_DATE_EDIT (page_schedule->priv->selector->end_date_edit);

	e_date_edit_set_date (start_date_edit,
		i_cal_time_get_year (start_tt),
		i_cal_time_get_month (start_tt),
		i_cal_time_get_day (start_tt));
	e_date_edit_set_time_of_day (start_date_edit,
		i_cal_time_get_hour (start_tt),
		i_cal_time_get_minute (start_tt));

	e_date_edit_set_date (end_date_edit,
		i_cal_time_get_year (end_tt),
		i_cal_time_get_month (end_tt),
		i_cal_time_get_day (end_tt));
	e_date_edit_set_time_of_day (end_date_edit,
		i_cal_time_get_hour (end_tt),
		i_cal_time_get_minute (end_tt));

	e_comp_editor_page_set_updating (E_COMP_EDITOR_PAGE (page_schedule), FALSE);

	g_clear_object (&start_tt);
	g_clear_object (&end_tt);
}

static void
ecep_schedule_editor_target_client_notify_cb (GObject *comp_editor,
					      GParamSpec *param,
					      gpointer user_data)
{
	ECompEditorPageSchedule *page_schedule = user_data;
	ECalClient *target_client;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_SCHEDULE (page_schedule));
	g_return_if_fail (page_schedule->priv->store != NULL);
	g_return_if_fail (page_schedule->priv->selector != NULL);

	target_client = e_comp_editor_get_target_client (E_COMP_EDITOR (comp_editor));
	e_meeting_store_set_client (page_schedule->priv->store, target_client);
	e_meeting_time_selector_refresh_free_busy (page_schedule->priv->selector, -1, TRUE);
}

static void
ecep_schedule_set_time_to_editor (ECompEditorPageSchedule *page_schedule)
{
	EMeetingTimeSelector *selector;
	ECompEditorPropertyPartDatetime *dtstart, *dtend;
	ECompEditorPropertyPart *dtstart_part = NULL, *dtend_part = NULL;
	ECompEditor *comp_editor;
	ICalTime *start_tt, *end_tt;
	gint year, month, day, hour, minute;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_SCHEDULE (page_schedule));
	g_return_if_fail (E_IS_MEETING_TIME_SELECTOR (page_schedule->priv->selector));

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_schedule));
	if (comp_editor)
		e_comp_editor_get_time_parts (comp_editor, &dtstart_part, &dtend_part);

	if (!dtstart_part || !dtend_part) {
		g_clear_object (&comp_editor);
		return;
	}

	selector = page_schedule->priv->selector;
	dtstart = E_COMP_EDITOR_PROPERTY_PART_DATETIME (dtstart_part);
	dtend = E_COMP_EDITOR_PROPERTY_PART_DATETIME (dtend_part);

	start_tt = e_comp_editor_property_part_datetime_get_value (dtstart);
	end_tt = e_comp_editor_property_part_datetime_get_value (dtend);

	if (!start_tt || !end_tt) {
		g_clear_object (&comp_editor);
		g_clear_object (&start_tt);
		g_clear_object (&end_tt);
		return;
	}

	if (!e_date_edit_get_date (E_DATE_EDIT (selector->start_date_edit), &year, &month, &day) ||
	    !e_date_edit_get_time_of_day (E_DATE_EDIT (selector->start_date_edit), &hour, &minute)) {
		g_clear_object (&comp_editor);
		g_clear_object (&start_tt);
		g_clear_object (&end_tt);
		return;
	}

	i_cal_time_set_date (start_tt, year, month, day);
	i_cal_time_set_time (start_tt, hour, minute, 0);

	if (!e_date_edit_get_date (E_DATE_EDIT (selector->end_date_edit), &year, &month, &day) ||
	    !e_date_edit_get_time_of_day (E_DATE_EDIT (selector->end_date_edit), &hour, &minute)) {
		g_clear_object (&comp_editor);
		g_clear_object (&start_tt);
		g_clear_object (&end_tt);
		return;
	}

	i_cal_time_set_date (end_tt, year, month, day);
	i_cal_time_set_time (end_tt, hour, minute, 0);

	if (!e_date_edit_get_show_time (E_DATE_EDIT (selector->start_date_edit))) {
		/* For All-Day Events, we set the timezone to NULL, and add 1 day to DTEND. */
		i_cal_time_set_timezone (start_tt, NULL);
		i_cal_time_set_is_date (start_tt, TRUE);
		i_cal_time_set_timezone (end_tt, NULL);
		i_cal_time_set_is_date (end_tt, TRUE);

		i_cal_time_adjust (end_tt, 1, 0, 0, 0);
	} else {
		i_cal_time_set_is_date (start_tt, FALSE);
		i_cal_time_set_is_date (end_tt, FALSE);
	}

	e_comp_editor_property_part_datetime_set_value (dtstart, start_tt);
	e_comp_editor_property_part_datetime_set_value (dtend, end_tt);

	g_clear_object (&comp_editor);
	g_clear_object (&start_tt);
	g_clear_object (&end_tt);
}

static void
ecep_schedule_selector_changed_cb (EMeetingTimeSelector *selector,
				   ECompEditorPageSchedule *page_schedule)
{
	ECompEditorPage *page;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_SCHEDULE (page_schedule));
	g_return_if_fail (page_schedule->priv->selector == selector);

	page = E_COMP_EDITOR_PAGE (page_schedule);

	if (e_comp_editor_page_get_updating (page))
		return;

	e_comp_editor_page_set_updating (page, TRUE);

	ecep_schedule_set_time_to_editor (page_schedule);

	e_comp_editor_page_set_updating (page, FALSE);
	e_comp_editor_page_emit_changed (page);
}

static void
ecep_schedule_sensitize_widgets (ECompEditorPage *page,
				 gboolean force_insensitive)
{
	ECompEditorPageSchedule *page_schedule;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_SCHEDULE (page));

	E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_schedule_parent_class)->sensitize_widgets (page, force_insensitive);

	page_schedule = E_COMP_EDITOR_PAGE_SCHEDULE (page);

	e_meeting_time_selector_set_read_only (page_schedule->priv->selector, force_insensitive);
}

static void
ecep_schedule_fill_widgets (ECompEditorPage *page,
			    ICalComponent *component)
{
	ECompEditorPageSchedule *page_schedule;
	ECompEditorPropertyPartDatetime *dtstart, *dtend;
	ECompEditorPropertyPart *dtstart_part = NULL, *dtend_part = NULL;
	ECompEditor *comp_editor;
	EMeetingTimeSelector *selector;
	ICalTime *start_tt, *end_tt;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_SCHEDULE (page));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_schedule_parent_class)->fill_widgets (page, component);

	page_schedule = E_COMP_EDITOR_PAGE_SCHEDULE (page);

	/* dtstart/dtend parts should be already populated, thus
	   get values from them, instead of from the component */

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_SCHEDULE (page_schedule));
	g_return_if_fail (E_IS_MEETING_TIME_SELECTOR (page_schedule->priv->selector));

	comp_editor = e_comp_editor_page_ref_editor (page);
	if (comp_editor)
		e_comp_editor_get_time_parts (comp_editor, &dtstart_part, &dtend_part);

	if (!dtstart_part || !dtend_part) {
		g_clear_object (&comp_editor);
		return;
	}

	selector = page_schedule->priv->selector;
	dtstart = E_COMP_EDITOR_PROPERTY_PART_DATETIME (dtstart_part);
	dtend = E_COMP_EDITOR_PROPERTY_PART_DATETIME (dtend_part);

	start_tt = e_comp_editor_property_part_datetime_get_value (dtstart);
	end_tt = e_comp_editor_property_part_datetime_get_value (dtend);

	if (!start_tt || !end_tt) {
		g_clear_object (&comp_editor);
		g_clear_object (&start_tt);
		g_clear_object (&end_tt);
		return;
	}

	if (i_cal_time_is_date (start_tt)) {
		/* For All-Day Events, we set the timezone to NULL, and add 1 day to DTEND. */
		i_cal_time_set_timezone (start_tt, NULL);
		i_cal_time_set_is_date (start_tt, TRUE);
		i_cal_time_set_timezone (end_tt, NULL);
		i_cal_time_set_is_date (end_tt, TRUE);

		i_cal_time_adjust (end_tt, 1, 0, 0, 0);
	} else {
		i_cal_time_set_is_date (start_tt, FALSE);
		i_cal_time_set_is_date (end_tt, FALSE);
	}

	e_comp_editor_page_set_updating (page, TRUE);

	e_date_edit_set_date (
		E_DATE_EDIT (selector->start_date_edit),
		i_cal_time_get_year (start_tt),
		i_cal_time_get_month (start_tt),
		i_cal_time_get_day (start_tt));
	e_date_edit_set_time_of_day (
		E_DATE_EDIT (selector->start_date_edit),
		i_cal_time_get_hour (start_tt),
		i_cal_time_get_minute (start_tt));
	e_date_edit_set_date (
		E_DATE_EDIT (selector->end_date_edit),
		i_cal_time_get_year (end_tt),
		i_cal_time_get_month (end_tt),
		i_cal_time_get_day (end_tt));
	e_date_edit_set_time_of_day (
		E_DATE_EDIT (selector->end_date_edit),
		i_cal_time_get_hour (end_tt),
		i_cal_time_get_minute (end_tt));

	e_comp_editor_page_set_updating (page, FALSE);

	g_clear_object (&comp_editor);
	g_clear_object (&start_tt);
	g_clear_object (&end_tt);
}

static gboolean
ecep_schedule_fill_component (ECompEditorPage *page,
			      ICalComponent *component)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_SCHEDULE (page), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT (component), FALSE);

	return E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_schedule_parent_class)->fill_component (page, component);
}

static void
e_comp_editor_page_schedule_set_store (ECompEditorPageSchedule *page_schedule,
				       EMeetingStore *store)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_SCHEDULE (page_schedule));
	g_return_if_fail (E_IS_MEETING_STORE (store));
	g_return_if_fail (page_schedule->priv->store == NULL);

	page_schedule->priv->store = g_object_ref (store);
}

static void
e_comp_editor_page_schedule_set_name_selector (ECompEditorPageSchedule *page_schedule,
					       ENameSelector *name_selector)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_SCHEDULE (page_schedule));
	g_return_if_fail (E_IS_NAME_SELECTOR (name_selector));
	g_return_if_fail (page_schedule->priv->name_selector == NULL);

	page_schedule->priv->name_selector = g_object_ref (name_selector);
}

static void
e_comp_editor_page_schedule_set_property (GObject *object,
					  guint property_id,
					  const GValue *value,
					  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_STORE:
			e_comp_editor_page_schedule_set_store (
				E_COMP_EDITOR_PAGE_SCHEDULE (object),
				g_value_get_object (value));
			return;

		case PROP_NAME_SELECTOR:
			e_comp_editor_page_schedule_set_name_selector (
				E_COMP_EDITOR_PAGE_SCHEDULE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_comp_editor_page_schedule_get_property (GObject *object,
					  guint property_id,
					  GValue *value,
					  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_STORE:
			g_value_set_object (
				value,
				e_comp_editor_page_schedule_get_store (
				E_COMP_EDITOR_PAGE_SCHEDULE (object)));
			return;

		case PROP_NAME_SELECTOR:
			g_value_set_object (
				value,
				e_comp_editor_page_schedule_get_name_selector (
				E_COMP_EDITOR_PAGE_SCHEDULE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
ecep_schedule_select_page_cb (EUIAction *action,
			      GVariant *parameter,
			      gpointer user_data)
{
	ECompEditorPage *page = user_data;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_SCHEDULE (page));

	e_comp_editor_page_select (page);
}

static void
ecep_schedule_setup_ui (ECompEditorPageSchedule *page_schedule)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<submenu action='options-menu'>"
		      "<placeholder id='tabs'>"
			"<item action='page-schedule'/>"
		      "</placeholder>"
		    "</submenu>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry options_actions[] = {
		{ "page-schedule",
		  "query-free-busy",
		  N_("_Schedule"),
		  NULL,
		  N_("Query free / busy information for the attendees"),
		  ecep_schedule_select_page_cb, NULL, NULL, NULL }
	};

	ECompEditor *comp_editor;
	EUIManager *ui_manager;
	EUIAction *action;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_SCHEDULE (page_schedule));

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_schedule));
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "individual", GETTEXT_PACKAGE,
		options_actions, G_N_ELEMENTS (options_actions), page_schedule, eui);

	action = e_comp_editor_get_action (comp_editor, "page-schedule");
	e_binding_bind_property (
		page_schedule, "visible",
		action, "visible",
		G_BINDING_SYNC_CREATE);

	g_clear_object (&comp_editor);
}

static void
e_comp_editor_page_schedule_constructed (GObject *object)
{
	ECompEditorPageSchedule *page_schedule;
	ECompEditor *comp_editor;
	GSettings *settings;
	GtkWidget *widget;
	gint weekday;

	G_OBJECT_CLASS (e_comp_editor_page_schedule_parent_class)->constructed (object);

	page_schedule = E_COMP_EDITOR_PAGE_SCHEDULE (object);

	g_return_if_fail (page_schedule->priv->store != NULL);

	widget = e_meeting_time_selector_new (page_schedule->priv->store);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (GTK_GRID (page_schedule), widget, 0, 0, 1, 1);

	page_schedule->priv->selector = E_MEETING_TIME_SELECTOR (widget);
	e_meeting_list_view_set_name_selector (page_schedule->priv->selector->list_view, page_schedule->priv->name_selector);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	for (weekday = G_DATE_BAD_WEEKDAY; weekday <= G_DATE_SUNDAY; weekday++) {
		gint start_hour = 8, start_minute = 0, end_hour = 17, end_minute = 0;

		ecep_schedule_get_work_day_range_for (settings, weekday,
			&start_hour, &start_minute, &end_hour, &end_minute);

		e_meeting_time_selector_set_working_hours (page_schedule->priv->selector,
			weekday, start_hour, start_minute, end_hour, end_minute);
	}

	g_clear_object (&settings);

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_schedule));
	if (comp_editor) {
		g_signal_connect (comp_editor, "times-changed",
			G_CALLBACK (ecep_schedule_editor_times_changed_cb), page_schedule);

		g_signal_connect (comp_editor, "notify::target-client",
			G_CALLBACK (ecep_schedule_editor_target_client_notify_cb), page_schedule);
	}

	g_clear_object (&comp_editor);

	g_signal_connect (page_schedule->priv->selector, "changed",
		G_CALLBACK (ecep_schedule_selector_changed_cb), page_schedule);

	ecep_schedule_setup_ui (page_schedule);
}

static void
e_comp_editor_page_schedule_dispose (GObject *object)
{
	ECompEditorPageSchedule *page_schedule;
	ECompEditor *comp_editor;

	page_schedule = E_COMP_EDITOR_PAGE_SCHEDULE (object);

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_schedule));
	if (comp_editor) {
		g_signal_handlers_disconnect_by_func (comp_editor,
			G_CALLBACK (ecep_schedule_editor_times_changed_cb), page_schedule);
		g_clear_object (&comp_editor);
	}

	g_clear_object (&page_schedule->priv->store);
	g_clear_object (&page_schedule->priv->name_selector);

	G_OBJECT_CLASS (e_comp_editor_page_schedule_parent_class)->dispose (object);
}

static void
e_comp_editor_page_schedule_init (ECompEditorPageSchedule *page_schedule)
{
	page_schedule->priv = e_comp_editor_page_schedule_get_instance_private (page_schedule);
}

static void
e_comp_editor_page_schedule_class_init (ECompEditorPageScheduleClass *klass)
{
	ECompEditorPageClass *page_class;
	GObjectClass *object_class;

	page_class = E_COMP_EDITOR_PAGE_CLASS (klass);
	page_class->sensitize_widgets = ecep_schedule_sensitize_widgets;
	page_class->fill_widgets = ecep_schedule_fill_widgets;
	page_class->fill_component = ecep_schedule_fill_component;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_comp_editor_page_schedule_set_property;
	object_class->get_property = e_comp_editor_page_schedule_get_property;
	object_class->constructed = e_comp_editor_page_schedule_constructed;
	object_class->dispose = e_comp_editor_page_schedule_dispose;

	g_object_class_install_property (
		object_class,
		PROP_STORE,
		g_param_spec_object (
			"store",
			"store",
			"an EMeetingStore",
			E_TYPE_MEETING_STORE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_NAME_SELECTOR,
		g_param_spec_object (
			"name-selector",
			"Name Selector",
			NULL,
			E_TYPE_NAME_SELECTOR,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

ECompEditorPage *
e_comp_editor_page_schedule_new (ECompEditor *editor,
				 EMeetingStore *meeting_store,
				 ENameSelector *name_selector)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (editor), NULL);

	return g_object_new (E_TYPE_COMP_EDITOR_PAGE_SCHEDULE,
		"editor", editor,
		"store", meeting_store,
		"name-selector", name_selector,
		NULL);
}

EMeetingStore *
e_comp_editor_page_schedule_get_store (ECompEditorPageSchedule *page_schedule)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_SCHEDULE (page_schedule), NULL);

	return page_schedule->priv->store;
}

EMeetingTimeSelector *
e_comp_editor_page_schedule_get_time_selector (ECompEditorPageSchedule *page_schedule)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_SCHEDULE (page_schedule), NULL);

	return page_schedule->priv->selector;
}

ENameSelector *
e_comp_editor_page_schedule_get_name_selector (ECompEditorPageSchedule *page_schedule)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_SCHEDULE (page_schedule), NULL);

	return page_schedule->priv->name_selector;
}
