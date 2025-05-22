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

static void
action_calendar_copy_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EShellView *shell_view = user_data;

	e_cal_base_shell_view_copy_calendar (shell_view);
}

static void
action_calendar_delete_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
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
action_calendar_go_back_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	e_cal_shell_content_move_view_range (cal_shell_view->priv->cal_shell_content, E_CALENDAR_VIEW_MOVE_PREVIOUS, 0);
}

static void
action_calendar_go_forward_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	e_cal_shell_content_move_view_range (cal_shell_view->priv->cal_shell_content, E_CALENDAR_VIEW_MOVE_NEXT, 0);
}

static void
action_calendar_go_today_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	e_cal_shell_content_move_view_range (cal_shell_view->priv->cal_shell_content, E_CALENDAR_VIEW_MOVE_TO_TODAY, 0);
}

static void
action_calendar_jump_to_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
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
action_calendar_manage_groups_cb (EUIAction *action,
				  GVariant *parameter,
				  gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	EShellView *shell_view;
	ESourceSelector *selector;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	selector = e_cal_base_shell_sidebar_get_selector (cal_shell_view->priv->cal_shell_sidebar);

	if (e_source_selector_manage_groups (selector) &&
	    e_source_selector_save_groups_setup (selector, e_shell_view_get_state_key_file (shell_view)))
		e_shell_view_set_state_dirty (shell_view);
}

static void
action_calendar_new_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
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

	e_cal_base_shell_view_preselect_source_config (shell_view, config);

	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), GTK_WINDOW (shell_window));

	icon_name = e_ui_action_get_icon_name (action);
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

		table = e_cal_list_view_get_table (E_CAL_LIST_VIEW (cal_view));
		print_table (table, _("Print"), _("Calendar"), print_action);
	} else {
		EPrintView print_view_type;
		ETable *tasks_table;
		time_t start = 0, end = 0;

		switch (e_cal_shell_content_get_current_view_id (cal_shell_content)) {
			case E_CAL_VIEW_KIND_DAY:
			case E_CAL_VIEW_KIND_YEAR:
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
action_calendar_print_cb (EUIAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	cal_shell_view_actions_print_or_preview (cal_shell_view, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
action_calendar_print_preview_cb (EUIAction *action,
				  GVariant *parameter,
				  gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	cal_shell_view_actions_print_or_preview (cal_shell_view, GTK_PRINT_OPERATION_ACTION_PREVIEW);
}

static void
action_calendar_properties_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
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

	icon_name = e_ui_action_get_icon_name (action);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Calendar Properties"));

	gtk_widget_show (dialog);
}

static void
action_calendar_purge_cb (EUIAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
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
action_calendar_refresh_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
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
action_calendar_refresh_backend_cb (EUIAction *action,
				    GVariant *parameter,
				    gpointer user_data)
{
	EShellView *shell_view = user_data;
	ESource *source;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (shell_view));

	source = e_cal_base_shell_view_get_clicked_source (shell_view);

	if (source && e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION))
		e_cal_base_shell_view_refresh_backend (shell_view, source);
}

static void
action_calendar_rename_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalBaseShellSidebar *cal_shell_sidebar;
	ESourceSelector *selector;

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (cal_shell_sidebar);

	e_source_selector_edit_primary_selection (selector);
}

static void
action_calendar_search_next_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	e_cal_shell_view_search_events (cal_shell_view, TRUE);
}

static void
action_calendar_search_prev_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	e_cal_shell_view_search_events (cal_shell_view, FALSE);
}

static void
action_calendar_search_stop_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	e_cal_shell_view_search_stop (cal_shell_view);
}

static void
action_calendar_select_all_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalBaseShellSidebar *cal_shell_sidebar;
	ESourceSelector *selector;

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (cal_shell_sidebar);

	e_source_selector_select_all (selector);
}

static void
action_calendar_select_one_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
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
action_calendar_view_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	e_ui_action_set_state (action, parameter);

	e_cal_shell_view_set_view_id_from_view_kind (cal_shell_view, g_variant_get_int32 (parameter));
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
	GSList *selected, *link;
	GHashTable *by_source; /* ESource ~> GSList{ICalComponent} */
	GHashTableIter iter;
	gpointer key, value;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	registry = e_shell_get_registry (e_shell_window_get_shell (shell_window));
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (selected != NULL);

	if (selected->data && is_move) {
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
		g_slist_free_full (selected, e_calendar_view_selection_data_free);
		return;
	}

	by_source = g_hash_table_new ((GHashFunc) e_source_hash, (GEqualFunc) e_source_equal);

	for (link = selected; link; link = g_slist_next (link)) {
		ECalendarViewSelectionData *sel_data = link->data;
		ESource *source;
		GSList *icomps;

		source = e_client_get_source (E_CLIENT (sel_data->client));
		if (!source)
			continue;

		icomps = g_hash_table_lookup (by_source, source);
		icomps = g_slist_prepend (icomps, sel_data->icalcomp);
		g_hash_table_insert (by_source, source, icomps);
	}

	e_cal_ops_transfer_components (shell_view, e_calendar_view_get_model (calendar_view),
		E_CAL_CLIENT_SOURCE_TYPE_EVENTS, by_source, destination_source, is_move);

	g_hash_table_iter_init (&iter, by_source);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		GSList *icomps = value;

		g_slist_free (icomps);
	}

	g_hash_table_destroy (by_source);
	g_clear_object (&destination_source);
	g_slist_free_full (selected, e_calendar_view_selection_data_free);
}

static void
action_event_copy_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	cal_shell_view_transfer_selected (cal_shell_view, FALSE);
}

static void
action_event_move_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	cal_shell_view_transfer_selected (cal_shell_view, TRUE);
}

