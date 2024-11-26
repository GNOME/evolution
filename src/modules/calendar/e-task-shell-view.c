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

#include "e-cal-base-shell-sidebar.h"
#include "e-task-shell-content.h"
#include "e-task-shell-view.h"

#include "e-task-shell-view-private.h"

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ETaskShellView, e_task_shell_view, E_TYPE_CAL_BASE_SHELL_VIEW, 0,
	G_ADD_PRIVATE_DYNAMIC (ETaskShellView))

enum {
	PROP_0,
	PROP_CONFIRM_PURGE
};

static void
task_shell_view_execute_search (EShellView *shell_view)
{
	ETaskShellContent *task_shell_content;
	EShellContent *shell_content;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	EUIAction *action;
	ECalComponentPreview *task_preview;
	EPreviewPane *preview_pane;
	ETaskTable *task_table;
	EWebView *web_view;
	ECalModel *model;
	ECalDataModel *data_model;
	GVariant *state;
	ICalTimezone *timezone;
	ICalTime *current_time;
	time_t start_range;
	time_t end_range;
	time_t now_time;
	gchar *start, *end;
	gchar *query;
	gchar *temp;
	gint value;

	shell_content = e_shell_view_get_shell_content (shell_view);

	task_shell_content = E_TASK_SHELL_CONTENT (shell_content);
	searchbar = e_task_shell_content_get_searchbar (task_shell_content);

	task_shell_content = E_TASK_SHELL_CONTENT (shell_content);
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	model = e_task_table_get_model (task_table);
	data_model = e_cal_model_get_data_model (model);
	timezone = e_cal_model_get_timezone (model);
	current_time = i_cal_time_new_current_with_zone (timezone);
	now_time = time_day_begin (i_cal_time_as_timet (current_time));
	g_clear_object (&current_time);

	action = ACTION (TASK_SEARCH_ANY_FIELD_CONTAINS);
	state = g_action_get_state (G_ACTION (action));
	value = g_variant_get_int32 (state);
	g_clear_pointer (&state, g_variant_unref);

	if (value == TASK_SEARCH_ADVANCED) {
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
	}

	/* Apply selected filter. */
	combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);
	value = e_action_combo_box_get_current_value (combo_box);
	switch (value) {
		case TASK_FILTER_ANY_CATEGORY:
			break;

		case TASK_FILTER_UNMATCHED:
			temp = g_strdup_printf (
				"(and (has-categories? #f) %s)", query);
			g_free (query);
			query = temp;
			break;

		case TASK_FILTER_UNCOMPLETED_TASKS:
			temp = g_strdup_printf (
				"(and (not (is-completed?)) %s)", query);
			g_free (query);
			query = temp;
			break;

		case TASK_FILTER_NEXT_7_DAYS_TASKS:
			start_range = now_time;
			end_range = time_day_end (time_add_day (start_range, 7));
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
			start_range = now_time;
			end_range = time_day_end (time_add_day (start_range, 365));
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
			end_range = time_day_end (now_time);
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

		case TASK_FILTER_CANCELLED_TASKS:
			temp = g_strdup_printf (
				"(and " CALENDAR_CONFIG_CANCELLED_TASKS_SEXP " %s)", query);
			g_free (query);
			query = temp;
			break;

		case TASK_FILTER_SCHEDULED_TASKS:
			temp = g_strdup_printf ("(and (has-due?) %s)", query);
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

		case TASK_FILTER_STARTED:
			temp = g_strdup_printf (
				"(or (and %s (starts-before? "
				"(time-now))) "
				"(not (has-start?)))", query);
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

	if (value != TASK_FILTER_CANCELLED_TASKS &&
	    calendar_config_get_hide_cancelled_tasks ()) {
		temp = g_strdup_printf ("(and " CALENDAR_CONFIG_NOT_CANCELLED_TASKS_SEXP " %s)", query);
		g_free (query);
		query = temp;
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

	/* Submit the query. */
	e_cal_data_model_set_filter (data_model, query);
	g_free (query);

	preview_pane = e_task_shell_content_get_preview_pane (task_shell_content);

	web_view = e_preview_pane_get_web_view (preview_pane);
	task_preview = E_CAL_COMPONENT_PREVIEW (web_view);
	e_cal_component_preview_clear (task_preview);
}

static void
task_shell_view_update_actions (EShellView *shell_view)
{
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EUIAction *action;
	const gchar *label;
	gboolean sensitive;
	guint32 state;

	/* Be descriptive. */
	gboolean any_tasks_selected;
	gboolean has_primary_source;
	gboolean multiple_tasks_selected;
	gboolean primary_source_is_writable;
	gboolean primary_source_is_removable;
	gboolean primary_source_is_remote_deletable;
	gboolean primary_source_in_collection;
	gboolean selection_has_url;
	gboolean selection_is_assignable;
	gboolean single_task_selected;
	gboolean some_tasks_complete;
	gboolean some_tasks_incomplete;
	gboolean sources_are_editable;
	gboolean refresh_supported;
	gboolean all_sources_selected;
	gboolean clicked_source_is_primary;
	gboolean clicked_source_is_collection;

	/* Chain up to parent's update_actions() method. */
	E_SHELL_VIEW_CLASS (e_task_shell_view_parent_class)->update_actions (shell_view);

	shell_content = e_shell_view_get_shell_content (shell_view);
	state = e_shell_content_check_state (shell_content);

	if (e_task_shell_content_get_preview_visible (E_TASK_SHELL_CONTENT (shell_content))) {
		EPreviewPane *preview_pane;

		preview_pane = e_task_shell_content_get_preview_pane (E_TASK_SHELL_CONTENT (shell_content));
		e_web_view_update_actions (e_preview_pane_get_web_view (preview_pane));
	}

	single_task_selected =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_SINGLE);
	multiple_tasks_selected =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_MULTIPLE);
	selection_is_assignable =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_CAN_ASSIGN);
	sources_are_editable =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_EDITABLE);
	some_tasks_complete =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_HAS_COMPLETE);
	some_tasks_incomplete =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_HAS_INCOMPLETE);
	selection_has_url =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_HAS_URL);

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

	any_tasks_selected = (single_task_selected || multiple_tasks_selected);

	action = ACTION (TASK_LIST_SELECT_ALL);
	sensitive = clicked_source_is_primary && !all_sources_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_SELECT_ONE);
	sensitive = clicked_source_is_primary;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_ASSIGN);
	sensitive =
		single_task_selected && sources_are_editable &&
		selection_is_assignable;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_BULK_EDIT);
	sensitive = any_tasks_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_DELETE);
	sensitive = any_tasks_selected && sources_are_editable;
	e_ui_action_set_sensitive (action, sensitive);
	if (multiple_tasks_selected)
		label = _("Delete Tasks");
	else
		label = _("Delete Task");
	e_ui_action_set_label (action, label);

	action = ACTION (TASK_FIND);
	sensitive = single_task_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_FORWARD);
	sensitive = single_task_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_COPY);
	sensitive = has_primary_source;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_DELETE);
	sensitive =
		primary_source_is_removable ||
		primary_source_is_remote_deletable;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_PRINT);
	sensitive = has_primary_source;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_PRINT_PREVIEW);
	sensitive = has_primary_source;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_PROPERTIES);
	sensitive = clicked_source_is_primary && primary_source_is_writable;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_REFRESH);
	sensitive = clicked_source_is_primary && refresh_supported;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_REFRESH_BACKEND);
	sensitive = clicked_source_is_collection;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_RENAME);
	sensitive = clicked_source_is_primary &&
		primary_source_is_writable &&
		!primary_source_in_collection;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_MARK_COMPLETE);
	sensitive =
		any_tasks_selected &&
		sources_are_editable &&
		some_tasks_incomplete;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_MARK_INCOMPLETE);
	sensitive =
		any_tasks_selected &&
		sources_are_editable &&
		some_tasks_complete;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_OPEN);
	sensitive = single_task_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_OPEN_URL);
	sensitive = single_task_selected && selection_has_url;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_PRINT);
	sensitive = single_task_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_PURGE);
	sensitive = sources_are_editable;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_SAVE_AS);
	sensitive = single_task_selected;
	e_ui_action_set_sensitive (action, sensitive);
}

