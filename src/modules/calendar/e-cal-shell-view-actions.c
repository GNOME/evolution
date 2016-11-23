/*
 * e-cal-shell-view-actions.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "calendar/gui/e-cal-dialogs.h"
#include "calendar/gui/e-cal-ops.h"
#include "calendar/gui/e-comp-editor.h"
#include "calendar/gui/itip-utils.h"
#include "calendar/gui/print.h"

#include "e-cal-base-shell-view.h"
#include "e-cal-shell-view-private.h"
#include "e-cal-shell-view.h"

/* This is for radio action groups whose value is persistent.  We
 * initialize it to a bogus value to ensure a "changed" signal is
 * emitted when a valid value is restored. */
#define BOGUS_INITIAL_VALUE G_MININT

static void
action_calendar_copy_cb (GtkAction *action,
			 EShellView *shell_view)
{
	e_cal_base_shell_view_copy_calendar (shell_view);
}

static void
action_calendar_delete_cb (GtkAction *action,
                           ECalShellView *cal_shell_view)
{
	ECalBaseShellSidebar *cal_shell_sidebar;
	EShellWindow *shell_window;
	EShellView *shell_view;
	ESource *source;
	ESourceSelector *selector;
	gint response;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (cal_shell_sidebar);

	source = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (source != NULL);

	if (e_source_get_remote_deletable (source)) {
		response = e_alert_run_dialog_for_args (
			GTK_WINDOW (shell_window),
			"calendar:prompt-delete-remote-calendar",
			e_source_get_display_name (source), NULL);

		if (response == GTK_RESPONSE_YES)
			e_shell_view_remote_delete_source (shell_view, source);

	} else {
		response = e_alert_run_dialog_for_args (
			GTK_WINDOW (shell_window),
			"calendar:prompt-delete-calendar",
			e_source_get_display_name (source), NULL);

		if (response == GTK_RESPONSE_YES)
			e_shell_view_remove_source (shell_view, source);
	}

	g_object_unref (source);
}

static void
action_calendar_go_back_cb (GtkAction *action,
                            ECalShellView *cal_shell_view)
{
	e_cal_shell_content_move_view_range (
		cal_shell_view->priv->cal_shell_content, E_CALENDAR_VIEW_MOVE_PREVIOUS, 0);
}

static void
action_calendar_go_forward_cb (GtkAction *action,
                               ECalShellView *cal_shell_view)
{
	e_cal_shell_content_move_view_range (
		cal_shell_view->priv->cal_shell_content, E_CALENDAR_VIEW_MOVE_NEXT, 0);
}

static void
action_calendar_go_today_cb (GtkAction *action,
                             ECalShellView *cal_shell_view)
{
	e_cal_shell_content_move_view_range (
		cal_shell_view->priv->cal_shell_content, E_CALENDAR_VIEW_MOVE_TO_TODAY, 0);
}

static void
action_calendar_jump_to_cb (GtkAction *action,
                            ECalShellView *cal_shell_view)
{
	ECalDataModel *data_model;
	ECalShellContent *cal_shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;
	GDate range_start, range_end;
	ECalendarViewMoveType move_type;
	time_t exact_date = time (NULL);

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	e_cal_shell_content_get_current_range_dates (cal_shell_content, &range_start, &range_end);
	data_model = e_cal_base_shell_content_get_data_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));

	if (e_cal_dialogs_goto_run (GTK_WINDOW (shell_window), data_model, &range_start, &move_type, &exact_date))
		e_cal_shell_content_move_view_range (cal_shell_content, move_type, exact_date);
}

static void
action_calendar_manage_groups_cb (GtkAction *action,
				  ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	ESourceSelector *selector;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	selector = e_cal_base_shell_sidebar_get_selector (cal_shell_view->priv->cal_shell_sidebar);

	if (e_source_selector_manage_groups (selector) &&
	    e_source_selector_save_groups_setup (selector, e_shell_view_get_state_key_file (shell_view)))
		e_shell_view_set_state_dirty (shell_view);
}

