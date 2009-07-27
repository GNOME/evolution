/*
 * e-memo-shell-view-private.c
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

#include "e-memo-shell-view-private.h"

#include "widgets/menus/gal-view-factory-etable.h"

static void
memo_shell_view_table_popup_event_cb (EShellView *shell_view,
                                      GdkEventButton *event)
{
	const gchar *widget_path;

	widget_path = "/memo-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, event);
}

static void
memo_shell_view_table_user_created_cb (EMemoShellView *memo_shell_view,
                                       EMemoTable *memo_table)
{
	EMemoShellSidebar *memo_shell_sidebar;
	ECalModel *model;
	ECal *client;
	ESource *source;

	/* This is the "Click to Add" handler. */

	model = e_memo_table_get_model (memo_table);
	client = e_cal_model_get_default_client (model);
	source = e_cal_get_source (client);

	memo_shell_sidebar = memo_shell_view->priv->memo_shell_sidebar;
	e_memo_shell_sidebar_add_source (memo_shell_sidebar, source);

	e_cal_model_add_client (model, client);
}

static void
memo_shell_view_selector_client_added_cb (EMemoShellView *memo_shell_view,
                                          ECal *client)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	ECalModel *model;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	model = e_memo_table_get_model (memo_table);

	e_cal_model_add_client (model, client);
}

static void
memo_shell_view_selector_client_removed_cb (EMemoShellView *memo_shell_view,
                                            ECal *client)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	ECalModel *model;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	model = e_memo_table_get_model (memo_table);

	e_cal_model_remove_client (model, client);
}

static gboolean
memo_shell_view_selector_popup_event_cb (EShellView *shell_view,
                                         ESource *primary_source,
                                         GdkEventButton *event)
{
	const gchar *widget_path;

	widget_path = "/memo-list-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, event);

	return TRUE;
}

static void
memo_shell_view_load_view_collection (EShellViewClass *shell_view_class)
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
                            "for memos");
	g_free (filename);

	factory = gal_view_factory_etable_new (spec);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);
	g_object_unref (spec);

	gal_view_collection_load (collection);
}