static void
action_event_delegate_cb (EUIAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ESourceRegistry *registry;
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	ECalendarViewSelectionData *sel_data;
	ECalComponent *component;
	ECalClient *client;
	ECalModel *model;
	GSList *selected;
	ICalComponent *clone;
	ICalProperty *prop;
	gboolean found = FALSE;
	gchar *attendee;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_slist_length (selected) == 1);

	model = e_calendar_view_get_model (calendar_view);
	registry = e_cal_model_get_registry (model);

	sel_data = selected->data;

	client = sel_data->client;
	clone = i_cal_component_clone (sel_data->icalcomp);

	/* Set the attendee status for the delegate. */

	component = e_cal_component_new_from_icalcomponent (i_cal_component_clone (clone));

	attendee = itip_get_comp_attendee (registry, component, client);

	for (prop = i_cal_component_get_first_property (clone, I_CAL_ATTENDEE_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (clone, I_CAL_ATTENDEE_PROPERTY)) {
		const gchar *candidate;

		candidate = e_cal_util_get_property_email (prop);

		if (e_cal_util_email_addresses_equal (candidate, attendee)) {
			ICalParameter *param;

			param = i_cal_parameter_new_role (I_CAL_ROLE_NONPARTICIPANT);
			i_cal_property_set_parameter (prop, param);
			g_clear_object (&param);

			param = i_cal_parameter_new_partstat (I_CAL_PARTSTAT_DELEGATED);
			i_cal_property_set_parameter (prop, param);
			g_clear_object (&param);

			found = TRUE;
			break;
		}
	}

	g_clear_object (&prop);

	/* If the attendee is not already in the component, add it. */
	if (!found) {
		ICalParameter *param;
		gchar *address;

		address = g_strdup_printf ("mailto:%s", attendee);

		prop = i_cal_property_new_attendee (address);

		param = i_cal_parameter_new_role (I_CAL_ROLE_NONPARTICIPANT);
		i_cal_property_take_parameter (prop, param);

		param = i_cal_parameter_new_cutype (I_CAL_CUTYPE_INDIVIDUAL);
		i_cal_property_take_parameter (prop, param);

		param = i_cal_parameter_new_rsvp (I_CAL_RSVP_TRUE);
		i_cal_property_take_parameter (prop, param);

		i_cal_component_take_property (clone, prop);
		g_free (address);
	}

	g_free (attendee);
	g_object_unref (component);

	e_calendar_view_open_event_with_flags (
		calendar_view, sel_data->client, clone,
		E_COMP_EDITOR_FLAG_WITH_ATTENDEES | E_COMP_EDITOR_FLAG_DELEGATE);

	g_object_unref (clone);
	g_slist_free_full (selected, e_calendar_view_selection_data_free);
}

static void
action_event_delete_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	e_selectable_delete_selection (E_SELECTABLE (calendar_view));
}

static void
action_event_delete_occurrence_cb (EUIAction *action,
				   GVariant *parameter,
				   gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	e_calendar_view_delete_selected_occurrence (calendar_view, E_CAL_OBJ_MOD_THIS);
}

static void
action_event_delete_occurrence_this_and_future_cb (EUIAction *action,
						   GVariant *parameter,
						   gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	e_calendar_view_delete_selected_occurrence (calendar_view, E_CAL_OBJ_MOD_THIS_AND_FUTURE);
}

static void
action_event_forward_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	ECalendarViewSelectionData *sel_data;
	ECalComponent *component;
	ECalClient *client;
	ICalComponent *icomp;
	GSList *selected;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_slist_length (selected) == 1);

	sel_data = selected->data;
	client = sel_data->client;
	icomp = sel_data->icalcomp;

	component = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));
	g_return_if_fail (component != NULL);

	itip_send_component_with_model (e_cal_model_get_data_model (e_calendar_view_get_model (calendar_view)),
		I_CAL_METHOD_PUBLISH, component, client,
		NULL, NULL, NULL, E_ITIP_SEND_COMPONENT_FLAG_STRIP_ALARMS | E_ITIP_SEND_COMPONENT_FLAG_ENSURE_MASTER_OBJECT | E_ITIP_SEND_COMPONENT_FLAG_AS_ATTACHMENT);

	g_object_unref (component);
	g_slist_free_full (selected, e_calendar_view_selection_data_free);
}

static void
action_event_new_cb (EUIAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	const gchar *action_name;
	gboolean is_all_day, is_meeting;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	action_name = g_action_get_name (G_ACTION (action));
	is_all_day = g_strcmp0 (action_name, "event-all-day-new") == 0;
	is_meeting = g_strcmp0 (action_name, "event-meeting-new") == 0;

	e_calendar_view_new_appointment (calendar_view,
		(is_all_day ? E_NEW_APPOINTMENT_FLAG_ALL_DAY : 0) |
		(is_meeting ? E_NEW_APPOINTMENT_FLAG_MEETING : 0) |
		(e_shell_view_is_active (E_SHELL_VIEW (cal_shell_view)) ? 0 : E_NEW_APPOINTMENT_FLAG_FORCE_CURRENT_TIME));
}

