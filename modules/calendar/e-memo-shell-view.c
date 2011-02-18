/*
 * e-memo-shell-view.c
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

static gpointer parent_class;
static GType memo_shell_view_type;

static void
memo_shell_view_dispose (GObject *object)
{
	e_memo_shell_view_private_dispose (E_MEMO_SHELL_VIEW (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
memo_shell_view_finalize (GObject *object)
{
	e_memo_shell_view_private_finalize (E_MEMO_SHELL_VIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
memo_shell_view_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	e_memo_shell_view_private_constructed (E_MEMO_SHELL_VIEW (object));
}

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

			categories = e_util_get_searchable_categories ();
			category_name = g_list_nth_data (categories, value);
			g_list_free (categories);

			temp = g_strdup_printf (
				"(and (has-categories? \"%s\") %s)",
				category_name, query);
			g_free (query);
			query = temp;
		}
	}

	/* Submit the query. */
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	model = e_memo_table_get_model (memo_table);
	e_cal_model_set_search_query (model, query);
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
	gboolean can_delete_primary_source;
	gboolean has_primary_source;
	gboolean multiple_memos_selected;
	gboolean selection_has_url;
	gboolean single_memo_selected;
	gboolean sources_are_editable;
	gboolean refresh_supported;

	/* Chain up to parent's update_actions() method. */
	E_SHELL_VIEW_CLASS (parent_class)->update_actions (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);

	shell_content = e_shell_view_get_shell_content (shell_view);
	state = e_shell_content_check_state (shell_content);

	single_memo_selected =
		(state & E_MEMO_SHELL_CONTENT_SELECTION_SINGLE);
	multiple_memos_selected =
		(state & E_MEMO_SHELL_CONTENT_SELECTION_MULTIPLE);
	sources_are_editable =
		(state & E_MEMO_SHELL_CONTENT_SELECTION_CAN_EDIT);
	selection_has_url =
		(state & E_MEMO_SHELL_CONTENT_SELECTION_HAS_URL);

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	state = e_shell_sidebar_check_state (shell_sidebar);

	has_primary_source =
		(state & E_MEMO_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE);
	can_delete_primary_source =
		(state & E_MEMO_SHELL_SIDEBAR_CAN_DELETE_PRIMARY_SOURCE);
	refresh_supported =
		(state & E_MEMO_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH);

	any_memos_selected =
		(single_memo_selected || multiple_memos_selected);

	action = ACTION (MEMO_DELETE);
	sensitive = any_memos_selected && sources_are_editable;
	gtk_action_set_sensitive (action, sensitive);
	if (multiple_memos_selected)
		label = _("Delete Memos");
	else
		label = _("Delete Memo");
	g_object_set (action, "label", label, NULL);

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
	sensitive = can_delete_primary_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_LIST_PROPERTIES);
	sensitive = has_primary_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_LIST_REFRESH);
	sensitive = refresh_supported;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_LIST_RENAME);
	sensitive = can_delete_primary_source;
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
memo_shell_view_class_init (EMemoShellViewClass *class,
                            GTypeModule *type_module)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;

	parent_class = g_type_class_peek_parent (class);
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
	shell_view_class->new_shell_sidebar = e_memo_shell_sidebar_new;
	shell_view_class->execute_search = memo_shell_view_execute_search;
	shell_view_class->update_actions = memo_shell_view_update_actions;
}

static void
memo_shell_view_init (EMemoShellView *memo_shell_view,
                      EShellViewClass *shell_view_class)
{
	memo_shell_view->priv =
		E_MEMO_SHELL_VIEW_GET_PRIVATE (memo_shell_view);

	e_memo_shell_view_private_init (memo_shell_view, shell_view_class);
}

GType
e_memo_shell_view_get_type (void)
{
	return memo_shell_view_type;
}

void
e_memo_shell_view_register_type (GTypeModule *type_module)
{
	const GTypeInfo type_info = {
		sizeof (EMemoShellViewClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) memo_shell_view_class_init,
		(GClassFinalizeFunc) NULL,
		type_module,
		sizeof (EMemoShellView),
		0,    /* n_preallocs */
		(GInstanceInitFunc) memo_shell_view_init,
		NULL  /* value_table */
	};

	memo_shell_view_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_VIEW,
		"EMemoShellView", &type_info, 0);
}
