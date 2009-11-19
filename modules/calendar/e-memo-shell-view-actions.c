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

	/* All shell views respond to the activation of this action,
	 * which is defined by EShellWindow.  But only the currently
	 * active shell view proceeds with saving the custom view. */
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
	ECalComponentPreview *memo_preview;
	EMemoTable *memo_table;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	memo_preview = e_memo_shell_content_get_memo_preview (memo_shell_content);

	e_memo_shell_view_set_status_message (
		memo_shell_view, _("Deleting selected memos..."), -1.0);
	e_memo_table_delete_selected (memo_table);
	e_memo_shell_view_set_status_message (memo_shell_view, NULL, -1.0);

	e_web_view_clear (E_WEB_VIEW (memo_preview));
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
	e_cal_component_set_icalcomponent (comp, clone);
	itip_send_comp (
		E_CAL_COMPONENT_METHOD_PUBLISH, comp,
		comp_data->client, NULL, NULL, NULL, TRUE, FALSE);
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
	EMemoShellBackend *memo_shell_backend;
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

	memo_shell_backend = memo_shell_view->priv->memo_shell_backend;
	source_list = e_memo_shell_backend_get_source_list (memo_shell_backend);

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

	print_action = GTK_PRINT_OPERATION_ACTION_PREVIEW;
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
action_memo_list_rename_cb (GtkAction *action,
                            EMemoShellView *memo_shell_view)
{
	EMemoShellSidebar *memo_shell_sidebar;
	ESourceSelector *selector;

	memo_shell_sidebar = memo_shell_view->priv->memo_shell_sidebar;
	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);

	e_source_selector_edit_primary_selection (selector);
}

static void
action_memo_list_select_one_cb (GtkAction *action,
                                EMemoShellView *memo_shell_view)
{
	EMemoShellSidebar *memo_shell_sidebar;
	ESourceSelector *selector;
	ESource *primary;
	GSList *list, *iter;

	/* XXX ESourceSelector should provide a function for this. */

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
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	ECal *client;
	ECalComponent *comp;
	CompEditor *editor;
	GSList *list;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	client = comp_data->client;
	editor = memo_editor_new (client, shell, COMP_EDITOR_NEW_ITEM);
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
	EShellView *shell_view;
	EShellWindow *shell_window;
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	icalproperty *prop;
	const gchar *uri;
	GSList *list;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only open the URI of the first selected memo. */
	prop = icalcomponent_get_first_property (
		comp_data->icalcomp, ICAL_URL_PROPERTY);
	g_return_if_fail (prop == NULL);

	uri = icalproperty_get_url (prop);
	e_show_uri (GTK_WINDOW (shell_window), uri);
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
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	EActivity *activity;
	GSList *list;
	GFile *file;
	gchar *string;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	file = e_shell_run_save_dialog (
		shell, _("Save as iCalendar"), NULL, NULL, NULL);
	if (file == NULL)
		return;

	/* XXX We only save the first selected memo. */
	string = e_cal_get_component_as_string (
		comp_data->client, comp_data->icalcomp);
	if (string == NULL) {
		g_warning ("Could not convert memo to a string");
		g_object_unref (file);
		return;
	}

	/* XXX No callback means errors are discarded. */
	activity = e_file_replace_contents_async (
		file, string, strlen (string), NULL, FALSE,
		G_FILE_CREATE_NONE, (GAsyncReadyCallback) NULL, NULL);
	e_shell_backend_add_activity (shell_backend, activity);

	/* Free the string when the activity is finalized. */
	g_object_set_data_full (
		G_OBJECT (activity),
		"file-content", string,
		(GDestroyNotify) g_free);

	g_object_unref (file);
}

static void
action_memo_view_cb (GtkRadioAction *action,
                     GtkRadioAction *current,
                     EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	GtkOrientable *orientable;
	GtkOrientation orientation;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	orientable = GTK_ORIENTABLE (memo_shell_content);

	switch (gtk_radio_action_get_current_value (action)) {
		case 0:
			orientation = GTK_ORIENTATION_VERTICAL;
			break;
		case 1:
			orientation = GTK_ORIENTATION_HORIZONTAL;
			break;
		default:
			g_return_if_reached ();
	}

	gtk_orientable_set_orientation (orientable, orientation);
}

