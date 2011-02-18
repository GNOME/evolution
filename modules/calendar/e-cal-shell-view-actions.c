/*
 * e-cal-shell-view-actions.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-util/e-alert-dialog.h"
#include "e-cal-shell-view-private.h"

/* This is for radio action groups whose value is persistent.  We
 * initialize it to a bogus value to ensure a "changed" signal is
 * emitted when a valid value is restored. */
#define BOGUS_INITIAL_VALUE G_MININT

static void
action_calendar_copy_cb (GtkAction *action,
                         ECalShellView *cal_shell_view)
{
	ECalShellSidebar *cal_shell_sidebar;
	EShellWindow *shell_window;
	EShellView *shell_view;
	ESourceSelector *selector;
	ESource *source;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);
	source = e_source_selector_peek_primary_selection (selector);
	g_return_if_fail (E_IS_SOURCE (source));

	copy_source_dialog (
		GTK_WINDOW (shell_window),
		source, E_CAL_SOURCE_TYPE_EVENT);
}

static void
action_calendar_delete_cb (GtkAction *action,
                           ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalShellSidebar *cal_shell_sidebar;
	EShellBackend *shell_backend;
	EShellWindow *shell_window;
	EShellView *shell_view;
	ECalendarView *calendar_view;
	GnomeCalendarViewType view_type;
	GnomeCalendar *calendar;
	ECalModel *model;
	ECal *client;
	ESourceSelector *selector;
	ESourceGroup *source_group;
	ESourceList *source_list;
	ESource *source;
	gint response;
	gchar *uri;
	GError *error = NULL;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);
	model = e_calendar_view_get_model (calendar_view);

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);
	source = e_source_selector_peek_primary_selection (selector);
	g_return_if_fail (E_IS_SOURCE (source));

	/* Ask for confirmation. */
	response = e_alert_run_dialog_for_args (
		GTK_WINDOW (shell_window),
		"calendar:prompt-delete-calendar",
		e_source_peek_name (source), NULL);
	if (response != GTK_RESPONSE_YES)
		return;

	uri = e_source_get_uri (source);
	client = e_cal_model_get_client_for_uri (model, uri);
	if (client == NULL)
		client = e_cal_new_from_uri (uri, E_CAL_SOURCE_TYPE_EVENT);
	g_free (uri);

	g_return_if_fail (client != NULL);

	if (!e_cal_remove (client, &error)) {
		g_warning ("%s", error->message);
		g_error_free (error);
		return;
	}

	if (e_source_selector_source_is_selected (selector, source)) {
		e_cal_shell_sidebar_remove_source (
			cal_shell_sidebar, source);
		e_source_selector_unselect_source (selector, source);
	}

	source_group = e_source_peek_group (source);
	e_source_group_remove_source (source_group, source);

	source_list = e_cal_shell_backend_get_source_list (
		E_CAL_SHELL_BACKEND (shell_backend));
	if (!e_source_list_sync (source_list, &error)) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
action_calendar_go_back_cb (GtkAction *action,
                            ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	gnome_calendar_previous (calendar);
}

static void
action_calendar_go_forward_cb (GtkAction *action,
                               ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	gnome_calendar_next (calendar);
}

static void
action_calendar_go_today_cb (GtkAction *action,
                             ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	gnome_calendar_goto_today (calendar);
}

static void
action_calendar_jump_to_cb (GtkAction *action,
                            ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;
	GnomeCalendar *calendar;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	goto_dialog (GTK_WINDOW (shell_window), calendar);
}

static void
action_calendar_new_cb (GtkAction *action,
                        ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	calendar_setup_new_calendar (GTK_WINDOW (shell_window));
}

static void
action_calendar_print_cb (GtkAction *action,
                          ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	GnomeCalendar *calendar;
	ECalendarView *view;
	GtkPrintOperationAction print_action;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	view = gnome_calendar_get_calendar_view (calendar, view_type);
	print_action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;

	if (E_IS_CAL_LIST_VIEW (view)) {
		ETable *table;

		table = E_CAL_LIST_VIEW (view)->table;
		print_table (table, _("Print"), _("Calendar"), print_action);
	} else {
		time_t start;

		gnome_calendar_get_current_time_range (calendar, &start, NULL);
		print_calendar (calendar, print_action, start);
	}
}

static void
action_calendar_print_preview_cb (GtkAction *action,
                                  ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	GnomeCalendar *calendar;
	ECalendarView *view;
	GtkPrintOperationAction print_action;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	view = gnome_calendar_get_calendar_view (calendar, view_type);
	print_action = GTK_PRINT_OPERATION_ACTION_PREVIEW;

	if (E_IS_CAL_LIST_VIEW (view)) {
		ETable *table;

		table = E_CAL_LIST_VIEW (view)->table;
		print_table (table, _("Print"), _("Calendar"), print_action);
	} else {
		time_t start;

		gnome_calendar_get_current_time_range (calendar, &start, NULL);
		print_calendar (calendar, print_action, start);
	}
}

static void
action_calendar_properties_cb (GtkAction *action,
                               ECalShellView *cal_shell_view)
{
	ECalShellSidebar *cal_shell_sidebar;
	EShellView *shell_view;
	EShellWindow *shell_window;
	ESource *source;
	ESourceSelector *selector;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);
	source = e_source_selector_peek_primary_selection (selector);
	g_return_if_fail (E_IS_SOURCE (source));

	/* XXX Does this -really- need a source group parameter? */
	calendar_setup_edit_calendar (
		GTK_WINDOW (shell_window), source,
		e_source_peek_group (source));
}

