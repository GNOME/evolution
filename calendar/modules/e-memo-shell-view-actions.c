/*
 * e-memo-shell-view-actions.c
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

static void
action_gal_save_custom_view_cb (GtkAction *action,
                                EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EShellView *shell_view;
	GalViewInstance *view_instance;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	if (!e_shell_view_is_active (shell_view))
		return;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	view_instance = e_memo_shell_content_get_view_instance (memo_shell_content);
	gal_view_instance_save_as (view_instance);
}

static void
action_memo_clipboard_copy_cb (GtkAction *action,
                               EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	e_memo_table_copy_clipboard (memo_table);
}

static void
action_memo_clipboard_cut_cb (GtkAction *action,
                              EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	e_memo_table_cut_clipboard (memo_table);
}

static void
action_memo_clipboard_paste_cb (GtkAction *action,
                                EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	e_memo_table_paste_clipboard (memo_table);
}

static void
action_memo_delete_cb (GtkAction *action,
                       EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoPreview *memo_preview;
	EMemoTable *memo_table;
	const gchar *status_message;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	memo_preview = e_memo_shell_content_get_memo_preview (memo_shell_content);

	status_message = _("Deleting selected memos...");
	e_memo_shell_view_set_status_message (memo_shell_view, status_message);
	e_memo_table_delete_selected (memo_table);
	e_memo_shell_view_set_status_message (memo_shell_view, NULL);

	e_memo_preview_clear (memo_preview);
}

static void
action_memo_forward_cb (GtkAction *action,
                        EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	ECalComponent *comp;
	icalcomponent *clone;
	ECalComponentItipMethod method;
	GSList *list;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only forward the first selected memo. */
	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	method = E_CAL_COMPONENT_METHOD_PUBLISH;
	e_cal_component_set_icalcomponent (comp, clone);
	itip_send_comp (method, comp, comp_data->client, NULL, NULL, NULL);
	g_object_unref (comp);
}

static void
action_memo_list_copy_cb (GtkAction *action,
                          EMemoShellView *memo_shell_view)
{
	EMemoShellSidebar *memo_shell_sidebar;
	EShellWindow *shell_window;
	EShellView *shell_view;
	ESourceSelector *selector;
	ESource *source;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	memo_shell_sidebar = memo_shell_view->priv->memo_shell_sidebar;
	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);
	source = e_source_selector_peek_primary_selection (selector);
	g_return_if_fail (E_IS_SOURCE (source));

	copy_source_dialog (
		GTK_WINDOW (shell_window),
		source, E_CAL_SOURCE_TYPE_JOURNAL);
}

static void
action_memo_list_delete_cb (GtkAction *action,
                            EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoShellSidebar *memo_shell_sidebar;
	EShellWindow *shell_window;
	EShellView *shell_view;
	EMemoTable *memo_table;
	ECal *client;
	ECalModel *model;
	ESourceSelector *selector;
	ESourceGroup *source_group;
	ESourceList *source_list;
	ESource *source;
	gint response;
	gchar *uri;
	GError *error = NULL;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	model = e_memo_table_get_model (memo_table);

	memo_shell_sidebar = memo_shell_view->priv->memo_shell_sidebar;
	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);
	source = e_source_selector_peek_primary_selection (selector);
	g_return_if_fail (E_IS_SOURCE (source));

	/* Ask for confirmation. */
	response = e_error_run (
		GTK_WINDOW (shell_window),
		"calendar:prompt-delete-memo-list",
		e_source_peek_name (source));
	if (response != GTK_RESPONSE_YES)
		return;

	uri = e_source_get_uri (source);
	client = e_cal_model_get_client_for_uri (model, uri);
	if (client == NULL)
		client = e_cal_new_from_uri (uri, E_CAL_SOURCE_TYPE_JOURNAL);
	g_free (uri);

	g_return_if_fail (client != NULL);

	if (!e_cal_remove (client, &error)) {
		g_warning ("%s", error->message);
		g_error_free (error);
		return;
	}

	if (e_source_selector_source_is_selected (selector, source)) {
		e_memo_shell_sidebar_remove_source (
			memo_shell_sidebar, source);
		e_source_selector_unselect_source (selector, source);
	}

	source_group = e_source_peek_group (source);
	e_source_group_remove_source (source_group, source);

	source_list = memo_shell_view->priv->source_list;
	if (!e_source_list_sync (source_list, &error)) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