static void
action_event_rsvp_response_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	ECalendarViewSelectionData *sel_data;
	ECalClient *client;
	ECalComponent *comp;
	ECalModel *model;
	ICalParameterPartstat partstat = I_CAL_PARTSTAT_NONE;
	ICalComponent *clone;
	GSList *selected;
	const gchar *action_name;
	gboolean ensure_master_object;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);
	action_name = g_action_get_name (G_ACTION (action));

	if (g_strcmp0 (action_name, "event-rsvp-accept") == 0 ||
	    g_strcmp0 (action_name, "event-rsvp-accept-1") == 0)
		partstat = I_CAL_PARTSTAT_ACCEPTED;
	else if (g_strcmp0 (action_name, "event-rsvp-decline") == 0 ||
		 g_strcmp0 (action_name, "event-rsvp-decline-1") == 0)
		partstat = I_CAL_PARTSTAT_DECLINED;
	else if (g_strcmp0 (action_name, "event-rsvp-tentative") == 0 ||
		 g_strcmp0 (action_name, "event-rsvp-tentative-1") == 0)
		partstat = I_CAL_PARTSTAT_TENTATIVE;
	else {
		g_warning ("%s: Do not know what to do with '%s'", G_STRFUNC, action_name);
	}

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_slist_length (selected) == 1);

	sel_data = selected->data;

	client = sel_data->client;
	model = e_calendar_view_get_model (calendar_view);

	clone = i_cal_component_clone (sel_data->icalcomp);
	comp = e_cal_component_new_from_icalcomponent (clone);

	if (!comp) {
		g_slist_free_full (selected, e_calendar_view_selection_data_free);
		g_warn_if_reached ();
		return;
	}

	ensure_master_object = (e_cal_util_component_is_instance (clone) ||
				e_cal_util_component_has_recurrences (clone)) &&
				!g_str_has_suffix (action_name, "-1");

	itip_send_component_with_model (e_cal_model_get_data_model (model), I_CAL_METHOD_REPLY,
		comp, client, NULL, NULL, NULL,
		E_ITIP_SEND_COMPONENT_FLAG_STRIP_ALARMS |
		(ensure_master_object ? E_ITIP_SEND_COMPONENT_FLAG_ENSURE_MASTER_OBJECT : 0) |
		(partstat == I_CAL_PARTSTAT_ACCEPTED ? E_ITIP_SEND_COMPONENT_FLAG_SAVE_RESPONSE_ACCEPTED : 0) |
		(partstat == I_CAL_PARTSTAT_DECLINED ? E_ITIP_SEND_COMPONENT_FLAG_SAVE_RESPONSE_DECLINED : 0) |
		(partstat == I_CAL_PARTSTAT_TENTATIVE ? E_ITIP_SEND_COMPONENT_FLAG_SAVE_RESPONSE_TENTATIVE : 0));

	g_slist_free_full (selected, e_calendar_view_selection_data_free);
	g_clear_object (&comp);
}

typedef struct {
	ECalClient *client;
	gchar *remove_uid;
	gchar *remove_rid;
	ICalComponent *create_icomp;
} MakeMovableData;

static void
make_movable_data_free (gpointer ptr)
{
	MakeMovableData *mmd = ptr;

	if (mmd) {
		g_clear_object (&mmd->client);
		g_free (mmd->remove_uid);
		g_free (mmd->remove_rid);
		g_clear_object (&mmd->create_icomp);
		g_slice_free (MakeMovableData, mmd);
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

	if (!e_cal_client_remove_object_sync (mmd->client, mmd->remove_uid, mmd->remove_rid, E_CAL_OBJ_MOD_THIS, E_CAL_OPERATION_FLAG_NONE, cancellable, error))
		return;

	e_cal_client_create_object_sync (mmd->client, mmd->create_icomp, E_CAL_OPERATION_FLAG_NONE, NULL, cancellable, error);
}

static void
action_event_occurrence_movable_cb (EUIAction *action,
				    GVariant *parameter,
				    gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ECalModel *model;
	ECalendarView *calendar_view;
	ECalendarViewSelectionData *sel_data;
	ECalComponent *exception_component;
	ECalComponent *recurring_component;
	ECalComponentDateTime *date;
	ECalComponentId *id;
	ECalClient *client;
	ICalComponent *icomp;
	ICalTimezone *timezone;
	ICalTime *itt_start = NULL, *itt_end = NULL;
	GSList *selected;
	time_t instance_start, instance_end;
	gchar *uid;
	EActivity *activity;
	MakeMovableData *mmd;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	model = e_calendar_view_get_model (calendar_view);
	timezone = e_cal_model_get_timezone (model);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_slist_length (selected) == 1);

	sel_data = selected->data;
	client = sel_data->client;
	icomp = sel_data->icalcomp;

	cal_comp_get_instance_times (client, icomp, timezone, &itt_start, &itt_end, NULL);

	instance_start = itt_start ? i_cal_time_as_timet_with_zone (itt_start,
		i_cal_time_get_timezone (itt_start)) : 0;
	instance_end = itt_end ? i_cal_time_as_timet_with_zone (itt_end,
		i_cal_time_get_timezone (itt_end)) : 0;

	g_clear_object (&itt_start);
	g_clear_object (&itt_end);

	/* For the recurring object, we add an exception
	 * to get rid of the instance. */

	recurring_component = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));
	id = e_cal_component_get_id (recurring_component);

	/* For the unrecurred instance, we duplicate the original object,
	 * create a new UID for it, get rid of the recurrence rules, and
	 * set the start and end times to the instance times. */

	exception_component = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));

	uid = e_util_generate_uid ();
	e_cal_component_set_uid (exception_component, uid);
	g_free (uid);

	e_cal_component_set_recurid (exception_component, NULL);
	e_cal_component_set_rdates (exception_component, NULL);
	e_cal_component_set_rrules (exception_component, NULL);
	e_cal_component_set_exdates (exception_component, NULL);
	e_cal_component_set_exrules (exception_component, NULL);

	date = e_cal_component_datetime_new_take (i_cal_time_new_from_timet_with_zone (instance_start, FALSE, timezone),
		timezone ? g_strdup (i_cal_timezone_get_tzid (timezone)) : NULL);
	cal_comp_set_dtstart_with_oldzone (client, exception_component, date);
	e_cal_component_datetime_take_value (date, i_cal_time_new_from_timet_with_zone (instance_end, FALSE, timezone));
	cal_comp_set_dtend_with_oldzone (client, exception_component, date);
	e_cal_component_datetime_free (date);

	e_cal_component_commit_sequence (exception_component);

	mmd = g_slice_new0 (MakeMovableData);
	mmd->client = g_object_ref (client);
	mmd->remove_uid = g_strdup (e_cal_component_id_get_uid (id));
	mmd->remove_rid = g_strdup (e_cal_component_id_get_rid (id));
	mmd->create_icomp = i_cal_component_clone (e_cal_component_get_icalcomponent (exception_component));

	activity = e_shell_view_submit_thread_job (E_SHELL_VIEW (cal_shell_view),
		_("Making an occurrence movable"), "calendar:failed-make-movable",
		NULL, make_movable_thread, mmd, make_movable_data_free);

	g_clear_object (&activity);
	e_cal_component_id_free (id);
	g_object_unref (recurring_component);
	g_object_unref (exception_component);
	g_slist_free_full (selected, e_calendar_view_selection_data_free);
}