static void
action_calendar_purge_cb (GtkAction *action,
                          ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;
	GtkSpinButton *spin_button;
	GtkWidget *container;
	GtkWidget *dialog;
	GtkWidget *widget;
	gint days;
	time_t tt;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	dialog = gtk_message_dialog_new (
		GTK_WINDOW (shell_window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_WARNING,
		GTK_BUTTONS_OK_CANCEL,
		_("This operation will permanently erase all events older "
		  "than the selected amount of time. If you continue, you "
		  "will not be able to recover these events."));

	gtk_dialog_set_default_response (
		GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	widget = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, FALSE, 6);
	gtk_widget_show (widget);

	container = widget;

	/* Translators: This is the first part of the sentence:
	 * "Purge events older than <<spin-button>> days" */
	widget = gtk_label_new (_("Purge events older than"));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, FALSE, 6);
	gtk_widget_show (widget);

	widget = gtk_spin_button_new_with_range (0.0, 1000.0, 1.0);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), 60.0);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 6);
	gtk_widget_show (widget);

	spin_button = GTK_SPIN_BUTTON (widget);

	/* Translators: This is the last part of the sentence:
	 * "Purge events older than <<spin-button>> days" */
	widget = gtk_label_new (_("days"));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, FALSE, 6);
	gtk_widget_show (widget);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
		goto exit;

	days = gtk_spin_button_get_value_as_int (spin_button);

	tt = time (NULL);
	tt -= (days * (24 * 3600));

	gnome_calendar_purge (calendar, tt);

exit:
	gtk_widget_destroy (dialog);
}

static void
action_calendar_refresh_cb (GtkAction *action,
                           ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalShellSidebar *cal_shell_sidebar;
	ESourceSelector *selector;
	ECal *client;
	ECalModel *model;
	ESource *source;
	gchar *uri;
	GError *error = NULL;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;

	model = e_cal_shell_content_get_model (cal_shell_content);
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);

	source = e_source_selector_peek_primary_selection (selector);
	g_return_if_fail (E_IS_SOURCE (source));

	uri = e_source_get_uri (source);
	client = e_cal_model_get_client_for_uri (model, uri);
	g_free (uri);

	if (client == NULL)
		return;

	g_return_if_fail (e_cal_get_refresh_supported (client));

	if (!e_cal_refresh (client, &error) && error) {
		g_warning (
			"%s: Failed to refresh '%s', %s\n",
			G_STRFUNC, e_source_peek_name (source),
			error->message);
		g_error_free (error);
	}
}

static void
action_calendar_rename_cb (GtkAction *action,
                           ECalShellView *cal_shell_view)
{
	ECalShellSidebar *cal_shell_sidebar;
	ESourceSelector *selector;

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);

	e_source_selector_edit_primary_selection (selector);
}

static void
action_calendar_select_one_cb (GtkAction *action,
                               ECalShellView *cal_shell_view)
{
	ECalShellSidebar *cal_shell_sidebar;
	ESourceSelector *selector;
	ESource *primary;

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);

	primary = e_source_selector_peek_primary_selection (selector);
	g_return_if_fail (primary != NULL);

	e_source_selector_select_exclusive (selector, primary);
}

static void
action_calendar_view_cb (GtkRadioAction *action,
                         GtkRadioAction *current,
                         ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	GnomeCalendarViewType view_type;
	const gchar *view_id;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	view_type = gtk_radio_action_get_current_value (action);

	switch (view_type) {
		case GNOME_CAL_DAY_VIEW:
			view_id = "Day_View";
			break;

		case GNOME_CAL_WORK_WEEK_VIEW:
			view_id = "Work_Week_View";
			break;

		case GNOME_CAL_WEEK_VIEW:
			view_id = "Week_View";
			break;

		case GNOME_CAL_MONTH_VIEW:
			view_id = "Month_View";
			break;

		case GNOME_CAL_LIST_VIEW:
			view_id = "List_View";
			break;

		default:
			g_return_if_reached ();
	}

	e_shell_view_set_view_id (shell_view, view_id);
}

static void
action_event_all_day_new_cb (GtkAction *action,
                             ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	ECalendarView *calendar_view;
	GnomeCalendar *calendar;

	/* These are just for readability. */
	gboolean all_day = TRUE;
	gboolean meeting = FALSE;
	gboolean no_past_date = FALSE;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	e_calendar_view_new_appointment_full (
		calendar_view, all_day, meeting, no_past_date);
}

static void
action_event_copy_cb (GtkAction *action,
                      ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	GnomeCalendar *calendar;
	ECalendarView *calendar_view;
	ESource *source_source = NULL, *destination_source = NULL;
	ECal *destination_client = NULL;
	GList *selected, *iter;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (selected != NULL);

	if (selected->data) {
		ECalendarViewEvent *event = selected->data;

		if (is_comp_data_valid (event) && event->comp_data->client)
			source_source = e_cal_get_source (event->comp_data->client);
	}

	/* Get a destination source from the user. */
	destination_source = select_source_dialog (
		GTK_WINDOW (shell_window), E_CAL_SOURCE_TYPE_EVENT, source_source);
	if (destination_source == NULL)
		return;

	/* Open the destination calendar. */
	destination_client = e_auth_new_cal_from_source (
		destination_source, E_CAL_SOURCE_TYPE_EVENT);
	if (destination_client == NULL)
		goto exit;
	if (!e_cal_open (destination_client, FALSE, NULL))
		goto exit;

	e_cal_shell_view_set_status_message (
		cal_shell_view, _("Copying Items"), -1.0);

	for (iter = selected; iter != NULL; iter = iter->next) {
		ECalendarViewEvent *event = iter->data;
		gboolean remove = FALSE;

		e_cal_shell_view_transfer_item_to (
			cal_shell_view, event, destination_client, remove);
	}

	e_cal_shell_view_set_status_message (cal_shell_view, NULL, -1.0);

exit:
	if (destination_client != NULL)
		g_object_unref (destination_client);
	if (destination_source != NULL)
		g_object_unref (destination_source);
	g_list_free (selected);
}

