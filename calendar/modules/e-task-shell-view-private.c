/*
 * e-task-shell-view-private.c
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

#include "e-task-shell-view-private.h"

#include <widgets/menus/gal-view-factory-etable.h>

static void
task_shell_view_config_hide_completed_tasks_changed_cb (GConfClient *client,
                                                        guint id,
                                                        GConfEntry *entry,
                                                        gpointer user_data)
{
	ETaskShellView *task_shell_view = user_data;
	ETaskShellContent *task_shell_content;
	ETaskShellSidebar *task_shell_sidebar;
	ECalendarTable *task_table;
	GList *clients;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	task_shell_sidebar = task_shell_view->priv->task_shell_sidebar;
	clients = e_task_shell_sidebar_get_clients (task_shell_sidebar);

	e_calendar_table_process_completed_tasks (task_table, clients, TRUE);

	/* Search query takes whether to show completed tasks into account,
	 * so if the preference has changed we need to update the query. */
	e_task_shell_view_execute_search (task_shell_view);

	g_list_free (clients);
}

static void
task_shell_view_config_timezone_changed_cb (GConfClient *client,
                                            guint id,
                                            GConfEntry *entry,
                                            gpointer user_data)
{
	ETaskShellView *task_shell_view = user_data;

	e_task_shell_view_update_timezone (task_shell_view);
}

static void
task_shell_view_table_popup_event_cb (EShellView *shell_view,
                                      GdkEventButton *event)
{
	const gchar *widget_path;

	widget_path = "/task-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, event);
}

static void
task_shell_view_table_user_created_cb (ETaskShellView *task_shell_view,
                                       ECalendarTable *task_table)
{
	ETaskShellSidebar *task_shell_sidebar;
	ECalModel *model;
	ECal *client;
	ESource *source;

	/* This is the "Click to Add" handler. */

	model = e_calendar_table_get_model (task_table);
	client = e_cal_model_get_default_client (model);
	source = e_cal_get_source (client);

	task_shell_sidebar = task_shell_view->priv->task_shell_sidebar;
	e_task_shell_sidebar_add_source (task_shell_sidebar, source);

	e_cal_model_add_client (model, client);
}

static void
task_shell_view_selector_client_added_cb (ETaskShellView *task_shell_view,
                                          ECal *client)
{
	ETaskShellContent *task_shell_content;
	ECalendarTable *task_table;
	ECalModel *model;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	model = e_calendar_table_get_model (task_table);

	e_cal_model_add_client (model, client);
	e_task_shell_view_update_timezone (task_shell_view);
}

static void
task_shell_view_selector_client_removed_cb (ETaskShellView *task_shell_view,
                                            ECal *client)
{
	ETaskShellContent *task_shell_content;
	ECalendarTable *task_table;
	ECalModel *model;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	model = e_calendar_table_get_model (task_table);

	e_cal_model_remove_client (model, client);
}

static gboolean
task_shell_view_selector_popup_event_cb (EShellView *shell_view,
                                         ESource *primary_source,
                                         GdkEventButton *event)
{
	const gchar *widget_path;

	widget_path = "/task-list-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, event);

	return TRUE;
}

static gboolean
task_shell_view_update_timeout_cb (ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskShellSidebar *task_shell_sidebar;
	ECalendarTable *task_table;
	ECalModel *model;
	GList *clients;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	model = e_calendar_table_get_model (task_table);

	task_shell_sidebar = task_shell_view->priv->task_shell_sidebar;
	clients = e_task_shell_sidebar_get_clients (task_shell_sidebar);

	e_calendar_table_process_completed_tasks (task_table, clients, FALSE);
	e_cal_model_tasks_update_due_tasks (E_CAL_MODEL_TASKS (model));

	g_list_free (clients);

	return TRUE;
}

static void
task_shell_view_load_view_collection (EShellViewClass *shell_view_class)
{
	GalViewCollection *collection;
	GalViewFactory *factory;
	ETableSpecification *spec;
	const gchar *base_dir;
	gchar *filename;

	collection = shell_view_class->view_collection;

	base_dir = EVOLUTION_ETSPECDIR;
	spec = e_table_specification_new ();
	filename = g_build_filename (base_dir, ETSPEC_FILENAME, NULL);
	if (!e_table_specification_load_from_file (spec, filename))
		g_critical ("Unable to load ETable specification file "
			    "for tasks");
	g_free (filename);

	factory = gal_view_factory_etable_new (spec);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);
	g_object_unref (spec);

	gal_view_collection_load (collection);
}