static void
action_event_open_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ECalendarView *view;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	e_calendar_view_open_event (view);
}

static void
action_event_edit_as_new_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	ECalendarViewSelectionData *sel_data;
	GSList *selected;
	ICalComponent *clone;
	guint32 flags = E_COMP_EDITOR_FLAG_IS_NEW;
	gchar *uid;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_slist_length (selected) == 1);

	sel_data = selected->data;

	if (e_cal_util_component_is_instance (sel_data->icalcomp)) {
		g_slist_free_full (selected, e_calendar_view_selection_data_free);
		return;
	}

	clone = i_cal_component_clone (sel_data->icalcomp);

	uid = e_util_generate_uid ();
	i_cal_component_set_uid (clone, uid);
	g_free (uid);

	if (e_cal_util_component_has_organizer (clone)) {
		ESourceRegistry *registry;
		ICalProperty *org_prop;

		registry = e_shell_get_registry (e_shell_window_get_shell (e_shell_view_get_shell_window (E_SHELL_VIEW (cal_shell_view))));

		org_prop = i_cal_component_get_first_property (clone, I_CAL_ORGANIZER_PROPERTY);
		if (org_prop) {
			const gchar *org_email = e_cal_util_get_property_email (org_prop);

			if (!org_email || !itip_address_is_user (registry, org_email)) {
				if (org_email && *org_email) {
					ICalProperty *attendee_prop;
					gboolean is_attendee = FALSE;

					for (attendee_prop = i_cal_component_get_first_property (clone, I_CAL_ATTENDEE_PROPERTY);
					     attendee_prop && !is_attendee;
					     g_object_unref (attendee_prop), attendee_prop = i_cal_component_get_next_property (clone, I_CAL_ATTENDEE_PROPERTY)) {
						const gchar *attendee_email = e_cal_util_get_property_email (attendee_prop);

						is_attendee = e_cal_util_email_addresses_equal (org_email, attendee_email);
					}

					g_clear_object (&attendee_prop);

					if (!is_attendee) {
						ICalParameter *param;

						attendee_prop = i_cal_property_new_attendee (i_cal_property_get_organizer (org_prop));

						for (param = i_cal_property_get_first_parameter (org_prop, I_CAL_ANY_PARAMETER);
						     param;
						     param = i_cal_property_get_next_parameter (org_prop, I_CAL_ANY_PARAMETER)) {
							i_cal_property_take_parameter (attendee_prop, param);
						}

						i_cal_component_take_property (clone, attendee_prop);
					}
				}
			}

			i_cal_component_remove_property (clone, org_prop);

			g_clear_object (&org_prop);

			flags |= E_COMP_EDITOR_FLAG_WITH_ATTENDEES;
		}
	}

	e_calendar_view_open_event_with_flags (calendar_view, sel_data->client, clone, flags);

	g_clear_object (&clone);
	g_slist_free_full (selected, e_calendar_view_selection_data_free);
}

static void
action_event_print_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	ECalendarViewSelectionData *sel_data;
	ECalComponent *component;
	ECalModel *model;
	ECalClient *client;
	ICalComponent *icomp;
	GSList *selected;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);
	model = e_calendar_view_get_model (calendar_view);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_slist_length (selected) == 1);

	sel_data = selected->data;
	client = sel_data->client;
	icomp = sel_data->icalcomp;

	component = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));

	print_comp (
		component, client,
		e_cal_model_get_timezone (model),
		e_cal_model_get_use_24_hour_format (model),
		GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);

	g_object_unref (component);
	g_slist_free_full (selected, e_calendar_view_selection_data_free);
}

static void
cal_shell_view_actions_reply (ECalShellView *cal_shell_view,
			      gboolean reply_all)
{
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	ECalendarViewSelectionData *sel_data;
	ECalComponent *component;
	ECalClient *client;
	ESourceRegistry *registry;
	ICalComponent *icomp;
	GSList *selected;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	registry = e_shell_get_registry (e_shell_window_get_shell (e_shell_view_get_shell_window (E_SHELL_VIEW (cal_shell_view))));
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_slist_length (selected) == 1);

	sel_data = selected->data;
	client = sel_data->client;
	icomp = sel_data->icalcomp;

	component = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));

	reply_to_calendar_comp (
		registry, I_CAL_METHOD_REPLY,
		component, client, reply_all, NULL, NULL);

	g_object_unref (component);
	g_slist_free_full (selected, e_calendar_view_selection_data_free);
}

static void
action_event_reply_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	cal_shell_view_actions_reply (cal_shell_view, FALSE);
}

static void
action_event_reply_all_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	cal_shell_view_actions_reply (cal_shell_view, TRUE);
}

