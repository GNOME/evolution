/*
 * e-task-shell-view.c
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

enum {
	PROP_0,
	PROP_CONFIRM_PURGE
};

static gpointer parent_class;
static GType task_shell_view_type;

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
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
task_shell_view_finalize (GObject *object)
{
	e_task_shell_view_private_finalize (E_TASK_SHELL_VIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
task_shell_view_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	e_task_shell_view_private_constructed (E_TASK_SHELL_VIEW (object));
}

static void
task_shell_view_execute_search (EShellView *shell_view)
{
	ETaskShellContent *task_shell_content;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	GtkRadioAction *action;
	ECalComponentPreview *task_preview;
	EPreviewPane *preview_pane;
	ETaskTable *task_table;
	EWebView *web_view;
	ECalModel *model;
	time_t start_range;
	time_t end_range;
	time_t now_time;
	gchar *start, *end;
	gchar *query;
	gchar *temp;
	gint value;

	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	task_shell_content = E_TASK_SHELL_CONTENT (shell_content);
	searchbar = e_task_shell_content_get_searchbar (task_shell_content);

	task_shell_content = E_TASK_SHELL_CONTENT (shell_content);
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	model = e_task_table_get_model (task_table);
	now_time = time_day_begin (icaltime_as_timet (icaltime_current_time_with_zone (e_cal_model_get_timezone (model))));

	action = GTK_RADIO_ACTION (ACTION (TASK_SEARCH_ANY_FIELD_CONTAINS));
	value = gtk_radio_action_get_current_value (action);

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

			categories = e_util_get_searchable_categories ();
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

	/* Submit the query. */
	e_cal_model_set_search_query (model, query);
	g_free (query);

	preview_pane =
		e_task_shell_content_get_preview_pane (task_shell_content);

	web_view = e_preview_pane_get_web_view (preview_pane);
	task_preview = E_CAL_COMPONENT_PREVIEW (web_view);
	e_cal_component_preview_clear (task_preview);
}

static void
task_shell_view_update_actions (EShellView *shell_view)
{
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	GtkAction *action;
	const gchar *label;
	gboolean sensitive;
	guint32 state;

	/* Be descriptive. */
	gboolean any_tasks_selected;
	gboolean can_delete_primary_source;
	gboolean has_primary_source;
	gboolean multiple_tasks_selected;
	gboolean selection_has_url;
	gboolean selection_is_assignable;
	gboolean single_task_selected;
	gboolean some_tasks_complete;
	gboolean some_tasks_incomplete;
	gboolean sources_are_editable;
	gboolean refresh_supported;

	/* Chain up to parent's update_actions() method. */
	E_SHELL_VIEW_CLASS (parent_class)->update_actions (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);

	shell_content = e_shell_view_get_shell_content (shell_view);
	state = e_shell_content_check_state (shell_content);

	single_task_selected =
		(state & E_TASK_SHELL_CONTENT_SELECTION_SINGLE);
	multiple_tasks_selected =
		(state & E_TASK_SHELL_CONTENT_SELECTION_MULTIPLE);
	selection_is_assignable =
		(state & E_TASK_SHELL_CONTENT_SELECTION_CAN_ASSIGN);
	sources_are_editable =
		(state & E_TASK_SHELL_CONTENT_SELECTION_CAN_EDIT);
	some_tasks_complete =
		(state & E_TASK_SHELL_CONTENT_SELECTION_HAS_COMPLETE);
	some_tasks_incomplete =
		(state & E_TASK_SHELL_CONTENT_SELECTION_HAS_INCOMPLETE);
	selection_has_url =
		(state & E_TASK_SHELL_CONTENT_SELECTION_HAS_URL);

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	state = e_shell_sidebar_check_state (shell_sidebar);

	has_primary_source =
		(state & E_TASK_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE);
	can_delete_primary_source =
		(state & E_TASK_SHELL_SIDEBAR_CAN_DELETE_PRIMARY_SOURCE);
	refresh_supported =
		(state & E_TASK_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH);

	any_tasks_selected =
		(single_task_selected || multiple_tasks_selected);

	action = ACTION (TASK_ASSIGN);
	sensitive =
		single_task_selected && sources_are_editable &&
		selection_is_assignable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_DELETE);
	sensitive = any_tasks_selected && sources_are_editable;
	gtk_action_set_sensitive (action, sensitive);
	if (multiple_tasks_selected)
		label = _("Delete Tasks");
	else
		label = _("Delete Task");
	g_object_set (action, "label", label, NULL);

	action = ACTION (TASK_FIND);
	sensitive = single_task_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_FORWARD);
	sensitive = single_task_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_COPY);
	sensitive = has_primary_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_DELETE);
	sensitive = can_delete_primary_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_PROPERTIES);
	sensitive = has_primary_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_REFRESH);
	sensitive = refresh_supported;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_RENAME);
	sensitive = can_delete_primary_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_MARK_COMPLETE);
	sensitive =
		any_tasks_selected &&
		sources_are_editable &&
		some_tasks_incomplete;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_MARK_INCOMPLETE);
	sensitive =
		any_tasks_selected &&
		sources_are_editable &&
		some_tasks_complete;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_OPEN);
	sensitive = single_task_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_OPEN_URL);
	sensitive = single_task_selected && selection_has_url;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_PRINT);
	sensitive = single_task_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_PURGE);
	sensitive = sources_are_editable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_SAVE_AS);
	sensitive = single_task_selected;
	gtk_action_set_sensitive (action, sensitive);
}