static void
task_shell_view_notify_view_id_cb (ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	GalViewInstance *view_instance;
	const gchar *view_id;

	task_shell_content = task_shell_view->priv->task_shell_content;
	view_instance =
		e_task_shell_content_get_view_instance (task_shell_content);
	view_id = e_shell_view_get_view_id (E_SHELL_VIEW (task_shell_view));

	/* A NULL view ID implies we're in a custom view.  But you can
	 * only get to a custom view via the "Define Views" dialog, which
	 * would have already modified the view instance appropriately.
	 * Furthermore, there's no way to refer to a custom view by ID
	 * anyway, since custom views have no IDs. */
	if (view_id == NULL)
		return;

	gal_view_instance_set_current_view_id (view_instance, view_id);
}

void
e_task_shell_view_private_init (ETaskShellView *task_shell_view,
                                EShellViewClass *shell_view_class)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;
	ESourceList *source_list;
	GObject *object;

	object = G_OBJECT (shell_view_class->type_module);
	source_list = g_object_get_data (object, "source-list");
	g_return_if_fail (E_IS_SOURCE_LIST (source_list));

	priv->source_list = g_object_ref (source_list);
	priv->task_actions = gtk_action_group_new ("tasks");
	priv->filter_actions = gtk_action_group_new ("tasks-filter");

	if (!gal_view_collection_loaded (shell_view_class->view_collection))
		task_shell_view_load_view_collection (shell_view_class);

	g_signal_connect (
		task_shell_view, "notify::view-id",
		G_CALLBACK (task_shell_view_notify_view_id_cb), NULL);
}