static void
task_shell_view_init_ui_data (EShellView *shell_view)
{
	g_return_if_fail (E_IS_TASK_SHELL_VIEW (shell_view));

	e_task_shell_view_actions_init (E_TASK_SHELL_VIEW (shell_view));
}

static void
task_shell_view_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CONFIRM_PURGE:
			e_task_shell_view_set_confirm_purge (
				E_TASK_SHELL_VIEW (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
task_shell_view_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CONFIRM_PURGE:
			g_value_set_boolean (
				value, e_task_shell_view_get_confirm_purge (
				E_TASK_SHELL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
task_shell_view_dispose (GObject *object)
{
	e_task_shell_view_private_dispose (E_TASK_SHELL_VIEW (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_task_shell_view_parent_class)->dispose (object);
}

static void
task_shell_view_finalize (GObject *object)
{
	e_task_shell_view_private_finalize (E_TASK_SHELL_VIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_task_shell_view_parent_class)->finalize (object);
}

static void
task_shell_view_constructed (GObject *object)
{
	EUIManager *ui_manager;
	EUICustomizer *customizer;

	ui_manager = e_shell_view_get_ui_manager (E_SHELL_VIEW (object));

	e_ui_manager_freeze (ui_manager);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_task_shell_view_parent_class)->constructed (object);

	e_task_shell_view_private_constructed (E_TASK_SHELL_VIEW (object));

	e_ui_manager_thaw (ui_manager);

	customizer = e_ui_manager_get_customizer (ui_manager);
	e_ui_customizer_register (customizer, "task-popup", _("Task Context Menu"));
	e_ui_customizer_register (customizer, "task-list-popup", _("Task List Context Menu"));
}

static void
e_task_shell_view_class_init (ETaskShellViewClass *class)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;
	ECalBaseShellViewClass *cal_base_shell_view_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = task_shell_view_set_property;
	object_class->get_property = task_shell_view_get_property;
	object_class->dispose = task_shell_view_dispose;
	object_class->finalize = task_shell_view_finalize;
	object_class->constructed = task_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = _("Tasks");
	shell_view_class->icon_name = "evolution-tasks";
	shell_view_class->ui_definition = "evolution-tasks.eui";
	shell_view_class->ui_manager_id = "org.gnome.evolution.tasks";
	shell_view_class->search_rules = "tasktypes.xml";
	shell_view_class->new_shell_content = e_task_shell_content_new;
	shell_view_class->new_shell_sidebar = e_cal_base_shell_sidebar_new;
	shell_view_class->execute_search = task_shell_view_execute_search;
	shell_view_class->update_actions = task_shell_view_update_actions;
	shell_view_class->init_ui_data = task_shell_view_init_ui_data;

	cal_base_shell_view_class = E_CAL_BASE_SHELL_VIEW_CLASS (class);
	cal_base_shell_view_class->source_type = E_CAL_CLIENT_SOURCE_TYPE_TASKS;

	g_object_class_install_property (
		object_class,
		PROP_CONFIRM_PURGE,
		g_param_spec_boolean (
			"confirm-purge",
			"Confirm Purge",
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	/* Ensure the GalView types we need are registered. */
	g_type_ensure (GAL_TYPE_VIEW_ETABLE);
}

static void
e_task_shell_view_class_finalize (ETaskShellViewClass *class)
{
}

static void
e_task_shell_view_init (ETaskShellView *task_shell_view)
{
	task_shell_view->priv = e_task_shell_view_get_instance_private (task_shell_view);

	e_task_shell_view_private_init (task_shell_view);
}

void
e_task_shell_view_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_task_shell_view_register_type (type_module);
}

gboolean
e_task_shell_view_get_confirm_purge (ETaskShellView *task_shell_view)
{
	g_return_val_if_fail (E_IS_TASK_SHELL_VIEW (task_shell_view), FALSE);

	return task_shell_view->priv->confirm_purge;
}

void
e_task_shell_view_set_confirm_purge (ETaskShellView *task_shell_view,
                                     gboolean confirm_purge)
{
	g_return_if_fail (E_IS_TASK_SHELL_VIEW (task_shell_view));

	if (task_shell_view->priv->confirm_purge == confirm_purge)
		return;

	task_shell_view->priv->confirm_purge = confirm_purge;

	g_object_notify (G_OBJECT (task_shell_view), "confirm-purge");
}