action_memo_list_new_cb (GtkAction *action,
                         EMemoShellView *memo_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	calendar_setup_new_memo_list (GTK_WINDOW (shell_window));
}

static void
action_memo_list_print_cb (GtkAction *action,
                           EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	ETable *table;
	GtkPrintOperationAction print_action;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	table = e_memo_table_get_table (memo_table);

	print_action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;
	print_table (table, _("Print Memos"), _("Memos"), print_action);
}

static void
action_memo_list_print_preview_cb (GtkAction *action,
                                   EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	ETable *table;
	GtkPrintOperationAction print_action;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	table = e_memo_table_get_table (memo_table);

	print_action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;
	print_table (table, _("Print Memos"), _("Memos"), print_action);
}

static void
action_memo_list_properties_cb (GtkAction *action,
                                EMemoShellView *memo_shell_view)
{
	EMemoShellSidebar *memo_shell_sidebar;
	EShellView *shell_view;
	EShellWindow *shell_window;
	ESource *source;
	ESourceSelector *selector;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	memo_shell_sidebar = memo_shell_view->priv->memo_shell_sidebar;
	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);
	source = e_source_selector_peek_primary_selection (selector);
	g_return_if_fail (E_IS_SOURCE (source));

	calendar_setup_edit_memo_list (GTK_WINDOW (shell_window), source);
}

static void
action_memo_list_select_one_cb (GtkAction *action,
                                EMemoShellView *memo_shell_view)
{
	EMemoShellSidebar *memo_shell_sidebar;
	ESourceSelector *selector;
	ESource *primary;
	GSList *list, *iter;

	memo_shell_sidebar = memo_shell_view->priv->memo_shell_sidebar;
	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);
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
action_memo_new_cb (GtkAction *action,
                    EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	ECal *client;
	ECalComponent *comp;
	CompEditor *editor;
	GSList *list;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	client = comp_data->client;
	editor = memo_editor_new (client, COMP_EDITOR_NEW_ITEM);
	comp = cal_comp_memo_new_with_defaults (client);
	comp_editor_edit_comp (editor, comp);

	gtk_window_present (GTK_WINDOW (editor));

	g_object_unref (comp);
	g_object_unref (client);
}

static void
action_memo_open_cb (GtkAction *action,
                     EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	GSList *list;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only open the first selected memo. */
	e_memo_shell_view_open_memo (memo_shell_view, comp_data);
}

