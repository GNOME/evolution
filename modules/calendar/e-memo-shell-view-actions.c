/*
 * e-memo-shell-view-actions.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "calendar/gui/e-cal-ops.h"
#include "calendar/gui/itip-utils.h"

#include "e-cal-base-shell-view.h"
#include "e-memo-shell-view-private.h"
#include "e-cal-shell-view.h"

static void
action_memo_delete_cb (GtkAction *action,
                       EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	e_selectable_delete_selection (E_SELECTABLE (memo_table));
}

static void
action_memo_find_cb (GtkAction *action,
                     EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EPreviewPane *preview_pane;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	preview_pane = e_memo_shell_content_get_preview_pane (memo_shell_content);

	e_preview_pane_show_search_bar (preview_pane);
}

static void
action_memo_forward_cb (GtkAction *action,
                        EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	ECalComponent *comp;
	GSList *list;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only forward the first selected memo. */
	comp = e_cal_component_new_from_icalcomponent (icalcomponent_new_clone (comp_data->icalcomp));
	g_return_if_fail (comp != NULL);

	itip_send_component_with_model (e_memo_table_get_model (memo_table),
		E_CAL_COMPONENT_METHOD_PUBLISH, comp,
		comp_data->client, NULL, NULL, NULL, TRUE, FALSE, TRUE);

	g_object_unref (comp);
}

static void
action_memo_list_copy_cb (GtkAction *action,
			  EShellView *shell_view)
{
	e_cal_base_shell_view_copy_calendar (shell_view);
}

static void
action_memo_list_delete_cb (GtkAction *action,
                            EMemoShellView *memo_shell_view)
{
	ECalBaseShellSidebar *memo_shell_sidebar;
	EShellWindow *shell_window;
	EShellView *shell_view;
	ESource *source;
	ESourceSelector *selector;
	gint response;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	memo_shell_sidebar = memo_shell_view->priv->memo_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (memo_shell_sidebar);

	source = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (source != NULL);

	if (e_source_get_remote_deletable (source)) {
		response = e_alert_run_dialog_for_args (
			GTK_WINDOW (shell_window),
			"calendar:prompt-delete-remote-memo-list",
			e_source_get_display_name (source), NULL);

		if (response == GTK_RESPONSE_YES)
			e_shell_view_remote_delete_source (shell_view, source);

	} else {
		response = e_alert_run_dialog_for_args (
			GTK_WINDOW (shell_window),
			"calendar:prompt-delete-memo-list",
			e_source_get_display_name (source), NULL);

		if (response == GTK_RESPONSE_YES)
			e_shell_view_remove_source (shell_view, source);
	}

	g_object_unref (source);
}

static void
action_memo_list_manage_groups_cb (GtkAction *action,
				   EMemoShellView *memo_shell_view)
{
	EShellView *shell_view;
	ESourceSelector *selector;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	selector = e_cal_base_shell_sidebar_get_selector (memo_shell_view->priv->memo_shell_sidebar);

	if (e_source_selector_manage_groups (selector) &&
	    e_source_selector_save_groups_setup (selector, e_shell_view_get_state_key_file (shell_view)))
		e_shell_view_set_state_dirty (shell_view);
}

static void
action_memo_list_new_cb (GtkAction *action,
                         EMemoShellView *memo_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	ESourceRegistry *registry;
	ECalClientSourceType source_type;
	GtkWidget *config;
	GtkWidget *dialog;
	const gchar *icon_name;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	registry = e_shell_get_registry (shell);
	source_type = E_CAL_CLIENT_SOURCE_TYPE_MEMOS;
	config = e_cal_source_config_new (registry, NULL, source_type);

	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), GTK_WINDOW (shell_window));

	icon_name = gtk_action_get_icon_name (action);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("New Memo List"));

	gtk_widget_show (dialog);
}

static void
action_memo_list_print_cb (GtkAction *action,
                           EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	print_table (
		E_TABLE (memo_table), _("Print Memos"), _("Memos"),
		GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
action_memo_list_print_preview_cb (GtkAction *action,
                                   EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	print_table (
		E_TABLE (memo_table), _("Print Memos"), _("Memos"),
		GTK_PRINT_OPERATION_ACTION_PREVIEW);
}

static void
action_memo_list_properties_cb (GtkAction *action,
                                EMemoShellView *memo_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalBaseShellSidebar *memo_shell_sidebar;
	ECalClientSourceType source_type;
	ESource *source;
	ESourceSelector *selector;
	ESourceRegistry *registry;
	GtkWidget *config;
	GtkWidget *dialog;
	const gchar *icon_name;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	memo_shell_sidebar = memo_shell_view->priv->memo_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (memo_shell_sidebar);
	source = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (source != NULL);

	source_type = E_CAL_CLIENT_SOURCE_TYPE_MEMOS;
	registry = e_source_selector_get_registry (selector);
	config = e_cal_source_config_new (registry, source, source_type);

	g_object_unref (source);

	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), GTK_WINDOW (shell_window));

	icon_name = gtk_action_get_icon_name (action);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Memo List Properties"));

	gtk_widget_show (dialog);
}