static void
action_event_delegate_cb (GtkAction *action,
                          ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	GnomeCalendar *calendar;
	ECalendarView *calendar_view;
	ECalendarViewEvent *event;
	ECalComponent *component;
	ECal *client;
	GList *selected;
	icalcomponent *clone;
	icalproperty *property;
	gboolean found = FALSE;
	gchar *attendee;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_list_length (selected) == 1);

	event = selected->data;

	if (!is_comp_data_valid (event))
		return;

	client = event->comp_data->client;
	clone = icalcomponent_new_clone (event->comp_data->icalcomp);

	/* Set the attendee status for the delegate. */

	component = e_cal_component_new ();
	e_cal_component_set_icalcomponent (
		component, icalcomponent_new_clone (clone));

	attendee = itip_get_comp_attendee (component, client);
	property = icalcomponent_get_first_property (
		clone, ICAL_ATTENDEE_PROPERTY);

	while (property != NULL) {
		const gchar *candidate;

		candidate = icalproperty_get_attendee (property);
		candidate = itip_strip_mailto (candidate);

		if (g_ascii_strcasecmp (candidate, attendee) == 0) {
			icalparameter *parameter;

			parameter = icalparameter_new_role (
				ICAL_ROLE_NONPARTICIPANT);
			icalproperty_set_parameter (property, parameter);

			parameter = icalparameter_new_partstat (
				ICAL_PARTSTAT_DELEGATED);
			icalproperty_set_parameter (property, parameter);

			found = TRUE;
			break;
		}

		property = icalcomponent_get_next_property (
			clone, ICAL_ATTENDEE_PROPERTY);
	}

	/* If the attendee is not already in the component, add it. */
	if (!found) {
		icalparameter *parameter;
		gchar *address;

		address = g_strdup_printf ("MAILTO:%s", attendee);

		property = icalproperty_new_attendee (address);
		icalcomponent_add_property (clone, property);

		parameter = icalparameter_new_role (ICAL_ROLE_NONPARTICIPANT);
		icalproperty_add_parameter (property, parameter);

		parameter = icalparameter_new_cutype (ICAL_CUTYPE_INDIVIDUAL);
		icalproperty_add_parameter (property, parameter);

		parameter = icalparameter_new_rsvp (ICAL_RSVP_TRUE);
		icalproperty_add_parameter (property, parameter);

		g_free (address);
	}

	g_free (attendee);
	g_object_unref (component);

	e_calendar_view_open_event_with_flags (
		calendar_view, event->comp_data->client, clone,
		COMP_EDITOR_MEETING | COMP_EDITOR_DELEGATE);

	icalcomponent_free (clone);
	g_list_free (selected);
}

static void
action_event_delete_cb (GtkAction *action,
                        ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;
	GnomeCalendarViewType view_type;
	ECalendarView *calendar_view;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	e_selectable_delete_selection (E_SELECTABLE (calendar_view));
}

static void
action_event_delete_occurrence_cb (GtkAction *action,
                                   ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;
	GnomeCalendarViewType view_type;
	ECalendarView *calendar_view;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	e_calendar_view_delete_selected_occurrence (calendar_view);
}

static void
action_event_forward_cb (GtkAction *action,
                         ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	ECalendarView *calendar_view;
	GnomeCalendar *calendar;
	ECalendarViewEvent *event;
	ECalComponent *component;
	ECal *client;
	icalcomponent *icalcomp;
	GList *selected;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_list_length (selected) == 1);

	event = selected->data;

	if (!is_comp_data_valid (event))
		return;

	client = event->comp_data->client;
	icalcomp = event->comp_data->icalcomp;

	component = e_cal_component_new ();

	e_cal_component_set_icalcomponent (
		component, icalcomponent_new_clone (icalcomp));
	itip_send_comp (
		E_CAL_COMPONENT_METHOD_PUBLISH,
		component, client, NULL, NULL, NULL, TRUE, FALSE);

	g_object_unref (component);

	g_list_free (selected);
}

static void
action_event_meeting_new_cb (GtkAction *action,
                             ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	ECalendarView *calendar_view;
	GnomeCalendar *calendar;

	/* These are just for readability. */
	gboolean all_day = FALSE;
	gboolean meeting = TRUE;
	gboolean no_past_date = FALSE;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	e_calendar_view_new_appointment_full (
		calendar_view, all_day, meeting, no_past_date);
}

static void
action_event_move_cb (GtkAction *action,
                      ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	GnomeCalendar *calendar;
	ECalendarView *calendar_view;
	ESource *source_source = NULL, *destination_source = NULL;
	ECal *destination_client = NULL;
	GList *selected, *iter;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (selected != NULL);

	if (selected->data) {
		ECalendarViewEvent *event = selected->data;

		if (is_comp_data_valid (event) && event->comp_data->client)
			source_source = e_cal_get_source (event->comp_data->client);
	}

	/* Get a destination source from the user. */
	destination_source = select_source_dialog (
		GTK_WINDOW (shell_window), E_CAL_SOURCE_TYPE_EVENT, source_source);
	if (destination_source == NULL)
		return;

	/* Open the destination calendar. */
	destination_client = e_auth_new_cal_from_source (
		destination_source, E_CAL_SOURCE_TYPE_EVENT);
	if (destination_client == NULL)
		goto exit;
	if (!e_cal_open (destination_client, FALSE, NULL))
		goto exit;

	e_cal_shell_view_set_status_message (
		cal_shell_view, _("Moving Items"), -1.0);

	for (iter = selected; iter != NULL; iter = iter->next) {
		ECalendarViewEvent *event = iter->data;
		gboolean remove = TRUE;

		e_cal_shell_view_transfer_item_to (
			cal_shell_view, event, destination_client, remove);
	}

	e_cal_shell_view_set_status_message (cal_shell_view, NULL, -1.0);

exit:
	if (destination_client != NULL)
		g_object_unref (destination_client);
	if (destination_source != NULL)
		g_object_unref (destination_source);
	g_list_free (selected);
}

