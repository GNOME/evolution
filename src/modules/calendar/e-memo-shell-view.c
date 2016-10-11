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

#include "e-memo-shell-view-private.h"
#include "e-memo-shell-view.h"

G_DEFINE_DYNAMIC_TYPE (EMemoShellView, e_memo_shell_view, E_TYPE_CAL_BASE_SHELL_VIEW)

static void
memo_shell_view_execute_search (EShellView *shell_view)
{
	EMemoShellContent *memo_shell_content;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	GtkRadioAction *action;
	ECalComponentPreview *memo_preview;
	EPreviewPane *preview_pane;
	EMemoTable *memo_table;
	EWebView *web_view;
	ECalModel *model;
	ECalDataModel *data_model;
	gchar *query;
	gchar *temp;
	gint value;

	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	memo_shell_content = E_MEMO_SHELL_CONTENT (shell_content);
	searchbar = e_memo_shell_content_get_searchbar (memo_shell_content);

	action = GTK_RADIO_ACTION (ACTION (MEMO_SEARCH_ANY_FIELD_CONTAINS));
	value = gtk_radio_action_get_current_value (action);

	if (value == MEMO_SEARCH_ADVANCED) {
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
	}

	/* Apply selected filter. */
	combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);
	value = e_action_combo_box_get_current_value (combo_box);
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

	/* Submit the query. */
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	model = e_memo_table_get_model (memo_table);
	data_model = e_cal_model_get_data_model (model);
	e_cal_data_model_set_filter (data_model, query);
	g_free (query);

	preview_pane =
		e_memo_shell_content_get_preview_pane (memo_shell_content);

	web_view = e_preview_pane_get_web_view (preview_pane);
	memo_preview = E_CAL_COMPONENT_PREVIEW (web_view);
	e_cal_component_preview_clear (memo_preview);
}

static void
memo_shell_view_update_actions (EShellView *shell_view)
{
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	GtkAction *action;
	const gchar *label;
	gboolean sensitive;
	guint32 state;

	/* Be descriptive. */
	gboolean any_memos_selected;
	gboolean has_primary_source;
	gboolean multiple_memos_selected;
	gboolean primary_source_is_writable;
	gboolean primary_source_is_removable;
	gboolean primary_source_is_remote_deletable;
	gboolean primary_source_in_collection;
	gboolean selection_has_url;
	gboolean single_memo_selected;
	gboolean sources_are_editable;
	gboolean refresh_supported;
	gboolean all_sources_selected;

	/* Chain up to parent's update_actions() method. */
	E_SHELL_VIEW_CLASS (e_memo_shell_view_parent_class)->
		update_actions (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);

	shell_content = e_shell_view_get_shell_content (shell_view);
	state = e_shell_content_check_state (shell_content);

	single_memo_selected =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_SINGLE);
	multiple_memos_selected =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_MULTIPLE);
	sources_are_editable =
		(state & E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_EDITABLE);
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

	any_memos_selected = (single_memo_selected || multiple_memos_selected);

	action = ACTION (MEMO_LIST_SELECT_ALL);
	sensitive = !all_sources_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_DELETE);
	sensitive = any_memos_selected && sources_are_editable;
	gtk_action_set_sensitive (action, sensitive);
	if (multiple_memos_selected)
		label = _("Delete Memos");
	else
		label = _("Delete Memo");
	gtk_action_set_label (action, label);

	action = ACTION (MEMO_FIND);
	sensitive = single_memo_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_FORWARD);
	sensitive = single_memo_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_LIST_COPY);
	sensitive = has_primary_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_LIST_DELETE);
	sensitive =
		primary_source_is_removable ||
		primary_source_is_remote_deletable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_LIST_PROPERTIES);
	sensitive = primary_source_is_writable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_LIST_REFRESH);
	sensitive = refresh_supported;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_LIST_RENAME);
	sensitive =
		primary_source_is_writable &&
		!primary_source_in_collection;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_OPEN);
	sensitive = single_memo_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_OPEN_URL);
	sensitive = single_memo_selected && selection_has_url;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_PRINT);
	sensitive = single_memo_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_SAVE_AS);
	sensitive = single_memo_selected;
	gtk_action_set_sensitive (action, sensitive);
}

static void
memo_shell_view_dispose (GObject *object)
{
	e_memo_shell_view_private_dispose (E_MEMO_SHELL_VIEW (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_memo_shell_view_parent_class)->dispose (object);
}

static void
memo_shell_view_finalize (GObject *object)
{
	e_memo_shell_view_private_finalize (E_MEMO_SHELL_VIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_memo_shell_view_parent_class)->finalize (object);
}

static void
memo_shell_view_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_memo_shell_view_parent_class)->constructed (object);

	e_memo_shell_view_private_constructed (E_MEMO_SHELL_VIEW (object));
}

static void
e_memo_shell_view_class_init (EMemoShellViewClass *class)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;
	ECalBaseShellViewClass *cal_base_shell_view_class;

	g_type_class_add_private (class, sizeof (EMemoShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = memo_shell_view_dispose;
	object_class->finalize = memo_shell_view_finalize;
	object_class->constructed = memo_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = _("Memos");
	shell_view_class->icon_name = "evolution-memos";
	shell_view_class->ui_definition = "evolution-memos.ui";
	shell_view_class->ui_manager_id = "org.gnome.evolution.memos";
	shell_view_class->search_options = "/memo-search-options";
	shell_view_class->search_rules = "memotypes.xml";
	shell_view_class->new_shell_content = e_memo_shell_content_new;
	shell_view_class->new_shell_sidebar = e_cal_base_shell_sidebar_new;
	shell_view_class->execute_search = memo_shell_view_execute_search;
	shell_view_class->update_actions = memo_shell_view_update_actions;

	cal_base_shell_view_class = E_CAL_BASE_SHELL_VIEW_CLASS (class);
	cal_base_shell_view_class->source_type = E_CAL_CLIENT_SOURCE_TYPE_MEMOS;

	/* Ensure the GalView types we need are registered. */
	g_type_ensure (GAL_TYPE_VIEW_ETABLE);
}

static void
e_memo_shell_view_class_finalize (EMemoShellViewClass *class)
{
}

static void
e_memo_shell_view_init (EMemoShellView *memo_shell_view)
{
	memo_shell_view->priv = E_MEMO_SHELL_VIEW_GET_PRIVATE (memo_shell_view);
}

void
e_memo_shell_view_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_memo_shell_view_register_type (type_module);
}