static void
action_calendar_new_cb (GtkAction *action,
                        ECalShellView *cal_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	ESourceRegistry *registry;
	ECalClientSourceType source_type;
	GtkWidget *config;
	GtkWidget *dialog;
	const gchar *icon_name;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	registry = e_shell_get_registry (shell);
	source_type = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
	config = e_cal_source_config_new (registry, NULL, source_type);

	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), GTK_WINDOW (shell_window));

	icon_name = gtk_action_get_icon_name (action);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("New Calendar"));

	gtk_widget_show (dialog);
}

static void
cal_shell_view_actions_print_or_preview (ECalShellView *cal_shell_view,
					 GtkPrintOperationAction print_action)
{
	ECalShellContent *cal_shell_content;
	ECalendarView *cal_view;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	cal_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	if (E_IS_CAL_LIST_VIEW (cal_view)) {
		ETable *table;

		table = E_CAL_LIST_VIEW (cal_view)->table;
		print_table (table, _("Print"), _("Calendar"), print_action);
	} else {
		EPrintView print_view_type;
		ETable *tasks_table;
		time_t start = 0, end = 0;

		switch (e_cal_shell_content_get_current_view_id (cal_shell_content)) {
			case E_CAL_VIEW_KIND_DAY:
				print_view_type = E_PRINT_VIEW_DAY;
				break;
			case E_CAL_VIEW_KIND_WORKWEEK:
				print_view_type = E_PRINT_VIEW_WORKWEEK;
				break;
			case E_CAL_VIEW_KIND_WEEK:
				print_view_type = E_PRINT_VIEW_WEEK;
				break;
			case E_CAL_VIEW_KIND_MONTH:
				print_view_type = E_PRINT_VIEW_MONTH;
				break;
			case E_CAL_VIEW_KIND_LIST:
				print_view_type = E_PRINT_VIEW_LIST;
				break;
			default:
				g_warn_if_reached ();
				return;
		}

		tasks_table = E_TABLE (e_cal_shell_content_get_task_table (cal_shell_content));

		g_warn_if_fail (e_calendar_view_get_selected_time_range (cal_view, &start, &end));

		print_calendar (cal_view, tasks_table, print_view_type, print_action, start);
	}
}

static void
action_calendar_print_cb (GtkAction *action,
                          ECalShellView *cal_shell_view)
{
	cal_shell_view_actions_print_or_preview (cal_shell_view, GTK_PRINT_OPERATION_ACTION_PRINT);
}

static void
action_calendar_print_preview_cb (GtkAction *action,
                                  ECalShellView *cal_shell_view)
{
	cal_shell_view_actions_print_or_preview (cal_shell_view, GTK_PRINT_OPERATION_ACTION_PREVIEW);
}

static void
action_calendar_properties_cb (GtkAction *action,
                               ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalBaseShellSidebar *cal_shell_sidebar;
	ECalClientSourceType source_type;
	ESource *source;
	ESourceSelector *selector;
	ESourceRegistry *registry;
	GtkWidget *config;
	GtkWidget *dialog;
	const gchar *icon_name;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (cal_shell_sidebar);
	source = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (source != NULL);

	source_type = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
	registry = e_source_selector_get_registry (selector);
	config = e_cal_source_config_new (registry, source, source_type);

	g_object_unref (source);

	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), GTK_WINDOW (shell_window));

	icon_name = gtk_action_get_icon_name (action);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Calendar Properties"));

	gtk_widget_show (dialog);
}

static void
action_calendar_purge_cb (GtkAction *action,
                          ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalShellContent *cal_shell_content;
	GtkSpinButton *spin_button;
	GtkWidget *container;
	GtkWidget *dialog;
	GtkWidget *widget;
	gint days;
	time_t tt;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;

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

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
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

	e_cal_ops_purge_components (e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content)), tt);

 exit:
	gtk_widget_destroy (dialog);
}

static void
action_calendar_refresh_cb (GtkAction *action,
                           ECalShellView *cal_shell_view)
{
	ECalBaseShellSidebar *cal_shell_sidebar;
	ESourceSelector *selector;
	EClient *client = NULL;
	ESource *source;

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (cal_shell_sidebar);

	source = e_source_selector_ref_primary_selection (selector);

	if (source != NULL) {
		client = e_client_selector_ref_cached_client (
			E_CLIENT_SELECTOR (selector), source);
		g_object_unref (source);
	}

	if (client == NULL)
		return;

	g_return_if_fail (e_client_check_refresh_supported (client));

	e_cal_base_shell_view_allow_auth_prompt_and_refresh (E_SHELL_VIEW (cal_shell_view), client);

	g_object_unref (client);
}