static void
memo_shell_view_notify_view_id_cb (EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	GalViewInstance *view_instance;
	const gchar *view_id;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	view_instance =
		e_memo_shell_content_get_view_instance (memo_shell_content);
	view_id = e_shell_view_get_view_id (E_SHELL_VIEW (memo_shell_view));

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
e_memo_shell_view_private_init (EMemoShellView *memo_shell_view,
                                EShellViewClass *shell_view_class)
{
	if (!gal_view_collection_loaded (shell_view_class->view_collection))
		memo_shell_view_load_view_collection (shell_view_class);

	g_signal_connect (
		memo_shell_view, "notify::view-id",
		G_CALLBACK (memo_shell_view_notify_view_id_cb), NULL);
}

void
e_memo_shell_view_private_constructed (EMemoShellView *memo_shell_view)
{
	EMemoShellViewPrivate *priv = memo_shell_view->priv;
	EMemoShellContent *memo_shell_content;
	EMemoShellSidebar *memo_shell_sidebar;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	EMemoTable *memo_table;
	ECalModel *model;
	ETable *table;
	ESourceSelector *selector;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	e_shell_window_add_action_group (shell_window, "memos");
	e_shell_window_add_action_group (shell_window, "memos-filter");

	/* Cache these to avoid lots of awkward casting. */
	priv->memo_shell_backend = g_object_ref (shell_backend);
	priv->memo_shell_content = g_object_ref (shell_content);
	priv->memo_shell_sidebar = g_object_ref (shell_sidebar);

	memo_shell_content = E_MEMO_SHELL_CONTENT (shell_content);
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	model = e_memo_table_get_model (memo_table);
	table = e_memo_table_get_table (memo_table);

	memo_shell_sidebar = E_MEMO_SHELL_SIDEBAR (shell_sidebar);
	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);

	g_signal_connect_swapped (
		memo_table, "open-component",
		G_CALLBACK (e_memo_shell_view_open_memo),
		memo_shell_view);

	g_signal_connect_swapped (
		memo_table, "popup-event",
		G_CALLBACK (memo_shell_view_table_popup_event_cb),
		memo_shell_view);

	g_signal_connect_swapped (
		memo_table, "status-message",
		G_CALLBACK (e_memo_shell_view_set_status_message),
		memo_shell_view);

	g_signal_connect_swapped (
		memo_table, "user-created",
		G_CALLBACK (memo_shell_view_table_user_created_cb),
		memo_shell_view);

	g_signal_connect_swapped (
		model, "model-changed",
		G_CALLBACK (e_memo_shell_view_update_sidebar),
		memo_shell_view);

	g_signal_connect_swapped (
		model, "model-rows-deleted",
		G_CALLBACK (e_memo_shell_view_update_sidebar),
		memo_shell_view);

	g_signal_connect_swapped (
		model, "model-rows-inserted",
		G_CALLBACK (e_memo_shell_view_update_sidebar),
		memo_shell_view);

	g_signal_connect_swapped (
		table, "selection-change",
		G_CALLBACK (e_memo_shell_view_update_sidebar),
		memo_shell_view);

	g_signal_connect_swapped (
		memo_shell_sidebar, "client-added",
		G_CALLBACK (memo_shell_view_selector_client_added_cb),
		memo_shell_view);

	g_signal_connect_swapped (
		memo_shell_sidebar, "client-removed",
		G_CALLBACK (memo_shell_view_selector_client_removed_cb),
		memo_shell_view);

	g_signal_connect_swapped (
		memo_shell_sidebar, "status-message",
		G_CALLBACK (e_memo_shell_view_set_status_message),
		memo_shell_view);

	g_signal_connect_swapped (
		selector, "popup-event",
		G_CALLBACK (memo_shell_view_selector_popup_event_cb),
		memo_shell_view);

	g_signal_connect_swapped (
		selector, "primary-selection-changed",
		G_CALLBACK (e_shell_view_update_actions),
		memo_shell_view);

	e_categories_register_change_listener (
		G_CALLBACK (e_memo_shell_view_update_search_filter),
		memo_shell_view);

	e_memo_shell_view_actions_init (memo_shell_view);
	e_memo_shell_view_update_sidebar (memo_shell_view);
	e_memo_shell_view_update_search_filter (memo_shell_view);

	e_memo_shell_view_execute_search (memo_shell_view);
}

void
e_memo_shell_view_private_dispose (EMemoShellView *memo_shell_view)
{
	EMemoShellViewPrivate *priv = memo_shell_view->priv;

	DISPOSE (priv->memo_shell_backend);
	DISPOSE (priv->memo_shell_content);
	DISPOSE (priv->memo_shell_sidebar);

	if (memo_shell_view->priv->activity != NULL) {
		/* XXX Activity is not cancellable. */
		e_activity_complete (memo_shell_view->priv->activity);
		g_object_unref (memo_shell_view->priv->activity);
		memo_shell_view->priv->activity = NULL;
	}
}

void
e_memo_shell_view_private_finalize (EMemoShellView *memo_shell_view)
{
	/* XXX Nothing to do? */
}