static void
action_event_new_cb (GtkAction *action,
                     ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	ECalendarView *calendar_view;
	GnomeCalendar *calendar;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	e_calendar_view_new_appointment (calendar_view);
}

static void
action_event_occurrence_movable_cb (GtkAction *action,
                                    ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	GnomeCalendar *calendar;
	ECalModel *model;
	ECalendarView *calendar_view;
	ECalendarViewEvent *event;
	ECalComponent *exception_component;
	ECalComponent *recurring_component;
	ECalComponentDateTime date;
	ECalComponentId *id;
	ECal *client;
	icalcomponent *icalcomp;
	icaltimetype itt;
	icaltimezone *timezone;
	GList *selected;
	gchar *uid;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	model = e_calendar_view_get_model (calendar_view);
	timezone = e_cal_model_get_timezone (model);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_list_length (selected) == 1);

	event = selected->data;

	if (!is_comp_data_valid (event))
		return;

	client = event->comp_data->client;
	icalcomp = event->comp_data->icalcomp;

	/* For the recurring object, we add an exception
	 * to get rid of the instance. */

	recurring_component = e_cal_component_new ();
	e_cal_component_set_icalcomponent (
		recurring_component, icalcomponent_new_clone (icalcomp));
	id = e_cal_component_get_id (recurring_component);

	/* For the unrecurred instance, we duplicate the original object,
	 * create a new UID for it, get rid of the recurrence rules, and
	 * set the start and end times to the instance times. */

	exception_component = e_cal_component_new ();
	e_cal_component_set_icalcomponent (
		exception_component, icalcomponent_new_clone (icalcomp));

	uid = e_cal_component_gen_uid ();
	e_cal_component_set_uid (exception_component, uid);
	g_free (uid);

	e_cal_component_set_recurid (exception_component, NULL);
	e_cal_component_set_rdate_list (exception_component, NULL);
	e_cal_component_set_rrule_list (exception_component, NULL);
	e_cal_component_set_exdate_list (exception_component, NULL);
	e_cal_component_set_exrule_list (exception_component, NULL);

	date.value = &itt;
	date.tzid = icaltimezone_get_tzid (timezone);
	*date.value = icaltime_from_timet_with_zone (
		event->comp_data->instance_start, FALSE, timezone);
	cal_comp_set_dtstart_with_oldzone (client, exception_component, &date);
	*date.value = icaltime_from_timet_with_zone (
		event->comp_data->instance_end, FALSE, timezone);
	cal_comp_set_dtstart_with_oldzone (client, exception_component, &date);
	e_cal_component_commit_sequence (exception_component);

	/* Now update both ECalComponents.  Note that we do this last
	*  since at present the updates happend synchronously so our
	*  event may disappear. */

	e_cal_remove_object_with_mod (
		client, id->uid, id->rid, CALOBJ_MOD_THIS, NULL);

	e_cal_component_free_id (id);
	g_object_unref (recurring_component);

	icalcomp = e_cal_component_get_icalcomponent (exception_component);
	if (e_cal_create_object (client, icalcomp, &uid, NULL))
		g_free (uid);

	g_object_unref (exception_component);

	g_list_free (selected);
}

static void
action_event_open_cb (GtkAction *action,
                      ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	GnomeCalendar *calendar;
	ECalendarView *view;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	view = gnome_calendar_get_calendar_view (calendar, view_type);

	e_calendar_view_open_event (view);
}

static void
action_event_print_cb (GtkAction *action,
                       ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	GnomeCalendar *calendar;
	ECalendarView *calendar_view;
	ECalendarViewEvent *event;
	ECalComponent *component;
	ECal *client;
	icalcomponent *icalcomp;
	GList *selected;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_list_length (selected) == 1);

	event = selected->data;

	if (!is_comp_data_valid (event))
		return;

	client = event->comp_data->client;
	icalcomp = event->comp_data->icalcomp;

	component = e_cal_component_new ();

	e_cal_component_set_icalcomponent (
		component, icalcomponent_new_clone (icalcomp));
	print_comp (
		component, client, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);

	g_object_unref (component);

	g_list_free (selected);
}

static void
action_event_reply_cb (GtkAction *action,
                       ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	ECalendarView *calendar_view;
	GnomeCalendar *calendar;
	ECalendarViewEvent *event;
	ECalComponent *component;
	ECal *client;
	icalcomponent *icalcomp;
	GList *selected;
	gboolean reply_all = FALSE;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_list_length (selected) == 1);

	event = selected->data;

	if (!is_comp_data_valid (event))
		return;

	client = event->comp_data->client;
	icalcomp = event->comp_data->icalcomp;

	component = e_cal_component_new ();

	e_cal_component_set_icalcomponent (
		component, icalcomponent_new_clone (icalcomp));
	reply_to_calendar_comp (
		E_CAL_COMPONENT_METHOD_REPLY,
		component, client, reply_all, NULL, NULL);

	g_object_unref (component);

	g_list_free (selected);
}