static void
action_search_filter_cb (GtkRadioAction *action,
                         GtkRadioAction *current,
                         EShellView *shell_view)
{
	e_shell_view_execute_search (shell_view);
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
	  N_("_Delete Memo"),
	  NULL,
	  N_("Delete selected memos"),
	  G_CALLBACK (action_memo_delete_cb) },

	{ "memo-forward",
	  "mail-forward",
	  N_("_Forward as iCalendar..."),
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

	{ "memo-list-properties",
	  GTK_STOCK_PROPERTIES,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_list_properties_cb) },

	{ "memo-list-rename",
	  NULL,
	  N_("_Rename..."),
	  "F2",
	  N_("Rename the selected memo list"),
	  G_CALLBACK (action_memo_list_rename_cb) },

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

	/*** Menus ***/

	{ "memo-preview-menu",
	  NULL,
	  N_("_Preview"),
	  NULL,
	  NULL,
	  NULL }
};

static EPopupActionEntry memo_popup_entries[] = {

	{ "memo-list-popup-copy",
	  NULL,
	  "memo-list-copy" },

	{ "memo-list-popup-delete",
	  NULL,
	  "memo-list-delete" },

	{ "memo-list-popup-properties",
	  NULL,
	  "memo-list-properties" },

	{ "memo-list-popup-rename",
	  NULL,
	  "memo-list-rename" },

	{ "memo-list-popup-select-one",
	  NULL,
	  "memo-list-select-one" },

	{ "memo-popup-clipboard-copy",
	  NULL,
	  "memo-clipboard-copy" },

	{ "memo-popup-clipboard-cut",
	  NULL,
	  "memo-clipboard-cut" },

	{ "memo-popup-clipboard-paste",
	  NULL,
	  "memo-clipboard-paste" },

	{ "memo-popup-delete",
	  NULL,
	  "memo-delete" },

	{ "memo-popup-forward",
	  NULL,
	  "memo-forward" },

	{ "memo-popup-open",
	  NULL,
	  "memo-open" },

	{ "memo-popup-open-url",
	  NULL,
	  "memo-open-url" }
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

static GtkRadioActionEntry memo_view_entries[] = {

	/* This action represents the initial active memo view.
	 * It should not be visible in the UI, nor should it be
	 * possible to switch to it from another shell view. */
	{ "memo-view-initial",
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  -1 },

	{ "memo-view-classic",
	  NULL,
	  N_("_Classic View"),
	  NULL,
	  N_("Show memo preview below the memo list"),
	  0 },

	{ "memo-view-vertical",
	  NULL,
	  N_("_Vertical View"),
	  NULL,
	  N_("Show memo preview alongside the memo list"),
	  1 }
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

	{ "memo-search-advanced-hidden",
	  NULL,
	  N_("Advanced search"),
	  NULL,
	  NULL,
	  MEMO_SEARCH_ADVANCED },

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

static GtkActionEntry lockdown_printing_entries[] = {

	{ "memo-list-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  "<Control>p",
	  N_("Print the list of memos"),
	  G_CALLBACK (action_memo_list_print_cb) },

	{ "memo-list-print-preview",
	  GTK_STOCK_PRINT_PREVIEW,
	  NULL,
	  NULL,
	  N_("Preview the list of memos to be printed"),
	  G_CALLBACK (action_memo_list_print_preview_cb) },

	{ "memo-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  NULL,
	  N_("Print the selected memo"),
	  G_CALLBACK (action_memo_print_cb) }
};

static EPopupActionEntry lockdown_printing_popup_entries[] = {

	{ "memo-popup-print",
	  NULL,
	  "memo-print" }
};

static GtkActionEntry lockdown_save_to_disk_entries[] = {

	{ "memo-save-as",
	  GTK_STOCK_SAVE_AS,
	  N_("_Save as iCalendar..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_save_as_cb) },
};

static EPopupActionEntry lockdown_save_to_disk_popup_entries[] = {

	{ "memo-popup-save-as",
	  NULL,
	  "memo-save-as" }
};

void
e_memo_shell_view_actions_init (EMemoShellView *memo_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EMemoShellContent *memo_shell_content;
	ECalComponentPreview *memo_preview;
	GtkActionGroup *action_group;
	GConfBridge *bridge;
	GtkAction *action;
	GtkRadioAction *radio_action;
	GObject *object;
	const gchar *key;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_preview = e_memo_shell_content_get_memo_preview (memo_shell_content);

	/* Memo Actions */
	action_group = ACTION_GROUP (MEMOS);
	gtk_action_group_add_actions (
		action_group, memo_entries,
		G_N_ELEMENTS (memo_entries), memo_shell_view);
	e_action_group_add_popup_actions (
		action_group, memo_popup_entries,
		G_N_ELEMENTS (memo_popup_entries));
	gtk_action_group_add_toggle_actions (
		action_group, memo_toggle_entries,
		G_N_ELEMENTS (memo_toggle_entries), memo_shell_view);
	gtk_action_group_add_radio_actions (
		action_group, memo_view_entries,
		G_N_ELEMENTS (memo_view_entries), -1,
		G_CALLBACK (action_memo_view_cb), memo_shell_view);
	gtk_action_group_add_radio_actions (
		action_group, memo_search_entries,
		G_N_ELEMENTS (memo_search_entries),
		-1, NULL, NULL);

	/* Advanced Search action */
	radio_action = GTK_RADIO_ACTION (ACTION (MEMO_SEARCH_ADVANCED_HIDDEN));
	e_shell_content_set_search_radio_action (e_shell_view_get_shell_content (shell_view), radio_action);
	gtk_action_set_visible (GTK_ACTION (radio_action), FALSE);
	gtk_radio_action_set_current_value (radio_action, MEMO_SEARCH_SUMMARY_CONTAINS);

	/* Lockdown Printing Actions */
	action_group = ACTION_GROUP (LOCKDOWN_PRINTING);
	gtk_action_group_add_actions (
		action_group, lockdown_printing_entries,
		G_N_ELEMENTS (lockdown_printing_entries),
		memo_shell_view);
	e_action_group_add_popup_actions (
		action_group, lockdown_printing_popup_entries,
		G_N_ELEMENTS (lockdown_printing_popup_entries));

	/* Lockdown Save-to-Disk Actions */
	action_group = ACTION_GROUP (LOCKDOWN_SAVE_TO_DISK);
	gtk_action_group_add_actions (
		action_group, lockdown_save_to_disk_entries,
		G_N_ELEMENTS (lockdown_save_to_disk_entries),
		memo_shell_view);
	e_action_group_add_popup_actions (
		action_group, lockdown_save_to_disk_popup_entries,
		G_N_ELEMENTS (lockdown_save_to_disk_popup_entries));

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (ACTION (MEMO_PREVIEW));
	key = "/apps/evolution/calendar/display/show_memo_preview";
	gconf_bridge_bind_property (bridge, key, object, "active");

	object = G_OBJECT (ACTION (MEMO_VIEW_VERTICAL));
	key = "/apps/evolution/calendar/display/memo_layout";
	gconf_bridge_bind_property (bridge, key, object, "current-value");

	/* Fine tuning. */

	action = ACTION (MEMO_DELETE);
	g_object_set (action, "short-label", _("Delete"), NULL);

	g_signal_connect (
		ACTION (GAL_SAVE_CUSTOM_VIEW), "activate",
		G_CALLBACK (action_gal_save_custom_view_cb), memo_shell_view);

	e_binding_new (
		ACTION (MEMO_PREVIEW), "active",
		ACTION (MEMO_VIEW_CLASSIC), "sensitive");

	e_binding_new (
		ACTION (MEMO_PREVIEW), "active",
		ACTION (MEMO_VIEW_VERTICAL), "sensitive");

	e_web_view_set_open_proxy (
		E_WEB_VIEW (memo_preview), ACTION (MEMO_OPEN));

	e_web_view_set_print_proxy (
		E_WEB_VIEW (memo_preview), ACTION (MEMO_PRINT));

	e_web_view_set_save_as_proxy (
		E_WEB_VIEW (memo_preview), ACTION (MEMO_SAVE_AS));
}

void
e_memo_shell_view_update_search_filter (EMemoShellView *memo_shell_view)
{
	EShellContent *shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;
	GtkActionGroup *action_group;
	GtkRadioAction *radio_action;
	GList *list, *iter;
	GSList *group;
	gint ii;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	action_group = ACTION_GROUP (MEMOS_FILTER);
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
		const gchar *filename;
		GtkAction *action;
		gchar *action_name;

		action_name = g_strdup_printf (
			"memo-filter-category-%d", ii);
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

	ii = MEMO_FILTER_UNMATCHED;
	e_shell_content_add_filter_separator_after (shell_content, ii);
}