static void
action_calendar_rename_cb (GtkAction *action,
                           ECalShellView *cal_shell_view)
{
	ECalBaseShellSidebar *cal_shell_sidebar;
	ESourceSelector *selector;

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (cal_shell_sidebar);

	e_source_selector_edit_primary_selection (selector);
}

static void
action_calendar_search_next_cb (GtkAction *action,
                                ECalShellView *cal_shell_view)
{
	e_cal_shell_view_search_events (cal_shell_view, TRUE);
}

static void
action_calendar_search_prev_cb (GtkAction *action,
                                ECalShellView *cal_shell_view)
{
	e_cal_shell_view_search_events (cal_shell_view, FALSE);
}

static void
action_calendar_search_stop_cb (GtkAction *action,
                                ECalShellView *cal_shell_view)
{
	e_cal_shell_view_search_stop (cal_shell_view);
}

static void
action_calendar_select_all_cb (GtkAction *action,
			       ECalShellView *cal_shell_view)
{
	ECalBaseShellSidebar *cal_shell_sidebar;
	ESourceSelector *selector;

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (cal_shell_sidebar);

	e_source_selector_select_all (selector);
}

static void
action_calendar_select_one_cb (GtkAction *action,
                               ECalShellView *cal_shell_view)
{
	ECalBaseShellSidebar *cal_shell_sidebar;
	ESourceSelector *selector;
	ESource *primary;

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (cal_shell_sidebar);

	primary = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (primary != NULL);

	e_source_selector_select_exclusive (selector, primary);

	g_object_unref (primary);
}

static void
action_calendar_view_cb (GtkRadioAction *action,
                         GtkRadioAction *current,
                         ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	ECalViewKind view_kind;
	const gchar *view_id;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	view_kind = gtk_radio_action_get_current_value (action);

	switch (view_kind) {
		case E_CAL_VIEW_KIND_DAY:
			view_id = "Day_View";
			break;

		case E_CAL_VIEW_KIND_WORKWEEK:
			view_id = "Work_Week_View";
			break;

		case E_CAL_VIEW_KIND_WEEK:
			view_id = "Week_View";
			break;

		case E_CAL_VIEW_KIND_MONTH:
			view_id = "Month_View";
			break;

		case E_CAL_VIEW_KIND_LIST:
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
	ECalendarView *calendar_view;

	/* These are just for readability. */
	gboolean all_day = TRUE;
	gboolean meeting = FALSE;
	gboolean no_past_date = FALSE;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	e_calendar_view_new_appointment_full (
		calendar_view, all_day, meeting, no_past_date);
}

static void
cal_shell_view_transfer_selected (ECalShellView *cal_shell_view,
				  gboolean is_move)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	ESource *source_source = NULL;
	ESource *destination_source = NULL;
	ESourceRegistry *registry;
	GList *selected, *link;
	GHashTable *by_source; /* ESource ~> GSList{icalcomponent} */
	GHashTableIter iter;
	gpointer key, value;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	registry = e_shell_get_registry (e_shell_window_get_shell (shell_window));
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (selected != NULL);

	if (selected->data) {
		ECalendarViewEvent *event = selected->data;

		if (is_comp_data_valid (event) && event->comp_data->client)
			source_source = e_client_get_source (
				E_CLIENT (event->comp_data->client));
	}

	/* Get a destination source from the user. */
	destination_source = e_cal_dialogs_select_source (
		GTK_WINDOW (shell_window), registry,
		E_CAL_CLIENT_SOURCE_TYPE_EVENTS, source_source);
	if (destination_source == NULL) {
		g_list_free (selected);
		return;
	}

	by_source = g_hash_table_new ((GHashFunc) e_source_hash, (GEqualFunc) e_source_equal);

	for (link = selected; link != NULL; link = g_list_next (link)) {
		ECalendarViewEvent *event = link->data;
		ESource *source;
		GSList *icalcomps;

		if (!event || !event->comp_data)
			continue;

		source = e_client_get_source (E_CLIENT (event->comp_data->client));
		if (!source)
			continue;

		icalcomps = g_hash_table_lookup (by_source, source);
		icalcomps = g_slist_prepend (icalcomps, event->comp_data->icalcomp);
		g_hash_table_insert (by_source, source, icalcomps);
	}

	e_cal_ops_transfer_components (shell_view, e_calendar_view_get_model (calendar_view),
		E_CAL_CLIENT_SOURCE_TYPE_EVENTS, by_source, destination_source, is_move);

	g_hash_table_iter_init (&iter, by_source);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		GSList *icalcomps = value;

		g_slist_free (icalcomps);
	}

	g_hash_table_destroy (by_source);
	g_clear_object (&destination_source);
	g_list_free (selected);
}