static void
action_event_reply_all_cb (GtkAction *action,
                           ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	ECalendarView *calendar_view;
	GnomeCalendar *calendar;
	ECalendarViewEvent *event;
	ECalComponent *component;
	ECal *client;
	icalcomponent *icalcomp;
	GList *selected;
	gboolean reply_all = TRUE;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_list_length (selected) == 1);

	event = selected->data;

	if (!is_comp_data_valid (event))
		return;

	client = event->comp_data->client;
	icalcomp = event->comp_data->icalcomp;

	component = e_cal_component_new ();

	e_cal_component_set_icalcomponent (
		component, icalcomponent_new_clone (icalcomp));
	reply_to_calendar_comp (
		E_CAL_COMPONENT_METHOD_REPLY,
		component, client, reply_all, NULL, NULL);

	g_object_unref (component);

	g_list_free (selected);
}

static void
action_event_save_as_cb (GtkAction *action,
                         ECalShellView *cal_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	GnomeCalendar *calendar;
	ECalendarView *calendar_view;
	ECalendarViewEvent *event;
	ECal *client;
	icalcomponent *icalcomp;
	EActivity *activity;
	GList *selected;
	GFile *file;
	gchar *string = NULL;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_list_length (selected) == 1);

	event = selected->data;

	if (!is_comp_data_valid (event))
		return;

	client = event->comp_data->client;
	icalcomp = event->comp_data->icalcomp;

	/* Translators: Default filename part saving an event to a file when
	 * no summary is filed, the '.ics' extension is concatenated to it. */
	string = icalcomp_suggest_filename (icalcomp, _("event"));
	file = e_shell_run_save_dialog (
		shell, _("Save as iCalendar"), string,
		"*.ics:text/calendar", NULL, NULL);
	g_free (string);
	if (file == NULL)
		return;

	string = e_cal_get_component_as_string (client, icalcomp);
	if (string == NULL) {
		g_warning ("Could not convert item to a string");
		goto exit;
	}

	/* XXX No callbacks means errors are discarded. */
	activity = e_file_replace_contents_async (
		file, string, strlen (string), NULL, FALSE,
		G_FILE_CREATE_NONE, (GAsyncReadyCallback) NULL, NULL);
	e_shell_backend_add_activity (shell_backend, activity);

	/* Free the string when the activity is finalized. */
	g_object_set_data_full (
		G_OBJECT (activity),
		"file-content", string,
		(GDestroyNotify) g_free);

exit:
	g_object_unref (file);

	g_list_free (selected);
}

static void
edit_event_as (ECalShellView *cal_shell_view, gboolean as_meeting)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	GnomeCalendar *calendar;
	ECalendarView *calendar_view;
	ECalendarViewEvent *event;
	ECal *client;
	icalcomponent *icalcomp;
	GList *selected;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_list_length (selected) == 1);

	event = selected->data;

	if (!is_comp_data_valid (event))
		return;

	client = event->comp_data->client;
	icalcomp = event->comp_data->icalcomp;

	if (!as_meeting && icalcomp) {
		/* remove organizer and all attendees */
		icalproperty *prop;

		/* do it on a copy, as user can cancel changes */
		icalcomp = icalcomponent_new_clone (icalcomp);

		prop = icalcomponent_get_first_property (
			icalcomp, ICAL_ATTENDEE_PROPERTY);
		while (prop != NULL) {
			icalcomponent_remove_property (icalcomp, prop);
			icalproperty_free (prop);

			prop = icalcomponent_get_first_property (
				icalcomp, ICAL_ATTENDEE_PROPERTY);
		}

		prop = icalcomponent_get_first_property (
			icalcomp, ICAL_ORGANIZER_PROPERTY);
		while (prop != NULL) {
			icalcomponent_remove_property (icalcomp, prop);
			icalproperty_free (prop);

			prop = icalcomponent_get_first_property (
				icalcomp, ICAL_ORGANIZER_PROPERTY);
		}
	}

	e_calendar_view_edit_appointment (
		calendar_view, client, icalcomp, as_meeting);

	if (!as_meeting && icalcomp) {
		icalcomponent_free (icalcomp);
	}

	g_list_free (selected);
}

static void
action_event_schedule_cb (GtkAction *action,
                          ECalShellView *cal_shell_view)
{
	edit_event_as (cal_shell_view, TRUE);
}

static void
quit_calendar_cb (GtkAction *action,
                  ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GdkWindow *window;
	GdkEvent *event;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	/* Synthesize a delete_event on this window. */
	event = gdk_event_new (GDK_DELETE);
	window = gtk_widget_get_window (GTK_WIDGET (shell_window));
	event->any.window = g_object_ref (window);
	event->any.send_event = TRUE;
	gtk_main_do_event (event);
	gdk_event_free (event);

}

static void
action_event_schedule_appointment_cb (GtkAction *action,
                                      ECalShellView *cal_shell_view)
{
	edit_event_as (cal_shell_view, FALSE);
}

static void
action_gal_save_custom_view_cb (GtkAction *action,
                                ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EShellView *shell_view;
	GalViewInstance *view_instance;

	/* All shell views respond to the activation of this action,
	 * which is defined by EShellWindow.  But only the currently
	 * active shell view proceeds with saving the custom view. */
	shell_view = E_SHELL_VIEW (cal_shell_view);
	if (!e_shell_view_is_active (shell_view))
		return;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	view_instance = e_cal_shell_content_get_view_instance (cal_shell_content);
	gal_view_instance_save_as (view_instance);
}

