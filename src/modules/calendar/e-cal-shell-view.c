/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "e-cal-shell-view-private.h"

#include "calendar/gui/ea-calendar.h"
#include "calendar/gui/calendar-view.h"
#include "calendar/gui/e-year-view.h"

#include "e-cal-base-shell-sidebar.h"
#include "e-cal-shell-content.h"
#include "e-cal-shell-view.h"

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ECalShellView, e_cal_shell_view, E_TYPE_CAL_BASE_SHELL_VIEW, 0,
	G_ADD_PRIVATE_DYNAMIC (ECalShellView))

static void
cal_shell_view_action_button_clicked_cb (GtkButton *button,
					 gpointer user_data)
{
	GAction *action = user_data;

	g_action_activate (action, NULL);
}

static void
cal_shell_view_add_action_button (GtkBox *box,
				  EUIAction *action,
				  EUIManager *ui_manager)
{
	GtkWidget *button, *icon;

	button = gtk_button_new ();
	icon = gtk_image_new_from_icon_name (e_ui_action_get_icon_name (action), GTK_ICON_SIZE_MENU);
	gtk_image_set_pixel_size (GTK_IMAGE (icon), 16);
	gtk_button_set_image (GTK_BUTTON (button), icon);
	gtk_box_pack_start (box, button, FALSE, FALSE, 0);
	gtk_widget_show (button);

	e_binding_bind_property (
		action, "visible",
		button, "visible",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		action, "sensitive",
		button, "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		action, "tooltip",
		button, "tooltip-text",
		G_BINDING_SYNC_CREATE);

	g_signal_connect_object (
		button, "clicked",
		G_CALLBACK (cal_shell_view_action_button_clicked_cb), action, 0);
}

static void
cal_shell_view_prepare_for_quit_cb (EShell *shell,
                                    EActivity *activity,
                                    ECalShellView *cal_shell_view)
{
	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));

	/* Stop running searches, if any; the activity tight
	 * on the search prevents application from quitting. */
	e_cal_shell_view_search_stop (cal_shell_view);
}