void
e_task_shell_view_private_constructed (ETaskShellView *task_shell_view)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;
	ETaskShellContent *task_shell_content;
	ETaskShellSidebar *task_shell_sidebar;
	EShellView *shell_view;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	ECalendarTable *task_table;
	ECalModel *model;
	ETable *table;
	ESourceSelector *selector;
	guint id;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	/* Cache these to avoid lots of awkward casting. */
	priv->task_shell_content = g_object_ref (shell_content);
	priv->task_shell_sidebar = g_object_ref (shell_sidebar);

	task_shell_content = E_TASK_SHELL_CONTENT (shell_content);
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	model = e_calendar_table_get_model (task_table);
	table = e_calendar_table_get_table (task_table);

	task_shell_sidebar = E_TASK_SHELL_SIDEBAR (shell_sidebar);
	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);

	g_signal_connect_swapped (
		task_table, "open-component",
		G_CALLBACK (e_task_shell_view_open_task),
		task_shell_view);

	g_signal_connect_swapped (
		task_table, "popup-event",
		G_CALLBACK (task_shell_view_table_popup_event_cb),
		task_shell_view);

	g_signal_connect_swapped (
		task_table, "status-message",
		G_CALLBACK (e_task_shell_view_set_status_message),
		task_shell_view);

	g_signal_connect_swapped (
		task_table, "user-created",
		G_CALLBACK (task_shell_view_table_user_created_cb),
		task_shell_view);

	g_signal_connect_swapped (
		model, "model-changed",
		G_CALLBACK (e_task_shell_view_update_sidebar),
		task_shell_view);

	g_signal_connect_swapped (
		model, "model-rows-deleted",
		G_CALLBACK (e_task_shell_view_update_sidebar),
		task_shell_view);

	g_signal_connect_swapped (
		model, "model-rows-inserted",
		G_CALLBACK (e_task_shell_view_update_sidebar),
		task_shell_view);

	g_signal_connect_swapped (
		table, "selection-change",
		G_CALLBACK (e_task_shell_view_update_sidebar),
		task_shell_view);

	g_signal_connect_swapped (
		task_shell_sidebar, "client-added",
		G_CALLBACK (task_shell_view_selector_client_added_cb),
		task_shell_view);

	g_signal_connect_swapped (
		task_shell_sidebar, "client-removed",
		G_CALLBACK (task_shell_view_selector_client_removed_cb),
		task_shell_view);

	g_signal_connect_swapped (
		task_shell_sidebar, "status-message",
		G_CALLBACK (e_task_shell_view_set_status_message),
		task_shell_view);

	g_signal_connect_swapped (
		selector, "popup-event",
		G_CALLBACK (task_shell_view_selector_popup_event_cb),
		task_shell_view);

	g_signal_connect_swapped (
		selector, "primary-selection-changed",
		G_CALLBACK (e_shell_view_update_actions),
		task_shell_view);

	e_categories_register_change_listener (
		G_CALLBACK (e_task_shell_view_update_search_filter),
		task_shell_view);

	task_shell_view_update_timeout_cb (task_shell_view);
	priv->update_timeout = g_timeout_add_full (
		G_PRIORITY_LOW, 60000, (GSourceFunc)
		task_shell_view_update_timeout_cb,
		task_shell_view, NULL);

	/* Listen for configuration changes. */

	/* Timezone */
	id = calendar_config_add_notification_timezone (
		task_shell_view_config_timezone_changed_cb, task_shell_view);
	priv->notifications = g_list_prepend (
		priv->notifications, GUINT_TO_POINTER (id));

	/* Hide Completed Tasks (enable/units/value) */
	id = calendar_config_add_notification_hide_completed_tasks (
		task_shell_view_config_hide_completed_tasks_changed_cb,
		task_shell_view);
	priv->notifications = g_list_prepend (
		priv->notifications, GUINT_TO_POINTER (id));
	id = calendar_config_add_notification_hide_completed_tasks_units (
		task_shell_view_config_hide_completed_tasks_changed_cb,
		task_shell_view);
	priv->notifications = g_list_prepend (
		priv->notifications, GUINT_TO_POINTER (id));
	id = calendar_config_add_notification_hide_completed_tasks_value (
		task_shell_view_config_hide_completed_tasks_changed_cb,
		task_shell_view);
	priv->notifications = g_list_prepend (
		priv->notifications, GUINT_TO_POINTER (id));

	e_task_shell_view_actions_init (task_shell_view);
	e_task_shell_view_update_sidebar (task_shell_view);
	e_task_shell_view_update_search_filter (task_shell_view);
	e_task_shell_view_update_timezone (task_shell_view);

	e_task_shell_view_execute_search (task_shell_view);
}

void
e_task_shell_view_private_dispose (ETaskShellView *task_shell_view)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;
	GList *iter;

	DISPOSE (priv->source_list);

	DISPOSE (priv->task_actions);
	DISPOSE (priv->filter_actions);

	DISPOSE (priv->task_shell_content);
	DISPOSE (priv->task_shell_sidebar);

	if (task_shell_view->priv->activity != NULL) {
		/* XXX Activity is no cancellable. */
		e_activity_complete (task_shell_view->priv->activity);
		g_object_unref (task_shell_view->priv->activity);
		task_shell_view->priv->activity = NULL;
	}

	if (priv->update_timeout > 0) {
		g_source_remove (priv->update_timeout);
		priv->update_timeout = 0;
	}

	for (iter = priv->notifications; iter != NULL; iter = iter->next) {
		guint notification_id = GPOINTER_TO_UINT (iter->data);
		calendar_config_remove_notification (notification_id);
	}
	g_list_free (priv->notifications);
	priv->notifications = NULL;
}

void
e_task_shell_view_private_finalize (ETaskShellView *task_shell_view)
{
	/* XXX Nothing to do? */
}