static void
action_memo_list_refresh_cb (GtkAction *action,
                             EMemoShellView *memo_shell_view)
{
	ECalBaseShellSidebar *memo_shell_sidebar;
	ESourceSelector *selector;
	EClient *client = NULL;
	ESource *source;

	memo_shell_sidebar = memo_shell_view->priv->memo_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (memo_shell_sidebar);

	source = e_source_selector_ref_primary_selection (selector);

	if (source != NULL) {
		client = e_client_selector_ref_cached_client (
			E_CLIENT_SELECTOR (selector), source);
		g_object_unref (source);
	}

	if (client == NULL)
		return;

	g_return_if_fail (e_client_check_refresh_supported (client));

	e_cal_base_shell_view_allow_auth_prompt_and_refresh (E_SHELL_VIEW (memo_shell_view), client);

	g_object_unref (client);
}

static void
action_memo_list_rename_cb (GtkAction *action,
                            EMemoShellView *memo_shell_view)
{
	ECalBaseShellSidebar *memo_shell_sidebar;
	ESourceSelector *selector;

	memo_shell_sidebar = memo_shell_view->priv->memo_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (memo_shell_sidebar);

	e_source_selector_edit_primary_selection (selector);
}

static void
action_memo_list_select_all_cb (GtkAction *action,
				EMemoShellView *memo_shell_view)
{
	ECalBaseShellSidebar *memo_shell_sidebar;
	ESourceSelector *selector;

	memo_shell_sidebar = memo_shell_view->priv->memo_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (memo_shell_sidebar);

	e_source_selector_select_all (selector);
}

static void
action_memo_list_select_one_cb (GtkAction *action,
                                EMemoShellView *memo_shell_view)
{
	ECalBaseShellSidebar *memo_shell_sidebar;
	ESourceSelector *selector;
	ESource *primary;

	memo_shell_sidebar = memo_shell_view->priv->memo_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (memo_shell_sidebar);

	primary = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (primary != NULL);

	e_source_selector_select_exclusive (selector, primary);

	g_object_unref (primary);
}