static void
action_event_save_as_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	ECalendarViewSelectionData *sel_data;
	ECalClient *client;
	ICalComponent *icomp;
	EActivity *activity;
	GSList *selected;
	GFile *file;
	gchar *string = NULL;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_slist_length (selected) == 1);

	sel_data = selected->data;
	client = sel_data->client;
	icomp = sel_data->icalcomp;

	/* Translators: Default filename part saving an event to a file when
	 * no summary is filed, the '.ics' extension is concatenated to it. */
	string = comp_util_suggest_filename (icomp, _("event"));
	file = e_shell_run_save_dialog (
		shell, _("Save as iCalendar"), string,
		"*.ics:text/calendar", NULL, NULL);
	g_free (string);
	if (file == NULL)
		goto exit;

	string = e_cal_client_get_component_as_string (client, icomp);
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
	g_clear_object (&file);
	g_slist_free_full (selected, e_calendar_view_selection_data_free);
}

static void
edit_event_as (ECalShellView *cal_shell_view,
               gboolean as_meeting)
{
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	ECalendarViewSelectionData *sel_data;
	ECalClient *client;
	ICalComponent *icomp;
	GSList *selected;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	selected = e_calendar_view_get_selected_events (calendar_view);
	g_return_if_fail (g_slist_length (selected) == 1);

	sel_data = selected->data;
	client = sel_data->client;
	icomp = sel_data->icalcomp;

	if (!as_meeting && icomp) {
		/* remove organizer and all attendees */
		/* do it on a copy, as user can cancel changes */
		icomp = i_cal_component_clone (icomp);

		e_cal_util_component_remove_property_by_kind (icomp, I_CAL_ATTENDEE_PROPERTY, TRUE);
		e_cal_util_component_remove_property_by_kind (icomp, I_CAL_ORGANIZER_PROPERTY, TRUE);
	}

	e_calendar_view_edit_appointment (
		calendar_view, client, icomp, as_meeting ?
		EDIT_EVENT_FORCE_MEETING : EDIT_EVENT_FORCE_APPOINTMENT);

	if (!as_meeting && icomp) {
		g_object_unref (icomp);
	}

	g_slist_free_full (selected, e_calendar_view_selection_data_free);
}

static void
action_event_schedule_cb (EUIAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	edit_event_as (cal_shell_view, TRUE);
}

static void
quit_calendar_cb (EUIAction *action,
		  GVariant *parameter,
		  gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
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
action_event_schedule_appointment_cb (EUIAction *action,
				      GVariant *parameter,
				      gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	edit_event_as (cal_shell_view, FALSE);
}

static void
action_calendar_show_tag_vpane_cb (EUIAction *action,
				   GVariant *parameter,
				   gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));

	e_cal_shell_content_set_show_tag_vpane (cal_shell_view->priv->cal_shell_content, !e_ui_action_get_active (action));
}

