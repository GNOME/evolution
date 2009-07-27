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

#include "e-cal-shell-view-private.h"

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
#if 0
	ECalShellContent *cal_shell_content;
	ECalShellSidebar *cal_shell_sidebar;
	EShellWindow *shell_window;
	EShellView *shell_view;
	ECalendarView *calendar_view;
	GnomeCalendarViewType view_type;
	ECalModel *model;
	ESourceSelector *selector;
	ESourceGroup *source_group;
	ESourceList *source_list;
	ESource *source;
	gint response;
	gchar *uri;
	GError *error = NULL;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_content->priv->cal_shell_content;
	view_type = e_cal_shell_content_get_current_view (cal_shell_content);
	calendar_view = e_cal_shell_content_get_calendar_view (
		cal_shell_content, view_type);
	model = e_calendar_view_get_model (calendar_view);

	cal_shell_sidebar = cal_shell_sidebar->priv->cal_shell_sidebar;
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);
	source = e_source_selector_peek_primary_selection (selector);
	g_return_if_fail (E_IS_SOURCE (source));

	/* Ask for confirmation. */
	response = e_error_run (
		GTK_WINDOW (shell_window),
		"calendar:prompt-delete-calendar",
		e_source_peek_name (source));
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

	source_list = cal_shell_view->priv->source_list;
	if (!e_source_list_sync (source_list, &error)) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
#endif
}

static void
action_calendar_go_back_cb (GtkAction *action,
                            ECalShellView *cal_shell_view)
{
#if 0
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	gnome_calendar_previous (calendar);
#endif
}

static void
action_calendar_go_forward_cb (GtkAction *action,
                               ECalShellView *cal_shell_view)
{
#if 0
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	gnome_calendar_next (calendar);
#endif
}

static void
action_calendar_go_today_cb (GtkAction *action,
                             ECalShellView *cal_shell_view)
{
#if 0
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	gnome_calendar_goto_today (calendar);
#endif
}

static void
action_calendar_jump_to_cb (GtkAction *action,
                            ECalShellView *cal_shell_view)
{
#if 0
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	goto_dialog (calendar);
#endif
}

static void
action_calendar_new_cb (GtkAction *action,
                        ECalShellView *cal_shell_view)
{
#if 0
	EShellView *shell_view;
	EShellWindow *shell_window;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	calendar_setup_new_calendar (GTK_WINDOW (shell_window));
#endif
}

static void
action_calendar_print_cb (GtkAction *action,
                          ECalShellView *cal_shell_view)
{
#if 0
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
		ECalListView *list_view;
		ETable *table;

		list_view = E_CAL_LIST_VIEW (view);
		table = e_table_scrolled_get_table (list_view->table_scrolled);
		print_table (table, _("Print"), _("Calendar"), action);
	} else {
		time_t start;

		gnome_calendar_get_current_time_range (calendar, &start, NULL);
		print_calendar (calendar, action, start);
	}
#endif
}

static void
action_calendar_print_preview_cb (GtkAction *action,
                                  ECalShellView *cal_shell_view)
{
#if 0
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
		ECalListView *list_view;
		ETable *table;

		list_view = E_CAL_LIST_VIEW (view);
		table = e_table_scrolled_get_table (list_view->table_scrolled);
		print_table (table, _("Print"), _("Calendar"), action);
	} else {
		time_t start;

		gnome_calendar_get_current_time_range (calendar, &start, NULL);
		print_calendar (calendar, action, start);
	}
#endif
}

static void
action_calendar_properties_cb (GtkAction *action,
                               ECalShellView *cal_shell_view)
{
#if 0
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

	calendar_setup_edit_calendar (GTK_WINDOW (shell_window), source);
#endif
}