static void
action_memo_new_cb (GtkAction *action,
                    EMemoShellView *memo_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	EClient *client = NULL;
	GSList *list;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	list = e_memo_table_get_selected (memo_table);
	if (list) {
		ECalModelComponent *comp_data;

		comp_data = list->data;
		client = g_object_ref (comp_data->client);
		g_slist_free (list);
	}

	e_cal_ops_new_component_editor (shell_window, E_CAL_CLIENT_SOURCE_TYPE_MEMOS,
		client ? e_source_get_uid (e_client_get_source (client)) : NULL, FALSE);

	g_clear_object (&client);
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
	g_return_if_fail (prop != NULL);

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
	ECalModel *model;
	icalcomponent *clone;
	GSList *list;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	model = e_memo_table_get_model (memo_table);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only print the first selected memo. */
	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	e_cal_component_set_icalcomponent (comp, clone);

	print_comp (
		comp, comp_data->client,
		e_cal_model_get_timezone (model),
		e_cal_model_get_use_24_hour_format (model),
		GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);

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

	/* Translators: Default filename part saving a memo to a file when
	 * no summary is filed, the '.ics' extension is concatenated to it. */
	string = icalcomp_suggest_filename (comp_data->icalcomp, _("memo"));
	file = e_shell_run_save_dialog (
		shell, _("Save as iCalendar"), string,
		"*.ics:text/calendar", NULL, NULL);
	g_free (string);
	if (file == NULL)
		return;

	/* XXX We only save the first selected memo. */
	string = e_cal_client_get_component_as_string (
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

static GtkActionEntry memo_entries[] = {

	{ "memo-delete",
	  "edit-delete",
	  N_("_Delete Memo"),
	  NULL,
	  N_("Delete selected memos"),
	  G_CALLBACK (action_memo_delete_cb) },

	{ "memo-find",
	  "edit-find",
	  N_("_Find in Memo..."),
	  "<Shift><Control>f",
	  N_("Search for text in the displayed memo"),
	  G_CALLBACK (action_memo_find_cb) },

	{ "memo-forward",
	  "mail-forward",
	  N_("_Forward as iCalendar..."),
	  "<Control>f",
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_forward_cb) },

	{ "memo-list-copy",
	  "edit-copy",
	  N_("_Copy..."),
	  "<Control>c",
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_list_copy_cb) },

	{ "memo-list-delete",
	  "edit-delete",
	  N_("D_elete Memo List"),
	  NULL,
	  N_("Delete the selected memo list"),
	  G_CALLBACK (action_memo_list_delete_cb) },

	{ "memo-list-manage-groups",
	  NULL,
	  N_("_Manage Memo List groups..."),
	  NULL,
	  N_("Manage Memo List groups order and visibility"),
	  G_CALLBACK (action_memo_list_manage_groups_cb) },

	{ "memo-list-new",
	  "stock_notes",
	  N_("_New Memo List"),
	  NULL,
	  N_("Create a new memo list"),
	  G_CALLBACK (action_memo_list_new_cb) },

	{ "memo-list-properties",
	  "document-properties",
	  N_("_Properties"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_list_properties_cb) },

	{ "memo-list-refresh",
	  "view-refresh",
	  N_("Re_fresh"),
	  NULL,
	  N_("Refresh the selected memo list"),
	  G_CALLBACK (action_memo_list_refresh_cb) },

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

	{ "memo-list-select-all",
	  "stock_check-filled",
	  N_("Sho_w All Memo Lists"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_list_select_all_cb) },

	{ "memo-new",
	  "stock_insert-note",
	  N_("New _Memo"),
	  NULL,
	  N_("Create a new memo"),
	  G_CALLBACK (action_memo_new_cb) },

	{ "memo-open",
	  "document-open",
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
	  N_("_Delete"),
	  "memo-list-delete" },

	{ "memo-list-popup-manage-groups",
	  N_("_Manage groups..."),
	  "memo-list-manage-groups" },

	{ "memo-list-popup-properties",
	  NULL,
	  "memo-list-properties" },

	{ "memo-list-popup-refresh",
	  NULL,
	  "memo-list-refresh" },

	{ "memo-list-popup-rename",
	  NULL,
	  "memo-list-rename" },

	{ "memo-list-popup-select-all",
	  NULL,
	  "memo-list-select-all" },

	{ "memo-list-popup-select-one",
	  NULL,
	  "memo-list-select-one" },

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
	  N_("Advanced Search"),
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
	  "document-print",
	  N_("Print..."),
	  "<Control>p",
	  N_("Print the list of memos"),
	  G_CALLBACK (action_memo_list_print_cb) },

	{ "memo-list-print-preview",
	  "document-print-preview",
	  N_("Pre_view..."),
	  NULL,
	  N_("Preview the list of memos to be printed"),
	  G_CALLBACK (action_memo_list_print_preview_cb) },

	{ "memo-print",
	  "document-print",
	  N_("Print..."),
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
	  "document-save-as",
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
	EMemoShellContent *memo_shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSearchbar *searchbar;
	EPreviewPane *preview_pane;
	EWebView *web_view;
	GtkActionGroup *action_group;
	GSettings *memo_settings;
	GtkAction *action;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	searchbar = e_memo_shell_content_get_searchbar (memo_shell_content);
	preview_pane = e_memo_shell_content_get_preview_pane (memo_shell_content);
	web_view = e_preview_pane_get_web_view (preview_pane);

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

	/* Advanced Search Action */
	action = ACTION (MEMO_SEARCH_ADVANCED_HIDDEN);
	gtk_action_set_visible (action, FALSE);
	e_shell_searchbar_set_search_option (
		searchbar, GTK_RADIO_ACTION (action));

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

	/* Bind GObject properties to settings keys. */

	memo_settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_bind (
		memo_settings, "show-memo-preview",
		ACTION (MEMO_PREVIEW), "active",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
		memo_settings, "memo-layout",
		ACTION (MEMO_VIEW_VERTICAL), "current-value",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (memo_settings);

	/* Fine tuning. */

	e_binding_bind_property (
		ACTION (MEMO_PREVIEW), "active",
		ACTION (MEMO_VIEW_CLASSIC), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (MEMO_PREVIEW), "active",
		ACTION (MEMO_VIEW_VERTICAL), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_web_view_set_open_proxy (web_view, ACTION (MEMO_OPEN));
	e_web_view_set_print_proxy (web_view, ACTION (MEMO_PRINT));
	e_web_view_set_save_as_proxy (web_view, ACTION (MEMO_SAVE_AS));
}

void
e_memo_shell_view_update_search_filter (EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	GtkActionGroup *action_group;
	GtkRadioAction *radio_action;
	GList *list, *iter;
	GSList *group;
	gint ii;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	action_group = ACTION_GROUP (MEMOS_FILTER);
	e_action_group_remove_all_actions (action_group);

	/* Add the standard filter actions.  No callback is needed
	 * because changes in the EActionComboBox are detected and
	 * handled by EShellSearchbar. */
	gtk_action_group_add_radio_actions (
		action_group, memo_filter_entries,
		G_N_ELEMENTS (memo_filter_entries),
		MEMO_FILTER_ANY_CATEGORY, NULL, NULL);

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
			"memo-filter-category-%d", ii);
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

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	searchbar = e_memo_shell_content_get_searchbar (memo_shell_content);
	combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);

	e_shell_view_block_execute_search (shell_view);

	/* Use any action in the group; doesn't matter which. */
	e_action_combo_box_set_action (combo_box, radio_action);

	ii = MEMO_FILTER_UNMATCHED;
	e_action_combo_box_add_separator_after (combo_box, ii);

	e_shell_view_unblock_execute_search (shell_view);
}