static void
cal_shell_view_execute_search (EShellView *shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalBaseShellSidebar *cal_shell_sidebar;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	ECalendar *calendar;
	ECalDataModel *data_model;
	EUIAction *action;
	ICalTimezone *timezone;
	ICalTime *current_time;
	GVariant *state;
	time_t start_range;
	time_t end_range;
	time_t now_time;
	gboolean range_search;
	gchar *query;
	gchar *temp;
	gint value;

	e_cal_shell_view_search_stop (E_CAL_SHELL_VIEW (shell_view));

	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	cal_shell_content = E_CAL_SHELL_CONTENT (shell_content);
	cal_shell_sidebar = E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar);

	searchbar = e_cal_shell_content_get_searchbar (cal_shell_content);

	data_model = e_cal_base_shell_content_get_data_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));
	timezone = e_cal_data_model_get_timezone (data_model);
	current_time = i_cal_time_new_current_with_zone (timezone);
	now_time = time_day_begin (i_cal_time_as_timet (current_time));
	g_clear_object (&current_time);

	action = ACTION (CALENDAR_SEARCH_ANY_FIELD_CONTAINS);
	state = g_action_get_state (G_ACTION (action));
	value = g_variant_get_int32 (state);
	g_clear_pointer (&state, g_variant_unref);

	if (value == CALENDAR_SEARCH_ADVANCED) {
		query = e_shell_view_get_search_query (shell_view);

		if (!query)
			query = g_strdup ("");
	} else {
		const gchar *format;
		const gchar *text;
		GString *string;

		text = e_shell_searchbar_get_search_text (searchbar);

		if (text == NULL || *text == '\0') {
			text = "";
			value = CALENDAR_SEARCH_SUMMARY_CONTAINS;
		}

		switch (value) {
			default:
				text = "";
				/* fall through */

			case CALENDAR_SEARCH_SUMMARY_CONTAINS:
				format = "(contains? \"summary\" %s)";
				break;

			case CALENDAR_SEARCH_DESCRIPTION_CONTAINS:
				format = "(contains? \"description\" %s)";
				break;

			case CALENDAR_SEARCH_ANY_FIELD_CONTAINS:
				format = "(contains? \"any\" %s)";
				break;
		}

		/* Build the query. */
		string = g_string_new ("");
		e_sexp_encode_string (string, text);
		query = g_strdup_printf (format, string->str);
		g_string_free (string, TRUE);
	}

	range_search = FALSE;
	start_range = end_range = 0;

	/* Apply selected filter. */
	combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);
	value = e_action_combo_box_get_current_value (combo_box);
	switch (value) {
		case CALENDAR_FILTER_ANY_CATEGORY:
			break;

		case CALENDAR_FILTER_UNMATCHED:
			temp = g_strdup_printf (
				"(and (has-categories? #f) %s)", query);
			g_free (query);
			query = temp;
			break;

		case CALENDAR_FILTER_ACTIVE_APPOINTMENTS:
			/* Show a year's worth of appointments. */
			start_range = now_time;
			end_range = time_day_end (time_add_day (start_range, 365));
			range_search = TRUE;
			break;

		case CALENDAR_FILTER_NEXT_7_DAYS_APPOINTMENTS:
			start_range = now_time;
			end_range = time_day_end (time_add_day (start_range, 7));
			range_search = TRUE;
			break;

		case CALENDAR_FILTER_OCCURS_LESS_THAN_5_TIMES:
			temp = g_strdup_printf (
				"(and %s (< (occurrences-count?) 5))", query);
			g_free (query);
			query = temp;
			break;

		default:
		{
			GList *categories;
			const gchar *category_name;

			categories = e_util_dup_searchable_categories ();
			category_name = g_list_nth_data (categories, value);

			temp = g_strdup_printf (
				"(and (has-categories? \"%s\") %s)",
				category_name, query);
			g_free (query);
			query = temp;

			g_list_free_full (categories, g_free);
			break;
		}
	}

	calendar = e_cal_base_shell_sidebar_get_date_navigator (cal_shell_sidebar);

	if (range_search) {
		/* Switch to list view and hide the date navigator. */
		action = ACTION (CALENDAR_VIEW_LIST);
		e_ui_action_set_active (action, TRUE);
		gtk_widget_hide (GTK_WIDGET (calendar));
	} else {
		ECalViewKind view_kind;

		view_kind = e_cal_shell_content_get_current_view_id (cal_shell_content);

		/* Ensure the date navigator is visible. */
		gtk_widget_set_visible (GTK_WIDGET (calendar), view_kind != E_CAL_VIEW_KIND_LIST && view_kind != E_CAL_VIEW_KIND_YEAR);
		e_cal_shell_content_get_current_range (cal_shell_content, &start_range, &end_range);
		end_range = time_day_end (end_range) - 1;
	}

	/* Submit the query. */
	e_cal_shell_content_update_filters (cal_shell_content, query, start_range, end_range);

	g_free (query);

	/* Update actions so Find Prev/Next/Stop
	 * buttons will be sensitive as expected. */
	e_shell_view_update_actions (shell_view);
}