static GtkActionEntry calendar_entries[] = {

	{ "calendar-copy",
	  GTK_STOCK_COPY,
	  N_("_Copy..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_copy_cb) },

	{ "calendar-delete",
	  GTK_STOCK_DELETE,
	  N_("D_elete Calendar"),
	  NULL,
	  N_("Delete the selected calendar"),
	  G_CALLBACK (action_calendar_delete_cb) },

	{ "calendar-go-back",
	  GTK_STOCK_GO_BACK,
	  N_("Previous"),
	  NULL,
	  N_("Go Back"),
	  G_CALLBACK (action_calendar_go_back_cb) },

	{ "calendar-go-forward",
	  GTK_STOCK_GO_FORWARD,
	  N_("Next"),
	  NULL,
	  N_("Go Forward"),
	  G_CALLBACK (action_calendar_go_forward_cb) },

	{ "calendar-go-today",
	  "go-today",
	  N_("Select _Today"),
	  "<Control>t",
	  N_("Select today"),
	  G_CALLBACK (action_calendar_go_today_cb) },

	{ "calendar-jump-to",
	  GTK_STOCK_JUMP_TO,
	  N_("Select _Date"),
	  "<Control>g",
	  N_("Select a specific date"),
	  G_CALLBACK (action_calendar_jump_to_cb) },

	{ "calendar-new",
	  "x-office-calendar",
	  N_("_New Calendar"),
	  NULL,
	  N_("Create a new calendar"),
	  G_CALLBACK (action_calendar_new_cb) },

	{ "calendar-properties",
	  GTK_STOCK_PROPERTIES,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_properties_cb) },

	{ "calendar-purge",
	  NULL,
	  N_("Purg_e"),
	  "<Control>e",
	  N_("Purge old appointments and meetings"),
	  G_CALLBACK (action_calendar_purge_cb) },

	{ "calendar-refresh",
	  GTK_STOCK_REFRESH,
	  N_("Re_fresh"),
	  NULL,
	  N_("Refresh the selected calendar"),
	  G_CALLBACK (action_calendar_refresh_cb) },

	{ "calendar-rename",
	  NULL,
	  N_("_Rename..."),
	  "F2",
	  N_("Rename the selected calendar"),
	  G_CALLBACK (action_calendar_rename_cb) },

	{ "calendar-select-one",
	  "stock_check-filled",
	  N_("Show _Only This Calendar"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_select_one_cb) },

	{ "event-copy",
	  NULL,
	  N_("Cop_y to Calendar..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_event_copy_cb) },

	{ "event-delegate",
	  NULL,
	  N_("_Delegate Meeting..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_event_delegate_cb) },

	{ "event-delete",
	  GTK_STOCK_DELETE,
	  N_("_Delete Appointment"),
	  "<Control>d",
	  N_("Delete selected appointments"),
	  G_CALLBACK (action_event_delete_cb) },

	{ "event-delete-occurrence",
	  GTK_STOCK_DELETE,
	  N_("Delete This _Occurrence"),
	  NULL,
	  N_("Delete this occurrence"),
	  G_CALLBACK (action_event_delete_occurrence_cb) },

	{ "event-delete-occurrence-all",
	  GTK_STOCK_DELETE,
	  N_("Delete All Occ_urrences"),
	  NULL,
	  N_("Delete all occurrences"),
	  G_CALLBACK (action_event_delete_cb) },

	{ "event-all-day-new",
	  "stock_new-24h-appointment",
	  N_("New All Day _Event..."),
	  NULL,
	  N_("Create a new all day event"),
	  G_CALLBACK (action_event_all_day_new_cb) },

	{ "event-forward",
	  "mail-forward",
	  N_("_Forward as iCalendar..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_event_forward_cb) },

	{ "event-meeting-new",
	  "stock_new-meeting",
	  N_("New _Meeting..."),
	  NULL,
	  N_("Create a new meeting"),
	  G_CALLBACK (action_event_meeting_new_cb) },

	{ "event-move",
	  NULL,
	  N_("Mo_ve to Calendar..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_event_move_cb) },

	{ "event-new",
	  "appointment-new",
	  N_("New _Appointment..."),
	  NULL,
	  N_("Create a new appointment"),
	  G_CALLBACK (action_event_new_cb) },

	{ "event-occurrence-movable",
	  NULL,
	  N_("Make this Occurrence _Movable"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_event_occurrence_movable_cb) },

	{ "event-open",
	  GTK_STOCK_OPEN,
	  N_("_Open Appointment"),
	  "<Control>o",
	  N_("View the current appointment"),
	  G_CALLBACK (action_event_open_cb) },

	{ "event-reply",
	  "mail-reply-sender",
	  N_("_Reply"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_event_reply_cb) },

	{ "event-reply-all",
	  "mail-reply-all",
	  N_("Reply to _All"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_event_reply_all_cb) },

	{ "event-save-as",
	  GTK_STOCK_SAVE_AS,
	  N_("Save as iCalendar..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_event_save_as_cb) },

	{ "event-schedule",
	  NULL,
	  N_("_Schedule Meeting..."),
	  NULL,
	  N_("Converts an appointment to a meeting"),
	  G_CALLBACK (action_event_schedule_cb) },

	{ "event-schedule-appointment",
	  NULL,
	  N_("Conv_ert to Appointment..."),
	  NULL,
	  N_("Converts a meeting to an appointment"),
	  G_CALLBACK (action_event_schedule_appointment_cb) },

	{ "quit-calendar",
	  GTK_STOCK_CLOSE,
	  N_("Quit"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (quit_calendar_cb) },

	/*** Menus ***/

	{ "calendar-actions-menu",
	  NULL,
	  N_("_Actions"),
	  NULL,
	  NULL,
	  NULL }
};

static EPopupActionEntry calendar_popup_entries[] = {

	/* FIXME No equivalent main menu items for the any of the calendar
	 *       popup menu items and for many of the event popup menu items.
	 *       This is an accessibility issue. */

	{ "calendar-popup-copy",
	  NULL,
	  "calendar-copy" },

	{ "calendar-popup-delete",
	  N_("_Delete"),
	  "calendar-delete" },

	{ "calendar-popup-go-today",
	  NULL,
	  "calendar-go-today" },

	{ "calendar-popup-jump-to",
	  NULL,
	  "calendar-jump-to" },

	{ "calendar-popup-properties",
	  NULL,
	  "calendar-properties" },

	{ "calendar-popup-refresh",
	  NULL,
	  "calendar-refresh" },

	{ "calendar-popup-rename",
	  NULL,
	  "calendar-rename" },

	{ "calendar-popup-select-one",
	  NULL,
	  "calendar-select-one" },

	{ "event-popup-copy",
	  NULL,
	  "event-copy" },

	{ "event-popup-delegate",
	  NULL,
	  "event-delegate" },

	{ "event-popup-delete",
	  NULL,
	  "event-delete" },

	{ "event-popup-delete-occurrence",
	  NULL,
	  "event-delete-occurrence" },

	{ "event-popup-delete-occurrence-all",
	  NULL,
	  "event-delete-occurrence-all" },

	{ "event-popup-forward",
	  NULL,
	  "event-forward" },

	{ "event-popup-move",
	  NULL,
	  "event-move" },

	{ "event-popup-occurrence-movable",
	  NULL,
	  "event-occurrence-movable" },

	{ "event-popup-open",
	  NULL,
	  "event-open" },

	{ "event-popup-reply",
	  NULL,
	  "event-reply" },

	{ "event-popup-reply-all",
	  NULL,
	  "event-reply-all" },

	{ "event-popup-save-as",
	  NULL,
	  "event-save-as" },

	{ "event-popup-schedule",
	  NULL,
	  "event-schedule" },

	{ "event-popup-schedule-appointment",
	  NULL,
	  "event-schedule-appointment" }
};

static GtkRadioActionEntry calendar_view_entries[] = {

	/* This action represents the initial calendar view.
	 * It should not be visible in the UI, nor should it be
	 * possible to switch to it from another calendar view. */
	{ "calendar-view-initial",
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  BOGUS_INITIAL_VALUE },

	{ "calendar-view-day",
	  "view-calendar-day",
	  N_("Day"),
	  NULL,
	  N_("Show one day"),
	  GNOME_CAL_DAY_VIEW },

	{ "calendar-view-list",
	  "view-calendar-list",
	  N_("List"),
	  NULL,
	  N_("Show as list"),
	  GNOME_CAL_LIST_VIEW },

	{ "calendar-view-month",
	  "view-calendar-month",
	  N_("Month"),
	  NULL,
	  N_("Show one month"),
	  GNOME_CAL_MONTH_VIEW },

	{ "calendar-view-week",
	  "view-calendar-week",
	  N_("Week"),
	  NULL,
	  N_("Show one week"),
	  GNOME_CAL_WEEK_VIEW },

	{ "calendar-view-workweek",
	  "view-calendar-workweek",
	  N_("Work Week"),
	  NULL,
	  N_("Show one work week"),
	  GNOME_CAL_WORK_WEEK_VIEW }
};

static GtkRadioActionEntry calendar_filter_entries[] = {

	{ "calendar-filter-active-appointments",
	  NULL,
	  N_("Active Appointments"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  CALENDAR_FILTER_ACTIVE_APPOINTMENTS },

	{ "calendar-filter-any-category",
	  NULL,
	  N_("Any Category"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  CALENDAR_FILTER_ANY_CATEGORY },

	{ "calendar-filter-next-7-days-appointments",
	  NULL,
	  N_("Next 7 Days' Appointments"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  CALENDAR_FILTER_NEXT_7_DAYS_APPOINTMENTS },

	{ "calendar-filter-unmatched",
	  NULL,
	  N_("Unmatched"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  CALENDAR_FILTER_UNMATCHED }
};

static GtkRadioActionEntry calendar_search_entries[] = {

	{ "calendar-search-advanced-hidden",
	  NULL,
	  N_("Advanced Search"),
	  NULL,
	  NULL,
	  CALENDAR_SEARCH_ADVANCED },

	{ "calendar-search-any-field-contains",
	  NULL,
	  N_("Any field contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  CALENDAR_SEARCH_ANY_FIELD_CONTAINS },

	{ "calendar-search-description-contains",
	  NULL,
	  N_("Description contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  CALENDAR_SEARCH_DESCRIPTION_CONTAINS },

	{ "calendar-search-summary-contains",
	  NULL,
	  N_("Summary contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  CALENDAR_SEARCH_SUMMARY_CONTAINS }
};

static GtkActionEntry lockdown_printing_entries[] = {

	{ "calendar-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  "<Control>p",
	  N_("Print this calendar"),
	  G_CALLBACK (action_calendar_print_cb) },

	{ "calendar-print-preview",
	  GTK_STOCK_PRINT_PREVIEW,
	  NULL,
	  NULL,
	  N_("Preview the calendar to be printed"),
	  G_CALLBACK (action_calendar_print_preview_cb) },

	{ "event-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_event_print_cb) }
};

static EPopupActionEntry lockdown_printing_popup_entries[] = {

	{ "event-popup-print",
	  NULL,
	  "event-print" }
};

void
e_cal_shell_view_actions_init (ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSearchbar *searchbar;
	GtkActionGroup *action_group;
	GtkAction *action;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	searchbar = e_cal_shell_content_get_searchbar (cal_shell_content);

	/* Calendar Actions */
	action_group = ACTION_GROUP (CALENDAR);
	gtk_action_group_add_actions (
		action_group, calendar_entries,
		G_N_ELEMENTS (calendar_entries), cal_shell_view);
	e_action_group_add_popup_actions (
		action_group, calendar_popup_entries,
		G_N_ELEMENTS (calendar_popup_entries));
	gtk_action_group_add_radio_actions (
		action_group, calendar_view_entries,
		G_N_ELEMENTS (calendar_view_entries), BOGUS_INITIAL_VALUE,
		G_CALLBACK (action_calendar_view_cb), cal_shell_view);
	gtk_action_group_add_radio_actions (
		action_group, calendar_search_entries,
		G_N_ELEMENTS (calendar_search_entries),
		-1, NULL, NULL);

	/* Advanced Search Action */
	action = ACTION (CALENDAR_SEARCH_ADVANCED_HIDDEN);
	gtk_action_set_visible (action, FALSE);
	if (searchbar)
		e_shell_searchbar_set_search_option (
			searchbar, GTK_RADIO_ACTION (action));

	/* Lockdown Printing Actions */
	action_group = ACTION_GROUP (LOCKDOWN_PRINTING);
	gtk_action_group_add_actions (
		action_group, lockdown_printing_entries,
		G_N_ELEMENTS (lockdown_printing_entries), cal_shell_view);
	e_action_group_add_popup_actions (
		action_group, lockdown_printing_popup_entries,
		G_N_ELEMENTS (lockdown_printing_popup_entries));

	/* Fine tuning. */

	action = ACTION (CALENDAR_GO_TODAY);
	g_object_set (action, "short-label", _("Today"), NULL);

	action = ACTION (CALENDAR_JUMP_TO);
	g_object_set (action, "short-label", _("Go To"), NULL);

	action = ACTION (CALENDAR_VIEW_DAY);
	g_object_set (action, "is-important", TRUE, NULL);

	action = ACTION (CALENDAR_VIEW_LIST);
	g_object_set (action, "is-important", TRUE, NULL);

	action = ACTION (CALENDAR_VIEW_MONTH);
	g_object_set (action, "is-important", TRUE, NULL);

	action = ACTION (CALENDAR_VIEW_WEEK);
	g_object_set (action, "is-important", TRUE, NULL);

	action = ACTION (CALENDAR_VIEW_WORKWEEK);
	g_object_set (action, "is-important", TRUE, NULL);

	g_signal_connect (
		ACTION (GAL_SAVE_CUSTOM_VIEW), "activate",
		G_CALLBACK (action_gal_save_custom_view_cb), cal_shell_view);

	/* Initialize the memo and task pad actions. */
	e_cal_shell_view_memopad_actions_init (cal_shell_view);
	e_cal_shell_view_taskpad_actions_init (cal_shell_view);
}

void
e_cal_shell_view_update_search_filter (ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	GtkActionGroup *action_group;
	GtkRadioAction *radio_action;
	GList *list, *iter;
	GSList *group;
	gint ii;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	action_group = ACTION_GROUP (CALENDAR_FILTER);
	e_action_group_remove_all_actions (action_group);

	/* Add the standard filter actions.  No callback is needed
	 * because changes in the EActionComboBox are detected and
	 * handled by EShellSearchbar. */
	gtk_action_group_add_radio_actions (
		action_group, calendar_filter_entries,
		G_N_ELEMENTS (calendar_filter_entries),
		CALENDAR_FILTER_ANY_CATEGORY, NULL, NULL);

	/* Retrieve the radio group from an action we just added. */
	list = gtk_action_group_list_actions (action_group);
	radio_action = GTK_RADIO_ACTION (list->data);
	group = gtk_radio_action_get_group (radio_action);
	g_list_free (list);

	/* Build the category actions. */

	list = e_util_get_searchable_categories ();
	for (iter = list, ii = 0; iter != NULL; iter = iter->next, ii++) {
		const gchar *category_name = iter->data;
		const gchar *filename;
		GtkAction *action;
		gchar *action_name;

		action_name = g_strdup_printf (
			"calendar-filter-category-%d", ii);
		radio_action = gtk_radio_action_new (
			action_name, category_name, NULL, NULL, ii);
		g_free (action_name);

		/* Convert the category icon file to a themed icon name. */
		filename = e_categories_get_icon_file_for (category_name);
		if (filename != NULL && *filename != '\0') {
			gchar *basename;
			gchar *cp;

			basename = g_path_get_basename (filename);

			/* Lose the file extension. */
			if ((cp = strrchr (basename, '.')) != NULL)
				*cp = '\0';

			g_object_set (
				radio_action, "icon-name", basename, NULL);

			g_free (basename);
		}

		gtk_radio_action_set_group (radio_action, group);
		group = gtk_radio_action_get_group (radio_action);

		/* The action group takes ownership of the action. */
		action = GTK_ACTION (radio_action);
		gtk_action_group_add_action (action_group, action);
		g_object_unref (radio_action);
	}
	g_list_free (list);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	searchbar = e_cal_shell_content_get_searchbar (cal_shell_content);
	if (searchbar) {
		combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);

		e_shell_view_block_execute_search (shell_view);

		/* Use any action in the group; doesn't matter which. */
		e_action_combo_box_set_action (combo_box, radio_action);

		ii = CALENDAR_FILTER_UNMATCHED;
		e_action_combo_box_add_separator_after (combo_box, ii);

		ii = CALENDAR_FILTER_NEXT_7_DAYS_APPOINTMENTS;
		e_action_combo_box_add_separator_after (combo_box, ii);

		e_shell_view_unblock_execute_search (shell_view);
	}
}