static void
action_event_copy_cb (GtkAction *action,
                      ECalShellView *cal_shell_view)
{
	cal_shell_view_transfer_selected (cal_shell_view, FALSE);
}

static void
action_event_move_cb (GtkAction *action,
                      ECalShellView *cal_shell_view)
{
	cal_shell_view_transfer_selected (cal_shell_view, TRUE);
}

static void
action_event_delegate_cb (GtkAction *action,
                          ECalShellView *cal_shell_view)
{
	ESourceRegistry *registry;
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	ECalendarViewEvent *event;
	ECalComponent *component;
	ECalClient *client;
	ECalModel *model;
	GList *selected;
	icalcomponent *clone;
	icalproperty *property;
	gboolean found = FALSE;
	gchar *attendee;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_list_length (selected) == 1);

	model = e_calendar_view_get_model (calendar_view);
	registry = e_cal_model_get_registry (model);

	event = selected->data;

	if (!is_comp_data_valid (event))
		return;

	client = event->comp_data->client;
	clone = icalcomponent_new_clone (event->comp_data->icalcomp);

	/* Set the attendee status for the delegate. */

	component = e_cal_component_new ();
	e_cal_component_set_icalcomponent (
		component, icalcomponent_new_clone (clone));

	attendee = itip_get_comp_attendee (
		registry, component, client);
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
		E_COMP_EDITOR_FLAG_WITH_ATTENDEES | E_COMP_EDITOR_FLAG_DELEGATE);

	icalcomponent_free (clone);
	g_list_free (selected);
}

static void
action_event_delete_cb (GtkAction *action,
                        ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	e_selectable_delete_selection (E_SELECTABLE (calendar_view));
}

static void
action_event_delete_occurrence_cb (GtkAction *action,
                                   ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	e_calendar_view_delete_selected_occurrence (calendar_view);
}

static void
action_event_forward_cb (GtkAction *action,
                         ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	ECalendarViewEvent *event;
	ECalComponent *component;
	ECalClient *client;
	icalcomponent *icalcomp;
	GList *selected;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_list_length (selected) == 1);

	event = selected->data;

	if (!is_comp_data_valid (event))
		return;

	client = event->comp_data->client;
	icalcomp = event->comp_data->icalcomp;

	component = e_cal_component_new_from_icalcomponent (icalcomponent_new_clone (icalcomp));
	g_return_if_fail (component != NULL);

	itip_send_component_with_model (e_calendar_view_get_model (calendar_view),
		E_CAL_COMPONENT_METHOD_PUBLISH, component, client,
		NULL, NULL, NULL, TRUE, FALSE, TRUE);

	g_object_unref (component);

	g_list_free (selected);
}

static void
action_event_meeting_new_cb (GtkAction *action,
                             ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;

	/* These are just for readability. */
	gboolean all_day = FALSE;
	gboolean meeting = TRUE;
	gboolean no_past_date = FALSE;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	e_calendar_view_new_appointment_full (
		calendar_view, all_day, meeting, no_past_date);
}

static void
action_event_new_cb (GtkAction *action,
                     ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	e_calendar_view_new_appointment (calendar_view);
}

typedef struct
{
	ECalClient *client;
	gchar *remove_uid;
	gchar *remove_rid;
	icalcomponent *create_icalcomp;
} MakeMovableData;

static void
make_movable_data_free (gpointer ptr)
{
	MakeMovableData *mmd = ptr;

	if (mmd) {
		g_clear_object (&mmd->client);
		g_free (mmd->remove_uid);
		g_free (mmd->remove_rid);
		icalcomponent_free (mmd->create_icalcomp);
		g_free (mmd);
	}
}