static void
cal_shell_view_update_actions (EShellView *shell_view)
{
	ECalShellView *self = E_CAL_SHELL_VIEW (shell_view);
	ECalShellContent *cal_shell_content;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShell *shell;
	ESource *source;
	ESourceRegistry *registry;
	ECalendarView *cal_view;
	EMemoTable *memo_table;
	ETaskTable *task_table;
	ECalDataModel *data_model;
	EUIAction *action;
	gchar *data_filter;
	gboolean is_searching;
	gboolean is_list_view;
	gboolean sensitive;
	guint32 state;

	/* Be descriptive. */
	gboolean any_events_selected;
	gboolean has_mail_identity = FALSE;
	gboolean has_primary_source;
	gboolean primary_source_is_writable;
	gboolean primary_source_is_removable;
	gboolean primary_source_is_remote_deletable;
	gboolean primary_source_in_collection;
	gboolean multiple_events_selected;
	gboolean selection_is_editable;
	gboolean selection_is_instance;
	gboolean selection_is_meeting;
	gboolean selection_is_recurring;
	gboolean selection_is_attendee;
	gboolean selection_can_delegate;
	gboolean single_event_selected;
	gboolean refresh_supported;
	gboolean all_sources_selected;
	gboolean clicked_source_is_primary;
	gboolean clicked_source_is_collection;
	gboolean this_and_future_supported;

	/* Chain up to parent's update_actions() method. */
	E_SHELL_VIEW_CLASS (e_cal_shell_view_parent_class)->update_actions (shell_view);

	shell = e_shell_window_get_shell (e_shell_view_get_shell_window (shell_view));

	registry = e_shell_get_registry (shell);
	source = e_source_registry_ref_default_mail_identity (registry);
	has_mail_identity = (source != NULL);
	if (source != NULL) {
		has_mail_identity = TRUE;
		g_object_unref (source);
	}

	cal_shell_content = self->priv->cal_shell_content;
	cal_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);
	is_list_view = E_IS_CAL_LIST_VIEW (cal_view);
	if (is_list_view)
		data_model = e_cal_shell_content_get_list_view_data_model (cal_shell_content);
	else
		data_model = e_cal_base_shell_content_get_data_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));
	data_filter = e_cal_data_model_dup_filter (data_model);
	is_searching = data_filter && *data_filter &&
		g_strcmp0 (data_filter, "#t") != 0 &&
		g_strcmp0 (data_filter, "(contains? \"summary\"  \"\")") != 0;
	g_free (data_filter);

	shell_content = e_shell_view_get_shell_content (shell_view);
	state = e_shell_content_check_state (shell_content);

	single_event_selected =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_SINGLE);
	multiple_events_selected =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_MULTIPLE);
	selection_is_editable =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_EDITABLE);
	selection_is_instance =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_INSTANCE);
	selection_is_meeting =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_MEETING);
	selection_is_attendee =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_ATTENDEE);
	selection_is_recurring =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_RECURRING);
	selection_can_delegate =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_CAN_DELEGATE);
	this_and_future_supported =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_THIS_AND_FUTURE_SUPPORTED);

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	state = e_shell_sidebar_check_state (shell_sidebar);

	has_primary_source =
		(state & E_CAL_BASE_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE);
	primary_source_is_writable =
		(state & E_CAL_BASE_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_WRITABLE);
	primary_source_is_removable =
		(state & E_CAL_BASE_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOVABLE);
	primary_source_is_remote_deletable =
		(state & E_CAL_BASE_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOTE_DELETABLE);
	primary_source_in_collection =
		(state & E_CAL_BASE_SHELL_SIDEBAR_PRIMARY_SOURCE_IN_COLLECTION);
	refresh_supported =
		(state & E_CAL_BASE_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH);
	all_sources_selected =
		(state & E_CAL_BASE_SHELL_SIDEBAR_ALL_SOURCES_SELECTED) != 0;
	clicked_source_is_primary =
		(state & E_CAL_BASE_SHELL_SIDEBAR_CLICKED_SOURCE_IS_PRIMARY) != 0;
	clicked_source_is_collection =
		(state & E_CAL_BASE_SHELL_SIDEBAR_CLICKED_SOURCE_IS_COLLECTION) != 0;

	any_events_selected = (single_event_selected || multiple_events_selected);

	action = ACTION (CALENDAR_SELECT_ALL);
	sensitive = clicked_source_is_primary && !all_sources_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_SELECT_ONE);
	sensitive = clicked_source_is_primary;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_COPY);
	sensitive = has_primary_source;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_DELETE);
	sensitive =
		primary_source_is_removable ||
		primary_source_is_remote_deletable;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_PRINT);
	sensitive = TRUE;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_PRINT_PREVIEW);
	sensitive = TRUE;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_PROPERTIES);
	sensitive = clicked_source_is_primary && primary_source_is_writable;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_REFRESH);
	sensitive = clicked_source_is_primary && refresh_supported;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_REFRESH_BACKEND);
	sensitive = clicked_source_is_collection;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_RENAME);
	sensitive = clicked_source_is_primary &&
		primary_source_is_writable &&
		!primary_source_in_collection;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_SEARCH_PREV);
	e_ui_action_set_sensitive (action, is_searching && !is_list_view);

	action = ACTION (CALENDAR_SEARCH_NEXT);
	e_ui_action_set_sensitive (action, is_searching && !is_list_view);

	action = ACTION (CALENDAR_SEARCH_STOP);
	sensitive = is_searching && self->priv->searching_activity != NULL;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_DELEGATE);
	sensitive =
		single_event_selected &&
		selection_is_editable &&
		selection_can_delegate &&
		selection_is_meeting;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_DELETE);
	sensitive =
		any_events_selected &&
		selection_is_editable &&
		!selection_is_recurring;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_DELETE_OCCURRENCE);
	sensitive =
		any_events_selected &&
		selection_is_editable &&
		selection_is_recurring;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_DELETE_OCCURRENCE_THIS_AND_FUTURE);
	sensitive =
		single_event_selected &&
		selection_is_editable &&
		selection_is_recurring &&
		this_and_future_supported;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_DELETE_OCCURRENCE_ALL);
	sensitive =
		any_events_selected &&
		selection_is_editable &&
		selection_is_recurring;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_FORWARD);
	sensitive = single_event_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_OCCURRENCE_MOVABLE);
	sensitive =
		single_event_selected &&
		selection_is_editable &&
		selection_is_recurring &&
		selection_is_instance;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_OPEN);
	sensitive = single_event_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_EDIT_AS_NEW);
	sensitive = single_event_selected &&
		!selection_is_instance;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_PRINT);
	sensitive = single_event_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_SAVE_AS);
	sensitive = single_event_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_SCHEDULE);
	sensitive =
		single_event_selected &&
		selection_is_editable &&
		!selection_is_meeting;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_SCHEDULE_APPOINTMENT);
	sensitive =
		single_event_selected &&
		selection_is_editable &&
		selection_is_meeting;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_REPLY);
	sensitive = single_event_selected && selection_is_meeting;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_REPLY_ALL);
	sensitive = single_event_selected && selection_is_meeting;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_MEETING_NEW);
	e_ui_action_set_visible (action, has_mail_identity);

	action = ACTION (EVENT_RSVP_SUBMENU);
	e_ui_action_set_visible (action, selection_is_attendee);

	sensitive = selection_is_instance || selection_is_recurring;
	e_ui_action_set_visible (ACTION (EVENT_RSVP_ACCEPT_1), sensitive);
	e_ui_action_set_visible (ACTION (EVENT_RSVP_DECLINE_1), sensitive);
	e_ui_action_set_visible (ACTION (EVENT_RSVP_TENTATIVE_1), sensitive);

	e_ui_action_set_sensitive (ACTION (CALENDAR_GO_BACK), !is_list_view);
	e_ui_action_set_sensitive (ACTION (CALENDAR_GO_FORWARD), !is_list_view);
	e_ui_action_set_sensitive (ACTION (CALENDAR_GO_TODAY), !is_list_view);
	e_ui_action_set_sensitive (ACTION (CALENDAR_JUMP_TO), !is_list_view);

	if ((cal_view && e_calendar_view_is_editing (cal_view)) ||
	    e_table_is_editing (E_TABLE (memo_table)) ||
	    e_table_is_editing (E_TABLE (task_table))) {
		EFocusTracker *focus_tracker;

		/* disable all clipboard actions, if any of the views is in editing mode */
		focus_tracker = e_shell_window_get_focus_tracker (e_shell_view_get_shell_window (shell_view));

		action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
		if (action)
			e_ui_action_set_sensitive (action, FALSE);

		action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
		if (action)
			e_ui_action_set_sensitive (action, FALSE);

		action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
		if (action)
			e_ui_action_set_sensitive (action, FALSE);

		action = e_focus_tracker_get_delete_selection_action (focus_tracker);
		if (action)
			e_ui_action_set_sensitive (action, FALSE);
	}

	if (E_IS_YEAR_VIEW (cal_view))
		e_year_view_update_actions (E_YEAR_VIEW (cal_view));
}