static void
action_memo_open_url_cb (GtkAction *action,
                         EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	icalproperty *prop;
	GdkScreen *screen;
	const gchar *uri;
	GSList *list;
	GError *error = NULL;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;

	/* XXX We only open the URI of the first selected memo. */
	prop = icalcomponent_get_first_property (
		comp_data->icalcomp, ICAL_URL_PROPERTY);
	g_return_if_fail (prop == NULL);

	screen = gtk_widget_get_screen (GTK_WIDGET (memo_shell_view));
	uri = icalproperty_get_url (prop);
	gtk_show_uri (screen, uri, GDK_CURRENT_TIME, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
action_memo_preview_cb (GtkToggleAction *action,
                        EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	gboolean visible;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	visible = gtk_toggle_action_get_active (action);
	e_memo_shell_content_set_preview_visible (memo_shell_content, visible);
}

static void
action_memo_print_cb (GtkAction *action,
                      EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	ECalComponent *comp;
	icalcomponent *clone;
	GtkPrintOperationAction print_action;
	GSList *list;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only print the first selected memo. */
	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	print_action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;
	e_cal_component_set_icalcomponent (comp, clone);
	print_comp (comp, comp_data->client, print_action);
	g_object_unref (comp);
}

static void
action_memo_save_as_cb (GtkAction *action,
                        EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	GSList *list;
	gchar *filename;
	gchar *string;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	filename = e_file_dialog_save (_("Save as..."), NULL);
	if (filename == NULL)
		return;

	string = e_cal_get_component_as_string (
		comp_data->client, comp_data->icalcomp);
	if (string == NULL) {
		g_warning ("Could not convert memo to a string");
		return;
	}

	e_write_file_uri (filename, string);

	g_free (filename);
	g_free (string);
}

static void
action_search_execute_cb (GtkAction *action,
                          EMemoShellView *memo_shell_view)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	if (!e_shell_view_is_active (shell_view))
		return;

	e_memo_shell_view_execute_search (memo_shell_view);
}

static void
action_search_filter_cb (GtkRadioAction *action,
                         GtkRadioAction *current,
                         EMemoShellView *memo_shell_view)
{
	e_memo_shell_view_execute_search (memo_shell_view);
}

static GtkActionEntry memo_entries[] = {

	{ "memo-clipboard-copy",
	  GTK_STOCK_COPY,
	  NULL,
	  NULL,
	  N_("Copy selected memo"),
	  G_CALLBACK (action_memo_clipboard_copy_cb) },

	{ "memo-clipboard-cut",
	  GTK_STOCK_CUT,
	  NULL,
	  NULL,
	  N_("Cut selected memo"),
	  G_CALLBACK (action_memo_clipboard_cut_cb) },

	{ "memo-clipboard-paste",
	  GTK_STOCK_PASTE,
	  NULL,
	  NULL,
	  N_("Paste memo from the clipboard"),
	  G_CALLBACK (action_memo_clipboard_paste_cb) },

	{ "memo-delete",
	  GTK_STOCK_DELETE,
	  N_("Delete Memo"),
	  NULL,
	  N_("Delete selected memos"),
	  G_CALLBACK (action_memo_delete_cb) },

	{ "memo-forward",
	  "mail-forward",
	  N_("_Forward as iCalendar"),
	  "<Control>f",
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_forward_cb) },

	{ "memo-list-copy",
	  GTK_STOCK_COPY,
	  N_("_Copy..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_list_copy_cb) },

	{ "memo-list-delete",
	  GTK_STOCK_DELETE,
	  N_("_Delete"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_list_delete_cb) },

	{ "memo-list-new",
	  "stock_notes",
	  N_("_New Memo List"),
	  NULL,
	  N_("Create a new memo list"),
	  G_CALLBACK (action_memo_list_new_cb) },

	{ "memo-list-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  NULL,
	  N_("Print the list of memos"),
	  G_CALLBACK (action_memo_list_print_cb) },

	{ "memo-list-print-preview",
	  GTK_STOCK_PRINT_PREVIEW,
	  NULL,
	  NULL,
	  N_("Preview the list of memos to be printed"),
	  G_CALLBACK (action_memo_list_print_preview_cb) },

	{ "memo-list-properties",
	  GTK_STOCK_PROPERTIES,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_list_properties_cb) },

	{ "memo-list-select-one",
	  "stock_check-filled",
	  N_("Show _Only This Memo List"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_list_select_one_cb) },

	{ "memo-new",
	  "stock_insert-note",
	  N_("New _Memo"),
	  NULL,
	  N_("Create a new memo"),
	  G_CALLBACK (action_memo_new_cb) },

	{ "memo-open",
	  GTK_STOCK_OPEN,
	  N_("_Open Memo"),
	  "<Control>o",
	  N_("View the selected memo"),
	  G_CALLBACK (action_memo_open_cb) },

	{ "memo-open-url",
	  "applications-internet",
	  N_("Open _Web Page"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_open_url_cb) },

	{ "memo-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  NULL,
	  N_("Print the selected memo"),
	  G_CALLBACK (action_memo_print_cb) },

	{ "memo-save-as",
	  GTK_STOCK_SAVE_AS,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_save_as_cb) }
};

static GtkToggleActionEntry memo_toggle_entries[] = {

	{ "memo-preview",
	  NULL,
	  N_("Memo _Preview"),
	  "<Control>m",
	  N_("Show memo preview pane"),
	  G_CALLBACK (action_memo_preview_cb),
	  TRUE }
};