static void
make_movable_thread (EAlertSinkThreadJobData *job_data,
		     gpointer user_data,
		     GCancellable *cancellable,
		     GError **error)
{
	MakeMovableData *mmd = user_data;

	g_return_if_fail (mmd != NULL);

	if (!e_cal_client_remove_object_sync (mmd->client, mmd->remove_uid, mmd->remove_rid, E_CAL_OBJ_MOD_THIS, cancellable, error))
		return;

	e_cal_client_create_object_sync (mmd->client, mmd->create_icalcomp, NULL, cancellable, error);
}

static void
action_event_occurrence_movable_cb (GtkAction *action,
                                    ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalModel *model;
	ECalendarView *calendar_view;
	ECalendarViewEvent *event;
	ECalComponent *exception_component;
	ECalComponent *recurring_component;
	ECalComponentDateTime date;
	ECalComponentId *id;
	ECalClient *client;
	icalcomponent *icalcomp;
	icaltimetype itt;
	icaltimezone *timezone;
	GList *selected;
	gchar *uid;
	EActivity *activity;
	MakeMovableData *mmd;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

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
	cal_comp_set_dtend_with_oldzone (client, exception_component, &date);
	e_cal_component_commit_sequence (exception_component);

	mmd = g_new0 (MakeMovableData, 1);
	mmd->client = g_object_ref (client);
	mmd->remove_uid = g_strdup (id->uid);
	mmd->remove_rid = g_strdup (id->rid);
	mmd->create_icalcomp = icalcomponent_new_clone (e_cal_component_get_icalcomponent (exception_component));

	activity = e_shell_view_submit_thread_job (E_SHELL_VIEW (cal_shell_view),
		_("Making an occurrence movable"), "calendar:failed-make-movable",
		NULL, make_movable_thread, mmd, make_movable_data_free);

	g_clear_object (&activity);
	e_cal_component_free_id (id);
	g_object_unref (recurring_component);
	g_object_unref (exception_component);
	g_list_free (selected);
}

static void
action_event_open_cb (GtkAction *action,
                      ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalendarView *view;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	e_calendar_view_open_event (view);
}

static void
action_event_print_cb (GtkAction *action,
                       ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	ECalendarViewEvent *event;
	ECalComponent *component;
	ECalModel *model;
	ECalClient *client;
	icalcomponent *icalcomp;
	GList *selected;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);
	model = e_calendar_view_get_model (calendar_view);

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
		component, client,
		e_cal_model_get_timezone (model),
		e_cal_model_get_use_24_hour_format (model),
		GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);

	g_object_unref (component);

	g_list_free (selected);
}

static void
cal_shell_view_actions_reply (ECalShellView *cal_shell_view,
			      gboolean reply_all)
{
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	ECalendarViewEvent *event;
	ECalComponent *component;
	ECalClient *client;
	ESourceRegistry *registry;
	icalcomponent *icalcomp;
	GList *selected;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	registry = e_shell_get_registry (e_shell_window_get_shell (e_shell_view_get_shell_window (E_SHELL_VIEW (cal_shell_view))));
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

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
		registry, E_CAL_COMPONENT_METHOD_REPLY,
		component, client, reply_all, NULL, NULL);

	g_object_unref (component);

	g_list_free (selected);
}

static void
action_event_reply_cb (GtkAction *action,
                       ECalShellView *cal_shell_view)
{
	cal_shell_view_actions_reply (cal_shell_view, FALSE);
}