static void
action_calendar_purge_cb (GtkAction *action,
                          ECalShellView *cal_shell_view)
{
	/* FIXME */
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
action_calendar_search_cb (GtkRadioAction *action,
                           GtkRadioAction *current,
                           ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	const gchar *search_hint;

	/* XXX Figure out a way to handle this in EShellContent
	 *     instead of every shell view having to handle it.
	 *     The problem is EShellContent does not know what
	 *     the search option actions are for this view.  It
	 *     would have to dig up the popup menu and retrieve
	 *     the action for each menu item.  Seems messy. */

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	search_hint = gtk_action_get_label (GTK_ACTION (current));
	e_shell_content_set_search_hint (shell_content, search_hint);
}

static void
action_calendar_select_one_cb (GtkAction *action,
                               ECalShellView *cal_shell_view)
{
	ECalShellSidebar *cal_shell_sidebar;
	ESourceSelector *selector;
	ESource *primary;
	GSList *list, *iter;

	/* XXX ESourceSelector should provide a function for this. */

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);
	primary = e_source_selector_peek_primary_selection (selector);
	g_return_if_fail (primary != NULL);

	list = e_source_selector_get_selection (selector);
	for (iter = list; iter != NULL; iter = iter->next) {
		ESource *source = iter->data;

		if (source == primary)
			continue;

		e_source_selector_unselect_source (selector, source);
	}
	e_source_selector_free_selection (list);

	e_source_selector_select_source (selector, primary);
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
action_event_clipboard_copy_cb (GtkAction *action,
                                ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	e_cal_shell_content_copy_clipboard (cal_shell_content);
}

static void
action_event_clipboard_cut_cb (GtkAction *action,
                               ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	e_cal_shell_content_cut_clipboard (cal_shell_content);
}

static void
action_event_clipboard_paste_cb (GtkAction *action,
                                 ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	e_cal_shell_content_paste_clipboard (cal_shell_content);
}

static void
action_event_copy_cb (GtkAction *action,
                      ECalShellView *cal_shell_view)
{
	/* FIXME */
}

static void
action_event_delegate_cb (GtkAction *action,
                          ECalShellView *cal_shell_view)
{
	/* FIXME */
}

static void
action_event_delete_cb (GtkAction *action,
                        ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	e_cal_shell_content_delete_selection (cal_shell_content);
}

static void
action_event_delete_occurrence_cb (GtkAction *action,
                                   ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	e_cal_shell_content_delete_selected_occurrence (cal_shell_content);
}

static void
action_event_delete_occurrence_all_cb (GtkAction *action,
                                       ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;

	/* XXX Same as "event-delete". */
	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	e_cal_shell_content_delete_selection (cal_shell_content);
}

static void
action_event_forward_cb (GtkAction *action,
                         ECalShellView *cal_shell_view)
{
	/* FIXME */
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
	/* FIXME */
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
	/* FIXME */
}

static void
action_event_open_cb (GtkAction *action,
                      ECalShellView *cal_shell_view)
{
#if 0
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	GnomeCalendar *calendar;
	ECalendarView *view;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	view = gnome_calendar_get_calendar_view (calendar, view_type);

	e_calendar_view_open_event (view);
#endif
}

static void
action_event_print_cb (GtkAction *action,
                       ECalShellView *cal_shell_view)
{
	/* FIXME */
}

static void
action_event_reply_cb (GtkAction *action,
                       ECalShellView *cal_shell_view)
{
	/* FIXME */
}

static void
action_event_reply_all_cb (GtkAction *action,
                           ECalShellView *cal_shell_view)
{
	/* FIXME */
}

static void
action_event_save_as_cb (GtkAction *action,
                         ECalShellView *cal_shell_view)
{
	/* FIXME */
}

static void
action_event_schedule_cb (GtkAction *action,
                          ECalShellView *cal_shell_view)
{
	/* FIXME */
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

static void
action_search_execute_cb (GtkAction *action,
                          ECalShellView *cal_shell_view)
{
	EShellView *shell_view;

	/* All shell views respond to the activation of this action,
	 * which is defined by EShellWindow.  But only the currently
	 * active shell view proceeds with executing the search. */
	shell_view = E_SHELL_VIEW (cal_shell_view);
	if (!e_shell_view_is_active (shell_view))
		return;

	e_cal_shell_view_execute_search (cal_shell_view);
}

static void
action_search_filter_cb (GtkRadioAction *action,
                         GtkRadioAction *current,
                         ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	gtk_action_activate (ACTION (SEARCH_EXECUTE));
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
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
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

	{ "event-clipboard-copy",
	  GTK_STOCK_COPY,
	  NULL,
	  NULL,
	  N_("Copy the selection"),
	  G_CALLBACK (action_event_clipboard_copy_cb) },

	{ "event-clipboard-cut",
	  GTK_STOCK_CUT,
	  NULL,
	  NULL,
	  N_("Cut the selection"),
	  G_CALLBACK (action_event_clipboard_cut_cb) },

	{ "event-clipboard-paste",
	  GTK_STOCK_PASTE,
	  NULL,
	  NULL,
	  N_("Paste the clipboard"),
	  G_CALLBACK (action_event_clipboard_paste_cb) },

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
	  NULL,
	  NULL,
	  N_("Delete the appointment"),
	  G_CALLBACK (action_event_delete_cb) },

	{ "event-delete-occurrence",
	  GTK_STOCK_DELETE,
	  N_("Delete This _Occurrence"),
	  NULL,
	  N_("Delete this occurrence"),
	  G_CALLBACK (action_event_delete_occurrence_cb) },

	{ "event-delete-occurrence-all",
	  GTK_STOCK_DELETE,
	  N_("Delete _All Occurrences"),
	  NULL,
	  N_("Delete all occurrences"),
	  G_CALLBACK (action_event_delete_occurrence_all_cb) },

	{ "event-all-day-new",
	  NULL,
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
	  NULL,
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
	  NULL,
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
	  NULL,
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
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_event_save_as_cb) },

	{ "event-schedule",
	  NULL,
	  N_("_Schedule Meeting..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_event_schedule_cb) },

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
	  NULL,
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

	{ "calendar-popup-rename",
	  NULL,
	  "calendar-rename" },

	{ "calendar-popup-select-one",
	  NULL,
	  "calendar-select-one" },

	{ "event-popup-clipboard-copy",
	  NULL,
	  "event-clipboard-copy" },

	{ "event-popup-clipboard-cut",
	  NULL,
	  "event-clipboard-cut" },

	{ "event-popup-clipboard-paste",
	  NULL,
	  "event-clipboard-paste" },

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
	  "event-schedule" }
};

static GtkRadioActionEntry calendar_view_entries[] = {

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
	  N_("Active Appointements"),
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
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	GtkAction *action;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

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
		G_N_ELEMENTS (calendar_view_entries), GNOME_CAL_DAY_VIEW,
		G_CALLBACK (action_calendar_view_cb), cal_shell_view);
	gtk_action_group_add_radio_actions (
		action_group, calendar_search_entries,
		G_N_ELEMENTS (calendar_search_entries),
		CALENDAR_SEARCH_SUMMARY_CONTAINS,
		G_CALLBACK (action_calendar_search_cb), cal_shell_view);

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

	action = ACTION (EVENT_DELETE);
	g_object_set (action, "short-label", _("Delete"), NULL);

	g_signal_connect (
		ACTION (GAL_SAVE_CUSTOM_VIEW), "activate",
		G_CALLBACK (action_gal_save_custom_view_cb), cal_shell_view);

	g_signal_connect (
		ACTION (SEARCH_EXECUTE), "activate",
		G_CALLBACK (action_search_execute_cb), cal_shell_view);

	/* Initialize the memo and task pad actions. */
	e_cal_shell_view_memopad_actions_init (cal_shell_view);
	e_cal_shell_view_taskpad_actions_init (cal_shell_view);
}

