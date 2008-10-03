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

enum {
	PROP_0,
	PROP_SOURCE_LIST
};

GType e_memo_shell_view_type = 0;
static gpointer parent_class;

static void
memo_shell_view_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE_LIST:
			g_value_set_object (
				value, e_memo_shell_view_get_source_list (
				E_MEMO_SHELL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

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
memo_shell_view_update_actions (EShellView *shell_view)
{
	EMemoShellViewPrivate *priv;
	EMemoShellContent *memo_shell_content;
	EMemoShellSidebar *memo_shell_sidebar;
	EShellWindow *shell_window;
	ESourceSelector *selector;
	ETable *table;
	EMemoTable *memo_table;
	ESource *source;
	GtkAction *action;
	GSList *list, *iter;
	const gchar *label;
	const gchar *uri = NULL;
	gboolean user_created_source;
	gboolean editable = TRUE;
	gboolean has_url = FALSE;
	gboolean sensitive;
	gint n_selected;

	priv = E_MEMO_SHELL_VIEW_GET_PRIVATE (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);

	memo_shell_content = priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	memo_shell_sidebar = priv->memo_shell_sidebar;
	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);

	table = e_memo_table_get_table (memo_table);
	n_selected = e_table_selected_count (table);

	list = e_memo_table_get_selected (memo_table);
	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		icalproperty *prop;
		gboolean read_only;

		e_cal_is_read_only (comp_data->client, &read_only, NULL);
		editable &= !read_only;

		prop = icalcomponent_get_first_property (
			comp_data->icalcomp, ICAL_URL_PROPERTY);
		has_url |= (prop != NULL);
	}
	g_slist_free (list);

	source = e_source_selector_peek_primary_selection (selector);
	if (source != NULL)
		uri = e_source_peek_relative_uri (source);
	user_created_source = (uri != NULL && strcmp (uri, "system") != 0);

	action = ACTION (MEMO_CLIPBOARD_COPY);
	sensitive = (n_selected > 0);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_CLIPBOARD_CUT);
	sensitive = (n_selected > 0) && editable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_CLIPBOARD_PASTE);
	sensitive = editable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_DELETE);
	sensitive = (n_selected > 0) && editable;
	gtk_action_set_sensitive (action, sensitive);
	label = ngettext ("Delete Memo", "Delete Memos", n_selected);
	g_object_set (action, "label", label, NULL);

	action = ACTION (MEMO_FORWARD);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_LIST_COPY);
	sensitive = (source != NULL);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_LIST_DELETE);
	sensitive = user_created_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_LIST_PROPERTIES);
	sensitive = (source != NULL);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_OPEN);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_OPEN_URL);
	sensitive = (n_selected == 1) && has_url;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_PRINT);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_SAVE_AS);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);
}

static void
memo_shell_view_class_init (EMemoShellView *class,
                            GTypeModule *type_module)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMemoShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = memo_shell_view_get_property;
	object_class->dispose = memo_shell_view_dispose;
	object_class->finalize = memo_shell_view_finalize;
	object_class->constructed = memo_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = N_("Memos");
	shell_view_class->icon_name = "evolution-memos";
	shell_view_class->ui_definition = "evolution-memos.ui";
	shell_view_class->search_options = "/memo-search-options";
	shell_view_class->search_rules = "memotypes.xml";
	shell_view_class->type_module = type_module;
	shell_view_class->new_shell_content = e_memo_shell_content_new;
	shell_view_class->new_shell_sidebar = e_memo_shell_sidebar_new;
	shell_view_class->update_actions = memo_shell_view_update_actions;

	g_object_class_install_property (
		object_class,
		PROP_SOURCE_LIST,
		g_param_spec_object (
			"source-list",
			_("Source List"),
			_("The registry of memo lists"),
			E_TYPE_SOURCE_LIST,
			G_PARAM_READABLE));
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
e_memo_shell_view_get_type (GTypeModule *type_module)
{
	if (e_memo_shell_view_type == 0) {
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

		e_memo_shell_view_type =
			g_type_module_register_type (
				type_module, E_TYPE_SHELL_VIEW,
				"EMemoShellView", &type_info, 0);
	}

	return e_memo_shell_view_type;
}

ESourceList *
e_memo_shell_view_get_source_list (EMemoShellView *memo_shell_view)
{
	g_return_val_if_fail (E_IS_MEMO_SHELL_VIEW (memo_shell_view), NULL);

	return memo_shell_view->priv->source_list;
}