void
e_task_shell_view_execute_search (ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	GtkAction *action;
	GString *string;
	ECalComponentPreview *task_preview;
	ECalendarTable *task_table;
	ECalModel *model;
	FilterRule *rule;
	const gchar *format;
	const gchar *text;
	time_t start_range;
	time_t end_range;
	gchar *start, *end;
	gchar *query;
	gchar *temp;
	gint value;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	text = e_shell_content_get_search_text (shell_content);

	shell_window = e_shell_view_get_shell_window (shell_view);
	action = ACTION (TASK_SEARCH_ANY_FIELD_CONTAINS);
	value = gtk_radio_action_get_current_value (
		GTK_RADIO_ACTION (action));

	if (text == NULL || *text == '\0') {
		text = "";
		value = TASK_SEARCH_SUMMARY_CONTAINS;
	}

	switch (value) {
		default:
			text = "";
			/* fall through */

		case TASK_SEARCH_SUMMARY_CONTAINS:
			format = "(contains? \"summary\" %s)";
			break;

		case TASK_SEARCH_DESCRIPTION_CONTAINS:
			format = "(contains? \"description\" %s)";
			break;

		case TASK_SEARCH_ANY_FIELD_CONTAINS:
			format = "(contains? \"any\" %s)";
			break;
	}

	/* Build the query. */
	string = g_string_new ("");
	e_sexp_encode_string (string, text);
	query = g_strdup_printf (format, string->str);
	g_string_free (string, TRUE);

	/* Apply selected filter. */
	value = e_shell_content_get_filter_value (shell_content);
	switch (value) {
		case TASK_FILTER_ANY_CATEGORY:
			break;

		case TASK_FILTER_UNMATCHED:
			temp = g_strdup_printf (
				"(and (has-categories? #f) %s)", query);
			g_free (query);
			query = temp;
			break;

		case TASK_FILTER_NEXT_7_DAYS_TASKS:
			start_range = time (NULL);
			end_range = time_add_day (start_range, 7);
			start = isodate_from_time_t (start_range);
			end = isodate_from_time_t (end_range);

			temp = g_strdup_printf (
				"(and %s (due-in-time-range? "
				"(make-time \"%s\") (make-time \"%s\")))",
				query, start, end);
			g_free (query);
			query = temp;
			break;

		case TASK_FILTER_ACTIVE_TASKS:
			start_range = time (NULL);
			end_range = time_add_day (start_range, 365);
			start = isodate_from_time_t (start_range);
			end = isodate_from_time_t (end_range);

			temp = g_strdup_printf (
				"(and %s (due-in-time-range? "
				"(make-time \"%s\") (make-time \"%s\")) "
				"(not (is-completed?)))",
				query, start, end);
			g_free (query);
			query = temp;
			break;

		case TASK_FILTER_OVERDUE_TASKS:
			start_range = 0;
			end_range = time (NULL);
			start = isodate_from_time_t (start_range);
			end = isodate_from_time_t (end_range);

			temp = g_strdup_printf (
				"(and %s (due-in-time-range? "
				"(make-time \"%s\") (make-time \"%s\")) "
				"(not (is-completed?)))",
				query, start, end);
			g_free (query);
			query = temp;
			break;

		case TASK_FILTER_COMPLETED_TASKS:
			temp = g_strdup_printf (
				"(and (is-completed?) %s)", query);
			g_free (query);
			query = temp;
			break;

		case TASK_FILTER_TASKS_WITH_ATTACHMENTS:
			temp = g_strdup_printf (
				"(and (has-attachments?) %s)", query);
			g_free (query);
			query = temp;
			break;

		default:
		{
			GList *categories;
			const gchar *category_name;

			categories = e_categories_get_list ();
			category_name = g_list_nth_data (categories, value);
			g_list_free (categories);

			temp = g_strdup_printf (
				"(and (has-categories? \"%s\") %s)",
				category_name, query);
			g_free (query);
			query = temp;
			break;
		}
	}

	/* Honor the user's preference to hide completed tasks. */
	temp = calendar_config_get_hide_completed_tasks_sexp (FALSE);
	if (temp != NULL) {
		gchar *temp2;

		temp2 = g_strdup_printf ("(and %s %s)", temp, query);
		g_free (query);
		g_free (temp);
		query = temp2;
	}

	/* XXX This is wrong.  We need to programmatically construct a
	 *     FilterRule, tell it to build code, and pass the resulting
	 *     expression string to ECalModel. */
	rule = filter_rule_new ();
	e_shell_content_set_search_rule (shell_content, rule);
	g_object_unref (rule);

	/* Submit the query. */
	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	model = e_calendar_table_get_model (task_table);
	e_cal_model_set_search_query (model, query);
	g_free (query);

	task_preview =
		e_task_shell_content_get_task_preview (task_shell_content);
	e_cal_component_preview_clear (task_preview);
}