void
e_cal_shell_view_update_search_filter (ECalShellView *cal_shell_view)
{
	EShellContent *shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;
	GtkActionGroup *action_group;
	GtkRadioAction *radio_action;
	GList *list, *iter;
	GSList *group;
	gint ii;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	action_group = ACTION_GROUP (CALENDAR_FILTER);
	e_action_group_remove_all_actions (action_group);

	/* Add the standard filter actions. */
	gtk_action_group_add_radio_actions (
		action_group, calendar_filter_entries,
		G_N_ELEMENTS (calendar_filter_entries),
		CALENDAR_FILTER_ANY_CATEGORY,
		G_CALLBACK (action_search_filter_cb),
		cal_shell_view);

	/* Retrieve the radio group from an action we just added. */
	list = gtk_action_group_list_actions (action_group);
	radio_action = GTK_RADIO_ACTION (list->data);
	group = gtk_radio_action_get_group (radio_action);
	g_list_free (list);

	/* Build the category actions. */

	list = e_categories_get_list ();
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

	/* Use any action in the group; doesn't matter which. */
	e_shell_content_set_filter_action (shell_content, radio_action);

	ii = CALENDAR_FILTER_UNMATCHED;
	e_shell_content_add_filter_separator_after (shell_content, ii);

	ii = CALENDAR_FILTER_NEXT_7_DAYS_APPOINTMENTS;
	e_shell_content_add_filter_separator_after (shell_content, ii);
}