static gboolean
e_cal_shell_view_ui_manager_create_item_cb (EUIManager *manager,
					    EUIElement *elem,
					    EUIAction *action,
					    EUIElementKind for_kind,
					    GObject **out_item,
					    gpointer user_data)
{
	ECalShellView *self = user_data;
	const gchar *name;

	g_return_val_if_fail (E_IS_CAL_SHELL_VIEW (self), FALSE);

	name = g_action_get_name (G_ACTION (action));

	if (!g_str_has_prefix (name, "ECalShellView::"))
		return FALSE;

	#define is_action(_nm) (g_strcmp0 (name, (_nm)) == 0)

	if (for_kind == E_UI_ELEMENT_KIND_MENU) {
		g_warning ("%s: Unhandled menu action '%s'", G_STRFUNC, name);
	} else if (for_kind == E_UI_ELEMENT_KIND_TOOLBAR) {
		g_warning ("%s: Unhandled toolbar action '%s'", G_STRFUNC, name);
	} else if (for_kind == E_UI_ELEMENT_KIND_HEADERBAR) {
		if (is_action ("ECalShellView::navigation-buttons")) {
			EShellView *shell_view;
			GtkWidget *widget;
			EUIAction *btn_action;

			shell_view = E_SHELL_VIEW (self);

			btn_action = ACTION (CALENDAR_GO_BACK);
			widget = e_header_bar_button_new (NULL, btn_action, manager);

			btn_action = ACTION (CALENDAR_GO_TODAY);
			e_header_bar_button_add_action (E_HEADER_BAR_BUTTON (widget), NULL, btn_action);

			btn_action = ACTION (CALENDAR_GO_FORWARD);
			e_header_bar_button_add_action (E_HEADER_BAR_BUTTON (widget), NULL, btn_action);

			gtk_widget_show (widget);

			*out_item = G_OBJECT (widget);
		} else {
			g_warning ("%s: Unhandled headerbar action '%s'", G_STRFUNC, name);
		}
	} else {
		g_warning ("%s: Unhandled element kind '%d' for action '%s'", G_STRFUNC, (gint) for_kind, name);
	}

	#undef is_action

	return TRUE;
}