static GtkRadioActionEntry memo_filter_entries[] = {

	{ "memo-filter-any-category",
	  NULL,
	  N_("Any Category"),
	  NULL,
	  NULL,
	  MEMO_FILTER_ANY_CATEGORY },

	{ "memo-filter-unmatched",
	  NULL,
	  N_("Unmatched"),
	  NULL,
	  NULL,
	  MEMO_FILTER_UNMATCHED }
};

static GtkRadioActionEntry memo_search_entries[] = {

	{ "memo-search-any-field-contains",
	  NULL,
	  N_("Any field contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MEMO_SEARCH_ANY_FIELD_CONTAINS },

	{ "memo-search-description-contains",
	  NULL,
	  N_("Description contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MEMO_SEARCH_DESCRIPTION_CONTAINS },

	{ "memo-search-summary-contains",
	  NULL,
	  N_("Summary contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MEMO_SEARCH_SUMMARY_CONTAINS }
};

void
e_memo_shell_view_actions_init (EMemoShellView *memo_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	GConfBridge *bridge;
	GtkAction *action;
	GObject *object;
	const gchar *domain;
	const gchar *key;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	manager = e_shell_window_get_ui_manager (shell_window);
	domain = GETTEXT_PACKAGE;

	action_group = memo_shell_view->priv->memo_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, memo_entries,
		G_N_ELEMENTS (memo_entries), memo_shell_view);
	gtk_action_group_add_toggle_actions (
		action_group, memo_toggle_entries,
		G_N_ELEMENTS (memo_toggle_entries), memo_shell_view);
	gtk_action_group_add_radio_actions (
		action_group, memo_search_entries,
		G_N_ELEMENTS (memo_search_entries),
		MEMO_SEARCH_SUMMARY_CONTAINS,
		NULL, NULL);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (ACTION (MEMO_PREVIEW));
	key = "/apps/evolution/calendar/display/show_memo_preview";
	gconf_bridge_bind_property (bridge, key, object, "active");

	/* Fine tuning. */

	action = ACTION (MEMO_DELETE);
	g_object_set (action, "short-label", _("Delete"), NULL);

	g_signal_connect (
		ACTION (GAL_SAVE_CUSTOM_VIEW), "activate",
		G_CALLBACK (action_gal_save_custom_view_cb), memo_shell_view);

	g_signal_connect (
		ACTION (SEARCH_EXECUTE), "activate",
		G_CALLBACK (action_search_execute_cb), memo_shell_view);
}

void
e_memo_shell_view_update_search_filter (EMemoShellView *memo_shell_view)
{
	EShellContent *shell_content;
	EShellView *shell_view;
	GtkActionGroup *action_group;
	GtkRadioAction *radio_action;
	GList *list, *iter;
	GSList *group;
	gint ii;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	action_group = memo_shell_view->priv->filter_actions;

	e_action_group_remove_all_actions (action_group);

	/* Add the standard filter actions. */
	gtk_action_group_add_radio_actions (
		action_group, memo_filter_entries,
		G_N_ELEMENTS (memo_filter_entries),
		MEMO_FILTER_ANY_CATEGORY,
		G_CALLBACK (action_search_filter_cb),
		memo_shell_view);

	/* Retrieve the radio group from an action we just added. */
	list = gtk_action_group_list_actions (action_group);
	radio_action = GTK_RADIO_ACTION (list->data);
	group = gtk_radio_action_get_group (radio_action);
	g_list_free (list);

	/* Build the category actions. */

	list = e_categories_get_list ();
	for (iter = list, ii = 0; iter != NULL; iter = iter->next, ii++) {
		const gchar *category_name = iter->data;
		GtkAction *action;
		gchar *action_name;

		action_name = g_strdup_printf (
			"memo-filter-category-%d", ii);
		radio_action = gtk_radio_action_new (
			action_name, category_name, NULL, NULL, ii);
		g_free (action_name);

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

	e_shell_content_add_filter_separator_after (
		shell_content, MEMO_FILTER_UNMATCHED);
}