void
e_memo_shell_view_execute_search (EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	GtkRadioAction *action;
	GString *string;
	ECalComponentPreview *memo_preview;
	EMemoTable *memo_table;
	ECalModel *model;
	FilterRule *rule;
	const gchar *format;
	const gchar *text;
	gchar *query;
	gchar *temp;
	gint value;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	text = e_shell_content_get_search_text (shell_content);

	shell_window = e_shell_view_get_shell_window (shell_view);
	action = GTK_RADIO_ACTION (ACTION (MEMO_SEARCH_ANY_FIELD_CONTAINS));
	value = gtk_radio_action_get_current_value (action);

	if (text == NULL || *text == '\0') {
		text = "";
		value = MEMO_SEARCH_SUMMARY_CONTAINS;
	}

	switch (value) {
		default:
			text = "";
			/* fall through */

		case MEMO_SEARCH_SUMMARY_CONTAINS:
			format = "(contains? \"summary\" %s)";
			break;

		case MEMO_SEARCH_DESCRIPTION_CONTAINS:
			format = "(contains? \"description\" %s)";
			break;

		case MEMO_SEARCH_ANY_FIELD_CONTAINS:
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
		case MEMO_FILTER_ANY_CATEGORY:
			break;

		case MEMO_FILTER_UNMATCHED:
			temp = g_strdup_printf (
				"(and (has-categories? #f) %s", query);
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
		}
	}

	/* XXX This is wrong.  We need to programmatically construct a
	 *     FilterRule, tell it to build code, and pass the resulting
	 *     expression string to ECalModel. */
	rule = filter_rule_new ();
	e_shell_content_set_search_rule (shell_content, rule);
	g_object_unref (rule);

	/* Submit the query. */
	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	model = e_memo_table_get_model (memo_table);
	e_cal_model_set_search_query (model, query);
	g_free (query);

	memo_preview =
		e_memo_shell_content_get_memo_preview (memo_shell_content);
	e_cal_component_preview_clear (memo_preview);
}

void
e_memo_shell_view_open_memo (EMemoShellView *memo_shell_view,
                             ECalModelComponent *comp_data)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	CompEditor *editor;
	CompEditorFlags flags = 0;
	ECalComponent *comp;
	icalcomponent *clone;
	const gchar *uid;

	g_return_if_fail (E_IS_MEMO_SHELL_VIEW (memo_shell_view));
	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	uid = icalcomponent_get_uid (comp_data->icalcomp);
	editor = comp_editor_find_instance (uid);

	if (editor != NULL)
		goto exit;

	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	e_cal_component_set_icalcomponent (comp, clone);

	if (e_cal_component_has_organizer (comp))
		flags |= COMP_EDITOR_IS_SHARED;

	if (itip_organizer_is_user (comp, comp_data->client))
		flags |= COMP_EDITOR_USER_ORG;

	editor = memo_editor_new (comp_data->client, shell, flags);
	comp_editor_edit_comp (editor, comp);

	g_object_unref (comp);

exit:
	gtk_window_present (GTK_WINDOW (editor));
}

void
e_memo_shell_view_set_status_message (EMemoShellView *memo_shell_view,
                                      const gchar *status_message,
                                      gdouble percent)
{
	EActivity *activity;
	EShellView *shell_view;
	EShellBackend *shell_backend;

	g_return_if_fail (E_IS_MEMO_SHELL_VIEW (memo_shell_view));

	activity = memo_shell_view->priv->activity;
	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	if (status_message == NULL || *status_message == '\0') {
		if (activity != NULL) {
			e_activity_complete (activity);
			g_object_unref (activity);
			activity = NULL;
		}

	} else if (activity == NULL) {
		activity = e_activity_new (status_message);
		e_activity_set_percent (activity, percent);
		e_shell_backend_add_activity (shell_backend, activity);

	} else {
		e_activity_set_percent (activity, percent);
		e_activity_set_primary_text (activity, status_message);
	}

	memo_shell_view->priv->activity = activity;
}

void
e_memo_shell_view_update_sidebar (EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	EMemoTable *memo_table;
	ECalModel *model;
	ETable *table;
	GString *string;
	const gchar *format;
	gint n_rows;
	gint n_selected;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	model = e_memo_table_get_model (memo_table);
	table = e_memo_table_get_table (memo_table);

	n_rows = e_table_model_row_count (E_TABLE_MODEL (model));
	n_selected = e_table_selected_count (table);

	string = g_string_sized_new (64);

	format = ngettext ("%d memo", "%d memos", n_rows);
	g_string_append_printf (string, format, n_rows);

	if (n_selected > 0) {
		format = _("%d selected");
		g_string_append_len (string, ", ", 2);
		g_string_append_printf (string, format, n_selected);
	}

	e_shell_sidebar_set_secondary_text (shell_sidebar, string->str);

	g_string_free (string, TRUE);
}