static void
cal_shell_view_init_ui_data (EShellView *shell_view)
{
	ECalShellView *cal_shell_view;
	EUIAction *action;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (shell_view));

	cal_shell_view = E_CAL_SHELL_VIEW (shell_view);

	g_signal_connect_object (e_shell_view_get_ui_manager (shell_view), "create-item",
		G_CALLBACK (e_cal_shell_view_ui_manager_create_item_cb), cal_shell_view, 0);

	e_cal_shell_view_actions_init (cal_shell_view);
	e_cal_shell_view_memopad_actions_init (cal_shell_view);
	e_cal_shell_view_taskpad_actions_init (cal_shell_view);

	action = e_ui_manager_get_action (e_shell_view_get_ui_manager (shell_view), "ECalShellView::navigation-buttons");
	e_ui_action_set_usable_for_kinds (action, E_UI_ELEMENT_KIND_HEADERBAR);
}

static void
cal_shell_view_dispose (GObject *object)
{
	e_cal_shell_view_private_dispose (E_CAL_SHELL_VIEW (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_cal_shell_view_parent_class)->dispose (object);
}

static void
cal_shell_view_finalize (GObject *object)
{
	e_cal_shell_view_private_finalize (E_CAL_SHELL_VIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_cal_shell_view_parent_class)->finalize (object);
}

static void
cal_shell_view_constructed (GObject *object)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSearchbar *searchbar;
	ECalShellView *cal_shell_view;
	ECalShellContent *cal_shell_content;
	EUIManager *ui_manager;
	EUICustomizer *customizer;
	GtkWidget *container;
	GtkWidget *widget;
	gulong handler_id;

	cal_shell_view = E_CAL_SHELL_VIEW (object);
	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);
	ui_manager = e_shell_view_get_ui_manager (shell_view);

	e_ui_manager_freeze (ui_manager);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_cal_shell_view_parent_class)->constructed (object);

	e_cal_shell_view_private_constructed (cal_shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	searchbar = e_cal_shell_content_get_searchbar (cal_shell_content);
	container = e_shell_searchbar_get_search_box (searchbar);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_style_context_add_class (gtk_widget_get_style_context (widget), "linked");
	cal_shell_view_add_action_button (GTK_BOX (widget), ACTION (CALENDAR_SEARCH_PREV), ui_manager);
	cal_shell_view_add_action_button (GTK_BOX (widget), ACTION (CALENDAR_SEARCH_NEXT), ui_manager);
	cal_shell_view_add_action_button (GTK_BOX (widget), ACTION (CALENDAR_SEARCH_STOP), ui_manager);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	handler_id = g_signal_connect (
		shell, "prepare-for-quit",
		G_CALLBACK (cal_shell_view_prepare_for_quit_cb),
		cal_shell_view);

	cal_shell_view->priv->shell = g_object_ref (shell);
	cal_shell_view->priv->prepare_for_quit_handler_id = handler_id;

	e_ui_manager_thaw (ui_manager);

	customizer = e_ui_manager_get_customizer (ui_manager);
	e_ui_customizer_register (customizer, "calendar-popup", _("Calendar Context Menu"));
	e_ui_customizer_register (customizer, "calendar-event-popup", _("Event Context Menu"));
	e_ui_customizer_register (customizer, "calendar-memopad-popup", _("Memo Context Menu"));
	e_ui_customizer_register (customizer, "calendar-taskpad-popup", _("Task Context Menu"));
	/* Translators: It refers to a context menu shown when there is no event below the cursor nor selected; the opposite is the "Event Context Menu" text */
	e_ui_customizer_register (customizer, "calendar-empty-popup", _("No Event Context Menu"));
}