static void
task_shell_view_class_init (ETaskShellViewClass *class,
                            GTypeModule *type_module)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ETaskShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = task_shell_view_set_property;
	object_class->get_property = task_shell_view_get_property;
	object_class->dispose = task_shell_view_dispose;
	object_class->finalize = task_shell_view_finalize;
	object_class->constructed = task_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = _("Tasks");
	shell_view_class->icon_name = "evolution-tasks";
	shell_view_class->ui_definition = "evolution-tasks.ui";
	shell_view_class->ui_manager_id = "org.gnome.evolution.tasks";
	shell_view_class->search_options = "/task-search-options";
	shell_view_class->search_rules = "tasktypes.xml";
	shell_view_class->new_shell_content = e_task_shell_content_new;
	shell_view_class->new_shell_sidebar = e_task_shell_sidebar_new;
	shell_view_class->execute_search = task_shell_view_execute_search;
	shell_view_class->update_actions = task_shell_view_update_actions;

	g_object_class_install_property (
		object_class,
		PROP_CONFIRM_PURGE,
		g_param_spec_boolean (
			"confirm-purge",
			"Confirm Purge",
			NULL,
			TRUE,
			G_PARAM_READWRITE));
}

static void
task_shell_view_init (ETaskShellView *task_shell_view,
                      EShellViewClass *shell_view_class)
{
	task_shell_view->priv =
		E_TASK_SHELL_VIEW_GET_PRIVATE (task_shell_view);

	e_task_shell_view_private_init (task_shell_view, shell_view_class);
}

GType
e_task_shell_view_get_type (void)
{
	return task_shell_view_type;
}

void
e_task_shell_view_register_type (GTypeModule *type_module)
{
	const GTypeInfo type_info = {
		sizeof (ETaskShellViewClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) task_shell_view_class_init,
		(GClassFinalizeFunc) NULL,
		type_module,
		sizeof (ETaskShellView),
		0,    /* n_preallocs */
		(GInstanceInitFunc) task_shell_view_init,
		NULL  /* value_table */
	};

	task_shell_view_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_VIEW,
		"ETaskShellView", &type_info, 0);
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

	task_shell_view->priv->confirm_purge = confirm_purge;

	g_object_notify (G_OBJECT (task_shell_view), "confirm-purge");
}