void
e_task_shell_view_open_task (ETaskShellView *task_shell_view,
                             ECalModelComponent *comp_data)
{
	CompEditor *editor;
	CompEditorFlags flags = 0;
	ECalComponent *comp;
	icalcomponent *clone;
	icalproperty *prop;
	const gchar *uid;

	g_return_if_fail (E_IS_TASK_SHELL_VIEW (task_shell_view));
	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));

	uid = icalcomponent_get_uid (comp_data->icalcomp);
	editor = comp_editor_find_instance (uid);

	if (editor != NULL)
		goto exit;

	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	e_cal_component_set_icalcomponent (comp, clone);

	prop = icalcomponent_get_first_property (
		comp_data->icalcomp, ICAL_ATTENDEE_PROPERTY);
	if (prop != NULL)
		flags |= COMP_EDITOR_IS_ASSIGNED;

	if (itip_organizer_is_user (comp, comp_data->client))
		flags |= COMP_EDITOR_USER_ORG;

	if (!e_cal_component_has_attendees (comp))
		flags |= COMP_EDITOR_USER_ORG;

	editor = task_editor_new (comp_data->client, flags);
	comp_editor_edit_comp (editor, comp);

	g_object_ref (comp);

	if (flags & COMP_EDITOR_IS_ASSIGNED)
		task_editor_show_assignment (TASK_EDITOR (editor));

exit:
	gtk_window_present (GTK_WINDOW (editor));
}

void
e_task_shell_view_set_status_message (ETaskShellView *task_shell_view,
                                      const gchar *status_message,
                                      gdouble percent)
{
	EActivity *activity;
	EShellView *shell_view;
	EShellModule *shell_module;

	g_return_if_fail (E_IS_TASK_SHELL_VIEW (task_shell_view));

	activity = task_shell_view->priv->activity;
	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_module = e_shell_view_get_shell_module (shell_view);

	if (status_message == NULL || *status_message == '\0') {
		if (activity != NULL) {
			e_activity_complete (activity);
			g_object_unref (activity);
			activity = NULL;
		}

	} else if (activity == NULL) {
		activity = e_activity_new (status_message);
		e_activity_set_percent (activity, percent);
		e_shell_module_add_activity (shell_module, activity);

	} else {
		e_activity_set_percent (activity, percent);
		e_activity_set_primary_text (activity, status_message);
	}

	task_shell_view->priv->activity = activity;
}

void
e_task_shell_view_update_sidebar (ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	ECalendarTable *task_table;
	ECalModel *model;
	ETable *table;
	GString *string;
	const gchar *format;
	gint n_rows;
	gint n_selected;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	model = e_calendar_table_get_model (task_table);
	table = e_calendar_table_get_table (task_table);

	n_rows = e_table_model_row_count (E_TABLE_MODEL (model));
	n_selected = e_table_selected_count (table);

	string = g_string_sized_new (64);

	format = ngettext ("%d task", "%d tasks", n_rows);
	g_string_append_printf (string, format, n_rows);

	if (n_selected > 0) {
		format = _("%d selected");
		g_string_append_len (string, ", ", 2);
		g_string_append_printf (string, format, n_selected);
	}

	e_shell_sidebar_set_secondary_text (shell_sidebar, string->str);

	g_string_free (string, TRUE);
}

void
e_task_shell_view_update_timezone (ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskShellSidebar *task_shell_sidebar;
	ECalComponentPreview *task_preview;
	icaltimezone *timezone;
	GList *clients, *iter;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_preview = e_task_shell_content_get_task_preview (task_shell_content);

	task_shell_sidebar = task_shell_view->priv->task_shell_sidebar;
	clients = e_task_shell_sidebar_get_clients (task_shell_sidebar);

	timezone = calendar_config_get_icaltimezone ();

	for (iter = clients; iter != NULL; iter = iter->next) {
		ECal *client = iter->data;

		if (e_cal_get_load_state (client) == E_CAL_LOAD_LOADED)
			e_cal_set_default_timezone (client, timezone, NULL);
	}

	e_cal_component_preview_set_default_timezone (task_preview, timezone);

	g_list_free (clients);
}