static void
e_cal_shell_view_class_init (ECalShellViewClass *class)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;
	ECalBaseShellViewClass *cal_base_shell_view_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = cal_shell_view_dispose;
	object_class->finalize = cal_shell_view_finalize;
	object_class->constructed = cal_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = _("Calendar");
	shell_view_class->icon_name = "x-office-calendar";
	shell_view_class->ui_definition = "evolution-calendars.eui";
	shell_view_class->ui_manager_id = "org.gnome.evolution.calendars";
	shell_view_class->search_rules = "caltypes.xml";
	shell_view_class->new_shell_content = e_cal_shell_content_new;
	shell_view_class->new_shell_sidebar = e_cal_base_shell_sidebar_new;
	shell_view_class->execute_search = cal_shell_view_execute_search;
	shell_view_class->update_actions = cal_shell_view_update_actions;
	shell_view_class->init_ui_data = cal_shell_view_init_ui_data;

	cal_base_shell_view_class = E_CAL_BASE_SHELL_VIEW_CLASS (class);
	cal_base_shell_view_class->source_type = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;

	/* Ensure the GalView types we need are registered. */
	g_type_ensure (GAL_TYPE_VIEW_CALENDAR_DAY);
	g_type_ensure (GAL_TYPE_VIEW_CALENDAR_WORK_WEEK);
	g_type_ensure (GAL_TYPE_VIEW_CALENDAR_WEEK);
	g_type_ensure (GAL_TYPE_VIEW_CALENDAR_MONTH);
	g_type_ensure (GAL_TYPE_VIEW_CALENDAR_YEAR);
	g_type_ensure (GAL_TYPE_VIEW_ETABLE);

	e_calendar_a11y_init ();
}

static void
e_cal_shell_view_class_finalize (ECalShellViewClass *class)
{
}

static void
e_cal_shell_view_init (ECalShellView *cal_shell_view)
{
	cal_shell_view->priv = e_cal_shell_view_get_instance_private (cal_shell_view);

	e_cal_shell_view_private_init (cal_shell_view);
}

void
e_cal_shell_view_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_cal_shell_view_register_type (type_module);
}

void
e_cal_shell_view_set_view_id_from_view_kind (ECalShellView *self,
					     ECalViewKind view_kind)
{
	const gchar *view_id = NULL;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (self));

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

		case E_CAL_VIEW_KIND_YEAR:
			view_id = "Year_View";
			break;

		case E_CAL_VIEW_KIND_LIST:
			view_id = "List_View";
			break;

		default:
			g_return_if_reached ();
	}

	e_shell_view_set_view_id (E_SHELL_VIEW (self), view_id);
}