static void
action_event_reply_all_cb (GtkAction *action,
                           ECalShellView *cal_shell_view)
{
	cal_shell_view_actions_reply (cal_shell_view, TRUE);
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
	ECalendarView *calendar_view;
	ECalendarViewEvent *event;
	ECalClient *client;
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
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

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

	string = e_cal_client_get_component_as_string (client, icalcomp);
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
edit_event_as (ECalShellView *cal_shell_view,
               gboolean as_meeting)
{
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	ECalendarViewEvent *event;
	ECalClient *client;
	icalcomponent *icalcomp;
	GList *selected;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

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
		calendar_view, client, icalcomp, as_meeting ?
		EDIT_EVENT_FORCE_MEETING : EDIT_EVENT_FORCE_APPOINTMENT);

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

static GtkActionEntry calendar_entries[] = {

	{ "calendar-copy",
	  "edit-copy",
	  N_("_Copy..."),
	  "<Control>c",
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_copy_cb) },

	{ "calendar-delete",
	  "edit-delete",
	  N_("D_elete Calendar"),
	  NULL,
	  N_("Delete the selected calendar"),
	  G_CALLBACK (action_calendar_delete_cb) },

	{ "calendar-go-back",
	  "go-previous",
	  N_("Previous"),
	  NULL,
	  N_("Go Back"),
	  G_CALLBACK (action_calendar_go_back_cb) },

	{ "calendar-go-forward",
	  "go-next",
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
	  "go-jump",
	  N_("Select _Date"),
	  "<Control>g",
	  N_("Select a specific date"),
	  G_CALLBACK (action_calendar_jump_to_cb) },

	{ "calendar-manage-groups",
	  NULL,
	  N_("_Manage Calendar groups..."),
	  NULL,
	  N_("Manage Calendar groups order and visibility"),
	  G_CALLBACK (action_calendar_manage_groups_cb) },

	{ "calendar-new",
	  "x-office-calendar",
	  N_("_New Calendar"),
	  NULL,
	  N_("Create a new calendar"),
	  G_CALLBACK (action_calendar_new_cb) },

	{ "calendar-properties",
	  "document-properties",
	  N_("_Properties"),
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
	  "view-refresh",
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

	{ "calendar-search-next",
	  "go-next",
	  N_("Find _Next"),
	  "<Control><Shift>n",
	  N_("Find next occurrence of the current search string"),
	  G_CALLBACK (action_calendar_search_next_cb) },

	{ "calendar-search-prev",
	  "go-previous",
	  N_("Find _Previous"),
	  "<Control><Shift>p",
	  N_("Find previous occurrence of the current search string"),
	  G_CALLBACK (action_calendar_search_prev_cb) },

	{ "calendar-search-stop",
	  "process-stop",
	  N_("Stop _Running Search"),
	  NULL,
	  N_("Stop currently running search"),
	  G_CALLBACK (action_calendar_search_stop_cb) },

	{ "calendar-select-all",
	  "stock_check-filled",
	  N_("Sho_w All Calendars"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_select_all_cb) },

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
	  "edit-delete",
	  N_("_Delete Appointment"),
	  "<Control>d",
	  N_("Delete selected appointments"),
	  G_CALLBACK (action_event_delete_cb) },

	{ "event-delete-occurrence",
	  "edit-delete",
	  N_("Delete This _Occurrence"),
	  NULL,
	  N_("Delete this occurrence"),
	  G_CALLBACK (action_event_delete_occurrence_cb) },

	{ "event-delete-occurrence-all",
	  "edit-delete",
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
	  "stock_people",
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
	  "document-open",
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
	  "window-close",
	  N_("Quit"),
	  "<Control>w",
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

	{ "calendar-popup-manage-groups",
	  N_("_Manage groups..."),
	  "calendar-manage-groups" },

	{ "calendar-popup-properties",
	  NULL,
	  "calendar-properties" },

	{ "calendar-popup-refresh",
	  NULL,
	  "calendar-refresh" },

	{ "calendar-popup-rename",
	  NULL,
	  "calendar-rename" },

	{ "calendar-popup-select-all",
	  NULL,
	  "calendar-select-all" },

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
	  E_CAL_VIEW_KIND_DAY },

	{ "calendar-view-list",
	  "view-calendar-list",
	  N_("List"),
	  NULL,
	  N_("Show as list"),
	  E_CAL_VIEW_KIND_LIST },

	{ "calendar-view-month",
	  "view-calendar-month",
	  N_("Month"),
	  NULL,
	  N_("Show one month"),
	  E_CAL_VIEW_KIND_MONTH },

	{ "calendar-view-week",
	  "view-calendar-week",
	  N_("Week"),
	  NULL,
	  N_("Show one week"),
	  E_CAL_VIEW_KIND_WEEK },

	{ "calendar-view-workweek",
	  "view-calendar-workweek",
	  N_("Work Week"),
	  NULL,
	  N_("Show one work week"),
	  E_CAL_VIEW_KIND_WORKWEEK }
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
	  N_("Next 7 Days’ Appointments"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  CALENDAR_FILTER_NEXT_7_DAYS_APPOINTMENTS },

	{ "calendar-filter-occurs-less-than-5-times",
	  NULL,
	  N_("Occurs Less Than 5 Times"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  CALENDAR_FILTER_OCCURS_LESS_THAN_5_TIMES },

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
	  "document-print",
	  N_("Print..."),
	  "<Control>p",
	  N_("Print this calendar"),
	  G_CALLBACK (action_calendar_print_cb) },

	{ "calendar-print-preview",
	  "document-print-preview",
	  N_("Pre_view..."),
	  NULL,
	  N_("Preview the calendar to be printed"),
	  G_CALLBACK (action_calendar_print_preview_cb) },

	{ "event-print",
	  "document-print",
	  N_("Print..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_event_print_cb) }
};

static EPopupActionEntry lockdown_printing_popup_entries[] = {

	{ "event-popup-print",
	  NULL,
	  "event-print" }
};

static GtkActionEntry lockdown_save_to_disk_entries[] = {

	{ "event-save-as",
	  "document-save-as",
	  N_("_Save as iCalendar..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_event_save_as_cb) },
};

static EPopupActionEntry lockdown_save_to_disk_popup_entries[] = {

	{ "event-popup-save-as",
	  NULL,
	  "event-save-as" },
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

	/* Lockdown Save-to-Disk Actions */
	action_group = ACTION_GROUP (LOCKDOWN_SAVE_TO_DISK);
	gtk_action_group_add_actions (
		action_group, lockdown_save_to_disk_entries,
		G_N_ELEMENTS (lockdown_save_to_disk_entries), cal_shell_view);
	e_action_group_add_popup_actions (
		action_group, lockdown_save_to_disk_popup_entries,
		G_N_ELEMENTS (lockdown_save_to_disk_popup_entries));

	/* Fine tuning. */

	action = ACTION (CALENDAR_GO_TODAY);
	gtk_action_set_short_label (action, _("Today"));

	action = ACTION (CALENDAR_JUMP_TO);
	gtk_action_set_short_label (action, _("Go To"));

	action = ACTION (CALENDAR_VIEW_DAY);
	gtk_action_set_is_important (action, TRUE);

	action = ACTION (CALENDAR_VIEW_LIST);
	gtk_action_set_is_important (action, TRUE);

	action = ACTION (CALENDAR_VIEW_MONTH);
	gtk_action_set_is_important (action, TRUE);

	action = ACTION (CALENDAR_VIEW_WEEK);
	gtk_action_set_is_important (action, TRUE);

	action = ACTION (CALENDAR_VIEW_WORKWEEK);
	gtk_action_set_is_important (action, TRUE);

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

	list = e_util_dup_searchable_categories ();
	for (iter = list, ii = 0; iter != NULL; iter = iter->next, ii++) {
		const gchar *category_name = iter->data;
		gchar *filename;
		GtkAction *action;
		gchar *action_name;

		action_name = g_strdup_printf (
			"calendar-filter-category-%d", ii);
		radio_action = gtk_radio_action_new (
			action_name, category_name, NULL, NULL, ii);
		g_free (action_name);

		/* Convert the category icon file to a themed icon name. */
		filename = e_categories_dup_icon_file_for (category_name);
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

		g_free (filename);

		gtk_radio_action_set_group (radio_action, group);
		group = gtk_radio_action_get_group (radio_action);

		/* The action group takes ownership of the action. */
		action = GTK_ACTION (radio_action);
		gtk_action_group_add_action (action_group, action);
		g_object_unref (radio_action);
	}
	g_list_free_full (list, g_free);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	searchbar = e_cal_shell_content_get_searchbar (cal_shell_content);
	if (searchbar) {
		combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);

		e_shell_view_block_execute_search (shell_view);

		/* Use any action in the group; doesn't matter which. */
		e_action_combo_box_set_action (combo_box, radio_action);

		ii = CALENDAR_FILTER_UNMATCHED;
		e_action_combo_box_add_separator_after (combo_box, ii);

		ii = CALENDAR_FILTER_OCCURS_LESS_THAN_5_TIMES;
		e_action_combo_box_add_separator_after (combo_box, ii);

		e_shell_view_unblock_execute_search (shell_view);
	}
}