void
e_cal_shell_view_actions_init (ECalShellView *self)
{
	static const EUIActionEntry calendar_entries[] = {

		{ "calendar-copy",
		  "edit-copy",
		  N_("_Copy…"),
		  NULL,
		  NULL,
		  action_calendar_copy_cb, NULL, NULL, NULL },

		{ "calendar-delete",
		  "edit-delete",
		  N_("D_elete Calendar…"),
		  NULL,
		  N_("Delete the selected calendar"),
		  action_calendar_delete_cb, NULL, NULL, NULL },

		{ "calendar-go-back",
		  "go-previous",
		  N_("Previous"),
		  "<Control>Page_Up",
		  N_("Go Back"),
		  action_calendar_go_back_cb, NULL, NULL, NULL },

		{ "calendar-go-forward",
		  "go-next",
		  N_("Next"),
		  "<Control>Page_Down",
		  N_("Go Forward"),
		  action_calendar_go_forward_cb, NULL, NULL, NULL },

		{ "calendar-go-today",
		  "go-today",
		  N_("Select _Today"),
		  "<Control>t",
		  N_("Select today"),
		  action_calendar_go_today_cb, NULL, NULL, NULL },

		{ "calendar-jump-to",
		  "go-jump",
		  N_("Select _Date"),
		  "<Control>g",
		  N_("Select a specific date"),
		  action_calendar_jump_to_cb, NULL, NULL, NULL },

		{ "calendar-manage-groups",
		  NULL,
		  N_("_Manage Calendar groups…"),
		  NULL,
		  N_("Manage Calendar groups order and visibility"),
		  action_calendar_manage_groups_cb, NULL, NULL, NULL },

		{ "calendar-manage-groups-popup",
		  NULL,
		  N_("_Manage groups…"),
		  NULL,
		  N_("Manage Calendar groups order and visibility"),
		  action_calendar_manage_groups_cb, NULL, NULL, NULL },

		{ "calendar-new",
		  "x-office-calendar",
		  N_("_New Calendar"),
		  NULL,
		  N_("Create a new calendar"),
		  action_calendar_new_cb, NULL, NULL, NULL },

		{ "calendar-properties",
		  "document-properties",
		  N_("_Properties"),
		  NULL,
		  NULL,
		  action_calendar_properties_cb, NULL, NULL, NULL },

		{ "calendar-purge",
		  NULL,
		  N_("Purg_e"),
		  "<Control>e",
		  N_("Purge old appointments and meetings"),
		  action_calendar_purge_cb, NULL, NULL, NULL },

		{ "calendar-refresh",
		  "view-refresh",
		  N_("Re_fresh"),
		  NULL,
		  N_("Refresh the selected calendar"),
		  action_calendar_refresh_cb, NULL, NULL, NULL },

		{ "calendar-refresh-backend",
		  "view-refresh",
		  N_("Re_fresh list of account calendars"),
		  NULL,
		  NULL,
		  action_calendar_refresh_backend_cb, NULL, NULL, NULL },

		{ "calendar-rename",
		  NULL,
		  N_("_Rename…"),
		  NULL,
		  N_("Rename the selected calendar"),
		  action_calendar_rename_cb, NULL, NULL, NULL },

		{ "calendar-search-next",
		  "go-next",
		  N_("Find _Next"),
		  "<Control><Shift>n",
		  N_("Find next occurrence of the current search string"),
		  action_calendar_search_next_cb, NULL, NULL, NULL },

		{ "calendar-search-prev",
		  "go-previous",
		  N_("Find _Previous"),
		  "<Control><Shift>p",
		  N_("Find previous occurrence of the current search string"),
		  action_calendar_search_prev_cb, NULL, NULL, NULL },

		{ "calendar-search-stop",
		  "process-stop",
		  N_("Stop _Running Search"),
		  NULL,
		  N_("Stop currently running search"),
		  action_calendar_search_stop_cb, NULL, NULL, NULL },

		{ "calendar-select-all",
		  "stock_check-filled-symbolic",
		  N_("Sho_w All Calendars"),
		  NULL,
		  NULL,
		  action_calendar_select_all_cb, NULL, NULL, NULL },

		{ "calendar-select-one",
		  "stock_check-filled-symbolic",
		  N_("Show _Only This Calendar"),
		  NULL,
		  NULL,
		  action_calendar_select_one_cb, NULL, NULL, NULL },

		{ "event-copy",
		  NULL,
		  N_("Cop_y to Calendar…"),
		  NULL,
		  NULL,
		  action_event_copy_cb, NULL, NULL, NULL },

		{ "event-delegate",
		  NULL,
		  N_("_Delegate Meeting…"),
		  NULL,
		  NULL,
		  action_event_delegate_cb, NULL, NULL, NULL },

		{ "event-delete",
		  "edit-delete",
		  N_("_Delete…"),
		  "<Control>d",
		  N_("Delete selected events"),
		  action_event_delete_cb, NULL, NULL, NULL },

		{ "event-delete-occurrence",
		  "edit-delete",
		  N_("Delete This _Occurrence…"),
		  NULL,
		  N_("Delete this occurrence"),
		  action_event_delete_occurrence_cb, NULL, NULL, NULL },

		{ "event-delete-occurrence-this-and-future",
		  "edit-delete",
		  N_("Delete This and F_uture Occurrences…"),
		  NULL,
		  N_("Delete this and any future occurrences"),
		  action_event_delete_occurrence_this_and_future_cb, NULL, NULL, NULL },

		{ "event-delete-occurrence-all",
		  "edit-delete",
		  N_("Delete All Occ_urrences…"),
		  NULL,
		  N_("Delete all occurrences"),
		  action_event_delete_cb, NULL, NULL, NULL },

		{ "event-edit-as-new",
		  NULL,
		  N_("Edit as Ne_w…"),
		  NULL,
		  N_("Edit the selected event as new"),
		  action_event_edit_as_new_cb, NULL, NULL, NULL },

		{ "event-all-day-new",
		  "stock_new-24h-appointment",
		  N_("New All Day _Event…"),
		  NULL,
		  N_("Create a new all day event"),
		  action_event_new_cb, NULL, NULL, NULL },

		{ "event-forward",
		  "mail-forward",
		  N_("_Forward as iCalendar…"),
		  NULL,
		  NULL,
		  action_event_forward_cb, NULL, NULL, NULL },

		{ "event-meeting-new",
		  "stock_people",
		  N_("New _Meeting…"),
		  NULL,
		  N_("Create a new meeting"),
		  action_event_new_cb, NULL, NULL, NULL },

		{ "event-rsvp-accept",
		  NULL,
		  N_("_Accept"),
		  NULL,
		  N_("Accept meeting request"),
		  action_event_rsvp_response_cb, NULL, NULL, NULL },

		{ "event-rsvp-accept-1",
		  NULL,
		  N_("A_ccept this instance"),
		  NULL,
		  N_("Accept meeting request for selected instance only"),
		  action_event_rsvp_response_cb, NULL, NULL, NULL },

		{ "event-rsvp-decline",
		  NULL,
		  N_("_Decline"),
		  NULL,
		  N_("Decline meeting request"),
		  action_event_rsvp_response_cb, NULL, NULL, NULL },

		{ "event-rsvp-decline-1",
		  NULL,
		  N_("D_ecline this instance"),
		  NULL,
		  N_("Decline meeting request for selected instance only"),
		  action_event_rsvp_response_cb, NULL, NULL, NULL },

		{ "event-rsvp-tentative",
		  NULL,
		  N_("_Tentatively accept"),
		  NULL,
		  N_("Tentatively accept meeting request"),
		  action_event_rsvp_response_cb, NULL, NULL, NULL },

		{ "event-rsvp-tentative-1",
		  NULL,
		  N_("Te_ntatively accept this instance"),
		  NULL,
		  N_("Tentatively accept meeting request for selected instance only"),
		  action_event_rsvp_response_cb, NULL, NULL, NULL },

		{ "event-move",
		  NULL,
		  N_("Mo_ve to Calendar…"),
		  NULL,
		  NULL,
		  action_event_move_cb, NULL, NULL, NULL },

		{ "event-new",
		  "appointment-new",
		  N_("New _Appointment…"),
		  NULL,
		  N_("Create a new appointment"),
		  action_event_new_cb, NULL, NULL, NULL },

		{ "event-occurrence-movable",
		  NULL,
		  N_("Make this Occurrence _Movable"),
		  NULL,
		  NULL,
		  action_event_occurrence_movable_cb, NULL, NULL, NULL },

		{ "event-open",
		  "document-open",
		  N_("_Open…"),
		  "<Control>o",
		  N_("Edit the selected event"),
		  action_event_open_cb, NULL, NULL, NULL },

		{ "event-reply",
		  "mail-reply-sender",
		  N_("_Reply"),
		  NULL,
		  NULL,
		  action_event_reply_cb, NULL, NULL, NULL },

		{ "event-reply-all",
		  "mail-reply-all",
		  N_("Reply to _All"),
		  NULL,
		  NULL,
		  action_event_reply_all_cb, NULL, NULL, NULL },

		{ "event-schedule",
		  NULL,
		  N_("_Schedule Meeting…"),
		  NULL,
		  N_("Converts an appointment to a meeting"),
		  action_event_schedule_cb, NULL, NULL, NULL },

		{ "event-schedule-appointment",
		  NULL,
		  N_("Conv_ert to Appointment…"),
		  NULL,
		  N_("Converts a meeting to an appointment"),
		  action_event_schedule_appointment_cb, NULL, NULL, NULL },

		{ "quit-calendar",
		  "window-close",
		  N_("Quit"),
		  "<Control>w",
		  NULL,
		  quit_calendar_cb, NULL, NULL, NULL },

		{ "calendar-preview",
		  NULL,
		  N_("Event _Preview"),
		  NULL,
		  N_("Show event preview pane"),
		  NULL, NULL, "true", NULL },  /* Handled by property bindings */

		{ "calendar-show-tag-vpane",
		  NULL,
		  N_("Show T_asks and Memos pane"),
		  NULL,
		  N_("Show Tasks and Memos pane"),
		  action_calendar_show_tag_vpane_cb, NULL, "true", NULL },

		/*** Menus ***/

		{ "calendar-actions-menu", NULL, N_("_Actions"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "calendar-preview-menu", NULL, N_("_Preview"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "event-rsvp-submenu", NULL, N_("Send _RSVP"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "ECalShellView::navigation-buttons", NULL, N_("Navigation buttons"), NULL, NULL, NULL, NULL, NULL, NULL }
	};

	static const EUIActionEnumEntry calendar_view_entries[] = {

		{ "calendar-view-day",
		  "view-calendar-day",
		  N_("Day"),
		  "<Control>y",
		  N_("Show one day"),
		  action_calendar_view_cb, E_CAL_VIEW_KIND_DAY },

		{ "calendar-view-list",
		  "view-calendar-list",
		  N_("List"),
		  "<Control>l",
		  N_("Show as list"),
		  action_calendar_view_cb, E_CAL_VIEW_KIND_LIST },

		{ "calendar-view-month",
		  "view-calendar-month",
		  N_("Month"),
		  "<Control>m",
		  N_("Show one month"),
		  action_calendar_view_cb, E_CAL_VIEW_KIND_MONTH },

		{ "calendar-view-week",
		  "view-calendar-week",
		  N_("Week"),
		  "<Control>k",
		  N_("Show one week"),
		  action_calendar_view_cb, E_CAL_VIEW_KIND_WEEK },

		{ "calendar-view-workweek",
		  "view-calendar-workweek",
		  N_("Work Week"),
		  "<Control>j",
		  N_("Show one work week"),
		  action_calendar_view_cb, E_CAL_VIEW_KIND_WORKWEEK },

		{ "calendar-view-year",
		  "view-calendar-year",
		  N_("Year"),
		  NULL,
		  N_("Show as year"),
		  action_calendar_view_cb, E_CAL_VIEW_KIND_YEAR }
	};

	static const EUIActionEnumEntry calendar_preview_entries[] = {

		{ "calendar-preview-horizontal",
		  NULL,
		  N_("_Classic View"),
		  NULL,
		  N_("Show event preview below the calendar"),
		  NULL, 0 },

		{ "calendar-preview-vertical",
		  NULL,
		  N_("_Vertical View"),
		  NULL,
		  N_("Show event preview alongside the calendar"),
		  NULL, 1 }
	};

	static const EUIActionEnumEntry calendar_search_entries[] = {

		{ "calendar-search-advanced-hidden",
		  NULL,
		  N_("Advanced Search"),
		  NULL,
		  NULL,
		  NULL, CALENDAR_SEARCH_ADVANCED },

		{ "calendar-search-any-field-contains",
		  NULL,
		  N_("Any field contains"),
		  NULL,
		  NULL,
		  NULL, CALENDAR_SEARCH_ANY_FIELD_CONTAINS },

		{ "calendar-search-description-contains",
		  NULL,
		  N_("Description contains"),
		  NULL,
		  NULL,
		  NULL, CALENDAR_SEARCH_DESCRIPTION_CONTAINS },

		{ "calendar-search-summary-contains",
		  NULL,
		  N_("Summary contains"),
		  NULL,
		  NULL,
		  NULL, CALENDAR_SEARCH_SUMMARY_CONTAINS }
	};

	static const EUIActionEntry lockdown_printing_entries[] = {

		{ "calendar-print",
		  "document-print",
		  N_("Print…"),
		  "<Control>p",
		  N_("Print this calendar"),
		  action_calendar_print_cb, NULL, NULL, NULL },

		{ "calendar-print-preview",
		  "document-print-preview",
		  N_("Pre_view…"),
		  NULL,
		  N_("Preview the calendar to be printed"),
		  action_calendar_print_preview_cb, NULL, NULL, NULL },

		{ "event-print",
		  "document-print",
		  N_("Print…"),
		  NULL,
		  NULL,
		  action_event_print_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry lockdown_save_to_disk_entries[] = {

		{ "event-save-as",
		  "document-save-as",
		  N_("_Save as iCalendar…"),
		  NULL,
		  NULL,
		  action_event_save_as_cb, NULL, NULL, NULL },
	};

	EShellView *shell_view;
	EUIManager *ui_manager;

	shell_view = E_SHELL_VIEW (self);
	ui_manager = e_shell_view_get_ui_manager (shell_view);

	/* Calendar Actions */
	e_ui_manager_add_actions (ui_manager, "calendar", NULL,
		calendar_entries, G_N_ELEMENTS (calendar_entries), self);
	e_ui_manager_add_actions_enum (ui_manager, "calendar", NULL,
		calendar_view_entries, G_N_ELEMENTS (calendar_view_entries), self);
	e_ui_manager_add_actions_enum (ui_manager, "calendar", NULL,
		calendar_preview_entries, G_N_ELEMENTS (calendar_preview_entries), self);
	e_ui_manager_add_actions_enum (ui_manager, "calendar", NULL,
		calendar_search_entries, G_N_ELEMENTS (calendar_search_entries), self);

	/* Lockdown Printing Actions */
	e_ui_manager_add_actions (ui_manager, "lockdown-printing", NULL,
		lockdown_printing_entries, G_N_ELEMENTS (lockdown_printing_entries), self);

	/* Lockdown Save-to-Disk Actions */
	e_ui_manager_add_actions (ui_manager, "lockdown-save-to-disk", NULL,
		lockdown_save_to_disk_entries, G_N_ELEMENTS (lockdown_save_to_disk_entries), self);

	e_binding_bind_property (
		ACTION (CALENDAR_PREVIEW), "active",
		ACTION (CALENDAR_PREVIEW_VERTICAL), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (CALENDAR_PREVIEW), "active",
		ACTION (CALENDAR_PREVIEW_HORIZONTAL), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_ui_manager_set_enum_entries_usable_for_kinds (ui_manager, 0,
		calendar_search_entries, G_N_ELEMENTS (calendar_search_entries));
}

void
e_cal_shell_view_update_search_filter (ECalShellView *cal_shell_view)
{
	static const EUIActionEnumEntry calendar_filter_entries[] = {

		{ "calendar-filter-active-appointments",
		  NULL,
		  N_("Active Appointments"),
		  NULL,
		  NULL,
		  NULL, CALENDAR_FILTER_ACTIVE_APPOINTMENTS },

		{ "calendar-filter-any-category",
		  NULL,
		  N_("Any Category"),
		  NULL,
		  NULL,
		  NULL, CALENDAR_FILTER_ANY_CATEGORY },

		{ "calendar-filter-next-7-days-appointments",
		  NULL,
		  N_("Next 7 Days’ Appointments"),
		  NULL,
		  NULL,
		  NULL, CALENDAR_FILTER_NEXT_7_DAYS_APPOINTMENTS },

		{ "calendar-filter-occurs-less-than-5-times",
		  NULL,
		  N_("Occurs Less Than 5 Times"),
		  NULL,
		  NULL,
		  NULL, CALENDAR_FILTER_OCCURS_LESS_THAN_5_TIMES },

		{ "calendar-filter-unmatched",
		  NULL,
		  N_("Without Category"),
		  NULL,
		  N_("Show events with no category set"),
		  NULL, CALENDAR_FILTER_UNMATCHED }
	};

	ECalShellContent *cal_shell_content;
	EShellView *shell_view;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	EUIActionGroup *action_group;
	EUIAction *action;
	GList *list, *iter;
	GPtrArray *radio_group;
	gint ii;

	shell_view = E_SHELL_VIEW (cal_shell_view);

	action_group = e_ui_manager_get_action_group (e_shell_view_get_ui_manager (shell_view), "calendar-filter");
	e_ui_action_group_remove_all (action_group);

	/* Add the standard filter actions.  No callback is needed
	 * because changes in the EActionComboBox are detected and
	 * handled by EShellSearchbar. */
	e_ui_manager_add_actions_enum (e_shell_view_get_ui_manager (shell_view),
		e_ui_action_group_get_name (action_group), NULL,
		calendar_filter_entries, G_N_ELEMENTS (calendar_filter_entries), NULL);

	radio_group = g_ptr_array_new ();

	for (ii = 0; ii < G_N_ELEMENTS (calendar_filter_entries); ii++) {
		action = e_ui_action_group_get_action (action_group, calendar_filter_entries[ii].name);
		e_ui_action_set_usable_for_kinds (action, 0);
		e_ui_action_set_radio_group (action, radio_group);
	}

	/* Build the category actions. */

	list = e_util_dup_searchable_categories ();
	for (iter = list, ii = 0; iter != NULL; iter = iter->next, ii++) {
		const gchar *category_name = iter->data;
		gchar *filename;
		gchar action_name[128];

		g_warn_if_fail (g_snprintf (action_name, sizeof (action_name), "calendar-filter-category-%d", ii) < sizeof (action_name));

		action = e_ui_action_new (e_ui_action_group_get_name (action_group), action_name, NULL);
		e_ui_action_set_label (action, category_name);
		e_ui_action_set_state (action, g_variant_new_int32 (ii));
		e_ui_action_set_usable_for_kinds (action, 0);
		e_ui_action_set_radio_group (action, radio_group);

		/* Convert the category icon file to a themed icon name. */
		filename = e_categories_dup_icon_file_for (category_name);
		if (filename != NULL && *filename != '\0') {
			gchar *basename;
			gchar *cp;

			basename = g_path_get_basename (filename);

			/* Lose the file extension. */
			if ((cp = strrchr (basename, '.')) != NULL)
				*cp = '\0';

			e_ui_action_set_icon_name (action, basename);

			g_free (basename);
		}

		g_free (filename);

		e_ui_action_group_add (action_group, action);

		g_object_unref (action);
	}
	g_list_free_full (list, g_free);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	searchbar = e_cal_shell_content_get_searchbar (cal_shell_content);
	if (searchbar) {
		combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);

		e_shell_view_block_execute_search (shell_view);

		/* Use any action in the group; doesn't matter which. */
		e_action_combo_box_set_action (combo_box, action);

		ii = CALENDAR_FILTER_UNMATCHED;
		e_action_combo_box_add_separator_after (combo_box, ii);

		ii = CALENDAR_FILTER_OCCURS_LESS_THAN_5_TIMES;
		e_action_combo_box_add_separator_after (combo_box, ii);

		e_shell_view_unblock_execute_search (shell_view);
	}

	g_ptr_array_unref (radio_group);
}
